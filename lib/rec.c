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

static int split_record (hawk_rtx_t* run, int prefer_number);
static int recomp_record_fields (hawk_rtx_t* run, hawk_oow_t lv, const hawk_oocs_t* str, int prefer_number);

int hawk_rtx_setrec (hawk_rtx_t* rtx, hawk_oow_t idx, const hawk_oocs_t* str, int prefer_number)
{
	hawk_val_t* v;

	if (idx == 0)
	{
		if (str->ptr == HAWK_OOECS_PTR(&rtx->inrec.line) &&
		    str->len == HAWK_OOECS_LEN(&rtx->inrec.line))
		{
			if (hawk_rtx_clrrec(rtx, 1) <= -1) return -1;
		}
		else
		{
			if (hawk_rtx_clrrec(rtx, 0) <= -1) return -1;
			if (hawk_ooecs_ncpy(&rtx->inrec.line, str->ptr, str->len) == (hawk_oow_t)-1) goto oops;
		}

		if (split_record(rtx, prefer_number) <= -1) goto oops;

		v = prefer_number? hawk_rtx_makenumorstrvalwithoochars(rtx, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line)):  /* number or string */
		                   hawk_rtx_makenstrvalwithoochars(rtx, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line));  /* str with nstr flag */

		if (HAWK_UNLIKELY(!v)) goto oops;
	}
	else
	{
		if (recomp_record_fields(rtx, idx, str, prefer_number) <= -1) goto oops;

		/* recompose $0 */
		v = hawk_rtx_makestrvalwithoochars(rtx, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line));
		if (HAWK_UNLIKELY(!v)) goto oops;
	}

	if (HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.d0) != HAWK_VAL_NIL) hawk_rtx_refdownval (rtx, rtx->inrec.d0);
	rtx->inrec.d0 = v;
	hawk_rtx_refupval (rtx, v);

	return 0;

oops:
	hawk_rtx_clrrec (rtx, 0);
	return -1;
}

#if 0
static int merge_fields (hawk_rtx_t* rtx)
{
	hawk_oow_t i;

	hawk_ooecs_clear (&rtx->inrec.line);

	for (i = 0; i < rtx->inrec.nflds; i++)
	{
		hawk_ooch_t* vp;
		hawk_oow_t vl;

		if (HAWK_LIKELY(i > 0))
		{
			if (hawk_ooecs_ncat(&rtx->inrec.line, rtx->gbl.ofs.ptr, rtx->gbl.ofs.len) == (hawk_oow_t)-1) return -1;
		}

		vp = hawk_rtx_getvaloocstr(rtx, rtx->inrec.flds[i].val, &vl);
		if (HAWK_UNLIKELY(!vp)) return -1;

		vl = hawk_ooecs_ncat(&rtx->inrec.line, vp, vl);
		hawk_rtx_freevaloocstr (rtx, rtx->inrec.flds[i].val, vp);

		if (HAWK_UNLIKELY(vl == (hawk_oow_t)-1)) return -1;
	}

	return 1;
}
#endif

static int split_record (hawk_rtx_t* rtx, int prefer_number)
{
	hawk_oocs_t tok;
	hawk_ooch_t* p, * px;
	hawk_oow_t len, nflds;
	hawk_val_t* v, * fs;
	hawk_val_type_t fsvtype;
	hawk_ooch_t* fs_ptr, * fs_free;
	hawk_oow_t fs_len;
	int how;

	/* inrec should be cleared before split_record is called */
	HAWK_ASSERT (rtx->inrec.nflds == 0);

	/* get FS */
	fs = hawk_rtx_getgbl(rtx, HAWK_GBL_FS);
	fsvtype = HAWK_RTX_GETVALTYPE (rtx, fs);
	if (fsvtype == HAWK_VAL_NIL)
	{
		fs_ptr = HAWK_T(" ");
		fs_len = 1;
		fs_free = HAWK_NULL;
	}
	else if (fsvtype == HAWK_VAL_STR)
	{
		fs_ptr = ((hawk_val_str_t*)fs)->val.ptr;
		fs_len = ((hawk_val_str_t*)fs)->val.len;
		fs_free = HAWK_NULL;
	}
	else
	{
		fs_ptr = hawk_rtx_valtooocstrdup(rtx, fs, &fs_len);
		if (HAWK_UNLIKELY(!fs_ptr)) return -1;
		fs_free = fs_ptr;
	}

	/* scan the input record to count the fields */
	if (fs_len == 5 && fs_ptr[0] ==  HAWK_T('?'))
	{
		if (hawk_ooecs_ncpy(&rtx->inrec.linew, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line)) == (hawk_oow_t)-1)
		{
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			return -1;
		}

		px = HAWK_OOECS_PTR(&rtx->inrec.linew);
		how = 1;
	}
	else
	{
		px = HAWK_OOECS_PTR(&rtx->inrec.line);
		how = (fs_len <= 1)? 0: 2;
	}

	p = px;
	len = HAWK_OOECS_LEN(&rtx->inrec.line);

