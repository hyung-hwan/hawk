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

void* hawk_gem_allocmem (hawk_gem_t* gem, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(gem->mmgr, size);
	if (!ptr) hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_gem_callocmem (hawk_gem_t* gem, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(gem->mmgr, size);
	if (ptr) HAWK_MEMSET (ptr, 0, size);
	else hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_gem_reallocmem (hawk_gem_t* gem, void* ptr, hawk_oow_t size)
{
	void* nptr = HAWK_MMGR_REALLOC(gem->mmgr, ptr, size);
	if (!nptr) hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOMEM);
	return nptr;
}

void* hawk_gem_callocmem_noerr (hawk_gem_t* gem, hawk_oow_t size)
{
	void* ptr = HAWK_MMGR_ALLOC(gem->mmgr, size);
	if (ptr) HAWK_MEMSET (ptr, 0, size);
	return ptr;
}

/* ------------------------------------------------------------------------ */

hawk_uch_t* hawk_gem_dupucstr (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t* _ucslen)
{
	hawk_uch_t* ptr;
	hawk_oow_t ucslen;

	ucslen = hawk_count_ucstr(ucs);
	ptr = (hawk_uch_t*)hawk_gem_allocmem(gem, (ucslen + 1) * HAWK_SIZEOF(hawk_uch_t));
	if (!ptr) return HAWK_NULL;

	hawk_copy_uchars (ptr, ucs, ucslen);
	ptr[ucslen] = '\0';

	if (_ucslen) *_ucslen = ucslen;
	return ptr;
}

hawk_bch_t* hawk_gem_dupbcstr (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t* _bcslen)
{
	hawk_bch_t* ptr;
	hawk_oow_t bcslen;

	bcslen = hawk_count_bcstr(bcs);
	ptr = (hawk_bch_t*)hawk_gem_allocmem(gem, (bcslen + 1) * HAWK_SIZEOF(hawk_bch_t));
	if (!ptr) return HAWK_NULL;

	hawk_copy_bchars (ptr, bcs, bcslen);
	ptr[bcslen] = '\0';

	if (_bcslen) *_bcslen = bcslen;
	return ptr;
}


hawk_uch_t* hawk_gem_dupuchars (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t ucslen)
{
	hawk_uch_t* ptr;

	ptr = (hawk_uch_t*)hawk_gem_allocmem(gem, (ucslen + 1) * HAWK_SIZEOF(hawk_uch_t));
	if (!ptr) return HAWK_NULL;

	hawk_copy_uchars (ptr, ucs, ucslen);
	ptr[ucslen] = '\0';
	return ptr;
}

hawk_bch_t* hawk_gem_dupbchars (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t bcslen)
{
	hawk_bch_t* ptr;

	ptr = (hawk_bch_t*)hawk_gem_allocmem(gem, (bcslen + 1) * HAWK_SIZEOF(hawk_bch_t));
	if (!ptr) return HAWK_NULL;

	hawk_copy_bchars (ptr, bcs, bcslen);
	ptr[bcslen] = '\0';
	return ptr;
}

hawk_uch_t* hawk_gem_dupucs (hawk_gem_t* gem, const hawk_ucs_t* ucs)
{
	hawk_uch_t* ptr;

	ptr = (hawk_uch_t*)hawk_gem_allocmem(gem, (ucs->len + 1) * HAWK_SIZEOF(hawk_uch_t));
	if (!ptr) return HAWK_NULL;

	hawk_copy_uchars (ptr, ucs->ptr, ucs->len);
	ptr[ucs->len] = '\0';
	return ptr;
}

hawk_bch_t* hawk_gem_dupbcs (hawk_gem_t* gem, const hawk_bcs_t* bcs)
{
	hawk_bch_t* ptr;

	ptr = (hawk_bch_t*)hawk_gem_allocmem(gem, (bcs->len + 1) * HAWK_SIZEOF(hawk_bch_t));
	if (!ptr) return HAWK_NULL;

	hawk_copy_bchars (ptr, bcs->ptr, bcs->len);
	ptr[bcs->len] = '\0';
	return ptr;
}

hawk_uch_t* hawk_gem_dupucstrarr (hawk_gem_t* gem, const hawk_uch_t* str[], hawk_oow_t* len)
{
	hawk_uch_t* buf, * ptr;
	hawk_oow_t i;
	hawk_oow_t capa = 0;

	for (i = 0; str[i]; i++) capa += hawk_count_ucstr(str[i]);

	buf = (hawk_uch_t*)hawk_gem_allocmem(gem, (capa + 1) * HAWK_SIZEOF(*buf));
	if (!buf) return HAWK_NULL;

	ptr = buf;
	for (i = 0; str[i]; i++) ptr += hawk_copy_ucstr_unlimited(ptr, str[i]);

	if (len) *len = capa;
	return buf;
}

hawk_bch_t* hawk_gem_dupbcstrarr (hawk_gem_t* gem, const hawk_bch_t* str[], hawk_oow_t* len)
{
	hawk_bch_t* buf, * ptr;
	hawk_oow_t i;
	hawk_oow_t capa = 0;

	for (i = 0; str[i]; i++) capa += hawk_count_bcstr(str[i]);

	buf = (hawk_bch_t*)hawk_gem_allocmem(gem, (capa + 1) * HAWK_SIZEOF(*buf));
	if (!buf) return HAWK_NULL;

	ptr = buf;
	for (i = 0; str[i]; i++) ptr += hawk_copy_bcstr_unlimited(ptr, str[i]);

	if (len) *len = capa;
	return buf;
}

/* ------------------------------------------------------------------------ */

int hawk_gem_convbtouchars (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all)
{
	/* length bound */
	int n;
	n = hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, gem->cmgr, all);
	/* -1: illegal character, -2: buffer too small, -3: incomplete sequence */
	if (n <= -1) hawk_gem_seterrnum (gem, HAWK_NULL, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR);
	return n;
}

