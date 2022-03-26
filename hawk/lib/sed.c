/*
 * $Id$
 *
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

#include "sed-prv.h"
#include "hawk-prv.h"
#include <hawk-chr.h>
#include <hawk-tre.h>

static void free_command (hawk_sed_t* sed, hawk_sed_cmd_t* cmd);
static void free_all_command_blocks (hawk_sed_t* sed);
static void free_all_cids (hawk_sed_t* sed);
static void free_appends (hawk_sed_t* sed);
static int emit_output (hawk_sed_t* sed, int skipline);

#define EMPTY_REX ((void*)1)

#define ERRNUM(sed) ((sed)->_gem.errnum)
#define CLRERR(sed) hawk_sed_seterrnum(sed, HAWK_NULL, HAWK_ENOERR)

#define ADJERR_LOC(sed,l) do { (sed)->_gem.errloc = *(l); } while (0)

#define SETERR0(sed,num,loc) \
do { hawk_sed_seterror (sed, num, HAWK_NULL, loc); } while (0)

#define SETERR1(sed,num,argp,argl,loc) \
do { \
	hawk_oocs_t __ea__; \
	__ea__.ptr = argp; __ea__.len = argl; \
	hawk_sed_seterror (sed, num, &__ea__, loc); \
} while (0)

static void free_all_cut_selector_blocks (hawk_sed_t* sed, hawk_sed_cmd_t* cmd);

hawk_sed_t* hawk_sed_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_errnum_t* errnum)
{
	hawk_sed_t* sed;

	sed = (hawk_sed_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(hawk_sed_t) + xtnsize);
	if (HAWK_LIKELY(sed))
	{
		if (hawk_sed_init(sed, mmgr) <= -1)
		{
			if (errnum) *errnum = hawk_sed_geterrnum(sed);
			HAWK_MMGR_FREE (mmgr, sed);
			sed = HAWK_NULL;
		}
		else HAWK_MEMSET (sed + 1, 0, xtnsize);
	}
	else if (errnum) *errnum = HAWK_ENOMEM;

	return sed;
}

void hawk_sed_close (hawk_sed_t* sed)
{
	hawk_sed_ecb_t* ecb;

	for (ecb = sed->ecb; ecb; ecb = ecb->next)
		if (ecb->close) ecb->close (sed);

	hawk_sed_fini (sed);
	HAWK_MMGR_FREE (hawk_sed_getmmgr(sed), sed);
}

int hawk_sed_init (hawk_sed_t* sed, hawk_mmgr_t* mmgr)
{
	HAWK_MEMSET (sed, 0, HAWK_SIZEOF(*sed));

	sed->_instsize = HAWK_SIZEOF(*sed);
	sed->_gem.mmgr = mmgr;
	sed->_gem.cmgr = HAWK_NULL; /* no cmgr used */

	/* initialize error handling fields */
	sed->_gem.errnum = HAWK_ENOERR;
	sed->_gem.errmsg[0] = '\0';
	sed->_gem.errloc.line = 0;
	sed->_gem.errloc.colm = 0;
	sed->_gem.errloc.file = HAWK_NULL;
	sed->_gem.errstr = hawk_dfl_errstr;

	if (hawk_ooecs_init(&sed->tmp.rex, hawk_sed_getgem(sed), 0) <= -1) goto oops_1;
	if (hawk_ooecs_init(&sed->tmp.lab, hawk_sed_getgem(sed), 0) <= -1) goto oops_2;

	if (hawk_map_init(&sed->tmp.labs, hawk_sed_getgem(sed), 128, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1) goto oops_3;
	hawk_map_setstyle (&sed->tmp.labs, hawk_get_map_style(HAWK_MAP_STYLE_INLINE_KEY_COPIER));

	/* init_append (sed); */
	if (hawk_ooecs_init(&sed->e.txt.hold, hawk_sed_getgem(sed), 256) <= -1) goto oops_6;
	if (hawk_ooecs_init(&sed->e.txt.scratch, hawk_sed_getgem(sed), 256) <= -1) goto oops_7;

	/* on init, the last points to the first */
	sed->cmd.lb = &sed->cmd.fb; 
	/* the block has no data yet */
	sed->cmd.fb.len = 0;

	/* initialize field buffers for cut */
	sed->e.cutf.cflds = HAWK_COUNTOF(sed->e.cutf.sflds);
	sed->e.cutf.flds = sed->e.cutf.sflds;

	return 0;

oops_7:
	hawk_ooecs_fini (&sed->e.txt.hold);
oops_6:
	hawk_map_fini (&sed->tmp.labs);
oops_3:
	hawk_ooecs_fini (&sed->tmp.lab);
oops_2:
	hawk_ooecs_fini (&sed->tmp.rex);
oops_1:
	return -1;
}

void hawk_sed_fini (hawk_sed_t* sed)
{
	free_all_command_blocks (sed);
	free_all_cids (sed);

	if (sed->e.cutf.flds != sed->e.cutf.sflds) 
		hawk_sed_freemem (sed, sed->e.cutf.flds);

	hawk_ooecs_fini (&sed->e.txt.scratch);
	hawk_ooecs_fini (&sed->e.txt.hold);
	free_appends (sed);

	hawk_map_fini (&sed->tmp.labs);
	hawk_ooecs_fini (&sed->tmp.lab);
	hawk_ooecs_fini (&sed->tmp.rex);
}

