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
#include "sh-prv.h"

#define SH_DFL_BUFFER_CAPA (64)

enum token_t
{
	TOK_ERROR = -1,
	TOK_END = 0,
	TOK_WORD,
	TOK_IO_NUMBER,
	TOK_NL,
	TOK_SEMI,
	TOK_PIPE, /* | */
	TOK_BG, /* & */
	TOK_AND, /* && */
	TOK_OR, /* || */
	TOK_REDIR /* >, >> */
};
typedef enum token_t token_t;

typedef struct parser_t parser_t;
typedef struct list_t list_t;
typedef struct string_input_t string_input_t;

struct parser_t
{
	hawk_sh_t* sh;
	hawk_sh_input_t input;
	void* ctx;

	hawk_ooci_t ch;

	int have_ch;
	int failed;
	int strip;
	int input_redir;

	token_t token;
	int rtype; /* redirection type. used if token is TOK_REDIR */

	hawk_ooecs_t word;
};

struct list_t
{
	list_t* next;
	hawk_sh_pipeline_t pipeline;
	int condition;
};

struct string_input_t
{
	const hawk_ooch_t* ptr;
};

static void set_error (hawk_sh_t* sh, hawk_errnum_t num, const hawk_bch_t* fmt, ...)
{
	hawk_loc_t loc;

	loc.line = sh->line;
	loc.colm = 0;
	loc.file = HAWK_NULL;

	if (fmt)
	{
		va_list ap;

		va_start(ap, fmt);
		hawk_gem_seterrbvfmt(&sh->gem_, &loc, num, fmt, ap);
		va_end(ap);
	}
	else hawk_gem_seterrnum(&sh->gem_, &loc, num);
}

static hawk_ooci_t peek_char (parser_t* p)
{
	if (p->failed) return HAWK_OOCI_EOF;

	if (!p->have_ch)
	{
		hawk_ooch_t ch;

		int n = p->input(p->sh, &ch, p->ctx);
		if (n <= -1) /* failure */
		{
			if (hawk_sh_geterrnum(p->sh) == HAWK_ENOERR) set_error(p->sh, HAWK_EREAD, HAWK_NULL);
			p->ch = HAWK_OOCI_EOF;
			p->failed = 1;
		}
		else if (n == 0) /* eof */
		{
			p->ch = HAWK_OOCI_EOF;
		}
		else
		{
			p->ch = (hawk_oochu_t)ch;
		}
		p->have_ch = 1;

#if 0
		if (n > 0 && ch == 0)
		{
			set_error(p->sh, HAWK_EINVAL, "NUL in shell input");
			p->failed = 1;
			p->ch = -1;
		}
#endif
	}

	return p->ch;
}

static hawk_ooci_t read_char (parser_t* p)
{
	hawk_ooci_t ci;
	ci = peek_char(p);
	p->have_ch = 0;
	if (ci == '\n') p->sh->line++; /* TODO: support different line termination convention */
	return ci;
}

static int is_special_char (hawk_ooci_t ch)
{
	return ch == HAWK_OOCI_EOF || hawk_is_ooch_space((hawk_oochu_t)ch) ||
	       ch == ';' || ch == '|' || ch == '&' || ch == '<' || ch == '>';
}

