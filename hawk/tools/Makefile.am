AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(abs_builddir)/../lib -L$(libdir)

CPPFLAGS_BIN_COMMON = $(CPPFLAGS_ALL_COMMON)
LDFLAGS_BIN_COMMON = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
LIBADD_BIN_COMMON =

bin_PROGRAMS = uni-case uni-prop

uni_case_SOURCES = uni-case.c
uni_case_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
uni_case_LDFLAGS = $(LDFLAGS_ALL_COMMON)
uni_case_LDADD = $(LIBADD_BIN_COMMON) 
uni_case_DEPENDENCIES = 

uni_prop_SOURCES = uni-prop.c
uni_prop_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
uni_prop_LDFLAGS = $(LDFLAGS_ALL_COMMON)
uni_prop_LDADD = $(LIBADD_BIN_COMMON) 
uni_prop_DEPENDENCIES = 
