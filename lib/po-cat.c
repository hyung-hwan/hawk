#include <hawk-po.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PO_CTX_SEP '\004'

typedef struct {
	char* data;
	hawk_oow_t len;
	hawk_oow_t cap;
} StrBuf;

typedef enum {
	PF_NONE,
	PF_MSGCTXT,
	PF_MSGID,
	PF_MSGID_PLURAL,
	PF_MSGSTR
} PoField;

typedef struct {
	PoEntry cur;
	PoField active_field;
	int active_msgstr_index;
} PoParser;

typedef struct {
	const char* s;
	unsigned long n;
} ExprParser;

static void die_oom(void) {
	fprintf(stderr, "out of memory\n");
	exit(1);
}

static void *xmalloc(hawk_oow_t n)
{
	void *p = malloc(n ? n : 1);
	if (!p) die_oom();
	return p;
}

static void *xcalloc(hawk_oow_t nmemb, hawk_oow_t size)
{
	void *p = calloc(nmemb ? nmemb : 1, size ? size : 1);
	if (!p) die_oom();
	return p;
}

static void *xrealloc(void *ptr, hawk_oow_t n)
{
	void *p = realloc(ptr, n ? n : 1);
	if (!p) die_oom();
	return p;
}

static char* xstrdup0(const char* s)
{
	hawk_oow_t n = strlen(s) + 1;
	char* out = (char* )xmalloc(n);
	memcpy(out, s, n);
	return out;
}

static char* xstrndup0(const char* s, hawk_oow_t n)
{
	char* out = (char* )xmalloc(n + 1);
	memcpy(out, s, n);
	out[n] = '\0';
	return out;
}

static const char* skip_ws(const char* s)
{
	while (*s && isspace((unsigned char)*s)) s++;
	return s;
}

static int is_blank_line(const char* s)
{
	s = skip_ws(s);
	return *s == '\0';
}

