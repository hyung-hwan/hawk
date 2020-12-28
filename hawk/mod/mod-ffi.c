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

#include "mod-ffi.h"
#include <hawk-utl.h>
#include "../lib/hawk-prv.h"

#include <errno.h>
#include <string.h>

#include <ffi.h>
#if (HAWK_SIZEOF_LONG_LONG > 0) && !defined(ffi_type_ulonglong)
#	if HAWK_SIZEOF_LONG_LONG == HAWK_SIZEOF_INT32_T
#		define ffi_type_ulonglong ffi_type_uint32
#		define ffi_type_slonglong ffi_type_sint32
#	elif HAWK_SIZEOF_LONG_LONG == HAWK_SIZEOF_INT64_T
#		define ffi_type_ulonglong ffi_type_uint64
#		define ffi_type_slonglong ffi_type_sint64
#	endif
#endif

#define FMTC_NULL '\0' /* internal use only */

#define FMTC_CHAR 'c'
#define FMTC_SHORT 'h'
#define FMTC_INT 'i'
#define FMTC_LONG 'l'
#define FMTC_LONGLONG 'L'
#define FMTC_MBS 's'
#define FMTC_STR 'S'

#define FMTC_INT8 '1'
#define FMTC_INT16 '2'
#define FMTC_INT32 '4'
#define FMTC_INT64 '8'

typedef union ffi_sv_t ffi_sv_t;
union ffi_sv_t
{
	void* p;
	unsigned char uc;
	char c;
	unsigned short int uh;
	short h;
	unsigned int ui;
	int i;
	unsigned long int ul;
	long int l;
#if (HAWK_SIZEOF_LONG_LONG > 0)
	unsigned long long int ull;
	long long int ll;
#endif

	hawk_uint8_t ui8;
	hawk_int8_t i8;
	hawk_uint16_t ui16;
	hawk_int16_t i16;
	hawk_uint32_t ui32;
	hawk_int32_t i32;
#if (HAWK_SIZEOF_INT64_T > 0)
	hawk_uint64_t ui64;
	hawk_int64_t i64;
#endif
};

typedef struct ffi_t ffi_t;
struct ffi_t
{
	void* handle;

	hawk_oow_t arg_count;
	hawk_oow_t arg_max;
	ffi_type** arg_types;
	void** arg_values;
	ffi_sv_t* arg_svs;

	ffi_sv_t ret_sv;
	ffi_cif cif;
	ffi_type* fmtc_to_type[2][128];
};


#define __IDMAP_NODE_T_DATA  ffi_t ffi;
#define __IDMAP_LIST_T_DATA  hawk_ooch_t errmsg[256];
#define __IDMAP_LIST_T ffi_list_t
#define __IDMAP_NODE_T ffi_node_t
#define __INIT_IDMAP_LIST __init_ffi_list
#define __FINI_IDMAP_LIST __fini_ffi_list
#define __MAKE_IDMAP_NODE __new_ffi_node
#define __FREE_IDMAP_NODE __free_ffi_node
#include "../lib/idmap-imp.h"

struct rtx_data_t
{
	ffi_list_t ffi_list;

};
typedef struct rtx_data_t rtx_data_t;

/* ------------------------------------------------------------------------ */
#define ERRNUM_TO_RC(errnum) (-((hawk_int_t)errnum))

static hawk_int_t copy_error_to_ffi_list (hawk_rtx_t* rtx, ffi_list_t* ffi_list)
{
	hawk_errnum_t errnum = hawk_rtx_geterrnum(rtx);
	hawk_copy_oocstr (ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), hawk_rtx_geterrmsg(rtx));
	return ERRNUM_TO_RC(errnum);
}

