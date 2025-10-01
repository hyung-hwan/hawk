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

#include "cut-prv.h"
#include "hawk-prv.h"
#include <hawk-chr.h>

static int hawk_cut_init (hawk_cut_t* cut, hawk_mmgr_t* mmgr, hawk_cmgr_t* cmgr);
static void hawk_cut_fini (hawk_cut_t* cut);

#define SETERR0(cut,num) \
	do { hawk_cut_seterrnum (cut, HAWK_NULL, num); } while (0)

#define DFL_LINE_CAPA 256

static int add_selector_block (hawk_cut_t* cut)
{
	hawk_cut_sel_blk_t* b;

	b = (hawk_cut_sel_blk_t*)hawk_cut_allocmem(cut, HAWK_SIZEOF(*b));
	if (HAWK_UNLIKELY(!b)) return -1;

	HAWK_MEMSET(b, 0, HAWK_SIZEOF(*b));
	b->next = HAWK_NULL;
	b->len = 0;

	cut->sel.lb->next = b;
	cut->sel.lb = b;
	cut->sel.count = 0;
	cut->sel.fcount = 0;
	cut->sel.ccount = 0;

	return 0;
}

static void free_all_selector_blocks (hawk_cut_t* cut)
{
	hawk_cut_sel_blk_t* b;

	for (b = cut->sel.fb.next; b != HAWK_NULL; )
	{
		hawk_cut_sel_blk_t* nxt = b->next;
		hawk_cut_freemem(cut, b);
		b = nxt;
	}

	cut->sel.lb = &cut->sel.fb;
	cut->sel.lb->len = 0;
	cut->sel.lb->next = HAWK_NULL;
	cut->sel.count = 0;
	cut->sel.fcount = 0;
	cut->sel.ccount = 0;
}

hawk_cut_t* hawk_cut_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	hawk_cut_t* cut;

	cut = (hawk_cut_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(hawk_cut_t) + xtnsize);
	if (HAWK_LIKELY(cut))
	{
		if (hawk_cut_init(cut, mmgr, cmgr) <= -1)
		{
			if (errinf) hawk_cut_geterrinf(cut, errinf);
			HAWK_MMGR_FREE (mmgr, cut);
			cut = HAWK_NULL;
		}
		else HAWK_MEMSET(cut + 1, 0, xtnsize);
	}
	else if (errinf)
	{
		HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
		errinf->num = HAWK_ENOMEM;
		hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(errinf->num));
	}

	return cut;
}

void hawk_cut_close (hawk_cut_t* cut)
{
	hawk_cut_fini (cut);
	HAWK_MMGR_FREE (hawk_cut_getmmgr(cut), cut);
}

static int hawk_cut_init (hawk_cut_t* cut, hawk_mmgr_t* mmgr, hawk_cmgr_t* cmgr)
{
	HAWK_MEMSET (cut, 0, HAWK_SIZEOF(*cut));

	cut->_instsize = HAWK_SIZEOF(*cut);
	cut->_gem.mmgr = mmgr;
	cut->_gem.cmgr = cmgr;

	/* initialize error handling fields */
	cut->_gem.errnum = HAWK_ENOERR;
	cut->_gem.errmsg[0] = '\0';
	cut->_gem.errloc.line = 0;
	cut->_gem.errloc.colm = 0;
	cut->_gem.errloc.file = HAWK_NULL;
	cut->_gem.errstr = hawk_dfl_errstr;


	/* on init, the last points to the first */
	cut->sel.lb = &cut->sel.fb;
	/* the block has no data yet */
	cut->sel.fb.len = 0;

	cut->e.in.cflds = HAWK_COUNTOF(cut->e.in.sflds);
	cut->e.in.flds = cut->e.in.sflds;

	if (hawk_ooecs_init(&cut->e.in.line, hawk_cut_getgem(cut), DFL_LINE_CAPA) <= -1) return  -1;

	return 0;
}

static void hawk_cut_fini (hawk_cut_t* cut)
{
	free_all_selector_blocks (cut);
	if (cut->e.in.flds != cut->e.in.sflds) hawk_cut_freemem(cut, cut->e.in.flds);
	hawk_ooecs_fini(&cut->e.in.line);
}

hawk_errstr_t hawk_cut_geterrstr (hawk_cut_t* cut)
{
	return cut->_gem.errstr;
}

