/*
    Copyright (c) 2006-2020 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hawk-prv.h"
#include "hawk-mus.h"
#include <hawk-ecs.h>

enum token_type_t
{
	TOK_TEXT,
	TOK_VAR,
	TOK_RAW,
	TOK_SECTION,
	TOK_INVERTED,
	TOK_END,
	TOK_PARTIAL
};
typedef struct token_t token_t;
struct token_t
{
	int type;
	hawk_oow_t off;
	hawk_oow_t len;
	hawk_oow_t jump;
	hawk_oow_t indent;
	hawk_oow_t indent_len;
};

typedef struct template_t template_t;
struct template_t
{
	hawk_ooch_t* text;
	hawk_oow_t len;
	hawk_ooch_t* path;
	token_t* tokens;
	hawk_oow_t count;
	hawk_oow_t capa;
};

typedef struct partial_t partial_t;
struct partial_t
{
	partial_t* next;
	template_t* owner;
	hawk_oow_t token;
	template_t* value;
};

typedef struct frame_t frame_t;
struct frame_t
{
	template_t* tpl;
	hawk_oow_t pc;
	hawk_oow_t end;
	void* context;
	void* array;
	hawk_oow_t index;
	hawk_oow_t count;
	hawk_oow_t begin;
};

struct hawk_mus_t
{
	HAWK_MUS_HDR;
	template_t* tpl;
	hawk_oow_t max_depth;
	hawk_ooch_t* partialdirs;
	int busy;
	frame_t* frames;
	hawk_oow_t depth;
	hawk_oow_t capa;
	partial_t* partials;
};

static int set_error (hawk_mus_t* mus, hawk_errnum_t num, const hawk_bch_t* text)
{
	hawk_mus_seterrbfmt(mus, HAWK_NULL, num, "%hs", text);
	return -1;
}

static int callback_error (hawk_mus_t* mus, const hawk_bch_t* text)
{
	if (hawk_mus_geterrnum(mus) == HAWK_ENOERR) set_error(mus, HAWK_EIOERR, text);
	return -1;
}

static int syntax_error (hawk_mus_t* mus, template_t* tpl, hawk_oow_t off, const hawk_bch_t* text)
{
	hawk_loc_t loc;
	hawk_oow_t i;

	HAWK_MEMSET(&loc, 0, HAWK_SIZEOF(loc));
	loc.line = loc.colm = 1;
	for (i = 0; i < off; i++)
	{
		if (tpl->text[i] == '\n')
		{
			loc.line++;
			loc.colm = 1;
		}
		else loc.colm++;
	}
	hawk_mus_seterrbfmt(mus, &loc, HAWK_EINVAL, "%hs", text);
	return -1;
}

static void* grow (hawk_mus_t* mus, void* ptr, hawk_oow_t* capa, hawk_oow_t need, hawk_oow_t size)
{
	hawk_oow_t n;
	void* tmp;

	if (need <= *capa) return ptr;

	if (need > HAWK_TYPE_MAX(hawk_oow_t) / size)
	{
		set_error(mus, HAWK_ENOMEM, "mustache buffer too large");
		return HAWK_NULL;
	}

	n = *capa? *capa: 16;
	while (n < need)
	{
		if (n > HAWK_TYPE_MAX(hawk_oow_t) / 2 / size)
		{
			n = need;
			break;
		}
		n *= 2;
	}

	tmp = hawk_mus_reallocmem(mus, ptr, n * size);
	if (HAWK_UNLIKELY(!tmp)) return HAWK_NULL;
	*capa = n;
	return tmp;
}

static void free_template (hawk_mus_t* mus, template_t* tpl)
{
	if (tpl->text) hawk_mus_freemem(mus, tpl->text);
	if (tpl->path) hawk_mus_freemem(mus, tpl->path);
	if (tpl->tokens) hawk_mus_freemem(mus, tpl->tokens);
	hawk_mus_freemem(mus, tpl);
}

static int add_token (hawk_mus_t* mus, template_t* tpl, int type, hawk_oow_t off, hawk_oow_t len)
{
	token_t* token;
	token_t* tokens;

	if (type == TOK_TEXT && len == 0) return 0;

	if (tpl->count == HAWK_TYPE_MAX(hawk_oow_t))
		return set_error(mus, HAWK_ENOMEM, "too many mustache tokens");

	tokens = grow(mus, tpl->tokens, &tpl->capa, tpl->count + 1, HAWK_SIZEOF(*token));
	if (HAWK_UNLIKELY(!tokens)) return -1;

	tpl->tokens = tokens;
	token = &tpl->tokens[tpl->count++];
	HAWK_MEMSET(token, 0, HAWK_SIZEOF(*token));
	token->type = type;
	token->off = off;
	token->len = len;
	return 0;
}

static int whitespace (hawk_ooch_t c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static hawk_oow_t find_delimiter (template_t* tpl, hawk_oow_t pos, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t i;

	while (pos <= tpl->len && len <= tpl->len - pos)
	{
		for (i = 0; i < len && tpl->text[pos + i] == ptr[i]; i++);
		if (i == len) return pos;
		pos++;
	}
	return tpl->len;
}

static template_t* parse_template (hawk_mus_t* mus, const hawk_ooch_t* ptr, hawk_oow_t len, const hawk_ooch_t* path)
{
	template_t* tpl;
	const hawk_ooch_t* open = HAWK_T("{{");
	const hawk_ooch_t* close = HAWK_T("}}");
	hawk_oow_t olen = 2, clen = 2;
	hawk_oow_t* stack = HAWK_NULL;
	hawk_oow_t* tmp_stack;
	hawk_oow_t depth = 0, capa = 0;
	hawk_oow_t pos = 0, off, end, next, beg, n, line, tail, i, index, top;
	int type, standalone;
	hawk_ooch_t marker;

	if (len > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(hawk_ooch_t) - 1)
	{
		set_error(mus, HAWK_ENOMEM, "mustache template too large");
		return HAWK_NULL;
	}

	tpl = hawk_gem_callocmem(&mus->gem_, HAWK_SIZEOF(*tpl));
	if (HAWK_UNLIKELY(!tpl)) return HAWK_NULL;

	tpl->text = hawk_gem_dupoochars(&mus->gem_, ptr? ptr: HAWK_T(""), len);
	if (!tpl->text) goto fail;

	tpl->len = len;
	if (path)
	{
		tpl->path = hawk_gem_dupoocstr(&mus->gem_, path, HAWK_NULL);
		if (HAWK_UNLIKELY(!tpl->path)) goto fail;
	}
	while (pos < len)
	{
		off = find_delimiter(tpl, pos, open, olen);
		if (off == len) break;

		beg = off + olen;
		end = find_delimiter(tpl, beg, close, clen);
		if (end == len)
		{
			syntax_error(mus, tpl, off, "unterminated mustache tag");
			goto fail;
		}
		next = end + clen;
		while (beg < end && whitespace(tpl->text[beg])) beg++;
		while (end > beg && whitespace(tpl->text[end - 1])) end--;
		if (beg == end)
		{
			syntax_error(mus, tpl, off, "empty mustache tag");
			goto fail;
		}

		marker = tpl->text[beg];
		type = TOK_VAR;

		if (marker == '#') type = TOK_SECTION;
		else if (marker == '^') type = TOK_INVERTED;
		else if (marker == '/') type = TOK_END;
		else if (marker == '>') type = TOK_PARTIAL;
		else if (marker == '&' || marker == '{') type = TOK_RAW;
		else if (marker == '!' || marker == '=') type = -1;
		else if (marker == '<' || marker == '$')
		{
			set_error(mus, HAWK_ENOIMPL, "mustache inheritance is not supported");
			goto fail;
		}

		if (marker == '{')
		{
			if (end > beg && tpl->text[end - 1] == '}') end--;
			else if (next < len && tpl->text[next] == '}') next++;
			else
			{
				syntax_error(mus, tpl, off, "invalid raw mustache tag");
				goto fail;
			}
		}
		if (type != TOK_VAR && type != -1) beg++;
		while (beg < end && whitespace(tpl->text[beg])) beg++;
		while (end > beg && whitespace(tpl->text[end - 1])) end--;
		if (type >= 0 && beg == end)
		{
			syntax_error(mus, tpl, off, "empty mustache name");
			goto fail;
		}

		for (i = beg; i < end; i++)
		{
			if (tpl->text[i] == 0)
			{
				syntax_error(mus, tpl, off, "NUL in mustache tag");
				goto fail;
			}
		}

		if (type == TOK_PARTIAL && tpl->text[beg] == '*')
		{
			set_error(mus, HAWK_ENOIMPL, "dynamic mustache partial names are not supported");
			goto fail;
		}

		/* Standalone control tags remove their entire line, including CRLF.
		 * The whitespace preceding a standalone partial belongs to its body. */
		standalone = type != TOK_VAR && type != TOK_RAW;
		line = off;

		while (line > 0 && tpl->text[line - 1] != '\n') line--;

		for (i = line; i < off; i++)
		{
			if (tpl->text[i] != ' ' && tpl->text[i] != '\t') standalone = 0;
		}

		tail = next;
		while (tail < len && (tpl->text[tail] == ' ' || tpl->text[tail] == '\t')) tail++;

		if (tail < len && tpl->text[tail] == '\r' && tail + 1 < len && tpl->text[tail + 1] == '\n') tail += 2;
		else if (tail < len && tpl->text[tail] == '\n') tail++;
		else if (tail != len) standalone = 0;

		if (add_token(mus, tpl, TOK_TEXT, pos, (standalone? line: off) - pos) <= -1) goto fail;

		index = tpl->count;
		if (type >= 0 && add_token(mus, tpl, type, beg, end - beg) <= -1) goto fail;
		if (type == TOK_PARTIAL && standalone)
		{
			tpl->tokens[index].indent = line;
			tpl->tokens[index].indent_len = off - line;
		}
		if (type == TOK_SECTION || type == TOK_INVERTED)
		{
			if (depth >= mus->max_depth)
			{
				set_error(mus, HAWK_EBUFFULL, "mustache section depth exceeded");
				goto fail;
			}

			tmp_stack = grow(mus, stack, &capa, depth + 1, HAWK_SIZEOF(*stack));
			if (HAWK_UNLIKELY(!tmp_stack)) goto fail;

			stack = tmp_stack;
			stack[depth++] = index;
		}
		else if (type == TOK_END)
		{
			if (depth == 0)
			{
				syntax_error(mus, tpl, off, "unexpected mustache section end");
				goto fail;
			}
			top = stack[--depth];
			if (tpl->tokens[top].len != end - beg ||
			    hawk_comp_oochars(tpl->text + tpl->tokens[top].off, tpl->tokens[top].len, tpl->text + beg, end - beg, 0) != 0)
			{
				syntax_error(mus, tpl, off, "mismatched mustache section end");
				goto fail;
			}
			tpl->tokens[top].jump = index;
			tpl->tokens[index].jump = top;
		}
		else if (marker == '=')
		{
			/* Two nonempty delimiter strings, separated by whitespace. */
			if (end <= beg + 1 || tpl->text[end - 1] != '=') goto bad_delimiter;

			beg++;
			end--;

			while (beg < end && whitespace(tpl->text[beg])) beg++;
			while (end > beg && whitespace(tpl->text[end - 1])) end--;

			n = beg;
			while (n < end && !whitespace(tpl->text[n]) && tpl->text[n] != '=') n++;
			if (n == beg || n == end || tpl->text[n] == '=') goto bad_delimiter;

			open = tpl->text + beg;
			olen = n - beg;
			while (n < end && whitespace(tpl->text[n])) n++;
			if (n == end) goto bad_delimiter;

			close = tpl->text + n;
			clen = end - n;
			for (i = n; i < end; i++)
			{
				if (whitespace(tpl->text[i]) || tpl->text[i] == '=') goto bad_delimiter;
			}
		}
		pos = standalone? tail: next;
	}
	if (depth != 0)
	{
		syntax_error(mus, tpl, len, "unclosed mustache section");
		goto fail;
	}

	if (add_token(mus, tpl, TOK_TEXT, pos, len - pos) <= -1) goto fail;
	if (stack) hawk_mus_freemem(mus, stack);

	return tpl;