int hawk_gem_convutobchars (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	int n;
	n = hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, gem->cmgr);
	if (n <= -1) hawk_gem_seterrnum (gem, HAWK_NULL, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR);
	return n;
}

int hawk_gem_convbtoucstr (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all)
{
	/* null-terminated. */
	int n;
	n = hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, gem->cmgr, all);
	if (n <= -1) hawk_gem_seterrnum (gem, HAWK_NULL, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR);
	return n;
}

int hawk_gem_convutobcstr (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	int n;
	n = hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, gem->cmgr);
	if (n <= -1) hawk_gem_seterrnum (gem, HAWK_NULL, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR);
	return n;
}

/* ------------------------------------------------------------------------ */

hawk_uch_t* hawk_gem_dupbtouchars (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t _bcslen, hawk_oow_t* _ucslen, int all)
{
	hawk_oow_t bcslen, ucslen;
	hawk_uch_t* ucs;

	bcslen = _bcslen;
	if (hawk_gem_convbtouchars(gem, bcs, &bcslen, HAWK_NULL, &ucslen, all) <= -1) return HAWK_NULL;

	ucs = hawk_gem_allocmem(gem, HAWK_SIZEOF(*ucs) * (ucslen + 1));
	if (!ucs) return HAWK_NULL;

	bcslen= _bcslen;
	hawk_gem_convbtouchars (gem, bcs, &bcslen, ucs, &ucslen, all);
	ucs[ucslen] = '\0';

	if (_ucslen) *_ucslen = ucslen;
	return ucs;
}

hawk_bch_t* hawk_gem_duputobchars (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t _ucslen, hawk_oow_t* _bcslen)
{
	hawk_oow_t bcslen, ucslen;
	hawk_bch_t* bcs;

	ucslen = _ucslen;
	if (hawk_gem_convutobchars(gem, ucs, &ucslen, HAWK_NULL, &bcslen) <= -1) return HAWK_NULL;

	bcs = hawk_gem_allocmem(gem, HAWK_SIZEOF(*bcs) * (bcslen + 1));
	if (!bcs) return HAWK_NULL;

	ucslen = _ucslen;
	hawk_gem_convutobchars (gem, ucs, &ucslen, bcs, &bcslen);
	bcs[bcslen] = '\0';

	if (_bcslen) *_bcslen = bcslen;
	return bcs;
}

