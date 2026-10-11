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

static hawk_val_t* make_record_nstr (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_val_t* v;
	hawk_int_t l;
	hawk_flt_t r;
	int n, stripstrspc;

	if (!prep) return hawk_rtx_makenstrvalwithoochars(rtx, ptr, len);
	stripstrspc = prep->gbl.stripstrspc > 0 ||
		(prep->gbl.stripstrspc < 0 && (rtx->hawk->parse.pragma.trait & HAWK_STRIPSTRSPC));
	n = hawk_oochars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 0, stripstrspc, 0), ptr, len, &l, &r);
	v = hawk_rtx_makestrvalwithoochars(rtx, ptr, len);
	if (v && n >= 0) v->v_nstr = n + 1;
	return v;
}

static int clrrec (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, int skip_inrec_line);

static int split_record (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, int prefer_number);
static int recomp_record_fields (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, hawk_oow_t lv, const hawk_oocs_t* str, int prefer_number);

int hawk_rtx_setrecwithstate (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, hawk_oow_t idx, const hawk_oocs_t* str, int prefer_number)
{
	hawk_val_t* v;

	if (idx == 0)
	{
		if (str->ptr == HAWK_OOECS_PTR(&rec->line) &&
		    str->len == HAWK_OOECS_LEN(&rec->line))
		{
			if (clrrec(rtx, rec, prep, 1) <= -1) return -1;
		}
		else
		{
			if (clrrec(rtx, rec, prep, 0) <= -1) return -1;
			if (hawk_ooecs_ncpy(&rec->line, str->ptr, str->len) == (hawk_oow_t)-1) goto oops;
		}

		if (split_record(rtx, rec, prep, prefer_number) <= -1) goto oops;

		v = prefer_number? hawk_rtx_makenumorstrvalwithoochars(rtx, HAWK_OOECS_PTR(&rec->line), HAWK_OOECS_LEN(&rec->line), 0):  /* number or string */
		                   make_record_nstr(rtx, prep, HAWK_OOECS_PTR(&rec->line), HAWK_OOECS_LEN(&rec->line));  /* str with nstr flag */

		if (HAWK_UNLIKELY(!v)) goto oops;
	}
	else
	{
		if (recomp_record_fields(rtx, rec, prep, idx, str, prefer_number) <= -1) goto oops;

		/* recompose $0 */
		v = hawk_rtx_makestrvalwithoochars(rtx, HAWK_OOECS_PTR(&rec->line), HAWK_OOECS_LEN(&rec->line));
		if (HAWK_UNLIKELY(!v)) goto oops;
	}

	if (HAWK_RTX_GETVALTYPE(rtx, rec->d0) != HAWK_VAL_NIL) hawk_rtx_refdownval_inline(rtx, rec->d0);
	rec->d0 = v;
	hawk_rtx_refupval_inline(rtx, v);

	return 0;

oops:
	clrrec(rtx, rec, prep, 0);
	return -1;
}

#if 0
static int merge_fields (hawk_rtx_t* rtx)
{
	hawk_oow_t i;

	hawk_ooecs_clear(&rec->line);

	for (i = 0; i < rec->nflds; i++)
	{
		hawk_ooch_t* vp;
		hawk_oow_t vl;

		if (HAWK_LIKELY(i > 0))
		{
			if (hawk_ooecs_ncat(&rec->line, gbl->ofs.ptr, gbl->ofs.len) == (hawk_oow_t)-1) return -1;
		}

		vp = prep? hawk_rtx_refprep_dupstr(rtx, prep, rec->flds[i].val, &vl): hawk_rtx_getvaloocstr(rtx, rec->flds[i].val, &vl);
		if (HAWK_UNLIKELY(!vp)) return -1;

		vl = hawk_ooecs_ncat(&rec->line, vp, vl);
		if (prep) hawk_rtx_freemem(rtx, vp);
			else hawk_rtx_freevaloocstr(rtx, rec->flds[i].val, vp);

		if (HAWK_UNLIKELY(vl == (hawk_oow_t)-1)) return -1;
	}

	return 1;
}
#endif