bad_delimiter:
	syntax_error(mus, tpl, off, "invalid mustache delimiters");
fail:
	if (stack) hawk_mus_freemem(mus, stack);
	free_template(mus, tpl);
	return HAWK_NULL;
}

static int close_io (hawk_mus_t* mus, hawk_mus_io_impl_t io, hawk_mus_io_arg_t* arg, void* ctx, int result)
{
	hawk_errinf_t error;
	hawk_ooi_t n;

	if (result <= -1) hawk_gem_geterrinf(&mus->gem_, &error);
	n = io(mus, HAWK_MUS_IO_CLOSE, arg, HAWK_NULL, 0, ctx);
	if (result <= -1) hawk_gem_seterrinf(&mus->gem_, &error);
	else if (n <= -1) result = callback_error(mus, "unable to close mustache stream");
	return result;
}

static template_t* load_template (hawk_mus_t* mus, const hawk_ooch_t* name, const hawk_ooch_t* parent,
	const hawk_ooch_t* indent, hawk_oow_t indent_len, hawk_mus_io_kind_t kind, hawk_mus_io_impl_t io, void* ctx)
{
	hawk_mus_io_arg_t arg;
	hawk_ooecs_t text;
	hawk_ooecs_t padded;
	hawk_ooch_t buffer[256];
	template_t* tpl = HAWK_NULL;
	hawk_ooi_t n;
	hawk_oow_t pos;
	int result = -1;

	HAWK_MEMSET(&arg, 0, HAWK_SIZEOF(arg));
	arg.kind = kind;
	arg.name = name;
	arg.parent = parent;
	n = io(mus, HAWK_MUS_IO_OPEN, &arg, HAWK_NULL, 0, ctx);
	if (n <= -1)
	{
		callback_error(mus, "unable to open mustache template");
		return HAWK_NULL;
	}
	if (n == 0)
	{
		if (kind == HAWK_MUS_IO_TEMPLATE)
		{
			set_error(mus, HAWK_ENOENT, "mustache template not found");
			return HAWK_NULL;
		}
		return parse_template(mus, HAWK_T(""), 0, HAWK_NULL);
	}

	if (hawk_ooecs_init(&text, &mus->gem_, 0) <= -1) goto close;

	while (1)
	{
		n = io(mus, HAWK_MUS_IO_READ, &arg, buffer, HAWK_COUNTOF(buffer), ctx);
		if (n <= -1)
		{
			callback_error(mus, "unable to read mustache template");
			goto fini;
		}
		if (n == 0) break;
		if ((hawk_oow_t)n > HAWK_COUNTOF(buffer))
		{
			set_error(mus, HAWK_EINVAL, "invalid mustache read count");
			goto fini;
		}
		if (hawk_ooecs_ncat(&text, buffer, n) == (hawk_oow_t)-1) goto fini;
	}

	if (indent_len > 0)
	{
		if (hawk_ooecs_init(&padded, &mus->gem_, 0) <= -1) goto fini;
		for (pos = 0; pos < HAWK_OOECS_LEN(&text); pos++)
		{
			if ((pos == 0 || HAWK_OOECS_PTR(&text)[pos - 1] == '\n') &&
			    hawk_ooecs_ncat(&padded, indent, indent_len) == (hawk_oow_t)-1) goto fini_padded;
			if (hawk_ooecs_ncat(&padded, HAWK_OOECS_PTR(&text) + pos, 1) == (hawk_oow_t)-1) goto fini_padded;
		}
		tpl = parse_template(mus, HAWK_OOECS_PTR(&padded), HAWK_OOECS_LEN(&padded), arg.path? arg.path: name);
	fini_padded:
		hawk_ooecs_fini(&padded);
	}
	else tpl = parse_template(mus, HAWK_OOECS_PTR(&text), HAWK_OOECS_LEN(&text), arg.path? arg.path: name);
	if (tpl) result = 0;

fini:
	hawk_ooecs_fini(&text);

close:
	if (close_io(mus, io, &arg, ctx, result) <= -1 && tpl)
	{
		free_template(mus, tpl);
		tpl = HAWK_NULL;
	}
	return tpl;
}

