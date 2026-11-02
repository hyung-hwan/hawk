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

#include <hawk-rgx.h>
#include "hawk-prv.h"

#define OCC_MAX HAWK_TYPE_MAX(hawk_oow_t)

/*#define XTRA_DEBUG*/

typedef struct comp_t comp_t;
struct comp_t
{
	hawk_rgx_t* rgx;

	const hawk_ooch_t* ptr;
	const hawk_ooch_t* end;

	struct
	{
		hawk_ooci_t value;
		int escaped;
		int special; /* grammar operator in the selected dialect */
		int backref; /* raw \\1..\\9, not a digit produced by \\x or \\u */
	} c;

	hawk_oow_t gdepth; /* group depth */
	unsigned int closed_groups; /* previously closed groups 1..9 */
	hawk_rgx_node_t* start;
};

typedef struct exec_t exec_t;
struct exec_t
{
	hawk_rgx_t* rgx;

	struct
	{
		const hawk_ooch_t* ptr;
		const hawk_ooch_t* end;
	} str;

	struct
	{
		const hawk_ooch_t* ptr;
		const hawk_ooch_t* end;
	} sub;

	struct
	{
		int active;
		int pending;
		hawk_arr_t set[2]; /* candidate arrays */
	} cand;

	hawk_oow_t nmatches;
	int failed; /* a candidate callback can fail even on the final array slot */
	int flags; /* per-execution anchor overrides, not object options */
	const hawk_ooch_t* matchend; /* 1 character past the match end */
	hawk_oow_t ncaps;
	hawk_oocs_t* bestcaps;
};

typedef struct pair_t pair_t;
struct pair_t
{
	hawk_rgx_node_t* head;
	hawk_rgx_node_t* tail;
};

/* The group_t type defines a structure to maintain the nested
 * traces of subgroups. The actual traces are maintained in a stack
 * of sinlgly linked group_t elements. The head element acts
 * as a management element where the occ field is a reference count
 * and the node field is HAWK_NULL always. When capture tracking is active,
 * the head owns a trailing array of completed capture slices. Keep this
 * head even after the last active group is popped: a later backreference
 * or the final match result can still need those completed captures.
 */
typedef struct group_t group_t;
struct group_t
{
	hawk_rgx_node_t* node;
	hawk_oow_t occ;
	group_t* next;
	hawk_oow_t revision; /* head: capture changes; member: value at iteration start */
	union
	{
		const hawk_ooch_t* start; /* start of the current group iteration */
		struct
		{
			hawk_oow_t count;
			hawk_oocs_t* ptr; /* completed captures, stored after the head */
		} caps;
	} u;
};

typedef struct cand_t cand_t;
struct cand_t
{
	hawk_rgx_node_t*   node;

	/* occurrence */
	hawk_oow_t        occ;

	/* the stack of groups that this candidate belongs to.
	 * it is in the singliy linked list form */
	group_t*          group;

	/* match pointer. the number of character advancement
	 * differs across various node types. BOL and EOL don't advance to
	 * the next character on match while ANY and CHAR do on match.
	 * therefore, the match pointer is managed per candidate basis. */
	const hawk_ooch_t* mptr;
};

int hawk_rgx_init (hawk_rgx_t* rgx, hawk_gem_t* gem, hawk_rgx_node_t* code)
{
	HAWK_MEMSET(rgx, 0, HAWK_SIZEOF(*rgx));
	rgx->gem = gem;

	HAWK_ASSERT(code == HAWK_NULL || code->id == HAWK_RGX_NODE_START);

	/* note that passing a compiled expression to hawk_rgx_open()
	 * is to delegate it to this rex object. when this rex object
	 * is closed, the code delegated is destroyed. */

	rgx->code = code;
	return 0;
}

hawk_rgx_t* hawk_rgx_open (hawk_gem_t* gem, hawk_oow_t xtnsize, hawk_rgx_node_t* code)
{
	hawk_rgx_t* rgx;

	HAWK_ASSERT(xtnsize <= HAWK_TYPE_MAX(hawk_oow_t) - HAWK_SIZEOF(*rgx));

	rgx = (hawk_rgx_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_rgx_t) + xtnsize);
	if (HAWK_UNLIKELY(!rgx)) return HAWK_NULL;

	if (hawk_rgx_init(rgx, gem, code) <= -1)
	{
		hawk_gem_freemem(gem, rgx);
		return HAWK_NULL;
	}

	HAWK_MEMSET((rgx + 1), 0, xtnsize);
	return rgx;
}

static void freenode (hawk_rgx_node_t* node, hawk_gem_t* gem)
{
	if (node->id == HAWK_RGX_NODE_CSET)
	{
		if (node->u.cset.member != HAWK_NULL)
			hawk_ooecs_close(node->u.cset.member);
	}

	hawk_gem_freemem(gem, node);
}

static void freeallnodes (hawk_rgx_node_t* start)
{
	hawk_rgx_node_t* x, * y;
	hawk_gem_t* gem;

	HAWK_ASSERT(start->id == HAWK_RGX_NODE_START);

	gem = start->u.s.gem;
	x = start->u.s.link;
	while (x != HAWK_NULL)
	{
		y = x; x = x->link;
		freenode(y, gem);
	}

	hawk_gem_freemem(gem, start);
}

void hawk_rgx_fini (hawk_rgx_t* rgx)
{
	if (rgx->code != HAWK_NULL)
	{
		freeallnodes(rgx->code);
		rgx->code = HAWK_NULL;
	}
}

void hawk_rgx_close (hawk_rgx_t* rgx)
{
	hawk_rgx_fini(rgx);
	hawk_gem_freemem(rgx->gem, rgx);
}

hawk_gem_t* hawk_rgx_getgem (hawk_rgx_t* rgx)
{
	return rgx->gem;
}

void* hawk_rgx_getxtn (hawk_rgx_t* rgx)
{
	return (rgx + 1);
}

hawk_rgx_node_t* hawk_rgx_yield (hawk_rgx_t* rgx)
{
	hawk_rgx_node_t* code = rgx->code;
	rgx->code = HAWK_NULL;
	return code;
}

int hawk_rgx_getopt (const hawk_rgx_t* rgx)
{
	return rgx->option;
}

void hawk_rgx_setopt (hawk_rgx_t* rgx, int opts)
{
	rgx->option = opts;
}

hawk_errnum_t hawk_rgx_geterrnum (const hawk_rgx_t* rgx)
{
	return hawk_gem_geterrnum(rgx->gem);
}

const hawk_ooch_t* hawk_rgx_geterrmsg (const hawk_rgx_t* rgx)
{
	return hawk_gem_geterrmsg(rgx->gem);
}