int hawk_sed_setopt (hawk_sed_t* sed, hawk_sed_opt_t id, const void* value)
{
	switch (id)
	{
		case HAWK_SED_TRAIT:
			sed->opt.trait = *(const int*)value;
			return 0;

		case HAWK_SED_TRACER:
			sed->opt.tracer = (hawk_sed_tracer_t)value;
			return 0;

		case HAWK_SED_LFORMATTER:
			sed->opt.lformatter = (hawk_sed_lformatter_t)value;
			return 0;

		case HAWK_SED_DEPTH_REX_BUILD:
			sed->opt.depth.rex.build = *(const hawk_oow_t*)value;
			return 0;

		case HAWK_SED_DEPTH_REX_MATCH:
			sed->opt.depth.rex.match = *(const hawk_oow_t*)value;
			return 0;
	}

	hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

int hawk_sed_getopt (hawk_sed_t* sed, hawk_sed_opt_t  id, void* value)
{
	switch  (id)
	{
		case HAWK_SED_TRAIT:
			*(int*)value = sed->opt.trait;
			return 0;

		case HAWK_SED_TRACER:
			*(hawk_sed_tracer_t*)value = sed->opt.tracer;
			return 0;

		case HAWK_SED_LFORMATTER:
			*(hawk_sed_lformatter_t*)value = sed->opt.lformatter;
			return 0;

		case HAWK_SED_DEPTH_REX_BUILD:
			*(hawk_oow_t*)value = sed->opt.depth.rex.build;
			return 0;

		case HAWK_SED_DEPTH_REX_MATCH:
			*(hawk_oow_t*)value = sed->opt.depth.rex.match;
			return 0;
	};

	hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

static void* build_rex (
	hawk_sed_t* sed, const hawk_oocs_t* str, 
	int ignorecase, const hawk_loc_t* loc)
{
	hawk_tre_t* tre;
	int opt = 0;

	tre = hawk_tre_open(hawk_sed_getgem(sed), 0);
	if (tre == HAWK_NULL)
	{
		ADJERR_LOC (sed, loc);
		return HAWK_NULL;
	}

	/* ignorecase is a compile option for TRE */
	if (ignorecase) opt |= HAWK_TRE_IGNORECASE; 
	if (sed->opt.trait & HAWK_SED_EXTENDEDREX) opt |= HAWK_TRE_EXTENDED;
	if (sed->opt.trait & HAWK_SED_NONSTDEXTREX) opt |= HAWK_TRE_NONSTDEXT;

	if (hawk_tre_compx(tre, str->ptr, str->len, HAWK_NULL, opt) <= -1)
	{
		hawk_tre_close (tre);
		return HAWK_NULL;
	}

	return tre;
}

static HAWK_INLINE void free_rex (hawk_sed_t* sed, void* rex)
{
	hawk_tre_close (rex);
}

static int matchtre (
	hawk_sed_t* sed, hawk_tre_t* tre, int opt, 
	const hawk_oocs_t* str, hawk_oocs_t* mat,
	hawk_oocs_t submat[9], const hawk_loc_t* loc)
{
	int n;
	/*hawk_tre_match_t match[10] = { { 0, 0 }, };*/
	hawk_tre_match_t match[10];
	HAWK_MEMSET (match, 0, HAWK_SIZEOF(match));

	n = hawk_tre_execx(tre, str->ptr, str->len, match, HAWK_COUNTOF(match), opt, HAWK_NULL);
	if (n <= -1)
	{
		/* chedk the error code stored in the gem area */
		if (hawk_sed_geterrnum(sed) == HAWK_EREXNOMAT) return 0;

		ADJERR_LOC (sed, loc);
		return -1;	
	}

	HAWK_ASSERT (match[0].rm_so != -1);
	if (mat)
	{
		mat->ptr = &str->ptr[match[0].rm_so];
		mat->len = match[0].rm_eo - match[0].rm_so;
	}

	if (submat)
	{
		int i;

		/* you must intialize submat before you pass into this 
		 * function because it can abort filling */
		for (i = 1; i < HAWK_COUNTOF(match); i++)
		{
			if (match[i].rm_so != -1) 
			{
				submat[i-1].ptr = &str->ptr[match[i].rm_so];
				submat[i-1].len = match[i].rm_eo - match[i].rm_so;
			}
		}
	}
	return 1;
}

/* check if c is a space character */
#define IS_SPACE(c) ((c) == HAWK_T(' ') || (c) == HAWK_T('\t') || (c) == HAWK_T('\r'))
#define IS_LINTERM(c) ((c) == HAWK_T('\n'))
#define IS_WSPACE(c) (IS_SPACE(c) || IS_LINTERM(c))

/* check if c is a command terminator excluding a space character */
#define IS_CMDTERM(c) \
	(c == HAWK_OOCI_EOF || c == HAWK_T('#') || \
	 c == HAWK_T(';') || IS_LINTERM(c) || \
	 c == HAWK_T('{') || c == HAWK_T('}'))
/* check if c can compose a label */
#define IS_LABCHAR(c) (!IS_CMDTERM(c) && !IS_WSPACE(c))

#define CURSC(sed) ((sed)->src.cc)
#define NXTSC(sed,c,errret) \
	do { if (getnextsc(sed,&(c)) <= -1) return (errret); } while (0)
#define NXTSC_GOTO(sed,c,label) \
	do { if (getnextsc(sed,&(c)) <= -1) goto label; } while (0)
#define PEEPNXTSC(sed,c,errret) \
	do { if (peepnextsc(sed,&(c)) <= -1) return (errret); } while (0)

static int open_script_stream (hawk_sed_t* sed)
{
	hawk_ooi_t n;

	CLRERR (sed);
	n = sed->src.fun (sed, HAWK_SED_IO_OPEN, &sed->src.arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		if (ERRNUM(sed) == HAWK_ENOERR)
			hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
		return -1;
	}

	sed->src.cur = sed->src.buf;
	sed->src.end = sed->src.buf;
	sed->src.cc  = HAWK_OOCI_EOF;
	sed->src.loc.line = 1;
	sed->src.loc.colm = 0;

	sed->src.eof = 0;
	return 0;
}

static int close_script_stream (hawk_sed_t* sed)
{
	hawk_ooi_t n;

	CLRERR (sed);
	n = sed->src.fun (sed, HAWK_SED_IO_CLOSE, &sed->src.arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		if (ERRNUM(sed) == HAWK_ENOERR)
			hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
		return -1;
	}

	return 0;
}

static int read_script_stream (hawk_sed_t* sed)
{
	hawk_ooi_t n;

	CLRERR (sed);
	n = sed->src.fun (
		sed, HAWK_SED_IO_READ, &sed->src.arg, 
		sed->src.buf, HAWK_COUNTOF(sed->src.buf)
	);
	if (n <= -1)
	{
		if (ERRNUM(sed) == HAWK_ENOERR)
			hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
		return -1; /* error */
	}

	if (n == 0)
	{
		/* don't change sed->src.cur and sed->src.end.
		 * they remain the same on eof  */
		sed->src.eof = 1;
		return 0; /* eof */
	}

	sed->src.cur = sed->src.buf;
	sed->src.end = sed->src.buf + n;
	return 1; /* read something */
}

static int getnextsc (hawk_sed_t* sed, hawk_ooci_t* c)
{
	/* adjust the line and column number of the next
	 * character based on the current character */
	if (sed->src.cc == HAWK_T('\n')) 
	{
		/* TODO: support different line end convension */
		sed->src.loc.line++;
		sed->src.loc.colm = 1;
	}
	else 
	{
		/* take note that if you keep on calling getnextsc()
		 * after HAWK_OOCI_EOF is read, this column number
		 * keeps increasing also. there should be a bug of
		 * reading more than necessary somewhere in the code
		 * if this happens. */
		sed->src.loc.colm++;
	}

	if (sed->src.cur >= sed->src.end && !sed->src.eof) 
	{
		/* read in more character if buffer is empty */
		if (read_script_stream (sed) <= -1) return -1;
	}

	sed->src.cc = 
		(sed->src.cur < sed->src.end)? 
		(*sed->src.cur++): HAWK_OOCI_EOF;

	*c = sed->src.cc;
	return 0;
}

static int peepnextsc (hawk_sed_t* sed, hawk_ooci_t* c)
{
	if (sed->src.cur >= sed->src.end && !sed->src.eof) 
	{
		/* read in more character if buffer is empty.
		 * it is ok to fill the buffer in the peeping
		 * function if it doesn't change sed->src.cc. */
		if (read_script_stream (sed) <= -1) return -1;
	}

	/* no changes in line nubmers, the 'cur' pointer, and
	 * most importantly 'cc' unlike getnextsc(). */
	*c = (sed->src.cur < sed->src.end)?  (*sed->src.cur): HAWK_OOCI_EOF;
	return 0;
}

static void free_address (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	if (cmd->a2.type == HAWK_SED_ADR_REX)
	{
		HAWK_ASSERT (cmd->a2.u.rex != HAWK_NULL);
		if (cmd->a2.u.rex != EMPTY_REX)
			free_rex (sed, cmd->a2.u.rex);
		cmd->a2.type = HAWK_SED_ADR_NONE;
	}
	if (cmd->a1.type == HAWK_SED_ADR_REX)
	{
		HAWK_ASSERT (cmd->a1.u.rex != HAWK_NULL);
		if (cmd->a1.u.rex != EMPTY_REX)
			free_rex (sed, cmd->a1.u.rex);
		cmd->a1.type = HAWK_SED_ADR_NONE;
	}
}

static int add_command_block (hawk_sed_t* sed)
{
	hawk_sed_cmd_blk_t* b;

	b = (hawk_sed_cmd_blk_t*) hawk_sed_callocmem (sed, HAWK_SIZEOF(*b));
	if (b == HAWK_NULL) return -1;

	b->next = HAWK_NULL;
	b->len = 0;

	sed->cmd.lb->next = b;
	sed->cmd.lb = b;

	return 0;
}

static void free_all_command_blocks (hawk_sed_t* sed)
{
	hawk_sed_cmd_blk_t* b;

	for (b = &sed->cmd.fb; b != HAWK_NULL; )
	{
		hawk_sed_cmd_blk_t* nxt = b->next;

		while (b->len > 0) free_command (sed, &b->buf[--b->len]);
		if (b != &sed->cmd.fb) hawk_sed_freemem (sed, b);

		b = nxt;	
	}

	HAWK_MEMSET (&sed->cmd.fb, 0, HAWK_SIZEOF(sed->cmd.fb));
	sed->cmd.lb = &sed->cmd.fb;
	sed->cmd.lb->len = 0;
	sed->cmd.lb->next = HAWK_NULL;
}

static void free_command (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	free_address (sed, cmd);

	switch (cmd->type)
	{
		case HAWK_SED_CMD_APPEND:
		case HAWK_SED_CMD_INSERT:
		case HAWK_SED_CMD_CHANGE:
			if (cmd->u.text.ptr)
				hawk_sed_freemem (sed, cmd->u.text.ptr);
			break;

		case HAWK_SED_CMD_READ_FILE:
		case HAWK_SED_CMD_READ_FILELN:
		case HAWK_SED_CMD_WRITE_FILE:
		case HAWK_SED_CMD_WRITE_FILELN:
			if (cmd->u.file.ptr)
				hawk_sed_freemem (sed, cmd->u.file.ptr);
			break;

		case HAWK_SED_CMD_BRANCH:
		case HAWK_SED_CMD_BRANCH_COND:
			if (cmd->u.branch.label.ptr)
				hawk_sed_freemem (sed, cmd->u.branch.label.ptr);
			break;
	
		case HAWK_SED_CMD_SUBSTITUTE:
			if (cmd->u.subst.file.ptr)
				hawk_sed_freemem (sed, cmd->u.subst.file.ptr);
			if (cmd->u.subst.rpl.ptr)
				hawk_sed_freemem (sed, cmd->u.subst.rpl.ptr);
			if (cmd->u.subst.rex && cmd->u.subst.rex != EMPTY_REX)
				free_rex (sed, cmd->u.subst.rex);
			break;

		case HAWK_SED_CMD_TRANSLATE:
			if (cmd->u.transet.ptr)
				hawk_sed_freemem (sed, cmd->u.transet.ptr);
			break;

		case HAWK_SED_CMD_CUT:
			free_all_cut_selector_blocks (sed, cmd);
			break;

		default: 
			break;
	}
}

static void free_all_cids (hawk_sed_t* sed)
{
	if (sed->src.cid == (hawk_sed_cid_t*)&sed->src.unknown_cid) 
		sed->src.cid = sed->src.cid->next;

	while (sed->src.cid)
	{
		hawk_sed_cid_t* next = sed->src.cid->next;
		hawk_sed_freemem (sed, sed->src.cid);
		sed->src.cid = next;
	}
}

static int trans_escaped (hawk_sed_t* sed, hawk_ooci_t c, hawk_ooci_t* ec, int* xamp)
{
	if (xamp) *xamp = 0;

	switch (c)
	{
		case HAWK_T('a'):
			c = HAWK_T('\a');
			break;
/*
Omitted for clash with regular expression \b.
		case HAWK_T('b'):
			c = HAWK_T('\b');
			break;
*/

		case HAWK_T('f'):
			c = HAWK_T('\f');
		case HAWK_T('n'):
			c = HAWK_T('\n');
			break;
		case HAWK_T('r'):
			c = HAWK_T('\r');
			break;
		case HAWK_T('t'):
			c = HAWK_T('\t');
			break;
		case HAWK_T('v'):
			c = HAWK_T('\v');
			break;

		case HAWK_T('x'):
		{
			/* \xnn */
			int cc;
			hawk_ooci_t peeped;

			PEEPNXTSC (sed, peeped, -1);
			cc = HAWK_XDIGIT_TO_NUM(peeped);
			if (cc <= -1) break;
			NXTSC (sed, peeped, -1); /* consume the character peeped */
			c = cc;

			PEEPNXTSC (sed, peeped, -1);
			cc = HAWK_XDIGIT_TO_NUM(peeped);
			if (cc <= -1) break;
			NXTSC (sed, peeped, -1); /* consume the character peeped */
			c = (c << 4) | cc;

			/* let's indicate that '&' is built from \x26. */
			if (xamp && c == HAWK_T('&')) *xamp = 1;
			break;
		}

#if defined(HAWK_OOCH_IS_UCH)
		case HAWK_T('X'):
		{
			/* \Xnnnn or \Xnnnnnnnn for wchar_t */
			int cc, i;
			hawk_ooci_t peeped;

			PEEPNXTSC (sed, peeped, -1);
			cc = HAWK_XDIGIT_TO_NUM(peeped);
			if (cc <= -1) break;
			NXTSC (sed, peeped, -1); /* consume the character peeped */
			c = cc;

			for (i = 1; i < HAWK_SIZEOF(hawk_ooch_t) * 2; i++)
			{
				PEEPNXTSC (sed, peeped, -1);
				cc = HAWK_XDIGIT_TO_NUM(peeped);
				if (cc <= -1) break;
				NXTSC (sed, peeped, -1); /* consume the character peeped */
				c = (c << 4) | cc;
			}

			/* let's indicate that '&' is built from \x26. */
			if (xamp && c == HAWK_T('&')) *xamp = 1;
			break;
		}
#endif
	}

	*ec = c;
	return 0;
}

static int pickup_rex (
	hawk_sed_t* sed, hawk_ooch_t rxend, 
	int replacement, const hawk_sed_cmd_t* cmd, hawk_ooecs_t* buf)
{
	/* 
	 * 'replacement' indicates that this functions is called for 
	 * 'replacement' in 's/pattern/replacement'.
	 */

	hawk_ooci_t c;
	hawk_oow_t chars_from_opening_bracket = 0;
	int bracket_state = 0;

	hawk_ooecs_clear (buf);

	while (1)
	{
		NXTSC (sed, c, -1);

	shortcut:
		if (c == HAWK_OOCI_EOF || IS_LINTERM(c))
		{
			if (cmd)
			{
				SETERR1 (
					sed, HAWK_SED_ECMDIC, 
					&cmd->type, 1,
					&sed->src.loc
				);
			}
			else
			{
				SETERR1 (
					sed, HAWK_SED_EREXIC, 
					HAWK_OOECS_PTR(buf), HAWK_OOECS_LEN(buf), 
					&sed->src.loc
				);
			}
			return -1;
		}

		if (c == rxend && bracket_state == 0) break;

		if (c == HAWK_T('\\'))
		{
			hawk_ooci_t nc;

			NXTSC (sed, nc, -1);
			if (nc == HAWK_OOCI_EOF /*|| IS_LINTERM(nc)*/)
			{
				if (cmd)
				{
					SETERR1 (
						sed, HAWK_SED_ECMDIC, 
						&cmd->type, 1,
						&sed->src.loc
					);
				}
				else
				{
					SETERR1 (
						sed, HAWK_SED_EREXIC,
						HAWK_OOECS_PTR(buf),
						HAWK_OOECS_LEN(buf),
						&sed->src.loc
					);
				}
				return -1;
			}

			if (bracket_state > 0 && nc == HAWK_T(']'))
			{
				/*
				 * if 'replacement' is not set, bracket_state is alyway 0.
				 * so this block is never reached. 
				 *
				 * a backslashed closing bracket is seen. 
				 * it is not :]. if bracket_state is 2, this \] 
				 * makes an illegal regular expression. but,
				 * let's not care.. just drop the state to 0
				 * as if the outer [ is closed.
				 */
				if (chars_from_opening_bracket > 1) bracket_state = 0;
			}
	
			if (nc == HAWK_T('\n')) c = nc;
			else
			{
				hawk_ooci_t ec;
				int xamp;

				if (trans_escaped (sed, nc, &ec, &xamp) <= -1) return -1;
				if (ec == nc || (xamp && replacement))
				{
					/* if the character after a backslash is not special 
					 * at the this layer, add the backslash into the 
					 * regular expression buffer as it is. 
					 *
					 * if \x26 is found in the replacement, i also need to 
					 * transform it to \& so that it is not treated as a 
					 * special &. 
					 */

					if (hawk_ooecs_ccat(buf, HAWK_T('\\')) == (hawk_oow_t)-1) return -1;
				}
				c = ec;
			}
		}
		else if (!replacement) 
		{
			/* this block sets a flag to indicate that we are in [] 
			 * of a regular expression. */

			if (c == HAWK_T('[')) 
			{
				if (bracket_state <= 0)
				{
					bracket_state = 1;
					chars_from_opening_bracket = 0;
				}
				else if (bracket_state == 1)
				{
					hawk_ooci_t nc;

					NXTSC (sed, nc, -1);
					if (nc == HAWK_T(':')) bracket_state = 2;

					if (hawk_ooecs_ccat(buf, c) == (hawk_oow_t)-1) return -1;

					chars_from_opening_bracket++;
					c = nc;
					goto shortcut;
				}
			}
			else if (c == HAWK_T(']'))
			{
				if (bracket_state == 1)
				{
					/* if it is the first character after [, 
					 * it is a normal character. */
					if (chars_from_opening_bracket > 1) bracket_state--;
				}
				else if (bracket_state == 2)
				{
					/* it doesn't really care if colon was for opening bracket 
					 * like in [[:]] */
					if (HAWK_OOECS_LASTCHAR(buf) == HAWK_T(':')) bracket_state--;
				}
			}
		}

		if (hawk_ooecs_ccat(buf, c) == (hawk_oow_t)-1) return -1;
		chars_from_opening_bracket++;
	} 

	return 0;
}

static HAWK_INLINE void* compile_rex_address (hawk_sed_t* sed, hawk_ooch_t rxend)
{
	int ignorecase = 0;
	hawk_ooci_t peeped;

	if (pickup_rex (sed, rxend, 0, HAWK_NULL, &sed->tmp.rex) <= -1)
		return HAWK_NULL;

	if (HAWK_OOECS_LEN(&sed->tmp.rex) <= 0) return EMPTY_REX;

	/* handle a modifer after having handled an empty regex.
	 * so a modifier is naturally disallowed for an empty regex. */
	PEEPNXTSC (sed, peeped, HAWK_NULL);
	if (peeped == HAWK_T('I')) 
	{
		ignorecase = 1;
		NXTSC (sed, peeped, HAWK_NULL); /* consume the character peeped */
	}

	return build_rex(sed, HAWK_OOECS_OOCS(&sed->tmp.rex), ignorecase, &sed->src.loc);
}

static hawk_sed_adr_t* get_address (hawk_sed_t* sed, hawk_sed_adr_t* a, int extended)
{
	hawk_ooci_t c;

	c = CURSC (sed);
	if (c == HAWK_T('$'))
	{
		a->type = HAWK_SED_ADR_DOL;
		NXTSC (sed, c, HAWK_NULL);
	}
	else if (c >= HAWK_T('0') && c <= HAWK_T('9'))
	{
		hawk_oow_t lno = 0;
		do
		{
			lno = lno * 10 + c - HAWK_T('0');
			NXTSC (sed, c, HAWK_NULL);
		}
		while (c >= HAWK_T('0') && c <= HAWK_T('9'));

		a->type = HAWK_SED_ADR_LINE;
		a->u.lno = lno;
	}
	else if (c == HAWK_T('/'))
	{
		/* /REGEX/ */
		a->u.rex = compile_rex_address (sed, c);
		if (a->u.rex == HAWK_NULL) return HAWK_NULL;
		a->type = HAWK_SED_ADR_REX;
		NXTSC (sed, c, HAWK_NULL);
	}
	else if (c == HAWK_T('\\'))
	{
		/* \cREGEXc */
		NXTSC (sed, c, HAWK_NULL);
		if (c == HAWK_OOCI_EOF || IS_LINTERM(c))
		{
			SETERR1 (sed, HAWK_SED_EREXIC, 
				HAWK_T(""), 0, &sed->src.loc);
			return HAWK_NULL;
		}

		a->u.rex = compile_rex_address (sed, c);
		if (a->u.rex == HAWK_NULL) return HAWK_NULL;
		a->type = HAWK_SED_ADR_REX;
		NXTSC (sed, c, HAWK_NULL);
	}
	else if (extended && (c == HAWK_T('+') || c == HAWK_T('~')))
	{
		hawk_oow_t lno = 0;

		a->type = (c == HAWK_T('+'))? HAWK_SED_ADR_RELLINE: HAWK_SED_ADR_RELLINEM;

		NXTSC (sed, c, HAWK_NULL);
		if (!(c >= HAWK_T('0') && c <= HAWK_T('9')))
		{
			SETERR0 (sed, HAWK_SED_EA2MOI, &sed->src.loc);
			return HAWK_NULL;
		}

		do
		{
			lno = lno * 10 + c - HAWK_T('0');
			NXTSC (sed, c, HAWK_NULL);
		}
		while (c >= HAWK_T('0') && c <= HAWK_T('9'));

		a->u.lno = lno;
	}
	else
	{
		a->type = HAWK_SED_ADR_NONE;
	}

	return a;
}


/* get the text for the 'a', 'i', and 'c' commands.
 * POSIX:
 *  The argument text shall consist of one or more lines. Each embedded 
 *  <newline> in the text shall be preceded by a backslash. Other backslashes
 *  in text shall be removed, and the following character shall be treated
 *  literally. */
static int get_text (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
#define ADD(sed,str,c,errlabel) \
do { \
	if (hawk_ooecs_ccat(str, c) == (hawk_oow_t)-1) \
	{ \
		goto errlabel; \
	} \
} while (0)

	hawk_ooci_t c;
	hawk_ooecs_t* t = HAWK_NULL;

	t = hawk_ooecs_open(hawk_sed_getgem(sed), 0, 128);
	if (t == HAWK_NULL) goto oops;

	c = CURSC (sed);

	do 
	{
		if (sed->opt.trait & HAWK_SED_STRIPLS)
		{
			/* get the first non-space character */
			while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);
		}

		while (c != HAWK_OOCI_EOF)
		{
			int nl = 0;

			if (c == HAWK_T('\\'))
			{
				NXTSC_GOTO (sed, c, oops);
				if (c == HAWK_OOCI_EOF) 
				{
					if (sed->opt.trait & HAWK_SED_KEEPTBS) 
						ADD (sed, t, HAWK_T('\\'), oops);
					break;
				}
			}
			else if (c == HAWK_T('\n')) nl = 1; /* unescaped newline */

			ADD (sed, t, c, oops);

			if (c == HAWK_T('\n'))
			{
				if (nl)
				{
					/* if newline is not escaped, stop */
					hawk_ooci_t dump;
					/* let's not pollute 'c' for ENSURELN check after done: */
					NXTSC_GOTO (sed, dump, oops);
					goto done;
				}

				/* else carry on reading the next line */
				NXTSC_GOTO (sed, c, oops);
				break;
			}

			NXTSC_GOTO (sed, c, oops);
		} 
	}
	while (c != HAWK_OOCI_EOF);

done:
	if ((sed->opt.trait & HAWK_SED_ENSURENL) && c != HAWK_T('\n'))
	{
		/* TODO: support different line end convension */
		ADD (sed, t, HAWK_T('\n'), oops);
	}

	hawk_ooecs_yield (t, &cmd->u.text, 0);
	hawk_ooecs_close (t);
	return 0;

oops:
	if (t) hawk_ooecs_close (t);
	return -1;

#undef ADD
}

static int get_label (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_ooci_t c;

	/* skip white spaces */
	c = CURSC (sed);
	while (IS_SPACE(c)) NXTSC (sed, c, -1);

	if (!IS_LABCHAR(c))
	{
		/* label name is empty */
		if (sed->opt.trait & HAWK_SED_STRICT)
		{
			SETERR0 (sed, HAWK_SED_ELABEM, &sed->src.loc);
			return -1;
		}

		/* empty label. noop command. don't register anything */
		hawk_ooecs_clear (&sed->tmp.lab);
	}
	else
	{
		hawk_ooecs_clear (&sed->tmp.lab);
		do
		{
			if (hawk_ooecs_ccat(&sed->tmp.lab, c) == (hawk_oow_t)-1) return -1;
			NXTSC (sed, c, -1);
		}
		while (IS_LABCHAR(c));

		if (hawk_map_search (
			&sed->tmp.labs, 
			HAWK_OOECS_PTR(&sed->tmp.lab),
			HAWK_OOECS_LEN(&sed->tmp.lab)) != HAWK_NULL)
		{
			SETERR1 (
				sed, HAWK_SED_ELABDU,
				HAWK_OOECS_PTR(&sed->tmp.lab),
				HAWK_OOECS_LEN(&sed->tmp.lab),
				&sed->src.loc
			);
			return -1;
		}

		if (hawk_map_insert (
			&sed->tmp.labs, 
			HAWK_OOECS_PTR(&sed->tmp.lab), HAWK_OOECS_LEN(&sed->tmp.lab),
			cmd, 0) == HAWK_NULL)
		{
			ADJERR_LOC (sed, &sed->src.loc);
			return -1;
		}

	}

	while (IS_SPACE(c)) NXTSC (sed, c, -1);

	if (IS_CMDTERM(c)) 
	{
		if (c != HAWK_T('}') && 
		    c != HAWK_T('#') &&
		    c != HAWK_OOCI_EOF) NXTSC (sed, c, -1);	
	}

	return 0;
}

static int terminate_command (hawk_sed_t* sed)
{
	hawk_ooci_t c;

	c = CURSC (sed);
	while (IS_SPACE(c)) NXTSC (sed, c, -1);
	if (!IS_CMDTERM(c))
	{
		SETERR0 (sed, HAWK_SED_ESCEXP, &sed->src.loc);
		return -1;
	}

	/* if the target is terminated by #, it should let the caller 
	 * to skip the comment text. so don't read in the next character.
	 * the same goes for brackets. */
	if (c != HAWK_T('#') && 
	    c != HAWK_T('{') &&
	    c != HAWK_T('}') && 
	    c != HAWK_OOCI_EOF) NXTSC (sed, c, -1);	
	return 0;
}

static int get_branch_target (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_ooci_t c;
	hawk_ooecs_t* t = HAWK_NULL;
	hawk_map_pair_t* pair;

	/* skip white spaces */
	c = CURSC(sed);
	while (IS_SPACE(c)) NXTSC (sed, c, -1);

	if (IS_CMDTERM(c))
	{
		/* no branch target is given -
		 * a branch command without a target should cause 
		 * sed to jump to the end of a script.
		 */
		cmd->u.branch.label.ptr = HAWK_NULL;
		cmd->u.branch.label.len = 0;
		cmd->u.branch.target = HAWK_NULL;
		return terminate_command (sed);
	}

	t = hawk_ooecs_open(hawk_sed_getgem(sed), 0, 32);
	if (t == HAWK_NULL) goto oops;

	while (IS_LABCHAR(c))
	{
		if (hawk_ooecs_ccat(t, c) == (hawk_oow_t)-1) goto oops;
		NXTSC_GOTO (sed, c, oops);
	}

	if (terminate_command (sed) <= -1) goto oops;

	pair = hawk_map_search (&sed->tmp.labs, HAWK_OOECS_PTR(t), HAWK_OOECS_LEN(t));
	if (pair == HAWK_NULL)
	{
		/* label not resolved yet */
		hawk_ooecs_yield (t, &cmd->u.branch.label, 0);
		cmd->u.branch.target = HAWK_NULL;
	}
	else
	{
		cmd->u.branch.label.ptr = HAWK_NULL;
		cmd->u.branch.label.len = 0;
		cmd->u.branch.target = HAWK_MAP_VPTR(pair);
	}
	
	hawk_ooecs_close (t);
	return 0;

oops:
	if (t) hawk_ooecs_close (t);
	return -1;
}

static int get_file (hawk_sed_t* sed, hawk_oocs_t* xstr)
{
	hawk_ooci_t c;
	hawk_ooecs_t* t = HAWK_NULL;
	hawk_oow_t trailing_spaces = 0;

	/* skip white spaces */
	c = CURSC(sed);
	while (IS_SPACE(c)) NXTSC (sed, c, -1);

	if (IS_CMDTERM(c))
	{
		SETERR0 (sed, HAWK_SED_EFILEM, &sed->src.loc);
		goto oops;	
	}

	t = hawk_ooecs_open(hawk_sed_getgem(sed), 0, 32);
	if (t == HAWK_NULL) goto oops;

	do
	{
		if (c == HAWK_T('\0'))
		{
			/* the file name should not contain '\0' */
			SETERR0 (sed, HAWK_SED_EFILIL, &sed->src.loc);
			goto oops;
		}

		if (IS_SPACE(c)) trailing_spaces++;
		else trailing_spaces = 0;

		if (c == HAWK_T('\\'))
		{
			NXTSC_GOTO (sed, c, oops);
			if (c == HAWK_T('\0') || c == HAWK_OOCI_EOF || IS_LINTERM(c))
			{
				SETERR0 (sed, HAWK_SED_EFILIL, &sed->src.loc);
				goto oops;
			}

			if (c == HAWK_T('n')) c = HAWK_T('\n');
		}

		if (hawk_ooecs_ccat(t, c) == (hawk_oow_t)-1) 
		{
			ADJERR_LOC (sed, &sed->src.loc);
			goto oops;
		} 

		NXTSC_GOTO (sed, c, oops);
	}
	while (!IS_CMDTERM(c));

	if (terminate_command(sed) <= -1) goto oops;

	if (trailing_spaces > 0)
	{
		hawk_ooecs_setlen (t, HAWK_OOECS_LEN(t) - trailing_spaces);
	}

	hawk_ooecs_yield (t, xstr, 0);
	hawk_ooecs_close (t);
	return 0;

oops:
	if (t) hawk_ooecs_close (t);
	return -1;
}

#define CHECK_CMDIC(sed,cmd,c,action) \
do { \
	if (c == HAWK_OOCI_EOF || IS_LINTERM(c)) \
	{ \
		SETERR1 (sed, HAWK_SED_ECMDIC, \
			&cmd->type, 1, &sed->src.loc); \
		action; \
	} \
} while (0)

#define CHECK_CMDIC_ESCAPED(sed,cmd,c,action) \
do { \
	if (c == HAWK_OOCI_EOF) \
	{ \
		SETERR1 (sed, HAWK_SED_ECMDIC, \
			&cmd->type, 1, &sed->src.loc); \
		action; \
	} \
} while (0)

static int get_subst (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_ooci_t c, delim;

	/*hawk_ooecs_t* t[2] = { HAWK_NULL, HAWK_NULL };*/
	hawk_ooecs_t* t[2];
	t[0] = HAWK_NULL;
	t[1] = HAWK_NULL;

	c = CURSC (sed);
	CHECK_CMDIC (sed, cmd, c, goto oops);

	delim = c;	
	if (delim == HAWK_T('\\'))
	{
		/* backspace is an illegal delimiter */
		SETERR0 (sed, HAWK_SED_EBSDEL, &sed->src.loc);
		goto oops;
	}

	t[0] = &sed->tmp.rex;
	hawk_ooecs_clear (t[0]);

	t[1] = hawk_ooecs_open(hawk_sed_getgem(sed), 0, 32);
	if (t[1] == HAWK_NULL) goto oops;

	if (pickup_rex(sed, delim, 0, cmd, t[0]) <= -1) goto oops;
	if (pickup_rex(sed, delim, 1, cmd, t[1]) <= -1) goto oops;

	/* skip spaces before options */
	do { NXTSC_GOTO (sed, c, oops); } while (IS_SPACE(c));

	/* get options */
	do
	{
		if (c == HAWK_T('p')) 
		{
			cmd->u.subst.p = 1;
			NXTSC_GOTO (sed, c, oops);
		}
		else if (c == HAWK_T('i') || c == HAWK_T('I')) 
		{
			cmd->u.subst.i = 1;
			NXTSC_GOTO (sed, c, oops);
		}
		else if (c == HAWK_T('g')) 
		{
			cmd->u.subst.g = 1;
			NXTSC_GOTO (sed, c, oops);
		}
		else if (c == HAWK_T('k')) 
		{
			cmd->u.subst.k = 1;
			NXTSC_GOTO (sed, c, oops);
		}
		else if (c >= HAWK_T('0') && c <= HAWK_T('9'))
		{
			unsigned long occ;

			if (cmd->u.subst.occ != 0)
			{
				SETERR0 (sed, HAWK_SED_EOCSDU, &sed->src.loc);
				goto oops;
			}

			occ = 0;

			do 
			{
				occ = occ * 10 + (c - HAWK_T('0')); 
				if (occ > HAWK_TYPE_MAX(unsigned short))
				{
					SETERR0 (sed, HAWK_SED_EOCSTL, &sed->src.loc);
					goto oops;
				}
				NXTSC_GOTO (sed, c, oops);
			}
			while (c >= HAWK_T('0') && c <= HAWK_T('9'));

			if (occ == 0)
			{
				SETERR0 (sed, HAWK_SED_EOCSZE, &sed->src.loc);
				goto oops;
			}

			cmd->u.subst.occ = occ;
		}
		else if (c == HAWK_T('w'))
		{
			NXTSC_GOTO (sed, c, oops);
			if (get_file (sed, &cmd->u.subst.file) <= -1) goto oops;
			break;
		}
		else break;
	}
	while (1);

	/* call terminate_command() if the 'w' option is not specified.
	 * if the 'w' option is given, it is called in get_file(). */
	if (cmd->u.subst.file.ptr == HAWK_NULL &&
	    terminate_command (sed) <= -1) goto oops;

	HAWK_ASSERT (cmd->u.subst.rex == HAWK_NULL);

	if (HAWK_OOECS_LEN(t[0]) <= 0) cmd->u.subst.rex = EMPTY_REX;
	else
	{
		cmd->u.subst.rex = build_rex(sed, HAWK_OOECS_OOCS(t[0]), cmd->u.subst.i, &sed->src.loc);
		if (cmd->u.subst.rex == HAWK_NULL) goto oops;
	}

	hawk_ooecs_yield (t[1], &cmd->u.subst.rpl, 0);
	if (cmd->u.subst.g == 0 && cmd->u.subst.occ == 0) cmd->u.subst.occ = 1;

	hawk_ooecs_close (t[1]);
	return 0;

oops:
	if (t[1]) hawk_ooecs_close (t[1]);
	return -1;
}

static int get_transet (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_ooci_t c, delim;
	hawk_ooecs_t* t = HAWK_NULL;
	hawk_oow_t pos;

	c = CURSC (sed);
	CHECK_CMDIC (sed, cmd, c, goto oops);

	delim = c;	
	if (delim == HAWK_T('\\'))
	{
		/* backspace is an illegal delimiter */
		SETERR0 (sed, HAWK_SED_EBSDEL, &sed->src.loc);
		goto oops;
	}

	t = hawk_ooecs_open(hawk_sed_getgem(sed), 0, 32);
	if (t == HAWK_NULL) goto oops;

	NXTSC_GOTO (sed, c, oops);
	while (c != delim)
	{
		hawk_ooch_t b[2];

		CHECK_CMDIC (sed, cmd, c, goto oops);

		if (c == HAWK_T('\\'))
		{
			NXTSC_GOTO (sed, c, oops);
			CHECK_CMDIC_ESCAPED (sed, cmd, c, goto oops);
			if (trans_escaped (sed, c, &c, HAWK_NULL) <= -1) goto oops;
		}

		b[0] = c;
		if (hawk_ooecs_ncat(t, b, 2) == (hawk_oow_t)-1) goto oops;

		NXTSC_GOTO (sed, c, oops);
	}	

	NXTSC_GOTO (sed, c, oops);
	for (pos = 1; c != delim; pos += 2)
	{
		CHECK_CMDIC (sed, cmd, c, goto oops);

		if (c == HAWK_T('\\'))
		{
			NXTSC_GOTO (sed, c, oops);
			CHECK_CMDIC_ESCAPED (sed, cmd, c, goto oops);
			if (trans_escaped (sed, c, &c, HAWK_NULL) <= -1) goto oops;
		}

		if (pos >= HAWK_OOECS_LEN(t))
		{
			/* source and target not the same length */
			SETERR0 (sed, HAWK_SED_ETSNSL, &sed->src.loc);
			goto oops;
		}

		HAWK_OOECS_CHAR(t,pos) = c;
		NXTSC_GOTO (sed, c, oops);
	}

	if (pos < HAWK_OOECS_LEN(t))
	{
		/* source and target not the same length */
		SETERR0 (sed, HAWK_SED_ETSNSL, &sed->src.loc);
		goto oops;
	}

	NXTSC_GOTO (sed, c, oops);
	if (terminate_command (sed) <= -1) goto oops;

	hawk_ooecs_yield (t, &cmd->u.transet, 0);
	hawk_ooecs_close (t);
	return 0;

oops:
	if (t) hawk_ooecs_close (t);
	return -1;
}

static int add_cut_selector_block (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_sed_cut_sel_t* b;

	b = (hawk_sed_cut_sel_t*) hawk_sed_callocmem (sed, HAWK_SIZEOF(*b));
	if (b == HAWK_NULL) return -1;

	b->next = HAWK_NULL;
	b->len = 0;

	if (cmd->u.cut.fb == HAWK_NULL) 
	{
		cmd->u.cut.fb = b;
		cmd->u.cut.lb = b;
	}
	else
	{
		cmd->u.cut.lb->next = b;
		cmd->u.cut.lb = b;
	}

	return 0;
}

static void free_all_cut_selector_blocks (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_sed_cut_sel_t* b, * next;

	for (b = cmd->u.cut.fb; b; b = next)
	{
		next = b->next;
		hawk_sed_freemem (sed, b);
	}

	cmd->u.cut.lb = HAWK_NULL;
	cmd->u.cut.fb = HAWK_NULL;

	cmd->u.cut.count = 0;
	cmd->u.cut.fcount = 0;
	cmd->u.cut.ccount = 0;
}

static int get_cut (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_ooci_t c, delim;
	hawk_oow_t i;
	int sel = HAWK_SED_CUT_SEL_CHAR;

	c = CURSC (sed);
	CHECK_CMDIC (sed, cmd, c, goto oops);

	delim = c;	
	if (delim == HAWK_T('\\'))
	{
		/* backspace is an illegal delimiter */
		SETERR0 (sed, HAWK_SED_EBSDEL, &sed->src.loc);
		goto oops;
	}

	/* initialize the delimeter to a space letter */
	for (i = 0; i < HAWK_COUNTOF(cmd->u.cut.delim); i++)
		cmd->u.cut.delim[i] = HAWK_T(' ');

	NXTSC_GOTO (sed, c, oops);
	while (1)
	{
		hawk_oow_t start = 0, end = 0;

#define MASK_START (1 << 1)
#define MASK_END (1 << 2)
#define MAX HAWK_TYPE_MAX(hawk_oow_t)
		int mask = 0;

		while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);
		if (c == HAWK_OOCI_EOF)
		{
			SETERR0 (sed, HAWK_SED_ECSLNV, &sed->src.loc);
			goto oops;
		}

		if (c == HAWK_T('d') || c == HAWK_T('D'))
		{
			int delim_idx = (c == HAWK_T('d'))? 0: 1;
			/* the next character is an input/output delimiter. */
			NXTSC_GOTO (sed, c, oops);
			if (c == HAWK_OOCI_EOF)
			{
				SETERR0 (sed, HAWK_SED_ECSLNV, &sed->src.loc);
				goto oops;
			}
			cmd->u.cut.delim[delim_idx] = c;
			NXTSC_GOTO (sed, c, oops);
		}
		else
		{
			if (c == HAWK_T('c') || c == HAWK_T('f'))
			{
				sel = c;
				NXTSC_GOTO (sed, c, oops);
				while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);
			}

			if (hawk_is_ooch_digit(c))
			{
				do 
				{ 
					start = start * 10 + (c - HAWK_T('0')); 
					NXTSC_GOTO (sed, c, oops);
				} 
				while (hawk_is_ooch_digit(c));
	
				while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);
				mask |= MASK_START;

				if (start >= 1) start--; /* convert it to index */
			}
			else start = 0;

			if (c == HAWK_T('-'))
			{
				NXTSC_GOTO (sed, c, oops);
				while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);

				if (hawk_is_ooch_digit(c))
				{
					do 
					{ 
						end = end * 10 + (c - HAWK_T('0')); 
						NXTSC_GOTO (sed, c, oops);
					} 
					while (hawk_is_ooch_digit(c));
					mask |= MASK_END;
				}
				else end = MAX;

				while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);

				if (end >= 1) end--; /* convert it to index */
			}
			else end = start;

			if (!(mask & (MASK_START | MASK_END)))
			{
				SETERR0 (sed, HAWK_SED_ECSLNV, &sed->src.loc);
				goto oops;
			}

			if (cmd->u.cut.lb == HAWK_NULL ||
			    cmd->u.cut.lb->len >= HAWK_COUNTOF(cmd->u.cut.lb->range))
			{
				if (add_cut_selector_block (sed, cmd) <= -1) goto oops;
			}

			cmd->u.cut.lb->range[cmd->u.cut.lb->len].id = sel;
			cmd->u.cut.lb->range[cmd->u.cut.lb->len].start = start;
			cmd->u.cut.lb->range[cmd->u.cut.lb->len].end = end;
			cmd->u.cut.lb->len++;

			cmd->u.cut.count++;
			if (sel == HAWK_SED_CUT_SEL_FIELD) cmd->u.cut.fcount++;
			else cmd->u.cut.ccount++;
		}

		while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);

		if (c == HAWK_OOCI_EOF)
		{
			SETERR0 (sed, HAWK_SED_ECSLNV, &sed->src.loc);
			goto oops;
		}

		if (c == delim) break;

		if (c != HAWK_T(',')) 
		{
			SETERR0 (sed, HAWK_SED_ECSLNV, &sed->src.loc);
			goto oops;
		}
		NXTSC_GOTO (sed, c, oops); /* skip a comma */
	}

	/* skip spaces before options */
	do { NXTSC_GOTO (sed, c, oops); } while (IS_SPACE(c));

	/* get options */
	do
	{
		if (c == HAWK_T('f')) 
		{
			cmd->u.cut.f = 1;
		}
		else if (c == HAWK_T('w')) 
		{
			cmd->u.cut.w = 1;
		}
		else if (c == HAWK_T('d'))
		{
			cmd->u.cut.d = 1;
		}
		else break;

		NXTSC_GOTO (sed, c, oops);
	}
	while (1);

	if (terminate_command (sed) <= -1) goto oops;
	return 0;