static int split_record (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, int prefer_number)
{
	hawk_rtx_gbl_t* gbl = prep? &prep->gbl: &rtx->gbl;
	int striprecspc = gbl->striprecspc > 0 || (gbl->striprecspc < 0 && (rtx->hawk->parse.pragma.trait & HAWK_STRIPRECSPC));
	hawk_oocs_t tok;
	hawk_ooch_t* p, * px;
	hawk_oow_t len, nflds;
	hawk_val_t* v, * fs;
	hawk_val_type_t fsvtype;
	hawk_ooch_t* fs_ptr, * fs_free;
	hawk_oow_t fs_len;
	int how;

	/* inrec should be cleared before split_record is called */
	HAWK_ASSERT(rec->nflds == 0);

	/* get FS */
	fs = hawk_rtx_refprep_getgbl(rtx, prep, HAWK_GBL_FS);
	fsvtype = HAWK_RTX_GETVALTYPE(rtx, fs);
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
		fs_ptr = hawk_rtx_refprep_dupstr(rtx, prep, fs, &fs_len);
		if (HAWK_UNLIKELY(!fs_ptr)) return -1;
		fs_free = fs_ptr;
	}

	/* scan the input record to count the fields */
	if (fs_len == 5 && fs_ptr[0] ==  HAWK_T('?'))
	{
		if (hawk_ooecs_ncpy(&rec->linew, HAWK_OOECS_PTR(&rec->line), HAWK_OOECS_LEN(&rec->line)) == (hawk_oow_t)-1)
		{
			if (fs_free) hawk_rtx_freemem(rtx, fs_free);
			return -1;
		}

		px = HAWK_OOECS_PTR(&rec->linew);
		how = 1;
	}
	else
	{
		px = HAWK_OOECS_PTR(&rec->line);
		how = (fs_len <= 1)? 0: 2;
	}

	p = px;
	len = HAWK_OOECS_LEN(&rec->line);

#if 0
	nflds = 0;
	while (p != HAWK_NULL)
	{
		switch (how)
		{
			case 0:
				p = hawk_rtx_tokoocharswithoocharswithoptions(rtx, p, len, fs_ptr, fs_len, &tok, gbl->ignorecase);
				break;

			case 1:
				break;

			default:
				p = hawk_rtx_tokoocharsbyrexwithoptions(
					rtx,
					HAWK_OOECS_PTR(&rec->line),
					HAWK_OOECS_LEN(&rec->line),
					p, len,
					gbl->fs[gbl->ignorecase != 0], &tok, striprecspc
				);
				if (p == HAWK_NULL && hawk_rtx_geterrnum(rtx) != HAWK_ENOERR)
				{
					if (fs_free) hawk_rtx_freemem(rtx, fs_free);
					return -1;
				}
		}

		if (nflds == 0 && p == HAWK_NULL && tok.len == 0)
		{
			/* there are no fields. it can just return here
			 * as hawk_rtx_clrrec has been called before this */
			if (fs_free) hawk_rtx_freemem(rtx, fs_free);
			return 0;
		}

		HAWK_ASSERT((tok.ptr != HAWK_NULL && tok.len > 0) || tok.len == 0);

		nflds++;
		len = HAWK_OOECS_LEN(&rec->line) - (p - HAWK_OOECS_PTR(&rec->line));
	}

	/* allocate space */
	if (nflds > rec->maxflds)
	{
		void* tmp = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*rec->flds) * nflds);
		if (HAWK_UNLIKELY(!tmp))
		{
			if (fs_free) hawk_rtx_freemem(rtx, fs_free);
			return -1;
		}

		if (rec->flds) hawk_rtx_freemem(rtx, rec->flds);
		rec->flds = tmp;
		rec->maxflds = nflds;
	}

	/* scan again and split it */
	if (how == 1)
	{
		if (hawk_ooecs_ncpy(&rec->linew, HAWK_OOECS_PTR(&rec->line), HAWK_OOECS_LEN(&rec->line)) == (hawk_oow_t)-1)
		{
			if (fs_free) hawk_rtx_freemem(rtx, fs_free);
			return -1;
		}
		px = HAWK_OOECS_PTR(&rec->linew):
	}
	else
	{
		px = HAWK_OOECS_PTR(&rec->line);
	}

	p = px;
	len = HAWK_OOECS_LEN(&rec->line);
