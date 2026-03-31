#include <hawk-po.h>
#include "hawk-prv.h"

#include <stdio.h>

#define HAWK_POCTX_SEP '\004'

typedef struct hawk_pocat_strbuf_t hawk_pocat_strbuf_t;
typedef struct hawk_pocat_expr_parser_t hawk_pocat_expr_parser_t;
typedef struct hawk_poparser_t hawk_poparser_t;

enum hawk_pofld_t
{
	HAWK_POFLD_NONE,
	HAWK_POFLD_MSGCTXT,
	HAWK_POFLD_MSGID,
	HAWK_POFLD_MSGID_PLURAL,
	HAWK_POFLD_MSGSTR
};
typedef enum hawk_pofld_t hawk_pofld_t;

struct hawk_pocat_strbuf_t
{
	hawk_gem_t* gem;
	char* data;
	hawk_oow_t len;
	hawk_oow_t capa;
};

struct hawk_pocat_expr_parser_t
{
	const char* s;
	unsigned long n;
};

struct hawk_poparser_t
{
	hawk_poent_t cur;
	hawk_pofld_t active_field;
	int active_msgstr_index;
};

static void strbuf_init (hawk_pocat_strbuf_t* sb, hawk_gem_t* gem)
{
	sb->gem = gem;
	sb->data = HAWK_NULL;
	sb->len = 0;
	sb->capa = 0;
}

static int strbuf_reserve (hawk_pocat_strbuf_t* sb, hawk_oow_t extra)
{
	hawk_oow_t req;
	hawk_oow_t capa;
	char* tmp;

	req = sb->len + extra + 1;
	if (req <= sb->capa) return 0;

	capa = (sb->capa > 0)? sb->capa: 32;
	while (capa < req) capa *= 2;

	tmp = (char*)hawk_gem_reallocmem(sb->gem, sb->data, capa);
	if (!tmp) return -1;

	sb->data = tmp;
	sb->capa = capa;
	return 0;
}

static int strbuf_append_char (hawk_pocat_strbuf_t* sb, char c)
{
	if (strbuf_reserve(sb, 1) <= -1) return -1;
	sb->data[sb->len++] = c;
	sb->data[sb->len] = '\0';
	return 0;
}

static char* strbuf_yield (hawk_pocat_strbuf_t* sb)
{
	char* ptr;

	if (!sb->data) return hawk_gem_dupbcstr(sb->gem, "", HAWK_NULL);

	ptr = sb->data;
	sb->data = HAWK_NULL;
	sb->len = 0;
	sb->capa = 0;
	return ptr;
}

static void strbuf_fini (hawk_pocat_strbuf_t* sb)
{
	if (sb->data) hawk_gem_freemem(sb->gem, sb->data);
	sb->data = HAWK_NULL;
	sb->len = 0;
	sb->capa = 0;
}

static int read_line (hawk_gem_t* gem, FILE* fp, char** line)
{
	hawk_pocat_strbuf_t sb;
	int ch;
	int got = 0;

	strbuf_init(&sb, gem);

	while ((ch = fgetc(fp)) != EOF)
	{
		got = 1;
		if (strbuf_append_char(&sb, (char)ch) <= -1)
		{
			strbuf_fini(&sb);
			return -1;
		}
		if (ch == '\n') break;
	}

	if (!got)
	{
		strbuf_fini(&sb);
		return 0;
	}

	*line = strbuf_yield(&sb);
	if (!*line)
	{
		strbuf_fini(&sb);
		return -1;
	}

	return 1;
}

static const char* skip_ws (const char* s)
{
	while (*s && hawk_is_bch_space((unsigned char)*s)) s++;
	return s;
}

static int is_blank_line (const char* s)
{
	s = skip_ws(s);
	return (*s == '\0');
}