oops:
	free_all_cut_selector_blocks (sed, cmd);
	return -1;
}

/* process a command code and following parts into cmd */
static int get_command (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_ooci_t c;

	c = CURSC (sed);
	cmd->lid = sed->src.cid? ((const hawk_ooch_t*)(sed->src.cid + 1)): HAWK_NULL;
	cmd->loc = sed->src.loc;
	switch (c)
	{
		default:
		{
			hawk_ooch_t cc = c;
			SETERR1 (sed, HAWK_SED_ECMDNR, &cc, 1, &sed->src.loc);
			return -1;
		}

		case HAWK_OOCI_EOF:
		case HAWK_T('\n'):
			SETERR0 (sed, HAWK_SED_ECMDMS, &sed->src.loc);
			return -1;	

		case HAWK_T(':'):
			if (cmd->a1.type != HAWK_SED_ADR_NONE)
			{
				/* label cannot have an address */
				SETERR1 (
					sed, HAWK_SED_EA1PHB,
					&cmd->type, 1, &sed->src.loc
				);
				return -1;
			}

			cmd->type = HAWK_SED_CMD_NOOP;

			NXTSC (sed, c, -1);
			if (get_label (sed, cmd) <= -1) return -1;

			c = CURSC (sed);
			while (IS_SPACE(c)) NXTSC (sed, c, -1);
			break;

		case HAWK_T('{'):
			/* insert a negated branch command at the beginning 
			 * of a group. this way, all the commands in a group
			 * can be skipped. the branch target is set once a
			 * corresponding } is met. */
			cmd->type = HAWK_SED_CMD_BRANCH;
			cmd->negated = !cmd->negated;

			if (sed->tmp.grp.level >= HAWK_COUNTOF(sed->tmp.grp.cmd))
			{
				/* group nesting too deep */
				SETERR0 (sed, HAWK_SED_EGRNTD, &sed->src.loc);
				return -1;
			}

			sed->tmp.grp.cmd[sed->tmp.grp.level++] = cmd;
			NXTSC (sed, c, -1);
			break;

		case HAWK_T('}'):
		{
			hawk_sed_cmd_t* tc;

			if (cmd->a1.type != HAWK_SED_ADR_NONE)
			{
				hawk_ooch_t tmpc = c;
				SETERR1 (
					sed, HAWK_SED_EA1PHB,
					&tmpc, 1, &sed->src.loc
				);
				return -1;
			}

			cmd->type = HAWK_SED_CMD_NOOP;

			if (sed->tmp.grp.level <= 0) 
			{
				/* group not balanced */
				SETERR0 (sed, HAWK_SED_EGRNBA, &sed->src.loc);
				return -1;
			}

			tc = sed->tmp.grp.cmd[--sed->tmp.grp.level];
			tc->u.branch.target = cmd;

			NXTSC (sed, c, -1);
			break;
		}

		case HAWK_T('q'):
		case HAWK_T('Q'):
			cmd->type = c;
			if (sed->opt.trait & HAWK_SED_STRICT &&
			    cmd->a2.type != HAWK_SED_ADR_NONE)
			{
				SETERR1 (
					sed, HAWK_SED_EA2PHB,
					&cmd->type, 1, &sed->src.loc
				);
				return -1;
			}

			NXTSC (sed, c, -1);
			if (terminate_command (sed) <= -1) return -1;
			break;

		case HAWK_T('a'):
		case HAWK_T('i'):
			if (sed->opt.trait & HAWK_SED_STRICT &&
			    cmd->a2.type != HAWK_SED_ADR_NONE)
			{
				hawk_ooch_t tmpc = c;
				SETERR1 (
					sed, HAWK_SED_EA2PHB,
					&tmpc, 1, &sed->src.loc
				);
				return -1;
			}
		case HAWK_T('c'):
		{
			cmd->type = c;

			NXTSC (sed, c, -1);
			while (IS_SPACE(c)) NXTSC (sed, c, -1);

			if (c != HAWK_T('\\'))
			{
				if ((sed->opt.trait & HAWK_SED_SAMELINE) && 
				    c != HAWK_OOCI_EOF && c != HAWK_T('\n')) 
				{
					/* allow text without a starting backslash 
					 * on the same line as a command */
					goto sameline_ok;
				}

				SETERR0 (sed, HAWK_SED_EBSEXP, &sed->src.loc);
				return -1;
			}
		
			NXTSC (sed, c, -1);
			while (IS_SPACE(c)) NXTSC (sed, c, -1);

			if (c != HAWK_OOCI_EOF && c != HAWK_T('\n'))
			{
				if (sed->opt.trait & HAWK_SED_SAMELINE) 
				{
					/* allow text with a starting backslash 
					 * on the same line as a command */
					goto sameline_ok;
				}

				SETERR0 (sed, HAWK_SED_EGBABS, &sed->src.loc);
				return -1;
			}
			
			NXTSC (sed, c, -1); /* skip the new line */

		sameline_ok:
			/* get_text() starts from the next line */
			if (get_text (sed, cmd) <= -1) return -1;

			break;
		}

		case HAWK_T('='):
			if (sed->opt.trait & HAWK_SED_STRICT &&
			    cmd->a2.type != HAWK_SED_ADR_NONE)
			{
				hawk_ooch_t tmpc = c;
				SETERR1 (
					sed, HAWK_SED_EA2PHB,
					&tmpc, 1, &sed->src.loc
				);
				return -1;
			}

		case HAWK_T('d'):
		case HAWK_T('D'):

		case HAWK_T('p'):
		case HAWK_T('P'):
		case HAWK_T('l'):

		case HAWK_T('h'):
		case HAWK_T('H'):
		case HAWK_T('g'):
		case HAWK_T('G'):
		case HAWK_T('x'):

		case HAWK_T('n'):
		case HAWK_T('N'):

		case HAWK_T('z'):
			cmd->type = c;
			NXTSC (sed, c, -1); 
			if (terminate_command (sed) <= -1) return -1;
			break;

		case HAWK_T('b'):
		case HAWK_T('t'):
			cmd->type = c;
			NXTSC (sed, c, -1); 
			if (get_branch_target (sed, cmd) <= -1) return -1;
			break;

		case HAWK_T('r'):
		case HAWK_T('R'):
		case HAWK_T('w'):
		case HAWK_T('W'):
			cmd->type = c;
			NXTSC (sed, c, -1); 
			if (get_file (sed, &cmd->u.file) <= -1) return -1;
			break;

		case HAWK_T('s'):
			cmd->type = c;
			NXTSC (sed, c, -1); 
			if (get_subst (sed, cmd) <= -1) return -1;
			break;

		case HAWK_T('y'):
			cmd->type = c;
			NXTSC (sed, c, -1); 
			if (get_transet (sed, cmd) <= -1) return -1;
			break;

		case HAWK_T('C'):
			cmd->type = c;
			NXTSC (sed, c, -1);
			if (get_cut (sed, cmd) <= -1) return -1;
			break;
	}

	return 0;
}

