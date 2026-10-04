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
#include <hawk-m4.h>
#include <hawk-fmt.h>

#define M4_HASH_SIZE  (199)
#define M4_NARGS      (33) /* macro name plus up to 32 arguments; TODO: remove limit */
#define M4_NDIVS      (32) /* TODO: support unlimited number of diversions */
#define M4_STR_BLOCK  (32)
#define M4_QUOTE_SIZE (32)

#define M4_DFL_BQUOTE HAWK_T('`')
#define M4_DFL_EQUOTE HAWK_T('\'')

typedef struct m4_str_t m4_str_t;
typedef struct m4_iframe_t m4_iframe_t;
typedef struct m4_oframe_t m4_oframe_t;
typedef struct m4_entry_t m4_entry_t;
typedef int (*m4_builtin_t) (hawk_m4_t*, m4_str_t**);

struct m4_str_t
{
	hawk_oow_t refs;
	hawk_oow_t hash;
	hawk_ooch_t* ptr;
	hawk_oow_t len;
	hawk_oow_t capa;
};

enum m4_input_type_t
{
	M4_INPUT_STREAM,
	M4_INPUT_STRING,
	M4_INPUT_REPLAY
};

struct m4_iframe_t
{
	m4_iframe_t* back;
	int type;
	int has_unget;
	hawk_ooch_t unget;
	union
	{
		struct
		{
			hawk_m4_io_arg_t arg;
			hawk_ooch_t* path;
			hawk_oow_t line;
			hawk_ooch_t buf[256];
			hawk_oow_t pos;
			hawk_oow_t len;
		} stream;
		struct
		{
			m4_str_t* str;
			hawk_oow_t pos;
		} string;
		struct
		{
			hawk_ooch_t ptr[M4_QUOTE_SIZE];
			hawk_oow_t pos;
			hawk_oow_t len;
		} replay;
	} u;
};

struct m4_oframe_t
{
	m4_oframe_t* back;
	m4_str_t* str;
};

enum m4_entry_type_t
{
	M4_ENTRY_BUILTIN,
	M4_ENTRY_MACRO
};

enum m4_entry_flag_t
{
	M4_ENTRY_FLAG_BLIND = (1 << 0)
};

struct m4_entry_t
{
	m4_entry_t* next;
	int type;
	int flags;
	union
	{
		m4_builtin_t builtin;
		m4_str_t* macro;
	} u;
	m4_str_t* name;
};

struct hawk_m4_t
{
	HAWK_M4_HDR;

	struct
	{
		hawk_oocs_t includedirs;
	} opt;

	m4_entry_t* sym[M4_HASH_SIZE];
	m4_iframe_t* input;
	m4_oframe_t* output;
	m4_str_t* diversion[M4_NDIVS];
	int divnum;
	int dnl;
	hawk_ooch_t bquote[M4_QUOTE_SIZE]; /* TODO: support dynamically sized quotes */
	hawk_ooch_t equote[M4_QUOTE_SIZE];
	hawk_oow_t bquote_len;
	hawk_oow_t equote_len;

	hawk_m4_io_impl_t io;
	hawk_m4_io_arg_t outarg;
	hawk_m4_io_arg_t errarg;
	int out_opened;
	int err_opened;
	int running;

	hawk_ooch_t outbuf[512];
	hawk_oow_t outlen;
	hawk_ooch_t* errfile;
};

static int process (hawk_m4_t* m4, int parens, hawk_ooci_t* endc);

static void set_error (hawk_m4_t* m4, hawk_errnum_t num, const hawk_bch_t* fmt, ...)
{
	hawk_loc_t loc;
	m4_iframe_t* p;
	hawk_ooch_t* errfile = HAWK_NULL;

	loc.line = 0;
	loc.colm = 0;
	loc.file = HAWK_NULL;

	for (p = m4->input; p; p = p->back)
	{
		if (p->type == M4_INPUT_STREAM)
		{
			loc.line = p->u.stream.line;
			if (p->u.stream.path)
			{
				errfile = hawk_gem_dupoocstr(hawk_m4_getgem(m4), p->u.stream.path, HAWK_NULL);
				if (!errfile) return;
				loc.file = errfile;
			}
			break;
		}
	}
	if (m4->errfile) hawk_m4_freemem(m4, m4->errfile);
	m4->errfile = errfile;

	if (fmt)
	{
		va_list ap;
		va_start(ap, fmt);
		hawk_gem_seterrbvfmt(hawk_m4_getgem(m4), &loc, num, fmt, ap);
		va_end(ap);
	}
	else hawk_gem_seterrnum(hawk_m4_getgem(m4), &loc, num);
}

static m4_str_t* str_new (hawk_m4_t* m4)
{
	m4_str_t* s;

	s = (m4_str_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*s));
	if (HAWK_UNLIKELY(!s)) return HAWK_NULL;

	s->ptr = (hawk_ooch_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*s->ptr) * M4_STR_BLOCK);
	if (HAWK_UNLIKELY(!s->ptr))
	{
		hawk_m4_freemem(m4, s);
		return HAWK_NULL;
	}

	s->refs = 1;
	s->hash = 0;
	s->len = 0;
	s->capa = M4_STR_BLOCK;
	s->ptr[0] = HAWK_T('\0');
	return s;
}

static void str_ref (m4_str_t* s)
{
	HAWK_ASSERT(s != HAWK_NULL);
	s->refs++;
}

static void str_unref (hawk_m4_t* m4, m4_str_t* s)
{
	HAWK_ASSERT(s != HAWK_NULL);
	if (--s->refs == 0)
	{
		hawk_m4_freemem(m4, s->ptr);
		hawk_m4_freemem(m4, s);
	}
}

static int str_reserve (hawk_m4_t* m4, m4_str_t* s, hawk_oow_t extra)
{
	hawk_oow_t capa;
	hawk_ooch_t* ptr;

	if (extra <= s->capa - s->len - 1) return 0;

	capa = s->capa;
	while (capa - s->len - 1 < extra)
	{
		hawk_oow_t ncapa = capa + M4_STR_BLOCK;
		if (ncapa < capa)
		{
			set_error(m4, HAWK_ENOMEM, HAWK_NULL);
			return -1;
		}
		capa = ncapa;
	}

	ptr = (hawk_ooch_t*)hawk_m4_reallocmem(m4, s->ptr, HAWK_SIZEOF(*ptr) * capa);
	if (HAWK_UNLIKELY(!ptr)) return -1;
	s->ptr = ptr;
	s->capa = capa;

	return 0;
}

static int str_append_char (hawk_m4_t* m4, m4_str_t* s, hawk_ooch_t c)
{
	if (str_reserve(m4, s, 1) <= -1) return -1;
	s->ptr[s->len++] = c;
	s->ptr[s->len] = HAWK_T('\0');
	s->hash += (hawk_uint32_t)c;
	return 0;
}

static int str_append_chars (hawk_m4_t* m4, m4_str_t* s, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t i;

	if (str_reserve(m4, s, len) <= -1) return -1;
	for (i = 0; i < len; i++) s->hash += (hawk_uint32_t)ptr[i];
	HAWK_MEMCPY(&s->ptr[s->len], ptr, HAWK_SIZEOF(*ptr) * len);
	s->len += len;
	s->ptr[s->len] = HAWK_T('\0');
	return 0;
}

static m4_str_t* str_from_cstr (hawk_m4_t* m4, const hawk_ooch_t* ptr)
{
	m4_str_t* s;

	s = str_new(m4);
	if (HAWK_UNLIKELY(!s)) return HAWK_NULL;

	if (str_append_chars(m4, s, ptr, hawk_count_oocstr(ptr)) <= -1)
	{
		str_unref(m4, s);
		return HAWK_NULL;
	}
	return s;
}