static hawk_rgx_node_t* newnode (comp_t* c, hawk_rgx_node_id_t id)
{
	hawk_rgx_node_t* node;

	/* TODO: performance optimization.
	 *       preallocate a large chunk of memory and allocate a node
	 *       from the chunk. increase the chunk if it has been used up.
	 */

	node = (hawk_rgx_node_t*)hawk_gem_allocmem(c->rgx->gem, HAWK_SIZEOF(hawk_rgx_node_t));
	if (HAWK_UNLIKELY(!node))
	{
		hawk_gem_seterrnum(c->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return HAWK_NULL;
	}

	HAWK_MEMSET(node, 0, HAWK_SIZEOF(*node));
	node->id = id;

	if (c->start != HAWK_NULL)
	{
		HAWK_ASSERT(c->start->id == HAWK_RGX_NODE_START);
		node->link = c->start->u.s.link;
		c->start->u.s.link = node;
	}

	return node;
}

static hawk_rgx_node_t* newstartnode (comp_t* c)
{
	hawk_rgx_node_t* n = newnode(c, HAWK_RGX_NODE_START);
	if (HAWK_LIKELY(n))
	{
		n->u.s.gem = c->rgx->gem;
		n->u.s.link = HAWK_NULL;
	}
	return n;
}

static hawk_rgx_node_t* newendnode (comp_t* c)
{
	return newnode(c, HAWK_RGX_NODE_END);
}

static hawk_rgx_node_t* newnopnode (comp_t* c)
{
	return newnode(c, HAWK_RGX_NODE_NOP);
}

static hawk_rgx_node_t* newgroupnode (comp_t* c)
{
	return newnode(c, HAWK_RGX_NODE_GROUP);
}

static hawk_rgx_node_t* newgroupendnode (comp_t* c, hawk_rgx_node_t* group)
{
	hawk_rgx_node_t* n = newnode(c, HAWK_RGX_NODE_GROUPEND);
	if (n != HAWK_NULL) n->u.ge.group = group;
	return n;
}

static hawk_rgx_node_t* newcharnode (comp_t* c, hawk_ooch_t ch)
{
	hawk_rgx_node_t* n = newnode(c, HAWK_RGX_NODE_CHAR);
	if (HAWK_LIKELY(n)) n->u.c = ch;
	return n;
}

static hawk_rgx_node_t* newbranchnode (comp_t* c, hawk_rgx_node_t* left, hawk_rgx_node_t* alter)
{
	hawk_rgx_node_t* n = newnode(c, HAWK_RGX_NODE_BRANCH);
	if (HAWK_LIKELY(n))
	{
		/*n->u.b.left = left; */
		n->next = left;
		n->u.b.alter = alter;
	}
	return n;
}

#define CHECK_END(com) \
	do { \
		if (com->ptr >= com->end) \
		{ \
			hawk_gem_seterrbfmt(com->rgx->gem, HAWK_NULL, HAWK_EREXBADPAT, "premature end of regular expression"); \
			return -1; \
		} \
	} while(0)

#define IS_HEX(c) \
	((c >= HAWK_T('0') && c <= HAWK_T('9')) || \
	 (c >= HAWK_T('A') && c <= HAWK_T('F')) || \
	 (c >= HAWK_T('a') && c <= HAWK_T('f')))

#define HEX_TO_NUM(c) \
	((c >= HAWK_T('0') && c <= HAWK_T('9'))? c-HAWK_T('0'):  \
	 (c >= HAWK_T('A') && c <= HAWK_T('F'))? c-HAWK_T('A')+10: \
	                                       c-HAWK_T('a')+10)

#define IS_SPE(com,ch) ((com)->c.value == (ch) && !(com)->c.escaped)
#define IS_OP(com,ch) ((com)->c.value == (ch) && (com)->c.special)
#define IS_EOF(com) ((com)->c.value == HAWK_OOCI_EOF)

#define getc_noesc(c) get_char(c,1)
#define getc_esc(c)   get_char(c,0)

static int get_char (comp_t* com, int noesc)
{
	hawk_ooch_t c;

	com->c.backref = 0;
	com->c.special = 0;

	if (com->ptr >= com->end)
	{
		com->c.value = HAWK_OOCI_EOF;
		com->c.escaped = 0;
		return 0;
	}

	com->c.value = (hawk_oochu_t)*com->ptr++;
	com->c.escaped = 0;

	if (noesc) return 0;
	if (com->c.value != HAWK_T('\\'))
	{
		switch (com->c.value)
		{
			case HAWK_T('.'):
			case HAWK_T('['):
			case HAWK_T('^'):
			case HAWK_T('$'):
			case HAWK_T('*'):
				com->c.special = 1;
				break;
			case HAWK_T('('):
			case HAWK_T(')'):
			case HAWK_T('{'):
			case HAWK_T('+'):
			case HAWK_T('?'):
			case HAWK_T('|'):
				com->c.special = !!(com->rgx->option & HAWK_RGX_EXTENDED);
				break;
		}
		return 0;
	}

	if (com->ptr >= com->end)
	{
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXESCAPE);
		return -1;
	}
	c = *com->ptr++;
	if (c >= HAWK_T('1') && c <= HAWK_T('9')) com->c.backref = c - HAWK_T('0');
	/* Recognize raw BRE operators before expanding character escapes.
	 * A digit or parenthesis produced by \\x/\\u is still a literal. */
	if (!(com->rgx->option & HAWK_RGX_EXTENDED) &&
	    (c == HAWK_T('(') || c == HAWK_T(')') || c == HAWK_T('{') ||
	     c == HAWK_T('+') || c == HAWK_T('?') || c == HAWK_T('|'))) com->c.special = 1;

	if (c == HAWK_T('n')) c = HAWK_T('\n');
	else if (c == HAWK_T('r')) c = HAWK_T('\r');
	else if (c == HAWK_T('t')) c = HAWK_T('\t');
	else if (c == HAWK_T('f')) c = HAWK_T('\f');
	else if (c == HAWK_T('b')) c = HAWK_T('\b');
	else if (c == HAWK_T('v')) c = HAWK_T('\v');
	else if (c == HAWK_T('a')) c = HAWK_T('\a');

#if 0
	/* backrefernce conflicts with octal notation */
	else if (c >= HAWK_T('0') && c <= HAWK_T('7'))
	{
		hawk_ooch_t cx;

		c = c - HAWK_T('0');

		CHECK_END (com);
		cx = *com->ptr++;
		if (cx >= HAWK_T('0') && cx <= HAWK_T('7'))
		{
			c = c * 8 + cx - HAWK_T('0');

			CHECK_END (com);
			cx = *com->ptr++;
			if (cx >= HAWK_T('0') && cx <= HAWK_T('7'))
			{
				c = c * 8 + cx - HAWK_T('0');
			}
		}
	}
#endif

	else if (c == HAWK_T('x'))
	{
		hawk_ooch_t cx;

		CHECK_END (com);
		cx = *com->ptr++;
		if (IS_HEX(cx))
		{
			c = HEX_TO_NUM(cx);

			CHECK_END (com);
			cx = *com->ptr++;
			if (IS_HEX(cx))
			{
				c = c * 16 + HEX_TO_NUM(cx);
			}
		}
	}
#if defined(HAWK_OOCH_IS_UCH)
	else if (c == HAWK_T('u') && HAWK_SIZEOF(hawk_ooch_t) >= 2)
	{
		hawk_ooch_t cx;

		CHECK_END (com);
		cx = *com->ptr++;
		if (IS_HEX(cx))
		{
			hawk_oow_t i;

			c = HEX_TO_NUM(cx);

			for (i = 0; i < 3; i++)
			{
				CHECK_END (com);
				cx = *com->ptr++;

				if (!IS_HEX(cx)) break;
				c = c * 16 + HEX_TO_NUM(cx);
			}
		}
	}
	else if (c == HAWK_T('U') && HAWK_SIZEOF(hawk_ooch_t) >= 4)
	{
		hawk_ooch_t cx;

		CHECK_END (com);
		cx = *com->ptr++;
		if (IS_HEX(cx))
		{
			hawk_oow_t i;

			c = HEX_TO_NUM(cx);

			for (i = 0; i < 7; i++)
			{
				CHECK_END (com);
				cx = *com->ptr++;

				if (!IS_HEX(cx)) break;
				c = c * 16 + HEX_TO_NUM(cx);
			}
		}
	}
#endif

	com->c.value = (hawk_oochu_t)c;
	com->c.escaped = 1;

#if 0
	com->c = (com->ptr < com->end)? *com->ptr++: HAWK_OOCI_EOF;
if (com->c == HAWK_OOCI_EOF)
hawk_printf(HAWK_T("get_char => <EOF>\n"));
else hawk_printf(HAWK_T("get_char => %c\n"), com->c);
#endif
	return 0;
}

/* Read a bracket member, optionally quoting the next character after a
 * backslash. This is quoting only: bracket \n/\1 are literal n/1, not the
 * character escapes/backreferences recognized by getc_esc() outside brackets.
 * Reject collation prefixes only when their '[' is unescaped. */
static int get_cset_char (comp_t* com)
{
	if (getc_noesc(com) <= -1) return -1;
	if ((com->rgx->option & HAWK_RGX_BRACKETESC) && com->c.value == HAWK_T('\\'))
	{
		if (com->ptr >= com->end)
		{
			/* Within brackets the missing ']' takes precedence, as in TRE. */
			hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXBRACK);
			return -1;
		}
		if (getc_noesc(com) <= -1) return -1;
		com->c.escaped = 1;
	}
	if (IS_SPE(com, HAWK_T('[')) && com->ptr < com->end &&
	    (*com->ptr == HAWK_T('.') || *com->ptr == HAWK_T('=')))
	{
		hawk_gem_seterrbfmt(com->rgx->gem, HAWK_NULL, HAWK_EREXCOLLATE,
			"%s are not supported in regular expressions",
			(*com->ptr == HAWK_T('.'))? "collating symbols": "equivalence classes");
		return -1;
	}
	return 0;
}

struct ccinfo_t
{
	hawk_oocs_t name;
	int (*func) (exec_t* e, hawk_ooch_t c);
};

#define ISBLANK(c) ((c) == HAWK_T(' ') || (c) == HAWK_T('\t'))

static int cc_isalnum (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_alnum(c); }
static int cc_isalpha (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_alpha(c); }
static int cc_isblank (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_blank(c); }
static int cc_iscntrl (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_cntrl(c); }
static int cc_isdigit (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_digit(c); }
static int cc_isgraph (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_graph(c); }

static int cc_islower (exec_t* e, hawk_ooch_t c)
{
	if (e->rgx->option & HAWK_RGX_IGNORECASE) return hawk_is_ooch_alpha(c);
	return hawk_is_ooch_lower(c);
}

static int cc_isprint (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_print(c); }
static int cc_ispunct (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_punct(c); }
static int cc_isspace (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_space(c); }

static int cc_isupper (exec_t* e, hawk_ooch_t c)
{
	if (e->rgx->option & HAWK_RGX_IGNORECASE) return hawk_is_ooch_alpha(c);
	return hawk_is_ooch_upper(c);
}

static int cc_isxdigit (exec_t* e, hawk_ooch_t c) { return hawk_is_ooch_xdigit (c); }

static int cc_isword (exec_t* e, hawk_ooch_t c)
{
	return hawk_is_ooch_alnum(c) || c == HAWK_T('_');
}

static struct ccinfo_t ccinfo[] =
{
	{ { HAWK_T("alnum"),  5 }, cc_isalnum },
	{ { HAWK_T("alpha"),  5 }, cc_isalpha },
	{ { HAWK_T("blank"),  5 }, cc_isblank },
	{ { HAWK_T("cntrl"),  5 }, cc_iscntrl },
	{ { HAWK_T("digit"),  5 }, cc_isdigit },
	{ { HAWK_T("graph"),  5 }, cc_isgraph },
	{ { HAWK_T("lower"),  5 }, cc_islower },
	{ { HAWK_T("print"),  5 }, cc_isprint },
	{ { HAWK_T("punct"),  5 }, cc_ispunct },
	{ { HAWK_T("space"),  5 }, cc_isspace },
	{ { HAWK_T("upper"),  5 }, cc_isupper },
	{ { HAWK_T("xdigit"), 6 }, cc_isxdigit },
	{ { HAWK_T("word"),   4 }, cc_isword },

	/*
	{ { HAWK_T("arabic"),   6 }, cc_isarabic },
	{ { HAWK_T("chinese"),  7 }, cc_ischinese },
	{ { HAWK_T("english"),  7 }, cc_isenglish },
	{ { HAWK_T("japanese"), 8 }, cc_isjapanese },
	{ { HAWK_T("korean"),   6 }, cc_iskorean },
	{ { HAWK_T("thai"),     4 }, cc_isthai },
	*/

	{ { HAWK_NULL,          0 }, HAWK_NULL }
};

static int charclass (comp_t* com)
{
	const struct ccinfo_t* ccp = ccinfo;
	hawk_oow_t len = com->end - com->ptr;

	while (ccp->name.ptr != HAWK_NULL)
	{
		if (len >= ccp->name.len && hawk_comp_oochars(com->ptr, ccp->name.len, ccp->name.ptr, ccp->name.len, 0) == 0) break;
		ccp++;
	}

	if (ccp->name.ptr == HAWK_NULL)
	{
		/* wrong class name */
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXCTYPE);
		return -1;
	}

	com->ptr += ccp->name.len;

	if (getc_noesc(com) <= -1) return -1;
	if (com->c.value != HAWK_T(':'))
	{
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXCTYPE);
		return -1;
	}

	if (getc_noesc(com) <= -1) return -1;
	if (com->c.value != HAWK_T(']'))
	{
		/* The inner ':]' belongs to the named class. A missing/wrong
		 * terminator is a class error, not the outer bracket's error. */
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXCTYPE);
		return -1;
	}

	/* The character class ends inside the enclosing bracket expression.
	 * Apply the selected bracket-quoting policy to the next member. */
	if (get_cset_char(com) <= -1) return -1;
	return (int)(ccp - ccinfo);
}