static void trim_eol(char* s)
{
	hawk_oow_t n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static int starts_with_kw(const char* s, const char* kw)
{
	hawk_oow_t n = strlen(kw);
	if (strncmp(s, kw, n) != 0) return 0;
	if (s[n] == '\0') return 1;
	return isspace((unsigned char)s[n]);
}

static void sb_init(StrBuf *sb) {
	sb->data = HAWK_NULL;
	sb->len = 0;
	sb->cap = 0;
}

static void sb_reserve(StrBuf *sb, hawk_oow_t extra) {
	hawk_oow_t need = sb->len + extra + 1;
	if (need <= sb->cap) return;
	hawk_oow_t cap = sb->cap ? sb->cap : 32;
	while (cap < need) cap *= 2;
	sb->data = (char* )xrealloc(sb->data, cap);
	sb->cap = cap;
}

static void sb_append_char(StrBuf *sb, char c) {
	sb_reserve(sb, 1);
	sb->data[sb->len++] = c;
	sb->data[sb->len] = '\0';
}

static char* sb_take(StrBuf *sb) {
	char* out;
	if (!sb->data) return xstrdup0("");
	out = sb->data;
	sb->data = HAWK_NULL;
	sb->len = 0;
	sb->cap = 0;
	return out;
}

static void sb_free(StrBuf *sb) {
	free(sb->data);
	sb->data = HAWK_NULL;
	sb->len = 0;
	sb->cap = 0;
}

static int read_line_dyn(FILE *fp, char* *line_out) {
	StrBuf sb;
	int ch;
	int got = 0;

	sb_init(&sb);
	while ((ch = fgetc(fp)) != EOF)
	{
		got = 1;
		sb_append_char(&sb, (char)ch);
		if (ch == '\n') break;
	}

	if (!got)
	{
		sb_free(&sb);
		return -1;
	}

	*line_out = sb_take(&sb);
	return 0;
}

static void po_entry_init(PoEntry *e)
{
	memset(e, 0, sizeof(*e));
}

static void po_entry_free(PoEntry *e)
{
	int i;
	free(e->msgctx);
	free(e->msgid);
	free(e->msgid_plural);
	for (i = 0; i < e->msgstr_count; i++) free(e->msgstr[i]);
	free(e->msgstr);
	memset(e, 0, sizeof(*e));
}

static void po_entry_ensure_msgstr(PoEntry *e, int idx)
{
	int old;
	if (idx < 0) return;
	if (idx < e->msgstr_count) return;
	old = e->msgstr_count;
	e->msgstr_count = idx + 1;
	e->msgstr = (char* *)xrealloc(e->msgstr, sizeof(char* ) * (hawk_oow_t)e->msgstr_count);
	while (old < e->msgstr_count) e->msgstr[old++] = HAWK_NULL;
}

static char* *po_entry_target_msgstr(PoEntry *e, int idx)
{
	po_entry_ensure_msgstr(e, idx);
	return &e->msgstr[idx];
}

static void po_cat_add_entry(PoCatalog *cat, const PoEntry *src)
{
	PoEntry *dst;
	int i;

	if (cat->count == cat->capacity)
	{
		cat->capacity = cat->capacity ? cat->capacity * 2 : 32;
		cat->entries = (PoEntry *)xrealloc(cat->entries, cat->capacity * sizeof(PoEntry));
	}

	dst = &cat->entries[cat->count++];
	po_entry_init(dst);

	dst->msgctx = src->msgctx ? xstrdup0(src->msgctx) : HAWK_NULL;
	dst->msgid = src->msgid ? xstrdup0(src->msgid) : HAWK_NULL;
	dst->msgid_plural = src->msgid_plural ? xstrdup0(src->msgid_plural) : HAWK_NULL;
	dst->fuzzy = src->fuzzy;
	dst->obsolete = src->obsolete;
	dst->msgstr_count = src->msgstr_count;

	if (src->msgstr_count > 0)
   {
		dst->msgstr = (char* *)xcalloc((hawk_oow_t)src->msgstr_count, sizeof(char* ));
		for (i = 0; i < src->msgstr_count; i++)
			dst->msgstr[i] = src->msgstr[i] ? xstrdup0(src->msgstr[i]) : HAWK_NULL;
	}
}

static const char* find_header_line_value(const char* header, const char* key)
{
	hawk_oow_t key_len = strlen(key);
	const char* p = header;

	while (*p)
	{
		const char* line = p;
		const char* nl = strchr(p, '\n');
		hawk_oow_t len = nl ? (hawk_oow_t)(nl - line) : strlen(line);

		if (len >= key_len && strncmp(line, key, key_len) == 0) return line + key_len;

		p = nl ? nl + 1 : line + len;
		if (!nl) break;
	}
	return HAWK_NULL;
}

static char* dup_header_value_line(const char* header, const char* key)
{
	const char* v = find_header_line_value(header, key);
	const char* end;
	if (!v) return HAWK_NULL;
	while (*v == ' ' || *v == '\t') v++;
	end = strchr(v, '\n');
	if (!end) end = v + strlen(v);
	return xstrndup0(v, (hawk_oow_t)(end - v));
}

static int parse_nplurals_from_plural_forms(const char* s)
{
	const char* p = strstr(s, "nplurals=");
	int n = 2;

	if (!p) return 2;
	p += 9;
	while (*p && isspace((unsigned char)*p)) p++;
	if (isdigit((unsigned char)*p))
	{
		n = 0;
		while (isdigit((unsigned char)*p))
	{
			n = n * 10 + (*p - '0');
			p++;
		}
	}
	return n > 0 ? n : 2;
}

static char* parse_plural_expr_from_plural_forms(const char* s)
{
	const char* p = strstr(s, "plural=");
	const char* end;
	if (!p) return xstrdup0("(n != 1)");
	p += 7;
	while (*p && isspace((unsigned char)*p)) p++;
	end = strchr(p, ';');
	if (!end) end = p + strlen(p);
	return xstrndup0(p, (hawk_oow_t)(end - p));
}

static void po_cat_parse_header(PoCatalog *cat, const PoEntry *e)
{
	char* content_type;
	char* charset;

	if (!e->msgid || e->msgid[0] != '\0') return;
	if (e->msgstr_count < 1 || !e->msgstr || !e->msgstr[0]) return;

	free(cat->charset);
	free(cat->plural_forms_raw);
	free(cat->plural_expr);
	cat->charset = HAWK_NULL;
	cat->plural_forms_raw = HAWK_NULL;
	cat->plural_expr = HAWK_NULL;
	cat->nplurals = 2;

	content_type = dup_header_value_line(e->msgstr[0], "Content-Type:");
	if (content_type)
	{
		const char* p = strstr(content_type, "charset=");
		if (p)
	{
			p += 8;
			charset = xstrdup0(p);
			free(cat->charset);
			cat->charset = charset;
		}
		free(content_type);
	}

	cat->plural_forms_raw = dup_header_value_line(e->msgstr[0], "Plural-Forms:");
	if (cat->plural_forms_raw)
	{
		cat->nplurals = parse_nplurals_from_plural_forms(cat->plural_forms_raw);
		cat->plural_expr = parse_plural_expr_from_plural_forms(cat->plural_forms_raw);
	}
	else
	{
		cat->plural_expr = xstrdup0("(n != 1)");
	}
}

static char* po_parse_quoted(const char* s, int *ok_out) {
	StrBuf sb;
	*ok_out = 0;

	s = skip_ws(s);
	if (*s != '"') return HAWK_NULL;
	s++;

	sb_init(&sb);

	while (*s && *s != '"')
	{
		unsigned char c = (unsigned char)*s++;
		if (c == '\\')
	{
			unsigned char e = (unsigned char)*s++;
			switch (e)
	    {
				case 'a': sb_append_char(&sb, '\a'); break;
				case 'b': sb_append_char(&sb, '\b'); break;
				case 'f': sb_append_char(&sb, '\f'); break;
				case 'n': sb_append_char(&sb, '\n'); break;
				case 'r': sb_append_char(&sb, '\r'); break;
				case 't': sb_append_char(&sb, '\t'); break;
				case 'v': sb_append_char(&sb, '\v'); break;
				case '\\': sb_append_char(&sb, '\\'); break;
				case '"': sb_append_char(&sb, '"'); break;
				case '\'': sb_append_char(&sb, '\''); break;
				case '?': sb_append_char(&sb, '?'); break;

				case 'x':
		{
					int val = 0;
					int digits = 0;
					while (isxdigit((unsigned char)*s))
		    {
						int d;
						unsigned char h = (unsigned char)*s++;
						if (isdigit(h)) d = h - '0';
						else if (h >= 'a' && h <= 'f') d = 10 + (h - 'a');
						else d = 10 + (h - 'A');
						val = (val << 4) | d;
						digits++;
					}
					if (digits == 0)
		    {
						sb_free(&sb);
						return HAWK_NULL;
					}
					sb_append_char(&sb, (char)val);
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
					sb_append_char(&sb, (char)val);
					break;
				}

				case '\0':
					sb_free(&sb);
					return HAWK_NULL;

				default:
					sb_append_char(&sb, (char)e);
					break;
			}
		}
	else
	{
			sb_append_char(&sb, (char)c);
		}
	}

	if (*s != '"')
	{
		sb_free(&sb);
		return HAWK_NULL;
	}

	s++;
	s = skip_ws(s);
	if (*s != '\0')
	{
		sb_free(&sb);
		return HAWK_NULL;
	}

	*ok_out = 1;
	return sb_take(&sb);
}

static void expr_skip_ws(ExprParser *p)
{
	while (*p->s && isspace((unsigned char)*p->s)) p->s++;
}

static long expr_parse_ternary(ExprParser *p);

static long expr_parse_primary(ExprParser *p)
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

	if (*p->s == '-' || isdigit((unsigned char)*p->s))
	{
		char* end;
		errno = 0;
		v = strtol(p->s, &end, 10);
		if (end != p->s)
	{
			p->s = end;
			return v;
		}
	}

	return 0;
}