static int lex (parser_t* p)
{
	hawk_ooci_t ci;
	hawk_ooci_t nc;
	int quote;
	int digits;

restart:
	quote = 0;
	digits = 1;
	hawk_ooecs_clear(&p->word);

	/* skip comments and spaces */
	while (1)
	{
		ci = peek_char(p);
		if (hawk_is_ooch_space((hawk_oochu_t)ci) && ci != '\n')
		{
			read_char(p);
			continue;
		}
		if (ci == '#')
		{
			while (peek_char(p) >= 0 && peek_char(p) != '\n') /* TODO: different line termination convention */
				read_char(p);
			continue;
		}
		break;
	}


	/* process meaningful input */
	ci = read_char(p);
	switch (ci)
	{
		case HAWK_OOCI_EOF:
			return p->failed? -1: (p->token = TOK_END);

		case '\n':
			return p->token = TOK_NL;

		case ';':
			return p->token = TOK_SEMI;

		case '|':
			if (peek_char(p) == '|') /* || */
			{
				read_char(p);
				return p->token = TOK_OR;
			}

			if (p->failed) return -1;
			p->token = TOK_PIPE;
			return p->token;

		case '&':
/* TODO: support &> , short form of >file 2>&1
 *               &>> , short form of >>file 2>&1 */
			if (peek_char(p) == '&') /* && */
			{
				read_char(p);
				return p->token = TOK_AND;
			}
			if (p->failed) return -1;

			p->token = TOK_BG;
			return p->token;

		case '<':
		case '>':
			p->strip = 0;
			p->input_redir = ci == '<';
			p->rtype = ci == '<'? HAWK_SH_IN: HAWK_SH_OUT;
			if ((nc = peek_char(p)) == ci) /* << or >> */
			{
/* TODO: support <<<, here string */
				read_char(p);
				p->rtype = ci == '<'? HAWK_SH_HEREDOC: HAWK_SH_APPEND;
				if (ci == '<' && peek_char(p) == '-') /* <<- */
				{
					/* heredoc to strip leading tab characters from every line */
					read_char(p);
					p->strip = 1;
				}
			}
			else if (nc == '&') /* <& or >& */
			{
				read_char(p);
				p->rtype = HAWK_SH_DUP;
			}
			else if (ci == '<' && nc == '>') /* <> */
			{
				read_char(p);
				p->rtype = HAWK_SH_INOUT;
			}

			if (p->failed) return -1;

			p->token = TOK_REDIR;
			return p->token;
	}

	while (1)
	{
		if (ci == '\\' && quote != '\'')
		{
			hawk_ooci_t next = read_char(p);

			if (next == HAWK_OOCI_EOF)
			{
				if (!p->failed) set_error(p->sh, HAWK_EEOF, "unfinished escape");
				return -1;
			}
			if (next != '\n')
			{
				if (hawk_ooecs_ccat(&p->word, ci) == (hawk_oow_t)-1 ||
				    hawk_ooecs_ccat(&p->word, next) == (hawk_oow_t)-1) return -1;
				digits = 0;
			}
		}
		else
		{
			if (!quote && (ci == '\'' || ci == '"')) quote = ci;
			else if (ci == quote) quote = 0;
			if (hawk_ooecs_ccat(&p->word, ci) == (hawk_oow_t)-1) return -1;
			if (ci < '0' || ci > '9') digits = 0;
		}
		ci = peek_char(p);
		if (p->failed) return -1;
		if (!quote && is_special_char(ci)) break;
		if (ci == HAWK_OOCI_EOF)
		{
			set_error(p->sh, HAWK_EEOF, "unterminated quote");
			return -1;
		}
		ci = read_char(p);
	}
	if (p->failed) return -1;
	if (HAWK_OOECS_LEN(&p->word) == 0) goto restart;

	return p->token = digits && (ci == '<' || ci == '>')? TOK_IO_NUMBER: TOK_WORD;
}