static void trim_eol (char* s)
{
	hawk_oow_t n = hawk_count_bcstr(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static int starts_with_kw (const char* s, const char* kw)
{
	hawk_oow_t n = hawk_count_bcstr(kw);
	if (hawk_comp_bchars_bcstr(s, n, kw, 0) != 0) return 0;
	return (s[n] == '\0' || hawk_is_bch_space((unsigned char)s[n]));
}

static void hawk_poent_init (hawk_poent_t* poent)
{
	HAWK_MEMSET(poent, 0, HAWK_SIZEOF(*poent));
}

static void hawk_poent_fini (hawk_poent_t* poent, hawk_gem_t* gem)
{
	int i;

	if (poent->msgctx) hawk_gem_freemem(gem, poent->msgctx);
	if (poent->msgid) hawk_gem_freemem(gem, poent->msgid);
	if (poent->msgid_plural) hawk_gem_freemem(gem, poent->msgid_plural);

	for (i = 0; i < poent->msgstr_count; i++)
	{
		if (poent->msgstr[i]) hawk_gem_freemem(gem, poent->msgstr[i]);
	}
	if (poent->msgstr) hawk_gem_freemem(gem, poent->msgstr);

	HAWK_MEMSET(poent, 0, HAWK_SIZEOF(*poent));
}

static int hawk_poent_ensure_msgstr (hawk_poent_t* poent, hawk_gem_t* gem, int index)
{
	int old_count;
	char** tmp;

	if (index < 0) return -1;
	if (index < poent->msgstr_count) return 0;

	old_count = poent->msgstr_count;
	tmp = (char**)hawk_gem_reallocmem(gem, poent->msgstr, HAWK_SIZEOF(*tmp) * (index + 1));
	if (!tmp) return -1;

	poent->msgstr = tmp;
	poent->msgstr_count = index + 1;
	while (old_count < poent->msgstr_count) poent->msgstr[old_count++] = HAWK_NULL;
	return 0;
}

static char** hawk_poent_target_msgstr (hawk_poent_t* poent, hawk_gem_t* gem, int index)
{
	if (hawk_poent_ensure_msgstr(poent, gem, index) <= -1) return HAWK_NULL;
	return &poent->msgstr[index];
}

static const char* find_header_line_value (const char* header, const char* key)
{
	hawk_oow_t key_len = hawk_count_bcstr(key);
	const char* p = header;

	while (*p)
	{
		const char* line = p;
		const char* nl = hawk_find_bchar_in_bcstr(p, '\n');
		hawk_oow_t len = nl? (hawk_oow_t)(nl - line): hawk_count_bcstr(line);

		if (len >= key_len && hawk_comp_bchars_bcstr(line, key_len, key, 0) == 0) return line + key_len;

		p = nl? (nl + 1): (line + len);
		if (!nl) break;
	}

	return HAWK_NULL;
}

static char* dup_header_value_line (hawk_gem_t* gem, const char* header, const char* key)
{
	const char* v;
	const char* end;

	v = find_header_line_value(header, key);
	if (!v) return HAWK_NULL;

	while (*v == ' ' || *v == '\t') v++;
	end = hawk_find_bchar_in_bcstr(v, '\n');
	if (!end) end = v + hawk_count_bcstr(v);

	return hawk_gem_dupbchars(gem, v, end - v);
}

static int parse_nplurals (const char* s)
{
	const char* p;
	int n = 2;

	p = hawk_find_bchars_in_bcstr(s, "nplurals=", 9, 0);
	if (!p) return 2;

	p += 9;
	while (*p && hawk_is_bch_space((unsigned char)*p)) p++;

	if (hawk_is_bch_digit((unsigned char)*p))
	{
		n = 0;
		while (hawk_is_bch_digit((unsigned char)*p))
		{
			n = n * 10 + (*p - '0');
			p++;
		}
	}

	return (n > 0)? n: 2;
}

static char* parse_plural_expr (hawk_gem_t* gem, const char* s)
{
	const char* p;
	const char* end;

	p = hawk_find_bchars_in_bcstr(s, "plural=", 7, 0);
	if (!p) return hawk_gem_dupbcstr(gem, "(n != 1)", HAWK_NULL);

	p += 7;
	while (*p && hawk_is_bch_space((unsigned char)*p)) p++;
	end = hawk_find_bchar_in_bcstr(p, ';');
	if (!end) end = p + hawk_count_bcstr(p);

	return hawk_gem_dupbchars(gem, p, end - p);
}

static int parse_header (hawk_pocat_t* pocat, const hawk_poent_t* poent)
{
	char* content_type;
	char* charset = HAWK_NULL;
	char* plural_forms_raw = HAWK_NULL;
	char* plural_expr = HAWK_NULL;

	if (!poent->msgid || poent->msgid[0] != '\0') return 0;
	if (poent->msgstr_count < 1 || !poent->msgstr || !poent->msgstr[0]) return 0;

	content_type = dup_header_value_line(pocat->gem, poent->msgstr[0], "Content-Type:");
	if (content_type)
	{
		const char* p = hawk_find_bchars_in_bcstr(content_type, "charset=", 8, 0);
		if (p)
		{
			p += 8;
			charset = hawk_gem_dupbcstr(pocat->gem, p, HAWK_NULL);
			if (!charset)
			{
				hawk_gem_freemem(pocat->gem, content_type);
				return -1;
			}
		}
		hawk_gem_freemem(pocat->gem, content_type);
	}

	plural_forms_raw = dup_header_value_line(pocat->gem, poent->msgstr[0], "Plural-Forms:");
	if (plural_forms_raw)
	{
		plural_expr = parse_plural_expr(pocat->gem, plural_forms_raw);
		if (!plural_expr)
		{
			if (charset) hawk_gem_freemem(pocat->gem, charset);
			hawk_gem_freemem(pocat->gem, plural_forms_raw);
			return -1;
		}
	}
	else
	{
		plural_expr = hawk_gem_dupbcstr(pocat->gem, "(n != 1)", HAWK_NULL);
		if (!plural_expr)
		{
			if (charset) hawk_gem_freemem(pocat->gem, charset);
			return -1;
		}
	}

	if (pocat->charset) hawk_gem_freemem(pocat->gem, pocat->charset);
	if (pocat->plural_forms_raw) hawk_gem_freemem(pocat->gem, pocat->plural_forms_raw);
	if (pocat->plural_expr) hawk_gem_freemem(pocat->gem, pocat->plural_expr);

	pocat->charset = charset;
	pocat->plural_forms_raw = plural_forms_raw;
	pocat->plural_expr = plural_expr;
	pocat->nplurals = plural_forms_raw? parse_nplurals(plural_forms_raw): 2;

	return 0;
}

static int add_entry (hawk_pocat_t* pocat, const hawk_poent_t* src)
{
	hawk_poent_t* dst;
	hawk_poent_t tmp;
	hawk_poent_t* entries;
	int i;

	if (pocat->count >= pocat->capa)
	{
		hawk_oow_t new_capa = (pocat->capa > 0)? (pocat->capa * 2): 32;
		entries = (hawk_poent_t*)hawk_gem_reallocmem(pocat->gem, pocat->entries, HAWK_SIZEOF(*entries) * new_capa);
		if (!entries) return -1;
		pocat->entries = entries;
		pocat->capa = new_capa;
	}

	hawk_poent_init(&tmp);

	if (src->msgctx)
	{
		tmp.msgctx = hawk_gem_dupbcstr(pocat->gem, src->msgctx, HAWK_NULL);
		if (!tmp.msgctx) goto oops;
	}

	if (src->msgid)
	{
		tmp.msgid = hawk_gem_dupbcstr(pocat->gem, src->msgid, HAWK_NULL);
		if (!tmp.msgid) goto oops;
	}

	if (src->msgid_plural)
	{
		tmp.msgid_plural = hawk_gem_dupbcstr(pocat->gem, src->msgid_plural, HAWK_NULL);
		if (!tmp.msgid_plural) goto oops;
	}

	tmp.fuzzy = src->fuzzy;
	tmp.obsolete = src->obsolete;
	tmp.msgstr_count = src->msgstr_count;

	if (src->msgstr_count > 0)
	{
		tmp.msgstr = (char**)hawk_gem_callocmem(pocat->gem, HAWK_SIZEOF(*tmp.msgstr) * src->msgstr_count);
		if (!tmp.msgstr) goto oops;

		for (i = 0; i < src->msgstr_count; i++)
		{
			if (src->msgstr[i])
			{
				tmp.msgstr[i] = hawk_gem_dupbcstr(pocat->gem, src->msgstr[i], HAWK_NULL);
				if (!tmp.msgstr[i]) goto oops;
			}
		}
	}

	dst = &pocat->entries[pocat->count++];
	*dst = tmp;
	return 0;

oops:
	hawk_poent_fini(&tmp, pocat->gem);
	return -1;
}

static int parse_quoted (hawk_gem_t* gem, const char* s, char** out)
{
	hawk_pocat_strbuf_t sb;

	*out = HAWK_NULL;
	s = skip_ws(s);
	if (*s != '"') return 0;
	s++;

	strbuf_init(&sb, gem);

	while (*s && *s != '"')
	{
		unsigned char c = (unsigned char)*s++;
		if (c == '\\')
		{
			unsigned char e = (unsigned char)*s++;
			switch (e)
			{
				case 'a': if (strbuf_append_char(&sb, '\a') <= -1) goto oops; break;
				case 'b': if (strbuf_append_char(&sb, '\b') <= -1) goto oops; break;
				case 'f': if (strbuf_append_char(&sb, '\f') <= -1) goto oops; break;
				case 'n': if (strbuf_append_char(&sb, '\n') <= -1) goto oops; break;
				case 'r': if (strbuf_append_char(&sb, '\r') <= -1) goto oops; break;
				case 't': if (strbuf_append_char(&sb, '\t') <= -1) goto oops; break;
				case 'v': if (strbuf_append_char(&sb, '\v') <= -1) goto oops; break;
				case '\\': if (strbuf_append_char(&sb, '\\') <= -1) goto oops; break;
				case '"': if (strbuf_append_char(&sb, '"') <= -1) goto oops; break;
				case '\'': if (strbuf_append_char(&sb, '\'') <= -1) goto oops; break;
				case '?': if (strbuf_append_char(&sb, '?') <= -1) goto oops; break;

				case 'x':
				{
					int val = 0;
					int digits = 0;
					while (hawk_is_bch_xdigit((unsigned char)*s))
					{
						int d;
						unsigned char h = (unsigned char)*s++;
						if (hawk_is_bch_digit(h)) d = h - '0';
						else if (h >= 'a' && h <= 'f') d = 10 + (h - 'a');
						else d = 10 + (h - 'A');
						val = (val << 4) | d;
						digits++;
					}
					if (digits <= 0) goto syntax_error;
					if (strbuf_append_char(&sb, (char)val) <= -1) goto oops;
					break;
				}

				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				{
					int val = e - '0';
					int count = 1;
					while (count < 3 && *s >= '0' && *s <= '7')
					{
						val = (val * 8) + (*s - '0');
						s++;
						count++;
					}
					if (strbuf_append_char(&sb, (char)val) <= -1) goto oops;
					break;
				}

				case '\0':
					goto syntax_error;

				default:
					if (strbuf_append_char(&sb, (char)e) <= -1) goto oops;
					break;
			}
		}
		else
		{
			if (strbuf_append_char(&sb, (char)c) <= -1) goto oops;
		}
	}

	if (*s != '"') goto syntax_error;
	s++;
	s = skip_ws(s);
	if (*s != '\0') goto syntax_error;

	*out = strbuf_yield(&sb);
	if (!*out) goto oops;
	return 1;

syntax_error:
	strbuf_fini(&sb);
	return 0;

oops:
	strbuf_fini(&sb);
	return -1;
}

static void expr_skip_ws (hawk_pocat_expr_parser_t* p)
{
	while (*p->s && hawk_is_bch_space((unsigned char)*p->s)) p->s++;
}

static long expr_parse_ternary (hawk_pocat_expr_parser_t* p);

static long expr_parse_primary (hawk_pocat_expr_parser_t* p)
{
	long v = 0;

	expr_skip_ws(p);

	if (*p->s == '(')
	{
		p->s++;
		v = expr_parse_ternary(p);
		expr_skip_ws(p);
		if (*p->s == ')') p->s++;
		return v;
	}

	if (*p->s == 'n')
	{
		p->s++;
		return (long)p->n;
	}

	if (*p->s == '-' || hawk_is_bch_digit((unsigned char)*p->s))
	{
		const char* end;

		v = hawk_bchars_to_int(p->s, hawk_count_bcstr(p->s), HAWK_OOCHARS_TO_INT_MAKE_OPTION(0, 0, 10), &end, HAWK_NULL);
		if (end != p->s)
		{
			p->s = end;
			return v;
		}
	}

	return 0;
}

static long expr_parse_unary (hawk_pocat_expr_parser_t* p)
{
	expr_skip_ws(p);
	if (*p->s == '!')
	{
		p->s++;
		return !expr_parse_unary(p);
	}
	if (*p->s == '+')
	{
		p->s++;
		return +expr_parse_unary(p);
	}
	if (*p->s == '-')
	{
		p->s++;
		return -expr_parse_unary(p);
	}
	return expr_parse_primary(p);
}

static long expr_parse_mul (hawk_pocat_expr_parser_t* p)
{
	long v = expr_parse_unary(p);

	for (;;)
	{
		long rhs;

		expr_skip_ws(p);
		if (*p->s == '*')
		{
			p->s++;
			rhs = expr_parse_unary(p);
			v = v * rhs;
		}
		else if (*p->s == '/')
		{
			p->s++;
			rhs = expr_parse_unary(p);
			v = (rhs == 0)? 0: (v / rhs);
		}
		else if (*p->s == '%')
		{
			p->s++;
			rhs = expr_parse_unary(p);
			v = (rhs == 0)? 0: (v % rhs);
		}
		else break;
	}

	return v;
}

static long expr_parse_add (hawk_pocat_expr_parser_t* p)
{
	long v = expr_parse_mul(p);

	for (;;)
	{
		long rhs;

		expr_skip_ws(p);
		if (*p->s == '+')
		{
			p->s++;
			rhs = expr_parse_mul(p);
			v = v + rhs;
		}
		else if (*p->s == '-')
		{
			p->s++;
			rhs = expr_parse_mul(p);
			v = v - rhs;
		}
		else break;
	}

	return v;
}

static long expr_parse_rel (hawk_pocat_expr_parser_t* p)
{
	long v = expr_parse_add(p);

	for (;;)
	{
		long rhs;

		expr_skip_ws(p);
		if (hawk_comp_bcstr_limited(p->s, "<=", 2, 0) == 0)
		{
			p->s += 2;
			rhs = expr_parse_add(p);
			v = (v <= rhs);
		}
		else if (hawk_comp_bcstr_limited(p->s, ">=", 2, 0) == 0)
		{
			p->s += 2;
			rhs = expr_parse_add(p);
			v = (v >= rhs);
		}
		else if (*p->s == '<')
		{
			p->s++;
			rhs = expr_parse_add(p);
			v = (v < rhs);
		}
		else if (*p->s == '>')
		{
			p->s++;
			rhs = expr_parse_add(p);
			v = (v > rhs);
		}
		else break;
	}

	return v;
}

static long expr_parse_eq (hawk_pocat_expr_parser_t* p)
{
	long v = expr_parse_rel(p);

	for (;;)
	{
		long rhs;

		expr_skip_ws(p);
		if (hawk_comp_bcstr_limited(p->s, "==", 2, 0) == 0)
		{
			p->s += 2;
			rhs = expr_parse_rel(p);
			v = (v == rhs);
		}
		else if (hawk_comp_bcstr_limited(p->s, "!=", 2, 0) == 0)
		{
			p->s += 2;
			rhs = expr_parse_rel(p);
			v = (v != rhs);
		}
		else break;
	}

	return v;
}

static long expr_parse_and (hawk_pocat_expr_parser_t* p)
{
	long v = expr_parse_eq(p);

	for (;;)
	{
		long rhs;

		expr_skip_ws(p);
		if (hawk_comp_bcstr_limited(p->s, "&&", 2, 0) != 0) break;
		p->s += 2;
		rhs = expr_parse_eq(p);
		v = (v && rhs);
	}

	return v;
}

static long expr_parse_or (hawk_pocat_expr_parser_t* p)
{
	long v = expr_parse_and(p);

	for (;;)
	{
		long rhs;

		expr_skip_ws(p);
		if (hawk_comp_bcstr_limited(p->s, "||", 2, 0) != 0) break;
		p->s += 2;
		rhs = expr_parse_and(p);
		v = (v || rhs);
	}

	return v;
}

static long expr_parse_ternary (hawk_pocat_expr_parser_t* p)
{
	long cond = expr_parse_or(p);

	expr_skip_ws(p);
	if (*p->s == '?')
	{
		long a;
		long b;

		p->s++;
		a = expr_parse_ternary(p);
		expr_skip_ws(p);
		if (*p->s == ':') p->s++;
		b = expr_parse_ternary(p);
		return cond? a: b;
	}

	return cond;
}

static int eval_plural_index (const hawk_pocat_t* pocat, unsigned long n)
{
	hawk_pocat_expr_parser_t p;
	long index;

	if (!pocat->plural_expr || !pocat->plural_expr[0]) return (n != 1)? 1: 0;

	p.s = pocat->plural_expr;
	p.n = n;
	index = expr_parse_ternary(&p);

	if (index < 0) index = 0;
	if (pocat->nplurals > 0 && index >= pocat->nplurals) index = pocat->nplurals - 1;

	return (int)index;
}

static void hawk_poparser_init (hawk_poparser_t* parser)
{
	hawk_poent_init(&parser->cur);
	parser->active_field = HAWK_POFLD_NONE;
	parser->active_msgstr_index = 0;
}

static void hawk_poparser_reset_current (hawk_poparser_t* parser, hawk_gem_t* gem)
{
	hawk_poent_fini(&parser->cur, gem);
	hawk_poent_init(&parser->cur);
	parser->active_field = HAWK_POFLD_NONE;
	parser->active_msgstr_index = 0;
}

static int hawk_poparser_has_current_content (const hawk_poparser_t* parser)
{
	return !!(parser->cur.msgctx || parser->cur.msgid || parser->cur.msgid_plural ||
	          parser->cur.msgstr_count > 0 || parser->cur.fuzzy || parser->cur.obsolete);
}

static int hawk_poparser_append_to_string (hawk_gem_t* gem, char** dst, const char* frag)
{
	hawk_oow_t old_len = (*dst)? hawk_count_bcstr(*dst): 0;
	hawk_oow_t add_len = hawk_count_bcstr(frag);
	char* tmp;

	tmp = (char*)hawk_gem_reallocmem(gem, *dst, old_len + add_len + 1);
	if (!tmp) return -1;

	*dst = tmp;
	HAWK_MEMCPY(&(*dst)[old_len], frag, add_len + 1);
	return 0;
}

static int hawk_poparser_append_fragment (hawk_poparser_t* parser, hawk_gem_t* gem, const char* frag)
{
	switch (parser->active_field)
	{
		case HAWK_POFLD_MSGCTXT:
			if (!parser->cur.msgctx)
			{
				parser->cur.msgctx = hawk_gem_dupbcstr(gem, "", HAWK_NULL);
				if (!parser->cur.msgctx) return -1;
			}
			return hawk_poparser_append_to_string(gem, &parser->cur.msgctx, frag);

		case HAWK_POFLD_MSGID:
			if (!parser->cur.msgid)
			{
				parser->cur.msgid = hawk_gem_dupbcstr(gem, "", HAWK_NULL);
				if (!parser->cur.msgid) return -1;
			}
			return hawk_poparser_append_to_string(gem, &parser->cur.msgid, frag);

		case HAWK_POFLD_MSGID_PLURAL:
			if (!parser->cur.msgid_plural)
			{
				parser->cur.msgid_plural = hawk_gem_dupbcstr(gem, "", HAWK_NULL);
				if (!parser->cur.msgid_plural) return -1;
			}
			return hawk_poparser_append_to_string(gem, &parser->cur.msgid_plural, frag);

		case HAWK_POFLD_MSGSTR:
		{
			char** slot = hawk_poent_target_msgstr(&parser->cur, gem, parser->active_msgstr_index);
			if (!slot) return -1;

			if (!*slot)
			{
				*slot = hawk_gem_dupbcstr(gem, "", HAWK_NULL);
				if (!*slot) return -1;
			}
			return hawk_poparser_append_to_string(gem, slot, frag);
		}

		default:
			return 0;
	}
}

static int hawk_poparser_parse_msgstr_index_line (const char* s, int* index, const char** after)
{
	int n = 0;

	if (hawk_comp_bcstr_limited(s, "msgstr[", 7, 0) != 0) return 0;

	s += 7;
	if (!hawk_is_bch_digit((unsigned char)*s)) return 0;

	while (hawk_is_bch_digit((unsigned char)*s))
	{
		n = n * 10 + (*s - '0');
		s++;
	}

	if (*s != ']') return 0;
	s++;

	*index = n;
	*after = s;
	return 1;
}

static int hawk_poparser_finalize_entry (hawk_pocat_t* pocat, hawk_poparser_t* parser)
{
	if (!hawk_poparser_has_current_content(parser)) return 0;

	if (!parser->cur.msgid)
	{
		parser->cur.msgid = hawk_gem_dupbcstr(pocat->gem, "", HAWK_NULL);
		if (!parser->cur.msgid) return -1;
	}

	if (add_entry(pocat, &parser->cur) <= -1) return -1;
	if (parser->cur.msgid[0] == '\0' && parse_header(pocat, &parser->cur) <= -1) return -1;

	hawk_poparser_reset_current(parser, pocat->gem);
	return 0;
}

hawk_pocat_t* hawk_pocat_open (hawk_gem_t* gem, hawk_oow_t xtnsize)
{
	hawk_pocat_t* pocat;

	pocat = (hawk_pocat_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(*pocat) + xtnsize);
	if (!pocat) return HAWK_NULL;

	if (hawk_pocat_init(pocat, gem) <= -1)
	{
		hawk_gem_freemem(gem, pocat);
		return HAWK_NULL;
	}

	HAWK_MEMSET(pocat + 1, 0, xtnsize);
	return pocat;
}

void hawk_pocat_close (hawk_pocat_t* pocat)
{
	hawk_gem_t* gem;

	gem = pocat->gem;
	hawk_pocat_fini(pocat);
	hawk_gem_freemem(gem, pocat);
}

int hawk_pocat_init (hawk_pocat_t* pocat, hawk_gem_t* gem)
{
	if (!pocat || !gem) return -1;
	HAWK_MEMSET(pocat, 0, HAWK_SIZEOF(*pocat));
	pocat->gem = gem;
	pocat->nplurals = 2;
	return 0;
}

void hawk_pocat_fini (hawk_pocat_t* pocat)
{
	hawk_oow_t i;

	if (!pocat) return;

	for (i = 0; i < pocat->count; i++) hawk_poent_fini(&pocat->entries[i], pocat->gem);
	if (pocat->entries) hawk_gem_freemem(pocat->gem, pocat->entries);
	if (pocat->charset) hawk_gem_freemem(pocat->gem, pocat->charset);
	if (pocat->plural_forms_raw) hawk_gem_freemem(pocat->gem, pocat->plural_forms_raw);
	if (pocat->plural_expr) hawk_gem_freemem(pocat->gem, pocat->plural_expr);

	HAWK_MEMSET(pocat, 0, HAWK_SIZEOF(*pocat));
}

int hawk_pocat_load_file (hawk_pocat_t* pocat, const char* path, char* errbuf, hawk_oow_t errbuf_sz)
{
	FILE* fp;
	hawk_poparser_t parser;
	char* line = HAWK_NULL;
	unsigned long line_no = 0;
	int status = 0;

	if (!pocat || !path)
	{
		if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "invalid arguments");
		return -1;
	}

	fp = fopen(path, "rb");
	if (!fp)
	{
		if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "cannot open %s", path);
		return -1;
	}

	hawk_poparser_init(&parser);

	for (;;)
	{
		const char* s;
		char* frag;
		int x;

		x = read_line(pocat->gem, fp, &line);
		if (x == 0) break;
		if (x <= -1)
		{
			if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "%s: out of memory", path);
			status = -1;
			goto done;
		}

		line_no++;
		trim_eol(line);
		s = skip_ws(line);

		if (is_blank_line(s))
		{
			if (hawk_poparser_finalize_entry(pocat, &parser) <= -1)
			{
				if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "%s: out of memory", path);
				status = -1;
				goto done;
			}

			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

		if (hawk_comp_bcstr_limited(s, "#~", 2, 0) == 0)
		{
			parser.cur.obsolete = 1;
			s += 2;
			s = skip_ws(s);
			if (*s == '\0')
			{
				hawk_gem_freemem(pocat->gem, line);
				line = HAWK_NULL;
				continue;
			}
		}
		else if (*s == '#')
		{
			if (hawk_comp_bcstr_limited(s, "#,", 2, 0) == 0 && hawk_find_bchars_in_bcstr(s + 2, "fuzzy", 5, 0)) parser.cur.fuzzy = 1;
			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgctx"))
		{
			x = parse_quoted(pocat->gem, s + 7, &frag);
			if (x == 0) goto syntax_error;
			if (x <= -1) goto nomem_error;
			if (parser.cur.msgctx) hawk_gem_freemem(pocat->gem, parser.cur.msgctx);
			parser.cur.msgctx = frag;
			parser.active_field = HAWK_POFLD_MSGCTXT;
			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgid_plural"))
		{
			x = parse_quoted(pocat->gem, s + 12, &frag);
			if (x == 0) goto syntax_error;
			if (x <= -1) goto nomem_error;
			if (parser.cur.msgid_plural) hawk_gem_freemem(pocat->gem, parser.cur.msgid_plural);
			parser.cur.msgid_plural = frag;
			parser.active_field = HAWK_POFLD_MSGID_PLURAL;
			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgid"))
		{
			if (parser.cur.msgid && (parser.cur.msgstr_count > 0 || parser.cur.msgctx || parser.cur.msgid_plural))
			{
				if (hawk_poparser_finalize_entry(pocat, &parser) <= -1) goto nomem_error;
			}

			x = parse_quoted(pocat->gem, s + 5, &frag);
			if (x == 0) goto syntax_error;
			if (x <= -1) goto nomem_error;
			if (parser.cur.msgid) hawk_gem_freemem(pocat->gem, parser.cur.msgid);
			parser.cur.msgid = frag;
			parser.active_field = HAWK_POFLD_MSGID;
			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgstr"))
		{
			int index = 0;
			const char* after = HAWK_NULL;

			if (hawk_poparser_parse_msgstr_index_line(s, &index, &after)) x = parse_quoted(pocat->gem, after, &frag);
			else x = parse_quoted(pocat->gem, s + 6, &frag);

			if (x == 0) goto syntax_error;
			if (x <= -1) goto nomem_error;

			if (hawk_poent_ensure_msgstr(&parser.cur, pocat->gem, index) <= -1)
			{
				hawk_gem_freemem(pocat->gem, frag);
				goto nomem_error;
			}

			if (parser.cur.msgstr[index]) hawk_gem_freemem(pocat->gem, parser.cur.msgstr[index]);
			parser.cur.msgstr[index] = frag;
			parser.active_field = HAWK_POFLD_MSGSTR;
			parser.active_msgstr_index = index;
			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

		if (*s == '"')
		{
			x = parse_quoted(pocat->gem, s, &frag);
			if (x == 0) goto syntax_error;
			if (x <= -1) goto nomem_error;

			if (hawk_poparser_append_fragment(&parser, pocat->gem, frag) <= -1)
			{
				hawk_gem_freemem(pocat->gem, frag);
				goto nomem_error;
			}

			hawk_gem_freemem(pocat->gem, frag);
			hawk_gem_freemem(pocat->gem, line);
			line = HAWK_NULL;
			continue;
		}

syntax_error:
		if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "%s:%lu: syntax error", path, line_no);
		status = -1;
		goto done;

nomem_error:
		if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "%s: out of memory", path);
		status = -1;
		goto done;
	}

	if (hawk_poparser_finalize_entry(pocat, &parser) <= -1)
	{
		if (errbuf && errbuf_sz > 0) snprintf(errbuf, errbuf_sz, "%s: out of memory", path);
		status = -1;
		goto done;
	}

	status = 0;

done:
	if (line) hawk_gem_freemem(pocat->gem, line);
	hawk_poparser_reset_current(&parser, pocat->gem);
	fclose(fp);
	return status;
}

static int hawk_poent_matches (const hawk_poent_t* poent, const char* msgctx, const char* msgid)
{
	if (!poent->msgid || !msgid) return 0;
	if (poent->obsolete) return 0;
	if (hawk_comp_bcstr(poent->msgid, msgid, 0) != 0) return 0;

	if (msgctx == HAWK_NULL && poent->msgctx == HAWK_NULL) return 1;
	if (msgctx != HAWK_NULL && poent->msgctx != HAWK_NULL && hawk_comp_bcstr(poent->msgctx, msgctx, 0) == 0) return 1;
	return 0;
}

static const hawk_poent_t* find (const hawk_pocat_t* pocat, const char* msgctx, const char* msgid)
{
	hawk_oow_t i;

	for (i = 0; i < pocat->count; i++)
	{
		if (hawk_poent_matches(&pocat->entries[i], msgctx, msgid)) return &pocat->entries[i];
	}

	return HAWK_NULL;
}

const char* hawk_pocat_get (const hawk_pocat_t* pocat, const char* msgctx, const char* msgid)
{
	const hawk_poent_t* poent;

	if (!pocat || !msgid) return HAWK_NULL;

	poent = find(pocat, msgctx, msgid);
	if (!poent) return HAWK_NULL;
	if (poent->fuzzy) return HAWK_NULL;
	if (poent->msgstr_count < 1 || !poent->msgstr || !poent->msgstr[0] || !poent->msgstr[0][0]) return HAWK_NULL;

	return poent->msgstr[0];
}

const char* hawk_pocat_gettext (const hawk_pocat_t* pocat, const char* msgctx, const char* msgid)
{
	const char* text;

	text = hawk_pocat_get(pocat, msgctx, msgid);
	return text? text: msgid;
}

const char* hawk_pocat_nget (const hawk_pocat_t* pocat, const char* msgctx, const char* msgid, const char* msgid_plural, unsigned long n)
{
	const hawk_poent_t* poent;
	int index;

	if (!pocat || !msgid || !msgid_plural) return (n == 1)? msgid: msgid_plural;

	poent = find(pocat, msgctx, msgid);
	if (!poent || poent->fuzzy) return (n == 1)? msgid: msgid_plural;

	index = eval_plural_index(pocat, n);
	if (index >= 0 && index < poent->msgstr_count && poent->msgstr[index] && poent->msgstr[index][0]) return poent->msgstr[index];

	return (n == 1)? msgid: msgid_plural;
}
