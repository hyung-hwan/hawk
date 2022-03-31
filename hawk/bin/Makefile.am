AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)
CFLAGS_COMMON =
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir)

## specify the libhawk.la file instead of -lhawk. 
##Use -l for third party libraries for automatic inclusion to DEPENDENCIES .
LIBADD_COMMON = ../lib/libhawk.la

bin_PROGRAMS = hawk #hsed

##################################################
# hawk
##################################################
hawk_SOURCES = main-hawk.c 
hawk_CPPFLAGS = $(CPPFLAGS_COMMON)
hawk_CFLAGS = $(CFLAGS_COMMON)
hawk_LDFLAGS = $(LDFLAGS_COMMON)
##
## -lm is omitted from dependency_libs in libhawk.la because
## libhawk contains some C++ code and postdeps_CXX contains -lm.
##
## Under the default libtool configuration, postdeps is empty.
## Under the C++ tagged libtool configuration, postdeps is set to 
## the value of postdeps_CXX.
##
## libtool doesn't include -lm in dependency_libs in libhawk.la 
## because libtool --tag=CXX is used for mixed C++ code.
##
## specify -lm to a C only target to satisify dependency regardless
## of different behavior depending on the libtool tag.
##
hawk_LDADD = $(LIBADD_COMMON) $(LIBM)

#hsed_SOURCES = main-hsed.c 
#hsed_CPPFLAGS = $(CPPFLAGS_COMMON)
#hsed_CFLAGS = $(CFLAGS_COMMON)
#hsed_LDFLAGS = $(LDFLAGS_COMMON)
#hsed_LDADD = $(LIBADD_COMMON) $(LIBM)