/* Expansion is delayed until execution: export X=...; echo "$X" works. */
static hawk_ooch_t* expand (hawk_sh_t* sh, const hawk_ooch_t* s, int variables, int heredoc)
{
	hawk_ooecs_t b, name;
	hawk_ooch_t* result;
	int quote = 0;

	/* A nonzero capacity also gives an empty expansion an owned string. */
	if (hawk_ooecs_init(&b, &sh->gem_, SH_DFL_BUFFER_CAPA) <= -1) return HAWK_NULL;
	hawk_ooecs_init(&name, &sh->gem_, 0);

	while (*s)
	{
		hawk_ooch_t ch = *s++;

		if (!heredoc && (ch == '\'' || ch == '"'))
		{
			if (!quote)
			{
				quote = ch;
				continue;
			}
			if (quote == ch)
			{
				quote = 0;
				continue;
			}
		}
		if (ch == '\\' && quote != '\'' && *s)
		{
			if (!quote && !heredoc) ch = *s++;
			else if (*s == '$' || *s == '\\' || *s == '\n' || (!heredoc && *s == '"')) ch = *s++;
			if (ch == '\n') continue;
		}
		else if (ch == '$' && quote != '\'' && variables)
		{
			const hawk_ooch_t* start;
			const hawk_ooch_t* value;
			int brace = *s == '{';

			if (*s == '(')
			{
				set_error(sh, HAWK_ENOIMPL, "command substitution is not implemented");
				goto fail;
			}
			if (brace) s++;
			start = s;
			if (*s == '?' || *s == '$' || *s == '!') s++;
			else while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_')
				s++;

			hawk_ooecs_clear(&name);
			if (hawk_ooecs_ncat(&name, start, s - start) == (hawk_oow_t)-1) goto fail;
			if (brace && *s != '}')
			{
				if (!*s) set_error(sh, HAWK_EEOF, "unfinished parameter expansion");
				else set_error(sh, HAWK_ENOIMPL, "parameter expansion operator is not implemented - %jc", *s);
				goto fail;
			}
			if (brace) s++;
			if (HAWK_OOECS_LEN(&name) == 0)
			{
				if (hawk_ooecs_ccat(&b, '$') == (hawk_oow_t)-1) goto fail;
			}
			else if (HAWK_OOECS_CHAR(&name, 0) == '?' && HAWK_OOECS_LEN(&name) == 1)
			{
				hawk_ooch_t tmp[HAWK_INT_DEC_STR_SIZE(int)];
				hawk_fmt_uintmax_to_oocstr(tmp, HAWK_COUNTOF(tmp), sh->status, 10, -1, 0, HAWK_NULL);
				if (hawk_ooecs_cat(&b, tmp) == (hawk_oow_t)-1) goto fail;
			}
			else
			{
				value = HAWK_NULL;
				if (sh->prm.getenv && sh->prm.getenv(sh, HAWK_OOECS_PTR(&name), &value, sh->prm_ctx) <= -1) goto fail;
				if (value && hawk_ooecs_cat(&b, value) == (hawk_oow_t)-1) goto fail;
			}
			continue;
		}
		else if (ch == '`' && quote != '\'' && variables)
		{
			set_error(sh, HAWK_ENOIMPL, "command substitution is not implemented");
			goto fail;
		}
		if (hawk_ooecs_ccat(&b, ch) == (hawk_oow_t)-1) goto fail;
	}

	result = hawk_ooecs_yieldptr(&b, 0);
	hawk_ooecs_fini(&name);
	hawk_ooecs_fini(&b);
	return result;

fail:
	hawk_ooecs_fini(&name);
	hawk_ooecs_fini(&b);
	return HAWK_NULL;
}

static void free_list (hawk_sh_t* sh, list_t* list)
{
	while (list)
	{
		list_t* next = list->next;
		hawk_sh_cmd_t* c = list->pipeline.commands;

		while (c)
		{
			hawk_sh_cmd_t* cn = c->next;
			hawk_sh_redir_t* r = c->redirs;
			hawk_oow_t i;

			for (i = 0; i < c->argc; i++) hawk_gem_freemem(&sh->gem_, c->argv[i]);
			hawk_gem_freemem(&sh->gem_, c->argv);
			while (r)
			{
				hawk_sh_redir_t* rn = r->next;

				hawk_gem_freemem(&sh->gem_, r->target);
				hawk_gem_freemem(&sh->gem_, r->body);
				hawk_gem_freemem(&sh->gem_, r);
				r = rn;
			}
			hawk_gem_freemem(&sh->gem_, c);
			c = cn;
		}
		hawk_gem_freemem(&sh->gem_, list);
		list = next;
	}
}

static int parse_fd (hawk_sh_t* sh, const hawk_ooch_t* s)
{
	int n = 0;

	if (!*s)
	{
		set_error(sh, HAWK_EINVAL, "empty file descriptor");
		return -1;
	}
	for (; *s; s++)
	{
		if (*s < '0' || *s > '9' || n > 25)
		{
			set_error(sh, HAWK_EINVAL, "file descriptor must be between 0 and 255");
			return -1;
		}
		n = n * 10 + *s - '0';
	}
	if (n > 255)
	{
		set_error(sh, HAWK_EINVAL, "file descriptor must be between 0 and 255");
		return -1;
	}
	return n;
}