int hawk_sed_comp (hawk_sed_t* sed, hawk_sed_io_impl_t inf)
{
	hawk_ooci_t c;
	hawk_sed_cmd_t* cmd = HAWK_NULL;
	hawk_loc_t a1_loc;

	if (inf == HAWK_NULL)
	{
		hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EINVAL);
		return -1;	
	}

	/* free all the commands previously compiled */
	free_all_command_blocks (sed);
	HAWK_ASSERT (sed->cmd.lb == &sed->cmd.fb && sed->cmd.lb->len == 0);

	/* free all the compilation identifiers */
	free_all_cids (sed);

	/* clear the label table */
	hawk_map_clear (&sed->tmp.labs);

	/* clear temporary data */
	sed->tmp.grp.level = 0;
	hawk_ooecs_clear (&sed->tmp.rex);

	/* open script */
	sed->src.fun = inf;
	if (open_script_stream (sed) <= -1) return -1;
	NXTSC_GOTO (sed, c, oops);

	while (1)
	{
		int n;

		/* skip spaces including newlines */
		while (IS_WSPACE(c)) NXTSC_GOTO (sed, c, oops);

		/* check if the end has been reached */
		if (c == HAWK_OOCI_EOF) break;

		/* check if the line is commented out */
		if (c == HAWK_T('#'))
		{
			do NXTSC_GOTO (sed, c, oops); 
			while (!IS_LINTERM(c) && c != HAWK_OOCI_EOF) ;
			NXTSC_GOTO (sed, c, oops);
			continue;
		}

		if (c == HAWK_T(';')) 
		{
			/* semicolon without a address-command pair */
			NXTSC_GOTO (sed, c, oops);
			continue;
		}

		/* initialize the current command */
		cmd = &sed->cmd.lb->buf[sed->cmd.lb->len];
		HAWK_MEMSET (cmd, 0, HAWK_SIZEOF(*cmd));

		/* process the first address */
		a1_loc = sed->src.loc;
		if (get_address (sed, &cmd->a1, 0) == HAWK_NULL) 
		{
			cmd = HAWK_NULL;
			SETERR0 (sed, HAWK_SED_EA1MOI, &sed->src.loc);
			goto oops;
		}

		c = CURSC (sed);
		if (cmd->a1.type != HAWK_SED_ADR_NONE)
		{
			while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);

			if (c == HAWK_T(',') ||
			    ((sed->opt.trait & HAWK_SED_EXTENDEDADR) && c == HAWK_T('~')))
			{
				hawk_ooch_t delim = c;

				/* maybe an address range */
				do { NXTSC_GOTO (sed, c, oops); } while (IS_SPACE(c));

				if (get_address (sed, &cmd->a2, (sed->opt.trait & HAWK_SED_EXTENDEDADR)) == HAWK_NULL) 
				{
					HAWK_ASSERT (cmd->a2.type == HAWK_SED_ADR_NONE);
					SETERR0 (sed, HAWK_SED_EA2MOI, &sed->src.loc);
					goto oops;
				}

				if (delim == HAWK_T(','))
				{
					if (cmd->a2.type == HAWK_SED_ADR_NONE)
					{
						SETERR0 (sed, HAWK_SED_EA2MOI, &sed->src.loc);
						goto oops;
					}
					if (cmd->a2.type == HAWK_SED_ADR_RELLINE || 
					    cmd->a2.type == HAWK_SED_ADR_RELLINEM)
					{
						if (cmd->a2.u.lno <= 0) 
						{
							/* tranform 'addr1,+0' and 'addr1,~0' to 'addr1' */
							cmd->a2.type = HAWK_SED_ADR_NONE;
						}
					}
				}
				else if ((sed->opt.trait & HAWK_SED_EXTENDEDADR) && 
				         (delim == HAWK_T('~')))
				{
					if (cmd->a1.type != HAWK_SED_ADR_LINE || 
					    cmd->a2.type != HAWK_SED_ADR_LINE)
					{
						SETERR0 (sed, HAWK_SED_EA2MOI, &sed->src.loc);
						goto oops;
					}

					if (cmd->a2.u.lno > 0)
					{
						cmd->a2.type = HAWK_SED_ADR_STEP;	
					}
					else
					{
						/* transform 'X,~0' to 'X' */
						cmd->a2.type = HAWK_SED_ADR_NONE;
					}
				}

				c = CURSC (sed);
			}
			else cmd->a2.type = HAWK_SED_ADR_NONE;
		}

		if (cmd->a1.type == HAWK_SED_ADR_LINE && cmd->a1.u.lno <= 0)
		{
			if (cmd->a2.type == HAWK_SED_ADR_STEP || 
			    ((sed->opt.trait & HAWK_SED_EXTENDEDADR) && 
			     cmd->a2.type == HAWK_SED_ADR_REX))
			{
				/* 0 as the first address is allowed in this two contexts.
				 *    0~step
				 *    0,/regex/
				 * '0~0' is not allowed. but at this point '0~0' 
				 * is already transformed to '0'. and disallowing it is 
				 * achieved gratuitously.
				 */
				/* nothing to do - adding negation to the condition dropped 
				 * code readability so i decided to write this part of code
				 * this way. 
				 */
			}
			else
			{
				SETERR0 (sed, HAWK_SED_EA1MOI, &a1_loc);
				goto oops;
			}
		}

		/* skip white spaces */
		while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);

		if (c == HAWK_T('!'))
		{
			/* allow any number of the negation indicators */
			do { 
				cmd->negated = !cmd->negated; 
				NXTSC_GOTO (sed, c, oops);
			} 
			while (c == HAWK_T('!'));

			while (IS_SPACE(c)) NXTSC_GOTO (sed, c, oops);
		}
	
		n = get_command (sed, cmd);
		if (n <= -1) goto oops;

		c = CURSC (sed);

		/* cmd's end of life */
		cmd = HAWK_NULL;

		/* increment the total numbers of complete commands */
		sed->cmd.lb->len++;
		if (sed->cmd.lb->len >= HAWK_COUNTOF(sed->cmd.lb->buf))
		{
			/* the number of commands in the block has
			 * reaches the maximum. add a new command block */
			if (add_command_block (sed) <= -1) goto oops;
		}
	}

	if (sed->tmp.grp.level != 0)
	{
		SETERR0 (sed, HAWK_SED_EGRNBA, &sed->src.loc);
		goto oops;
	}

	close_script_stream (sed);
	return 0;

