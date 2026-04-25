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
check_SCRIPTS += h-003.hawk h-004.hawk h-009.hawk h-010.hawk \
	h-011.hawk h-012.hawk h-013.hawk h-014.hawk h-015.hawk \
	h-016.hawk h-017.hawk h-018.hawk h-019.hawk h-020.hawk \
	h-021.hawk h-022.hawk h-023.hawk h-024.hawk

check_SCRIPTS += regress-filename.sh regress-extra-info.sh

check_ERRORS = e-001.err

##noinst_SCRIPTS = $(check_SCRIPTS)
EXTRA_DIST = $(check_SCRIPTS) $(check_ERRORS) tap.inc err.sh \
	journal-toc.hawk journal-toc.in journal-toc.out journal-toc-html.out \
	bibtex-to-html.hawk bibtex-to-html.out \
	fs-test.hawk fs-test.in fs-test.out \
	JSON.awk.in JSON.awk.out \
	h-024-child.hawk h-024.in \
	run-hawk-test.sh regress-extra-info.sh \
	two-way-pipe.hawk two-way-pipe.out

check_PROGRAMS = t-001 t-002 t-003 t-004 t-005 t-006 t-007 t-008 t-009 t-010

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

t_010_SOURCES = t-010.c tap.h
t_010_CPPFLAGS = $(CPPFLAGS_COMMON)
t_010_CFLAGS = $(CFLAGS_COMMON)
t_010_LDFLAGS = $(LDFLAGS_COMMON)
t_010_LDADD = $(LIBADD_COMMON)

if ENABLE_CXX
t_101_SOURCES = t-101.cpp tap.h
t_101_CPPFLAGS = $(CPPFLAGS_COMMON)
t_101_CFLAGS = $(CFLAGS_COMMON)
t_101_LDFLAGS = $(LDFLAGS_COMMON)
t_101_LDADD = $(LIBADD_COMMON)
endif

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/ac/tap-driver.sh
TESTS = $(check_PROGRAMS) $(check_SCRIPTS) $(check_ERRORS)

TEST_EXTENSIONS = .hawk .err .sh

# override these from environment when needed.
# e.g.
#   make check HAWK_TEST_COMPILER=../samples/hawk51 HAWK_CHECK_OPTS=
#   make check VALGRIND=1
HAWK_TEST_COMPILER ?= ../bin/hawk
HAWK_TEST_MODLIBDIRS_OPT ?= --modlibdirs=../lib/.libs:../mod/.libs
HAWK_CHECK_OPTS ?=
VALGRIND ?= 0
VALGRIND_CMD ?= valgrind
VALGRIND_ERROR_EXITCODE ?= 99
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 --num-callers=20

HAWK_LOG_DRIVER = $(LOG_DRIVER)
HAWK_LOG_COMPILER = env \
	VALGRIND='$(VALGRIND)' \
	VALGRIND_CMD='$(VALGRIND_CMD)' \
	VALGRIND_ERROR_EXITCODE='$(VALGRIND_ERROR_EXITCODE)' \
	VALGRIND_FLAGS='$(VALGRIND_FLAGS)' \
	HAWK_TEST_COMPILER='$(HAWK_TEST_COMPILER)' \
	$(SHELL) $(abs_srcdir)/run-hawk-test.sh
AM_HAWK_LOG_FLAGS = -vTDIR=${abs_srcdir} $(HAWK_TEST_MODLIBDIRS_OPT) $(HAWK_CHECK_OPTS) -f

ERR_LOG_DRIVER = $(SHELL) $(top_srcdir)/ac/test-driver
ERR_LOG_COMPILER = $(SHELL) $(abs_srcdir)/err.sh $(HAWK_TEST_COMPILER) -vTDIR=${abs_srcdir} $(HAWK_TEST_MODLIBDIRS_OPT) -f
AM_ERR_LOG_FLAGS =

SH_LOG_DRIVER = $(LOG_DRIVER)
SH_LOG_COMPILER = env HAWK_BIN='$(HAWK_TEST_COMPILER)' $(SHELL)
AM_SH_LOG_FLAGS =
