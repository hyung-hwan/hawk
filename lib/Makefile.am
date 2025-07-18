AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_srcdir) \
	-I$(includedir)

CFLAGS_ALL_COMMON =
CXXFLAGS_ALL_COMMON =

if ENABLE_STATIC_MODULE
LDFLAGS_ALL_COMMON = -L$(abs_builddir)/../mod -L$(abs_builddir) -L$(libdir)
else
LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(libdir)
endif

##################################################
# MAIN LIBRARY 
##################################################

if WIN32
# you must adjust the value of DEFAULT_MODPOSTFIX according
# to the first number in -version-info above
CPPFLAGS_PFMOD = -DHAWK_DEFAULT_MODLIBDIRS=\"\" -DHAWK_DEFAULT_MODPREFIX=\"libhawk-\" -DHAWK_DEFAULT_MODPOSTFIX=\"-1.dll\"
else
CPPFLAGS_PFMOD = -DHAWK_DEFAULT_MODLIBDIRS=\"$(libdir)\" -DHAWK_DEFAULT_MODPREFIX=\"libhawk-\"
if MACOSX
CPPFLAGS_PFMOD += -DHAWK_DEFAULT_MODPOSTFIX=\".dylib\"
else
CPPFLAGS_PFMOD += -DHAWK_DEFAULT_MODPOSTFIX=\".so\"
endif
endif

EXTRA_DIST = \
	gem-glob.c.m4 \
	gem-glob.m4 \
	hawk-str.h.m4 \
	utl-str.c.m4 \
	utl-str.m4

BUILT_SOURCES = \
	gem-glob.c \
	hawk-str.h \
	utl-str.c

##################################################
# include files to deploy
##################################################

pkgincludedir = $(includedir)

pkginclude_HEADERS = \
	hawk.h \
	hawk-arr.h \
	hawk-chr.h \
	hawk-cli.h \
	hawk-cmn.h \
	hawk-cut.h \
	hawk-dir.h \
	hawk-ecs.h \
	hawk-fio.h \
	hawk-fmt.h \
	hawk-gem.h \
	hawk-glob.h \
	hawk-htb.h \
	hawk-json.h \
	hawk-map.h \
	hawk-mtx.h \
	hawk-rbt.h \
	hawk-pac1.h \
	hawk-pio.h \
	hawk-skad.h \
	hawk-utl.h \
	hawk-sed.h \
	hawk-sio.h \
	hawk-std.h \
	hawk-str.h \
	hawk-tio.h \
	hawk-tre.h \
	hawk-upac.h \
	hawk-xma.h


##################################################
# libraries to deploy
##################################################
pkglibdir = $(libdir)
pkglib_LTLIBRARIES = libhawk.la

libhawk_la_SOURCES = \
	$(pkginclude_HEADERS) \
	arr.c \
	chr.c \
	cut-prv.h \
	cut.c \
	dir.c \
	ecs-imp.h \
	ecs.c \
	err-prv.h \
	err.c \
	err-sys.c \
	fmt-imp.h \
	fmt.c \
	fnc-prv.h \
	fnc.c \
	gem.c \
	gem-glob.c \
	gem-nwif.c \
	gem-nwif2.c \
	hawk-prv.h \
	hawk.c \
	htb.c \
	idmap-imp.h \
	json.c \
	json-prv.h \
	mb8.c \
	misc-imp.h \
	misc-prv.h \
	misc.c \
	parse-prv.h \
	parse.c \
	rbt.c \
	rec.c \
	rio-prv.h \
	rio.c \
	run-prv.h \
	run.c \
	sed-prv.h \
	sed.c \
	skad-prv.h \
	skad.c \
	tre-prv.h \
	tre-ast.c \
	tre-ast.h \
	tre-compile.c \
	tre-compile.h \
	tre-match-bt.c \
	tre-match-pa.c \
	tre-match-ut.h \
	tre-mem.c \
	tre-mem.h \
	tre-parse.c \
	tre-parse.h \
	tre-stack.h \
	tre-stack.c \
	tre.c \
	tree-prv.h \
	tree.c \
	uch-prop.h \
	uch-case.h \
	utf16.c \
	utf8.c \
	utl-ass.c \
	utl-cmgr.c \
	utl-rnd.c\
	utl-sort.c \
	utl-str.c \
	utl-sys.c \
	utl-xstr.c \
	utl.c \
	val-prv.h \
	val.c \
	xma.c