oops:
	if (cmd) free_address (sed, cmd);
	close_script_stream (sed);
	return -1;
}

static int read_char (hawk_sed_t* sed, hawk_ooch_t* c)
{
	hawk_ooi_t n;

	if (sed->e.in.xbuf_len == 0)
	{
		if (sed->e.in.pos >= sed->e.in.len)
		{
			CLRERR (sed);
			n = sed->e.in.fun (
				sed, HAWK_SED_IO_READ, &sed->e.in.arg, 
				sed->e.in.buf, HAWK_COUNTOF(sed->e.in.buf)
			);
			if (n <= -1) 
			{
				if (ERRNUM(sed) == HAWK_ENOERR)
					hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
				return -1;
			}
	
			if (n == 0) return 0; /* end of file */
	
			sed->e.in.len = n;
			sed->e.in.pos = 0;
		}
	
		*c = sed->e.in.buf[sed->e.in.pos++];
		return 1;
	}
	else if (sed->e.in.xbuf_len > 0)
	{
		HAWK_ASSERT (sed->e.in.xbuf_len == 1);
		*c = sed->e.in.xbuf[--sed->e.in.xbuf_len];
		return 1;
	}
	else /*if (sed->e.in.xbuf_len < 0)*/
	{
		HAWK_ASSERT (sed->e.in.xbuf_len == -1);
		return 0;
	}	
}

static int read_line (hawk_sed_t* sed, int append)
{
	hawk_oow_t len = 0;
	hawk_ooch_t c;
	int n;

	if (!append) hawk_ooecs_clear (&sed->e.in.line);
	if (sed->e.in.eof) 
	{
	#if 0
		/* no more input detected in the previous read.
		 * set eof back to 0 here so that read_char() is called
		 * if read_line() is called again. that way, the result
		 * of subsequent calls counts on read_char(). */
		sed->e.in.eof = 0; 
	#endif
		return 0;
	}

	while (1)
	{
		n = read_char(sed, &c);
		if (n <= -1) return -1;
		if (n == 0)
		{
			sed->e.in.eof = 1;
			if (len == 0) return 0;
			/*sed->e.in.eof = 1;*/
			break;
		}

		if (hawk_ooecs_ccat(&sed->e.in.line, c) == (hawk_oow_t)-1) return -1;
		len++;	

		/* TODO: support different line end convension */
		if (c == HAWK_T('\n')) break;
	}

	sed->e.in.num++;
	sed->e.subst_done = 0;
	return 1;	
}

static int flush (hawk_sed_t* sed)
{
	hawk_oow_t pos = 0;
	hawk_ooi_t n;

	while (sed->e.out.len > 0)
	{
		CLRERR (sed);

		n = sed->e.out.fun (
			sed, HAWK_SED_IO_WRITE, &sed->e.out.arg,
			&sed->e.out.buf[pos], sed->e.out.len);

		if (n <= -1)
		{
			if (ERRNUM(sed) == HAWK_ENOERR)
				hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
			return -1;
		}

		if (n == 0)
		{
			/* reached the end of file - this is also an error */
			if (ERRNUM(sed) == HAWK_ENOERR)
				hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
			return -1;
		}

		pos += n;
		sed->e.out.len -= n;
	}

	return 0;
}

static int write_char (hawk_sed_t* sed, hawk_ooch_t c)
{
	sed->e.out.buf[sed->e.out.len++] = c;
	if (c == HAWK_T('\n') ||
	    sed->e.out.len >= HAWK_COUNTOF(sed->e.out.buf))
	{
		return flush (sed);
	}

	return 0;
}

static int write_str (hawk_sed_t* sed, const hawk_ooch_t* str, hawk_oow_t len)
{
	hawk_oow_t i;
	int flush_needed = 0;

	for (i = 0; i < len; i++)
	{
		/*if (write_char (sed, str[i]) <= -1) return -1;*/
		sed->e.out.buf[sed->e.out.len++] = str[i];
		if (sed->e.out.len >= HAWK_COUNTOF(sed->e.out.buf))
		{
			if (flush (sed) <= -1) return -1;
			flush_needed = 0;
		}
		/* TODO: handle different line ending convension... */
		else if (str[i] == HAWK_T('\n')) flush_needed = 1;
	}

	if (flush_needed && flush(sed) <= -1) return -1;
	return 0;
}

static int write_first_line (
	hawk_sed_t* sed, const hawk_ooch_t* str, hawk_oow_t len)
{
	hawk_oow_t i;
	for (i = 0; i < len; i++)
	{
		if (write_char (sed, str[i]) <= -1) return -1;
		/* TODO: handle different line ending convension... */
		if (str[i] == HAWK_T('\n')) break;
	}
	return 0;
}

#define NTOC(n) (HAWK_T("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ")[n])

static int write_num (hawk_sed_t* sed, hawk_oow_t x, int base, int width)
{
	hawk_oow_t last = x % base;
	hawk_oow_t y = 0; 
	int dig = 0;

	HAWK_ASSERT (base >= 2 && base <= 36);

	if (x < 0) 
	{
		if (write_char(sed, HAWK_T('-')) <= -1) return -1;
		if (width > 0) width--;
	}

	x = x / base;
	if (x < 0) x = -x;

	while (x > 0)
	{
		y = y * base + (x % base);
		x = x / base;
		dig++;
	}

	if (width > 0)
	{
		while (--width > dig)
		{
			if (write_char (sed, HAWK_T('0')) <= -1) return -1;
		}
	}

	while (y > 0)
	{
		if (write_char (sed, NTOC(y % base)) <= -1) return -1;
		y = y / base;
		dig--;
	}

	while (dig > 0) 
	{ 
		dig--; 
		if (write_char (sed, HAWK_T('0')) <= -1) return -1;
	}
	if (last < 0) last = -last;
	if (write_char (sed, NTOC(last)) <= -1) return -1;

	return 0;
}

#define WRITE_CHAR(sed,c) \
	do { if (write_char(sed,c) <= -1) return -1; } while (0)
#define WRITE_STR(sed,str,len) \
	do { if (write_str(sed,str,len) <= -1) return -1; } while (0)
#define WRITE_NUM(sed,num,base,width) \
	do { if (write_num(sed,num,base,width) <= -1) return -1; } while (0)

static int write_str_clearly (
	hawk_sed_t* sed, const hawk_ooch_t* str, hawk_oow_t len)
{
	const hawk_ooch_t* p = str;
	const hawk_ooch_t* end = str + len;

/* TODO: break down long lines.... */
	while (p < end)
	{
		hawk_ooch_t c = *p++;

		switch (c)
		{
			case HAWK_T('\\'):
				WRITE_STR (sed, HAWK_T("\\\\"), 2);
				break;
			/*case HAWK_T('\0'):
				WRITE_STR (sed, HAWK_T("\\0"), 2);
				break;*/
			case HAWK_T('\n'):
				WRITE_STR (sed, HAWK_T("$\n"), 2);
				break;
			case HAWK_T('\a'):
				WRITE_STR (sed, HAWK_T("\\a"), 2);
				break;
			case HAWK_T('\b'):
				WRITE_STR (sed, HAWK_T("\\b"), 2);
				break;
			case HAWK_T('\f'):
				WRITE_STR (sed, HAWK_T("\\f"), 2);
				break;
			case HAWK_T('\r'):
				WRITE_STR (sed, HAWK_T("\\r"), 2);
				break;
			case HAWK_T('\t'):
				WRITE_STR (sed, HAWK_T("\\t"), 2);
				break;
			case HAWK_T('\v'):
				WRITE_STR (sed, HAWK_T("\\v"), 2);
				break;
			default:
			{
				if (hawk_is_ooch_print(c)) WRITE_CHAR (sed, c);
				else
				{
				#if defined(HAWK_OOCH_IS_BCH)
					WRITE_CHAR (sed, HAWK_T('\\'));
					WRITE_NUM (sed, (unsigned char)c, 8, HAWK_SIZEOF(hawk_ooch_t)*3);
				#else
					if (HAWK_SIZEOF(hawk_ooch_t) <= 2)
					{
						WRITE_STR (sed, HAWK_T("\\u"), 2);
					}
					else
					{
						WRITE_STR (sed, HAWK_T("\\U"), 2);
					}
					WRITE_NUM (sed, c, 16, HAWK_SIZEOF(hawk_ooch_t)*2);
				#endif
				}
			}
		}
	}		

	if (len > 1 && end[-1] != HAWK_T('\n')) 
		WRITE_STR (sed, HAWK_T("$\n"), 2);

	return 0;
}

static int write_str_to_file (
	hawk_sed_t* sed, hawk_sed_cmd_t* cmd,
	const hawk_ooch_t* str, hawk_oow_t len, 
	const hawk_ooch_t* path, hawk_oow_t plen)
{
	hawk_ooi_t n;
	hawk_map_pair_t* pair;
	hawk_sed_io_arg_t* ap;

	pair = hawk_map_search(&sed->e.out.files, path, plen);
	if (pair == HAWK_NULL)
	{
		hawk_sed_io_arg_t arg;

		HAWK_MEMSET (&arg, 0, HAWK_SIZEOF(arg));
		pair = hawk_map_insert(&sed->e.out.files,
			(void*)path, plen, &arg, HAWK_SIZEOF(arg));
		if (pair == HAWK_NULL)
		{
			ADJERR_LOC (sed, &cmd->loc);
			return -1;
		}
	}

	ap = HAWK_MAP_VPTR(pair);
	if (ap->handle == HAWK_NULL)
	{
		CLRERR (sed);
		ap->path = path;
		n = sed->e.out.fun(sed, HAWK_SED_IO_OPEN, ap, HAWK_NULL, 0);
		if (n <= -1)
		{
			if (ERRNUM(sed) == HAWK_ENOERR)
				SETERR1 (sed, HAWK_SED_EIOFIL, path, plen, &cmd->loc);
			else
				ADJERR_LOC (sed, &cmd->loc);
			return -1;
		}
	}

	while (len > 0)
	{
		CLRERR (sed);
		n = sed->e.out.fun(sed, HAWK_SED_IO_WRITE, ap, (hawk_ooch_t*)str, len);
		if (n <= -1) 
		{
			sed->e.out.fun (sed, HAWK_SED_IO_CLOSE, ap, HAWK_NULL, 0);
			ap->handle = HAWK_NULL;
			if (ERRNUM(sed) == HAWK_ENOERR)
				SETERR1 (sed, HAWK_SED_EIOFIL, path, plen, &cmd->loc);
			else
				ADJERR_LOC (sed, &cmd->loc);
			return -1;
		}

		if (n == 0)
		{
			/* eof is returned on the write stream. 
			 * it is also an error as it can't write any more */
			sed->e.out.fun (sed, HAWK_SED_IO_CLOSE, ap, HAWK_NULL, 0);
			ap->handle = HAWK_NULL;
			SETERR1 (sed, HAWK_SED_EIOFIL, path, plen, &cmd->loc);
			return -1;
		}

		len -= n;
	}

	return 0;
}

