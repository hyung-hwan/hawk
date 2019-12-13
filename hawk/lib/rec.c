/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

static int split_record (hawk_rtx_t* run);
static int recomp_record_fields (hawk_rtx_t* run, hawk_oow_t lv, const hawk_oocs_t* str);

int hawk_rtx_setrec (hawk_rtx_t* rtx, hawk_oow_t idx, const hawk_oocs_t* str)
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

			if (hawk_ooecs_ncpy(&rtx->inrec.line, str->ptr, str->len) == (hawk_oow_t)-1)
			{
				hawk_rtx_clrrec (rtx, 0);
				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}
		}

		v = hawk_rtx_makenstrvalwithoocs(rtx, str);
		if (v == HAWK_NULL)
		{
			hawk_rtx_clrrec (rtx, 0);
			return -1;
		}

		HAWK_ASSERT (hawk_rtx_getawk(rtx), HAWK_RTX_GETVALTYPE (rtx, rtx->inrec.d0) == HAWK_VAL_NIL);
		/* d0 should be cleared before the next line is reached
		 * as it doesn't call hawk_rtx_refdownval on rtx->inrec.d0 */
		rtx->inrec.d0 = v;
		hawk_rtx_refupval (rtx, v);

		if (split_record(rtx) <= -1) 
		{
			hawk_rtx_clrrec (rtx, 0);
			return -1;
		}
	}
	else
	{
		if (recomp_record_fields(rtx, idx, str) <= -1)
		{
			hawk_rtx_clrrec (rtx, 0);
			return -1;
		}
	
		/* recompose $0 */
		v = hawk_rtx_makestrvalwithoocs (rtx, HAWK_OOECS_OOCS(&rtx->inrec.line));
		if (v == HAWK_NULL)
		{
			hawk_rtx_clrrec (rtx, 0);
			return -1;
		}

		hawk_rtx_refdownval (rtx, rtx->inrec.d0);
		rtx->inrec.d0 = v;
		hawk_rtx_refupval (rtx, v);
	}

	return 0;
}

