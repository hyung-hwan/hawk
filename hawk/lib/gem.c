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

/* ------------------------------------------------------------------ */

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
