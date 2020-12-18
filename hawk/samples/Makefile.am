AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

CFLAGS_COMMON =
CXXFLAGS_COMMON =
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir)
LIBADD_COMMON = ../lib/libhawk.la

noinst_PROGRAMS = 

if ENABLE_CXX

noinst_PROGRAMS += hawk02 hawk51

hawk02_SOURCES = hawk02.c
hawk02_CPPFLAGS = $(CPPFLAGS_COMMON)
hawk02_CFLAGS = $(CFLAGS_COMMON)
hawk02_LDFLAGS = $(LDFLAGS_COMMON)
hawk02_LDADD = $(LIBADD_COMMON) $(LIBM)

hawk51_SOURCES = hawk51.cpp
hawk51_CPPFLAGS = $(CPPFLAGS_COMMON)
hawk51_CFLAGS = $(CFLAGS_COMMON)
hawk51_CXXFLAGS = $(CXXFLAGS_COMMON)
hawk51_LDFLAGS = $(LDFLAGS_COMMON)
hawk51_LDADD = $(LIBADD_COMMON)

endif