#define ADD_CSET_CODE(com,node,code,len) \
	do { if (add_cset_code(com,node,code,len) <= -1) return -1; } while(0)

static int add_cset_code (
	comp_t* com, hawk_rgx_node_t* node, const hawk_ooch_t* c, hawk_oow_t l)
{
	if (hawk_ooecs_ncat(node->u.cset.member, c, l) == (hawk_oow_t)-1)
	{
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return -1;
	}
	return 0;
}

static int charset (comp_t* com, hawk_rgx_node_t* node)
{
	HAWK_ASSERT(node->id == HAWK_RGX_NODE_CSET);
	HAWK_ASSERT(node->u.cset.negated == 0);
	HAWK_ASSERT(node->u.cset.member == HAWK_NULL);

	if (IS_SPE(com,HAWK_T('^')))
	{
		/* negate an expression */
		node->u.cset.negated = 1;
		if (get_cset_char(com) <= -1) return -1;
	}


	/* initialize the member array */
	node->u.cset.member = hawk_ooecs_open(com->rgx->gem, 0, 64);
	if (node->u.cset.member == HAWK_NULL)
	{
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return -1;
	}

	/* if ] is the first character or the second character following ^,
	 * it is treated literally */

	do
	{
		hawk_ooci_t c1, c2;
		int c1_escaped;

		c1 = com->c.value;
		c1_escaped = com->c.escaped;
		/* Test the integer token before narrowing it to a character. EOF
		 * is not representable by byte or 16-bit character types. */
		if (IS_EOF(com))
		{
			hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXBRACK);
			return -1;
		}

		/* Keep c1's quoting state before reading the lookahead member. An
		 * escaped '[' must not introduce a named class when c2 is ':'. */
		if (get_cset_char(com) <= -1) return -1;
		c2 = com->c.value;

		if (c1 == HAWK_T('[') && !c1_escaped && IS_SPE(com, HAWK_T(':')))
		{
			int n;
			hawk_ooch_t tmp[2];

			/* begins with [:
			 * don't read in the next character as charclass()
			 * matches a class name differently from other routines.
			 * if (getc_noesc(com) <= -1) return -1;
			 */
			if ((n = charclass(com)) <= -1) return -1;

			HAWK_ASSERT(n < HAWK_TYPE_MAX(hawk_ooch_t));

			tmp[0] = HAWK_RGX_CSET_CLASS;
			tmp[1] = n;
			ADD_CSET_CODE(com, node, tmp, HAWK_COUNTOF(tmp));
		}
		else if (IS_SPE(com, HAWK_T('-')))
		{
			if (get_cset_char(com) <= -1) return -1;
			if (IS_SPE(com, HAWK_T(']')))
			{
				hawk_ooch_t tmp[4];

				/* '-' is the last character in the set.
				 * treat it literally */

				tmp[0] = HAWK_RGX_CSET_CHAR;
				tmp[1] = c1;
				tmp[2] = HAWK_RGX_CSET_CHAR;
				tmp[3] = c2;

				ADD_CSET_CODE(com, node, tmp, HAWK_COUNTOF(tmp));
				break;
			}

			if (c1 > com->c.value)
			{
				/* range end must be >= range start */
				hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXRANGE);
				return -1;
			}
			else if (c1 == com->c.value)
			{
				/* if two chars in the range are the same,
				 * treat it as a single character */
				hawk_ooch_t tmp[2];
				tmp[0] = HAWK_RGX_CSET_CHAR;
				tmp[1] = c1;
				ADD_CSET_CODE(com, node, tmp, HAWK_COUNTOF(tmp));
			}
			else
			{
				hawk_ooch_t tmp[3];
				tmp[0] = HAWK_RGX_CSET_RANGE;
				tmp[1] = c1;
				tmp[2] = com->c.value;
				ADD_CSET_CODE(com, node, tmp, HAWK_COUNTOF(tmp));
			}

			if (get_cset_char(com) <= -1) return -1;
		}
		else
		{
			hawk_ooch_t tmp[2];
			tmp[0] = HAWK_RGX_CSET_CHAR;
			tmp[1] = c1;
			ADD_CSET_CODE(com, node, tmp, HAWK_COUNTOF(tmp));
		}
	}
	while (!IS_SPE(com,HAWK_T(']')));

	/* the bracket expression has ended. resume normal escape processing. */
	if (getc_esc(com) <= -1) return -1;
	return 0;
}

static int occbound (comp_t* com, hawk_rgx_node_t* n)
{
	hawk_oow_t bound;

	if (IS_EOF(com)) goto bad_brace;
	if (com->c.value < HAWK_T('0') || com->c.value > HAWK_T('9')) goto bad_bound;

	bound = 0;
	while (com->c.value >= HAWK_T('0') && com->c.value <= HAWK_T('9'))
	{
		if (bound > (OCC_MAX - (com->c.value - HAWK_T('0'))) / 10) goto bad_bound;
		bound = bound * 10 + com->c.value - HAWK_T('0');
		if (getc_noesc(com) <= -1) return -1;
	}

	n->occ.min = bound;

	if (com->c.value == HAWK_T(','))
	{
		if (getc_noesc(com) <= -1) return -1;

		if (com->c.value >= HAWK_T('0') && com->c.value <= HAWK_T('9'))
		{
			bound = 0;

			do
			{
				if (bound > (OCC_MAX - (com->c.value - HAWK_T('0'))) / 10) goto bad_bound;
				bound = bound * 10 + com->c.value - HAWK_T('0');
				if (getc_noesc(com) <= -1) return -1;
			}
			while (com->c.value >= HAWK_T('0') && com->c.value <= HAWK_T('9'));

			n->occ.max = bound;
		}
		else n->occ.max = OCC_MAX;
	}
	else n->occ.max = n->occ.min;

	if (n->occ.min > n->occ.max)
	{
		/* invalid occurrences range */
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXBADBR);
		return -1;
	}

	/* End of input before a terminator is an imbalanced brace. A wrong
	 * terminator or unfinished BRE '\}' is invalid brace content instead.
	 * Keep this distinction identical to TRE's interval parser. */
	if (IS_EOF(com)) goto bad_brace;
	if (!(com->rgx->option & HAWK_RGX_EXTENDED))
	{
		if (com->c.value != HAWK_T('\\')) goto bad_bound;
		if (getc_noesc(com) <= -1) return -1;
	}
	if (com->c.value != HAWK_T('}')) goto bad_bound;

	if (getc_esc(com) <= -1) return -1;
	return 0;

bad_brace:
	hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXBRACE);
	return -1;

bad_bound:
	hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXBADBR);
	return -1;
}

static hawk_rgx_node_t* comp_branches (comp_t* com, hawk_rgx_node_t* ge);

static hawk_rgx_node_t* comp_group (comp_t* com)
{
	/* enter a subgroup */
	hawk_rgx_node_t* body, *g, * ge;

	g = newgroupnode(com);
	if (g == HAWK_NULL) return HAWK_NULL;

	ge = newgroupendnode(com, g);
	if (ge == HAWK_NULL) return HAWK_NULL;
	g->u.g.index = com->start->u.s.nsub++;

	/* skip '(' */
	if (getc_esc(com) <= -1) return HAWK_NULL;

	com->gdepth++;

	/* pass the GROUPEND node so that the
	 * last node in the subgroup links to
	 * this GROUPEND node. */
	body = comp_branches(com, ge);
	if (body == HAWK_NULL) return HAWK_NULL;

	if (!IS_OP(com,HAWK_T(')')))
	{
		hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXPAREN);
		return HAWK_NULL;
	}

	com->gdepth--;
	if (g->u.g.index < 9) com->closed_groups |= 1u << g->u.g.index;

	/* skip ')' */
	if (getc_esc(com) <= -1) return HAWK_NULL;

	g->u.g.head = body;
	g->u.g.end = ge;

	return g;
}

static hawk_rgx_node_t* comp_occ (comp_t* com, hawk_rgx_node_t* atom)
{
	switch (com->c.value)
	{
		case HAWK_T('?'):
			atom->occ.min = 0;
			atom->occ.max = 1;
			if (getc_esc(com) <= -1) return HAWK_NULL;
			break;

		case HAWK_T('*'):
			atom->occ.min = 0;
			atom->occ.max = OCC_MAX;
			if (getc_esc(com) <= -1) return HAWK_NULL;
			break;

		case HAWK_T('+'):
			atom->occ.min = 1;
			atom->occ.max = OCC_MAX;
			if (getc_esc(com) <= -1) return HAWK_NULL;
			break;

		case HAWK_T('{'):
			if (!(com->rgx->option & HAWK_RGX_NOBOUND))
			{
				if (getc_noesc(com) <= -1 ||
				    occbound(com,atom) <= -1) return HAWK_NULL;
			}
			break;
	}

	return atom;
}