static int push_frame (hawk_mus_t* mus, template_t* tpl, hawk_oow_t pc, hawk_oow_t end, void* context)
{
	frame_t* frame;
	frame_t* frames;

	if (mus->depth >= mus->max_depth)
		return set_error(mus, HAWK_EBUFFULL, "mustache rendering depth exceeded");

	frames = grow(mus, mus->frames, &mus->capa, mus->depth + 1, HAWK_SIZEOF(*frame));
	if (HAWK_UNLIKELY(!frames)) return -1;

	mus->frames = frames;
	frame = &mus->frames[mus->depth++];
	HAWK_MEMSET(frame, 0, HAWK_SIZEOF(*frame));
	frame->tpl = tpl;
	frame->begin = frame->pc = pc;
	frame->end = end;
	frame->context = context;
	return 0;
}

static int lookup (hawk_mus_t* mus, const hawk_mus_data_t* data, void* ctx, const hawk_ooch_t* name, hawk_oow_t len, void** value)
{
	hawk_oow_t n, pos, end, depth;
	int found = 0;

	*value = HAWK_NULL;
	if (len == 1 && name[0] == '.')
	{
		*value = mus->frames[mus->depth - 1].context;
		return 1;
	}

	for (n = 0; n < len && name[n] != '.'; n++) /* loop */;

	for (depth = mus->depth; depth > 0; depth--)
	{
		void* context = mus->frames[depth - 1].context;
		if (!context) continue;
		found = data->get(mus, context, name, n, value, ctx);
		if (found <= -1) return callback_error(mus, "mustache member lookup failed");
		if (found != 0) break;
	}
	if (!found) return 0;

	for (pos = n; pos < len; pos = end)
	{
		end = pos + 1;
		while (end < len && name[end] != '.') end++;
		if (!*value) return 0;
		found = data->get(mus, *value, name + pos + 1, end - pos - 1, value, ctx);
		if (found <= -1) return callback_error(mus, "mustache member lookup failed");
		if (!found) return 0;
	}

	return 1;
}

