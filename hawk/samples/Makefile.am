AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir) \
	$(PTHREAD_CFLAGS)

LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(abs_builddir)/../lib -L$(libdir)

CPPFLAGS_BIN_COMMON = $(CPPFLAGS_ALL_COMMON)
LDFLAGS_BIN_COMMON = $(LDFLAGS_ALL_COMMON) -no-undefined
LIBADD_BIN_COMMON = $(LIBM) 

bin_PROGRAMS = 


if ENABLE_CXX

bin_PROGRAMS += hawk02 hawk51

hawk02_SOURCES = hawk02.c
hawk02_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hawk02_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hawk02_LDADD = -lhawk $(LIBADD_BIN_COMMON) $(SOCKET_LIBS)
hawk02_DEPENDENCIES = ../lib/libhawk.la

hawk51_SOURCES = hawk51.cpp
hawk51_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hawk51_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hawk51_LDADD = -lhawk $(LIBADD_BIN_COMMON) $(SOCKET_LIBS)
hawk51_DEPENDENCIES = ../lib/libhawk.la

endif
