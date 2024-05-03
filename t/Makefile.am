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

check_SCRIPTS = h-001.hawk h-002.hawk h-003.hawk h-004.hawk h-009.hawk

check_ERRORS = e-001.err

##noinst_SCRIPTS = $(check_SCRIPTS)
EXTRA_DIST = $(check_SCRIPTS) $(check_ERRORS) tap.inc err.sh \
	journal-toc.hawk journal-toc.in journal-toc.out journal-toc-html.out \
	bibtex-to-html.hawk bibtex-to-html.out

check_PROGRAMS = t-001 t-002 t-003 t-004 t-005 t-006

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

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/ac/tap-driver.sh
TESTS = $(check_PROGRAMS) $(check_SCRIPTS) $(check_ERRORS)

TEST_EXTENSIONS = .hawk .err

HAWK_LOG_DRIVER = $(LOG_DRIVER)
HAWK_LOG_COMPILER = ../bin/hawk
AM_HAWK_LOG_FLAGS = -vTDIR=${abs_srcdir} --modlibdirs=../lib/.libs:../mod/.libs -f

ERR_LOG_DRIVER = $(SHELL) $(top_srcdir)/ac/test-driver
ERR_LOG_COMPILER = $(SHELL) $(abs_srcdir)/err.sh ../bin/hawk -vTDIR=${abs_srcdir} --modlibdirs=../lib/.libs:../mod/.libs -f
AM_ERR_LOG_FLAGS =