/*
static hawk_int_t set_error_on_ffi_list_with_errno (hawk_rtx_t* rtx, ffi_list_t* ffi_list, const hawk_ooch_t* title)
{
	int err = errno;

	if (title)
		 hawk_rtx_fmttooocstr (rtx, ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), HAWK_T("%js - %hs"), title, strerror(err));
	else
		 hawk_rtx_fmttooocstr (rtx, ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), HAWK_T("%hs"), strerror(err));
	return ERRNUM_TO_RC(hawk_syserr_to_errnum(err));
}*/

static hawk_int_t set_error_on_ffi_list (hawk_rtx_t* rtx, ffi_list_t* ffi_list, hawk_errnum_t errnum, const hawk_ooch_t* errfmt, ...)
{
	va_list ap;
	if (errfmt)
	{
		va_start (ap, errfmt);
		hawk_rtx_vfmttooocstr (rtx, ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_rtx_fmttooocstr (rtx, ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), HAWK_T("%js"), hawk_geterrstr(hawk_rtx_gethawk(rtx))(errnum));
	}
	return ERRNUM_TO_RC(errnum);
}

static void set_errmsg_on_ffi_list (hawk_rtx_t* rtx, ffi_list_t* ffi_list, const hawk_ooch_t* errfmt, ...)
{
	if (errfmt)
	{
		va_list ap;
		va_start (ap, errfmt);
		hawk_rtx_vfmttooocstr (rtx, ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_copy_oocstr (ffi_list->errmsg, HAWK_COUNTOF(ffi_list->errmsg), hawk_rtx_geterrmsg(rtx));
	}
}

/* ------------------------------------------------------------------------ */

static HAWK_INLINE rtx_data_t* rtx_to_data (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	return (rtx_data_t*)HAWK_RBT_VPTR(pair);
}

static HAWK_INLINE ffi_list_t* rtx_to_ffi_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return &rtx_to_data(rtx, fi)->ffi_list;
}

/* ------------------------------------------------------------------------ */

static ffi_node_t* new_ffi_node (hawk_rtx_t* rtx, ffi_list_t* ffi_list, const hawk_ooch_t* name)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	ffi_node_t* ffi_node;
	
	void* handle;

	ffi_node = __new_ffi_node(rtx, ffi_list);
	if (!ffi_node) return HAWK_NULL;

	if (name)
	{
		hawk_mod_spec_t spec;
		HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
		spec.name = name;
		handle = hawk->prm.modopen(hawk, &spec);
	}
	else
	{
		handle = hawk->prm.modopen(hawk, HAWK_NULL);
	}
	if (!handle) 
	{
		const hawk_ooch_t* olderrmsg;
		olderrmsg = hawk_backuperrmsg(hawk);
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_ENOMEM, HAWK_T("unable to open the '%js' ffi module - %js"), (name? name: HAWK_T("")), olderrmsg);

		__free_ffi_node (rtx, ffi_list, ffi_node);
		return HAWK_NULL;
	}

	ffi_node->ffi.handle = handle;

	ffi_node->ffi.fmtc_to_type[0][FMTC_NULL] = &ffi_type_void;
	ffi_node->ffi.fmtc_to_type[1][FMTC_NULL] = &ffi_type_void;

	ffi_node->ffi.fmtc_to_type[0][FMTC_CHAR] = &ffi_type_schar;
	ffi_node->ffi.fmtc_to_type[1][FMTC_CHAR] = &ffi_type_uchar;
	ffi_node->ffi.fmtc_to_type[0][FMTC_SHORT] = &ffi_type_sshort;
	ffi_node->ffi.fmtc_to_type[1][FMTC_SHORT] = &ffi_type_ushort;
	ffi_node->ffi.fmtc_to_type[0][FMTC_INT] = &ffi_type_sint;
	ffi_node->ffi.fmtc_to_type[1][FMTC_INT] = &ffi_type_uint;
	ffi_node->ffi.fmtc_to_type[0][FMTC_LONG] = &ffi_type_slong;
	ffi_node->ffi.fmtc_to_type[1][FMTC_LONG] = &ffi_type_ulong;
	#if (HAWK_SIZEOF_LONG_LONG > 0)
	ffi_node->ffi.fmtc_to_type[0][FMTC_LONGLONG] = &ffi_type_slonglong;
	ffi_node->ffi.fmtc_to_type[1][FMTC_LONGLONG] = &ffi_type_ulonglong;
	#endif
	ffi_node->ffi.fmtc_to_type[0][FMTC_INT8] = &ffi_type_sint8;
	ffi_node->ffi.fmtc_to_type[1][FMTC_INT8] = &ffi_type_uint8;
	ffi_node->ffi.fmtc_to_type[0][FMTC_INT16] = &ffi_type_sint16;
	ffi_node->ffi.fmtc_to_type[1][FMTC_INT16] = &ffi_type_uint16;
	ffi_node->ffi.fmtc_to_type[0][FMTC_INT32] = &ffi_type_sint32;
	ffi_node->ffi.fmtc_to_type[1][FMTC_INT32] = &ffi_type_uint32;
	ffi_node->ffi.fmtc_to_type[0][FMTC_INT64] = &ffi_type_sint64;
	ffi_node->ffi.fmtc_to_type[1][FMTC_INT64] = &ffi_type_uint64;

	ffi_node->ffi.fmtc_to_type[0][FMTC_MBS] = &ffi_type_pointer;
	ffi_node->ffi.fmtc_to_type[1][FMTC_MBS] = &ffi_type_pointer;
	ffi_node->ffi.fmtc_to_type[0][FMTC_STR] = &ffi_type_pointer;
	ffi_node->ffi.fmtc_to_type[1][FMTC_STR] = &ffi_type_pointer;

	return ffi_node;
}


