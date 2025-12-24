AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

CFLAGS_COMMON =

LDFLAGS_COMMON_STATIC =-L$(libdir)
LIBADD_COMMON_STATIC =

LDFLAGS_COMMON_DYNAMIC = -L../lib -L$(libdir) -version-info 1:0:0 ## -no-undefined
LIBADD_COMMON_DYNAMIC = -lhawk

pkgmodexecdir = $(libdir)
pkgmodexec_LTLIBRARIES =

#-------------------------------------------------
# ACTUAL MODULES TO BUILD AS SHARED OBJECTS
#-------------------------------------------------
if ENABLE_MOD_FFI
if !ENABLE_MOD_FFI_STATIC
pkgmodexec_LTLIBRARIES += libhawk-ffi.la
libhawk_ffi_la_SOURCES = mod-ffi.c mod-ffi.h
libhawk_ffi_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhawk_ffi_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_ffi_la_LDFLAGS = $(LDFLAGS_COMMON_DYNAMIC)
libhawk_ffi_la_LIBADD = $(LIBADD_COMMON_DYNAMIC) $(FFI_LIBS)
endif
endif

if ENABLE_MOD_MEMC
if !ENABLE_MOD_MEMC_STATIC
pkgmodexec_LTLIBRARIES += libhawk-memc.la
libhawk_memc_la_SOURCES = mod-memc.c mod-memc.h
libhawk_memc_la_CPPFLAGS = $(CPPFLAGS_COMMON) $(MEMCACHED_CFLAGS)
libhawk_memc_la_CFLAGS = $(CFLAGS_COMMON)
libhawk_memc_la_LDFLAGS = $(LDFLAGS_COMMON_DYNAMIC) $(MEMCACHED_LDFLAGS)
libhawk_memc_la_LIBADD = $(LIBADD_COMMON_DYNAMIC) $(MEMCACHED_LIBS)
endif
endif

if ENABLE_MOD_MYSQL
if !ENABLE_MOD_MYSQL_STATIC
pkgmodexec_LTLIBRARIES += libhawk-mysql.la
libhawk_mysql_la_SOURCES = mod-mysql.c mod-mysql.h
libhawk_mysql_la_CPPFLAGS = $(CPPFLAGS_COMMON) $(MYSQL_CFLAGS)
libhawk_mysql_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_mysql_la_LDFLAGS = $(LDFLAGS_COMMON_DYNAMIC) $(MYSQL_LDFLAGS)
libhawk_mysql_la_LIBADD = $(LIBADD_COMMON_DYNAMIC) $(MYSQL_LIBS)
endif
endif

if ENABLE_MOD_SED
if !ENABLE_MOD_SED_STATIC
pkgmodexec_LTLIBRARIES += libhawk-sed.la
libhawk_sed_la_SOURCES = mod-sed.c mod-sed.h
libhawk_sed_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhawk_sed_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_sed_la_LDFLAGS = $(LDFLAGS_COMMON_DYNAMIC)
libhawk_sed_la_LIBADD = $(LIBADD_COMMON_DYNAMIC)
endif
endif

if ENABLE_MOD_SQLITE
if !ENABLE_MOD_SQLITE_STATIC
pkgmodexec_LTLIBRARIES += libhawk-sqlite.la
libhawk_sqlite_la_SOURCES = mod-sqlite.c mod-sqlite.h
libhawk_sqlite_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhawk_sqlite_la_CFLAGS = $(CFLAGS_COMMON)
libhawk_sqlite_la_LDFLAGS = $(LDFLAGS_COMMON_DYNAMIC)
libhawk_sqlite_la_LIBADD = $(LIBADD_COMMON_DYNAMIC) $(SQLITE_LIBS)
endif
endif

if ENABLE_MOD_UCI
if !ENABLE_MOD_UCI_STATIC
pkgmodexec_LTLIBRARIES += libhawk-uci.la
libhawk_uci_la_SOURCES = mod-uci.c mod-uci.h
libhawk_uci_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhawk_uci_la_CFLAGS = $(CFLAGS_COMMON) 
libhawk_uci_la_LDFLAGS = $(LDFLAGS_COMMON_DYNAMIC)
libhawk_uci_la_LIBADD = $(LIBADD_COMMON_DYNAMIC) $(UCI_LIBS)
endif
endif