static int value_type (hawk_mus_t* mus, const hawk_mus_data_t* data, void* ctx, void* value)
{
	int type;

	if (!value) return HAWK_MUS_VAL_NIL;
	type = data->type(mus, value, ctx);
	if (type <= -1) return callback_error(mus, "mustache value classification failed");
	if (type > HAWK_MUS_VAL_ARR) return set_error(mus, HAWK_EINVAL, "invalid mustache value type");
	return type;
}

static int write_text (hawk_mus_t* mus, hawk_mus_io_impl_t io, void* ctx, hawk_mus_io_arg_t* output, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_ooi_t n;

	while (len > 0)
	{
		n = io(mus, HAWK_MUS_IO_WRITE, output, (hawk_ooch_t*)ptr, len, ctx);
		if (n <= -1)
			return callback_error(mus, "unable to write mustache output");
		if (n == 0 || (hawk_oow_t)n > len)
			return set_error(mus, HAWK_EIOERR, "invalid mustache write count");

		ptr += n;
		len -= n;
	}
	return 0;
}

static int write_value (hawk_mus_t* mus, hawk_mus_io_impl_t io, void* ctx, hawk_mus_io_arg_t* output,
	const hawk_ooch_t* ptr, hawk_oow_t len, int escape)
{
	hawk_oow_t pos, begin = 0;
	const hawk_ooch_t* replacement;

	if (!ptr && len) return set_error(mus, HAWK_EINVAL, "invalid mustache string value");
	if (!escape) return write_text(mus, io, ctx, output, ptr, len);

	for (pos = 0; pos < len; pos++)
	{
		switch (ptr[pos])
		{
			case '&':
				replacement = HAWK_T("&amp;");
				break;

			case '<':
				replacement = HAWK_T("&lt;");
				break;

			case '>':
				replacement = HAWK_T("&gt;");
				break;

			case '"':
				replacement = HAWK_T("&quot;");
				break;

			case '\'':
				replacement = HAWK_T("&#39;");
				break;

			default:
				continue;
		}
		if (write_text(mus, io, ctx, output, ptr + begin, pos - begin) <= -1 ||
		    write_text(mus, io, ctx, output, replacement, hawk_count_oocstr(replacement)) <= -1) return -1;
		begin = pos + 1;
	}
	return write_text(mus, io, ctx, output, ptr? ptr + begin: ptr, len - begin);
}