static int str_equal (const m4_str_t* x, const m4_str_t* y)
{
	if (!x || !y) return x == y;
	return x->len == y->len && hawk_comp_oochars(x->ptr, x->len, y->ptr, y->len, 0) == 0;
}

static m4_entry_t* find_entry (hawk_m4_t* m4, const m4_str_t* name)
{
	m4_entry_t* e;
	hawk_oow_t bucket = name->hash % M4_HASH_SIZE;
	for (e = m4->sym[bucket]; e; e = e->next)
	{
		if (e->name->hash == name->hash && str_equal(e->name, name)) return e;
	}
	return HAWK_NULL;
}

static int add_builtin (hawk_m4_t* m4, const hawk_ooch_t* name, m4_builtin_t builtin, int flags)
{
	m4_entry_t* e;
	hawk_oow_t bucket;

	e = (m4_entry_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*e));
	if (HAWK_UNLIKELY(!e)) return -1;

	e->name = str_from_cstr(m4, name);
	if (!e->name)
	{
		hawk_m4_freemem(m4, e);
		return -1;
	}

	bucket = e->name->hash % M4_HASH_SIZE;

	e->type = M4_ENTRY_BUILTIN;
	e->flags = flags;
	e->u.builtin = builtin;
	e->next = m4->sym[bucket];
	m4->sym[bucket] = e;
	return 0;
}

static int is_name_start (hawk_ooch_t c)
{
	return hawk_is_ooch_alpha(c) || c == HAWK_T('_');
}

static int is_name_char (hawk_ooch_t c)
{
	return is_name_start(c) || hawk_is_ooch_digit(c);
}

static int flush_primary (hawk_m4_t* m4)
{
	hawk_oow_t pos = 0;

	while (pos < m4->outlen)
	{
		hawk_ooi_t n = m4->io(m4, HAWK_M4_IO_WRITE, &m4->outarg, &m4->outbuf[pos], m4->outlen - pos);
		if (n <= 0)
		{
			set_error(m4, HAWK_EWRITE, HAWK_NULL);
			return -1;
		}
		pos += n;
	}
	m4->outlen = 0;
	return 0;
}

static int output_char (hawk_m4_t* m4, hawk_ooch_t c)
{
	if (m4->output) return str_append_char(m4, m4->output->str, c);
	if (m4->divnum < 0 || m4->divnum >= M4_NDIVS) return 0;
	if (m4->divnum > 0) return str_append_char(m4, m4->diversion[m4->divnum], c);
	m4->outbuf[m4->outlen++] = c;
	if (c == HAWK_T('\n') || m4->outlen >= HAWK_COUNTOF(m4->outbuf)) return flush_primary(m4);
	return 0;
}

static int output_chars (hawk_m4_t* m4, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t i;

	if (m4->output) return str_append_chars(m4, m4->output->str, ptr, len);
	if (m4->divnum < 0 || m4->divnum >= M4_NDIVS) return 0;
	if (m4->divnum > 0) return str_append_chars(m4, m4->diversion[m4->divnum], ptr, len);

	for (i = 0; i < len; i++)
		if (output_char(m4, ptr[i]) <= -1) return -1;

	return 0;
}

static void pop_input (hawk_m4_t* m4)
{
	m4_iframe_t* f;

	f = m4->input;
	HAWK_ASSERT(f != HAWK_NULL); /* must not call this function if the input stack is empty */

	m4->input = f->back;
	if (f->type == M4_INPUT_STRING) str_unref(m4, f->u.string.str);
	else if (f->type == M4_INPUT_STREAM)
	{
		m4->io(m4, HAWK_M4_IO_CLOSE, &f->u.stream.arg, HAWK_NULL, 0); /* ignore the error for now. TODO: enhance it */
		if (f->u.stream.path) hawk_m4_freemem(m4, f->u.stream.path);
	}

	hawk_m4_freemem(m4, f);
}

static int push_file (hawk_m4_t* m4, const hawk_ooch_t* path)
{
	m4_iframe_t* f;

	f = (m4_iframe_t*)hawk_m4_callocmem(m4, HAWK_SIZEOF(*f));
	if (HAWK_UNLIKELY(!f)) return -1;

	f->type = M4_INPUT_STREAM;
	f->u.stream.line = 1;

	if (path)
	{
		f->u.stream.path = hawk_gem_dupoocstr(hawk_m4_getgem(m4), path, HAWK_NULL);
		if (HAWK_UNLIKELY(!f->u.stream.path))
		{
			hawk_m4_freemem(m4, f);
			return -1;
		}
	}

	f->u.stream.arg.kind = HAWK_M4_IO_INPUT;
	f->u.stream.arg.path = f->u.stream.path;
	if (m4->io(m4, HAWK_M4_IO_OPEN, &f->u.stream.arg, HAWK_NULL, 0) <= -1)
	{
		if (hawk_m4_geterrnum(m4) == HAWK_ENOERR) set_error(m4, HAWK_EOPEN, HAWK_NULL);
		if (f->u.stream.path) hawk_m4_freemem(m4, f->u.stream.path);
		hawk_m4_freemem(m4, f);
		return -1;
	}
	f->back = m4->input;
	m4->input = f;
	return 0;
}

static int push_string (hawk_m4_t* m4, m4_str_t* str)
{
	m4_iframe_t* f;

	if (!str || str->len == 0) return 0;

	f = (m4_iframe_t*)hawk_m4_callocmem(m4, HAWK_SIZEOF(*f));
	if (HAWK_UNLIKELY(!f)) return -1;

	f->type = M4_INPUT_STRING;
	f->u.string.str = str;
	str_ref(str);
	f->back = m4->input;
	m4->input = f;
	return 0;
}

static int push_replay (hawk_m4_t* m4, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	m4_iframe_t* f;

	HAWK_ASSERT(len > 0 && len <= M4_QUOTE_SIZE);

	f = (m4_iframe_t*)hawk_m4_callocmem(m4, HAWK_SIZEOF(*f));
	if (HAWK_UNLIKELY(!f)) return -1;

	f->type = M4_INPUT_REPLAY;
	f->u.replay.len = len;
	HAWK_MEMCPY(f->u.replay.ptr, ptr, HAWK_SIZEOF(*ptr) * len);
	f->back = m4->input;
	m4->input = f;
	return 0;
}

static int push_capture (hawk_m4_t* m4, m4_str_t* str)
{
	m4_oframe_t* f;

	f = (m4_oframe_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*f));
	if (HAWK_UNLIKELY(!f)) return -1;

	f->str = str;
	str_ref(str);
	f->back = m4->output;
	m4->output = f;

	return 0;
}

static void pop_capture (hawk_m4_t* m4)
{
	m4_oframe_t* f = m4->output;
	m4->output = f->back;
	str_unref(m4, f->str);
	hawk_m4_freemem(m4, f);
}

