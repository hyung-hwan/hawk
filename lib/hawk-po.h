#ifndef _HAWK_PO_H_
#define _HAWK_PO_H_

#include <hawk-cmn.h>

typedef struct hawk_poent_t hawk_poent_t;
typedef struct hawk_pocat_t hawk_pocat_t;

struct hawk_poent_t
{
	char* msgctx;
	char* msgid;
	char* msgid_plural;
	char** msgstr;
	int msgstr_count;
	int fuzzy;
	int obsolete;
};

struct hawk_pocat_t
{
	hawk_gem_t* gem;

	hawk_poent_t* entries;
	hawk_oow_t count;
	hawk_oow_t capa;

	char* charset;
	char* plural_forms_raw;
	char* plural_expr;
	int nplurals;
};

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_pocat_t* hawk_pocat_open (
	hawk_gem_t* gem,
	hawk_oow_t  xtnsize
);

HAWK_EXPORT void hawk_pocat_close (
	hawk_pocat_t* pocat
);

HAWK_EXPORT int hawk_pocat_init (
	hawk_pocat_t* pocat,
	hawk_gem_t*   gem
);

HAWK_EXPORT void hawk_pocat_fini (
	hawk_pocat_t* pocat
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_pocat_getxtn (hawk_pocat_t* pocat) { return (void*)(pocat + 1); }
#else
#define hawk_pocat_getxtn(pocat) ((void*)((hawk_pocat_t*)(pocat) + 1))
#endif

HAWK_EXPORT int hawk_pocat_load_file (
	hawk_pocat_t*  pocat,
	const char*    path,
	char*          errbuf,
	hawk_oow_t     errbuf_sz
);

HAWK_EXPORT const char* hawk_pocat_get (
	const hawk_pocat_t* pocat,
	const char*         msgctx,
	const char*         msgid
);

HAWK_EXPORT const char* hawk_pocat_gettext (
	const hawk_pocat_t* pocat,
	const char*         msgctx,
	const char*         msgid
);

HAWK_EXPORT const char* hawk_pocat_nget (
	const hawk_pocat_t* pocat,
	const char*         msgctx,
	const char*         msgid,
	const char*         msgid_plural,
	unsigned long       n
);

#if defined(__cplusplus)
}
#endif

#endif