static long expr_parse_unary(ExprParser *p)
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

static long expr_parse_mul(ExprParser *p)
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
			v = (rhs == 0) ? 0 : (v / rhs);
		}
	else if (*p->s == '%')
	{
			p->s++;
			rhs = expr_parse_unary(p);
			v = (rhs == 0) ? 0 : (v % rhs);
		}
	else
	{
			break;
		}
	}
	return v;
}

static long expr_parse_add(ExprParser *p)
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
	else
	{
			break;
		}
	}
	return v;
}

static long expr_parse_rel(ExprParser *p) {
	long v = expr_parse_add(p);
	for (;;)
	{
		long rhs;
		expr_skip_ws(p);
		if (strncmp(p->s, "<=", 2) == 0)
	{
			p->s += 2;
			rhs = expr_parse_add(p);
			v = (v <= rhs);
		}
	else if (strncmp(p->s, ">=", 2) == 0)
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
	else
	{
			break;
		}
	}
	return v;
}

static long expr_parse_eq(ExprParser *p) {
	long v = expr_parse_rel(p);
	for (;;)
	{
		long rhs;
		expr_skip_ws(p);
		if (strncmp(p->s, "==", 2) == 0)
	{
			p->s += 2;
			rhs = expr_parse_rel(p);
			v = (v == rhs);
		}
	else if (strncmp(p->s, "!=", 2) == 0)
	{
			p->s += 2;
			rhs = expr_parse_rel(p);
			v = (v != rhs);
		}
	else
	{
			break;
		}
	}
	return v;
}

