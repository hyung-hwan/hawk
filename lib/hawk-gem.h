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

#ifndef _HAWK_GEM_H_
#define _HAWK_GEM_H_

#include <hawk-cmn.h>
#include <hawk-skad.h>
#include <stdarg.h>

#define HAWK_SKAD_TO_OOCSTR_ADDR (1 << 0)
#define HAWK_SKAD_TO_OOCSTR_PORT (1 << 1)
#define HAWK_SKAD_TO_UCSTR_ADDR HAWK_SKAD_TO_OOCSTR_ADDR
#define HAWK_SKAD_TO_UCSTR_PORT HAWK_SKAD_TO_OOCSTR_PORT
#define HAWK_SKAD_TO_BCSTR_ADDR HAWK_SKAD_TO_OOCSTR_ADDR
#define HAWK_SKAD_TO_BCSTR_PORT HAWK_SKAD_TO_OOCSTR_PORT

typedef struct hawk_ifcfg_t hawk_ifcfg_t;

enum hawk_ifcfg_flag_t
{
	HAWK_IFCFG_UP       = (1 << 0),
	HAWK_IFCFG_RUNNING  = (1 << 1),
	HAWK_IFCFG_BCAST    = (1 << 2),
	HAWK_IFCFG_PTOP     = (1 << 3), /* peer to peer */
	HAWK_IFCFG_LINKUP   = (1 << 4),
	HAWK_IFCFG_LINKDOWN = (1 << 5)
};

enum hawk_ifcfg_type_t
{
	HAWK_IFCFG_IN4 = HAWK_AF_INET,
	HAWK_IFCFG_IN6 = HAWK_AF_INET6
};

typedef enum hawk_ifcfg_type_t hawk_ifcfg_type_t;
struct hawk_ifcfg_t
{
	hawk_ifcfg_type_t type;     /* in */
	hawk_ooch_t       name[64]; /* in/out */
	unsigned int      index;    /* in/out */

	/* ---------------- */
	int               flags;    /* out */
	int               mtu;      /* out */

	hawk_skad_t       addr;     /* out */
	hawk_skad_t       mask;     /* out */
	hawk_skad_t       ptop;     /* out */
	hawk_skad_t       bcast;    /* out */

	/* ---------------- */

	/* TODO: add hwaddr?? */	
	/* i support ethernet only currently */
	hawk_uint8_t      ethw[6];  /* out */
};

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_gem_allocmem_noseterr (hawk_gem_t* gem, hawk_oow_t size) { return HAWK_MMGR_ALLOC(gem->mmgr, size); }
static HAWK_INLINE void* hawk_gem_reallocmem_noseterr (hawk_gem_t* gem, void* ptr, hawk_oow_t size) { return HAWK_MMGR_REALLOC(gem->mmgr, ptr, size); }
#else
#define hawk_gem_allocmem_noseterr(gem, size) (HAWK_MMGR_ALLOC((gem)->mmgr, size))
#define hawk_gem_reallocmem_noseterr(gem, ptr, size) (HAWK_MMGR_REALLOC((gem)->mmgr, ptr, size))
#endif

HAWK_EXPORT void* hawk_gem_allocmem (
	hawk_gem_t*       gem,
	hawk_oow_t        size
);

HAWK_EXPORT void* hawk_gem_callocmem (
	hawk_gem_t*       gem,
	hawk_oow_t        size
);