static void free_ffi_node (hawk_rtx_t* rtx, ffi_list_t* ffi_list, ffi_node_t* ffi_node)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);

	if (ffi_node->ffi.arg_types)
	{
		hawk_rtx_freemem (rtx, ffi_node->ffi.arg_types);
		ffi_node->ffi.arg_types = HAWK_NULL;
	}
	if (ffi_node->ffi.arg_values)
	{
		hawk_rtx_freemem (rtx, ffi_node->ffi.arg_values);
		ffi_node->ffi.arg_values = HAWK_NULL;
	}
	if (ffi_node->ffi.arg_svs)
	{
		hawk_rtx_freemem (rtx, ffi_node->ffi.arg_svs);
		ffi_node->ffi.arg_svs = HAWK_NULL;
	}
	ffi_node->ffi.arg_max = 0;
	ffi_node->ffi.arg_count = 0;

	hawk->prm.modclose (hawk, ffi_node->ffi.handle);
	ffi_node->ffi.handle = HAWK_NULL;

	__free_ffi_node (rtx, ffi_list, ffi_node);
}

static HAWK_INLINE ffi_node_t* get_ffi_list_node (ffi_list_t* ffi_list, hawk_int_t id)
{
	if (id < 0 || id >= ffi_list->map.high || !ffi_list->map.tab[id]) return HAWK_NULL;
	return ffi_list->map.tab[id];
}

static ffi_node_t* get_ffi_list_node_with_arg (hawk_rtx_t* rtx, ffi_list_t* ffi_list, hawk_val_t* arg, hawk_int_t* rx)
{
	hawk_int_t id;
	ffi_node_t* ffi_node;


	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		 *rx = ERRNUM_TO_RC(hawk_rtx_geterrnum(rtx));
		 set_errmsg_on_ffi_list (rtx, ffi_list, HAWK_T("illegal handle value"));
		 return HAWK_NULL;
	}
	else if (!(ffi_node = get_ffi_list_node(ffi_list, id)))
	{
		 *rx = ERRNUM_TO_RC(HAWK_EINVAL);
		 set_errmsg_on_ffi_list (rtx, ffi_list, HAWK_T("invalid handle - %zd"), (hawk_oow_t)id);
		 return HAWK_NULL;
	}

	return ffi_node;
}

