#include <hawk-cmn.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if HAWK_SIZEOF_UCH_T == HAWK_SIZEOF_INT16_T
	#define MAX_CHAR 0xFFFF
#else
	/*#define MAX_CHAR 0xE01EF*/
	#define MAX_CHAR 0x10FFFF
#endif

#define UCH_PROP_PAGE_SIZE 256
#define MAX_UCH_PROP_PAGE_COUNT ((MAX_CHAR + UCH_PROP_PAGE_SIZE) / UCH_PROP_PAGE_SIZE)

typedef struct prop_page_t prop_page_t;
struct prop_page_t
{
	size_t no;
	hawk_uint16_t props[UCH_PROP_PAGE_SIZE];
	prop_page_t* next;
};

size_t prop_page_count = 0;
prop_page_t* prop_pages = NULL;

size_t prop_map_count = 0;
prop_page_t* prop_maps[MAX_UCH_PROP_PAGE_COUNT];

enum
{
	UCH_PROP_UPPER  = (1 << 0),
	UCH_PROP_LOWER  = (1 << 1),
	UCH_PROP_ALPHA  = (1 << 2),
	UCH_PROP_DIGIT  = (1 << 3),
	UCH_PROP_XDIGIT = (1 << 4),
	UCH_PROP_ALNUM  = (1 << 5),
	UCH_PROP_SPACE  = (1 << 6),
	UCH_PROP_PRINT  = (1 << 8),
	UCH_PROP_GRAPH  = (1 << 9),
	UCH_PROP_CNTRL  = (1 << 10),
	UCH_PROP_PUNCT  = (1 << 11),
	UCH_PROP_BLANK  = (1 << 12)
};

int get_prop (hawk_uci_t code)
{
	int prop = 0;

	if (iswupper(code))    prop |= UCH_PROP_UPPER;
	if (iswlower(code))    prop |= UCH_PROP_LOWER;
	if (iswalpha(code))    prop |= UCH_PROP_ALPHA;
	if (iswdigit(code))    prop |= UCH_PROP_DIGIT;
	if (iswxdigit(code))   prop |= UCH_PROP_XDIGIT;
	if (iswalnum(code))    prop |= UCH_PROP_ALNUM;
	if (iswspace(code))    prop |= UCH_PROP_SPACE;
	if (iswprint(code))    prop |= UCH_PROP_PRINT;
	if (iswgraph(code))    prop |= UCH_PROP_GRAPH;
	if (iswcntrl(code))    prop |= UCH_PROP_CNTRL;
	if (iswpunct(code))    prop |= UCH_PROP_PUNCT;
	if (iswblank(code))    prop |= UCH_PROP_BLANK;
	/*
	if (iswascii(code))    prop |= UCH_PROP_ASCII;
	if (isphonogram(code)) prop |= UCH_PROP_PHONO;
	if (isideogram(code))  prop |= UCH_PROP_IDEOG;
	if (isenglish(code))   prop |= UCH_PROP_ENGLI;
	*/

	return prop;
}

void make_prop_page (hawk_uci_t start, hawk_uci_t end)
{
	hawk_uci_t code;
	hawk_uint16_t props[UCH_PROP_PAGE_SIZE];
	prop_page_t* page;

	memset (props, 0, sizeof(props));
	for (code = start; code <= end; code++) {
		props[code - start] = get_prop(code);
	}

	for (page = prop_pages; page != NULL; page = page->next) {
		if (memcmp (props, page->props, sizeof(props)) == 0) {
			prop_maps[prop_map_count++] = page;
			return;
		}
	}

	page = (prop_page_t*)malloc (sizeof(prop_page_t));
	page->no = prop_page_count++;
	memcpy (page->props, props, sizeof(props));
	page->next = prop_pages;

	prop_pages = page;
	prop_maps[prop_map_count++] = page;
}

