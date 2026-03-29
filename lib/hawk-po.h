#ifndef _HAWK_PO_H_
#define _HAWK_PO_H_

#include <hawk-cmn.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    char *msgctx;
    char *msgid;
    char *msgid_plural;
    char **msgstr;
    int msgstr_count;
    int fuzzy;
    int obsolete;
} PoEntry;

typedef struct {
    PoEntry *entries;
    hawk_oow_t count;
    hawk_oow_t capacity;

    char *charset;
    char *plural_forms_raw;
    char *plural_expr;
    int nplurals;
} PoCatalog;

int po_cat_init(PoCatalog *cat);
void po_cat_fini(PoCatalog *cat);

int po_cat_load_file(PoCatalog *cat, const char *path, char *errbuf, hawk_oow_t errbuf_sz);

const char *po_cat_get(const PoCatalog *cat, const char *msgctx, const char *msgid);
const char* po_cat_gettext (const PoCatalog *cat, const char* msgctx, const char* msgid);
const char *po_cat_nget(const PoCatalog *cat, const char *msgctx, const char *msgid, const char *msgid_plural, unsigned long n);

#if defined(__cplusplus)
}
#endif

#endif