static int split_record (hawk_rtx_t* rtx)
{
	hawk_oocs_t tok;
	hawk_ooch_t* p, * px;
	hawk_oow_t len, nflds;
	hawk_val_t* v, * fs;
	hawk_val_type_t fsvtype;
	hawk_ooch_t* fs_ptr, * fs_free;
	hawk_oow_t fs_len;
	hawk_errnum_t errnum;
	int how;
	

	/* inrec should be cleared before split_record is called */
	HAWK_ASSERT (hawk_rtx_getawk(rtx), rtx->inrec.nflds == 0);

	/* get FS */
	fs = hawk_rtx_getgbl (rtx, HAWK_GBL_FS);
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
		if (fs_ptr == HAWK_NULL) return -1;
		fs_free = fs_ptr;
	}

	/* scan the input record to count the fields */
	if (fs_len == 5 && fs_ptr[0] ==  HAWK_T('?'))
	{
		if (hawk_ooecs_ncpy (
			&rtx->inrec.linew, 
			HAWK_OOECS_PTR(&rtx->inrec.line),
			HAWK_OOECS_LEN(&rtx->inrec.line)) == (hawk_oow_t)-1)
		{
			if (fs_free != HAWK_NULL) 
				hawk_rtx_freemem (rtx, fs_free);
			hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
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
				p = hawk_rtx_strxntok (rtx,
					p, len, fs_ptr, fs_len, &tok);
				break;

			case 1:
				break;

			default:
				p = hawk_rtx_strxntokbyrex (
					rtx, 
					HAWK_OOECS_PTR(&rtx->inrec.line),
					HAWK_OOECS_LEN(&rtx->inrec.line),
					p, len, 
					rtx->gbl.fs[rtx->gbl.ignorecase], &tok, &errnum
				); 
				if (p == HAWK_NULL && errnum != HAWK_ENOERR)
				{
					if (fs_free != HAWK_NULL) 
						hawk_rtx_freemem (rtx, fs_free);
					hawk_rtx_seterrnum (rtx, errnum, HAWK_NULL);
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

		HAWK_ASSERT (hawk_rtx_getawk(rtx), (tok.ptr != HAWK_NULL && tok.len > 0) || tok.len == 0);

		nflds++;
		len = HAWK_OOECS_LEN(&rtx->inrec.line) - (p - HAWK_OOECS_PTR(&rtx->inrec.line));
	}

	/* allocate space */
	if (nflds > rtx->inrec.maxflds)
	{
		void* tmp = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*rtx->inrec.flds) * nflds);
		if (!tmp) 
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
		if (hawk_ooecs_ncpy (
			&rtx->inrec.linew, 
			HAWK_OOECS_PTR(&rtx->inrec.line),
			HAWK_OOECS_LEN(&rtx->inrec.line)) == (hawk_oow_t)-1)
		{
			if (fs_free) hawk_rtx_freemem (rtx, fs_free);
			hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
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

	while (p != HAWK_NULL)
	{
		switch (how)
		{
			case 0:
				/* 1 character FS */
				p = hawk_rtx_strxntok (
					rtx, p, len, fs_ptr, fs_len, &tok);
				break;

			case 1:
				/* 5 character FS beginning with ? */
				p = hawk_rtx_strxnfld (
					rtx, p, len, 
					fs_ptr[1], fs_ptr[2],
					fs_ptr[3], fs_ptr[4], &tok);
				break;

			default:
				/* all other cases */
				p = hawk_rtx_strxntokbyrex (
					rtx, 
					HAWK_OOECS_PTR(&rtx->inrec.line),
					HAWK_OOECS_LEN(&rtx->inrec.line),
					p, len,
					rtx->gbl.fs[rtx->gbl.ignorecase], &tok, &errnum
				); 
				if (p == HAWK_NULL && errnum != HAWK_ENOERR)
				{
					if (fs_free != HAWK_NULL) 
						hawk_rtx_freemem (rtx, fs_free);
					hawk_rtx_seterrnum (rtx, errnum, HAWK_NULL);
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

		HAWK_ASSERT (hawk_rtx_getawk(rtx), (tok.ptr != HAWK_NULL && tok.len > 0) || tok.len == 0);

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
		rtx->inrec.flds[rtx->inrec.nflds].val = hawk_rtx_makenstrvalwithoocs (rtx, &tok);

		if (rtx->inrec.flds[rtx->inrec.nflds].val == HAWK_NULL)
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
	if (hawk_rtx_setgbl(rtx, HAWK_GBL_NF, v) == -1) 
	{
		hawk_rtx_refdownval (rtx, v);
		return -1;
	}
	hawk_rtx_refdownval (rtx, v);
	return 0;
}

int hawk_rtx_clrrec (hawk_rtx_t* run, int skip_inrec_line)
{
	hawk_oow_t i;
	int n = 0;

	if (run->inrec.d0 != hawk_val_nil)
	{
		hawk_rtx_refdownval (run, run->inrec.d0);
		run->inrec.d0 = hawk_val_nil;
	}

	if (run->inrec.nflds > 0)
	{
		HAWK_ASSERT (hawk_rtx_getawk(rtx), run->inrec.flds != HAWK_NULL);

		for (i = 0; i < run->inrec.nflds; i++) 
		{
			HAWK_ASSERT (hawk_rtx_getawk(rtx), run->inrec.flds[i].val != HAWK_NULL);
			hawk_rtx_refdownval (run, run->inrec.flds[i].val);
		}
		run->inrec.nflds = 0;

		if (hawk_rtx_setgbl (
			run, HAWK_GBL_NF, HAWK_VAL_ZERO) == -1)
		{
			/* first of all, this should never happen. 
			 * if it happened, it would return an error
			 * after all the clearance tasks */
			n = -1;
		}
	}

	HAWK_ASSERT (hawk_rtx_getawk(rtx), run->inrec.nflds == 0);
	if (!skip_inrec_line) hawk_ooecs_clear (&run->inrec.line);

	return n;
}

static int recomp_record_fields (hawk_rtx_t* rtx, hawk_oow_t lv, const hawk_oocs_t* str)
{
	hawk_val_t* v;
	hawk_oow_t max, i, nflds;

	/* recomposes the record and the fields when $N has been assigned 
	 * a new value and recomputes NF accordingly */

	HAWK_ASSERT (hawk_rtx_getawk(rtx), lv > 0);
	max = (lv > rtx->inrec.nflds)? lv: rtx->inrec.nflds;

	nflds = rtx->inrec.nflds;
	if (max > rtx->inrec.maxflds)
	{
		void* tmp;

		/* if the given field number is greater than the maximum
		 * number of fields that the current record can hold,
		 * the field spaces are resized */

		tmp = hawk_rtx_reallocmem(rtx, rtx->inrec.flds, HAWK_SIZEOF(*rtx->inrec.flds) * max);
		if (!tmp) return -1;

		rtx->inrec.flds = tmp;
		rtx->inrec.maxflds = max;
	}

	lv = lv - 1; /* adjust the value to 0-based index */

	hawk_ooecs_clear (&rtx->inrec.line);

	for (i = 0; i < max; i++)
	{
		if (i > 0)
		{
			if (hawk_ooecs_ncat(&rtx->inrec.line, rtx->gbl.ofs.ptr, rtx->gbl.ofs.len) == (hawk_oow_t)-1) 
			{
				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}
		}

		if (i == lv)
		{
			hawk_val_t* tmp;

			rtx->inrec.flds[i].ptr = HAWK_OOECS_PTR(&rtx->inrec.line) + HAWK_OOECS_LEN(&rtx->inrec.line);
			rtx->inrec.flds[i].len = str->len;

			if (hawk_ooecs_ncat(&rtx->inrec.line, str->ptr, str->len) == (hawk_oow_t)-1)
			{
				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}

			tmp = hawk_rtx_makestrvalwithoocs (rtx, str);
			if (tmp == HAWK_NULL) return -1;

			if (i < nflds)
				hawk_rtx_refdownval (rtx, rtx->inrec.flds[i].val);
			else rtx->inrec.nflds++;

			rtx->inrec.flds[i].val = tmp;
			hawk_rtx_refupval (rtx, tmp);
		}
		else if (i >= nflds)
		{
			rtx->inrec.flds[i].ptr = HAWK_OOECS_PTR(&rtx->inrec.line) + HAWK_OOECS_LEN(&rtx->inrec.line);
			rtx->inrec.flds[i].len = 0;

			if (hawk_ooecs_cat(&rtx->inrec.line, HAWK_T("")) == (hawk_oow_t)-1)
			{
				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}

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
			hawk_val_str_t* tmp;

			tmp = (hawk_val_str_t*)rtx->inrec.flds[i].val;

			rtx->inrec.flds[i].ptr = HAWK_OOECS_PTR(&rtx->inrec.line) + HAWK_OOECS_LEN(&rtx->inrec.line);
			rtx->inrec.flds[i].len = tmp->val.len;

			if (hawk_ooecs_ncat(&rtx->inrec.line, tmp->val.ptr, tmp->val.len) == (hawk_oow_t)-1)
			{
				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}
		}
	}

	v = hawk_rtx_getgbl (rtx, HAWK_GBL_NF);
	HAWK_ASSERT (hawk_rtx_getawk(rtx), HAWK_RTX_GETVALTYPE (rtx, v) == HAWK_VAL_INT);

	if (HAWK_RTX_GETINTFROMVAL (rtx, v)!= max)
	{
		v = hawk_rtx_makeintval (rtx, (hawk_int_t)max);
		if (v == HAWK_NULL) return -1;

		hawk_rtx_refupval (rtx, v);
		if (hawk_rtx_setgbl (rtx, HAWK_GBL_NF, v) == -1) 
		{
			hawk_rtx_refdownval (rtx, v);
			return -1;
		}
		hawk_rtx_refdownval (rtx, v);
	}

	return 0;
}

