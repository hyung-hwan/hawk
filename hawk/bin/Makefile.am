AUTOMAKE_OPTIONS = nostdinc

AM_CPPFLAGS = $(PTHREAD_CFLAGS)

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(abs_builddir)/../lib -L$(libdir)

CPPFLAGS_BIN_COMMON = $(CPPFLAGS_ALL_COMMON)
LDFLAGS_BIN_COMMON = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
LIBADD_BIN_COMMON = $(LIBM) 

bin_PROGRAMS = hawk

##################################################
# hawk
##################################################
hawk_SOURCES = main.c 
hawk_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hawk_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hawk_LDADD = $(LIBADD_BIN_COMMON) $(SOCKET_LIBS) $(PTHREAD_LIBS) -lhawk
hawk_DEPENDENCIES = ../lib/libhawk.la