hawk_uch_t* hawk_gem_dupb2touchars (hawk_gem_t* gem, const hawk_bch_t* bcs1, hawk_oow_t bcslen1, const hawk_bch_t* bcs2, hawk_oow_t bcslen2, hawk_oow_t* ucslen, int all)
{
	hawk_oow_t inlen, outlen1, outlen2;
	hawk_uch_t* ptr;

	inlen = bcslen1;
	if (hawk_gem_convbtouchars(gem, bcs1, &inlen, HAWK_NULL, &outlen1, all) <= -1) return HAWK_NULL;

	inlen = bcslen2;
	if (hawk_gem_convbtouchars(gem, bcs2, &inlen, HAWK_NULL, &outlen2, all) <= -1) return HAWK_NULL;

	ptr = (hawk_uch_t*)hawk_gem_allocmem(gem, (outlen1 + outlen2 + 1) * HAWK_SIZEOF(*ptr));
	if (!ptr) return HAWK_NULL;

	inlen = bcslen1;
	hawk_gem_convbtouchars (gem, bcs1, &inlen, &ptr[0], &outlen1, all);

	inlen = bcslen2;
	hawk_gem_convbtouchars (gem, bcs2, &inlen, &ptr[outlen1], &outlen2, all);

	/* hawk_convbtouchars() doesn't null-terminate the target. 
	 * but in hawk_dupbtouchars(), i allocate space. so i don't mind
	 * null-terminating it with 1 extra character overhead */
	ptr[outlen1 + outlen2] = '\0'; 
	if (ucslen) *ucslen = outlen1 + outlen2;
	return ptr;
}

hawk_bch_t* hawk_gem_dupu2tobchars (hawk_gem_t* gem, const hawk_uch_t* ucs1, hawk_oow_t ucslen1, const hawk_uch_t* ucs2, hawk_oow_t ucslen2, hawk_oow_t* bcslen)
{
	hawk_oow_t inlen, outlen1, outlen2;
	hawk_bch_t* ptr;

	inlen = ucslen1;
	if (hawk_gem_convutobchars(gem, ucs1, &inlen, HAWK_NULL, &outlen1) <= -1) return HAWK_NULL;

	inlen = ucslen2;
	if (hawk_gem_convutobchars(gem, ucs2, &inlen, HAWK_NULL, &outlen2) <= -1) return HAWK_NULL;

	ptr = (hawk_bch_t*)hawk_gem_allocmem(gem, (outlen1 + outlen2 + 1) * HAWK_SIZEOF(*ptr));
	if (!ptr) return HAWK_NULL;

	inlen = ucslen1;
	hawk_gem_convutobchars (gem, ucs1, &inlen, &ptr[0], &outlen1);

	inlen = ucslen2;
	hawk_gem_convutobchars (gem, ucs2, &inlen, &ptr[outlen1], &outlen2);

	ptr[outlen1 + outlen2] = '\0';
	if (bcslen) *bcslen = outlen1 + outlen2;

	return ptr;
}

hawk_uch_t* hawk_gem_dupbtoucstr (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t* _ucslen, int all)
{
	hawk_oow_t bcslen, ucslen;
	hawk_uch_t* ucs;

	if (hawk_gem_convbtoucstr(gem, bcs, &bcslen, HAWK_NULL, &ucslen, all) <= -1) return HAWK_NULL;
	ucslen = ucslen + 1; /* for terminating null */

	ucs = hawk_gem_allocmem(gem, HAWK_SIZEOF(*ucs) * ucslen);
	if (!ucs) return HAWK_NULL;

	hawk_gem_convbtoucstr (gem, bcs, &bcslen, ucs, &ucslen, all);
	if (_ucslen) *_ucslen = ucslen;
	return ucs;
}

hawk_bch_t* hawk_gem_duputobcstr (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t* _bcslen)
{
	hawk_oow_t bcslen, ucslen;
	hawk_bch_t* bcs;

	if (hawk_gem_convutobcstr(gem, ucs, &ucslen, HAWK_NULL, &bcslen) <= -1) return HAWK_NULL;
	bcslen = bcslen + 1; /* for terminating null */

	bcs = hawk_gem_allocmem(gem, HAWK_SIZEOF(*bcs) * bcslen);
	if (!bcs) return HAWK_NULL;

	hawk_gem_convutobcstr (gem, ucs, &ucslen, bcs, &bcslen);
	if (_bcslen) *_bcslen = bcslen;
	return bcs;
}

hawk_uch_t* hawk_gem_dupbtoucharswithcmgr (hawk_gem_t* gem, const hawk_bch_t* bcs, hawk_oow_t _bcslen, hawk_oow_t* _ucslen, hawk_cmgr_t* cmgr, int all)
{
	hawk_oow_t bcslen, ucslen;
	hawk_uch_t* ucs;
	int n;

	bcslen = _bcslen; 
	n = hawk_conv_bchars_to_uchars_with_cmgr(bcs, &bcslen, HAWK_NULL, &ucslen, cmgr, all);
	if (n <= -1) 
	{
		/* -1: illegal character, -2: buffer too small, -3: incomplete sequence */
		hawk_gem_seterrnum (gem, HAWK_NULL, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR);
		return HAWK_NULL;
	}

	ucs = hawk_gem_allocmem(gem, HAWK_SIZEOF(*ucs) * (ucslen + 1));
	if (!ucs) return HAWK_NULL;

	bcslen= _bcslen;
	hawk_conv_bchars_to_uchars_with_cmgr(bcs, &bcslen, ucs, &ucslen, cmgr, all);
	ucs[ucslen] = '\0';

	if (_ucslen) *_ucslen = ucslen;
	return ucs;
}