void emit_prop_page (prop_page_t* page)
{
	size_t i;
	int prop, need_or;

	printf ("static hawk_uint16_t uch_prop_page_%04X[%u] =\n{\n", 
		(unsigned int)page->no, (unsigned int)UCH_PROP_PAGE_SIZE);

	for (i = 0; i < UCH_PROP_PAGE_SIZE; i++) {

		need_or = 0;
		prop = page->props[i];

		if (i != 0) printf (",\n");
		printf ("\t");

		if (prop == 0) {
			printf ("0");
			continue;
		}

		if (prop & UCH_PROP_UPPER) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_UPPER");
			need_or = 1;
		}

		if (prop & UCH_PROP_LOWER) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_LOWER");
			need_or = 1;
		}

		if (prop & UCH_PROP_ALPHA) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_ALPHA");
			need_or = 1;
		}

		if (prop & UCH_PROP_DIGIT) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_DIGIT");
			need_or = 1;
		}

		if (prop & UCH_PROP_XDIGIT) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_XDIGIT");
			need_or = 1;
		}

		if (prop & UCH_PROP_ALNUM) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_ALNUM");
			need_or = 1;
		}

		if (prop & UCH_PROP_SPACE) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_SPACE");
			need_or = 1;
		}

		if (prop & UCH_PROP_PRINT) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_PRINT");
			need_or = 1;
		}

		if (prop & UCH_PROP_GRAPH) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_GRAPH");
			need_or = 1;
		}

		if (prop & UCH_PROP_CNTRL) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_CNTRL");
			need_or = 1;
		}

		if (prop & UCH_PROP_PUNCT) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_PUNCT");
			need_or = 1;
		}

		if (prop & UCH_PROP_BLANK) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_BLANK");
			need_or = 1;
		}


		/*
		if (prop & UCH_PROP_ASCII) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_ASCII");
			need_or = 1;
		}

		if (prop & UCH_PROP_IDEOG) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_IDEOG");
			need_or = 1;
		}

		if (prop & UCH_PROP_PHONO) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_PHONO");
			need_or = 1;
		}

		if (prop & UCH_PROP_ENGLI) {
			if (need_or) printf (" | ");
			printf ("HAWK_UCH_PROP_ENGLI");
			need_or = 1;
		}
		*/

	}

	printf ("\n};\n");
}

void emit_prop_map ()
{
	size_t i;

	printf ("static hawk_uint16_t* uch_prop_map[%u] =\n{\n", (unsigned int)prop_map_count);

	for (i = 0; i < prop_map_count; i++) {
		if (i != 0) printf (",\n");
		printf ("\t /* 0x%lX-0x%lX */ ", 
			(unsigned long int)(i * UCH_PROP_PAGE_SIZE),
			(unsigned long int)((i + 1) * UCH_PROP_PAGE_SIZE - 1));
		printf ("uch_prop_page_%04X", (int)prop_maps[i]->no);
	}

	printf ("\n};\n");
}

static void emit_prop_macros (void)
{
	printf ("/* generated by tools/uni-prop.c */\n\n");
	printf ("#define UCH_PROP_MAX 0x%lX\n", (unsigned long)MAX_CHAR);
	printf ("\n");
}

int main ()
{
	hawk_uci_t code;
	prop_page_t* page;
	char* locale;

	locale = setlocale (LC_ALL, "");
	if (locale == NULL ||
	    (strstr(locale, ".utf8") == NULL && strstr(locale, ".UTF8") == NULL &&
	     strstr(locale, ".utf-8") == NULL && strstr(locale, ".UTF-8") == NULL)) {
		fprintf (stderr, "error: the locale should be utf-8 compatible\n");
		return -1;
	}


	for (code = 0; code < MAX_CHAR; code += UCH_PROP_PAGE_SIZE) {
		make_prop_page (code, code + UCH_PROP_PAGE_SIZE - 1);
	}

	emit_prop_macros ();

	for (page = prop_pages; page != NULL; page = page->next) {
		emit_prop_page (page);	
		printf ("\n");
	}

	emit_prop_map ();

	return 0;
}

