AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)
CFLAGS_COMMON = 
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir) 
LIBADD_COMMON = 
DEPENDENCIES_COMMON = 

bin_PROGRAMS = uni-case uni-prop

uni_case_SOURCES = uni-case.c
uni_case_CPPFLAGS = $(CPPFLAGS_COMMON)
uni_case_CFLAGS = $(CFLAGS_COMMON)
uni_case_LDFLAGS = $(LDFLAGS_COMMON)
uni_case_LDADD = $(LIBADD_COMMON) 
uni_case_DEPENDENCIES = $(DEPENDENCIES_COMMON)

uni_prop_SOURCES = uni-prop.c
uni_prop_CPPFLAGS = $(CPPFLAGS_COMMON)
uni_prop_CFLAGS = $(CFLAGS_COMMON)
uni_prop_LDFLAGS = $(LDFLAGS_COMMON)
uni_prop_LDADD = $(LIBADD_COMMON) 
uni_prop_DEPENDENCIES = $(DEPENDENCIES_COMMON)
