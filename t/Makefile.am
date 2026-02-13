AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)
CFLAGS_COMMON =
LDFLAGS_COMMON=-L$(abs_builddir)/../lib  -L$(libdir)
## place $(LIBM)  here as all programs below are C only programs linked
## against the C/C++ hybrid library. Read comments in ../bin/Makefile.am
## for more information.
LIBADD_COMMON = ../lib/libhawk.la $(LIBM)

check_SCRIPTS =
if ENABLE_WIDE_CHAR
check_SCRIPTS += h-001.hawk h-002.hawk
endif
check_SCRIPTS += h-003.hawk h-004.hawk h-009.hawk h-010.hawk h-011.hawk h-012.hawk h-013.hawk h-014.hawk h-015.hawk h-016.hawk

check_ERRORS = e-001.err

##noinst_SCRIPTS = $(check_SCRIPTS)
EXTRA_DIST = $(check_SCRIPTS) $(check_ERRORS) tap.inc err.sh \
	journal-toc.hawk journal-toc.in journal-toc.out journal-toc-html.out \
	bibtex-to-html.hawk bibtex-to-html.out \
	fs-test.hawk fs-test.in fs-test.out \
	JSON.awk.in JSON.awk.out \
	two-way-pipe.hawk two-way-pipe.out

check_PROGRAMS = t-001 t-002 t-003 t-004 t-005 t-006 t-007 t-008 t-009

if ENABLE_CXX
check_PROGRAMS += t-101
endif

t_001_SOURCES = t-001.c tap.h
t_001_CPPFLAGS = $(CPPFLAGS_COMMON)

t_002_SOURCES = t-002.c tap.h
t_002_CPPFLAGS = $(CPPFLAGS_COMMON)
t_002_CFLAGS = $(CFLAGS_COMMON)
t_002_LDFLAGS = $(LDFLAGS_COMMON)
t_002_LDADD = $(LIBADD_COMMON)

t_003_SOURCES = t-003.c tap.h
t_003_CPPFLAGS = $(CPPFLAGS_COMMON)
t_003_CFLAGS = $(CFLAGS_COMMON)
t_003_LDFLAGS = $(LDFLAGS_COMMON)
t_003_LDADD = $(LIBADD_COMMON)

t_004_SOURCES = t-004.c tap.h
t_004_CPPFLAGS = $(CPPFLAGS_COMMON)
t_004_CFLAGS = $(CFLAGS_COMMON)
t_004_LDFLAGS = $(LDFLAGS_COMMON)
t_004_LDADD = $(LIBADD_COMMON)

t_005_SOURCES = t-005.c tap.h
t_005_CPPFLAGS = $(CPPFLAGS_COMMON)
t_005_CFLAGS = $(CFLAGS_COMMON)
t_005_LDFLAGS = $(LDFLAGS_COMMON)
t_005_LDADD = $(LIBADD_COMMON)

t_006_SOURCES = t-006.c tap.h
t_006_CPPFLAGS = $(CPPFLAGS_COMMON)
t_006_CFLAGS = $(CFLAGS_COMMON)
t_006_LDFLAGS = $(LDFLAGS_COMMON)
t_006_LDADD = $(LIBADD_COMMON)

t_007_SOURCES = t-007.c tap.h
t_007_CPPFLAGS = $(CPPFLAGS_COMMON)
t_007_CFLAGS = $(CFLAGS_COMMON)
t_007_LDFLAGS = $(LDFLAGS_COMMON)
t_007_LDADD = $(LIBADD_COMMON)

t_008_SOURCES = t-008.c tap.h
t_008_CPPFLAGS = $(CPPFLAGS_COMMON)
t_008_CFLAGS = $(CFLAGS_COMMON)
t_008_LDFLAGS = $(LDFLAGS_COMMON)
t_008_LDADD = $(LIBADD_COMMON)

t_009_SOURCES = t-009.c tap.h
t_009_CPPFLAGS = $(CPPFLAGS_COMMON)
t_009_CFLAGS = $(CFLAGS_COMMON)
t_009_LDFLAGS = $(LDFLAGS_COMMON)
t_009_LDADD = $(LIBADD_COMMON)

if ENABLE_CXX
t_101_SOURCES = t-101.cpp tap.h
t_101_CPPFLAGS = $(CPPFLAGS_COMMON)
t_101_CFLAGS = $(CFLAGS_COMMON)
t_101_LDFLAGS = $(LDFLAGS_COMMON)
t_101_LDADD = $(LIBADD_COMMON)
endif


LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/ac/tap-driver.sh
TESTS = $(check_PROGRAMS) $(check_SCRIPTS) $(check_ERRORS)

TEST_EXTENSIONS = .hawk .err

HAWK_LOG_DRIVER = $(LOG_DRIVER)
HAWK_LOG_COMPILER = ../bin/hawk
AM_HAWK_LOG_FLAGS = -vTDIR=${abs_srcdir} --modlibdirs=../lib/.libs:../mod/.libs -f

ERR_LOG_DRIVER = $(SHELL) $(top_srcdir)/ac/test-driver
ERR_LOG_COMPILER = $(SHELL) $(abs_srcdir)/err.sh ../bin/hawk -vTDIR=${abs_srcdir} --modlibdirs=../lib/.libs:../mod/.libs -f
AM_ERR_LOG_FLAGS =