#if 0
	nflds = 0;
	while (p != HAWK_NULL)
	{
		switch (how)
		{
			case 0:
				p = hawk_rtx_tokoocharswithoochars (rtx, p, len, fs_ptr, fs_len, &tok);
				break;

			case 1:
				break;

			default:
				p = hawk_rtx_tokoocharsbyrex(
					rtx,
					HAWK_OOECS_PTR(&rtx->inrec.line),
					HAWK_OOECS_LEN(&rtx->inrec.line),
					p, len,
					rtx->gbl.fs[rtx->gbl.ignorecase], &tok
				);
				if (p == HAWK_NULL && hawk_rtx_geterrnum(rtx) != HAWK_ENOERR)
				{
					if (fs_free) hawk_rtx_freemem (rtx, fs_free);
					return -1;
				}
		}

		if (nflds == 0 && p == HAWK_NULL && tok.len == 0)
		{
			/* there are no fields. it can just return here
			 * as hawk_rtx_clrrec has been called before this */
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			return 0;
		}

		HAWK_ASSERT ((tok.ptr != HAWK_NULL && tok.len > 0) || tok.len == 0);

		nflds++;
		len = HAWK_OOECS_LEN(&rtx->inrec.line) - (p - HAWK_OOECS_PTR(&rtx->inrec.line));
	}

	/* allocate space */
	if (nflds > rtx->inrec.maxflds)
	{
		void* tmp = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*rtx->inrec.flds) * nflds);
		if (HAWK_UNLIKELY(!tmp))
		{
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			return -1;
		}

		if (rtx->inrec.flds) hawk_rtx_freemem (rtx, rtx->inrec.flds);
		rtx->inrec.flds = tmp;
		rtx->inrec.maxflds = nflds;
	}

	/* scan again and split it */
	if (how == 1)
	{
		if (hawk_ooecs_ncpy(&rtx->inrec.linew, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line)) == (hawk_oow_t)-1)
		{
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			return -1;
		}
		px = HAWK_OOECS_PTR(&rtx->inrec.linew):
	}
	else
	{
		px = HAWK_OOECS_PTR(&rtx->inrec.line);
	}

	p = px;
	len = HAWK_OOECS_LEN(&rtx->inrec.line);