static hawk_rgx_node_t* comp_atom (comp_t* com, int first)
{
	hawk_rgx_node_t* atom;
	int assertion = 0;

	if (com->c.backref)
	{
		if (!(com->closed_groups & (1u << (com->c.backref - 1))))
		{
			hawk_gem_seterrbfmt(com->rgx->gem, HAWK_NULL, HAWK_EREXSUBREG,
				"backreference refers to an unclosed or nonexistent group");
			return HAWK_NULL;
		}

		atom = newnode(com, HAWK_RGX_NODE_BACKREF);
		if (HAWK_UNLIKELY(!atom)) return HAWK_NULL;
		atom->u.bref.index = com->c.backref - 1;
		com->start->u.s.backrefs = 1;
		if (getc_esc(com) <= -1) return HAWK_NULL;
	}
	else if (com->c.special)
	{
		switch (com->c.value)
		{
			case HAWK_T('('):
				atom = comp_group(com);
				if (atom == HAWK_NULL) return HAWK_NULL;
				break;

			case HAWK_T('.'):
				atom = newnode(com, HAWK_RGX_NODE_ANY);
				if (HAWK_UNLIKELY(!atom)) return HAWK_NULL;
				if (getc_esc(com) <= -1) return HAWK_NULL;
				break;

			case HAWK_T('^'):
				if (!(com->rgx->option & HAWK_RGX_EXTENDED) && !first) goto normal_char;
				assertion = 1;
				atom = newnode(com, HAWK_RGX_NODE_BOL);
				if (HAWK_UNLIKELY(!atom)) return HAWK_NULL;
				if (getc_esc(com) <= -1) return HAWK_NULL;
				break;

			case HAWK_T('$'):
				if (!(com->rgx->option & HAWK_RGX_EXTENDED) &&
				    com->ptr != com->end &&
				    !(com->end - com->ptr >= 2 && com->ptr[0] == HAWK_T('\\') &&
				      (com->ptr[1] == HAWK_T(')') || com->ptr[1] == HAWK_T('|'))))
					goto normal_char;
				assertion = 1;
				atom = newnode(com, HAWK_RGX_NODE_EOL);
				if (HAWK_UNLIKELY(!atom)) return HAWK_NULL;
				if (getc_esc(com) <= -1) return HAWK_NULL;
				break;

			case HAWK_T('['):
				atom = newnode(com, HAWK_RGX_NODE_CSET);
				if (HAWK_UNLIKELY(!atom)) return HAWK_NULL;
				/* Read the first member with the same quoting policy as
				 * later members, including before the initial '^' test. */
				if (get_cset_char(com) <= -1) return HAWK_NULL;
				if (charset(com, atom) <= -1) return HAWK_NULL;
				break;

			default:
				if (!(com->rgx->option & HAWK_RGX_EXTENDED))
				{
					/* A leading '*' is literal in BRE, including after
					 * an initial anchor. An unmatched \\) is an error. */
					if (com->c.value == HAWK_T('*')) goto normal_char;
					if (com->c.value == HAWK_T(')'))
					{
						hawk_gem_seterrnum(com->rgx->gem, HAWK_NULL, HAWK_EREXPAREN);
						return HAWK_NULL;
					}
				}
				if (com->rgx->option & HAWK_RGX_STRICT)
				{
					/* check if a special charcter is at the
					 * position that requires a normal character. */
					switch (com->c.value)
					{
						case HAWK_T('{'):
							/* { is a normal charcter when bound is disabled */
							if (com->rgx->option & HAWK_RGX_NOBOUND) break;

						case HAWK_T(')'):
						case HAWK_T('?'):
						case HAWK_T('*'):
						case HAWK_T('+'):
							/* it's at the wrong postion */
							hawk_gem_seterrbfmt(com->rgx->gem, HAWK_NULL, HAWK_EREXBADPAT, "special character at wrong position in regular expression");
							return HAWK_NULL;
					}
				}

				goto normal_char;
		}
	}
	else
	{
	normal_char:
		/* normal character */
		atom = newcharnode(com, com->c.value);
		if (HAWK_UNLIKELY(!atom)) return HAWK_NULL;
		if (getc_esc(com) <= -1) return HAWK_NULL;
	}

	atom->occ.min = 1;
	atom->occ.max = 1;

	/* BRE anchors cannot be repeated. Leave a following '*' to be
	 * parsed as a literal atom rather than creating an empty loop. */
	if (com->c.special && ((com->rgx->option & HAWK_RGX_EXTENDED) || !assertion))
	{
		/* handle the occurrence specifier, if any */
		if (comp_occ(com, atom) == HAWK_NULL) return HAWK_NULL;
	}

	return atom;
}

#if 0
static hawk_rgx_node_t* zero_or_more (comp_t* c, hawk_rgx_node_t* atom)
{
	hawk_rgx_node_t* b;

	b = newbranchnode(c, HAWK_NULL, atom);
	if (b == HAWK_NULL) return HAWK_NULL;

	atom->occ.min = 1;
	atom->occ.max = 1;
	atom->next = b;

	return b;
}

static hawk_rgx_node_t* one_or_more (comp_t* c, hawk_rgx_node_t* atom)
{
	hawk_rgx_node_t* b;

	b = newbranchnode(c, atom, HAWK_NULL);

	atom->occ.min = 1;
	atom->occ.max = 1;
	atom->next = b;

	TODO: return b as the tail....
	return atom;
}
#endif

static hawk_rgx_node_t* pseudo_group (comp_t* c, hawk_rgx_node_t* atom)
{
	hawk_rgx_node_t* g, *ge, * b;

	HAWK_ASSERT(atom->occ.min <= 0);

	g = newgroupnode(c);
	if (HAWK_UNLIKELY(!g)) return HAWK_NULL;

	ge = newgroupendnode(c, g);
	if (HAWK_UNLIKELY(!ge)) return HAWK_NULL;

	b = newbranchnode(c, atom, ge);
	if (HAWK_UNLIKELY(!b)) return HAWK_NULL;

	atom->occ.min = 1;
	atom->next = ge;
	HAWK_ASSERT(atom->occ.max >= atom->occ.min);

	g->occ.max = 1;
	g->occ.min = 1;
	g->u.g.end = ge;
	g->u.g.head = b;
	g->u.g.pseudo = 1;
	ge->u.ge.pseudo = 1;

	return g;
}

/* compile a list of atoms at the outermost level and/or
 * within a subgroup */
static hawk_rgx_node_t* comp_branch (comp_t* c, pair_t* pair)
{
#define REACHED_END(c) \
	(IS_EOF(c) || IS_OP(c,HAWK_T('|')) || \
	 (c->gdepth > 0 && IS_OP(c,HAWK_T(')'))))

	if (REACHED_END(c))
	{
		hawk_rgx_node_t* nop = newnopnode(c);
		if (nop == HAWK_NULL) return HAWK_NULL;
		nop->occ.min = 1; nop->occ.max = 1;
		pair->head = nop; pair->tail = nop;
	}
	else
	{
		pair->head = HAWK_NULL; pair->tail = HAWK_NULL;

		do
		{
			hawk_rgx_node_t* atom = comp_atom(c, pair->head == HAWK_NULL);
			if (atom == HAWK_NULL) return HAWK_NULL;

			/* An exact zero count consumes nothing. Do not wrap it with a
			 * pseudo group whose nonempty alternative requires min >= 1. */
			if (atom->occ.max == 0)
			{
				atom = newnopnode(c);
				if (!atom) return HAWK_NULL;
				atom->occ.min = atom->occ.max = 1;
			}

			if (atom->occ.min <= 0)
			{
			#if 0
				if (atom->occ.max >= OCC_MAX)
				{
					/*
					 * +-----------next--+
					 * v                 |
					 * BR --alter----> ORG(atom)
					 * |
					 * +----next------------------->
					 *
					 */
					atom = zero_or_more (c, atom);
				}
				else
				{
			#endif
					/*
					 * Given an atom, enclose it with a
					 * pseudogroup head and a psuedogroup
					 * tail. the head is followed by a
					 * branch that conntects to the tail
					 * and the atom given. The atom given
					 * gets connected to the tail.
					 *   Head -> BR        -> Tail
					 *        -> ORG(atom) -> Tail
					 */
					atom = pseudo_group (c, atom);
			#if 0
				}
			#endif
				if (atom == HAWK_NULL) return HAWK_NULL;
			}

			if (pair->tail == HAWK_NULL)
			{
				HAWK_ASSERT(pair->head == HAWK_NULL);
				pair->head = atom;
			}
			else pair->tail->next = atom;
			pair->tail = atom;
		}
		while (!REACHED_END(c));
	}

	return pair->head;
#undef REACHED_END
}

static hawk_rgx_node_t* comp_branches (comp_t* c, hawk_rgx_node_t* ge)
{
	hawk_rgx_node_t* left, * right, * tmp;
	pair_t xpair;

	left = comp_branch(c, &xpair);
	if (left == HAWK_NULL) return HAWK_NULL;
	xpair.tail->next = ge;

	while (IS_OP(c,HAWK_T('|')))
	{
		if (getc_esc(c) <= -1) return HAWK_NULL;

		right = comp_branch(c, &xpair);
		if (right == HAWK_NULL) return HAWK_NULL;

		xpair.tail->next = ge;

		tmp = newbranchnode(c, left, right);
		if (tmp == HAWK_NULL) return HAWK_NULL;

		left = tmp;
	}

	return left;
}

hawk_rgx_node_t* hawk_rgx_comp (hawk_rgx_t* rgx, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	comp_t c;
	hawk_rgx_node_t* end, * body;

	c.rgx = rgx;
	c.ptr = ptr;
	c.end = ptr + len;

	c.c.value = HAWK_OOCI_EOF;

	c.gdepth = 0;
	c.closed_groups = 0;
	c.start = HAWK_NULL;

	/* read the first character */
	if (getc_esc(&c) <= -1) return HAWK_NULL;

	c.start = newstartnode(&c);
	if (c.start == HAWK_NULL) return HAWK_NULL;

	end = newendnode(&c);
	if (end == HAWK_NULL)
	{
		freenode(c.start, c.rgx->gem);
		return HAWK_NULL;
	}

	body = comp_branches(&c, end);
	if (body == HAWK_NULL)
	{
		freeallnodes(c.start);
		return HAWK_NULL;
	}

	c.start->next = body;
	if (rgx->code != HAWK_NULL) freeallnodes(rgx->code);
	rgx->code = c.start;

	return rgx->code;
}

static void freegroupstackmembers (group_t* gs, hawk_gem_t* gem)
{
	while (gs != HAWK_NULL)
	{
		group_t* next = gs->next;
		hawk_gem_freemem(gem, gs);
		gs = next;
	}
}