static int get_char (hawk_m4_t* m4, hawk_ooci_t* ci)
{
	hawk_ooch_t c;

	while (m4->input)
	{
		m4_iframe_t* f = m4->input;

		if (f->has_unget)
		{
			f->has_unget = 0;
			c = f->unget;
			if (c == HAWK_T('\n') && f->type == M4_INPUT_STREAM) f->u.stream.line++;
		}
		else if (f->type == M4_INPUT_STRING)
		{
			if (f->u.string.pos >= f->u.string.str->len)
			{
				pop_input(m4);
				continue;
			}
			c = f->u.string.str->ptr[f->u.string.pos++];
		}
		else if (f->type == M4_INPUT_REPLAY)
		{
			if (f->u.replay.pos >= f->u.replay.len)
			{
				pop_input(m4);
				continue;
			}
			c = f->u.replay.ptr[f->u.replay.pos++];
		}
		else
		{
			if (f->u.stream.pos >= f->u.stream.len)
			{
				hawk_ooi_t n;

				n  = m4->io(m4, HAWK_M4_IO_READ, &f->u.stream.arg, f->u.stream.buf, HAWK_COUNTOF(f->u.stream.buf));
				if (n <= -1)
				{
					if (hawk_m4_geterrnum(m4) == HAWK_ENOERR) set_error(m4, HAWK_EREAD, HAWK_NULL);
					return -1; /* error */
				}
				if (n == 0)
				{
					pop_input(m4);
					continue;
				}
				f->u.stream.pos = 0;
				f->u.stream.len = n;
			}
			c = f->u.stream.buf[f->u.stream.pos++];
			if (c == HAWK_T('\n')) f->u.stream.line++;
		}

		if (!m4->dnl)
		{
			*ci = c;
			return 1;
		}

		if (c == HAWK_T('\n')) m4->dnl = 0;
	}

	*ci = HAWK_OOCI_EOF;
	return 0; /* eof */
}

static void unget_char (hawk_m4_t* m4, hawk_ooch_t c)
{
	m4_iframe_t* f = m4->input;

	HAWK_ASSERT(f && !f->has_unget);
	f->unget = c;
	f->has_unget = 1;
	if (c == HAWK_T('\n') && f->type == M4_INPUT_STREAM && f->u.stream.line > 1) f->u.stream.line--;
}

static int push_number (hawk_m4_t* m4, hawk_intmax_t value)
{
	hawk_ooch_t buf[64];
	m4_str_t* s;
	int len;

	len = hawk_fmt_intmax_to_oocstr(buf, HAWK_COUNTOF(buf), value, 10, -1, HAWK_T('\0'), HAWK_NULL);
	if (len <= -1)
	{
		set_error(m4, HAWK_EINTERN, HAWK_NULL);
		return -1;
	}

	s = str_from_cstr(m4, buf);
	if (HAWK_UNLIKELY(!s)) return -1;

	if (push_string(m4, s) <= -1)
	{
		str_unref(m4, s);
		return -1;
	}

	str_unref(m4, s);
	return 0;
}

static hawk_intmax_t string_to_int (const m4_str_t* s)
{
	hawk_oow_t i = 0;
	hawk_intmax_t n = 0;
	int neg = 0;

	while (i < s->len && hawk_is_ooch_space(s->ptr[i])) i++;
	if (i < s->len && (s->ptr[i] == HAWK_T('+') || s->ptr[i] == HAWK_T('-')))
	{
		neg = s->ptr[i++] == HAWK_T('-');
	}
	while (i < s->len && hawk_is_ooch_digit(s->ptr[i])) n = n * 10 + (s->ptr[i++] - HAWK_T('0'));
	return neg? -n: n;
}

static int expand_macro (hawk_m4_t* m4, m4_str_t* body, m4_str_t** arg)
{
	m4_str_t* out;
	hawk_oow_t i;

	out = str_new(m4);
	if (HAWK_UNLIKELY(!out)) return -1;

	for (i = 0; i < body->len; i++)
	{
		if (body->ptr[i] != HAWK_T('$'))
		{
			if (str_append_char(m4, out, body->ptr[i]) <= -1) goto oops;
		}
		else if (i + 1 >= body->len || !hawk_is_ooch_digit(body->ptr[i + 1]))
		{
			if (str_append_char(m4, out, HAWK_T('$')) <= -1) goto oops;
		}
		else
		{
			m4_str_t* a;
			hawk_oow_t idx;

			idx = 0;
			do
			{
				idx = idx * 10 + (body->ptr[++i] - HAWK_T('0'));
				if (idx >= M4_NARGS)
				{
					set_error(m4, HAWK_EINVAL, "invalid reference index $%zu", idx);
					goto oops;
				}
			}
			while ((i + 1) < body->len && hawk_is_ooch_digit(body->ptr[i + 1]));
			a = arg[idx];
			if (a && str_append_chars(m4, out, a->ptr, a->len) <= -1) goto oops;
		}
	}
	if (push_string(m4, out) <= -1) goto oops;
	str_unref(m4, out);
	return 0;

oops:
	str_unref(m4, out);
	return -1;
}

static int collect_arguments (hawk_m4_t* m4, m4_str_t* arg[M4_NARGS])
{
	int argc = 0;
	hawk_ooci_t delim;

	/* this must be called after "(" has been read */
	do
	{
		m4_str_t* a;

		/* create a buffer to hold each argument */
		a = str_new(m4);
		if (HAWK_UNLIKELY(!a)) return -1;

		/* chain the buffer to the stack of capture chain */
		if (push_capture(m4, a) <= -1)
		{
			str_unref(m4, a);
			return -1;
		}

		if (process(m4, 1, &delim) <= -1) /* TODO: remove recursion? */
		{
			pop_capture(m4);
			str_unref(m4, a);
			return -1;
		}

		HAWK_ASSERT(m4->output != HAWK_NULL && m4->output->str == a);
		pop_capture(m4);

		/* "a" holds the actual argument scanned */
		argc++;
		if (argc < M4_NARGS && a->len > 0) arg[argc] = a; /* TODO: REMOVE LIMIT */
		else str_unref(m4, a);
	}
	while (delim != HAWK_T(')') && delim != HAWK_OOCI_EOF);

	return argc;
}

enum m4_quote_match_t
{
	M4_QUOTE_ERROR = -1,
	M4_QUOTE_NONE,
	M4_QUOTE_BEGIN,
	M4_QUOTE_END
};

static int match_quote (hawk_m4_t* m4, hawk_ooch_t first, int quoted)
{
	hawk_ooch_t buf[M4_QUOTE_SIZE];
	hawk_oow_t len = 1;
	int bm = first == m4->bquote[0];
	int em = quoted && first == m4->equote[0];

	HAWK_ASSERT(bm || em);
	buf[0] = first;

	while (1)
	{
		/* Within quoted text, the ending delimiter takes precedence. If it
		 * remains a possible match, defer an exact beginning-delimiter match. */
		if (em)
		{
			if (len >= m4->equote_len) return M4_QUOTE_END;
		}
		else if (bm && len >= m4->bquote_len) return M4_QUOTE_BEGIN;

		{
			hawk_ooci_t c;
			int n = get_char(m4, &c);

			if (n <= -1) return M4_QUOTE_ERROR;
			if (n == 0) goto no_match;

			HAWK_ASSERT(len < HAWK_COUNTOF(buf));
			buf[len++] = c;
			if (bm && (len > m4->bquote_len || c != m4->bquote[len - 1])) bm = 0;
			if (em && (len > m4->equote_len || c != m4->equote[len - 1])) em = 0;
			if (!bm && !em) goto no_match;
		}
	}

no_match:
	/* The first character is returned to the caller as ordinary input. Put
	 * the lookahead back on top of the input stack for normal tokenization. */
	if (len > 1 && push_replay(m4, &buf[1], len - 1) <= -1) return M4_QUOTE_ERROR;
	return M4_QUOTE_NONE;
}