static long expr_parse_and(ExprParser *p)
{
	long v = expr_parse_eq(p);
	for (;;)
	{
		long rhs;
		expr_skip_ws(p);
		if (strncmp(p->s, "&&", 2) == 0)
	{
			p->s += 2;
			rhs = expr_parse_eq(p);
			v = (v && rhs);
		}
	else
	{
			break;
		}
	}
	return v;
}

static long expr_parse_or(ExprParser *p)
{
	long v = expr_parse_and(p);
	for (;;)
	{
		long rhs;
		expr_skip_ws(p);
		if (strncmp(p->s, "||", 2) == 0)
	{
			p->s += 2;
			rhs = expr_parse_and(p);
			v = (v || rhs);
		}
	else
	{
			break;
		}
	}
	return v;
}

static long expr_parse_ternary(ExprParser *p)
{
	long cond = expr_parse_or(p);
	expr_skip_ws(p);
	if (*p->s == '?')
	{
		long a, b;
		p->s++;
		a = expr_parse_ternary(p);
		expr_skip_ws(p);
		if (*p->s == ':') p->s++;
		b = expr_parse_ternary(p);
		return cond ? a : b;
	}
	return cond;
}

static int po_eval_plural_index(const PoCatalog *cat, unsigned long n)
{
	ExprParser p;
	long idx;

	if (!cat->plural_expr || !*cat->plural_expr) return (n != 1) ? 1 : 0;

	p.s = cat->plural_expr;
	p.n = n;
	idx = expr_parse_ternary(&p);

	if (idx < 0) idx = 0;
	if (cat->nplurals > 0 && idx >= cat->nplurals) idx = cat->nplurals - 1;
	return (int)idx;
}

static void parser_init(PoParser *p)
{
	po_entry_init(&p->cur);
	p->active_field = PF_NONE;
	p->active_msgstr_index = 0;
}

static void parser_reset_current(PoParser *p)
{
	po_entry_free(&p->cur);
	po_entry_init(&p->cur);
	p->active_field = PF_NONE;
	p->active_msgstr_index = 0;
}

static int parser_has_current_content(const PoParser *p)
{
	return p->cur.msgctx || p->cur.msgid || p->cur.msgid_plural ||
		   p->cur.msgstr_count > 0 || p->cur.fuzzy || p->cur.obsolete;
}

static void append_to_string(char* *dst, const char* frag)
{
	hawk_oow_t old_len = *dst ? strlen(*dst) : 0;
	hawk_oow_t add_len = strlen(frag);
	*dst = (char* )xrealloc(*dst, old_len + add_len + 1);
	memcpy(*dst + old_len, frag, add_len + 1);
}