static void freegroupstack(group_t* gs, hawk_gem_t* gem)
{
	HAWK_ASSERT(gs != HAWK_NULL);

	/* The head of a group stack must point to HAWK_NULL for management purpose */
	HAWK_ASSERT(gs->node == HAWK_NULL);

	freegroupstackmembers(gs, gem);
}

static void refupgroupstack(group_t* gs)
{
	if (gs != HAWK_NULL)
	{
		/* The head of a group stack must point to HAWK_NULL for management purpose */
		HAWK_ASSERT(gs->node == HAWK_NULL);
		gs->occ++;
	}
}

static void refdowngroupstack(group_t* gs, hawk_gem_t* gem)
{
	if (gs != HAWK_NULL)
	{
		HAWK_ASSERT(gs->node == HAWK_NULL);
		if (--gs->occ <= 0)
		{
			freegroupstack(gs, gem);
		}
	}
}

static group_t* newgrouphead (exec_t* e, const group_t* source)
{
	group_t* head;
	hawk_oow_t bytes;

	if (e->ncaps > (HAWK_TYPE_MAX(hawk_oow_t) - HAWK_SIZEOF(*head)) / HAWK_SIZEOF(hawk_oocs_t))
	{
		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return HAWK_NULL;
	}
	bytes = e->ncaps * HAWK_SIZEOF(hawk_oocs_t);

	head = (group_t*)hawk_gem_allocmem(e->rgx->gem, HAWK_SIZEOF(*head) + bytes);
	if (HAWK_UNLIKELY(!head)) return HAWK_NULL;

	head->node = HAWK_NULL;
	head->occ = 0;
	head->next = HAWK_NULL;
	head->revision = source? source->revision: 0;
	head->u.caps.count = e->ncaps;
	head->u.caps.ptr = (hawk_oocs_t*)(head + 1);
	if (source) HAWK_MEMCPY(head->u.caps.ptr, source->u.caps.ptr, bytes);
	else HAWK_MEMSET(head->u.caps.ptr, 0, bytes);

	return head;
}

static group_t* dupgroupstackmembers (exec_t* e, group_t* g)
{
	group_t* yg, * xg = HAWK_NULL;

	HAWK_ASSERT(g != HAWK_NULL);

	if (g->next != HAWK_NULL)
	{
		/* TODO: make it non recursive or
		 *       implement stack overflow protection */
		xg = dupgroupstackmembers(e, g->next);
		if (HAWK_UNLIKELY(!xg)) return HAWK_NULL;
	}

	yg = (group_t*)hawk_gem_allocmem(e->rgx->gem, HAWK_SIZEOF(*yg));
	if (HAWK_UNLIKELY(!yg))
	{
		if (xg) freegroupstackmembers(xg, e->rgx->gem);
		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return HAWK_NULL;
	}

	HAWK_MEMCPY(yg, g, HAWK_SIZEOF(*yg));
	yg->next = xg;

	return yg;
}

static group_t* dupgroupstack (exec_t* e, group_t* gs)
{
	group_t* head;

	HAWK_ASSERT(gs != HAWK_NULL);
	/*The head of a group stack must point to HAWK_NULL for management purpose.*/
	HAWK_ASSERT(gs->node == HAWK_NULL);

	head = newgrouphead(e, gs);
	if (!head) return HAWK_NULL;
	if (gs->next)
	{
		head->next = dupgroupstackmembers(e, gs->next);
		if (!head->next)
		{
			hawk_gem_freemem(e->rgx->gem, head);
			return HAWK_NULL;
		}
	}
	return head;
}

/* push 'gn' to the group stack 'gs'.
 * if dup is non-zero, the group stack is duplicated and 'gn' is pushed to
 * its top */
static group_t* __groupstackpush (exec_t* e, group_t* gs, hawk_rgx_node_t* gn, int dup)
{
	group_t* head, * elem;

	HAWK_ASSERT(gn->id == HAWK_RGX_NODE_GROUP);

	if (!gs)
	{
		/* gn is the first group pushed. no stack yet.
		 * create the head to store management info. */
		head = newgrouphead(e, HAWK_NULL);
		if (head == HAWK_NULL)
		{
			hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
			return HAWK_NULL;
		}

	}
	else
	{
		if (dup)
		{
			/* duplicate existing stack */
			head = dupgroupstack(e, gs);
			if (head == HAWK_NULL) return HAWK_NULL;
		}
		else
		{
			head = gs;
		}
	}

	/* create a new stack element */
	elem = (group_t*)hawk_gem_allocmem(e->rgx->gem, HAWK_SIZEOF(*elem));
	if (HAWK_UNLIKELY(!elem))
	{
		/* rollback */
		if (!gs)
			hawk_gem_freemem(e->rgx->gem, head);
		else if (dup)
			freegroupstack(head, e->rgx->gem);

		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return HAWK_NULL;
	}

	/* initialize the element */
	elem->node = gn;
	elem->occ = 0;
	elem->u.start = HAWK_NULL;
	elem->revision = head->revision;

	/* make it the top */
	elem->next = head->next;
	head->next = elem;

	return head;
}

#define dupgroupstackpush(e,gs,gn) __groupstackpush(e,gs,gn,1)
#define groupstackpush(e,gs,gn) __groupstackpush(e,gs,gn,0)

/* duplidate a group stack excluding the top data element */
static group_t* dupgroupstackpop (exec_t* e, group_t* gs)
{
	group_t* dupg, * head;

	HAWK_ASSERT(gs != HAWK_NULL);
	/* The head of a group stack must point to HAWK_NULL for management purpose. */
	HAWK_ASSERT(gs->node == HAWK_NULL);
	HAWK_ASSERT(gs->next != HAWK_NULL);

	dupg = HAWK_NULL;
	if (gs->next->next)
	{
		dupg = dupgroupstackmembers(e, gs->next->next);
		if (HAWK_UNLIKELY(!dupg)) return HAWK_NULL;
	}

	head = newgrouphead(e, gs);
	if (HAWK_UNLIKELY(!head))
	{
		if (dupg) freegroupstackmembers(dupg, e->rgx->gem);
		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return HAWK_NULL;
	}

	head->node = HAWK_NULL;
	head->occ = 0;
	head->next = dupg;

	return head;
}

static group_t* groupstackpop (exec_t* e, group_t* gs)
{
	group_t* top;

	HAWK_ASSERT(gs != HAWK_NULL);

	/* the head of a group stack must point to HAWK_NULL for management purpose. */
	HAWK_ASSERT(gs->node == HAWK_NULL);

	HAWK_ASSERT(gs->next != HAWK_NULL);

	top = gs->next;
	gs->next = top->next;

	hawk_gem_freemem(e->rgx->gem, top);
	return gs;
}

static int addsimplecand (exec_t* e, group_t* group, hawk_rgx_node_t* node, hawk_oow_t occ, const hawk_ooch_t* mptr)
{
	cand_t cand;

	HAWK_ASSERT(
		node->id == HAWK_RGX_NODE_NOP ||
		node->id == HAWK_RGX_NODE_BOL ||
		node->id == HAWK_RGX_NODE_EOL ||
		node->id == HAWK_RGX_NODE_ANY ||
		node->id == HAWK_RGX_NODE_CHAR ||
		node->id == HAWK_RGX_NODE_CSET ||
		node->id == HAWK_RGX_NODE_BACKREF
	);

	/* Once an unbounded atom's minimum is met, larger counts describe
	 * the same future choices. Saturate them so capture snapshots with
	 * equivalent state can still be merged. Bounded counts stay exact. */
	if (e->ncaps && node->occ.max == OCC_MAX && occ > node->occ.min) occ = node->occ.min;
	cand.node = node;
	cand.occ = occ;
	cand.group = group;
	cand.mptr = mptr;

	if (hawk_arr_search(&e->cand.set[e->cand.pending], 0, HAWK_ARR_SIZE(&e->cand.set[e->cand.pending]), &cand, 1) != HAWK_ARR_NIL)
	{
		/* exclude any existing entries in the array.
		 * see comp_cand() for the equality test used.
		 * note this linear search may be a performance bottle neck
		 * if the arrary grows large. not so sure if it should be
		 * switched to a different data structure such as a hash table.
		 * the problem is that most practical regular expressions
		 * won't have many candidates for a particular match point.
		 * so i'm a bit skeptical about data struct switching.
		 */
		return 0;
	}

	/* Queued capture states must be snapshots: addcands() may continue
	 * modifying the current path after publishing another candidate. */
	if (e->ncaps)
	{
		cand.group = dupgroupstack(e, group);
		if (!cand.group) return -1;
	}
	if (hawk_arr_insert(&e->cand.set[e->cand.pending], HAWK_ARR_SIZE(&e->cand.set[e->cand.pending]), &cand, 1) == HAWK_ARR_NIL)
	{
		if (e->ncaps) freegroupstack(cand.group, e->rgx->gem);
		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return -1;
	}

	/* the reference must be decremented by the freeer */
	refupgroupstack(cand.group);
	return 0;
}

/* Whole-match length takes precedence. On a tie, compare captures in
 * opening-parenthesis order: participation, length, then starting position.
 * This makes selection independent of candidate arrival order. */
static int bettercaptures (exec_t* e, const group_t* group)
{
	hawk_oow_t i;
	const hawk_oocs_t* caps = group->u.caps.ptr;

	for (i = 0; i < e->ncaps; i++)
	{
		if (!!caps[i].ptr != !!e->bestcaps[i].ptr) return caps[i].ptr != HAWK_NULL;
		if (!caps[i].ptr) continue;
		if (caps[i].len != e->bestcaps[i].len) return caps[i].len > e->bestcaps[i].len;
		if (caps[i].ptr != e->bestcaps[i].ptr) return caps[i].ptr < e->bestcaps[i].ptr;
	}
	return 0;
}

/* addcands() function add a candicate from candnode.
 * if candnode is not a simple node, it traverses further
 * until it reaches a simple node. prevnode is the last
 * GROUPEND node visited during traversal. If no GROUPEND
 * is visited yet, it can be any starting node */