#endif

	while (p)
	{
		switch (how)
		{
			case 0:
				/* 1 character FS */
				p = hawk_rtx_tokoocharswithoochars(rtx, p, len, fs_ptr, fs_len, &tok);
				break;

			case 1:
				/* 5 character FS beginning with ? */
				p = hawk_rtx_fldoochars(rtx, p, len, fs_ptr[1], fs_ptr[2], fs_ptr[3], fs_ptr[4], &tok);
				break;

			default:
				/* all other cases */
				p = hawk_rtx_tokoocharsbyrex(
					rtx,
					HAWK_OOECS_PTR(&rtx->inrec.line),
					HAWK_OOECS_LEN(&rtx->inrec.line),
					p, len,
					rtx->gbl.fs[rtx->gbl.ignorecase], &tok
				);
				if (p == HAWK_NULL && hawk_rtx_geterrnum(rtx) != HAWK_ENOERR)
				{
					if (fs_free) hawk_rtx_freemem (rtx, fs_free);
					return -1;
				}
		}
#if 1
		if (rtx->inrec.nflds == 0 && p == HAWK_NULL && tok.len == 0)
		{
			/* there are no fields. it can just return here
			 * as hawk_rtx_clrrec has been called before this */
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			return 0;
		}
#endif

		HAWK_ASSERT ((tok.ptr != HAWK_NULL && tok.len > 0) || tok.len == 0);

#if 1
		if (rtx->inrec.nflds >= rtx->inrec.maxflds)
		{
			void* tmp;

			if (rtx->inrec.nflds < 16) nflds = 32;
			else nflds = rtx->inrec.nflds * 2;

			tmp = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*rtx->inrec.flds) * nflds);
			if (tmp == HAWK_NULL)
			{
				if (fs_free) hawk_rtx_freemem (rtx, fs_free);
				return -1;
			}

			if (rtx->inrec.flds != HAWK_NULL)
			{
				HAWK_MEMCPY (tmp, rtx->inrec.flds, HAWK_SIZEOF(*rtx->inrec.flds) * rtx->inrec.nflds);
				hawk_rtx_freemem (rtx, rtx->inrec.flds);
			}

			rtx->inrec.flds = tmp;
			rtx->inrec.maxflds = nflds;
		}
#endif

		rtx->inrec.flds[rtx->inrec.nflds].ptr = tok.ptr;
		rtx->inrec.flds[rtx->inrec.nflds].len = tok.len;
		/*rtx->inrec.flds[rtx->inrec.nflds].val = hawk_rtx_makenstrvalwithoocs(rtx, &tok);*/
		rtx->inrec.flds[rtx->inrec.nflds].val =
			prefer_number? hawk_rtx_makenumorstrvalwithoochars(rtx, tok.ptr, tok.len):
			               hawk_rtx_makestrvalwithoochars(rtx, tok.ptr, tok.len);
		if (HAWK_UNLIKELY(!rtx->inrec.flds[rtx->inrec.nflds].val))
		{
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			return -1;
		}

		hawk_rtx_refupval (rtx, rtx->inrec.flds[rtx->inrec.nflds].val);
		rtx->inrec.nflds++;

		len = HAWK_OOECS_LEN(&rtx->inrec.line) - (p - px);
	}

	if (fs_free) hawk_rtx_freemem (rtx, fs_free);

	/* set the number of fields */
	v = hawk_rtx_makeintval(rtx, (hawk_int_t)rtx->inrec.nflds);
	if (v == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, v);
	if (hawk_rtx_setgbl(rtx, HAWK_GBL_NF, v) <= -1)
	{
		hawk_rtx_refdownval (rtx, v);
		return -1;
	}
	hawk_rtx_refdownval (rtx, v);
	return 0;
}

int hawk_rtx_clrrec (hawk_rtx_t* rtx, int skip_inrec_line)
{
	hawk_oow_t i;
	int n = 0;

	if (HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.d0) != HAWK_VAL_NIL)
	{
		hawk_rtx_refdownval (rtx, rtx->inrec.d0);
		rtx->inrec.d0 = hawk_val_nil;
	}

	if (rtx->inrec.nflds > 0)
	{
		HAWK_ASSERT (rtx->inrec.flds != HAWK_NULL);

		for (i = 0; i < rtx->inrec.nflds; i++)
		{
			HAWK_ASSERT (rtx->inrec.flds[i].val != HAWK_NULL);
			hawk_rtx_refdownval (rtx, rtx->inrec.flds[i].val);
		}
		rtx->inrec.nflds = 0;

		if (hawk_rtx_setgbl(rtx, HAWK_GBL_NF, HAWK_VAL_ZERO) <= -1)
		{
			/* first of all, this should never happen.
			 * if it happened, it would return an error
			 * after all the clearance tasks */
			n = -1;
		}
	}

	HAWK_ASSERT (rtx->inrec.nflds == 0);
	if (!skip_inrec_line) hawk_ooecs_clear (&rtx->inrec.line);

	return n;
}