static void clear_render (hawk_mus_t* mus)
{
	partial_t* part;

	mus->depth = 0;
	while ((part = mus->partials))
	{
		mus->partials = part->next;
		free_template(mus, part->value);
		hawk_mus_freemem(mus, part);
	}
}

static template_t* get_partial (hawk_mus_t* mus, template_t* tpl, hawk_oow_t index, hawk_mus_io_impl_t io, void* ctx)
{
	partial_t* part;
	token_t* token = &tpl->tokens[index];
	hawk_ooch_t* name;
	template_t* value;

	for (part = mus->partials; part; part = part->next)
	{
		if (part->owner == tpl && part->token == index) return part->value;
	}

	name = hawk_gem_dupoochars(&mus->gem_, tpl->text + token->off, token->len);
	if (HAWK_UNLIKELY(!name)) return HAWK_NULL;

	value = load_template(mus, name, tpl->path, tpl->text + token->indent, token->indent_len, HAWK_MUS_IO_PARTIAL, io, ctx);
	hawk_mus_freemem(mus, name);
	if (HAWK_UNLIKELY(!value)) return HAWK_NULL;

	part = hawk_mus_allocmem(mus, HAWK_SIZEOF(*part));
	if (HAWK_UNLIKELY(!part))
	{
		free_template(mus, value);
		return HAWK_NULL;
	}
	part->owner = tpl;
	part->token = index;
	part->value = value;
	part->next = mus->partials;
	mus->partials = part;
	return value;
}