static int addcands (
	exec_t* e, group_t* group, hawk_rgx_node_t* prevnode,
	hawk_rgx_node_t* candnode, const hawk_ooch_t* mptr)
{
	hawk_rgx_node_t* curcand = candnode;
	group_t* owned_group = HAWK_NULL;
	int result = 0;

warpback:

	/* skip all NOP nodes */
	while (curcand != HAWK_NULL && curcand->id == HAWK_RGX_NODE_NOP)
		curcand = curcand->next;

	/* nothing to add */
	if (curcand == HAWK_NULL) goto done;

	switch (curcand->id)
	{
		case HAWK_RGX_NODE_BOL:
		case HAWK_RGX_NODE_EOL:
			/* Assertions consume no input. Traverse them here so an
			 * anchor-only repeated group retains its empty-loop guard. */
			if (curcand->id == HAWK_RGX_NODE_BOL && (mptr != e->str.ptr || (e->flags & HAWK_RGX_NOTBOL))) break;
			if (curcand->id == HAWK_RGX_NODE_EOL && (mptr < e->str.end || (e->flags & HAWK_RGX_NOTEOL))) break;
			curcand = curcand->next;
			goto warpback;

		case HAWK_RGX_NODE_END:
		{
			if (e->matchend == HAWK_NULL || mptr > e->matchend ||
			    (e->ncaps && mptr == e->matchend && bettercaptures(e, group)))
			{
				e->matchend = mptr;
				if (e->ncaps) HAWK_MEMCPY(e->bestcaps, group->u.caps.ptr, e->ncaps * HAWK_SIZEOF(*e->bestcaps));
			}
			e->nmatches++;
			break;
		}

		case HAWK_RGX_NODE_BRANCH:
		{
			group_t* gx = group;
			int n;

			if (group != HAWK_NULL)
			{
				gx = dupgroupstack(e, group);
				if (gx == HAWK_NULL) goto oops;
			}

			refupgroupstack(gx);
			n = addcands(e, gx, prevnode, curcand->u.b.alter, mptr);
			refdowngroupstack(gx, e->rgx->gem);
			if (n <= -1) goto oops;

			curcand = curcand->next;
			goto warpback;
		}

		case HAWK_RGX_NODE_GROUP:
		{
			hawk_rgx_node_t* front;
			group_t* gx;

		#ifdef XTRA_DEBUG
			hawk_printf(HAWK_T("DEBUG: GROUP %p(pseudo=%d) PREV %p\n"),
				curcand, curcand->u.g.pseudo, prevnode);
		#endif
			if (curcand->u.g.pseudo)
			{
				curcand = curcand->u.g.head;
				goto warpback;
			}

			/* skip all NOP nodes */
			front = curcand->u.g.head;

			while (front->id == HAWK_RGX_NODE_NOP) front = front->next;

			if (front->id == HAWK_RGX_NODE_GROUPEND && !e->ncaps)
			{
				/* an empty group cannot affect a whole-match-only
				 * search. with capture tracking, process it normally
				 * so participation is recorded even at zero length. */
				curcand = curcand->next;
				goto warpback;
			}

			gx = groupstackpush(e, group, curcand);
			if (gx == HAWK_NULL) goto oops;

			/* add the first node in the group to
			 * the candidate array */
			/* a newly created stack needs a local owner until a candidate
			 * retains it. early branch/allocation failures may occur before
			 * any candidate is added to either cleanup array. */
			if (!group)
			{
				owned_group = gx;
				refupgroupstack(owned_group);
			}
			group = gx;
			if (e->ncaps) group->next->u.start = mptr;
			curcand = front;
			goto warpback;
		}

		case HAWK_RGX_NODE_GROUPEND:
		{
			int n;
			group_t* top;
			hawk_rgx_node_t* node;
			hawk_oow_t occ;

		#ifdef XTRA_DEBUG
			hawk_printf(HAWK_T("DEBUG: GROUPEND %p(pseudo=%d) PREV %p\n"),
				curcand, curcand->u.ge.pseudo, prevnode);
		#endif

			if (curcand->u.ge.pseudo)
			{
				curcand = curcand->u.ge.group->next;
				goto warpback;
			}

			/*GROUPEND must be paired up with GROUP */
			HAWK_ASSERT(group != HAWK_NULL && group->next != HAWK_NULL);

			top = group->next;
			node = top->node;

			/* the stack entry must identify this GROUPEND's GROUP. */
			HAWK_ASSERT(node == curcand->u.ge.group);

			if (prevnode == curcand && top->occ >= node->occ.min &&
			    (!e->ncaps || (top->revision == group->revision &&
			     group->u.caps.ptr[node->u.g.index].ptr == top->u.start &&
			     group->u.caps.ptr[node->u.g.index].len == (hawk_oow_t)(mptr - top->u.start))))
			{
				/* consider a pattern like (x*)*.
				 * when GROUPEND is reached, an 'if' block
				 * below tries to add the first node
				 * (node->u.g.head) in the group again.
				 * however, it('x') is optional, a possible
				 * path reach GROUPEND directly without
				 * adding a candidate. this check is needed to
				 * avoid the infinite loop, which otherwise is
				 * not avoidable. */

				/* repeating a nullable group may return here
				 * without consuming input. Allow enough empty
				 * iterations to satisfy its minimum, as required
				 * for patterns such as (a*){2}.
				 *
				 * once the minimum is satisfied, the preceding
				 * iteration has already explored the continuation.
				 * stop this empty path to prevent an infinite loop
				 * in patterns such as (a*)*. With captures, first allow
				 * an empty iteration that changes any captured value:
				 * (a?)+\\1 can consume "a", then capture an empty
				 * iteration and satisfy the empty backreference. */
				break;
			}

			top->occ++;

			/* unbounded group counts likewise need only distinguish
			 * whether the minimum has been reached. */
			if (e->ncaps && node->occ.max == OCC_MAX && top->occ > node->occ.min) top->occ = node->occ.min;

			occ = top->occ;
			if (e->ncaps)
			{
				hawk_oocs_t* capture = &group->u.caps.ptr[node->u.g.index];
				if (capture->ptr != top->u.start || capture->len != (hawk_oow_t)(mptr - top->u.start)) group->revision++;
				capture->ptr = (hawk_ooch_t*)top->u.start;
				capture->len = mptr - top->u.start;
			}

			if (occ >= node->occ.min)
			{
				group_t* gx;

				/* the lower bound has been met.
				 * for a pattern (abc){3,4}, 'abc' has been
				 * repeated 3 times. in this case, the next
				 * node can be added to the candiate array.
				 * it is actually a branch case. move on. */

				if (top->next == HAWK_NULL && !e->ncaps)
				{
					/* only one element in the stack.
					 * falls back to HAWK_NULL regardless
					 * of the need to reuse it */
					gx = HAWK_NULL;
				}
				else if (occ < node->occ.max)
				{
					/* check if the group will be repeated.
					 * if so, duplicate the group stack
					 * excluding the top. it goes along a
					 * different path and hence requires
					 * duplication. */

					gx = dupgroupstackpop(e, group);
					if (gx == HAWK_NULL) goto oops;
				}
				else
				{
					/* reuse the group stack. pop the top
					 * data element off the stack */

					gx = groupstackpop(e, group);

					/* this function always succeeds and
					 * returns the same head */
					HAWK_ASSERT(gx == group);
				}

				refupgroupstack(gx);

				if (prevnode != HAWK_NULL &&
				    prevnode->id == HAWK_RGX_NODE_GROUPEND)
					n = addcands(e, gx, prevnode, node->next, mptr);
				else
					n = addcands(e, gx, curcand, node->next, mptr);

				refdowngroupstack(gx, e->rgx->gem);
				if (n <= -1) goto oops;
			}

			if (occ < node->occ.max)
			{
				/* repeat itself. */
				if (e->ncaps)
				{
					group->next->u.start = mptr;
					group->next->revision = group->revision;
				}
				prevnode = curcand;
				curcand = node->u.g.head;
				goto warpback;
			}

			break;
		}

		case HAWK_RGX_NODE_BACKREF:
		{
			const hawk_oocs_t* capture = &group->u.caps.ptr[curcand->u.bref.index];

			if (!capture->ptr) break;
			if (capture->len == 0)
			{
				/* an empty backreference is an epsilon transition.
				 * any permitted number of repetitions has the same
				 * effect. keep prevnode so surrounding empty group
				 * loops still reach the GROUPEND cycle guard. */
				curcand = curcand->next;
				goto warpback;
			}
			goto add_simple;
		}

		default:
	add_simple:
		{
			int n;

			if (group) refupgroupstack(group);
			n = addsimplecand (e, group, curcand, 1, mptr);
			if (group) refdowngroupstack(group, e->rgx->gem);

			if (n <= -1) goto oops;
			break;
		}
	}

done:
	refdowngroupstack(owned_group, e->rgx->gem);
	return result;

oops:
	result = -1;
	goto done;
}

static int charset_matched (exec_t* e, hawk_rgx_node_t* node, hawk_ooch_t c)
{
	const hawk_ooch_t* ptr, * end;
	int matched = 0;

	HAWK_ASSERT(node->u.cset.member != HAWK_NULL);

	ptr = HAWK_OOECS_PTR (node->u.cset.member);
	end = ptr + HAWK_OOECS_LEN (node->u.cset.member);

	while (ptr < end && !matched)
	{
		switch (*ptr)
		{
			case HAWK_RGX_CSET_CHAR:
			{
				ptr++;

				if (e->rgx->option & HAWK_RGX_IGNORECASE)
				{
					if (hawk_to_ooch_upper((hawk_oochu_t)c) == hawk_to_ooch_upper((hawk_oochu_t)*ptr)) matched = !0;
				}
				else
				{
					if (c == *ptr) matched = !0;
				}
				break;
			}

			case HAWK_RGX_CSET_RANGE:
			{
				hawk_ooci_t c1, c2;

				if (e->rgx->option & HAWK_RGX_IGNORECASE)
				{
					hawk_ooci_t c3;
					ptr++; c1 = hawk_to_ooch_upper((hawk_oochu_t)*ptr);
					ptr++; c2 = hawk_to_ooch_upper((hawk_oochu_t)*ptr);
					c3 = hawk_to_ooch_upper((hawk_oochu_t)c);
					if (c3 >= c1 && c3 <= c2) matched = !0;
				}
				else
				{
					c1 = (hawk_oochu_t)*++ptr; c2 = (hawk_oochu_t)*++ptr;
					if ((hawk_oochu_t)c >= c1 && (hawk_oochu_t)c <= c2) matched = !0;
				}
				break;
			}

			case HAWK_RGX_CSET_CLASS:
			{
				hawk_oow_t c1;

				c1 = (hawk_oochu_t)*++ptr;
				HAWK_ASSERT(c1 < HAWK_COUNTOF(ccinfo));
				if (ccinfo[c1].func(e,c)) matched = !0;

				break;
			}

			default:
			{
				HAWK_ASSERT("must never happen - membership code for a character set must be one of HAWK_RGX_CSET_CHAR, HAWK_RGX_CSET_RANGE, HAWK_RGX_CSET_CLASS" == HAWK_NULL);
				/* return no match if this part is reached.
				 * however, something is totally wrong if it
				 * happens. */
				return 0;
			}
		}

		ptr++;
	}

	if (node->u.cset.negated) matched = !matched;
	return matched;
}