#endif

	while (p)
	{
		switch (how)
		{
			case 0:
				/* 1 character FS */
				p = hawk_rtx_tokoocharswithoocharswithoptions(rtx, p, len, fs_ptr, fs_len, &tok, gbl->ignorecase);
				break;

			case 1:
				/* 5 character FS beginning with ? */
				p = hawk_rtx_fldoochars(rtx, p, len, fs_ptr[1], fs_ptr[2], fs_ptr[3], fs_ptr[4], &tok);
				break;

			default:
				/* all other cases */
				p = hawk_rtx_tokoocharsbyrexwithoptions(
					rtx,
					HAWK_OOECS_PTR(&rec->line),
					HAWK_OOECS_LEN(&rec->line),
					p, len,
					gbl->fs[gbl->ignorecase != 0], &tok, striprecspc
				);
				if (p == HAWK_NULL && hawk_rtx_geterrnum(rtx) != HAWK_ENOERR)
				{
					if (fs_free) hawk_rtx_freemem(rtx, fs_free);
					return -1;
				}
		}
#if 1
		if (rec->nflds == 0 && p == HAWK_NULL && tok.len == 0)
		{
			/* there are no fields. it can just return here
			 * as hawk_rtx_clrrec has been called before this */
			if (fs_free) hawk_rtx_freemem(rtx, fs_free);
			return 0;
		}
#endif

		HAWK_ASSERT((tok.ptr != HAWK_NULL && tok.len > 0) || tok.len == 0);

#if 1
		if (rec->nflds >= rec->maxflds)
		{
			void* tmp;

			if (rec->nflds < 16) nflds = 32;
			else
			{
				if (rec->nflds > HAWK_TYPE_MAX(hawk_oow_t) / (2 * HAWK_SIZEOF(*rec->flds)))
				{
					if (fs_free) hawk_rtx_freemem(rtx, fs_free);
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENOMEM);
					return -1;
				}
				nflds = rec->nflds * 2;
			}

			tmp = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*rec->flds) * nflds);
			if (HAWK_UNLIKELY(!tmp))
			{
				if (fs_free) hawk_rtx_freemem(rtx, fs_free);
				return -1;
			}

			if (rec->flds != HAWK_NULL)
			{
				HAWK_MEMCPY(tmp, rec->flds, HAWK_SIZEOF(*rec->flds) * rec->nflds);
				hawk_rtx_freemem(rtx, rec->flds);
			}

			rec->flds = tmp;
			rec->maxflds = nflds;
		}
#endif

		rec->flds[rec->nflds].ptr = tok.ptr;
		rec->flds[rec->nflds].len = tok.len;
		/*rec->flds[rec->nflds].val = hawk_rtx_makenstrvalwithoocs(rtx, &tok);*/
		rec->flds[rec->nflds].val =
			prefer_number? hawk_rtx_makenumorstrvalwithoochars(rtx, tok.ptr, tok.len, 0):
			               hawk_rtx_makestrvalwithoochars(rtx, tok.ptr, tok.len);
		if (HAWK_UNLIKELY(!rec->flds[rec->nflds].val))
		{
			if (fs_free) hawk_rtx_freemem(rtx, fs_free);
			return -1;
		}

		hawk_rtx_refupval_inline(rtx, rec->flds[rec->nflds].val);
		rec->nflds++;

		if (p) len = HAWK_OOECS_LEN(&rec->line) - (p - px);
	}

	if (fs_free) hawk_rtx_freemem(rtx, fs_free);

	/* set the number of fields */
	v = hawk_rtx_makeintval_inline(rtx, (hawk_int_t)rec->nflds);
	if (v == HAWK_NULL) return -1;

	hawk_rtx_refupval_inline(rtx, v);
	if (hawk_rtx_refprep_setgbl(rtx, prep, HAWK_GBL_NF, v) <= -1)
	{
		hawk_rtx_refdownval_inline(rtx, v);
		return -1;
	}
	hawk_rtx_refdownval_inline(rtx, v);
	return 0;
}