HAWK_EXPORT void* hawk_gem_reallocmem (
	hawk_gem_t*       gem,
	void*             ptr,
	hawk_oow_t        size
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_gem_freemem (hawk_gem_t* gem, void* ptr) { HAWK_MMGR_FREE (gem->mmgr, ptr); }
#else
#define hawk_gem_freemem(gem, ptr) HAWK_MMGR_FREE((gem)->mmgr, ptr);
#endif


void* hawk_gem_callocmem_noseterr (
	hawk_gem_t*       gem,
	hawk_oow_t        size
);

/* ----------------------------------------------------------------------- */

HAWK_EXPORT hawk_uch_t* hawk_gem_dupucstr (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t*       _ucslen
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbcstr (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t*       _bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupuchars (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t        ucslen
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbchars (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupucs (
	hawk_gem_t*       gem,
	const hawk_ucs_t* ucs
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbcs (
	hawk_gem_t*       gem,
	const hawk_bcs_t* bcs
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupucstrarr (
	hawk_gem_t*       gem,
	const hawk_uch_t* str[],
	hawk_oow_t*       len
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbcstrarr (
	hawk_gem_t*       gem,
	const hawk_bch_t* str[],
	hawk_oow_t*       len
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_gem_dupoocstr    hawk_gem_dupucstr
#	define hawk_gem_dupoochars   hawk_gem_dupuchars
#	define hawk_gem_dupoocs      hawk_gem_dupucs
#	define hawk_gem_dupoocstrarr hawk_gem_dupucstrarr
#else
#	define hawk_gem_dupoocstr    hawk_gem_dupbcstr
#	define hawk_gem_dupoochars   hawk_gem_dupbchars
#	define hawk_gem_dupoocs      hawk_gem_dupbcs
#	define hawk_gem_dupoocstrarr hawk_gem_dupbcstrarr
#endif

/* ----------------------------------------------------------------------- */

HAWK_EXPORT int hawk_gem_convbtouchars (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	int               all
);

HAWK_EXPORT int hawk_gem_convutobchars (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t*       ucslen,
	hawk_bch_t*       bcs,
	hawk_oow_t*       bcslen
);

HAWK_EXPORT int hawk_gem_convbtoucstr (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	int               all
);

HAWK_EXPORT int hawk_gem_convutobcstr (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t*       ucslen,
	hawk_bch_t*       bcs,
	hawk_oow_t*       bcslen
);

/* ----------------------------------------------------------------------- */


HAWK_EXPORT hawk_uch_t* hawk_gem_dupbtouchars (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs,
	hawk_oow_t         bcslen,
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_duputobchars (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs,
	hawk_oow_t         ucslen,
	hawk_oow_t*        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupb2touchars (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs1,
	hawk_oow_t         bcslen1,
	const hawk_bch_t*  bcs2,
	hawk_oow_t         bcslen2,
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupu2tobchars (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs1,
	hawk_oow_t         ucslen1,
	const hawk_uch_t*  ucs2,
	hawk_oow_t         ucslen2,
	hawk_oow_t*        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupbtoucstr (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs,
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_duputobcstr (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs,
	hawk_oow_t*        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupbtoucharswithcmgr (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs,
	hawk_oow_t         _bcslen,
	hawk_oow_t*        _ucslen,
	hawk_cmgr_t*       cmgr,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_duputobcharswithcmgr (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs,
	hawk_oow_t         _ucslen,
	hawk_oow_t*        _bcslen,
	hawk_cmgr_t*       cmgr
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupbcstrarrtoucstr (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs[],
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupucstrarrtobcstr (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs[],
	hawk_oow_t*        bcslen
);


/* ----------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_gem_vfmttoucstr (
	hawk_gem_t*       gem,
	hawk_uch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_uch_t* fmt,
	va_list           ap
);

HAWK_EXPORT hawk_oow_t hawk_gem_fmttoucstr (
	hawk_gem_t*       gem,
	hawk_uch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_uch_t* fmt,
	...
);

HAWK_EXPORT hawk_oow_t hawk_gem_vfmttobcstr (
	hawk_gem_t*       gem,
	hawk_bch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_bch_t* fmt,
	va_list           ap
);

HAWK_EXPORT hawk_oow_t hawk_gem_fmttobcstr (
	hawk_gem_t*       gem,
	hawk_bch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_bch_t* fmt,
	...
);

/* ----------------------------------------------------------------------- */

HAWK_EXPORT int hawk_gem_ucharstoskad (
	hawk_gem_t*        gem,
	const hawk_uch_t*  str,
	hawk_oow_t         len,
	hawk_skad_t*       skad
);

HAWK_EXPORT int hawk_gem_bcharstoskad (
	hawk_gem_t*        gem,
	const hawk_bch_t*  str,
	hawk_oow_t         len,
	hawk_skad_t*       skad
);

#define hawk_gem_ucstrtoskad(gem,str,skad) hawk_gem_ucharstoskad(gem, str, hawk_count_ucstr(str), skad)
#define hawk_gem_bcstrtoskad(gem,str,skad) hawk_gem_bcharstoskad(gem, str, hawk_count_bcstr(str), skad)

HAWK_EXPORT hawk_oow_t hawk_gem_skadtoucstr (
	hawk_gem_t*        gem,
	const hawk_skad_t* skad,
	hawk_uch_t*        buf,
	hawk_oow_t         len,
	int                flags
);

HAWK_EXPORT hawk_oow_t hawk_gem_skadtobcstr (
	hawk_gem_t*        gem,
	const hawk_skad_t* skad,
	hawk_bch_t*        buf,
	hawk_oow_t         len,
	int                flags
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_gem_oocstrtoskad hawk_gem_ucstrtoskad
#	define hawk_gem_oocharstoskad hawk_gem_ucharstoskad
#	define hawk_gem_skadtooocstr hawk_gem_skadtoucstr
#else
#	define hawk_gem_oocstrtoskad hawk_gem_bcstrtoskad
#	define hawk_gem_oocharstoskad hawk_gem_bcharstoskad
#	define hawk_gem_skadtooocstr hawk_gem_skadtobcstr
#endif
/* ----------------------------------------------------------------------- */

HAWK_EXPORT int hawk_gem_bcstrtoifindex (
	hawk_gem_t*        gem,
	const hawk_bch_t*  ptr,
	unsigned int*      index
);

HAWK_EXPORT int hawk_gem_bcharstoifindex (
	hawk_gem_t*        gem,
	const hawk_bch_t*  ptr,
	hawk_oow_t         len,
	unsigned int*      index
);

HAWK_EXPORT int hawk_gem_ucstrtoifindex (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ptr,
	unsigned int*      index
);

HAWK_EXPORT int hawk_gem_ucharstoifindex (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ptr,
	hawk_oow_t         len,
	unsigned int*      index
);

HAWK_EXPORT int hawk_gem_ifindextobcstr (
	hawk_gem_t*        gem,
	unsigned int       index,
	hawk_bch_t*        buf,
	hawk_oow_t         len
);

HAWK_EXPORT int hawk_gem_ifindextoucstr (
	hawk_gem_t*        gem,
	unsigned int       index,
	hawk_uch_t*        buf,
	hawk_oow_t         len
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_gem_oocstrtoifindex hawk_gem_ucstrtoifindex
#	define hawk_gem_oocharstoifindex hawk_gem_ucharstoifindex
#	define hawk_gem_ifindextooocstr hawk_gem_ifindextoucstr
#else
#	define hawk_gem_oocstrtoifindex hawk_gem_bcstrtoifindex
#	define hawk_gem_oocharstoifindex hawk_gem_bcharstoifindex
#	define hawk_gem_ifindextooocstr hawk_gem_ifindextobcstr
#endif

HAWK_EXPORT int hawk_gem_getifcfg (
	hawk_gem_t*     gem,
	hawk_ifcfg_t*   cfg
);

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_errnum_t hawk_gem_geterrnum (hawk_gem_t* gem) { return gem->errnum; }
static HAWK_INLINE const hawk_loc_t* hawk_gem_geterrloc (hawk_gem_t* gem) { return &gem->errloc; }
#else
#define hawk_gem_geterrnum(gem) (((hawk_gem_t*)(gem))->errnum) 
#define hawk_gem_geterrloc(gem) (&((hawk_gem_t*)(gem))->errloc) 
#endif

HAWK_EXPORT void hawk_gem_geterrinf (
	hawk_gem_t*         gem,
	hawk_errinf_t*      errinf
);

HAWK_EXPORT void hawk_gem_geterror (
	hawk_gem_t*         gem,
	hawk_errnum_t*      errnum,
	const hawk_ooch_t** errmsg,
	hawk_loc_t*         errloc
);

HAWK_EXPORT const hawk_bch_t* hawk_gem_geterrbmsg (
	hawk_gem_t* gem
);

HAWK_EXPORT const hawk_uch_t* hawk_gem_geterrumsg (
	hawk_gem_t* gem
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_gem_geterrmsg hawk_gem_geterrbmsg
#else
#	define hawk_gem_geterrmsg hawk_gem_geterrumsg
#endif

HAWK_EXPORT void hawk_gem_seterrinf (
	hawk_gem_t*          gem,
	const hawk_errinf_t* errinf
);

HAWK_EXPORT void hawk_gem_seterror (
	hawk_gem_t*          gem,
	const hawk_loc_t*    errloc,
	hawk_errnum_t        errnum,
	const hawk_oocs_t*   errarg
);

HAWK_EXPORT void hawk_gem_seterrnum (
	hawk_gem_t*       gem,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum
);

HAWK_EXPORT void hawk_gem_seterrbfmt (
	hawk_gem_t*         gem,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_bch_t*   errfmt,
	...
);

HAWK_EXPORT void hawk_gem_seterrufmt (
	hawk_gem_t*         gem,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_uch_t*   errfmt,
	...
);

HAWK_EXPORT void hawk_gem_seterrbvfmt (
	hawk_gem_t*         gem,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_bch_t*   errfmt,
	va_list             ap
);

HAWK_EXPORT void hawk_gem_seterruvfmt (
	hawk_gem_t*         gem,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_uch_t*   errfmt,
	va_list             ap
);

HAWK_EXPORT const hawk_ooch_t* hawk_gem_backuperrmsg (
	hawk_gem_t* gem
);

HAWK_EXPORT int hawk_gem_buildrex (
	hawk_gem_t*         gem,
	const hawk_ooch_t*  ptn,
	hawk_oow_t          len,
	int                 nobound,
	hawk_tre_t**        code,
	hawk_tre_t**        icode
);

HAWK_EXPORT void hawk_gem_freerex (
	hawk_gem_t*         gem,
	hawk_tre_t*         code,
	hawk_tre_t*         icode
);

#if defined(__cplusplus)
}
#endif

#endif
