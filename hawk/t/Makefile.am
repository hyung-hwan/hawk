AUTOMAKE_OPTIONS = nostdinc

AM_CFLAGS = $(PTHREAD_CFLAGS)
AM_CXXFLAGS = $(PTHREAD_CFLAGS)

AM_CPPFLAGS = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

AM_LDFLAGS = -L$(abs_builddir)/../lib  -L$(libdir)
LDADD = $(LIBM)

check_SCRIPTS = h-001.hawk h-002.hawk
##noinst_SCRIPTS = $(check_SCRIPTS)
EXTRA_DIST = $(check_SCRIPTS)

check_PROGRAMS = t-001 t-002 t-003 t-004 t-005 t-006

t_001_SOURCES = t-001.c t.h

t_002_SOURCES = t-002.c t.h
t_002_LDADD = -lhawk $(LDADD)
t_002_DEPENDENCIES = ../lib/libhawk.la

t_003_SOURCES = t-003.c t.h
t_003_LDADD = -lhawk $(LDADD)
t_003_DEPENDENCIES = ../lib/libhawk.la

t_004_SOURCES = t-004.c t.h
t_004_LDADD = -lhawk $(LDADD)
t_004_DEPENDENCIES = ../lib/libhawk.la

t_005_SOURCES = t-005.c t.h
t_005_LDADD = -lhawk $(LDADD)
t_005_DEPENDENCIES = ../lib/libhawk.la

t_006_SOURCES = t-006.c t.h
t_006_LDADD = -lhawk $(LDADD)
t_006_DEPENDENCIES = ../lib/libhawk.la


TESTS = $(check_PROGRAMS) $(check_SCRIPTS)

TEST_EXTENSIONS = .hawk
HAWK_LOG_COMPILER = ../bin/hawk
AM_HAWK_LOG_FLAGS = --modlibdirs=../lib/.libs:../mod/.libs -f