static int recomp_record_fields (hawk_rtx_t* rtx, hawk_oow_t lv, const hawk_oocs_t* str, int prefer_number)
{
	hawk_val_t* v;
	hawk_oow_t max, i, nflds;

	/* recomposes the record and the fields when $N has been assigned
	 * a new value and recomputes NF accordingly.
	 *
	 * BEGIN { OFS=":" } { $2 = "Q"; print $0; }
	 * If input is abc def xxx, $0 becomes abc:Q:xxx.
	 *
	 * We should store the value in rtx->inrec.line so that the caller
	 * can use it to make a value for $0.
	 */

	HAWK_ASSERT (lv > 0);
	max = (lv > rtx->inrec.nflds)? lv: rtx->inrec.nflds;

	nflds = rtx->inrec.nflds;
	if (max > rtx->inrec.maxflds)
	{
		void* tmp;

		/* if the given field number is greater than the maximum
		 * number of fields that the current record can hold,
		 * the field spaces are resized */

		tmp = hawk_rtx_reallocmem(rtx, rtx->inrec.flds, HAWK_SIZEOF(*rtx->inrec.flds) * max);
		if (HAWK_UNLIKELY(!tmp)) return -1;

		rtx->inrec.flds = tmp;
		rtx->inrec.maxflds = max;
	}

	lv = lv - 1; /* adjust the value to 0-based index */

	hawk_ooecs_clear (&rtx->inrec.line);

	for (i = 0; i < max; i++)
	{
		if (i > 0)
		{
			if (hawk_ooecs_ncat(&rtx->inrec.line, rtx->gbl.ofs.ptr, rtx->gbl.ofs.len) == (hawk_oow_t)-1) return -1;
		}

		if (i == lv)
		{
			hawk_val_t* tmp;

			rtx->inrec.flds[i].ptr = HAWK_OOECS_PTR(&rtx->inrec.line) + HAWK_OOECS_LEN(&rtx->inrec.line);
			rtx->inrec.flds[i].len = str->len;

			if (hawk_ooecs_ncat(&rtx->inrec.line, str->ptr, str->len) == (hawk_oow_t)-1) return -1;

			tmp = prefer_number? hawk_rtx_makenumorstrvalwithoochars(rtx, str->ptr, str->len):
			                     hawk_rtx_makestrvalwithoochars(rtx, str->ptr, str->len);
			if (HAWK_UNLIKELY(!tmp)) return -1;

			if (i < nflds) hawk_rtx_refdownval (rtx, rtx->inrec.flds[i].val);
			else rtx->inrec.nflds++;

			rtx->inrec.flds[i].val = tmp;
			hawk_rtx_refupval (rtx, tmp);
		}
		else if (i >= nflds)
		{
			rtx->inrec.flds[i].ptr = HAWK_OOECS_PTR(&rtx->inrec.line) + HAWK_OOECS_LEN(&rtx->inrec.line);
			rtx->inrec.flds[i].len = 0;

			if (hawk_ooecs_cat(&rtx->inrec.line, HAWK_T("")) == (hawk_oow_t)-1) return -1;

			/* hawk_rtx_refdownval should not be called over
			 * rtx->inrec.flds[i].val as it is not initialized
			 * to any valid values */
			/*hawk_rtx_refdownval (rtx, rtx->inrec.flds[i].val);*/
			rtx->inrec.flds[i].val = hawk_val_zls;
			hawk_rtx_refupval (rtx, hawk_val_zls);
			rtx->inrec.nflds++;
		}
		else
		{
			hawk_ooch_t* vp;
			hawk_oow_t vl;

			vp = hawk_rtx_getvaloocstr(rtx, rtx->inrec.flds[i].val, &vl);
			if (HAWK_UNLIKELY(!vp)) return -1;

			vl = hawk_ooecs_ncat(&rtx->inrec.line, vp, vl);
			hawk_rtx_freevaloocstr (rtx, rtx->inrec.flds[i].val, vp);

			if (HAWK_UNLIKELY(vl == (hawk_oow_t)-1)) return -1;
		}
	}

	v = hawk_rtx_getgbl(rtx, HAWK_GBL_NF);
	HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_INT);

	if (HAWK_RTX_GETINTFROMVAL(rtx, v) != max)
	{
		v = hawk_rtx_makeintval (rtx, (hawk_int_t)max);
		if (v == HAWK_NULL) return -1;

		hawk_rtx_refupval (rtx, v);
		if (hawk_rtx_setgbl(rtx, HAWK_GBL_NF, v) <= -1)
		{
			hawk_rtx_refdownval (rtx, v);
			return -1;
		}
		hawk_rtx_refdownval (rtx, v);
	}

	return 0;
}