static int write_file (hawk_sed_t* sed, hawk_sed_cmd_t* cmd, int first_line)
{
	hawk_ooi_t n;
	hawk_sed_io_arg_t arg;
#if defined(HAWK_OOCH_IS_BCH)
	hawk_ooch_t buf[1024];
#else
	hawk_ooch_t buf[512];
#endif

	arg.handle = HAWK_NULL;
	arg.path = cmd->u.file.ptr;
	CLRERR (sed);
	n = sed->e.in.fun(sed, HAWK_SED_IO_OPEN, &arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		/*if (ERRNUM(sed) != HAWK_ENOERR)
		 *	hawk_sed_seterrnum (sed, &cmd->loc, HAWK_EIOIMPL);
		 *return -1;*/
		/* it is ok if it is not able to open a file */
		return 0;	
	}

	while (1)
	{
		CLRERR (sed);
		n = sed->e.in.fun(sed, HAWK_SED_IO_READ, &arg, buf, HAWK_COUNTOF(buf));
		if (n <= -1)
		{
			sed->e.in.fun(sed, HAWK_SED_IO_CLOSE, &arg, HAWK_NULL, 0);
			if (ERRNUM(sed) == HAWK_ENOERR)
				SETERR1 (sed, HAWK_SED_EIOFIL, cmd->u.file.ptr, cmd->u.file.len, &cmd->loc);
			else
				ADJERR_LOC (sed, &cmd->loc);
			return -1;
		}
		if (n == 0) break;

		if (first_line)
		{
			hawk_oow_t i;

			for (i = 0; i < n; i++)
			{
				if (write_char (sed, buf[i]) <= -1) return -1;

				/* TODO: support different line end convension */
				if (buf[i] == HAWK_T('\n')) goto done;
			}
		}
		else
		{
			if (write_str(sed, buf, n) <= -1) return -1;
		}
	}

done:
	sed->e.in.fun (sed, HAWK_SED_IO_CLOSE, &arg, HAWK_NULL, 0);
	return 0;
}

static int link_append (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	if (sed->e.append.count < HAWK_COUNTOF(sed->e.append.s))
	{
		/* link it to the static buffer if it is not full */
		sed->e.append.s[sed->e.append.count++].cmd = cmd;
	}
	else
	{
		hawk_sed_app_t* app;

		/* otherwise, link it using a linked list */
		app = hawk_sed_allocmem(sed, HAWK_SIZEOF(*app));
		if (app == HAWK_NULL)
		{
			ADJERR_LOC (sed, &cmd->loc);
			return -1;
		}
		app->cmd = cmd;
		app->next = HAWK_NULL;

		if (sed->e.append.d.tail == HAWK_NULL)
			sed->e.append.d.head = app;
		else
			sed->e.append.d.tail->next = app;
		sed->e.append.d.tail = app;
		/*sed->e.append.count++; don't really care */
	}

	return 0;
}

static void free_appends (hawk_sed_t* sed)
{
	hawk_sed_app_t* app = sed->e.append.d.head;
	hawk_sed_app_t* next;
	
	while (app)
	{	
		next = app->next;
		hawk_sed_freemem (sed, app);
		app = next;
	}

	sed->e.append.d.head = HAWK_NULL;
	sed->e.append.d.tail = HAWK_NULL;
	sed->e.append.count = 0;		
}

static int emit_append (hawk_sed_t* sed, hawk_sed_app_t* app)
{
	switch (app->cmd->type)
	{
		case HAWK_SED_CMD_APPEND:
			return write_str(sed, app->cmd->u.text.ptr, app->cmd->u.text.len);

		case HAWK_SED_CMD_READ_FILE:
			return write_file(sed, app->cmd, 0);

		case HAWK_SED_CMD_READ_FILELN:
			return write_file(sed, app->cmd, 1);

		default:
			HAWK_ASSERT (!"should never happen. app->cmd->type must be one of APPEND,READ_FILE,READ_FILELN");
			hawk_sed_seterrnum (sed, &app->cmd->loc, HAWK_EINTERN);
			return -1;
	}
}

static int emit_appends (hawk_sed_t* sed)
{
	hawk_sed_app_t* app;
	hawk_oow_t i;

	for (i = 0; i < sed->e.append.count; i++)
	{
		if (emit_append(sed, &sed->e.append.s[i]) <= -1) return -1;
	}

	app = sed->e.append.d.head;
	while (app)
	{	
		if (emit_append(sed, app) <= -1) return -1;
		app = app->next;
	}

	return 0;
}

static const hawk_ooch_t* trim_line (hawk_sed_t* sed, hawk_oocs_t* str)
{
	const hawk_ooch_t* lineterm;

	str->ptr = HAWK_OOECS_PTR(&sed->e.in.line);
	str->len = HAWK_OOECS_LEN(&sed->e.in.line);

	/* TODO: support different line end convension */
	if (str->len > 0 && str->ptr[str->len-1] == HAWK_T('\n')) 
	{
		str->len--;
		if (str->len > 0 && str->ptr[str->len-1] == HAWK_T('\r')) 
		{
			lineterm = HAWK_T("\r\n");
			str->len--;
		}
		else
		{
			lineterm = HAWK_T("\n");
		}
	}
	else lineterm = HAWK_NULL;

	return lineterm;
}

static int do_subst (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_oocs_t mat, pmat;
	int opt = 0, repl = 0, n;
	const hawk_ooch_t* lineterm;

	hawk_oocs_t str, cur;
	const hawk_ooch_t* str_end;
	hawk_oow_t m, i, max_count, sub_count;

	HAWK_ASSERT (cmd->type == HAWK_SED_CMD_SUBSTITUTE);

	hawk_ooecs_clear (&sed->e.txt.scratch);

	lineterm = trim_line(sed, &str);
		
	str_end = str.ptr + str.len;
	cur = str;

	sub_count = 0;
	max_count = (cmd->u.subst.g)? 0: cmd->u.subst.occ;

	pmat.ptr = HAWK_NULL;
	pmat.len = 0;

	/* perform test when cur_ptr == str_end also because
	 * end of string($) needs to be tested */
	while (cur.ptr <= str_end)
	{
		hawk_oocs_t submat[9];
		HAWK_MEMSET (submat, 0, HAWK_SIZEOF(submat));

		if (max_count == 0 || sub_count < max_count)
		{
			void* rex;

			if (cmd->u.subst.rex == EMPTY_REX)
			{
				rex = sed->e.last_rex;
				if (rex == HAWK_NULL)
				{
					SETERR0 (sed, HAWK_SED_ENPREX, &cmd->loc);
					return -1;
				}
			}
			else 
			{
				rex = cmd->u.subst.rex;
				sed->e.last_rex = rex;
			}

			n = matchtre (
				sed, rex,
				((str.ptr == cur.ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
				&cur, &mat, submat, &cmd->loc
			);
			if (n <= -1) return -1;
		}
		else n = 0;

		if (n == 0) 
		{
			/* no more match found or substitution occurrence matched.
			 * copy the remaining portion and finish */
			if (!cmd->u.subst.k)
			{
				/* copy the remaining portion */
				m = hawk_ooecs_ncat (&sed->e.txt.scratch, cur.ptr, cur.len);
				if (m == (hawk_oow_t)-1) return -1;
			}
			break;
		}

		if (mat.len == 0 && 
		    pmat.ptr && mat.ptr == pmat.ptr + pmat.len)
		{
			/* match length is 0 and the match is still at the
			 * end of the previous match */
			goto skip_one_char;
		}

		if (max_count > 0 && sub_count + 1 != max_count)
		{
			/* substition occurrence specified.
			 * but this is not the occurrence yet */

			if (!cmd->u.subst.k && cur.ptr < str_end)
			{
				/* copy the unmatched portion and the matched portion
				 * together as if the matched portion was not matched */
				m = hawk_ooecs_ncat(
					&sed->e.txt.scratch,
					cur.ptr, mat.ptr - cur.ptr + mat.len
				);
				if (m == (hawk_oow_t)-1) return -1;
			}
		}
		else
		{
			/* perform actual substitution */

			repl = 1;

			if (!cmd->u.subst.k && cur.ptr < str_end)
			{
				m = hawk_ooecs_ncat(&sed->e.txt.scratch, cur.ptr, mat.ptr - cur.ptr);
				if (m == (hawk_oow_t)-1) return -1;
			}

			for (i = 0; i < cmd->u.subst.rpl.len; i++)
			{
				if ((i+1) < cmd->u.subst.rpl.len && 
				    cmd->u.subst.rpl.ptr[i] == HAWK_T('\\'))
				{
					hawk_ooch_t nc = cmd->u.subst.rpl.ptr[i+1];

					if (nc >= HAWK_T('1') && nc <= HAWK_T('9'))
					{
						int smi = nc - HAWK_T('1');
						m = hawk_ooecs_ncat (
							&sed->e.txt.scratch,
							submat[smi].ptr, submat[smi].len
						);
					}
					else
					{
						/* Known speical characters have been escaped
						 * in get_subst(). so i don't call trans_escaped() here. 
						 * It's a normal character that's escaped. 
						 * For example, \1 is just 1. and \M is just M. */
						m = hawk_ooecs_ccat(&sed->e.txt.scratch, nc);
					}

					i++;
				}
				else if (cmd->u.subst.rpl.ptr[i] == HAWK_T('&'))
				{
					m = hawk_ooecs_ncat(&sed->e.txt.scratch, mat.ptr, mat.len);
				}
				else 
				{
					m = hawk_ooecs_ccat(&sed->e.txt.scratch, cmd->u.subst.rpl.ptr[i]);
				}

				if (m == (hawk_oow_t)-1) return -1;
			}
		}

		sub_count++;
		cur.len = cur.len - ((mat.ptr - cur.ptr) + mat.len);
		cur.ptr = mat.ptr + mat.len;

		pmat = mat;

		if (mat.len == 0)
		{
		skip_one_char:
			if (cur.ptr < str_end)
			{
				/* special treament is needed if the match length is 0 */
				m = hawk_ooecs_ncat(&sed->e.txt.scratch, cur.ptr, 1);
				if (m == (hawk_oow_t)-1) return -1;
			}

			cur.ptr++; cur.len--;
		}
	}

	if (lineterm)
	{
		m = hawk_ooecs_cat(&sed->e.txt.scratch, lineterm);
		if (m == (hawk_oow_t)-1) return -1;
	}

	hawk_ooecs_swap (&sed->e.in.line, &sed->e.txt.scratch);

	if (repl)
	{
		if (cmd->u.subst.p)
		{
			n = write_str (
				sed, 
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line)
			);
			if (n <= -1) return -1;
		}

		if (cmd->u.subst.file.ptr)
		{
			n = write_str_to_file (
				sed, cmd,
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line),
				cmd->u.subst.file.ptr,
				cmd->u.subst.file.len
			);
			if (n <= -1) return -1;
		}

		sed->e.subst_done = 1;
	}

	return 0;
}

static int split_into_fields_for_cut (
	hawk_sed_t* sed, hawk_sed_cmd_t* cmd, const hawk_oocs_t* str)
{
	hawk_oow_t i, x = 0, xl = 0;

	sed->e.cutf.delimited = 0;
	sed->e.cutf.flds[x].ptr = str->ptr;

	for (i = 0; i < str->len; )
	{
		int isdelim = 0;
		hawk_ooch_t c = str->ptr[i++];

		if (cmd->u.cut.w)
		{ 
			/* the w option ignores the d specifier */
			if (hawk_is_ooch_space(c))
			{
				/* the w option assumes the f option */
				while (i < str->len && hawk_is_ooch_space(str->ptr[i])) i++;
				isdelim = 1;
			}
		}
		else
		{
			if (c == cmd->u.cut.delim[0])
			{
				if (cmd->u.cut.f)
				{
					/* fold consecutive delimiters */
					while (i < str->len && str->ptr[i] == cmd->u.cut.delim[0]) i++;
				}
				isdelim = 1;
			}
		}

		if (isdelim)
		{
			sed->e.cutf.flds[x++].len = xl;

			if (x >= sed->e.cutf.cflds)
			{
				hawk_oocs_t* tmp;
				hawk_oow_t nsz;

				nsz = sed->e.cutf.cflds;
				if (nsz > 50000) nsz += 50000;
				else nsz *= 2;
				
				if (sed->e.cutf.flds == sed->e.cutf.sflds)
				{
					tmp = hawk_sed_allocmem (sed, HAWK_SIZEOF(*tmp) * nsz);
					if (tmp == HAWK_NULL) return -1;
					HAWK_MEMCPY (tmp, sed->e.cutf.flds, HAWK_SIZEOF(*tmp) * sed->e.cutf.cflds);
				}
				else
				{
					tmp = hawk_sed_reallocmem (sed, sed->e.cutf.flds, HAWK_SIZEOF(*tmp) * nsz);
					if (tmp == HAWK_NULL) return -1;
				}

				sed->e.cutf.flds = tmp;
				sed->e.cutf.cflds = nsz;
			}

			xl = 0;
			sed->e.cutf.flds[x].ptr = &str->ptr[i];

			/* mark that this line is delimited at least once */
			sed->e.cutf.delimited = 1; 
		}
		else xl++;
	}

	sed->e.cutf.flds[x].len = xl;
	sed->e.cutf.nflds = ++x;

	return 0;
}

static int do_cut (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	hawk_sed_cut_sel_t* b;
	const hawk_ooch_t* lineterm;
	hawk_oocs_t str;
	int out_state;

	hawk_ooecs_clear (&sed->e.txt.scratch);

	lineterm = trim_line(sed, &str);

	if (str.len <= 0) goto done;

	if (cmd->u.cut.fcount > 0) 
	{
	    if (split_into_fields_for_cut (sed, cmd, &str) <= -1) goto oops;

		if (cmd->u.cut.d && !sed->e.cutf.delimited) 
		{
			/* if the 'd' option is set and the line is not 
			 * delimited by the input delimiter, delete the pattern
			 * space and finish the current cycle */
			hawk_ooecs_clear (&sed->e.in.line);
			return 0;
		}
	}

	out_state = 0;

	for (b = cmd->u.cut.fb; b; b = b->next)
	{
		hawk_oow_t i, s, e;

		for (i = 0; i < b->len; i++)
		{
			if (b->range[i].id == HAWK_SED_CUT_SEL_CHAR)
			{
				s = b->range[i].start;
				e = b->range[i].end;

				if (s <= e)
				{
					if (s < str.len)
					{
						if (e >= str.len) e = str.len - 1;
						if ((out_state == 2 && hawk_ooecs_ccat(&sed->e.txt.scratch, cmd->u.cut.delim[1]) == (hawk_oow_t)-1) ||
						    hawk_ooecs_ncat(&sed->e.txt.scratch, &str.ptr[s], e - s + 1) == (hawk_oow_t)-1) 
							{
								goto oops;
							}

						out_state = 1;
					}
				}
				else
				{
					if (e < str.len)
					{
						if (s >= str.len) s = str.len - 1;
						if ((out_state == 2 && hawk_ooecs_ccat(&sed->e.txt.scratch, cmd->u.cut.delim[1]) == (hawk_oow_t)-1) ||
						    hawk_ooecs_nrcat(&sed->e.txt.scratch, &str.ptr[e], s - e + 1) == (hawk_oow_t)-1)
						{
							goto oops;
						}

						out_state = 1;
					}
				}
			}
			else /*if (b->range[i].id == HAWK_SED_CUT_SEL_FIELD)*/
			{
				s = b->range[i].start;
				e = b->range[i].end;

				if (s <= e)
				{
					if (s < str.len)
					{
						if (e >= sed->e.cutf.nflds) e = sed->e.cutf.nflds - 1;

						while (s <= e)
						{	
							if ((out_state > 0 && hawk_ooecs_ccat(&sed->e.txt.scratch, cmd->u.cut.delim[1]) == (hawk_oow_t)-1) ||
							    hawk_ooecs_ncat(&sed->e.txt.scratch, sed->e.cutf.flds[s].ptr, sed->e.cutf.flds[s].len) == (hawk_oow_t)-1)
							{
								goto oops;
							}
							s++;

							out_state = 2;
						}
					}
				}
				else
				{
					if (e < str.len)
					{
						if (s >= sed->e.cutf.nflds) s = sed->e.cutf.nflds - 1;

						while (e <= s)
						{	
							if ((out_state > 0 && hawk_ooecs_ccat(&sed->e.txt.scratch, cmd->u.cut.delim[1]) == (hawk_oow_t)-1) ||
							    hawk_ooecs_ncat(&sed->e.txt.scratch, sed->e.cutf.flds[e].ptr, sed->e.cutf.flds[e].len) == (hawk_oow_t)-1)
							{
								goto oops;
							}
							e++;

							out_state = 2;
						}
					}
				}	
			}
		}
	}

done:
	if (lineterm)
	{
		if (hawk_ooecs_cat(&sed->e.txt.scratch, lineterm) == (hawk_oow_t)-1) return -1;
	}

	hawk_ooecs_swap (&sed->e.in.line, &sed->e.txt.scratch);
	return 1;

oops:
	return -1;
}

