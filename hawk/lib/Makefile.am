AUTOMAKE_OPTIONS = nostdinc

##AM_CFLAGS = $(PTHREAD_CFLAGS)

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_srcdir) \
	-I$(includedir)

if ENABLE_STATIC_MODULE
LDFLAGS_ALL_COMMON = -L../mod 
else
LDFLAGS_ALL_COMMON = -L. 
endif

##################################################
# MAIN LIBRARY 
##################################################

CPPFLAGS_LIB_COMMON = $(CPPFLAGS_ALL_COMMON) $(LTDLINCL)
LDFLAGS_LIB_COMMON = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
LIBADD_LIB_COMMON = $(LIBM)
DEPENDENCIES_LIB_COMMON =

if ENABLE_LIBLTDL
LIBADD_LIB_COMMON += $(LTDL_LIBS)
else
LIBADD_LIB_COMMON += $(DL_LIBS)
endif

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
	hawk-chr.h \
	hawk-cmn.h \
	hawk-ecs.h \
	hawk-fmt.h \
	hawk-gem.h \
	hawk-htb.h \
	hawk-rbt.h \
	hawk-pack1.h \
	hawk-pma.h \
	hawk-utl.h \
	hawk-std.h \
	hawk-tre.h \
	hawk-unpack.h


##################################################
# libraries to deploy
##################################################
pkglibdir = $(libdir)
pkglib_LTLIBRARIES = libhawk.la

libhawk_la_SOURCES =  \
	$(pkginclude_HEADERS) \
	arr.c \
	chr.c \
	ecs-imp.h \
	ecs.c \
	err-prv.h \
	err.c \
	fmt-imp.h \
	fmt.c \
	fnc-prv.h \
	fnc.c \
	htb.c \
	gem.c \
	hawk-prv.h \
	hawk.c \
	mb8.c \
	misc-prv.h \
	misc.c \
	parse-prv.h \
	parse.c \
	pma.c \
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
	tre-parse.c \
	tre-parse.h \
	tre-stack.c \
	tre-stack.h \
	tre.c \
	tree-prv.h \
	tree.c \
	utf16.c \
	utf8.c \
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
	cli.c \
	fio.c \
	mtx.c \
	pio.c \
	sio.c \
	syscall.h \
	syserr.h \
	tio.c \
	std-prv.h \
	std.c 

libhawk_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
libhawk_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhawk_la_LIBADD = $(LIBADD_LIB_COMMON)
libhawk_la_DEPENDENCIES = $(DEPENDENCIES_LIB_COMMON)

if ENABLE_CXX
pkglib_LTLIBRARIES += libhawkxx.la
libhawkxx_la_SOURCES = Hawk.cpp StdHawk.cpp
libhawkxx_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
libhawkxx_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhawkxx_la_LIBADD = -lhawk $(LIBADD_LIB_COMMON) 
libhawkxx_la_DEPENDENCIES = libhawk.la
endif

if ENABLE_STATIC_MODULE

##################################################
# STATIC MODULES BUILT INTO MAIN LIBRARY
##################################################
libhawk_la_SOURCES += \
	imap-imp.h \
	mod-math.c mod-math.h \
	mod-str.c mod-str.h
##libhawk_la_LIBADD +=

### mod-dir.c mod-dir.h 
### mod-sys.c mod-sys.h

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

#modexecdir = $(libdir)
#
#modexec_LTLIBRARIES = libhawk-dir.la
#libhawk_dir_la_SOURCES = mod-dir.c mod-dir.h
#libhawk_dir_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
#libhawk_dir_la_LDFLAGS = $(LDFLAGS_MOD_COMMON) -L../si
#libhawk_dir_la_LIBADD = $(LIBADD_MOD_COMMON) -lqsesi
#libhawk_dir_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON) ../../lib/si/libqsesi.la
#
#modexec_LTLIBRARIES += libhawk-math.la
#libhawk_math_la_SOURCES = mod-math.c mod-math.h
#libhawk_math_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
#libhawk_math_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
#libhawk_math_la_LIBADD = $(LIBADD_MOD_COMMON)
#libhawk_math_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON)
#
#modexec_LTLIBRARIES += libhawk-str.la
#libhawk_str_la_SOURCES = mod-str.c mod-str.h
#libhawk_str_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
#libhawk_str_la_LDFLAGS = $(LDFLAGS_MOD_COMMON)
#libhawk_str_la_LIBADD = $(LIBADD_MOD_COMMON)
#libhawk_str_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON)
#
#modexec_LTLIBRARIES += libhawk-sys.la
#libhawk_sys_la_SOURCES = mod-sys.c mod-sys.h
#libhawk_sys_la_CPPFLAGS = $(CPPFLAGS_MOD_COMMON)
#libhawk_sys_la_LDFLAGS = $(LDFLAGS_MOD_COMMON) -L../si
#libhawk_sys_la_LIBADD = $(LIBADD_MOD_COMMON) -lqsesi
#libhawk_sys_la_DEPENDENCIES = $(DEPENDENCIES_MOD_COMMON) ../../lib/si/libqsesi.la

endif