libhawk_la_SOURCES += \
	cli-imp.h \
	cli.c \
	fio.c \
	mtx.c \
	pio.c \
	sio.c \
	syscall.h \
	tio.c \
	std.c  \
	std-cut.c \
	std-json.c \
	std-sed.c

libhawk_la_CPPFLAGS = $(CPPFLAGS_ALL_COMMON) $(CPPFLAGS_PFMOD)
libhawk_la_CFLAGS = $(CFLAGS_ALL_COMMON)
libhawk_la_CXXFLAGS = $(CXXFLAGS_ALL_COMMON)
libhawk_la_LDFLAGS = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 ## -no-undefined
## socket() is used in gem-nwif.c and gem-nwif2.c
libhawk_la_LIBADD = $(LIBM) $(SOCKET_LIBS)

if ENABLE_LIBLTDL
libhawk_la_CPPFLAGS += $(LTDLINCL)
libhawk_la_LIBADD += $(LTDL_LIBS)
else
libhawk_la_LIBADD += $(DL_LIBS)
endif
if ENABLE_LIBUNWIND
libhawk_la_LIBADD += $(UNWIND_LIBS)
endif

if ENABLE_CXX
pkginclude_HEADERS += Hawk.hpp Hawk-Sed.hpp
libhawk_la_SOURCES += Hawk.cpp Std.cpp Sed.cpp Std-Sed.cpp

###libhawk_la_LINK = $(CXXLINK)
libhawk_la_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=link $(CXXLD) $(AM_CXXFLAGS) \
	$(CXXFLAGS) $(libhawk_la_LDFLAGS) $(LDFLAGS) -o $@
else

## as long as a c++ file is listed in the SOURCES list, automake chooses 
## --tag=CXX and $(CXXLD) for the linker regardless of ENABLE_CXX.
## i want $(CCLD) to be used when CXX is disabled because a fatal problem
## arises when CXXLD is chosen but the system doesn't have a c++ compiler.
##
##libhawk_la_LINK = $(LINK)
## $(LINK) includes $(AM_LDFLAGS) instead of $(libhawk_la_LDFLAGS) 
## so i compose the entire libhawk specific LINK line below.
libhawk_la_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) \
	$(CFLAGS) $(libhawk_la_LDFLAGS) $(LDFLAGS) -o $@
endif

if ENABLE_STATIC_MODULE

#######################################################
# BASIC MODULES STATICALLY BUILT INTO MAIN LIBRARY
#######################################################
libhawk_la_SOURCES += \
	mod-hawk.c mod-hawk.h \
	mod-math.c mod-math.h \
	mod-str.c mod-str.h \
	mod-sys.c mod-sys.h
libhawk_la_LIBADD += $(SOCKET_LIBS)

else
#######################################################
# BUILD BASIC MODULES AS SHARED OBJECTS
#######################################################

CPPFLAGS_MOD_COMMON = $(CPPFLAGS_ALL_COMMON)
CFLAGS_MOD_COMMON = $(CFLAGS_ALL_COMMON)
CXXFLAGS_MOD_COMMON = $(CXXFLAGS_ALL_COMMON)
LDFLAGS_MOD_COMMON = $(LDFLAGS_ALL_COMMON)
LIBADD_MOD_COMMON = libhawk.la

pkglib_LTLIBRARIES += libhawk-hawk.la
libhawk_hawk_la_SOURCES = mod-hawk.c mod-hawk.h
libhawk_hawk_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_hawk_la_CFLAGS = $(CFLAGS_MOD_COMMON)
libhawk_hawk_la_CXXFLAGS = $(CXXFLAGS_MOD_COMMON)
libhawk_hawk_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_hawk_la_LIBADD = $(LIBADD_MOD_COMMON)

pkglib_LTLIBRARIES += libhawk-math.la
libhawk_math_la_SOURCES = mod-math.c mod-math.h
libhawk_math_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_math_la_CFLAGS = $(CFLAGS_MOD_COMMON)
libhawk_math_la_CXXFLAGS = $(CXXFLAGS_MOD_COMMON)
libhawk_math_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_math_la_LIBADD = $(LIBADD_MOD_COMMON) $(LIBM)