static int process (hawk_m4_t* m4, int parens, hawk_ooci_t* endc)
{
	hawk_ooci_t c, last = HAWK_OOCI_EOF;
	int quotes = 0;

	if (get_char(m4, &c) <= -1) return -1;
	if (parens)
	{
		/* skip spaces only inside () */
		do
		{
			if (c == HAWK_OOCI_EOF || !hawk_is_ooch_space(c)) break;
			if (get_char(m4, &c) <= -1) return -1;
		}
		while (1);
	}

	HAWK_ASSERT(m4->bquote_len > 0);
	HAWK_ASSERT(m4->equote_len > 0);

	while (c != HAWK_OOCI_EOF)
	{
		int qm = M4_QUOTE_NONE;

		if ((quotes > 0 && (c == m4->equote[0] || c == m4->bquote[0])) ||
		    (quotes <= 0 && c == m4->bquote[0]))
		{
			qm = match_quote(m4, c, quotes > 0);
			if (qm <= M4_QUOTE_ERROR) return -1;
		}

		if (qm == M4_QUOTE_BEGIN)
		{
			if (quotes++ > 0 && output_chars(m4, m4->bquote, m4->bquote_len) <= -1) return -1;
			last = HAWK_OOCI_EOF;
		}
		else if (qm == M4_QUOTE_END)
		{
			HAWK_ASSERT(quotes > 0);
			if (--quotes > 0 && output_chars(m4, m4->equote, m4->equote_len) <= -1) return -1;
			last = HAWK_OOCI_EOF;
		}
		else if (quotes > 0)
		{
			if (output_char(m4, c) <= -1) return -1;
		}
		else if ((c == HAWK_T(')') && parens > 0 && --parens == 0) || (c == HAWK_T(',') && parens == 1))
		{
			/* balanced closing parenthesis or a comma inside () */
			*endc = c;
			return 0;
		}
		else if (is_name_start(c) && (last == HAWK_OOCI_EOF || (!is_name_char(last))))
		{
			m4_str_t* name;
			m4_entry_t* e;
			m4_str_t* arg[M4_NARGS];
			m4_str_t* macro = HAWK_NULL;
			m4_builtin_t builtin = HAWK_NULL;
			int argc = 0;
			int type;
			int i;
			int x;

			/* scan identifier */

			name = str_new(m4);
			if (HAWK_UNLIKELY(!name)) return -1;

			do
			{
				if (str_append_char(m4, name, c) <= -1)
				{
					str_unref(m4, name);
					return -1;
				}
				last = c;
				if (get_char(m4, &c) <= -1)
				{
					str_unref(m4, name);
					return -1;
				}
			}
			while (c != HAWK_OOCI_EOF && is_name_char(c));

			e = find_entry(m4, name);
			if (!e)
			{
				/* no defintion found. output literally */
				if (output_chars(m4, name->ptr, name->len) <= -1)
				{
					str_unref(m4, name);
					return -1;
				}
				str_unref(m4, name);
				continue;
			}

			if (e->type == M4_ENTRY_BUILTIN && (e->flags & M4_ENTRY_FLAG_BLIND) && c != HAWK_T('('))
			{
				/* a builtin function that requires an opening parenthesis is not followed by it.
				 * treat it literally */
				if (output_chars(m4, name->ptr, name->len) <= -1)
				{
					str_unref(m4, name);
					return -1;
				}
				str_unref(m4, name);
				continue;
			}

			type = e->type;
			if (type == M4_ENTRY_MACRO)
			{
				macro = e->u.macro;
				str_ref(macro);
			}
			else builtin = e->u.builtin;

			for (i = 0; i < M4_NARGS; i++) arg[i] = HAWK_NULL;
			arg[0] = name;

			if (c != HAWK_T('('))
			{
				if (c != HAWK_OOCI_EOF) unget_char(m4, c);
			}
			else
			{
				/* macro_name(arg1, arg2, ...) */
				argc = collect_arguments(m4, arg);
				if (argc <= -1)
				{
					x = -1;
					goto done;
				}
			}

			x = type == M4_ENTRY_MACRO? expand_macro(m4, macro, arg): builtin(m4, arg);

		done:
			for (i = 0; i < M4_NARGS; i++)
				if (arg[i]) str_unref(m4, arg[i]);
			if (macro) str_unref(m4, macro);
			if (x <= -1) return -1;

			c = HAWK_OOCI_EOF;
		}
		else
		{
			if (c == HAWK_T('(') && parens > 0) parens++;
			if (output_char(m4, c) <= -1) return -1;
		}

		if (qm == M4_QUOTE_NONE) last = c;
		if (get_char(m4, &c) <= -1) return -1;
	}

	if (quotes > 0 || parens > 0)
	{
		/* the ending quote or the closing parenthesis is not found. unbalanced */
		set_error(m4, HAWK_EEOF, HAWK_NULL);
		return -1;
	}

	*endc = HAWK_OOCI_EOF;
	return 0;
}

static int builtin_changequote (hawk_m4_t* m4, m4_str_t** arg)
{
	const hawk_ooch_t* bptr;
	const hawk_ooch_t* eptr;
	hawk_oow_t blen;
	hawk_oow_t elen;

	/* the default beginning quote is a backquote.
	 * the default ending quote is a single quote */

	if (arg[1] && arg[1]->len > 0)
	{
		bptr = arg[1]->ptr;
		blen = arg[1]->len;
	}
	else
	{
		bptr = HAWK_NULL;
		blen = 1;
	}

	if (arg[2] && arg[2]->len > 0)
	{
		eptr = arg[2]->ptr;
		elen = arg[2]->len;
	}
	else
	{
		eptr = HAWK_NULL;
		elen = 1;
	}

	/* Validate both delimiters before changing either one. */
	if (blen > HAWK_COUNTOF(m4->bquote))
	{
		set_error(m4, HAWK_EINVAL, "beginning quote too long");
		return -1;
	}
	if (elen > HAWK_COUNTOF(m4->equote))
	{
		set_error(m4, HAWK_EINVAL, "ending quote too long");
		return -1;
	}

	if (bptr) HAWK_MEMCPY(m4->bquote, bptr, HAWK_SIZEOF(*bptr) * blen);
	else m4->bquote[0] = M4_DFL_BQUOTE;
	m4->bquote_len = blen;

	if (eptr) HAWK_MEMCPY(m4->equote, eptr, HAWK_SIZEOF(*eptr) * elen);
	else m4->equote[0] = M4_DFL_EQUOTE;
	m4->equote_len = elen;

	return 0;
}

static int valid_macro_name (const m4_str_t* name)
{
	hawk_oow_t i;
	if (!name || name->len == 0 || !is_name_start(name->ptr[0])) return 0;
	for (i = 1; i < name->len; i++) if (!is_name_char(name->ptr[i])) return 0;
	return 1;
}

static int builtin_define (hawk_m4_t* m4, m4_str_t** arg)
{
	m4_entry_t* e;
	hawk_oow_t bucket;
	m4_str_t* value;

	if (!valid_macro_name(arg[1]))
	{
		set_error(m4, HAWK_EINVAL, "invalid macro name");
		return -1;
	}

	value = arg[2];
	if (!value)
	{
		value = str_new(m4);
		if (HAWK_UNLIKELY(!value)) return -1;
	}
	else str_ref(value);

	e = find_entry(m4, arg[1]);
	if (e)
	{
		if (e->type == M4_ENTRY_MACRO) str_unref(m4, e->u.macro);

		/* override the value even if the existing defintion exists */
		e->type = M4_ENTRY_MACRO;
		e->flags = 0;
		e->u.macro = value;
		return 0;
	}

	e = (m4_entry_t*)hawk_m4_callocmem(m4, HAWK_SIZEOF(*e));
	if (HAWK_UNLIKELY(!e))
	{
		str_unref(m4, value);
		return -1;
	}

	bucket = arg[1]->hash % M4_HASH_SIZE;
	e->name = arg[1];
	str_ref(e->name);

	e->type = M4_ENTRY_MACRO;
	e->flags = 0;
	e->u.macro = value;
	e->next = m4->sym[bucket];

	m4->sym[bucket] = e;
	return 0;
}