static int clrrec (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, int skip_inrec_line)
{
	hawk_oow_t i;
	int n = 0;

	if (HAWK_RTX_GETVALTYPE(rtx, rec->d0) != HAWK_VAL_NIL)
	{
		hawk_rtx_refdownval_inline(rtx, rec->d0);
		rec->d0 = hawk_val_nil;
	}

	if (rec->nflds > 0)
	{
		HAWK_ASSERT(rec->flds != HAWK_NULL);

		for (i = 0; i < rec->nflds; i++)
		{
			HAWK_ASSERT(rec->flds[i].val != HAWK_NULL);
			hawk_rtx_refdownval_inline(rtx, rec->flds[i].val);
		}
		rec->nflds = 0;

		if (hawk_rtx_refprep_setgbl(rtx, prep, HAWK_GBL_NF, HAWK_VAL_ZERO) <= -1)
		{
			/* first of all, this should never happen.
			 * if it happened, it would return an error
			 * after all the clearance tasks */
			n = -1;
		}
	}

	HAWK_ASSERT(rec->nflds == 0);
	if (!skip_inrec_line) hawk_ooecs_clear(&rec->line);

	return n;
}

static int recomp_record_fields (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, hawk_oow_t lv, const hawk_oocs_t* str, int prefer_number)
{
	hawk_rtx_gbl_t* gbl = prep? &prep->gbl: &rtx->gbl;
	hawk_val_t* v;
	hawk_oow_t max, i, nflds, offset;

	/* recomposes the record and the fields when $N has been assigned
	 * a new value and recomputes NF accordingly.
	 *
	 * BEGIN { OFS=":" } { $2 = "Q"; print $0; }
	 * If input is abc def xxx, $0 becomes abc:Q:xxx.
	 *
	 * We should store the value in rec->line so that the caller
	 * can use it to make a value for $0.
	 */

	HAWK_ASSERT(lv > 0);
	max = (lv > rec->nflds)? lv: rec->nflds;

	nflds = rec->nflds;
	if (max > rec->maxflds)
	{
		void* tmp;

		/* if the given field number is greater than the maximum
		 * number of fields that the current record can hold,
		 * the field spaces are resized */

		if (max > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(*rec->flds))
		{
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENOMEM);
			return -1;
		}
		tmp = hawk_rtx_reallocmem(rtx, rec->flds, HAWK_SIZEOF(*rec->flds) * max);
		if (HAWK_UNLIKELY(!tmp)) return -1;

		rec->flds = tmp;
		rec->maxflds = max;
	}

	lv = lv - 1; /* adjust the value to 0-based index */

	hawk_ooecs_clear(&rec->line);

	for (i = 0; i < max; i++)
	{
		if (i > 0)
		{
			if (hawk_ooecs_ncat(&rec->line, gbl->ofs.ptr, gbl->ofs.len) == (hawk_oow_t)-1) return -1;
		}

		if (i == lv)
		{
			hawk_val_t* tmp;

			rec->flds[i].ptr = HAWK_OOECS_PTR(&rec->line) + HAWK_OOECS_LEN(&rec->line);
			rec->flds[i].len = str->len;

			if (hawk_ooecs_ncat(&rec->line, str->ptr, str->len) == (hawk_oow_t)-1) return -1;

			tmp = prefer_number? hawk_rtx_makenumorstrvalwithoochars(rtx, str->ptr, str->len, 0):
			                     hawk_rtx_makestrvalwithoochars(rtx, str->ptr, str->len);
			if (HAWK_UNLIKELY(!tmp)) return -1;

			if (i < nflds) hawk_rtx_refdownval_inline(rtx, rec->flds[i].val);
			else rec->nflds++;

			rec->flds[i].val = tmp;
			hawk_rtx_refupval_inline(rtx, tmp);
		}
		else if (i >= nflds)
		{
			rec->flds[i].ptr = HAWK_OOECS_PTR(&rec->line) + HAWK_OOECS_LEN(&rec->line);
			rec->flds[i].len = 0;

			if (hawk_ooecs_cat(&rec->line, HAWK_T("")) == (hawk_oow_t)-1) return -1;

			/* hawk_rtx_refdownval_inline should not be called over
			 * rec->flds[i].val as it is not initialized
			 * to any valid values */
			/*hawk_rtx_refdownval_inline(rtx, rec->flds[i].val);*/
			rec->flds[i].val = hawk_val_zls;
			hawk_rtx_refupval_inline(rtx, hawk_val_zls);
			rec->nflds++;
		}
		else
		{
			hawk_ooch_t* vp;
			hawk_oow_t vl;

			vp = prep? hawk_rtx_refprep_dupstr(rtx, prep, rec->flds[i].val, &vl): hawk_rtx_getvaloocstr(rtx, rec->flds[i].val, &vl);
			if (HAWK_UNLIKELY(!vp)) return -1;

			rec->flds[i].len = vl;
			vl = hawk_ooecs_ncat(&rec->line, vp, vl);
			if (prep) hawk_rtx_freemem(rtx, vp);
			else hawk_rtx_freevaloocstr(rtx, rec->flds[i].val, vp);

			if (HAWK_UNLIKELY(vl == (hawk_oow_t)-1)) return -1;
		}
	}

	/* Buffer growth can move line. Rebase every field after recomposition. */
	offset = 0;
	for (i = 0; i < max; i++)
	{
		if (i > 0) offset += gbl->ofs.len;
		rec->flds[i].ptr = HAWK_OOECS_PTR(&rec->line) + offset;
		offset += rec->flds[i].len;
	}

	v = hawk_rtx_refprep_getgbl(rtx, prep, HAWK_GBL_NF);
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_INT);

	if (HAWK_RTX_GETINTFROMVAL(rtx, v) != max)
	{
		v = hawk_rtx_makeintval_inline(rtx, (hawk_int_t)max);
		if (v == HAWK_NULL) return -1;

		hawk_rtx_refupval_inline(rtx, v);
		if (hawk_rtx_refprep_setgbl(rtx, prep, HAWK_GBL_NF, v) <= -1)
		{
			hawk_rtx_refdownval_inline(rtx, v);
			return -1;
		}
		hawk_rtx_refdownval_inline(rtx, v);
	}

	return 0;
}