static int parse_command (parser_t* p, hawk_sh_cmd_t* c)
{
	hawk_sh_redir_t** tail = &c->redirs;
	int present = 0;

	while (p->token == TOK_WORD || p->token == TOK_REDIR || p->token == TOK_IO_NUMBER)
	{
		present = 1;

		if (p->token == TOK_WORD)
		{
			if ((c->argc + 1) >= c->argx)
			{
				hawk_ooch_t** new_argv;
				hawk_oow_t new_argx;
				const hawk_oow_t inc = 16; /* grow by this chunk size */

				if (c->argc >= (HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(*new_argv)) - inc)
				{
					/* assuming the chunk size is never be very large, it must be acceptable
					 * to reject growth if a while chunk can't be added */
					set_error(p->sh, HAWK_EBUFFULL, "too many arguments");
					return -1;
				}

				new_argx = c->argx + inc;

				new_argv = hawk_gem_reallocmem(&p->sh->gem_, c->argv, new_argx * HAWK_SIZEOF(*new_argv));
				if (HAWK_UNLIKELY(!new_argv)) return -1;

				c->argv = new_argv;
				c->argx = new_argx;
			}

			c->argv[c->argc] = hawk_gem_dupoocstr(&p->sh->gem_, HAWK_OOECS_PTR(&p->word), HAWK_NULL);
			if (HAWK_UNLIKELY(!c->argv[c->argc])) return -1;

			c->argv[++c->argc] = HAWK_NULL;
		}
		else
		{
			int fd = -1;
			hawk_sh_redir_t* r;

			if (p->token == TOK_IO_NUMBER)
			{
				fd = parse_fd(p->sh, HAWK_OOECS_PTR(&p->word));
				if (fd < 0 || lex(p) < 0) return -1;
			}

			r = hawk_gem_callocmem(&p->sh->gem_, HAWK_SIZEOF(*r));
			if (HAWK_UNLIKELY(!r)) return -1;

			*tail = r;
			tail = &r->next;
			r->type = p->rtype;
			r->strip_tabs = p->strip;
			r->fd = fd >= 0? fd: (p->input_redir? 0: 1);
			if (lex(p) < 0) return -1;
			if (p->token != TOK_WORD && p->token != TOK_IO_NUMBER)
			{
				set_error(p->sh, p->token == TOK_END? HAWK_EEOF: HAWK_EINVAL, "missing redirection operand");
				return -1;
			}

			if (r->type == HAWK_SH_HEREDOC)
			{
				hawk_oow_t i;
				for (i = 0; i < HAWK_OOECS_LEN(&p->word); i++)
				{
					hawk_ooch_t ch = HAWK_OOECS_CHAR(&p->word, i);
					if (ch == '\\' || ch == '\'' || ch == '"') r->quoted = 1;
				}
				r->target = expand(p->sh, HAWK_OOECS_PTR(&p->word), 0, 0);
			}
			else
			{
				r->target = hawk_gem_dupoocstr(&p->sh->gem_, HAWK_OOECS_PTR(&p->word), HAWK_NULL);
			}
			if (!r->target) return -1;
		}

		if (lex(p) < 0) return -1;
	}

	if (!present)
	{
		set_error(p->sh, p->token == TOK_END? HAWK_EEOF: HAWK_EINVAL, "expected command");
		return -1;
	}
	return 0;
}