static hawk_arr_walk_t walk_cands_for_match (hawk_arr_t* arr, hawk_oow_t index, void* ctx)
{
	exec_t* e = (exec_t*)ctx;
	cand_t* cand = HAWK_ARR_DPTR(arr,index);
	hawk_rgx_node_t* node = cand->node;
	const hawk_ooch_t* nmptr = HAWK_NULL;

	switch (node->id)
	{
		case HAWK_RGX_NODE_BOL:
			if (cand->mptr == e->str.ptr && !(e->flags & HAWK_RGX_NOTBOL))
			{
				/* the next match pointer remains
				 * the same as ^ matches a position,
				 * not a character. */
				nmptr = cand->mptr;
			#ifdef XTRA_DEBUG
				hawk_printf(HAWK_T("DEBUG: matched <^>\n"));
			#endif
			}
			break;

		case HAWK_RGX_NODE_EOL:
			if (cand->mptr >= e->str.end && !(e->flags & HAWK_RGX_NOTEOL))
			{
				/* the next match pointer remains
				 * the same as $ matches a position,
				 * not a character. */
				nmptr = cand->mptr;
			#ifdef XTRA_DEBUG
				hawk_printf(HAWK_T("DEBUG: matched <$>\n"));
			#endif
			}
			break;

		case HAWK_RGX_NODE_ANY:
			if (cand->mptr < e->sub.end)
			{
				/* advance the match pointer to the
				 * next chracter.*/
				nmptr = cand->mptr + 1;
			#ifdef XTRA_DEBUG
				hawk_printf(HAWK_T("DEBUG: matched <.>\n"));
			#endif
			}
			break;

		case HAWK_RGX_NODE_CHAR:
		{
			if (cand->mptr < e->sub.end)
			{
				int equal;

				equal = (e->rgx->option & HAWK_RGX_IGNORECASE)?
					(hawk_to_ooch_upper((hawk_oochu_t)node->u.c) == hawk_to_ooch_upper((hawk_oochu_t)*cand->mptr)):
					(node->u.c == *cand->mptr) ;

				if (equal)
				{
					/* advance the match pointer to the
					 * next chracter.*/
					nmptr = cand->mptr + 1;
				}
			#ifdef XTRA_DEBUG
				hawk_printf(HAWK_T("DEBUG: matched %c\n"), node->u.c);
			#endif
			}
			break;
		}

		case HAWK_RGX_NODE_CSET:
		{
			if (cand->mptr < e->sub.end &&
			    charset_matched(e, node, *cand->mptr))
			{
				/* advance the match pointer
				 * to the next chracter.*/
				nmptr = cand->mptr + 1;
			}

			break;
		}

		case HAWK_RGX_NODE_BACKREF:
		{
			const hawk_oocs_t* capture = &cand->group->u.caps.ptr[node->u.bref.index];
			hawk_oow_t i;

			/* unmatched and empty captures are different. only a capture
			 * that actually participated may satisfy a backreference. */
			if (!capture->ptr || capture->len > (hawk_oow_t)(e->sub.end - cand->mptr)) break;
			for (i = 0; i < capture->len; i++)
			{
				if (e->rgx->option & HAWK_RGX_IGNORECASE)
				{
					if (hawk_to_ooch_upper((hawk_oochu_t)capture->ptr[i]) !=
					    hawk_to_ooch_upper((hawk_oochu_t)cand->mptr[i])) break;
				}
				else if (capture->ptr[i] != cand->mptr[i]) break;
			}
			if (i == capture->len) nmptr = cand->mptr + capture->len;
			break;
		}

		default:
		{
			HAWK_ASSERT("invalid simple regex node" == HAWK_NULL);
			break;
		}
	}

	if (nmptr != HAWK_NULL)
	{
		int n;

		if (cand->occ >= node->occ.min)
		{
			group_t* gx;

			if (cand->occ < node->occ.max && cand->group != HAWK_NULL)
			{
				gx = dupgroupstack(e, cand->group);
				if (gx == HAWK_NULL)
				{
					e->failed = 1;
					return HAWK_ARR_WALK_STOP;
				}
			}
			else gx = cand->group;

			/* move on to the next candidate */
			refupgroupstack(gx);
			n = addcands(e, gx, node, node->next, nmptr);
			refdowngroupstack(gx, e->rgx->gem);

			if (n <= -1)
			{
				e->failed = 1;
				return HAWK_ARR_WALK_STOP;
			}
		}

		if (cand->occ < node->occ.max)
		{
			/* repeat itself more */
			refupgroupstack(cand->group);
			n = addsimplecand (
				e, cand->group,
				node, cand->occ + 1, nmptr);
			refdowngroupstack(cand->group, e->rgx->gem);

			if (n <= -1)
			{
				e->failed = 1;
				return HAWK_ARR_WALK_STOP;
			}
		}
	}

	return HAWK_ARR_WALK_FORWARD;
}

static int exec (exec_t* e)
{
	int n;
	group_t* initial = HAWK_NULL;

	e->nmatches = 0;
	e->matchend = HAWK_NULL;

	e->cand.pending = 0;
	e->cand.active = 1;

	/* empty the pending set to collect the initial candidates */
	hawk_arr_clear(&e->cand.set[e->cand.pending]);

	/* the first node must be the START node */
	HAWK_ASSERT(e->rgx->code->id == HAWK_RGX_NODE_START);

	/* collect an initial set of candidates into the pending set */
	if (e->ncaps)
	{
		initial = newgrouphead(e, HAWK_NULL);
		if (!initial) return -1;
		refupgroupstack(initial);
	}
	n = addcands(
		e,                  /* execution structure */
		initial,            /* empty active group stack, possibly retaining captures */
		e->rgx->code,       /* dummy previous node, the start node */
		e->rgx->code->next, /* start from the second node */
		e->sub.ptr          /* current match pointer */
	);
	refdowngroupstack(initial, e->rgx->gem);
	if (n <= -1) return -1;

	do
	{
		hawk_oow_t ncands_active;

		/* swap the pending and active set indices.
		 * the pending set becomes active after which the match()
		 * function tries each candidate in it. New candidates
		 * are added into the pending set which will become active
		 * later when the loop reaches here again */
		int tmp = e->cand.pending;
		e->cand.pending = e->cand.active;
		e->cand.active = tmp;

		ncands_active = HAWK_ARR_SIZE(&e->cand.set[e->cand.active]);
		if (ncands_active <= 0)
		{
			/* we can't go on with no candidates in the
			 * active set. */
			break;
		}

		/* clear the pending set */
		hawk_arr_clear(&e->cand.set[e->cand.pending]);

#ifdef XTRA_DEBUG
		{
			int i;
			hawk_printf(HAWK_T("SET="));
			for (i = 0; i < ncands_active; i++)
			{
				cand_t* cand = HAWK_ARR_DPTR(&e->cand.set[e->cand.active],i);
				hawk_rgx_node_t* node = cand->node;

				if (node->id == HAWK_RGX_NODE_CHAR)
					hawk_printf(HAWK_T("%c "), node->u.c);
				else if (node->id == HAWK_RGX_NODE_ANY)
					hawk_printf(HAWK_T(". "), node->u.c);
				else if (node->id == HAWK_RGX_NODE_BOL)
					hawk_printf(HAWK_T("^ "));
				else if (node->id == HAWK_RGX_NODE_EOL)
					hawk_printf(HAWK_T("$ "));
			}
			hawk_printf(HAWK_T("\n"));
		}
#endif

		if (hawk_arr_walk (
			&e->cand.set[e->cand.active],
			walk_cands_for_match, e) != ncands_active || e->failed)
		{
			/* if the number of walks is different the number of
			 * candidates, traversal must have been aborted for
			 * an error. */
			return -1;
		}
	}
	while (1);

#ifdef XTRA_DEBUG
	if (e->nmatches > 0)
	{
		hawk_printf(HAWK_T("MATCH: %d [%.*s]\n"),
			(int)(e->matchend - e->sub.ptr),
			(int)(e->matchend - e->sub.ptr), e->sub.ptr);
	}
	hawk_printf(HAWK_T("TOTAL MATCHES FOUND... %d\n"), e->nmatches);
#endif

	return (e->nmatches > 0)? 1: 0;
}

static void refdowngroupstack_incand (hawk_arr_t* arr, void* dptr, hawk_oow_t dlen)
{
	HAWK_ASSERT(dlen == 1);
	refdowngroupstack(((cand_t*)dptr)->group, arr->gem);
}

/* addsimplecand() uses this callback through hawk_arr_search() to decide
 * whether a pending candidate already exists. Return 0 only when both
 * candidates describe the same matching state; return 1 otherwise.
 * This is an equality test for duplicate removal, not an ordering relation.
 *
 * A candidate includes both its current atom state and the repetition state
 * of all enclosing groups. Discarding a candidate merely because its atom
 * state matches another candidate can lose a valid longer match. For example,
 * (aa|a){2,3} must match all six characters in "aaaaaa", even when different
 * paths reach the same atom and input position with different group counts. */