hawk_bch_t* hawk_gem_duputobcharswithcmgr (hawk_gem_t* gem, const hawk_uch_t* ucs, hawk_oow_t _ucslen, hawk_oow_t* _bcslen, hawk_cmgr_t* cmgr)
{
	hawk_oow_t bcslen, ucslen;
	hawk_bch_t* bcs;
	int n;

	ucslen = _ucslen;
	n = hawk_conv_uchars_to_bchars_with_cmgr(ucs, &ucslen, HAWK_NULL, &bcslen, cmgr);
	if (n <= -1) 
	{
		/* -1: illegal character, -2: buffer too small, -3: incomplete sequence */
		hawk_gem_seterrnum (gem, HAWK_NULL, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR);
		return HAWK_NULL;
	}

	bcs = hawk_gem_allocmem(gem, HAWK_SIZEOF(*bcs) * (bcslen + 1));
	if (!bcs) return HAWK_NULL;

	ucslen = _ucslen;
	hawk_conv_uchars_to_bchars_with_cmgr (ucs, &ucslen, bcs, &bcslen, cmgr);
	bcs[bcslen] = '\0';

	if (_bcslen) *_bcslen = bcslen;
	return bcs;
}

hawk_uch_t* hawk_gem_dupbcstrarrtoucstr (hawk_gem_t* gem, const hawk_bch_t* bcs[], hawk_oow_t* ucslen, int all)
{
	hawk_oow_t bl, ul, capa, pos, i;
	hawk_uch_t* ucs;

	for (capa = 0, i = 0; bcs[i]; i++)
	{
		if (hawk_gem_convbtoucstr(gem, bcs[i], &bl, HAWK_NULL, &ul, all) <= -1)  return HAWK_NULL;
		capa += ul;
	}

	ucs = (hawk_uch_t*)hawk_gem_allocmem(gem, (capa + 1) * HAWK_SIZEOF(*ucs));
	if (!ucs) return HAWK_NULL;

	for (pos = 0, i = 0; bcs[i]; i++)
	{
		ul = capa - pos + 1;
		hawk_gem_convbtoucstr (gem, bcs[i], &bl, &ucs[pos], &ul, all);
		pos += ul;
	}

	if (ucslen) *ucslen = capa;
	return ucs;
}

hawk_bch_t* hawk_gem_dupucstrarrtobcstr (hawk_gem_t* gem, const hawk_uch_t* ucs[], hawk_oow_t* bcslen)
{
	hawk_oow_t ul, bl, capa, pos, i;
	hawk_bch_t* bcs;

	for (capa = 0, i = 0; ucs[i]; i++)
	{
		if (hawk_gem_convutobcstr(gem, ucs[i], &ul, HAWK_NULL, &bl) <= -1)  return HAWK_NULL;
		capa += bl;
	}

	bcs = (hawk_bch_t*)hawk_gem_allocmem(gem, (capa + 1) * HAWK_SIZEOF(*bcs));
	if (!bcs) return HAWK_NULL;

	for (pos = 0, i = 0; ucs[i]; i++)
	{
		bl = capa - pos + 1;
		hawk_gem_convutobcstr (gem, ucs[i], &ul, &bcs[pos], &bl);
		pos += bl;
	}

	if (bcslen) *bcslen = capa;
	return bcs;
}


/* ------------------------------------------------------------------------ */

struct fmt_uch_buf_t
{
	hawk_gem_t* gem;
	hawk_uch_t* ptr;
	hawk_oow_t len;
	hawk_oow_t capa;
};
typedef struct fmt_uch_buf_t fmt_uch_buf_t;

static int fmt_put_bchars_to_uch_buf (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	fmt_uch_buf_t* b = (fmt_uch_buf_t*)fmtout->ctx;
	hawk_oow_t bcslen, ucslen;
	int n;

	bcslen = b->capa - b->len;
	ucslen = len;
	n = hawk_conv_bchars_to_uchars_with_cmgr(ptr, &bcslen, &b->ptr[b->len], &ucslen, b->gem->cmgr, 1);
	if (n <= -1) 
	{
		if (n == -2) 
		{
			return 0; /* buffer full. stop */
		}
		else
		{
			hawk_gem_seterrnum (b->gem, HAWK_NULL, HAWK_EECERR);
			return -1;
		}
	}

	return 1; /* success. carry on */
}