static int collect_heredocs (parser_t* p, list_t* list)
{
	for (; list; list = list->next)
	{
		hawk_sh_cmd_t* c;

		for (c = list->pipeline.commands; c; c = c->next)
		{
			hawk_sh_redir_t* r;

			for (r = c->redirs; r; r = r->next)
			{
				hawk_ooecs_t body, line;

				if (r->type != HAWK_SH_HEREDOC || r->body) continue;
				if (hawk_ooecs_init(&body, &p->sh->gem_, SH_DFL_BUFFER_CAPA) <= -1) return -1;
				if (hawk_ooecs_init(&line, &p->sh->gem_, SH_DFL_BUFFER_CAPA) <= -1)
				{
					hawk_ooecs_fini(&body);
					return -1;
				}
				p->sh->reading_heredoc = 1;

				while (1)
				{
					hawk_ooci_t ch;

					hawk_ooecs_clear(&line);
					if (r->strip_tabs) while (peek_char(p) == '\t') read_char(p);
					while ((ch = read_char(p)) >= 0 && ch != '\n')
					{
						if (hawk_ooecs_ccat(&line, ch) == (hawk_oow_t)-1) goto fail;
					}
					if (p->failed) goto fail;
					if ((ch >= 0 || HAWK_OOECS_LEN(&line) > 0) &&
					    hawk_comp_oocstr(HAWK_OOECS_PTR(&line), r->target, 0) == 0) break;
					if (ch == HAWK_OOCI_EOF)
					{
						set_error(p->sh, HAWK_EEOF, "unterminated here-document");
						goto fail;
					}
					if (HAWK_OOECS_LEN(&line) > 0 &&
					    hawk_ooecs_ncat(&body, HAWK_OOECS_PTR(&line), HAWK_OOECS_LEN(&line)) == (hawk_oow_t)-1) goto fail;
					if (hawk_ooecs_ccat(&body, '\n') == (hawk_oow_t)-1) goto fail;
				}

				r->body = hawk_ooecs_yieldptr(&body, 0);
				hawk_ooecs_fini(&body);
				hawk_ooecs_fini(&line);
				p->sh->reading_heredoc = 0;
				continue;

			fail:
				hawk_ooecs_fini(&body);
				hawk_ooecs_fini(&line);
				p->sh->reading_heredoc = 0;
				return -1;
			}
		}
	}
	return 0;
}

static int expand_pipeline (hawk_sh_t* sh, hawk_sh_pipeline_t* pipeline)
{
	hawk_sh_cmd_t* c;

	for (c = pipeline->commands; c; c = c->next)
	{
		hawk_oow_t i;
		hawk_sh_redir_t* r;

		for (i = 0; i < c->argc; i++)
		{
			hawk_ooch_t* s = expand(sh, c->argv[i], 1, 0);

			if (!s) return -1;
			hawk_gem_freemem(&sh->gem_, c->argv[i]);
			c->argv[i] = s;
		}
		for (r = c->redirs; r; r = r->next)
		{
			hawk_ooch_t** field = r->type == HAWK_SH_HEREDOC? &r->body: &r->target;
			hawk_ooch_t* s;

			if (r->type == HAWK_SH_HEREDOC && r->quoted) continue;
			s = expand(sh, *field, 1, r->type == HAWK_SH_HEREDOC);
			if (!s) return -1;
			hawk_gem_freemem(&sh->gem_, *field);
			*field = s;
		}
	}
	return 0;
}