static int builtin_pushdef (hawk_m4_t* m4, m4_str_t** arg)
{
	m4_entry_t* e;
	m4_str_t* value;
	hawk_oow_t bucket;

	if (!valid_macro_name(arg[1]))
	{
		set_error(m4, HAWK_EINVAL, "invalid macro name");
		return -1;
	}

	value = arg[2];
	if (!value)
	{
		value = str_new(m4);
		if (HAWK_UNLIKELY(!value)) return -1;
	}
	else str_ref(value);

	e = (m4_entry_t*)hawk_m4_callocmem(m4, HAWK_SIZEOF(*e));
	if (HAWK_UNLIKELY(!e))
	{
		str_unref(m4, value);
		return -1;
	}

	bucket = arg[1]->hash % M4_HASH_SIZE;
	e->name = arg[1];
	str_ref(e->name);

	e->type = M4_ENTRY_MACRO;
	e->flags = 0;
	e->u.macro = value;
	e->next = m4->sym[bucket];

	m4->sym[bucket] = e;
	return 0;
}

static int builtin_popdef (hawk_m4_t* m4, m4_str_t** arg)
{
	int i;

	for (i = 1; i < M4_NARGS; i++)
	{
		m4_entry_t* e;
		m4_entry_t* prev = HAWK_NULL;
		hawk_oow_t bucket;

		if (!arg[i]) continue;
		bucket = arg[i]->hash % M4_HASH_SIZE;
		for (e = m4->sym[bucket]; e; prev = e, e = e->next)
		{
			if (e->name->hash == arg[i]->hash && str_equal(e->name, arg[i])) break;
		}
		if (!e) continue;

		if (prev) prev->next = e->next;
		else m4->sym[bucket] = e->next;

		if (e->type == M4_ENTRY_MACRO) str_unref(m4, e->u.macro);
		str_unref(m4, e->name);
		hawk_m4_freemem(m4, e);
	}
	return 0;
}

static int builtin_divert (hawk_m4_t* m4, m4_str_t** arg)
{
	m4->divnum = arg[1]? (int)string_to_int(arg[1]): 0;
	return 0;
}

static int builtin_divnum (hawk_m4_t* m4, m4_str_t** arg)
{
	(void)arg;
	return push_number(m4, m4->divnum);
}

static int builtin_dnl (hawk_m4_t* m4, m4_str_t** arg)
{
	(void)arg;
	m4->dnl = 1;
	return 0;
}

static int output_definition (hawk_m4_t* m4, m4_entry_t* e)
{
	if (e->type != M4_ENTRY_MACRO) return 0;
	if (output_chars(m4, m4->bquote, m4->bquote_len) <= -1) return -1;
	if (output_chars(m4, e->u.macro->ptr, e->u.macro->len) <= -1) return -1;
	return output_chars(m4, m4->equote, m4->equote_len);
}

static int builtin_dumpdef (hawk_m4_t* m4, m4_str_t** arg)
{
	int i, found = 0;

	for (i = 1; i < M4_NARGS; i++)
	{
		m4_entry_t* e;
		if (!arg[i]) continue;
		e = find_entry(m4, arg[i]);
		if (e && output_definition(m4, e) <= -1) return -1;
		found = 1;
	}
	if (!found)
	{
		for (i = 0; i < M4_HASH_SIZE; i++)
		{
			m4_entry_t* e;
			for (e = m4->sym[i]; e; e = e->next)
			{
				if (output_chars(m4, m4->bquote, m4->bquote_len) <= -1 ||
				    output_chars(m4, e->name->ptr, e->name->len) <= -1 ||
				    output_chars(m4, m4->equote, m4->equote_len) <= -1 ||
				    output_char(m4, HAWK_T('\t')) <= -1 ||
				    output_definition(m4, e) <= -1 ||
				    output_char(m4, HAWK_T('\n')) <= -1) return -1;
			}
		}
	}
	return 0;
}

static int write_error (hawk_m4_t* m4, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t pos = 0;
	while (pos < len)
	{
		hawk_ooi_t n = m4->io(m4, HAWK_M4_IO_WRITE, &m4->errarg, (hawk_ooch_t*)&ptr[pos], len - pos);
		if (n <= 0)
		{
			set_error(m4, HAWK_EWRITE, HAWK_NULL);
			return -1;
		}
		pos += n;
	}
	return 0;
}

static int builtin_errprint (hawk_m4_t* m4, m4_str_t** arg)
{
	int i;
	for (i = 1; i < M4_NARGS; i++)
	{
		if (arg[i] && write_error(m4, arg[i]->ptr, arg[i]->len) <= -1) return -1;
	}
	return 0;
}

enum calc_op_t
{
	CALC_EX,
	CALC_MUL,
	CALC_DIV,
	CALC_MOD,
	CALC_ADD,
	CALC_SUB,
	CALC_EQ,
	CALC_NE,
	CALC_LE,
	CALC_GE,
	CALC_LT,
	CALC_GT,
	CALC_AND,
	CALC_OR,
	CALC_RP,
	CALC_END
};
typedef enum calc_op_t calc_op_t;

struct calc_op_data_t {
	calc_op_t op;
	hawk_ooch_t first;
	hawk_ooch_t second;
	int prec;
};

static const struct calc_op_data_t calc_ops[] =
{
	{ CALC_EX,  HAWK_T('^'), HAWK_T('\0'), 7 },
	{ CALC_EX,  HAWK_T('*'), HAWK_T('*'),  7 },
	{ CALC_MUL, HAWK_T('*'), HAWK_T('\0'), 6 },
	{ CALC_DIV, HAWK_T('/'), HAWK_T('\0'), 6 },
	{ CALC_MOD, HAWK_T('%'), HAWK_T('\0'), 6 },
	{ CALC_ADD, HAWK_T('+'), HAWK_T('\0'), 5 },
	{ CALC_SUB, HAWK_T('-'), HAWK_T('\0'), 5 },
	{ CALC_EQ,  HAWK_T('='), HAWK_T('='),  4 },
	{ CALC_GE,  HAWK_T('>'), HAWK_T('='),  4 },
	{ CALC_GT,  HAWK_T('>'), HAWK_T('\0'), 4 },
	{ CALC_NE,  HAWK_T('!'), HAWK_T('='),  4 },
	{ CALC_LE,  HAWK_T('<'), HAWK_T('='),  4 },
	{ CALC_LT,  HAWK_T('<'), HAWK_T('\0'), 4 },
	{ CALC_AND, HAWK_T('&'), HAWK_T('&'),  2 },
	{ CALC_AND, HAWK_T('&'), HAWK_T('\0'), 2 },
	{ CALC_OR,  HAWK_T('|'), HAWK_T('|'),  1 },
	{ CALC_OR,  HAWK_T('|'), HAWK_T('\0'), 1 },
	{ CALC_RP,  HAWK_T(')'), HAWK_T('\0'), 10 },
	{ CALC_END, HAWK_T('\0'), HAWK_T('\0'), 0 }
};