int hawk_rtx_truncrec (hawk_rtx_t* rtx, hawk_oow_t nflds)
{
	hawk_val_t* v = HAWK_NULL, * w;
	hawk_ooch_t* ofs_free = HAWK_NULL, * ofs_ptr;
	hawk_oow_t ofs_len, i;
	hawk_val_type_t vtype;
	hawk_ooecs_t tmp;
	int fini_tmp = 0;

	HAWK_ASSERT (nflds <= rtx->inrec.nflds);

	if (hawk_ooecs_init(&tmp, hawk_rtx_getgem(rtx), HAWK_OOECS_LEN(&rtx->inrec.line)) <= -1) goto oops;
	fini_tmp = 1;

	if (nflds > 0)
	{
		if (nflds > 1)
		{
			v = HAWK_RTX_STACK_GBL(rtx, HAWK_GBL_OFS);
			hawk_rtx_refupval (rtx, v);
			vtype = HAWK_RTX_GETVALTYPE(rtx, v);

			if (vtype == HAWK_VAL_NIL)
			{
				/* OFS not set */
				ofs_ptr = HAWK_T(" ");
				ofs_len = 1;
			}
			else
			{
				ofs_ptr = hawk_rtx_getvaloocstr(rtx, v, &ofs_len);
				if (HAWK_UNLIKELY(!ofs_ptr)) goto oops;
				ofs_free = ofs_ptr;
			}
		}

		if (hawk_ooecs_ncat(&tmp, rtx->inrec.flds[0].ptr, rtx->inrec.flds[0].len) == (hawk_oow_t)-1) goto oops;
		for (i = 1; i < nflds; i++)
		{
			if (i > 0 && hawk_ooecs_ncat(&tmp,ofs_ptr,ofs_len) == (hawk_oow_t)-1) goto oops;
			if (hawk_ooecs_ncat(&tmp, rtx->inrec.flds[i].ptr, rtx->inrec.flds[i].len) == (hawk_oow_t)-1) goto oops;
		}

		if (v)
		{
			if (ofs_free) hawk_rtx_freevaloocstr (rtx, v, ofs_free);
			hawk_rtx_refdownval(rtx, v);
			v = HAWK_NULL;
		}
	}

	w = (hawk_val_t*)hawk_rtx_makestrvalwithoocs(rtx, HAWK_OOECS_OOCS(&tmp));
	if (HAWK_UNLIKELY(!w)) goto oops;

	hawk_rtx_refdownval (rtx, rtx->inrec.d0);
	rtx->inrec.d0 = w;
	hawk_rtx_refupval (rtx, rtx->inrec.d0);

	hawk_ooecs_swap (&tmp, &rtx->inrec.line);
	hawk_ooecs_fini (&tmp);
	fini_tmp = 0;

	for (i = nflds; i < rtx->inrec.nflds; i++)
	{
		hawk_rtx_refdownval (rtx, rtx->inrec.flds[i].val);
	}

	rtx->inrec.nflds = nflds;
	return 0;

oops:
	if (fini_tmp) hawk_ooecs_fini (&tmp);
	if (v)
	{
		if (ofs_free) hawk_rtx_freevaloocstr (rtx, v, ofs_free);
		hawk_rtx_refdownval(rtx, v);
	}
	return -1;
}