int hawk_sh_exec (hawk_sh_t* sh, hawk_sh_input_t input, void* ctx)
{
	parser_t p;
	list_t* list = HAWK_NULL;
	int result = -1;

	if (sh->running)
	{
		set_error(sh, HAWK_ESTATE, "recursive shell execution");
		return -1;
	}
	if (!input || !sh->prm.exec)
	{
		set_error(sh, HAWK_EINVAL, "missing shell input or execution callback");
		return -1;
	}

	HAWK_MEMSET(&p, 0, HAWK_SIZEOF(p));
	p.sh = sh;
	p.input = input;
	p.ctx = ctx;

	hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_ENOERR);

	if (hawk_ooecs_init(&p.word, &sh->gem_, SH_DFL_BUFFER_CAPA) <= -1) return -1;

	sh->running = 1;
	sh->exiting = 0;
	sh->line = 1;

	if (lex(&p) < 0) goto done;

	while (p.token != TOK_END && !sh->exiting)
	{
		list_t** tail = &list;
		list_t* item;
		int condition = 0;

		if (p.token == TOK_NL)
		{
			if (lex(&p) < 0) goto done;
			continue;
		}

		while (1)
		{
			hawk_sh_cmd_t** ct;

			item = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(*item));
			if (HAWK_UNLIKELY(!item)) goto done;

			*tail = item;
			tail = &item->next;
			item->condition = condition;
			ct = &item->pipeline.commands;
			while (1)
			{
				*ct = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(**ct));
				if (HAWK_UNLIKELY(!*ct)) goto done;
				if (parse_command(&p, *ct) < 0) goto done;

				ct = &(*ct)->next;
				if (p.token != TOK_PIPE) break;

				if (lex(&p) < 0) goto done;

				while (p.token == TOK_NL)
				{
					if (collect_heredocs(&p, list) < 0 || lex(&p) < 0) goto done;
				}
			}
			if (p.token == TOK_END || p.token == TOK_NL) break;
			if (p.token != TOK_SEMI && p.token != TOK_BG && p.token != TOK_AND && p.token != TOK_OR)
			{
				set_error(sh, HAWK_EINVAL, "unexpected token");
				goto done;
			}

			item->pipeline.background = (p.token == TOK_BG);
			condition = (p.token == TOK_AND || p.token == TOK_OR? p.token: 0);
			if (lex(&p) < 0) goto done;
			if (!condition && (p.token == TOK_END || p.token == TOK_NL)) break;
			while (condition && p.token == TOK_NL)
			{
				if (collect_heredocs(&p, list) < 0 || lex(&p) < 0) goto done;
			}
		}

		if (collect_heredocs(&p, list) < 0) goto done;
		for (item = list; item && !sh->exiting; item = item->next)
		{
			int rc;

			if ((item->condition == TOK_AND && sh->status != 0) || (item->condition == TOK_OR && sh->status == 0)) continue;
			if (expand_pipeline(sh, &item->pipeline) <= -1) goto done;

			rc = sh->prm.exec(sh, &item->pipeline, sh->prm_ctx);
			if (rc <= -1) goto done;
			sh->status = rc & 255;
		}
		free_list(sh, list);
		list = HAWK_NULL;

		if (!sh->exiting && p.token != TOK_END && lex(&p) <= -1) goto done;
	}
	result = sh->status;

done:
	free_list(sh, list);
	hawk_ooecs_fini(&p.word);
	sh->running = 0;
	return result;
}

static int string_input (hawk_sh_t* sh, hawk_ooch_t* ch, void* ctx)
{
	string_input_t* s = ctx;

	if (!*s->ptr) return 0;
	*ch = *s->ptr++;
	return 1;
}

int hawk_sh_execbcs (hawk_sh_t* sh, const hawk_bch_t* s)
{
	string_input_t input;
	int rc;

#if defined(HAWK_OOCH_IS_UCH)
	hawk_ooch_t* converted = hawk_gem_dupbtoucstr(&sh->gem_, s, HAWK_NULL, 0);

	if (!converted) return -1;
	input.ptr = converted;
	rc = hawk_sh_exec(sh, string_input, &input);
	hawk_gem_freemem(&sh->gem_, converted);
#else
	input.ptr = s;
	rc = hawk_sh_exec(sh, string_input, &input);
#endif
	return rc;
}

int hawk_sh_execucs (hawk_sh_t* sh, const hawk_uch_t* s)
{
	string_input_t input;
	int rc;

#if defined(HAWK_OOCH_IS_BCH)
	hawk_ooch_t* converted = hawk_gem_duputobcstr(&sh->gem_, s, HAWK_NULL);

	if (!converted) return -1;
	input.ptr = converted;
	rc = hawk_sh_exec(sh, string_input, &input);
	hawk_gem_freemem(&sh->gem_, converted);
#else
	input.ptr = s;
	rc = hawk_sh_exec(sh, string_input, &input);
#endif
	return rc;
}