void hawk_cut_seterrbfmt (hawk_cut_t* cut, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hawk_gem_seterrbvfmt (hawk_cut_getgem(cut), errloc, errnum, fmt, ap);
	va_end (ap);
}

void hawk_cut_seterrufmt (hawk_cut_t* cut, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hawk_gem_seterruvfmt (hawk_cut_getgem(cut), errloc, errnum, fmt, ap);
	va_end (ap);
}

void hawk_cut_seterrbvfmt (hawk_cut_t* cut, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, va_list ap)
{
	hawk_gem_seterrbvfmt (hawk_cut_getgem(cut), errloc, errnum, errfmt, ap);
}

void hawk_cut_seterruvfmt (hawk_cut_t* cut, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, va_list ap)
{
	hawk_gem_seterruvfmt (hawk_cut_getgem(cut), errloc, errnum, errfmt, ap);
}

void hawk_cut_setoption (hawk_cut_t* cut, int option)
{
	cut->option = option;
}

int hawk_cut_getoption (hawk_cut_t* cut)
{
	return cut->option;
}

void hawk_cut_clear (hawk_cut_t* cut)
{
	free_all_selector_blocks (cut);
	if (cut->e.in.flds != cut->e.in.sflds) hawk_cut_freemem(cut, cut->e.in.flds);
	cut->e.in.cflds = HAWK_COUNTOF(cut->e.in.sflds);
	cut->e.in.flds = cut->e.in.sflds;

	hawk_ooecs_clear(&cut->e.in.line);
	hawk_ooecs_setcapa(&cut->e.in.line, DFL_LINE_CAPA);
}

/* ---------------------------------------------------------------- */

#define CURSC(cut) ((cut)->src.cc)
#define NXTSC(cut,c,errret) \
	do { if (getnextsc(cut,&(c)) <= -1) return (errret); } while (0)
#define NXTSC_GOTO(cut,c,label) \
	do { if (getnextsc(cut,&(c)) <= -1) goto label; } while (0)
#define PEEPNXTSC(cut,c,errret) \
	do { if (peepnextsc(cut,&(c)) <= -1) return (errret); } while (0)

static int open_script_stream (hawk_cut_t* cut)
{
	hawk_ooi_t n;

	n = cut->src.fun(cut, HAWK_CUT_IO_OPEN, &cut->src.arg, HAWK_NULL, 0);
	if (n <= -1) return -1;

	cut->src.cur = cut->src.buf;
	cut->src.end = cut->src.buf;
	cut->src.cc  = HAWK_OOCI_EOF;
	cut->src.loc.line = 1;
	cut->src.loc.colm = 0;

	cut->src.eof = 0;
	return 0;
}

static HAWK_INLINE int close_script_stream (hawk_cut_t* cut)
{
	return cut->src.fun(cut, HAWK_CUT_IO_CLOSE, &cut->src.arg, HAWK_NULL, 0);
}

static int read_script_stream (hawk_cut_t* cut)
{
	hawk_ooi_t n;

	n = cut->src.fun(cut, HAWK_CUT_IO_READ, &cut->src.arg, cut->src.buf, HAWK_COUNTOF(cut->src.buf));
	if (n <= -1) return -1; /* error */

	if (n == 0)
	{
		/* don't change cut->src.cur and cut->src.end.
		 * they remain the same on eof  */
		cut->src.eof = 1;
		return 0; /* eof */
	}

	cut->src.cur = cut->src.buf;
	cut->src.end = cut->src.buf + n;
	return 1; /* read something */
}

static int getnextsc (hawk_cut_t* cut, hawk_ooci_t* c)
{
	/* adjust the line and column number of the next
	 * character bacut on the current character */
	if (cut->src.cc == HAWK_T('\n'))
	{
		/* TODO: support different line end convension */
		cut->src.loc.line++;
		cut->src.loc.colm = 1;
	}
	else
	{
		/* take note that if you keep on calling getnextsc()
		 * after HAWK_OOCI_EOF is read, this column number
		 * keeps increasing also. there should be a bug of
		 * reading more than necessary somewhere in the code
		 * if this happens. */
		cut->src.loc.colm++;
	}

	if (cut->src.cur >= cut->src.end && !cut->src.eof)
	{
		/* read in more character if buffer is empty */
		if (read_script_stream(cut) <= -1) return -1;
	}

	cut->src.cc = (cut->src.cur < cut->src.end)? (*cut->src.cur++): HAWK_OOCI_EOF;

	*c = cut->src.cc;
	return 0;
}

