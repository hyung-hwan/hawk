AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

CFLAGS_COMMON =

##################################################
if ENABLE_STATIC_MODULE
##################################################

#-------------------------------------------------
# STATIC MODULES BUILT INTO MAIN LIBRARY
#-------------------------------------------------
LDFLAGS_COMMON =-L$(libdir) 
LIBADD_COMMON =

noinst_LTLIBRARIES =

if ENABLE_MOD_FFI
noinst_LTLIBRARIES += libhawk-ffi.la
endif

if ENABLE_MOD_MYSQL
noinst_LTLIBRARIES += libhawk-mysql.la
endif

if ENABLE_MOD_UCI
noinst_LTLIBRARIES += libhawk-uci.la
endif

##################################################
else
##################################################

#-------------------------------------------------
# DYNAMIC MODULES
#-------------------------------------------------
LDFLAGS_COMMON = -L../lib -L$(libdir) -version-info 1:0:0 ## -no-undefined
LIBADD_COMMON = -lhawk

pkgmodexecdir = $(libdir)
pkgmodexec_LTLIBRARIES =

if ENABLE_MOD_FFI
pkgmodexec_LTLIBRARIES += libhawk-ffi.la
endif

if ENABLE_MOD_MYSQL
pkgmodexec_LTLIBRARIES += libhawk-mysql.la
endif

if ENABLE_MOD_UCI
pkgmodexec_LTLIBRARIES += libhawk-uci.la
endif

##################################################
endif
##################################################


#-------------------------------------------------
# ACTUAL MODULES
#-------------------------------------------------
if ENABLE_MOD_FFI
libhawk_ffi_la_SOURCES = mod-ffi.c mod-ffi.h
libhawk_ffi_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhawk_ffi_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_ffi_la_LDFLAGS = $(LDFLAGS_COMMON)
libhawk_ffi_la_LIBADD = $(LIBADD_COMMON) $(FFI_LIBS)
endif

if ENABLE_MOD_MYSQL
libhawk_mysql_la_SOURCES = mod-mysql.c mod-mysql.h
libhawk_mysql_la_CPPFLAGS = $(CPPFLAGS_COMMON) $(MYSQL_CFLAGS)
libhawk_mysql_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_mysql_la_LDFLAGS = $(LDFLAGS_COMMON) $(MYSQL_LDFLAGS)
libhawk_mysql_la_LIBADD = $(LIBADD_COMMON) $(MYSQL_LIBS)
endif

if ENABLE_MOD_UCI
libhawk_uci_la_SOURCES = mod-uci.c mod-uci.h
libhawk_uci_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhawk_uci_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_uci_la_LDFLAGS = $(LDFLAGS_COMMON)
libhawk_uci_la_LIBADD = $(LIBADD_COMMON) $(UCI_LIBS)
endif