/* ------------------------------------------------------------------------ */

static int fnc_open (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	ffi_list_t* ffi_list;
	ffi_node_t* ffi_node = HAWK_NULL;
	hawk_int_t ret = -1;
	hawk_val_t* retv;
	hawk_val_t* a0;
	hawk_oocs_t name;

	ffi_list = rtx_to_ffi_list(rtx, fi);

	if (hawk_rtx_getnargs(rtx) >= 1)
	{
		a0 = hawk_rtx_getarg(rtx, 0);
		name.ptr = hawk_rtx_getvaloocstr(rtx, a0, &name.len);
		if (HAWK_UNLIKELY(!name.ptr))
		{
			ret = copy_error_to_ffi_list (rtx, ffi_list);
			goto done;
		}

		if (name.len != hawk_count_oocstr(name.ptr))
		{
			ret = set_error_on_ffi_list(rtx, ffi_list, HAWK_EINVAL, HAWK_T("invalid ffi module name '%.*js'"), name.len, name.ptr);
			goto done;
		}
	}
	else
	{
		name.ptr = HAWK_NULL;
		a0 = HAWK_NULL;
	}

	ffi_node = new_ffi_node(rtx, ffi_list, name.ptr);
	if (ffi_node) ret = ffi_node->id;
	else ret = copy_error_to_ffi_list (rtx, ffi_list);

done:
	if (name.ptr) hawk_rtx_freevaloocstr (rtx, a0, name.ptr);

	retv = hawk_rtx_makeintval(rtx, ret);
	if (HAWK_UNLIKELY(!retv))
	{
		if (ffi_node) free_ffi_node (rtx, ffi_list, ffi_node);
		return -1;
	}

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	ffi_list_t* ffi_list;
	ffi_node_t* ffi_node;
	hawk_int_t ret = -1;

	ffi_list = rtx_to_ffi_list(rtx, fi);
	ffi_node = get_ffi_list_node_with_arg(rtx, ffi_list, hawk_rtx_getarg(rtx, 0), &ret);
	if (ffi_node)
	{
		if (ffi_node->ffi.arg_types)
		{
			hawk_rtx_freemem (rtx, ffi_node->ffi.arg_types);
			ffi_node->ffi.arg_types = HAWK_NULL;
		}
		if (ffi_node->ffi.arg_values)
		{
			hawk_rtx_freemem (rtx, ffi_node->ffi.arg_values);
			ffi_node->ffi.arg_values = HAWK_NULL;
		}
		if (ffi_node->ffi.arg_svs)
		{
			hawk_rtx_freemem (rtx, ffi_node->ffi.arg_svs);
			ffi_node->ffi.arg_svs = HAWK_NULL;
		}
		ffi_node->ffi.arg_max = 0;
		ffi_node->ffi.arg_count = 0;

		hawk->prm.modclose (hawk, ffi_node->ffi.handle);
		ffi_node->ffi.handle = HAWK_NULL;
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static HAWK_INLINE int add_ffi_arg (hawk_rtx_t* rtx, ffi_list_t* ffi_list, ffi_node_t* ffi_node, hawk_ooch_t fmtc, int _unsigned, hawk_val_t* arg, hawk_int_t* rx)
{
	ffi_t* ffi = &ffi_node->ffi;

	if (ffi->arg_count >= ffi->arg_max)
	{
		ffi_type** ttmp;
		void** vtmp;
		ffi_sv_t* stmp;
		hawk_oow_t newmax;

		newmax = ffi->arg_max + 16; /* TODO: adjust this? */

		ttmp = hawk_rtx_reallocmem(rtx, ffi->arg_types, HAWK_SIZEOF(*ttmp) * newmax);
		if (HAWK_UNLIKELY(!ttmp)) goto oops;
		ffi->arg_types = ttmp;

		vtmp = hawk_rtx_reallocmem(rtx, ffi->arg_values, HAWK_SIZEOF(*vtmp) * newmax);
		if (HAWK_UNLIKELY(!vtmp)) goto oops;
		ffi->arg_values = vtmp;

		stmp = hawk_rtx_reallocmem(rtx, ffi->arg_svs, HAWK_SIZEOF(*stmp) * newmax);
		if (HAWK_UNLIKELY(!stmp)) goto oops;
		ffi->arg_svs = stmp;

		ffi->arg_max = newmax;
	}

	switch (fmtc)
	{
		case FMTC_INT8:
		{
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ui8;
				ffi->arg_svs[ffi->arg_count].ui8 = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].i8;
				ffi->arg_svs[ffi->arg_count].i8 = v;
			}
			break;
		}

		case FMTC_INT16:
		{
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ui16;
				ffi->arg_svs[ffi->arg_count].ui16 = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].i16;
				ffi->arg_svs[ffi->arg_count].i16 = v;
			}
			break;
		}

		case FMTC_INT32:
		{
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ui32;
				ffi->arg_svs[ffi->arg_count].ui32 = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].i32;
				ffi->arg_svs[ffi->arg_count].i32 = v;
			}
			break;
		}

		case FMTC_INT64:
		{
		#if (HAWK_SIZEOF_INT64_T > 0)
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ui64;
				ffi->arg_svs[ffi->arg_count].ui64 = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].i64;
				ffi->arg_svs[ffi->arg_count].i64 = v;
			}
			break;
		#else
			goto inval_sig_ch;
		#endif
		}

		case FMTC_CHAR: /* this is a byte */
		{
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].uc;
				ffi->arg_svs[ffi->arg_count].uc = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].c;
				ffi->arg_svs[ffi->arg_count].c = v;
			}
			break;
		}

		case FMTC_SHORT:
		{
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].uh;
				ffi->arg_svs[ffi->arg_count].uh = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].h;
				ffi->arg_svs[ffi->arg_count].h = v;
			}
			break;
		}

		case FMTC_INT:
		{
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ui;
				ffi->arg_svs[ffi->arg_count].ui = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].i;
				ffi->arg_svs[ffi->arg_count].i = v;
			}
			break;
		}

		case FMTC_LONG:
		{
			hawk_int_t v;
		arg_as_long:
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ul;
				ffi->arg_svs[ffi->arg_count].ul = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].l;
				ffi->arg_svs[ffi->arg_count].l = v;
			}
			break;
		}

		case FMTC_LONGLONG:
		{
		#if (HAWK_SIZEOF_LONG_LONG <= HAWK_SIZEOF_LONG)
			goto arg_as_long;
		#else
			hawk_int_t v;
			if (hawk_rtx_valtoint(rtx, arg, &v) <=  -1) goto inval_arg_value;
			if (_unsigned)
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ull;
				ffi->arg_svs[ffi->arg_count].ull = v;
			}
			else
			{
				ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].ll;
				ffi->arg_svs[ffi->arg_count].ll = v;
			}
			break;
		#endif
		}

		case FMTC_MBS:
		{
			hawk_bch_t* ptr;
			hawk_oow_t len;

			ptr = hawk_rtx_valtobcstrdup(rtx, arg,  &len);
			if(HAWK_UNLIKELY(!ptr)) goto inval_arg_value;

			ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].p;
			ffi->arg_svs[ffi->arg_count].p = ptr;
			break;
		}

		case FMTC_STR:
		{
			hawk_uch_t* ptr;
			hawk_oow_t len;

			ptr = hawk_rtx_valtooocstrdup(rtx, arg, &len);
			if(HAWK_UNLIKELY(!ptr)) goto inval_arg_value;

			ffi->arg_values[ffi->arg_count] = &ffi->arg_svs[ffi->arg_count].p;
			ffi->arg_svs[ffi->arg_count].p = ptr;
			break;
		}

		default:
			*rx = set_error_on_ffi_list(rtx, ffi_list, HAWK_EINVAL, HAWK_T("invalid argument type signature - %jc"), fmtc);
			return -1;
	}

	ffi->arg_types[ffi->arg_count] = ffi->fmtc_to_type[_unsigned][fmtc];
	ffi->arg_count++;
	return 0;