static void parser_append_fragment(PoParser *p, const char* frag)
{
	switch (p->active_field) {
		case PF_MSGCTXT:
			if (!p->cur.msgctx) p->cur.msgctx = xstrdup0("");
			append_to_string(&p->cur.msgctx, frag);
			break;
		case PF_MSGID:
			if (!p->cur.msgid) p->cur.msgid = xstrdup0("");
			append_to_string(&p->cur.msgid, frag);
			break;
		case PF_MSGID_PLURAL:
			if (!p->cur.msgid_plural) p->cur.msgid_plural = xstrdup0("");
			append_to_string(&p->cur.msgid_plural, frag);
			break;
		case PF_MSGSTR:
	{
			char* *slot = po_entry_target_msgstr(&p->cur, p->active_msgstr_index);
			if (!*slot) *slot = xstrdup0("");
			append_to_string(slot, frag);
			break;
		}
		default:
			break;
	}
}

static int parse_msgstr_index_line(const char* s, int *idx_out, const char* *after_out)
{
	int idx = 0;
	if (strncmp(s, "msgstr[", 7) != 0) return 0;
	s += 7;
	if (!isdigit((unsigned char)*s)) return 0;
	while (isdigit((unsigned char)*s))
	{
		idx = idx * 10 + (*s - '0');
		s++;
	}
	if (*s != ']') return 0;
	s++;
	*idx_out = idx;
	*after_out = s;
	return 1;
}

static int parser_finalize_entry(PoCatalog *cat, PoParser *p)
{
	if (!parser_has_current_content(p)) return 1;
	if (!p->cur.msgid) p->cur.msgid = xstrdup0("");
	po_cat_add_entry(cat, &p->cur);
	if (p->cur.msgid[0] == '\0') po_cat_parse_header(cat, &p->cur);
	parser_reset_current(p);
	return 1;
}

int po_cat_init (PoCatalog *cat)
{
	if (!cat) return -1;
	memset(cat, 0, sizeof(*cat));
	cat->nplurals = 2;
	return 0;
}

void po_cat_fini (PoCatalog *cat)
{
	hawk_oow_t i;

	if (!cat) return;
	for (i = 0; i < cat->count; i++) po_entry_free(&cat->entries[i]);
	free(cat->entries);
	free(cat->charset);
	free(cat->plural_forms_raw);
	free(cat->plural_expr);
	memset(cat, 0, sizeof(*cat));
}