static int comp_cand (hawk_arr_t* arr, const void* dptr1, hawk_oow_t dlen1, const void* dptr2, hawk_oow_t dlen2)
{
	cand_t* c1 = (cand_t*)dptr1;
	cand_t* c2 = (cand_t*)dptr2;

#if 0
/*hawk_printf(HAWK_T("%p(%d) %p(%d), %p %p, %d %d\n"), c1->node,c1->node->id, c2->node,c1->node->id, c1->mptr, c2->mptr, (int)c1->occ, (int)c2->occ);*/
	return (c1->node == c2->node &&
	        c1->mptr == c2->mptr &&
	        c1->occ == c2->occ)? 0: 1;
#else

	const group_t* g1;
	const group_t* g2;
	hawk_oow_t i, ncaps, revision1, revision2;

	/* first compare the current atom states:
	 * - node identifies the exact position in the compiled regex graph
	 * - mptr identifies the input position where this atom will be tested
	 * - occ identifies the atom repetition being attempted and affects
	 *   whether it may advance to the next node or repeat again
	 * any difference requires keeping the candidates separate, regardless
	 * of whether their enclosing group stacks happen to be equal */
	if (c1->node != c2->node || c1->mptr != c2->mptr || c1->occ != c2->occ) return 1;

	/* the atom states agree. compare the enclosing group state next.
	 * each non-null pointer initially addresses a stack management head */
	g1 = c1->group;
	g2 = c2->group;

	/* identical pointers mean the candidates share the very same stack,
	 * so its contents need not be compared. this also handles two null
	 * pointers: neither candidate has an enclosing group */
	if (g1 == g2) return 0;

	/* the pointers differ. if just one is null, one candidate has an
	 * enclosing group context that the other does not have */
	if (!g1 || !g2) return 1;

	/* both stacks exist, but may be separately allocated copies of the
	 * same state. skip their management heads before comparing contents.
	 * a head has node == HAWK_NULL and uses occ as an ownership reference
	 * count. that count does not affect matching and must not participate
	 * in the equality test. The following elements hold actual groups */
	HAWK_ASSERT(g1->node == HAWK_NULL);
	HAWK_ASSERT(g2->node == HAWK_NULL);
	ncaps = g1->u.caps.count;
	revision1 = g1->revision;
	revision2 = g2->revision;
	if (ncaps != g2->u.caps.count) return 1;
	/* Completed captures survive after their groups are popped. They
	 * affect both backreferences and the reported winning captures. */
	for (i = 0; i < ncaps; i++)
	{
		if (g1->u.caps.ptr[i].ptr != g2->u.caps.ptr[i].ptr ||
		    g1->u.caps.ptr[i].len != g2->u.caps.ptr[i].len) return 1;
	}
	g1 = g1->next;
	g2 = g2->next;

	/* walk from the innermost active group towards the outermost group.
	 * corresponding entries must identify the same compiled GROUP node
	 * and have completed the same number of repetitions. different counts
	 * can change whether a group may finish or must/can repeat, even when
	 * the current atom and input position are identical. compare contents
	 * rather than entry addresses because branching may copy the stack */
	while (g1 && g2)
	{
		if (g1->node != g2->node || g1->occ != g2->occ) return 1;
		if (ncaps && g1->u.start != g2->u.start) return 1;
		/* only whether a capture changed during this iteration matters;
		 * absolute serial numbers are bookkeeping, not matching state. */
		if (ncaps && (g1->revision == revision1) != (g2->revision == revision2)) return 1;
		g1 = g1->next;
		g2 = g2->next;
	}

	/* all compared entries agree. equality also requires both stacks to
	 * end together. at this point at least one pointer is null; matching
	 * pointers therefore mean both are null. a remaining entry on either
	 * side represents an extra enclosing group and makes the states differ */
	return (g1 == g2)? 0: 1;
#endif
}

static int init_exec_dds (exec_t* e, hawk_gem_t* gem)
{
	static const hawk_arr_style_t cand_style =
	{
		HAWK_ARR_COPIER_INLINE,
		refdowngroupstack_incand,
		comp_cand,
		HAWK_ARR_KEEPER_DEFAULT,
		HAWK_ARR_SIZER_DEFAULT
	};

	/* initializes dynamic data structures */
	if (hawk_arr_init(&e->cand.set[0], gem, 100) <= -1)
	{
		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		return -1;
	}
	if (hawk_arr_init(&e->cand.set[1], gem, 100) <= -1)
	{
		hawk_gem_seterrnum(e->rgx->gem, HAWK_NULL, HAWK_ENOMEM);
		hawk_arr_fini(&e->cand.set[0]);
		return -1;
	}

	hawk_arr_setscale(&e->cand.set[0], HAWK_SIZEOF(cand_t));
	hawk_arr_setscale(&e->cand.set[1], HAWK_SIZEOF(cand_t));

	hawk_arr_setstyle(&e->cand.set[0], &cand_style);
	hawk_arr_setstyle(&e->cand.set[1], &cand_style);

	return 0;
}

static void fini_exec_dds (exec_t* e)
{
	hawk_arr_fini(&e->cand.set[1]);
	hawk_arr_fini(&e->cand.set[0]);
}

int hawk_rgx_exec (
	hawk_rgx_t* rgx, const hawk_oocs_t* str,
	const hawk_oocs_t* substr, hawk_oocs_t* matstr, int flags)
{
	return hawk_rgx_execx(rgx, str, substr, matstr, matstr? 1: 0, flags);
}

hawk_oow_t hawk_rgx_getnsub (const hawk_rgx_t* rgx)
{
	return rgx->code? rgx->code->u.s.nsub: 0;
}

int hawk_rgx_execx (
	hawk_rgx_t* rgx, const hawk_oocs_t* str,
	const hawk_oocs_t* substr, hawk_oocs_t* matches, hawk_oow_t count, int flags)
{
	exec_t e;
	int n = 0;
	hawk_oow_t i;

	if ((count && !matches) || (flags & ~(HAWK_RGX_NOTBOL | HAWK_RGX_NOTEOL)))
	{
		hawk_gem_seterrnum(rgx->gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	if (rgx->code == HAWK_NULL)
	{
		hawk_gem_seterrnum(rgx->gem, HAWK_NULL, HAWK_EREXBADPAT);
		return -1;
	}

	HAWK_MEMSET(&e, 0, HAWK_SIZEOF(e));

	e.rgx = rgx;
	e.flags = flags;
	e.str.ptr = str->ptr;
	e.str.end = str->ptr + str->len;
	e.sub.ptr = substr->ptr;
	e.sub.end = substr->ptr + substr->len;

	if (init_exec_dds(&e, rgx->gem) <= -1) return -1;
	if (count > 1 || rgx->code->u.s.backrefs) e.ncaps = rgx->code->u.s.nsub;
	if (e.ncaps)
	{
		if (e.ncaps > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(*e.bestcaps))
		{
			hawk_gem_seterrnum(rgx->gem, HAWK_NULL, HAWK_ENOMEM);
			n = -1;
			goto done;
		}
		e.bestcaps = (hawk_oocs_t*)hawk_gem_allocmem(rgx->gem, e.ncaps * HAWK_SIZEOF(*e.bestcaps));
		if (HAWK_UNLIKELY(!e.bestcaps))
		{
			n = -1;
			goto done;
		}
	}

	while (e.sub.ptr <= e.sub.end)
	{
		n = exec(&e);
		if (n <= -1)
		{
			n = -1;
			break;
		}

		if (n >= 1)
		{
			HAWK_ASSERT(e.nmatches > 0);
			HAWK_ASSERT(e.matchend != HAWK_NULL);
			if (count)
			{
				matches[0].ptr = (hawk_ooch_t*)e.sub.ptr;
				matches[0].len = e.matchend - e.sub.ptr;
				for (i = 1; i < count; i++)
				{
					if (i - 1 < e.ncaps) matches[i] = e.bestcaps[i - 1];
					else
					{
						matches[i].ptr = HAWK_NULL;
						matches[i].len = 0;
					}
				}
			}
			break;
		}

		if (e.sub.ptr == e.sub.end) break;
		e.sub.ptr++;
	}

done:
	if (e.bestcaps) hawk_gem_freemem(rgx->gem, e.bestcaps);
	fini_exec_dds(&e);

	return n;
}

hawk_rgx_node_t* hawk_rgx_build (hawk_gem_t* gem, int option, const hawk_ooch_t* ptn, hawk_oow_t len)
{
	hawk_rgx_t rgx;
	hawk_rgx_node_t* code;

	hawk_rgx_init(&rgx, gem, HAWK_NULL);
	hawk_rgx_setopt(&rgx, option);
	if (!hawk_rgx_comp(&rgx, ptn, len))
	{
		hawk_rgx_fini(&rgx);
		return HAWK_NULL;
	}
	code = hawk_rgx_yield(&rgx);
	hawk_rgx_fini(&rgx);
	return code;
}

int hawk_rgx_match (hawk_gem_t* gem, hawk_rgx_node_t* code, int option,
	const hawk_oocs_t* str, const hawk_oocs_t* substr, hawk_oocs_t* match, int flags)
{
	return hawk_rgx_matchx(gem, code, option, str, substr, match, match? 1: 0, flags);
}

int hawk_rgx_matchx (hawk_gem_t* gem, hawk_rgx_node_t* code, int option,
	const hawk_oocs_t* str, const hawk_oocs_t* substr, hawk_oocs_t* matches, hawk_oow_t count, int flags)
{
	hawk_rgx_t rgx;
	int n;

	hawk_rgx_init(&rgx, gem, code);
	hawk_rgx_setopt(&rgx, option);
	n = hawk_rgx_execx(&rgx, str, substr, matches, count, flags);
	hawk_rgx_yield(&rgx);
	hawk_rgx_fini(&rgx);
	return n;
}

void hawk_rgx_freecode (hawk_rgx_node_t* code)
{
	if (code) freeallnodes(code);
}