static int match_a (hawk_sed_t* sed, hawk_sed_cmd_t* cmd, hawk_sed_adr_t* a)
{
	switch (a->type)
	{
		case HAWK_SED_ADR_LINE:
			return (sed->e.in.num == a->u.lno)? 1: 0;

		case HAWK_SED_ADR_REX:
		{
			hawk_oocs_t line;
			void* rex;

			HAWK_ASSERT (a->u.rex != HAWK_NULL);

			line.ptr = HAWK_OOECS_PTR(&sed->e.in.line);
			line.len = HAWK_OOECS_LEN(&sed->e.in.line);

			if (line.len > 0 &&
			    line.ptr[line.len-1] == HAWK_T('\n')) 
			{
				line.len--;
				if (line.len > 0 && line.ptr[line.len-1] == HAWK_T('\r')) line.len--;
			}

			if (a->u.rex == EMPTY_REX)
			{
				rex = sed->e.last_rex;
				if (rex == HAWK_NULL)
				{
					hawk_sed_seterrnum (sed, &cmd->loc, HAWK_SED_ENPREX);
					return -1;
				}
			}
			else 
			{
				rex = a->u.rex;
				sed->e.last_rex = rex;
			}
			return matchtre(sed, rex, 0, &line, HAWK_NULL, HAWK_NULL, &cmd->loc);

		}
		case HAWK_SED_ADR_DOL:
		{
			hawk_ooch_t c;
			int n;

			if (sed->e.in.xbuf_len < 0)
			{
				/* we know that we've reached eof as it has
				 * been done so previously */
				return 1;
			}

			n = read_char (sed, &c);
			if (n <= -1) return -1;

			HAWK_ASSERT (sed->e.in.xbuf_len == 0);
			if (n == 0)
			{
				/* eof has been reached */
				sed->e.in.xbuf_len--;
				return 1;
			}
			else
			{
				sed->e.in.xbuf[sed->e.in.xbuf_len++] = c;
				return 0;
			}
		}

		case HAWK_SED_ADR_RELLINE:
			/* this address type should be seen only when matching 
			 * the second address */
			HAWK_ASSERT (cmd->state.a1_matched && cmd->state.a1_match_line >= 1);
			return (sed->e.in.num >= cmd->state.a1_match_line + a->u.lno)? 1: 0;

		case HAWK_SED_ADR_RELLINEM:
		{
			/* this address type should be seen only when matching 
			 * the second address */
			hawk_oow_t tmp;

			HAWK_ASSERT (cmd->state.a1_matched && cmd->state.a1_match_line >= 1);
			HAWK_ASSERT (a->u.lno > 0);

			/* TODO: is it better to store this value some in the state
			 *       not to calculate this every time?? */
			tmp = (cmd->state.a1_match_line + a->u.lno) - 
			      (cmd->state.a1_match_line % a->u.lno);

			return (sed->e.in.num >= tmp)? 1: 0;
		}

		default:
			HAWK_ASSERT (a->type == HAWK_SED_ADR_NONE);
			return 1; /* match */
	}
}

/* match an address against input.
 * return -1 on error, 0 on no match, 1 on match. */
static int match_address (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	int n;

	cmd->state.c_ready = 0;
	if (cmd->a1.type == HAWK_SED_ADR_NONE)
	{
		HAWK_ASSERT (cmd->a2.type == HAWK_SED_ADR_NONE);
		cmd->state.c_ready = 1;
		return 1;
	}
	else if (cmd->a2.type == HAWK_SED_ADR_STEP)
	{
		HAWK_ASSERT (cmd->a1.type == HAWK_SED_ADR_LINE);

		/* stepping address */
		cmd->state.c_ready = 1;
		if (sed->e.in.num < cmd->a1.u.lno) return 0;
		HAWK_ASSERT (cmd->a2.u.lno > 0);
		if ((sed->e.in.num - cmd->a1.u.lno) % cmd->a2.u.lno == 0) return 1;
		return 0;
	}
	else if (cmd->a2.type != HAWK_SED_ADR_NONE)
	{
		/* two addresses */
		if (cmd->state.a1_matched)
		{
			n = match_a (sed, cmd, &cmd->a2);
			if (n <= -1) return -1;
			if (n == 0)
			{
				if (cmd->a2.type == HAWK_SED_ADR_LINE &&
				    sed->e.in.num > cmd->a2.u.lno)
				{
					/* This check is needed because matching of the second 
					 * address could be skipped while it could match.
					 * 
					 * Consider commands like '1,3p;2N'.
					 * '3' in '1,3p' is skipped because 'N' in '2N' triggers
					 * reading of the third line.
					 *
					 * Unfortunately, I can't handle a non-line-number
					 * second address like this. If 'abcxyz' is given as the third 
					 * line for command '1,/abc/p;2N', 'abcxyz' is not matched
					 * against '/abc/'. so it doesn't exit the range.
					 */
					cmd->state.a1_matched = 0;
					return 0;
				}

				/* still in the range. return match 
				 * despite the actual mismatch */
				return 1;
			}

			/* exit the range */
			cmd->state.a1_matched = 0;
			cmd->state.c_ready = 1;
			return 1;
		}
		else 
		{
			n = match_a (sed, cmd, &cmd->a1);
			if (n <= -1) return -1;
			if (n == 0) 
			{
				return 0;
			}

			if (cmd->a2.type == HAWK_SED_ADR_LINE &&
			    sed->e.in.num >= cmd->a2.u.lno) 
			{
				/* the line number specified in the second 
				 * address is equal to or less than the current
				 * line number. */
				cmd->state.c_ready = 1;
			}
			else
			{
				/* mark that the first is matched so as to
				 * move on to the range test */
				cmd->state.a1_matched = 1;
				cmd->state.a1_match_line = sed->e.in.num;
			}

			return 1;
		}
	}
	else
	{
		/* single address */
		cmd->state.c_ready = 1;

		n = match_a (sed, cmd, &cmd->a1);
		return (n <= -1)? -1:
		       (n ==  0)? 0: 1;
	}
}

