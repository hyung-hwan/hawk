AUTOMAKE_OPTIONS = nostdinc


CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)
CFLAGS_COMMON =
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir)
##
## -lm may get omitted from dependency_libs in libhawk.la.
## the generated libtool in the build directory may include -lm in postdeps
## dependency_libs excludes items in postdeps.
## specify -lm explicitly to satisfy dependency regardless of libtool behavior.
##
LIBADD_COMMON = -lhawk $(LIBM)
DEPENDENCIES_COMMON = ../lib/libhawk.la

bin_PROGRAMS = hawk

##################################################
# hawk
##################################################
hawk_SOURCES = main.c 
hawk_CPPFLAGS = $(CPPFLAGS_COMMON)
hawk_CFLAGS = $(CFLAGS_COMMON)
hawk_LDFLAGS = $(LDFLAGS_COMMON)
hawk_LDADD = $(LIBADD_COMMON)
hawk_DEPENDENCIES = $(DEPENDENCIES_COMMON)