inval_arg_value:
	*rx = set_error_on_ffi_list(rtx, ffi_list, HAWK_EINVAL, HAWK_T("invalid argument value - %O"), arg);
	return -1;

oops:
	*rx = copy_error_to_ffi_list(rtx, ffi_list);
	return -1;
}

static int capture_return (hawk_rtx_t* rtx, ffi_list_t* ffi_list, ffi_node_t* ffi_node, hawk_ooch_t fmtc, int _unsigned, hawk_oow_t refidx, hawk_int_t* rx)
{
	hawk_val_t* r;
	ffi_t* ffi = &ffi_node->ffi;

	switch (fmtc)
	{
/* TODO: support more types... */
/* TODO: proper return value conversion */
		case FMTC_INT8:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ui8: ffi->ret_sv.i8);
			break;

		case FMTC_INT16:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ui16: ffi->ret_sv.i16);
			break;

		case FMTC_INT32:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ui32: ffi->ret_sv.i32);
			break;

		case FMTC_INT64:
		#if (HAWK_SIZEOF_INT64_T > 0)
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ui64: ffi->ret_sv.i64);
			break;
		#else
			hawk_rtx_seterrbfmt (hawk, HAWK_NULL, HAWK_EINVAL, "invalid return type signature - %jc", fmtc);
			goto oops;
		#endif

		case FMTC_CHAR:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.uc: ffi->ret_sv.c);
			break;

		case FMTC_SHORT:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.uh: ffi->ret_sv.h);
			break;

		case FMTC_INT:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ui: ffi->ret_sv.i);
			break;

		case FMTC_LONG:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ul: ffi->ret_sv.l);
			break;

		case FMTC_LONGLONG:
			r = hawk_rtx_makeintval(rtx, _unsigned? ffi->ret_sv.ull: ffi->ret_sv.ll);
			break;