hawk_sh_t* hawk_sh_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	hawk_sh_t* sh;

	if (!mmgr) mmgr = hawk_get_sys_mmgr();
	if (!cmgr) cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	HAWK_ASSERT(xtnsize <= HAWK_TYPE_MAX(hawk_oow_t) - HAWK_SIZEOF(*sh));
	sh = HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(*sh) + xtnsize);
	if (HAWK_UNLIKELY(!sh))
	{
		if (errinf)
		{
			HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
			errinf->num = HAWK_ENOMEM;
			hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(HAWK_ENOMEM));
		}
		return HAWK_NULL;
	}

	HAWK_MEMSET(sh, 0, HAWK_SIZEOF(*sh) + xtnsize);
	sh->instsize_ = HAWK_SIZEOF(*sh);
	sh->gem_.mmgr = mmgr;
	sh->gem_.cmgr = cmgr;
	sh->gem_.errstr = hawk_dfl_errstr;

	return sh;
}

void hawk_sh_close (hawk_sh_t* sh)
{
	if (sh->std_close) sh->std_close(sh);

	while (sh->creg)
		hawk_sh_delcmd(sh, sh->creg->name);

	HAWK_MMGR_FREE(sh->gem_.mmgr, sh);
}

hawk_gem_t* hawk_sh_getgem (hawk_sh_t* sh)
{
	return &sh->gem_;
}

void hawk_sh_setprm (hawk_sh_t* sh, const hawk_sh_prm_t* prm, void* ctx)
{
	sh->prm = *prm;
	sh->prm_ctx = ctx;
}

void hawk_sh_setmode (hawk_sh_t* sh, hawk_sh_mode_t mode)
{
	sh->mode = mode;
}

hawk_sh_mode_t hawk_sh_getmode (hawk_sh_t* sh)
{
	return sh->mode;
}

hawk_ooi_t hawk_sh_read (hawk_sh_t* sh, int fd, void* ptr, hawk_oow_t len)
{
	return sh->prm.read? sh->prm.read(sh, fd, ptr, len, sh->prm_ctx): -1;
}

hawk_ooi_t hawk_sh_write (hawk_sh_t* sh, int fd, const void* ptr, hawk_oow_t len)
{
	return sh->prm.write? sh->prm.write(sh, fd, ptr, len, sh->prm_ctx): -1;
}

static hawk_sh_creg_t* lookup (hawk_sh_t* sh, const hawk_ooch_t* name)
{
	hawk_sh_creg_t* r;

	for (r = sh->creg; r; r = r->next)
	{
		if (hawk_comp_oocstr(r->name, name, 0) == 0) return r;
	}
	return HAWK_NULL;
}

int hawk_sh_addcmd (hawk_sh_t* sh, const hawk_ooch_t* name, const hawk_ooch_t* path, hawk_sh_cmd_impl_t impl, void* ctx)
{
	hawk_sh_creg_t* r;

	if (!name || !*name || (!!path == !!impl) || (path && !*path))
	{
		set_error(sh, HAWK_EINVAL, "invalid command registration");
		return -1;
	}

	r = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(*r));
	if (!r) return -1;

	r->name = hawk_gem_dupoocstr(&sh->gem_, name, HAWK_NULL);
	if (path) r->path = hawk_gem_dupoocstr(&sh->gem_, path, HAWK_NULL);
	if (!r->name || (path && !r->path))
	{
		hawk_gem_freemem(&sh->gem_, r->name);
		hawk_gem_freemem(&sh->gem_, r->path);
		hawk_gem_freemem(&sh->gem_, r);
		return -1;
	}
	hawk_sh_delcmd(sh, name);
	r->impl = impl;
	r->ctx = ctx;
	r->next = sh->creg;
	sh->creg = r;
	return 0;
}

int hawk_sh_delcmd (hawk_sh_t* sh, const hawk_ooch_t* name)
{
	hawk_sh_creg_t** p;

	for (p = &sh->creg; *p; p = &(*p)->next)
	{
		hawk_sh_creg_t* r = *p;

		if (hawk_comp_oocstr(r->name, name, 0) != 0) continue;
		*p = r->next;
		hawk_gem_freemem(&sh->gem_, r->name);
		hawk_gem_freemem(&sh->gem_, r->path);
		hawk_gem_freemem(&sh->gem_, r);
		return 1;
	}
	return 0;
}

