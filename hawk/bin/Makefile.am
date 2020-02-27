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

bin_PROGRAMS = hawk

##################################################
# hawk
##################################################
hawk_SOURCES = main.c 
hawk_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hawk_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hawk_LDADD = -lhawk $(LIBADD_BIN_COMMON) $(SOCKET_LIBS) $(PTHREAD_LIBS)
hawk_DEPENDENCIES = ../lib/libhawk.la