static hawk_intmax_t calculate (hawk_m4_t* m4, int prec, const hawk_ooch_t** p, int* ok)
{
	const hawk_ooch_t* s = *p;
	hawk_intmax_t lhs = 0;
	hawk_ooch_t c;

	while (hawk_is_ooch_space(*s)) s++;

	c = *s++;
	if (hawk_is_ooch_digit(c))
	{
		lhs = c - HAWK_T('0');
		while (hawk_is_ooch_digit(*s)) lhs = lhs * 10 + (*s++ - HAWK_T('0'));
	}
	else if (c == HAWK_T('+'))
	{
		lhs = calculate(m4, 8, &s, ok);
	}
	else if (c == HAWK_T('-'))
	{
		lhs = -calculate(m4, 8, &s, ok);
	}
	else if (c == HAWK_T('!'))
	{
		lhs = !calculate(m4, 3, &s, ok);
	}
	else if (c == HAWK_T('('))
	{
		lhs = calculate(m4, 0, &s, ok);
	}
	else
	{
		set_error(m4, HAWK_EINVAL, "invalid operand");
		*ok = 0;
		return 0;
	}
	if (!*ok) return 0;

	while (1)
	{
		const struct calc_op_data_t* op;
		hawk_intmax_t rhs, power;
		int oplen = 1;

		while (hawk_is_ooch_space(*s)) s++;

		c = *s;
		for (op = calc_ops; op->op != CALC_END; op++)
		{
			if (c != op->first) continue;
			if (op->second == HAWK_T('\0')) break;
			if (s[1] == op->second) { oplen = 2; break; }
		}

		if (op->op == CALC_END)
		{
			if (c == HAWK_T('\0'))
			{
				*p = s;
				return lhs;
			}
			set_error(m4, HAWK_EINVAL, "invalid operator in expression - %jc", c);
			*ok = 0;
			return 0;
		}
		if (op->prec <= prec)
		{
			*p = s;
			return lhs;
		}
		s += oplen;

		if (op->op == CALC_RP)
		{
			*p = s;
			return lhs;
		}

		rhs = calculate(m4, op->prec, &s, ok);
		if (!*ok) return 0;

		switch (op->op)
		{
			case CALC_EX:
				if (rhs < 0) lhs = 0;
				else
				{
					for (power = 1; rhs > 0; rhs--) power *= lhs;
					lhs = power;
				}
				break;

			case CALC_MUL:
				lhs *= rhs;
				break;

			case CALC_DIV:
				if (rhs == 0)
				{
					set_error(m4, HAWK_EDIVBY0, HAWK_NULL);
					*ok = 0;
					return 0;
				}
				lhs /= rhs;
				break;

			case CALC_MOD:
				if (rhs == 0)
				{
					set_error(m4, HAWK_EDIVBY0, HAWK_NULL);
					*ok = 0;
					return 0;
				}
				lhs %= rhs;
				break;

			case CALC_ADD:
				lhs += rhs;
				break;

			case CALC_SUB:
				lhs -= rhs;
				break;

			case CALC_EQ:
				lhs = lhs == rhs;
				break;

			case CALC_NE:
				lhs = lhs != rhs;
				break;

			case CALC_LE:
				lhs = lhs <= rhs;
				break;

			case CALC_GE:
				lhs = lhs >= rhs;
				break;

			case CALC_LT:
				lhs = lhs < rhs;
				break;

			case CALC_GT:
				lhs = lhs > rhs;
				break;

			case CALC_AND:
				lhs = lhs && rhs;
				break;

			case CALC_OR:
				lhs = lhs || rhs;
				break;

			case CALC_RP:
			case CALC_END:
				HAWK_ASSERT(!"should never happen - non operator opcode encountered");
				break;
		}
	}
}

static int builtin_eval (hawk_m4_t* m4, m4_str_t** arg)
{
	const hawk_ooch_t* p;
	hawk_intmax_t n;
	int ok = 1;

	if (!arg[1]) return push_number(m4, 0);

	p = arg[1]->ptr;
	n = calculate(m4, 0, &p, &ok);
	if (!ok) return -1;

	while (hawk_is_ooch_space(*p)) p++;
	if (*p != HAWK_T('\0'))
	{
		set_error(m4, HAWK_EINVAL, "invalid trailing data in expression - %js", p);
		return -1;
	}

	return push_number(m4, n);
}

static int builtin_ifdef (hawk_m4_t* m4, m4_str_t** arg)
{
	return push_string(m4, arg[1] && find_entry(m4, arg[1])? arg[2]: arg[3]);
}

static int more_args (m4_str_t** arg, int from)
{
	int i;
	for (i = from; i < M4_NARGS; i++) if (arg[i]) return 1;
	return 0;
}

static int builtin_ifelse (hawk_m4_t* m4, m4_str_t** arg)
{
	if (str_equal(arg[1], arg[2])) return push_string(m4, arg[3]);

	if (more_args(arg, 5))
	{
		if (str_equal(arg[4], arg[5])) return push_string(m4, arg[6]);
		if (more_args(arg, 8)) return str_equal(arg[7], arg[8])? push_string(m4, arg[9]): 0;
		return push_string(m4, arg[7]);
	}

	return push_string(m4, arg[4]);
}

static int do_include (hawk_m4_t* m4, m4_str_t** arg, int silent)
{
	if (!arg[1])
	{
		if (silent) return 0;
		set_error(m4, HAWK_EINVAL, "file name required for include");
		return -1;
	}
	if (push_file(m4, arg[1]->ptr) <= -1)
	{
		if (silent)
		{
			hawk_gem_seterrnum(hawk_m4_getgem(m4), HAWK_NULL, HAWK_ENOERR);
			return 0;
		}
		return -1;
	}
	return 0;
}

static int builtin_include (hawk_m4_t* m4, m4_str_t** arg)
{
	return do_include(m4, arg, 0);
}

static int builtin_sinclude (hawk_m4_t* m4, m4_str_t** arg)
{
	return do_include(m4, arg, 1);
}

static int builtin_incr (hawk_m4_t* m4, m4_str_t** arg)
{
	return push_number(m4, arg[1]? string_to_int(arg[1]) + 1: 1);
}

static int builtin_decr (hawk_m4_t* m4, m4_str_t** arg)
{
	return push_number(m4, arg[1]? string_to_int(arg[1]) - 1: -1);
}

static int builtin_index (hawk_m4_t* m4, m4_str_t** arg)
{
	hawk_oow_t i;

	if (!arg[2]) return push_number(m4, 0);
	if (!arg[1] || arg[2]->len > arg[1]->len) return push_number(m4, -1);
	for (i = 0; i + arg[2]->len <= arg[1]->len; i++)
		if (hawk_comp_oochars(&arg[1]->ptr[i], arg[2]->len, arg[2]->ptr, arg[2]->len, 0) == 0) return push_number(m4, i);
	return push_number(m4, -1);
}

static int builtin_len (hawk_m4_t* m4, m4_str_t** arg)
{
	return push_number(m4, arg[1]? arg[1]->len: 0);
}

static int builtin_substr (hawk_m4_t* m4, m4_str_t** arg)
{
	hawk_intmax_t start, count, len;
	m4_str_t* out;

	if (!arg[1]) return 0;

	len = arg[1]->len;
	if (!arg[2])
	{
		count = arg[3]? string_to_int(arg[3]): len;
		start = count > 0? 0: -1;
	}
	else
	{
		start = string_to_int(arg[2]);
		count = arg[3]? string_to_int(arg[3]): (start > 0? len: -len);
	}
	if (count == 0) return 0;
	if (start < 0) start += len;
	if (count < 0) { start += count + 1; count = -count; }
	if (start < 0) { count += start; start = 0; }
	if (start >= len || count <= 0) return 0;
	if (count > len - start) count = len - start;
	out = str_new(m4);
	if (!out) return -1;

	if (str_append_chars(m4, out, &arg[1]->ptr[start], count) <= -1 || push_string(m4, out) <= -1)
	{
		str_unref(m4, out);
		return -1;
	}
	str_unref(m4, out);
	return 0;
}