int hawk_rtx_truncrecwithstate (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec, hawk_rtx_refprep_t* prep, hawk_oow_t nflds)
{
	hawk_val_t* v = HAWK_NULL, * w;
	hawk_ooch_t* ofs_free = HAWK_NULL, * ofs_ptr;
	hawk_oow_t ofs_len = 0, i, offset;
	hawk_val_type_t vtype;
	hawk_ooecs_t tmp;
	int fini_tmp = 0;

	HAWK_ASSERT(nflds <= rec->nflds);

	if (hawk_ooecs_init(&tmp, hawk_rtx_getgem(rtx), HAWK_OOECS_LEN(&rec->line)) <= -1) goto oops;
	fini_tmp = 1;

	if (nflds > 0)
	{
		if (nflds > 1)
		{
			v = hawk_rtx_refprep_getgbl(rtx, prep, HAWK_GBL_OFS);
			hawk_rtx_refupval_inline(rtx, v);
			vtype = HAWK_RTX_GETVALTYPE(rtx, v);

			if (vtype == HAWK_VAL_NIL)
			{
				/* OFS not set */
				ofs_ptr = HAWK_T(" ");
				ofs_len = 1;
			}
			else
			{
				ofs_ptr = prep? hawk_rtx_refprep_dupstr(rtx, prep, v, &ofs_len): hawk_rtx_getvaloocstr(rtx, v, &ofs_len);
				if (HAWK_UNLIKELY(!ofs_ptr)) goto oops;
				ofs_free = ofs_ptr;
			}
		}

		if (hawk_ooecs_ncat(&tmp, rec->flds[0].ptr, rec->flds[0].len) == (hawk_oow_t)-1) goto oops;
		for (i = 1; i < nflds; i++)
		{
			if (i > 0 && hawk_ooecs_ncat(&tmp,ofs_ptr,ofs_len) == (hawk_oow_t)-1) goto oops;
			if (hawk_ooecs_ncat(&tmp, rec->flds[i].ptr, rec->flds[i].len) == (hawk_oow_t)-1) goto oops;
		}

		if (v)
		{
			if (ofs_free)
			{
				if (prep) hawk_rtx_freemem(rtx, ofs_free);
				else hawk_rtx_freevaloocstr(rtx, v, ofs_free);
			}
			hawk_rtx_refdownval_inline(rtx, v);
			v = HAWK_NULL;
		}
	}

	w = (hawk_val_t*)hawk_rtx_makestrvalwithoocs(rtx, HAWK_OOECS_OOCS(&tmp));
	if (HAWK_UNLIKELY(!w)) goto oops;

	hawk_rtx_refdownval_inline(rtx, rec->d0);
	rec->d0 = w;
	hawk_rtx_refupval_inline(rtx, rec->d0);

	hawk_ooecs_swap(&tmp, &rec->line);
	/* Surviving fields must point into the new line, not the freed buffer. */
	offset = 0;
	for (i = 0; i < nflds; i++)
	{
		if (i > 0) offset += ofs_len;
		rec->flds[i].ptr = HAWK_OOECS_PTR(&rec->line) + offset;
		offset += rec->flds[i].len;
	}
	hawk_ooecs_fini(&tmp);
	fini_tmp = 0;

	for (i = nflds; i < rec->nflds; i++)
	{
		hawk_rtx_refdownval_inline(rtx, rec->flds[i].val);
	}

	rec->nflds = nflds;
	return 0;

oops:
	if (fini_tmp) hawk_ooecs_fini(&tmp);
	if (v)
	{
		if (ofs_free)
		{
			if (prep) hawk_rtx_freemem(rtx, ofs_free);
			else hawk_rtx_freevaloocstr(rtx, v, ofs_free);
		}
		hawk_rtx_refdownval_inline(rtx, v);
	}
	return -1;
}