int hawk_mus_render (hawk_mus_t* mus, void* root, const hawk_mus_data_t* data, void* data_ctx, hawk_mus_io_impl_t io, void* io_ctx)
{
	hawk_mus_io_arg_t output;
	frame_t* frame;
	token_t* token;
	template_t* tpl;
	void* value;
	void* item;
	const hawk_ooch_t* ptr;
	hawk_oow_t len, count, index, end;
	int type, found, result = -1;
	hawk_ooi_t opened;

	if (mus->busy) return set_error(mus, HAWK_EPERM, "mustache engine is busy");
	hawk_mus_seterrnum(mus, HAWK_NULL, HAWK_ENOERR);
	if (!mus->tpl || !data || !data->get || !data->type || !data->str || !io)
		return set_error(mus, HAWK_EINVAL, "mustache template, data callbacks, and I/O are required");
	mus->busy = 1;

	HAWK_MEMSET(&output, 0, HAWK_SIZEOF(output));
	output.kind = HAWK_MUS_IO_OUTPUT;

	opened = io(mus, HAWK_MUS_IO_OPEN, &output, HAWK_NULL, 0, io_ctx);
	if (opened <= 0)
	{
		callback_error(mus, "unable to open mustache output");
		goto done;
	}

	if (push_frame(mus, mus->tpl, 0, mus->tpl->count, root) <= -1) goto close;

	while (mus->depth > 0)
	{
		frame = &mus->frames[mus->depth - 1];
		if (frame->pc == frame->end)
		{
			if (frame->array && frame->index < frame->count - 1)
			{
				frame->index++;
				if (data->item(mus, frame->array, frame->index, &item, data_ctx) <= -1)
				{
					callback_error(mus, "mustache array item lookup failed");
					goto close;
				}
				frame->context = item;
				frame->pc = frame->begin;
			}
			else mus->depth--;
			continue;
		}
		index = frame->pc++;
		tpl = frame->tpl;

		token = &tpl->tokens[index];
		if (token->type == TOK_TEXT)
		{
			if (write_text(mus, io, io_ctx, &output, tpl->text + token->off, token->len) <= -1) goto close;
		}
		else if (token->type == TOK_PARTIAL)
		{
			/* Loading/parsing can allocate, but the template and token are stable. */
			value = frame->context;
			tpl = get_partial(mus, tpl, index, io, io_ctx);
			if (!tpl || push_frame(mus, tpl, 0, tpl->count, value) <= -1) goto close;
		}
		else
		{
			found = lookup(mus, data, data_ctx, tpl->text + token->off, token->len, &value);
			if (found <= -1) goto close;

			type = found? value_type(mus, data, data_ctx, value): HAWK_MUS_VAL_NIL;
			if (type <= -1) goto close;
			if (token->type == TOK_VAR || token->type == TOK_RAW)
			{
				if (type == HAWK_MUS_VAL_NIL) continue;
				ptr = HAWK_NULL;
				len = 0;
				if (data->str(mus, value, &ptr, &len, data_ctx) <= -1)
				{
					callback_error(mus, "mustache string conversion failed");
					goto close;
				}
				if (write_value(mus, io, io_ctx, &output, ptr, len, token->type == TOK_VAR) <= -1) goto close;
			}
			else
			{
				count = type == HAWK_MUS_VAL_NIL || type == HAWK_MUS_VAL_FALSE? 0: 1;
				if (type == HAWK_MUS_VAL_ARR)
				{
					if (!data->count || !data->item)
					{
						set_error(mus, HAWK_EINVAL, "mustache array callbacks are required");
						goto close;
					}
					if (data->count(mus, value, &count, data_ctx) <= -1)
					{
						callback_error(mus, "mustache array size lookup failed");
						goto close;
					}
				}
				end = token->jump;
				frame->pc = end + 1;
				if (token->type == TOK_INVERTED)
				{
					if (count == 0 && push_frame(mus, tpl, index + 1, end, frame->context) <= -1) goto close;
				}
				else if (count > 0)
				{
					item = value;
					if (type == HAWK_MUS_VAL_ARR && data->item(mus, value, 0, &item, data_ctx) <= -1)
					{
						callback_error(mus, "mustache array item lookup failed");
						goto close;
					}

					if (push_frame(mus, tpl, index + 1, end, item) <= -1) goto close;
					if (type == HAWK_MUS_VAL_ARR)
					{
						frame = &mus->frames[mus->depth - 1];
						frame->array = value;
						frame->count = count;
					}
				}
			}
		}
	}
	result = 0;

close:
	result = close_io(mus, io, &output, io_ctx, result);

done:
	clear_render(mus);
	mus->busy = 0;
	return result;
}