static int builtin_syscmd (hawk_m4_t* m4, m4_str_t** arg)
{
	hawk_m4_io_arg_t ioarg;

	if (!arg[1]) return 0;

	if (flush_primary(m4) <= -1) return -1;

	HAWK_MEMSET(&ioarg, 0, HAWK_SIZEOF(ioarg));
	ioarg.kind = HAWK_M4_IO_OUTPUT;
	if (m4->io(m4, HAWK_M4_IO_SYSCMD, &ioarg, arg[1]->ptr, arg[1]->len) <= -1)
	{
		if (hawk_m4_geterrnum(m4) == HAWK_ENOERR) set_error(m4, HAWK_ESYSERR, HAWK_NULL);
		return -1;
	}
	return 0;
}

static int builtin_translit (hawk_m4_t* m4, m4_str_t** arg)
{
	m4_str_t* out;
	hawk_oow_t i, j;

	if (!arg[1] || !arg[2]) return 0;

	out = str_new(m4);
	if (!out) return -1;
	for (i = 0; i < arg[1]->len; i++)
	{
		for (j = 0; j < arg[2]->len; j++) if (arg[1]->ptr[i] == arg[2]->ptr[j]) break;
		if (j >= arg[2]->len)
		{
			if (str_append_char(m4, out, arg[1]->ptr[i]) <= -1) goto oops;
		}
		else if (arg[3] && j < arg[3]->len && str_append_char(m4, out, arg[3]->ptr[j]) <= -1) goto oops;
	}
	if (push_string(m4, out) <= -1) goto oops;
	str_unref(m4, out);
	return 0;

oops:
	str_unref(m4, out);
	return -1;
}

static int builtin_undefine (hawk_m4_t* m4, m4_str_t** arg)
{
	int i;

	for (i = 1; i < M4_NARGS; i++)
	{
		m4_entry_t* e;
		m4_entry_t* prev = HAWK_NULL;
		hawk_oow_t bucket;

		if (!arg[i]) continue;
		bucket = arg[i]->hash % M4_HASH_SIZE;
		e = m4->sym[bucket];
		while (e)
		{
			m4_entry_t* next = e->next;
			if (e->name->hash == arg[i]->hash && str_equal(e->name, arg[i]))
			{
				if (prev) prev->next = next;
				else m4->sym[bucket] = next;

				if (e->type == M4_ENTRY_MACRO) str_unref(m4, e->u.macro);
				str_unref(m4, e->name);
				hawk_m4_freemem(m4, e);
			}
			else prev = e;
			e = next;
		}
	}
	return 0;
}

int hawk_m4_define (hawk_m4_t* m4, const hawk_ooch_t* name, const hawk_ooch_t* value)
{
	m4_str_t* arg[M4_NARGS];
	int i, n;

	if (!name)
	{
		set_error(m4, HAWK_EINVAL, "invalid macro name");
		return -1;
	}

	for (i = 0; i < M4_NARGS; i++) arg[i] = HAWK_NULL;
	arg[1] = str_from_cstr(m4, name);
	if (!arg[1]) return -1;
	if (value)
	{
		arg[2] = str_from_cstr(m4, value);
		if (!arg[2])
		{
			str_unref(m4, arg[1]);
			return -1;
		}
	}

	n = builtin_define(m4, arg);
	if (arg[2]) str_unref(m4, arg[2]);
	str_unref(m4, arg[1]);
	return n;
}

int hawk_m4_undefine (hawk_m4_t* m4, const hawk_ooch_t* name)
{
	m4_str_t* arg[M4_NARGS];
	int i, n;

	if (!name)
	{
		set_error(m4, HAWK_EINVAL, "invalid macro name");
		return -1;
	}

	for (i = 0; i < M4_NARGS; i++) arg[i] = HAWK_NULL;
	arg[1] = str_from_cstr(m4, name);
	if (!arg[1]) return -1;
	n = builtin_undefine(m4, arg);
	str_unref(m4, arg[1]);
	return n;
}

static int undivert_one (hawk_m4_t* m4, int n)
{
	m4_str_t* s;

	if (n <= 0 || n >= M4_NDIVS || n == m4->divnum) return 0;

	s = m4->diversion[n];
	if (s->len > 0 && output_chars(m4, s->ptr, s->len) <= -1) return -1;
	s->len = 0;
	s->hash = 0;
	s->ptr[0] = HAWK_T('\0');
	return 0;
}

static int builtin_undivert (hawk_m4_t* m4, m4_str_t** arg)
{
	int i, found = 0;
	if (arg)
	{
		for (i = 1; i < M4_NARGS; i++)
		{
			if (!arg[i]) continue;
			if (undivert_one(m4, (int)string_to_int(arg[i])) <= -1) return -1;
			found = 1;
		}
	}
	if (!found)
	{
		for (i = 1; i < M4_NDIVS; i++)
		{
			if (undivert_one(m4, i) <= -1) return -1;
		}
	}
	return 0;
}

static void clear_runtime (hawk_m4_t* m4)
{
	while (m4->output) pop_capture(m4);
	while (m4->input) pop_input(m4);
	m4->dnl = 0;
	m4->divnum = 0;
	m4->outlen = 0;
}

static void clear_symbols (hawk_m4_t* m4)
{
	int i;
	for (i = 0; i < M4_HASH_SIZE; i++)
	{
		m4_entry_t* e = m4->sym[i];
		while (e)
		{
			m4_entry_t* next = e->next;
			if (e->type == M4_ENTRY_MACRO) str_unref(m4, e->u.macro);
			str_unref(m4, e->name);
			hawk_m4_freemem(m4, e);
			e = next;
		}
		m4->sym[i] = HAWK_NULL;
	}
}

static int init_builtins (hawk_m4_t* m4)
{
	return
		add_builtin(m4, HAWK_T("changequote"), builtin_changequote, 0)                   <= -1 ||
		add_builtin(m4, HAWK_T("decr"),        builtin_decr,        M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("define"),      builtin_define,      M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("divert"),      builtin_divert,      0)                   <= -1 ||
		add_builtin(m4, HAWK_T("divnum"),      builtin_divnum,      0)                   <= -1 ||
		add_builtin(m4, HAWK_T("dnl"),         builtin_dnl,         0)                   <= -1 ||
		add_builtin(m4, HAWK_T("dumpdef"),     builtin_dumpdef,     0)                   <= -1 ||
		add_builtin(m4, HAWK_T("errprint"),    builtin_errprint,    M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("eval"),        builtin_eval,        M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("ifdef"),       builtin_ifdef,       M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("ifelse"),      builtin_ifelse,      M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("include"),     builtin_include,     M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("incr"),        builtin_incr,        M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("index"),       builtin_index,       M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("len"),         builtin_len,         M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("popdef"),      builtin_popdef,      M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("pushdef"),     builtin_pushdef,     M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("sinclude"),    builtin_sinclude,    M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("substr"),      builtin_substr,      M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("syscmd"),      builtin_syscmd,      M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("translit"),    builtin_translit,    M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("undefine"),    builtin_undefine,    M4_ENTRY_FLAG_BLIND) <= -1 ||
		add_builtin(m4, HAWK_T("undivert"),    builtin_undivert,    0)                   <= -1? -1: 0;
}