static int peepnextsc (hawk_cut_t* cut, hawk_ooci_t* c)
{
	if (cut->src.cur >= cut->src.end && !cut->src.eof)
	{
		/* read in more character if buffer is empty.
		 * it is ok to fill the buffer in the peeping
		 * function if it doesn't change cut->src.cc. */
		if (read_script_stream (cut) <= -1) return -1;
	}

	/* no changes in line nubmers, the 'cur' pointer, and
	 * most importantly 'cc' unlike getnextsc(). */
	*c = (cut->src.cur < cut->src.end)?  (*cut->src.cur): HAWK_OOCI_EOF;
	return 0;
}

/* ---------------------------------------------------------------- */

int hawk_cut_comp (hawk_cut_t* cut, hawk_cut_io_impl_t inf)
{
	hawk_ooci_t c;
	hawk_ooci_t sel = HAWK_CUT_SEL_CHAR;

	if (inf == HAWK_NULL)
	{
		hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

#define EOF(x) ((x) == HAWK_OOCI_EOF)
#define MASK_START (1 << 1)
#define MASK_END (1 << 2)
#define MAX HAWK_TYPE_MAX(hawk_oow_t)

	/* free selector blocks compiled previously */
	free_all_selector_blocks(cut);

	/* set the default delimiters */
	cut->sel.din = HAWK_T(' ');
	cut->sel.dout = HAWK_T(' ');

        /* open script */
        cut->src.fun = inf;
        if (open_script_stream(cut) <= -1) return -1;
        NXTSC_GOTO(cut, c, oops);

	while (1)
	{
		hawk_oow_t start = 0, end = 0;
		int mask = 0;

		while (hawk_is_ooch_space(c)) NXTSC_GOTO(cut, c, oops);
		if (EOF(c)) break;

		if (c == HAWK_T('d'))
		{
			/* the next character is the input delimiter.
			 * the output delimiter defaults to the input
			 * delimiter. */
			NXTSC_GOTO(cut, c, oops);
			if (EOF(c))
			{
				SETERR0(cut, HAWK_CUT_ESELNV);
				return -1;
			}
			cut->sel.din = c;
			cut->sel.dout = c;

			NXTSC_GOTO(cut, c, oops);
		}
		else if (c == HAWK_T('D'))
		{
			/* the next two characters are the input and
			 * the output delimiter each. */
			NXTSC_GOTO(cut, c, oops);
			if (EOF(c))
			{
				SETERR0(cut, HAWK_CUT_ESELNV);
				return -1;
			}
			cut->sel.din = c;

			NXTSC_GOTO(cut, c, oops);
			if (EOF(c))
			{
				SETERR0(cut, HAWK_CUT_ESELNV);
				return -1;
			}
			cut->sel.dout = c;

			NXTSC_GOTO(cut, c, oops);
		}
		else
		{
			if (c == HAWK_T('c') || c == HAWK_T('f'))
			{
				sel = c;
				NXTSC_GOTO(cut, c, oops);
				while (hawk_is_ooch_space(c)) NXTSC_GOTO(cut, c, oops);
			}

			if (hawk_is_ooch_digit(c))
			{
				do
				{
					start = start * 10 + (c - HAWK_T('0'));
					NXTSC_GOTO(cut, c, oops);
				}
				while (hawk_is_ooch_digit(c));

				while (hawk_is_ooch_space(c)) NXTSC_GOTO(cut, c, oops);
				mask |= MASK_START;
			}
			else start++;

			if (c == HAWK_T('-'))
			{
				NXTSC_GOTO(cut, c, oops);
				while (hawk_is_ooch_space(c)) NXTSC_GOTO(cut, c, oops);

				if (hawk_is_ooch_digit(c))
				{
					do
					{
						end = end * 10 + (c - HAWK_T('0'));
						NXTSC_GOTO(cut, c, oops);
					}
					while (hawk_is_ooch_digit(c));
					mask |= MASK_END;
				}
				else end = MAX;

				while (hawk_is_ooch_space(c)) NXTSC_GOTO(cut, c, oops);
			}
			else end = start;

			if (!(mask & (MASK_START | MASK_END)))
			{
				SETERR0(cut, HAWK_CUT_ESELNV);
				return -1;
			}

			if (cut->sel.lb->len >= HAWK_COUNTOF(cut->sel.lb->range))
			{
				if (add_selector_block(cut) <= -1) return -1;
			}

			cut->sel.lb->range[cut->sel.lb->len].id = sel;
			cut->sel.lb->range[cut->sel.lb->len].start = start;
			cut->sel.lb->range[cut->sel.lb->len].end = end;
			cut->sel.lb->len++;
			cut->sel.count++;
			if (sel == HAWK_CUT_SEL_FIELD) cut->sel.fcount++;
			else cut->sel.ccount++;
		}

		if (EOF(c)) break;
		if (c == HAWK_T(',')) NXTSC_GOTO(cut, c, oops);
	}

	return 0;

oops:
	close_script_stream(cut);
	return -1;
}

static int read_char (hawk_cut_t* cut, hawk_ooch_t* c)
{
	hawk_ooi_t n;

	if (cut->e.in.pos >= cut->e.in.len)
	{
		n = cut->e.in.fun(cut, HAWK_CUT_IO_READ, &cut->e.in.arg, cut->e.in.buf, HAWK_COUNTOF(cut->e.in.buf));
		if (n <= -1) return -1;
		if (n == 0) return 0; /* end of file */

		cut->e.in.len = n;
		cut->e.in.pos = 0;
	}

	*c = cut->e.in.buf[cut->e.in.pos++];
	return 1;
}

static int read_line (hawk_cut_t* cut)
{
	hawk_oow_t len = 0;
	hawk_ooch_t c;
	int n;

	hawk_ooecs_clear(&cut->e.in.line);
	if (cut->e.in.eof) return 0;

	while (1)
	{
		n = read_char(cut, &c);
		if (n <= -1) return -1;
		if (n == 0)
		{
			cut->e.in.eof = 1;
			if (len == 0) return 0;
			break;
		}

		if (c == HAWK_T('\n'))
		{
			/* don't include the line terminater to a line */
			/* TODO: support different line end convension */
			break;
		}

		if (hawk_ooecs_ccat(&cut->e.in.line, c) == (hawk_oow_t)-1) return -1;
		len++;
	}

	cut->e.in.num++;

	if (cut->option & HAWK_CUT_TRIMSPACE) hawk_ooecs_trim(&cut->e.in.line, HAWK_TRIM_LEFT | HAWK_TRIM_RIGHT);
	if (cut->option & HAWK_CUT_NORMSPACE) hawk_ooecs_compact(&cut->e.in.line);
	return 1;
}

static int flush (hawk_cut_t* cut)
{
	hawk_oow_t pos = 0;
	hawk_ooi_t n;

	while (cut->e.out.len > 0)
	{
		n = cut->e.out.fun(cut, HAWK_CUT_IO_WRITE, &cut->e.out.arg, &cut->e.out.buf[pos], cut->e.out.len);
		if (n <= -1) return -1;
		if (n == 0) return -1; /* reached the end of file - this is also an error */

		pos += n;
		cut->e.out.len -= n;
	}

	return 0;
}

static int write_char (hawk_cut_t* cut, hawk_ooch_t c)
{
	cut->e.out.buf[cut->e.out.len++] = c;
	if (c == HAWK_T('\n') || cut->e.out.len >= HAWK_COUNTOF(cut->e.out.buf)) return flush(cut);
	return 0;
}

static int write_linebreak (hawk_cut_t* cut)
{
	/* TODO: different line termination convention */
	return write_char(cut, HAWK_T('\n'));
}

static int write_str (hawk_cut_t* cut, const hawk_ooch_t* str, hawk_oow_t len)
{
	hawk_oow_t i;

	for (i = 0; i < len; i++)
	{
		if (write_char(cut, str[i]) <= -1) return -1;
	}

	return 0;
}

static int cut_chars (hawk_cut_t* cut, hawk_oow_t start, hawk_oow_t end, int delim)
{
	const hawk_ooch_t* ptr = HAWK_OOECS_PTR(&cut->e.in.line);
	hawk_oow_t len = HAWK_OOECS_LEN(&cut->e.in.line);

	if (len <= 0) return 0;

	if (start <= end)
	{
		if (start <= len && end > 0)
		{
			if (start >= 1) start--;
			if (end >= 1) end--;

			if (end >= len) end = len - 1;

			if (delim && write_char(cut, cut->sel.dout) <= -1) return -1;
			if (write_str(cut, &ptr[start], end-start+1) <= -1) return -1;

			return 1;
		}
	}
	else
	{
		if (start > 0 && end <= len)
		{
			hawk_oow_t i;

			if (start >= 1) start--;
			if (end >= 1) end--;

			if (start >= len) start = len - 1;

			if (delim && write_char (cut, cut->sel.dout) <= -1)
				return -1;

			for (i = start; i >= end; i--)
			{
				if (write_char (cut, ptr[i]) <= -1)
					return -1;
			}

			return 1;
		}
	}

	return 0;
}

static int isdelim (hawk_cut_t* cut, hawk_ooch_t c)
{
	return ((cut->option & HAWK_CUT_WHITESPACE) && hawk_is_ooch_space(c)) ||
	        (!(cut->option & HAWK_CUT_WHITESPACE) && c == cut->sel.din);
}

static int split_line (hawk_cut_t* cut)
{
	const hawk_ooch_t* ptr = HAWK_OOECS_PTR(&cut->e.in.line);
	hawk_oow_t len = HAWK_OOECS_LEN(&cut->e.in.line);
	hawk_oow_t i, x = 0, xl = 0;

	cut->e.in.delimited = 0;
	cut->e.in.flds[x].ptr = ptr;
	for (i = 0; i < len; )
	{
		hawk_ooch_t c = ptr[i++];
		if (isdelim(cut,c))
		{
			if (cut->option & HAWK_CUT_FOLDDELIMS)
			{
				while (i < len && isdelim(cut,ptr[i])) i++;
			}

			cut->e.in.flds[x++].len = xl;

			if (x >= cut->e.in.cflds)
			{
				hawk_oocs_t* tmp;
				hawk_oow_t nsz;

				nsz = cut->e.in.cflds;
				if (nsz > 100000) nsz += 100000;
				else nsz *= 2;

				tmp = hawk_cut_allocmem(cut, HAWK_SIZEOF(*tmp) * nsz);
				if (HAWK_UNLIKELY(!tmp)) return -1;

				HAWK_MEMCPY (tmp, cut->e.in.flds, HAWK_SIZEOF(*tmp) * cut->e.in.cflds);

				if (cut->e.in.flds != cut->e.in.sflds) hawk_cut_freemem(cut, cut->e.in.flds);
				cut->e.in.flds = tmp;
				cut->e.in.cflds = nsz;
			}

			xl = 0;
			cut->e.in.flds[x].ptr = &ptr[i];
			cut->e.in.delimited = 1;
		}
		else xl++;
	}
	cut->e.in.flds[x].len = xl;
	cut->e.in.nflds = ++x;
	return 0;
}

static int cut_fields (hawk_cut_t* cut, hawk_oow_t start, hawk_oow_t end, int delim)
{
	hawk_oow_t len = cut->e.in.nflds;

	if (!cut->e.in.delimited /*|| len <= 0*/) return 0;

	HAWK_ASSERT (len > 0);
	if (start <= end)
	{
		if (start <= len && end > 0)
		{
			hawk_oow_t i;

			if (start >= 1) start--;
			if (end >= 1) end--;

			if (end >= len) end = len - 1;

			if (delim && write_char(cut, cut->sel.dout) <= -1) return -1;

			for (i = start; i <= end; i++)
			{
				if (write_str(cut, cut->e.in.flds[i].ptr, cut->e.in.flds[i].len) <= -1) return -1;
				if (i < end && write_char(cut, cut->sel.dout) <= -1) return -1;
			}

			return 1;
		}
	}
	else
	{
		if (start > 0 && end <= len)
		{
			hawk_oow_t i;

			if (start >= 1) start--;
			if (end >= 1) end--;

			if (start >= len) start = len - 1;

			if (delim && write_char (cut, cut->sel.dout) <= -1)
				return -1;

			for (i = start; i >= end; i--)
			{
				if (write_str(cut, cut->e.in.flds[i].ptr, cut->e.in.flds[i].len) <= -1) return -1;
				if (i > end && write_char (cut, cut->sel.dout) <= -1) return -1;
			}

			return 1;
		}
	}

	return 0;
}

int hawk_cut_exec (hawk_cut_t* cut, hawk_cut_io_impl_t inf, hawk_cut_io_impl_t outf)
{
	int ret = 0;
	hawk_ooi_t n;

	cut->e.out.fun = outf;
	cut->e.out.eof = 0;
	cut->e.out.len = 0;

	cut->e.in.fun = inf;
	cut->e.in.eof = 0;
	cut->e.in.len = 0;
	cut->e.in.pos = 0;
	cut->e.in.num = 0;

	n = cut->e.in.fun(cut, HAWK_CUT_IO_OPEN, &cut->e.in.arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		ret = -1;
		goto done3;
	}
	if (n == 0)
	{
		/* EOF reached upon opening an input stream.
		* no data to process. this is success */
		goto done2;
	}

	n = cut->e.out.fun(cut, HAWK_CUT_IO_OPEN, &cut->e.out.arg, HAWK_NULL, 0);
	if (n <= -1)
	{
		ret = -1;
		goto done2;
	}
	if (n == 0)
	{
		/* still don't know if we will write something.
		 * just mark EOF on the output stream and continue */
		cut->e.out.eof = 1;
	}

	while (1)
	{
		hawk_cut_sel_blk_t* b;
		int id = 0; /* mark 'no output' so far */
		int delimited = 0;
		int linebreak = 0;

		n = read_line (cut);
		if (n <= -1) { ret = -1; goto done; }
		if (n == 0) goto done;

		if (cut->sel.fcount > 0)
		{
			if (split_line (cut) <= -1) { ret = -1; goto done; }
			delimited = cut->e.in.delimited;
		}

		for (b = &cut->sel.fb; b != HAWK_NULL; b = b->next)
		{
			hawk_oow_t i;

			for (i = 0; i < b->len; i++)
			{
				if (b->range[i].id == HAWK_CUT_SEL_CHAR)
				{
					n = cut_chars (
						cut,
						b->range[i].start,
						b->range[i].end,
						id == 2
					);
					if (n >= 1)
					{
						/* mark a char's been output */
						id = 1;
					}
				}
				else
				{
					n = cut_fields (
						cut,
						b->range[i].start,
						b->range[i].end,
						id > 0
					);
					if (n >= 1)
					{
						/* mark a field's been output */
						id = 2;
					}
				}

				if (n <= -1) { ret = -1; goto done; }
			}
		}

		if (cut->sel.ccount > 0)
		{
			/* so long as there is a character selector,
			 * a newline must be printed */
			linebreak = 1;
		}
		else if (cut->sel.fcount > 0)
		{
			/* if only field selectors are specified */

			if (delimited)
			{
				/* and if the input line is delimited,
				 * write a line break */
				linebreak = 1;
			}
			else if (!(cut->option & HAWK_CUT_DELIMONLY))
			{
				/* if not delimited, write the
				 * entire undelimited input line depending
				 * on the option set. */
				if (write_str(cut, HAWK_OOECS_PTR(&cut->e.in.line), HAWK_OOECS_LEN(&cut->e.in.line)) <= -1)
				{
					ret = -1; goto done;
				}

				/* a line break is needed in this case */
				linebreak = 1;
			}
		}

		if (linebreak && write_linebreak(cut) <= -1)
		{
			ret = -1; goto done;
		}
	}

done:
	cut->e.out.fun(cut, HAWK_CUT_IO_CLOSE, &cut->e.out.arg, HAWK_NULL, 0);
done2:
	cut->e.in.fun(cut, HAWK_CUT_IO_CLOSE, &cut->e.in.arg, HAWK_NULL, 0);
done3:
	return ret;
}


/* -------------------------------------- */

void* hawk_cut_allocmem (hawk_cut_t* cut, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(hawk_cut_getmmgr(cut), size);
	if (HAWK_UNLIKELY(!ptr)) hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_cut_callocmem (hawk_cut_t* cut, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(hawk_cut_getmmgr(cut), size);
	if (HAWK_LIKELY(ptr)) HAWK_MEMSET(ptr, 0, size);
	else hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_cut_reallocmem (hawk_cut_t* cut, void* ptr, hawk_oow_t size)
{
	void* nptr = HAWK_MMGR_REALLOC(hawk_cut_getmmgr(cut), ptr, size);
	if (HAWK_UNLIKELY(!nptr)) hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_ENOMEM);
	return nptr;
}

void hawk_cut_freemem (hawk_cut_t* cut, void* ptr)
{
	HAWK_MMGR_FREE(hawk_cut_getmmgr(cut), ptr);
}