hawk_mus_t* hawk_mus_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	hawk_mus_t* mus;
	hawk_errnum_t num;

	num = !mmgr? HAWK_EINVAL: HAWK_ENOMEM;
	mus = HAWK_NULL;
	if (mmgr && xtnsize <= HAWK_TYPE_MAX(hawk_oow_t) - HAWK_SIZEOF(*mus))
		mus = HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(*mus) + xtnsize);
	if (!mus)
	{
		if (errinf)
		{
			HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
			errinf->num = num;
			hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(num));
		}
		return HAWK_NULL;
	}

	HAWK_MEMSET(mus, 0, HAWK_SIZEOF(*mus) + xtnsize);
	mus->instsize_ = HAWK_SIZEOF(*mus);
	mus->gem_.mmgr = mmgr;
	mus->gem_.cmgr = cmgr? cmgr: hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
	mus->gem_.errstr = hawk_dfl_errstr;
	mus->max_depth = 256;

	return mus;
}

void hawk_mus_reset (hawk_mus_t* mus)
{
	HAWK_ASSERT(!mus->busy);
	if (mus->tpl)
	{
		free_template(mus, mus->tpl);
		mus->tpl = HAWK_NULL;
	}
}

void hawk_mus_close (hawk_mus_t* mus)
{
	hawk_mus_reset(mus);
	if (mus->frames) hawk_mus_freemem(mus, mus->frames);
	if (mus->partialdirs) hawk_mus_freemem(mus, mus->partialdirs);
	HAWK_MMGR_FREE(mus->gem_.mmgr, mus);
}

int hawk_mus_compilewithoochars (hawk_mus_t* mus, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	template_t* tpl;

	if (mus->busy) return set_error(mus, HAWK_EPERM, "mustache engine is busy");
	hawk_mus_seterrnum(mus, HAWK_NULL, HAWK_ENOERR);
	if (!ptr && len) return set_error(mus, HAWK_EINVAL, "mustache source is missing");

	mus->busy = 1;
	tpl = parse_template(mus, ptr, len, HAWK_NULL);
	mus->busy = 0;
	if (!tpl) return -1;

	if (mus->tpl) free_template(mus, mus->tpl);
	mus->tpl = tpl;

	return 0;
}

int hawk_mus_compilewithbchars (hawk_mus_t* mus, const hawk_bch_t* ptr, hawk_oow_t len)
{
#if defined(HAWK_OOCH_IS_BCH)
	return hawk_mus_compilewithoochars(mus, ptr, len);
#else
	hawk_uch_t* text;
	hawk_oow_t count;
	int n;

	if (mus->busy) return set_error(mus, HAWK_EPERM, "mustache engine is busy");
	hawk_mus_seterrnum(mus, HAWK_NULL, HAWK_ENOERR);

	if ((!ptr && len) || len > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(hawk_uch_t) - 1)
		return set_error(mus, HAWK_EINVAL, "invalid mustache byte source");

	text = hawk_gem_dupbtouchars(&mus->gem_, ptr? ptr: "", len, &count, 0);
	if (!text) return -1;

	n = hawk_mus_compilewithoochars(mus, text, count);
	hawk_mus_freemem(mus, text);
	return n;
#endif
}