/* TODO: capture char or bchr??? not as numeric but as a character... */
		case FMTC_MBS:
			r = hawk_rtx_makembsvalwithbcstr(rtx, ffi->ret_sv.p);
			break;

		case FMTC_STR:
			r = hawk_rtx_makestrvalwithoocstr(rtx, ffi->ret_sv.p);
			break;

		default:
			/* capture no return value */
			return 0;
	}
	if (HAWK_UNLIKELY(!r)) goto oops;

	if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, refidx), r) <= -1) goto oops;
	return 0;

oops:
	*rx = copy_error_to_ffi_list(rtx, ffi_list);
	return 0;
}

/*
BEGIN {
        t = ffi::open("libc.so.6");
        if (t <= -1)
        {
                print t, ffi::errmsg();
                return -1;
        }

        ##t = ffi::call(t, r, "printf", "s|ii>X", "%d  **%%** %d\n", 10, 20);
        x = ffi::call(t, r, "printf", "s|ii>i", "%d  **%%** %d\n", 10, 20);
        if (x <= -1)
        {
                print "unable to call -", t, ffi::errmsg();
        }
        else
        {
                print "return value => ", r;
        }

        x = ffi::call(t, r, "getenv", "s>s", "PATH");
        if (x <= -1)
        {
                print "unable to call -", t, ffi::errmsg();
        }
        else
        {
                print "return value => ", r;
        }

        ffi::close (t);
} */
static int fnc_call (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	ffi_list_t* ffi_list;
	ffi_node_t* ffi_node;
	ffi_t* ffi;
	hawk_int_t ret;

	hawk_val_t* a2;
	hawk_oocs_t fun;
	void* funx;

	hawk_val_t* a3;
	hawk_oocs_t sig;

	hawk_oow_t i, j, nfixedargs, _unsigned;
	int vbar = 0;
	hawk_ooch_t fmtc;

	ffi_status fs;

#define FNC_CALL_ARG_BASE 4

	ret = -1;
	ffi = HAWK_NULL;
	fun.ptr = HAWK_NULL;
	sig.ptr = HAWK_NULL;
	a2 = HAWK_NULL;
	a3 = HAWK_NULL;

	/* ffi::call (ffi-handle, return-value, "function name", "signature", argument....); */
	ffi_list = rtx_to_ffi_list(rtx, fi);
	ffi_node = get_ffi_list_node_with_arg(rtx, ffi_list, hawk_rtx_getarg(rtx, 0), &ret);
	if (!ffi_node) goto done;

	a2 = hawk_rtx_getarg(rtx, 2);
	fun.ptr = hawk_rtx_getvaloocstr(rtx, a2, &fun.len);
	if (HAWK_UNLIKELY(!fun.ptr))
	{
		ret = copy_error_to_ffi_list (rtx, ffi_list);
		goto done;
	}

	if (hawk_count_oocstr(fun.ptr) != fun.len)
	{
		ret = set_error_on_ffi_list(rtx, ffi_list, HAWK_EINVAL,  HAWK_T("invalid function name - %.*js"), fun.len, fun.ptr);
		goto done;
	}

	a3 = hawk_rtx_getarg(rtx, 3);
	sig.ptr = hawk_rtx_getvaloocstr(rtx, a3, &sig.len);
	if (HAWK_UNLIKELY(!sig.ptr))
	{
		ret = copy_error_to_ffi_list (rtx, ffi_list);
		goto done;
	}

	ffi = &ffi_node->ffi;
	ffi->arg_count = 0;

	funx = hawk->prm.modgetsym(hawk, ffi->handle, fun.ptr);
	if (!funx)
	{
		const hawk_ooch_t* olderr = hawk_backuperrmsg(hawk);
		ret = set_error_on_ffi_list(rtx, ffi_list, HAWK_ENOENT, HAWK_T("unable to find function %js - %js "), fun.ptr, olderr);
		goto done;
	}

	/* check argument signature */
	for (i = 0, j = 0, nfixedargs = 0, _unsigned = 0; i < sig.len; i++)
	{
		fmtc = sig.ptr[i];
		if (fmtc == ' ') continue;

		if (fmtc == '>') /* end of signature. return value expected */
		{
			i++;
			if (!vbar) nfixedargs = j;
			break;
		}
		else if (fmtc == '|')
		{
			if (!vbar)
			{
				nfixedargs = j;
				vbar = 1;
			}
			continue;
		}
		else if (fmtc == 'u')
		{
			_unsigned = 1;
			continue;
		}

		/* more items in signature than the actual argument */  
		if (j >= hawk_rtx_getnargs(rtx) - FNC_CALL_ARG_BASE) 
		{
			ret = set_error_on_ffi_list(rtx, ffi_list, HAWK_EINVAL, HAWK_T("two few arguments for the argument signature"));
			goto done;
		}

		if (add_ffi_arg(rtx, ffi_list, ffi_node, fmtc, _unsigned, hawk_rtx_getarg(rtx, j + FNC_CALL_ARG_BASE), &ret) <= -1) goto done;
		_unsigned = 0;
		j++;
	}

	while (i < sig.len && sig.ptr[i] == ' ') i++; /* skip all spaces after > */
	fmtc = (i >= sig.len)? FMTC_NULL: sig.ptr[i];
	if (fmtc == 'u') 
	{
		_unsigned = 1;
		fmtc = (i >= sig.len)? FMTC_NULL: sig.ptr[i];
	}
	else
	{
		_unsigned = 0;
	}

	if (!ffi->fmtc_to_type[0][fmtc])
	{
		ret = set_error_on_ffi_list(rtx, ffi_list, HAWK_EINVAL, HAWK_T("invalid return type signature - %jc"), fmtc);
		goto done;
	}

	fs = (nfixedargs == j)? ffi_prep_cif(&ffi->cif, FFI_DEFAULT_ABI, j, ffi->fmtc_to_type[0][fmtc], ffi->arg_types):
	                        ffi_prep_cif_var(&ffi->cif, FFI_DEFAULT_ABI, nfixedargs, j, ffi->fmtc_to_type[0][fmtc], ffi->arg_types);
	if (fs != FFI_OK)
	{
		ret = set_error_on_ffi_list(rtx, ffi_list, HAWK_ESYSERR, HAWK_T("unable to prepare the ffi_cif structure"));
		goto done;
	}

	ffi_call (&ffi->cif, FFI_FN(funx), &ffi->ret_sv, ffi->arg_values);

	if (fmtc != FMTC_NULL && capture_return(rtx, ffi_list, ffi_node, fmtc, _unsigned, 1, &ret) <= -1) goto done;
	ret = 0;

done:
	if (ffi)
	{
		for (i = 0; i  < ffi->arg_count; i++)
		{
			if (ffi->arg_types[i] == &ffi_type_pointer)
			{
				hawk_rtx_freemem (rtx, ffi->arg_svs[i].p);
			}
		}
	}

	if (sig.ptr) hawk_rtx_freevaloocstr(rtx, a3, sig.ptr);
	if (fun.ptr) hawk_rtx_freevaloocstr(rtx, a2, fun.ptr);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	ffi_list_t* ffi_list;
	hawk_val_t* retv;

	ffi_list = rtx_to_ffi_list(rtx, fi);
	retv = hawk_rtx_makestrvalwithoocstr(rtx, ffi_list->errmsg);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */
#define A_MAX HAWK_TYPE_MAX(hawk_oow_t)

static hawk_mod_fnc_tab_t fnctab[] =
{
	{ HAWK_T("call"),      { { 4, A_MAX, HAWK_T("vrv") },  fnc_call,     0 } },
	{ HAWK_T("close"),     { { 1, 1, HAWK_NULL },  fnc_close,     0 } },
	{ HAWK_T("errmsg"),    { { 0, 0, HAWK_NULL },  fnc_errmsg,    0 } },
	{ HAWK_T("open"),      { { 0, 1, HAWK_NULL },  fnc_open,      0 } },
};

/* ------------------------------------------------------------------------ */

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	rtx_data_t data, * datap;
	hawk_rbt_pair_t* pair;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_MEMSET (&data, 0, HAWK_SIZEOF(data));
	pair = hawk_rbt_insert(rbt, &rtx, HAWK_SIZEOF(rtx), &data, HAWK_SIZEOF(data));
	if (HAWK_UNLIKELY(!pair)) return -1;

	datap = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	__init_ffi_list (rtx, &datap->ffi_list);

	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	hawk_rbt_pair_t* pair;

	rbt = (hawk_rbt_t*)mod->ctx;

	/* garbage clean-up */
	pair = hawk_rbt_search(rbt, &rtx, HAWK_SIZEOF(rtx));
	if (pair)
	{
		rtx_data_t* data;

		data = (rtx_data_t*)HAWK_RBT_VPTR(pair);

		__fini_ffi_list (rtx, &data->ffi_list);

		hawk_rbt_delete (rbt, &rtx, HAWK_SIZEOF(rtx));
	}
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_ASSERT (HAWK_RBT_SIZE(rbt) == 0);
	hawk_rbt_close (rbt);
}

int hawk_mod_ffi (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;
	mod->init = init;
	mod->fini = fini;

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (HAWK_UNLIKELY(!rbt)) return -1;

	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));
	mod->ctx = rbt;

	return 0;
}