pkglib_LTLIBRARIES += libhawk-str.la
libhawk_str_la_SOURCES = mod-str.c mod-str.h
libhawk_str_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_str_la_CFLAGS = $(CFLAGS_MOD_COMMON)
libhawk_str_la_CXXFLAGS = $(CXXFLAGS_MOD_COMMON)
libhawk_str_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_str_la_LIBADD = $(LIBADD_MOD_COMMON)

pkglib_LTLIBRARIES += libhawk-sys.la
libhawk_sys_la_SOURCES = mod-sys.c mod-sys.h
libhawk_sys_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_sys_la_CFLAGS = $(CFLAGS_MOD_COMMON)
libhawk_sys_la_CXXFLAGS = $(CXXFLAGS_MOD_COMMON)
libhawk_sys_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_sys_la_LIBADD = $(LIBADD_MOD_COMMON) $(SOCKET_LIBS)

endif

#######################################################
# ADDITIONAL MODULES BUILT STATICALLY INTO MAIN LIBRARY
#######################################################

if ENABLE_MOD_FFI_STATIC
libhawk_la_SOURCES += ../mod/mod-ffi.c ../mod/mod-ffi.h
libhawk_la_LIBADD += $(FFI_LIBS)
endif

if ENABLE_MOD_MEMC_STATIC
libhawk_la_SOURCES += ../mod/mod-memc.c ../mod/mod-memc.h
libhawk_la_LIBADD += $(MEMCACHED_LIBS)
endif

if ENABLE_MOD_MYSQL_STATIC
libhawk_la_SOURCES += ../mod/mod-mysql.c ../mod/mod-mysql.h
libhawk_la_CPPFLAGS += $(MYSQL_CFLAGS) ## put CFLAGS in CPPFLAGS as MYSQL_CFLAGS contains -I options only
## $(MYSQL_LDFLAGS) has -L options. If this is not listed in LIBADD, it doesn't enter the result .la file
## it caused a link failure that is caused by the library not found for this missing path.
## Let's placs it under LIBADD. The same part in ../mod/Makefile.am still places it under LDFLAGS.
##libhawk_la_LDFLAGS += $(MYSQL_LDFLAGS)
libhawk_la_LIBADD += $(MYSQL_LDFLAGS) $(MYSQL_LIBS)
endif

if ENABLE_MOD_SED_STATIC
libhawk_la_SOURCES += ../mod/mod-sed.c ../mod/mod-sed.h
endif

if ENABLE_MOD_UCI_STATIC
libhawk_la_SOURCES += ../mod/mod-uci.c ../mod/mod-uci.h
libhawk_la_LIBADD += $(UCI_LIBS)
endif

##############################################################
# installation
##############################################################
install-data-hook:
	@echo "#ifndef _HAWK_CFG_H_" > "$(DESTDIR)$(pkgincludedir)/hawk-cfg.h"
	@echo "#define _HAWK_CFG_H_" >> "$(DESTDIR)$(pkgincludedir)/hawk-cfg.h"
	@$(EGREP) "#define[ ]+HAWK_" "$(abs_builddir)/hawk-cfg.h" >> "$(DESTDIR)$(pkgincludedir)/hawk-cfg.h" 
	@echo "#endif" >> "$(DESTDIR)$(pkgincludedir)/hawk-cfg.h"
	@rm -f "$(DESTDIR)$(pkgincludedir)/hawk-cfg.h.in"
	@$(SED) 's|/\*#define HAWK_HAVE_CFG_H\*/|#define HAWK_HAVE_CFG_H|' "$(srcdir)/hawk-cmn.h" > "$(DESTDIR)$(pkgincludedir)/hawk-cmn.h"

uninstall-hook:
	@rm -f "$(DESTDIR)$(pkgincludedir)/hawk-cfg.h"

gem-glob.c: gem-glob.m4 gem-glob.c.m4
	m4 -I$(srcdir) $(srcdir)/gem-glob.c.m4 > $(srcdir)/gem-glob.c

hawk-str.h: utl-str.m4 hawk-str.h.m4
	m4 -I$(srcdir) $(srcdir)/hawk-str.h.m4 > $(srcdir)/hawk-str.h

utl-str.c: utl-str.m4 utl-str.c.m4
	m4 -I$(srcdir) $(srcdir)/utl-str.c.m4 > $(srcdir)/utl-str.c