int hawk_mus_compilewithuchars (hawk_mus_t* mus, const hawk_uch_t* ptr, hawk_oow_t len)
{
#if defined(HAWK_OOCH_IS_UCH)
	return hawk_mus_compilewithoochars(mus, ptr, len);
#else
	hawk_bch_t* text;
	hawk_uch_t empty = 0;
	hawk_oow_t count;
	int n;

	if (mus->busy) return set_error(mus, HAWK_EPERM, "mustache engine is busy");
	hawk_mus_seterrnum(mus, HAWK_NULL, HAWK_ENOERR);

	if ((!ptr && len) || len > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(hawk_uch_t) - 1)
		return set_error(mus, HAWK_EINVAL, "invalid mustache Unicode source");

	text = hawk_gem_duputobchars(&mus->gem_, ptr? ptr: &empty, len, &count);
	if (!text) return -1;

	n = hawk_mus_compilewithoochars(mus, text, count);
	hawk_mus_freemem(mus, text);
	return n;
#endif
}

int hawk_mus_compile (hawk_mus_t* mus, const hawk_ooch_t* name, hawk_mus_io_impl_t io, void* ctx)
{
	template_t* tpl;

	if (mus->busy) return set_error(mus, HAWK_EPERM, "mustache engine is busy");
	hawk_mus_seterrnum(mus, HAWK_NULL, HAWK_ENOERR);

	if (!io) return set_error(mus, HAWK_EINVAL, "mustache input callback is required");

	mus->busy = 1;
	tpl = load_template(mus, name, HAWK_NULL, HAWK_NULL, 0, HAWK_MUS_IO_TEMPLATE, io, ctx);
	mus->busy = 0;
	if (!tpl) return -1;

	if (mus->tpl) free_template(mus, mus->tpl);
	mus->tpl = tpl;

	return 0;
}

int hawk_mus_setopt (hawk_mus_t* mus, hawk_mus_opt_t id, const void* value)
{
	hawk_oow_t depth;
	hawk_ooch_t* dirs;

	if (mus->busy) return set_error(mus, HAWK_EPERM, "mustache engine is busy");
	if (!value) return set_error(mus, HAWK_EINVAL, "mustache option value is required");

	if (id == HAWK_MUS_OPT_MAX_DEPTH)
	{
		depth = *(const hawk_oow_t*)value;
		if (depth == 0) return set_error(mus, HAWK_EINVAL, "mustache depth must be positive");
		mus->max_depth = depth;
	}
	else if (id == HAWK_MUS_OPT_PARTIALDIRS)
	{
		dirs = hawk_gem_dupoocstr(&mus->gem_, value, HAWK_NULL);
		if (!dirs) return -1;
		if (mus->partialdirs) hawk_mus_freemem(mus, mus->partialdirs);
		mus->partialdirs = dirs;
	}
	else return set_error(mus, HAWK_EINVAL, "unknown mustache option");

	return 0;
}

int hawk_mus_getopt (hawk_mus_t* mus, hawk_mus_opt_t id, void* value)
{
	if (!value) return set_error(mus, HAWK_EINVAL, "mustache option output is required");
	if (id == HAWK_MUS_OPT_MAX_DEPTH) *(hawk_oow_t*)value = mus->max_depth;
	else if (id == HAWK_MUS_OPT_PARTIALDIRS) *(const hawk_ooch_t**)value = mus->partialdirs;
	else return set_error(mus, HAWK_EINVAL, "unknown mustache option");
	return 0;
}

void hawk_mus_seterrbfmt (hawk_mus_t* mus, const hawk_loc_t* loc, hawk_errnum_t num, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hawk_gem_seterrbvfmt(&mus->gem_, loc, num, fmt, ap);
	va_end(ap);
}

void hawk_mus_seterrufmt (hawk_mus_t* mus, const hawk_loc_t* loc, hawk_errnum_t num, const hawk_uch_t* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hawk_gem_seterruvfmt(&mus->gem_, loc, num, fmt, ap);
	va_end(ap);
}