int hawk_rtx_setrec (hawk_rtx_t* rtx, hawk_oow_t idx, const hawk_oocs_t* str, int prefer_number)
{
	return hawk_rtx_setrecwithstate(rtx, &rtx->inrec, HAWK_NULL, idx, str, prefer_number);
}

int hawk_rtx_clrrec (hawk_rtx_t* rtx, int skip_inrec_line)
{
	return clrrec(rtx, &rtx->inrec, HAWK_NULL, skip_inrec_line);
}

int hawk_rtx_truncrec (hawk_rtx_t* rtx, hawk_oow_t nflds)
{
	return hawk_rtx_truncrecwithstate(rtx, &rtx->inrec, HAWK_NULL, nflds);
}

void hawk_rtx_refprep_freerec (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec)
{
	hawk_oow_t i;

	for (i = 0; i < rec->nflds; i++)
		hawk_rtx_refdownval_inline(rtx, rec->flds[i].val);

	hawk_rtx_refdownval_inline(rtx, rec->d0);

	if (rec->flds) hawk_rtx_freemem(rtx, rec->flds);

	hawk_ooecs_fini(&rec->linew);
	hawk_ooecs_fini(&rec->line);
	hawk_rtx_freemem(rtx, rec);
}

int hawk_rtx_refprep_record (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep)
{
	hawk_rtx_inrec_t* rec;
	hawk_oow_t i;

	if (prep->rec) return 0;

	rec = hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*rec));
	if (HAWK_UNLIKELY(!rec)) return -1;

	rec->d0 = hawk_val_nil;

	if (hawk_ooecs_init(&rec->line, hawk_rtx_getgem(rtx), 0) <= -1) goto fail;
	if (hawk_ooecs_init(&rec->linew, hawk_rtx_getgem(rtx), 0) <= -1)
	{
		hawk_ooecs_fini(&rec->line);
		goto fail;
	}

	if (hawk_ooecs_ncpy(&rec->line, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line)) == (hawk_oow_t)-1 ||
	    hawk_ooecs_ncpy(&rec->linew, HAWK_OOECS_PTR(&rtx->inrec.linew), HAWK_OOECS_LEN(&rtx->inrec.linew)) == (hawk_oow_t)-1)
	{
		goto fail_record;
	}

	if (rtx->inrec.nflds > 0)
	{
		rec->flds = hawk_rtx_allocmem(rtx, rtx->inrec.nflds * HAWK_SIZEOF(*rec->flds));
		if (HAWK_UNLIKELY(!rec->flds)) goto fail_record;

		rec->maxflds = rtx->inrec.nflds;
		for (i = 0; i < rtx->inrec.nflds; i++)
		{
			hawk_oow_t ptr = (hawk_oow_t)rtx->inrec.flds[i].ptr;
			hawk_oow_t base = (hawk_oow_t)HAWK_OOECS_PTR(&rtx->inrec.line);

			rec->flds[i] = rtx->inrec.flds[i];
			if (ptr >= base && ptr - base <= HAWK_OOECS_LEN(&rtx->inrec.line) * HAWK_SIZEOF(hawk_ooch_t))
			{
				rec->flds[i].ptr = HAWK_OOECS_PTR(&rec->line) + (ptr - base) / HAWK_SIZEOF(hawk_ooch_t);
			}
			else
			{
				base = (hawk_oow_t)HAWK_OOECS_PTR(&rtx->inrec.linew);
				HAWK_ASSERT(ptr >= base && ptr - base <= HAWK_OOECS_LEN(&rtx->inrec.linew) * HAWK_SIZEOF(hawk_ooch_t));
				rec->flds[i].ptr = HAWK_OOECS_PTR(&rec->linew) + (ptr - base) / HAWK_SIZEOF(hawk_ooch_t);
			}
			hawk_rtx_refupval_inline(rtx, rec->flds[i].val);
			rec->nflds++;
		}
	}

	rec->d0 = rtx->inrec.d0;
	hawk_rtx_refupval_inline(rtx, rec->d0);
	prep->rec = rec;

	return 0;

fail_record:
	hawk_rtx_refprep_freerec(rtx, rec);
	return -1;

fail:
	hawk_rtx_freemem(rtx, rec);
	return -1;
}

void hawk_rtx_refprep_swaprec (hawk_rtx_t* rtx, hawk_rtx_inrec_t* rec)
{
	hawk_val_t* d0;
	hawk_oow_t n;
	void* flds;

	/* input buffering and getline buffers belong to the live runtime. */
	hawk_ooecs_swap(&rtx->inrec.line, &rec->line);
	hawk_ooecs_swap(&rtx->inrec.linew, &rec->linew);

	d0 = rtx->inrec.d0;
	rtx->inrec.d0 = rec->d0;
	rec->d0 = d0;

	flds = rtx->inrec.flds;
	rtx->inrec.flds = rec->flds;
	rec->flds = flds;

	n = rtx->inrec.maxflds;
	rtx->inrec.maxflds = rec->maxflds;
	rec->maxflds = n;

	n = rtx->inrec.nflds;
	rtx->inrec.nflds = rec->nflds;
	rec->nflds = n;
}