hawk_m4_t* hawk_m4_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	hawk_m4_t* m4;
	int i;

	if (!mmgr) mmgr = hawk_get_sys_mmgr();
	if (!cmgr) cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
	m4 = (hawk_m4_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(*m4) + xtnsize);
	if (!m4)
	{
		if (errinf)
		{
			HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
			errinf->num = HAWK_ENOMEM;
			hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(HAWK_ENOMEM));
		}
		return HAWK_NULL;
	}
	HAWK_MEMSET(m4, 0, HAWK_SIZEOF(*m4));
	m4->instsize_ = HAWK_SIZEOF(*m4);
	m4->gem_.mmgr = mmgr;
	m4->gem_.cmgr = cmgr;
	m4->gem_.errnum = HAWK_ENOERR;
	m4->gem_.errstr = hawk_dfl_errstr;

	m4->bquote[0] = M4_DFL_BQUOTE;
	m4->bquote_len = 1;
	m4->equote[0] = M4_DFL_EQUOTE;
	m4->equote_len = 1;

	for (i = 1; i < M4_NDIVS; i++)
	{
		/* [NOTE] m4->diversion[0] is left unused. */
		m4->diversion[i] = str_new(m4);
		if (HAWK_UNLIKELY(!m4->diversion[i])) goto oops;
	}
	if (init_builtins(m4) <= -1) goto oops;
	HAWK_MEMSET(m4 + 1, 0, xtnsize);
	return m4;

oops:
	if (errinf) hawk_m4_geterrinf(m4, errinf);
	clear_symbols(m4);
	for (i = 1; i < M4_NDIVS; i++)
	{
		if (m4->diversion[i])
			str_unref(m4, m4->diversion[i]);
	}
	HAWK_MMGR_FREE(mmgr, m4);
	return HAWK_NULL;
}

void hawk_m4_close (hawk_m4_t* m4)
{
	int i;

	clear_runtime(m4);
	clear_symbols(m4);
	if (m4->opt.includedirs.ptr) hawk_m4_freemem(m4, m4->opt.includedirs.ptr);
	for (i = 1; i < M4_NDIVS; i++)
	{
		if (m4->diversion[i])
			str_unref(m4, m4->diversion[i]);
	}
	if (m4->errfile) hawk_m4_freemem(m4, m4->errfile);
	HAWK_MMGR_FREE(hawk_m4_getmmgr(m4), m4);
}

int hawk_m4_setopt (hawk_m4_t* m4, hawk_m4_opt_t id, const void* value)
{
	switch (id)
	{
		case HAWK_M4_OPT_INCDIRS:
		{
			hawk_oocs_t tmp;

			if (value)
			{
				tmp.len = hawk_count_oocstr((const hawk_ooch_t*)value);
				tmp.ptr = (hawk_ooch_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*tmp.ptr) * (tmp.len + 1));
				if (!tmp.ptr) return -1;
				hawk_copy_oocstr_unlimited(tmp.ptr, (const hawk_ooch_t*)value);
			}
			else
			{
				tmp.ptr = HAWK_NULL;
				tmp.len = 0;
			}

			if (m4->opt.includedirs.ptr) hawk_m4_freemem(m4, m4->opt.includedirs.ptr);
			m4->opt.includedirs = tmp;
			return 0;
		}
	}

	set_error(m4, HAWK_EINVAL, "invalid m4 option - %d", (int)id);
	return -1;
}

int hawk_m4_getopt (hawk_m4_t* m4, hawk_m4_opt_t id, void* value)
{
	switch (id)
	{
		case HAWK_M4_OPT_INCDIRS:
			*(const hawk_ooch_t**)value = m4->opt.includedirs.ptr;
			return 0;
	}

	set_error(m4, HAWK_EINVAL, "invalid m4 option - %d", (int)id);
	return -1;
}

/*int hawk_m4_comp (hawk_m4_t* m4)
{
	read a separate definition file?
}*/

int hawk_m4_exec (hawk_m4_t* m4, const hawk_ooch_t* const input[], hawk_m4_io_impl_t io)
{
	hawk_oow_t i;
	hawk_ooci_t endc;
	int ret = -1;

	if (!io || m4->running)
	{
		hawk_gem_seterrnum(hawk_m4_getgem(m4), HAWK_NULL, m4->running? HAWK_EBUSY: HAWK_EINVAL);
		return -1;
	}

	if (m4->errfile)
	{
		hawk_m4_freemem(m4, m4->errfile);
		m4->errfile = HAWK_NULL;
	}

	hawk_gem_seterrnum(hawk_m4_getgem(m4), HAWK_NULL, HAWK_ENOERR);
	clear_runtime(m4);

	for (i = 1; i < M4_NDIVS; i++)
	{
		m4->diversion[i]->len = 0;
		m4->diversion[i]->hash = 0;
		m4->diversion[i]->ptr[0] = HAWK_T('\0');
	}
	m4->io = io;
	m4->running = 1; /* to prevent double calls */
	m4->outarg.kind = HAWK_M4_IO_OUTPUT;
	if (io(m4, HAWK_M4_IO_OPEN, &m4->outarg, HAWK_NULL, 0) <= -1)
	{
		if (hawk_m4_geterrnum(m4) == HAWK_ENOERR) set_error(m4, HAWK_EOPEN, HAWK_NULL);
		goto done;
	}
	m4->out_opened = 1;

	m4->errarg.kind = HAWK_M4_IO_ERROR;
	if (io(m4, HAWK_M4_IO_OPEN, &m4->errarg, HAWK_NULL, 0) <= -1)
	{
		if (hawk_m4_geterrnum(m4) == HAWK_ENOERR) set_error(m4, HAWK_EOPEN, HAWK_NULL);
		goto done;
	}
	m4->err_opened = 1;

	if (!input || !input[0])
	{
		if (push_file(m4, HAWK_NULL) <= -1 || process(m4, 0, &endc) <= -1) goto done;
	}
	else
	{
		for (i = 0; input[i]; i++)
		{
			if (push_file(m4, input[i]) <= -1 || process(m4, 0, &endc) <= -1) goto done;
		}
	}
	m4->divnum = 0;
	if (builtin_undivert(m4, HAWK_NULL) <= -1 || flush_primary(m4) <= -1) goto done;
	ret = 0;

done:
	clear_runtime(m4);
	if (m4->err_opened)
	{
		io(m4, HAWK_M4_IO_CLOSE, &m4->errarg, HAWK_NULL, 0);
		m4->err_opened = 0;
	}
	if (m4->out_opened)
	{
		io(m4, HAWK_M4_IO_CLOSE, &m4->outarg, HAWK_NULL, 0);
		m4->out_opened = 0;
	}
	m4->running = 0;
	m4->io = HAWK_NULL;
	return ret;
}

void hawk_m4_seterrbfmt (hawk_m4_t* m4, const hawk_loc_t* loc, hawk_errnum_t num, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hawk_gem_seterrbvfmt(hawk_m4_getgem(m4), loc, num, fmt, ap);
	va_end(ap);
}

void hawk_m4_seterrufmt (hawk_m4_t* m4, const hawk_loc_t* loc, hawk_errnum_t num, const hawk_uch_t* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hawk_gem_seterruvfmt(hawk_m4_getgem(m4), loc, num, fmt, ap);
	va_end(ap);
}

void* hawk_m4_allocmem (hawk_m4_t* m4, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(hawk_m4_getmmgr(m4), size);
	if (HAWK_UNLIKELY(!ptr)) hawk_m4_seterrnum(m4, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_m4_callocmem (hawk_m4_t* m4, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(hawk_m4_getmmgr(m4), size);
	if (ptr) HAWK_MEMSET(ptr, 0, size);
	else hawk_m4_seterrnum(m4, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_m4_reallocmem (hawk_m4_t* m4, void* ptr, hawk_oow_t size)
{
	void* nptr = HAWK_MMGR_REALLOC(hawk_m4_getmmgr(m4), ptr, size);
	if (HAWK_UNLIKELY(!nptr)) hawk_m4_seterrnum(m4, HAWK_NULL, HAWK_ENOMEM);
	return nptr;
}

void hawk_m4_freemem (hawk_m4_t* m4, void* ptr)
{
	HAWK_MMGR_FREE(hawk_m4_getmmgr(m4), ptr);
}
