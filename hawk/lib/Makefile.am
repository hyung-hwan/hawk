AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_srcdir) \
	-I$(includedir) $(PTHREAD_CFLAGS)

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
CPPFLAGS_PFMOD = -DHAWK_DEFAULT_MODPREFIX=\"libhawk-\" -DHAWK_DEFAULT_MODPOSTFIX=\"-1.dll\"
else
CPPFLAGS_PFMOD = -DHAWK_DEFAULT_MODPREFIX=\"$(libdir)/libhawk-\"
if MACOSX
CPPFLAGS_PFMOD += -DHAWK_DEFAULT_MODPOSTFIX=\".dylib\"
else
CPPFLAGS_PFMOD += -DHAWK_DEFAULT_MODPOSTFIX=\".so\"
endif
endif

##################################################
# include files to deploy
##################################################

pkgincludedir = $(includedir)

pkginclude_HEADERS = \
	hawk.h \
	hawk-arr.h \
	hawk-chr.h \
	hawk-cmn.h \
	hawk-dir.h \
	hawk-ecs.h \
	hawk-fmt.h \
	hawk-gem.h \
	hawk-htb.h \
	hawk-map.h \
	hawk-rbt.h \
	hawk-pack1.h \
	hawk-utl.h \
	hawk-std.h \
	hawk-tre.h \
	hawk-unpack.h


##################################################
# libraries to deploy
##################################################
pkglibdir = $(libdir)
pkglib_LTLIBRARIES = libhawk.la

libhawk_la_SOURCES = \
	$(pkginclude_HEADERS) \
	arr.c \
	chr.c \
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
	htb.c \
	gem.c \
	gem-skad.c \
	gem-nwif.c \
	gem-nwif2.c \
	hawk-prv.h \
	hawk.c \
	mb8.c \
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
	utl-skad.c \
	utl-skad.h \
	utl-sort.c \
	utl-str.c \
	utl-sys.c \
	utl.c \
	val-prv.h \
	val.c

libhawk_la_SOURCES += \
	hawk-cli.h \
	hawk-fio.h \
	hawk-mtx.h \
	hawk-pio.h \
	hawk-sio.h \
	hawk-tio.h \
	cli-imp.h \
	cli.c \
	fio.c \
	mtx.c \
	pio.c \
	sio.c \
	syscall.h \
	tio.c \
	std-prv.h \
	std.c 

libhawk_la_CPPFLAGS = $(CPPFLAGS_ALL_COMMON) $(CPPFLAGS_PFMOD)
libhawk_la_LDFLAGS = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
libhawk_la_LIBADD = $(LIBM) $(PTHREAD_LIBS)
libhawk_la_DEPENDENCIES = 

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
pkginclude_HEADERS += Hawk.hpp HawkStd.hpp
libhawk_la_SOURCES += Hawk.cpp HawkStd.cpp

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

##################################################
# STATIC MODULES BUILT INTO MAIN LIBRARY
##################################################
libhawk_la_SOURCES += \
	imap-imp.h \
	mod-math.c mod-math.h \
	mod-str.c mod-str.h \
	mod-sys.c mod-sys.h
libhawk_la_LIBADD +=

if ENABLE_MOD_MYSQL
libhawk_la_LIBADD += -lhawk-mysql
libhawk_la_DEPENDENCIES += ../mod/libhawk-mysql.la
endif

if ENABLE_MOD_UCI
libhawk_la_LIBADD += -lhawk-uci
libhawk_la_DEPENDENCIES += ../mod/libhawk-uci.la
endif

else
##################################################
# DYNAMIC MODULES 
##################################################

CPPFLAGS_MOD_COMMON = $(CPPFLAGS_ALL_COMMON)
LDFLAGS_MOD_COMMON = $(LDFLAGS_ALL_COMMON)
LIBADD_MOD_COMMON = -lhawk
DEPENDENCIES_MOD_COMMON = libhawk.la

#pkglibdir = $(libdir)
#pkglib_LTLIBRARIES = 

pkglib_LTLIBRARIES += libhawk-math.la
libhawk_math_la_SOURCES = mod-math.c mod-math.h
libhawk_math_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_math_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_math_la_LIBADD = $(LIBADD_MOD_COMMON)
libhawk_math_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON)

pkglib_LTLIBRARIES += libhawk-str.la
libhawk_str_la_SOURCES = mod-str.c mod-str.h
libhawk_str_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_str_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_str_la_LIBADD = $(LIBADD_MOD_COMMON)
libhawk_str_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON)

pkglib_LTLIBRARIES += libhawk-sys.la
libhawk_sys_la_SOURCES = mod-sys.c mod-sys.h
libhawk_sys_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
libhawk_sys_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
libhawk_sys_la_LIBADD = $(LIBADD_MOD_COMMON)
libhawk_sys_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON)

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