const hawk_ooch_t* hawk_sh_findcmd (hawk_sh_t* sh, const hawk_ooch_t* name)
{
	hawk_sh_creg_t* r = lookup(sh, name);

	return r? r->path: HAWK_NULL;
}

static int same (const hawk_ooch_t* a, const hawk_ooch_t* b)
{
	return hawk_comp_oocstr(a, b, 0) == 0;
}

int hawk_sh_isbuiltin (hawk_sh_t* sh, const hawk_ooch_t* name)
{
	hawk_sh_creg_t* r = lookup(sh, name);

	return same(name, HAWK_T("mode")) || same(name, HAWK_T("addcmd")) || same(name, HAWK_T("delcmd")) ||
	       same(name, HAWK_T("exit")) || same(name, HAWK_T(":")) || same(name, HAWK_T("true")) || same(name, HAWK_T("false")) || (r && r->impl);
}

int hawk_sh_call (hawk_sh_t* sh, const hawk_sh_cmd_t* c, int* status)
{
	const hawk_ooch_t* name;
	hawk_sh_creg_t* r;
	int rc = 0;

	if (!c->argc)
	{
		*status = 0;
		return 1;
	}

	name = c->argv[0];
	if (same(name, HAWK_T("mode")))
	{
		if (c->argc == 1)
		{
			const hawk_bch_t* s = sh->mode == HAWK_SH_PATH? "path\n": "registered\n";

			rc = hawk_sh_write(sh, 1, s, hawk_count_bcstr(s)) <= -1? 1: 0;
		}
		else if (c->argc == 2 && (same(c->argv[1], HAWK_T("path")) || same(c->argv[1], HAWK_T("registered"))))
		{
			sh->mode = same(c->argv[1], HAWK_T("path"))? HAWK_SH_PATH: HAWK_SH_REGISTERED;
		}
		else
		{
			const hawk_bch_t* msg = "usage: mode [path|registered]\n";
			hawk_sh_write(sh, 2, msg, hawk_count_bcstr(msg));
			rc = 2;
		}
	}
	else if (same(name, HAWK_T("addcmd")))
	{
		if (c->argc != 3 || c->argv[2][0] != '/')
		{
			const hawk_bch_t* msg = "usage: addcmd name /absolute/path\n";
			hawk_sh_write(sh, 2, msg, hawk_count_bcstr(msg));
			rc = 2;
		}
		else if (hawk_sh_addcmd(sh, c->argv[1], c->argv[2], HAWK_NULL, HAWK_NULL) <= -1) return -1;
	}
	else if (same(name, HAWK_T("delcmd")))
	{
		if (c->argc != 2) rc = 2;
		else hawk_sh_delcmd(sh, c->argv[1]);
	}
	else if (same(name, HAWK_T("exit")))
	{
		rc = sh->status;
		if (c->argc > 2) rc = 2;
		else
		{
			if (c->argc == 2)
			{
				const hawk_ooch_t* s = c->argv[1];
				int negative = *s == '-';

				if (*s == '+' || *s == '-') s++;
				rc = 0;
				if (!*s) rc = 2;
				while (*s)
				{
					if (*s < '0' || *s > '9')
					{
						rc = 2;
						break;
					}
					rc = (rc * 10 + *s++ - '0') & 255;
				}
				if (negative) rc = (-rc) & 255;
			}
			sh->exiting = 1;
		}
	}
	else if (same(name, HAWK_T("false")))
	{
		rc = 1;
	}
	else if (same(name, HAWK_T(":")) || same(name, HAWK_T("true")))
	{
		rc = 0;
	}
	else
	{
		r = lookup(sh, name);
		if (!r || !r->impl) return 0;
		rc = r->impl(sh, c->argc, (const hawk_ooch_t* const*)c->argv, r->ctx);
		if (rc <= -1) return -1;
	}

	*status = rc & 255;
	return 1;
}