static int fmt_put_uchars_to_uch_buf (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	fmt_uch_buf_t* b = (fmt_uch_buf_t*)fmtout->ctx;
	hawk_oow_t n;

	/* this function null-terminates the destination. so give the restored buffer size */
	n = hawk_copy_uchars_to_ucstr(&b->ptr[b->len], b->capa - b->len + 1, ptr, len);
	b->len += n;
	if (n < len)
	{
		hawk_gem_seterrnum (b->gem, HAWK_NULL, HAWK_EBUFFULL);
		return 0; /* stop. insufficient buffer */
	}

	return 1; /* success */
}

hawk_oow_t hawk_gem_vfmttoucstr (hawk_gem_t* gem, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, va_list ap)
{
	hawk_fmtout_t fo;
	fmt_uch_buf_t fb;

	if (bufsz <= 0) return 0;

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = gem->mmgr;
	fo.putbchars = fmt_put_bchars_to_uch_buf;
	fo.putuchars = fmt_put_uchars_to_uch_buf;
	fo.ctx = &fb;

	HAWK_MEMSET (&fb, 0, HAWK_SIZEOF(fb));
	fb.gem = gem;
	fb.ptr = buf;
	fb.capa = bufsz - 1;

	if (hawk_ufmt_outv(&fo, fmt, ap) <= -1) return -1;

	buf[fb.len] = '\0';
	return fb.len;
}

hawk_oow_t hawk_gem_fmttoucstr (hawk_gem_t* gem, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, ...)
{
	hawk_oow_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hawk_gem_vfmttoucstr(gem, buf, bufsz, fmt, ap);
	va_end (ap);

	return x;
}

/* ------------------------------------------------------------------------ */

struct fmt_bch_buf_t
{
	hawk_gem_t* gem;
	hawk_bch_t* ptr;
	hawk_oow_t len;
	hawk_oow_t capa;
};
typedef struct fmt_bch_buf_t fmt_bch_buf_t;


static int fmt_put_bchars_to_bch_buf (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	fmt_bch_buf_t* b = (fmt_bch_buf_t*)fmtout->ctx;
	hawk_oow_t n;

	/* this function null-terminates the destination. so give the restored buffer size */
	n = hawk_copy_bchars_to_bcstr(&b->ptr[b->len], b->capa - b->len + 1, ptr, len);
	b->len += n;
	if (n < len)
	{
		hawk_gem_seterrnum (b->gem, HAWK_NULL, HAWK_EBUFFULL);
		return 0; /* stop. insufficient buffer */
	}

	return 1; /* success */
}


static int fmt_put_uchars_to_bch_buf (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	fmt_bch_buf_t* b = (fmt_bch_buf_t*)fmtout->ctx;
	hawk_oow_t bcslen, ucslen;
	int n;

	bcslen = b->capa - b->len;
	ucslen = len;
	n = hawk_conv_uchars_to_bchars_with_cmgr(ptr, &ucslen, &b->ptr[b->len], &bcslen, b->gem->cmgr);
	b->len += bcslen;
	if (n <= -1)
	{
		if (n == -2)
		{
			return 0; /* buffer full. stop */
		}
		else
		{
			hawk_gem_seterrnum (b->gem, HAWK_NULL, HAWK_EECERR);
			return -1;
		}
	}

	return 1; /* success. carry on */
}

hawk_oow_t hawk_gem_vfmttobcstr (hawk_gem_t* gem, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, va_list ap)
{
	hawk_fmtout_t fo;
	fmt_bch_buf_t fb;

	if (bufsz <= 0) return 0;

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = gem->mmgr;
	fo.putbchars = fmt_put_bchars_to_bch_buf;
	fo.putuchars = fmt_put_uchars_to_bch_buf;
	fo.ctx = &fb;

	HAWK_MEMSET (&fb, 0, HAWK_SIZEOF(fb));
	fb.gem = gem;
	fb.ptr = buf;
	fb.capa = bufsz - 1;

	if (hawk_bfmt_outv(&fo, fmt, ap) <= -1) return -1;

	buf[fb.len] = '\0';
	return fb.len;
}

hawk_oow_t hawk_gem_fmttobcstr (hawk_gem_t* gem, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, ...)
{
	hawk_oow_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hawk_gem_vfmttobcstr(gem, buf, bufsz, fmt, ap);
	va_end (ap);

	return x;
}