int po_cat_load_file (PoCatalog *cat, const char* path, char* errbuf, hawk_oow_t errbuf_sz)
{
	FILE *fp = fopen(path, "rb");
	PoParser p;
	char* line = HAWK_NULL;
	unsigned long line_no = 0;
	int ok = 1;

	if (!cat || !path)
	{
		if (errbuf && errbuf_sz) snprintf(errbuf, errbuf_sz, "invalid arguments");
		return -1;
	}

	if (!fp)
	{
		if (errbuf && errbuf_sz) snprintf(errbuf, errbuf_sz, "cannot open %s", path);
		return -1;
	}

	parser_init(&p);

	while (read_line_dyn(fp, &line) >= 0)
	{
		const char* s;
		char* frag;
		int qok;

		line_no++;
		trim_eol(line);
		s = skip_ws(line);

		if (is_blank_line(s))
		{
			if (!parser_finalize_entry(cat, &p))
			{
				ok = 0;
				break;
			}
			free(line);
			line = HAWK_NULL;
			continue;
		}

		if (strncmp(s, "#~", 2) == 0)
		{
			p.cur.obsolete = 1;
			s += 2;
			s = skip_ws(s);
			if (*s == '\0') {
				free(line);
				line = HAWK_NULL;
				continue;
			}
		}
		else if (*s == '#')
		{
			if (strncmp(s, "#,", 2) == 0 && strstr(s + 2, "fuzzy")) p.cur.fuzzy = 1;
			free(line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgctx"))
		{
			frag = po_parse_quoted(s + 7, &qok);
			if (!qok) goto syntax_error;
			free(p.cur.msgctx);
			p.cur.msgctx = frag;
			p.active_field = PF_MSGCTXT;
			free(line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgid_plural"))
		{
			frag = po_parse_quoted(s + 12, &qok);
			if (!qok) goto syntax_error;
			free(p.cur.msgid_plural);
			p.cur.msgid_plural = frag;
			p.active_field = PF_MSGID_PLURAL;
			free(line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgid"))
		{
			if (p.cur.msgid && (p.cur.msgstr_count > 0 || p.cur.msgctx || p.cur.msgid_plural))
			{
				if (!parser_finalize_entry(cat, &p))
				{
					ok = 0;
					break;
				}
			}
			frag = po_parse_quoted(s + 5, &qok);
			if (!qok) goto syntax_error;
			free(p.cur.msgid);
			p.cur.msgid = frag;
			p.active_field = PF_MSGID;
			free(line);
			line = HAWK_NULL;
			continue;
		}

		if (starts_with_kw(s, "msgstr"))
		{
			int idx = 0;
			const char* after = HAWK_NULL;

			if (parse_msgstr_index_line(s, &idx, &after))
			{
				frag = po_parse_quoted(after, &qok);
			}
			else
			{
				idx = 0;
				frag = po_parse_quoted(s + 6, &qok);
			}

			if (!qok) goto syntax_error;

			po_entry_ensure_msgstr(&p.cur, idx);
			free(p.cur.msgstr[idx]);
			p.cur.msgstr[idx] = frag;
			p.active_field = PF_MSGSTR;
			p.active_msgstr_index = idx;

			free(line);
			line = HAWK_NULL;
			continue;
		}

		if (*s == '"')
		{
			frag = po_parse_quoted(s, &qok);
			if (!qok) goto syntax_error;
			parser_append_fragment(&p, frag);
			free(frag);
			free(line);
			line = HAWK_NULL;
			continue;
		}

syntax_error:
		if (errbuf && errbuf_sz) snprintf(errbuf, errbuf_sz, "%s:%lu: syntax error", path, line_no);
		ok = 0;
		free(line);
		line = HAWK_NULL;
		break;
	}

	if (ok) ok = parser_finalize_entry(cat, &p);

	free(line);
	parser_reset_current(&p);
	fclose(fp);

	return ok? 0: -1;
}

static int po_entry_matches (const PoEntry *e, const char* msgctx, const char* msgid)
{
	if (!e->msgid || !msgid) return 0;
	if (e->obsolete) return 0;
	if (strcmp(e->msgid, msgid) != 0) return 0;

	if (msgctx == HAWK_NULL && e->msgctx == HAWK_NULL) return 1;

	/* match the context as well */
	if (msgctx != HAWK_NULL && e->msgctx != HAWK_NULL && strcmp(e->msgctx, msgctx) == 0) return 1;
	return 0;
}

static const PoEntry *po_cat_find (const PoCatalog *cat, const char* msgctx, const char* msgid)
{
	hawk_oow_t i;
	/* TODO: this serial match is slow... */
	for (i = 0; i < cat->count; i++)
	{
		if (po_entry_matches(&cat->entries[i], msgctx, msgid)) return &cat->entries[i];
	}
	return HAWK_NULL;
}

const char* po_cat_get (const PoCatalog *cat, const char* msgctx, const char* msgid)
{
	const PoEntry *e;
	if (!cat || !msgid) return HAWK_NULL;

	e = po_cat_find(cat, msgctx, msgid);
	if (!e) return HAWK_NULL;
	if (e->fuzzy) return HAWK_NULL;
	if (e->msgstr_count < 1 || !e->msgstr || !e->msgstr[0] || !e->msgstr[0][0]) return HAWK_NULL;
	return e->msgstr[0];
}

const char* po_cat_gettext (const PoCatalog *cat, const char* msgctx, const char* msgid)
{
	const char* text;
	text = po_cat_get(cat, msgctx, msgid);
	if (!text) text = msgid;
	return text;
}

const char* po_cat_nget (const PoCatalog *cat, const char* msgctx, const char* msgid, const char* msgid_plural, unsigned long n)
{
	const PoEntry *e;
	int idx;

	if (!cat || !msgid || !msgid_plural) return (n == 1)? msgid : msgid_plural;

	e = po_cat_find(cat, msgctx, msgid);
	if (!e || e->fuzzy) return (n == 1)? msgid : msgid_plural;

	idx = po_eval_plural_index(cat, n);
	if (idx >= 0 && idx < e->msgstr_count && e->msgstr[idx] && e->msgstr[idx][0]) return e->msgstr[idx];

	return (n == 1) ? msgid : msgid_plural;
}