static hawk_sed_cmd_t* exec_cmd (hawk_sed_t* sed, hawk_sed_cmd_t* cmd)
{
	int n;
	hawk_sed_cmd_t* jumpto = HAWK_NULL;

	switch (cmd->type)
	{
		case HAWK_SED_CMD_NOOP:
			break;
			
		case HAWK_SED_CMD_QUIT:
			jumpto = &sed->cmd.quit;
			break;

		case HAWK_SED_CMD_QUIT_QUIET:
			jumpto = &sed->cmd.quit_quiet;
			break;

		case HAWK_SED_CMD_APPEND:
			if (link_append (sed, cmd) <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_INSERT:
			n = write_str (sed,
				cmd->u.text.ptr,
				cmd->u.text.len
			);
			if (n <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_CHANGE:
			if (cmd->state.c_ready)
			{
				/* change the pattern space */
				n = hawk_ooecs_ncpy(
					&sed->e.in.line,
					cmd->u.text.ptr,
					cmd->u.text.len
				);
				if (n == (hawk_oow_t)-1) return HAWK_NULL;
			}
			else 
			{		
				hawk_ooecs_clear (&sed->e.in.line);
			}

			/* move past the last command so as to start 
			 * the next cycle */
			jumpto = &sed->cmd.over;
			break;

		case HAWK_SED_CMD_DELETE_FIRSTLN:
		{
			hawk_ooch_t* nl;

			/* delete the first line from the pattern space */
			nl = hawk_find_oochar_in_oochars(
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line),
				HAWK_T('\n'));
			if (nl) 
			{
				/* if a new line is found. delete up to it  */
				hawk_ooecs_del (&sed->e.in.line, 0, nl - HAWK_OOECS_PTR(&sed->e.in.line) + 1);	

				if (HAWK_OOECS_LEN(&sed->e.in.line) > 0)
				{
					/* if the pattern space is not empty,
					 * arrange to execute from the first
					 * command */
					jumpto = &sed->cmd.again;
				}
				else
				{
					/* finish the current cycle */
					jumpto = &sed->cmd.over;
				}
				break;
			}

			/* otherwise clear the entire pattern space below */
		}
		case HAWK_SED_CMD_DELETE:
			/* delete the pattern space */
			hawk_ooecs_clear (&sed->e.in.line);

			/* finish the current cycle */
			jumpto = &sed->cmd.over;
			break;

		case HAWK_SED_CMD_PRINT_LNNUM:
			if (write_num(sed, sed->e.in.num, 10, 0) <= -1) return HAWK_NULL;
			if (write_char(sed, HAWK_T('\n')) <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_PRINT:
			n = write_str (
				sed, 
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line)
			);
			if (n <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_PRINT_FIRSTLN:
			n = write_first_line (
				sed, 
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line)
			);
			if (n <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_PRINT_CLEARLY:
			if (sed->opt.lformatter)
			{
				n = sed->opt.lformatter (
					sed,
					HAWK_OOECS_PTR(&sed->e.in.line),
					HAWK_OOECS_LEN(&sed->e.in.line),
					write_char
				);
			}
			else {
				n = write_str_clearly (
					sed,
					HAWK_OOECS_PTR(&sed->e.in.line),
					HAWK_OOECS_LEN(&sed->e.in.line)
				);
			}
			if (n <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_HOLD:
			/* copy the pattern space to the hold space */
			if (hawk_ooecs_ncpy (&sed->e.txt.hold, 	
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line)) == (hawk_oow_t)-1)
			{
				return HAWK_NULL;	
			}
			break;
				
		case HAWK_SED_CMD_HOLD_APPEND:
			/* append the pattern space to the hold space */
			if (hawk_ooecs_ncat (&sed->e.txt.hold, 	
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line)) == (hawk_oow_t)-1)
			{
				return HAWK_NULL;	
			}
			break;

		case HAWK_SED_CMD_RELEASE:
			/* copy the hold space to the pattern space */
			if (hawk_ooecs_ncpy (&sed->e.in.line,
				HAWK_OOECS_PTR(&sed->e.txt.hold),
				HAWK_OOECS_LEN(&sed->e.txt.hold)) == (hawk_oow_t)-1)
			{
				return HAWK_NULL;	
			}
			break;

		case HAWK_SED_CMD_RELEASE_APPEND:
			/* append the hold space to the pattern space */
			if (hawk_ooecs_ncat (&sed->e.in.line,
				HAWK_OOECS_PTR(&sed->e.txt.hold),
				HAWK_OOECS_LEN(&sed->e.txt.hold)) == (hawk_oow_t)-1)
			{
				return HAWK_NULL;	
			}
			break;

		case HAWK_SED_CMD_EXCHANGE:
			/* exchange the pattern space and the hold space */
			hawk_ooecs_swap (&sed->e.in.line, &sed->e.txt.hold);
			break;

		case HAWK_SED_CMD_NEXT:
			if (emit_output (sed, 0) <= -1) return HAWK_NULL;

			/* read the next line and fill the pattern space */
			n = read_line (sed, 0);
			if (n <= -1) return HAWK_NULL;
			if (n == 0) 
			{
				/* EOF is reached. */
				jumpto = &sed->cmd.over;
			}
			break;

		case HAWK_SED_CMD_NEXT_APPEND:
			/* append the next line to the pattern space */
			if (emit_output (sed, 1) <= -1) return HAWK_NULL;

			n = read_line (sed, 1);
			if (n <= -1) return HAWK_NULL;
			if (n == 0)
			{
				/* EOF is reached. */
				jumpto = &sed->cmd.over;
			}
			break;
				
		case HAWK_SED_CMD_READ_FILE:
			if (link_append (sed, cmd) <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_READ_FILELN:
			if (link_append (sed, cmd) <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_WRITE_FILE:
			n = write_str_to_file (
				sed, cmd,
				HAWK_OOECS_PTR(&sed->e.in.line),
				HAWK_OOECS_LEN(&sed->e.in.line),
				cmd->u.file.ptr,
				cmd->u.file.len
			);
			if (n <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_WRITE_FILELN:
		{
			const hawk_ooch_t* ptr = HAWK_OOECS_PTR(&sed->e.in.line);
			hawk_oow_t i, len = HAWK_OOECS_LEN(&sed->e.in.line);
			for (i = 0; i < len; i++)
			{	
				/* TODO: handle different line end convension */
				if (ptr[i] == HAWK_T('\n')) 
				{
					i++;
					break;
				}
			}

			n = write_str_to_file (
				sed, cmd, ptr, i,
				cmd->u.file.ptr,
				cmd->u.file.len
			);
			if (n <= -1) return HAWK_NULL;
			break;
		}
			
		case HAWK_SED_CMD_BRANCH_COND:
			if (!sed->e.subst_done) break;
			sed->e.subst_done = 0;
		case HAWK_SED_CMD_BRANCH:
			HAWK_ASSERT (cmd->u.branch.target != HAWK_NULL);
			jumpto = cmd->u.branch.target;
			break;

		case HAWK_SED_CMD_SUBSTITUTE:
			if (do_subst (sed, cmd) <= -1) return HAWK_NULL;
			break;

		case HAWK_SED_CMD_TRANSLATE:
		{
			hawk_ooch_t* ptr = HAWK_OOECS_PTR(&sed->e.in.line);
			hawk_oow_t i, len = HAWK_OOECS_LEN(&sed->e.in.line);

		/* TODO: sort cmd->u.transset and do binary search 
		 * when sorted, you can, before binary search, check 
		 * if ptr[i] < transet[0] || ptr[i] > transset[transset_size-1].
		 * if so, it has not mathing translation */

			/* TODO: support different line end convension */
			if (len > 0 && ptr[len-1] == HAWK_T('\n')) 
			{
				len--;
				if (len > 0 && ptr[len-1] == HAWK_T('\r')) len--;
			}

			for (i = 0; i < len; i++)
			{
				const hawk_ooch_t* tptr = cmd->u.transet.ptr;
				hawk_oow_t j, tlen = cmd->u.transet.len;
				for (j = 0; j < tlen; j += 2)
				{
					if (ptr[i] == tptr[j])
					{
						ptr[i] = tptr[j+1];
						break;
					}
				}
			}
			break;
		}

		case HAWK_SED_CMD_CLEAR_PATTERN:
			/* clear pattern space */
			hawk_ooecs_clear (&sed->e.in.line);
			break;

		case HAWK_SED_CMD_CUT:
			n = do_cut (sed, cmd);
			if (n <= -1) return HAWK_NULL;
			if (n == 0) jumpto = &sed->cmd.over; /* finish the current cycle */
			break;
	}

	if (jumpto == HAWK_NULL) jumpto = cmd->state.next;
	return jumpto;
} 

static void close_outfile (hawk_map_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_sed_io_arg_t* arg = dptr;
	HAWK_ASSERT (dlen == HAWK_SIZEOF(*arg));

	if (arg->handle)
	{
		hawk_sed_t* sed = *(hawk_sed_t**)(map + 1);
		sed->e.out.fun (sed, HAWK_SED_IO_CLOSE, arg, HAWK_NULL, 0);
		arg->handle = HAWK_NULL;
	}
}

static int init_command_block_for_exec (hawk_sed_t* sed, hawk_sed_cmd_blk_t* b)
{
	hawk_oow_t i;

	HAWK_ASSERT (b->len <= HAWK_COUNTOF(b->buf));

	for (i = 0; i < b->len; i++)
	{
		hawk_sed_cmd_t* c = &b->buf[i];
		const hawk_oocs_t* file = HAWK_NULL;

		/* clear states */
		c->state.a1_matched = 0;

		if (sed->opt.trait & HAWK_SED_EXTENDEDADR)
		{
			if (c->a2.type == HAWK_SED_ADR_REX &&
			    c->a1.type == HAWK_SED_ADR_LINE &&
			    c->a1.u.lno <= 0)
			{
				/* special handling for 0,/regex/ */
				c->state.a1_matched = 1;
				c->state.a1_match_line = 0;
			}
		}

		c->state.c_ready = 0;

		/* let c point to the next command */
		if (i + 1 >= b->len)
		{
			if (b->next == HAWK_NULL || b->next->len <= 0)
				c->state.next = &sed->cmd.over;
			else
				c->state.next = &b->next->buf[0];
		}
		else
		{
			c->state.next = &b->buf[i+1];
		}

		if ((c->type == HAWK_SED_CMD_BRANCH ||
		     c->type == HAWK_SED_CMD_BRANCH_COND) &&
		    c->u.branch.target == HAWK_NULL)
		{
			/* resolve unresolved branch targets */
			hawk_map_pair_t* pair;
			hawk_oocs_t* lab = &c->u.branch.label;

			if (lab->ptr == HAWK_NULL)
			{
				/* arrange to branch past the last */
				c->u.branch.target = &sed->cmd.over;
			}
			else
			{
				/* resolve the target */
			  	pair = hawk_map_search (
					&sed->tmp.labs, lab->ptr, lab->len);
				if (pair == HAWK_NULL)
				{
					SETERR1 (
						sed, HAWK_SED_ELABNF,
						lab->ptr, lab->len, &c->loc
					);
					return -1;
				}

				c->u.branch.target = HAWK_MAP_VPTR(pair);

				/* free resolved label name */ 
				hawk_sed_freemem (sed, lab->ptr);
				lab->ptr = HAWK_NULL;
				lab->len = 0;
			}
		}
		else
		{
			/* open output files in advance */
			if (c->type == HAWK_SED_CMD_WRITE_FILE ||
			    c->type == HAWK_SED_CMD_WRITE_FILELN)
			{
				file = &c->u.file;
			}
			else if (c->type == HAWK_SED_CMD_SUBSTITUTE &&
			         c->u.subst.file.ptr)
			{
				file = &c->u.subst.file;
			}
		
			if (file)
			{
				/* call this function to an open output file */
				int n = write_str_to_file (
					sed, c, HAWK_NULL, 0, 
					file->ptr, file->len
				);
				if (n <= -1) return -1;
			}
		}
	}

	return 0;
}

static int init_all_commands_for_exec (hawk_sed_t* sed)
{
	hawk_sed_cmd_blk_t* b;

	for (b = &sed->cmd.fb; b != HAWK_NULL; b = b->next)
	{
		if (init_command_block_for_exec (sed, b) <= -1) return -1;
	}

	return 0;
}

static int emit_output (hawk_sed_t* sed, int skipline)
{
	int n;

	if (!skipline && !(sed->opt.trait & HAWK_SED_QUIET))
	{
		/* write the pattern space */
		n = write_str (sed, 
			HAWK_OOECS_PTR(&sed->e.in.line),
			HAWK_OOECS_LEN(&sed->e.in.line));
		if (n <= -1) return -1;
	}

	if (emit_appends (sed) <= -1) return -1;
	free_appends (sed);

	/* flush the output stream in case it's not flushed 
	 * in write functions */
	n = flush (sed);
	if (n <= -1) return -1;

	return 0;
}

int hawk_sed_exec (hawk_sed_t* sed, hawk_sed_io_impl_t inf, hawk_sed_io_impl_t outf)
{
	hawk_ooi_t n;
	int ret = 0;

	static hawk_map_style_t style =
	{
		{
			HAWK_MAP_COPIER_INLINE,
			HAWK_MAP_COPIER_INLINE
		},
		{
			HAWK_MAP_FREEER_DEFAULT,
			close_outfile
		},
		HAWK_MAP_COMPER_DEFAULT,
		HAWK_MAP_KEEPER_DEFAULT
#if defined(HAWK_MAP_IS_HTB)
		,
		HAWK_MAP_SIZER_DEFAULT,
		HAWK_MAP_HASHER_DEFAULT
#endif
	};

	sed->e.haltreq = 0;
	sed->e.last_rex = HAWK_NULL;

	sed->e.subst_done = 0;

	free_appends (sed);
	hawk_ooecs_clear (&sed->e.txt.scratch);
	hawk_ooecs_clear (&sed->e.txt.hold);
	if (hawk_ooecs_ccat(&sed->e.txt.hold, HAWK_T('\n')) == (hawk_oow_t)-1) return -1;

	sed->e.out.fun = outf;
	sed->e.out.eof = 0;
	sed->e.out.len = 0;
	if (hawk_map_init(
		&sed->e.out.files, hawk_sed_getgem(sed),
		128, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1) return -1;

	HAWK_ASSERT ((void*)(&sed->e.out.files + 1) == (void*)&sed->e.out.files_ext);
	*(hawk_sed_t**)(&sed->e.out.files + 1) = sed;
	hawk_map_setstyle (&sed->e.out.files, &style);

	sed->e.in.fun = inf;
	sed->e.in.eof = 0;
	sed->e.in.len = 0;
	sed->e.in.pos = 0;
	sed->e.in.num = 0;
	if (hawk_ooecs_init(&sed->e.in.line, hawk_sed_getgem(sed), 256) <= -1)
	{
		hawk_map_fini (&sed->e.out.files);
		return -1;
	}

	CLRERR (sed);
	sed->e.in.arg.path = HAWK_NULL;
	n = sed->e.in.fun(sed, HAWK_SED_IO_OPEN, &sed->e.in.arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		ret = -1;
		if (ERRNUM(sed) == HAWK_ENOERR) 
			hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
		goto done3;
	}
	
	CLRERR (sed);
	sed->e.out.arg.path = HAWK_NULL;
	n = sed->e.out.fun(sed, HAWK_SED_IO_OPEN, &sed->e.out.arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		ret = -1;
		if (ERRNUM(sed) == HAWK_ENOERR) 
			hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_EIOIMPL);
		goto done2;
	}

	if (init_all_commands_for_exec (sed) <= -1)
	{
		ret = -1;
		goto done;
	}

	while (!sed->e.haltreq)
	{
#if defined(HAWK_ENABLE_SED_TRACER)
		if (sed->opt.tracer) sed->opt.tracer (sed, HAWK_SED_TRACER_READ, HAWK_NULL);
#endif

		n = read_line (sed, 0);
		if (n <= -1) { ret = -1; goto done; }
		if (n == 0) goto done;

		if (sed->cmd.fb.len > 0)
		{
			/* the first command block contains at least 1 command
			 * to execute. an empty script like ' ' has no commands,
			 * so we execute no commands */

			hawk_sed_cmd_t* c, * j;

		again:
			c = &sed->cmd.fb.buf[0];

			while (c != &sed->cmd.over)
			{
#if defined(HAWK_ENABLE_SED_TRACER)
				if (sed->opt.tracer) sed->opt.tracer (sed, HAWK_SED_TRACER_MATCH, c);
#endif

				n = match_address (sed, c);
				if (n <= -1) { ret = -1; goto done; }
	
				if (c->negated) n = !n;
				if (n == 0)
				{
					c = c->state.next;
					continue;
				}

#if defined(HAWK_ENABLE_SED_TRACER)
				if (sed->opt.tracer) sed->opt.tracer (sed, HAWK_SED_TRACER_EXEC, c);
#endif
				j = exec_cmd (sed, c);
				if (j == HAWK_NULL) { ret = -1; goto done; }
				if (j == &sed->cmd.quit_quiet) goto done;
				if (j == &sed->cmd.quit) 
				{ 
					if (emit_output (sed, 0) <= -1) ret = -1;
					goto done;
				}
				if (sed->e.haltreq) goto done;
				if (j == &sed->cmd.again) goto again;

				/* go to the next command */
				c = j;
			}
		}

#if defined(HAWK_ENABLE_SED_TRACER)
		if (sed->opt.tracer) sed->opt.tracer (sed, HAWK_SED_TRACER_WRITE, HAWK_NULL);
#endif
		if (emit_output (sed, 0) <= -1) { ret = -1; goto done; }
	}

done:
	hawk_map_clear (&sed->e.out.files);
	sed->e.out.fun (sed, HAWK_SED_IO_CLOSE, &sed->e.out.arg, HAWK_NULL, 0);
done2:
	sed->e.in.fun (sed, HAWK_SED_IO_CLOSE, &sed->e.in.arg, HAWK_NULL, 0);
done3:
	hawk_ooecs_fini (&sed->e.in.line);
	hawk_map_fini (&sed->e.out.files);
	return ret;
}

void hawk_sed_halt (hawk_sed_t* sed)
{
	sed->e.haltreq = 1;
}

int hawk_sed_ishalt (hawk_sed_t* sed)
{
	return sed->e.haltreq;
}

const hawk_ooch_t* hawk_sed_getcompid (hawk_sed_t* sed)
{
	return sed->src.cid? ((const hawk_ooch_t*)(sed->src.cid + 1)): HAWK_NULL;
}

const hawk_ooch_t* hawk_sed_setcompid (hawk_sed_t* sed, const hawk_ooch_t* id)
{
	hawk_sed_cid_t* cid;
	hawk_oow_t len;
	
	if (sed->src.cid == (hawk_sed_cid_t*)&sed->src.unknown_cid) 
	{
		/* if an error has occurred in a previously, you can't set it
		 * any more */
		return (const hawk_ooch_t*)(sed->src.cid + 1);
	}

	if (id == HAWK_NULL) id = HAWK_T("");

	len = hawk_count_oocstr(id);
	cid = hawk_sed_allocmem(sed, HAWK_SIZEOF(*cid) + ((len + 1) * HAWK_SIZEOF(*id)));
	if (cid == HAWK_NULL) 
	{
		/* mark that an error has occurred */ 
		sed->src.unknown_cid.buf[0] = HAWK_T('\0');
		cid = (hawk_sed_cid_t*)&sed->src.unknown_cid;
	}
	else
	{
		hawk_copy_oocstr_unlimited ((hawk_ooch_t*)(cid + 1), id);
	}

	cid->next = sed->src.cid;
	sed->src.cid = cid;
	return (const hawk_ooch_t*)(cid + 1);
}

hawk_oow_t hawk_sed_getlinenum (hawk_sed_t* sed)
{
	return sed->e.in.num;
}

void hawk_sed_setlinenum (hawk_sed_t* sed, hawk_oow_t num)
{
	sed->e.in.num = num;
}

hawk_sed_ecb_t* hawk_sed_popecb (hawk_sed_t* sed)
{
	hawk_sed_ecb_t* top = sed->ecb;
	if (top) sed->ecb = top->next;
	return top;
}

void hawk_sed_pushecb (hawk_sed_t* sed, hawk_sed_ecb_t* ecb)
{
	ecb->next = sed->ecb;
	sed->ecb = ecb;
}

void* hawk_sed_allocmem (hawk_sed_t* sed, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(hawk_sed_getmmgr(sed), size);
	if (ptr == HAWK_NULL) hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_sed_callocmem (hawk_sed_t* sed, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(hawk_sed_getmmgr(sed), size);
	if (ptr) HAWK_MEMSET (ptr, 0, size);
	else hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_sed_reallocmem (hawk_sed_t* sed, void* ptr, hawk_oow_t size)
{
	void* nptr = HAWK_MMGR_REALLOC(hawk_sed_getmmgr(sed), ptr, size);
	if (nptr == HAWK_NULL) hawk_sed_seterrnum (sed, HAWK_NULL, HAWK_ENOMEM);
	return nptr;
}

void hawk_sed_freemem (hawk_sed_t* sed, void* ptr)
{
	HAWK_MMGR_FREE (hawk_sed_getmmgr(sed), ptr);
}


void hawk_sed_getspace (hawk_sed_t* sed, hawk_sed_space_t space, hawk_oocs_t* str)
{
	switch (space)
	{
		case HAWK_SED_SPACE_HOLD:
			str->ptr = HAWK_OOECS_PTR(&sed->e.txt.hold);
			str->len = HAWK_OOECS_LEN(&sed->e.txt.hold);
			break;
		case HAWK_SED_SPACE_PATTERN:
			str->ptr = HAWK_OOECS_PTR(&sed->e.in.line);
			str->len = HAWK_OOECS_LEN(&sed->e.in.line);
			break;
	}
}
