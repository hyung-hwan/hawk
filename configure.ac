dnl AC_PREREQ([2.71])

AC_INIT([hawk],[0.9.8],[Chung, Hyung-Hwan (hyunghwan.chung@gmail.com)],[],[https://code.miflux.net/hyung-hwan/hawk])

AC_CONFIG_HEADERS([lib/hawk-cfg.h])
AC_CONFIG_AUX_DIR([ac])
AC_CONFIG_MACRO_DIR([m4])

dnl use CC and CPP for compilation tests with .c test programs.
AC_LANG(C)

dnl initialize automake
AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects foreign])
AM_PROG_AR

AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_REQUIRE_AUX_FILE([test-driver])

dnl enable silent rules if automake supports them.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([no])])
dnl m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl determine a C compiler to use
AC_PROG_CC

dnl determine a C++ compiler to use
AC_PROG_CXX

dnl check if the C++ compiler exists in PATH
AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)

if test "${HAVE_CXX}" = "yes"  
then
	dnl check if the C++ compiler supports namespace
	AX_CXX_NAMESPACES
	dnl HAVE_CXX11
	AX_CXX_COMPILE_STDCXX(11, , optional)
fi

dnl determine some key programs
AC_PROG_INSTALL
AC_PROG_EGREP
AC_PROG_LN_S
AC_PROG_SED

dnl initialize libtool
LT_INIT
AC_SUBST(LIBTOOL_DEPS)

dnl overrides the default CFLAGS setting
if test "$ac_test_CFLAGS" != "set"
then
	if test "$GCC" = "yes"
	then
		CFLAGS="$CFLAGS -Wall"
	fi
fi

if test "$ac_test_CXXFLAGS" != "set"
then
	if test "$GCC" = "yes"
	then
		CXXFLAGS="$CXXFLAGS -Wall"
	fi
fi

dnl indicate the existence of config.h
CFLAGS="$CFLAGS -DHAWK_HAVE_CFG_H"
CXXFLAGS="$CXXFLAGS -DHAWK_HAVE_CFG_H"

HAWK_TRY_CFLAGS([-fshort-wchar])
HAWK_TRY_CFLAGS([-fno-short-wchar])

dnl make visible the 64-bit interface to the file system
AC_SYS_LARGEFILE()

dnl define the WIN32 conditional if necessary
case "$host" in
*-*-mingw*|*-*-cygwin*)
	#CFLAGS="$CFLAGS -D_WIN32_WINNT=0x0400"
	platform_win32=yes
	platform_macosx=no
	;;
*-*-darwin*)
	platform_win32=no
	platform_macosx=yes
	;;
*)
	platform_win32=no
	platform_macosx=no
	;;
esac
AM_CONDITIONAL(WIN32, test "x${platform_win32}" = "xyes" )
AM_CONDITIONAL(MACOSX, test "x${platform_macosx}" = "xyes" )

dnl check the math library (check if -lm is needed)
LT_LIB_M
AC_SUBST(LIBM, $LIBM)

dnl ===== enable-wide-char =====
dnl this enable option is placed way above other options
dnl as -fshort-wchar affects the size of wchar_t.
AC_ARG_ENABLE([wide-char], 
	[AS_HELP_STRING([--enable-wide-char],[Use the wide-character type as the default character type. one of yes, no, yes:2, yes:2w, yes:4, yes:4w, no:2, no:2w, no:4, no:4w (default. yes)])],
	enable_wide_char=$enableval,
	enable_wide_char=yes
)
if test "${enable_wide_char}" = "yes:4" -o "${enable_wide_char}" = "yes:4w"
then
	AC_DEFINE([HAWK_WIDE_CHAR_SIZE],[4],[Wide-character type size])
	AC_DEFINE([HAWK_ENABLE_WIDE_CHAR],[1],[Use the wide-character type as the default character type])
	if test "${platform_win32}" = "yes"
	then
		[CFLAGS="$CFLAGS -DUNICODE -D_UNICODE"]
		[CXXFLAGS="$CXXFLAGS -DUNICODE -D_UNICODE"]
	fi 

	if test "${ac_cv_cflags__fno_short_wchar}" = "yes"
	then
		[CFLAGS="$CFLAGS -fno-short-wchar"]
		[CXXFLAGS="$CXXFLAGS -fno-short-wchar"]
	fi
	if test "${enable_wide_char}" = "yes:4w"
	then
		AC_DEFINE([HAWK_PREFER_PREFIX_L],[1],[Prefix wide-character literals with L])
	fi

elif test "${enable_wide_char}" = "yes:2" -o "${enable_wide_char}" = "yes:2w" -o "${enable_wide_char}" = "yes"
then
	AC_DEFINE([HAWK_WIDE_CHAR_SIZE],[2],[Wide-character type size])
	AC_DEFINE([HAWK_ENABLE_WIDE_CHAR],[1],[Use the wide-character type as the default character type])
	if test "${platform_win32}" = "yes"
	then
		[CFLAGS="$CFLAGS -DUNICODE -D_UNICODE"]
		[CXXFLAGS="$CXXFLAGS -DUNICODE -D_UNICODE"]
	fi
	if test "${ac_cv_cflags__fshort_wchar}" = "yes"
	then
		[CFLAGS="$CFLAGS -fshort-wchar"]
		[CXXFLAGS="$CXXFLAGS -fshort-wchar"]
	fi
	if test "${enable_wide_char}" = "yes:2w"
	then
		AC_DEFINE([HAWK_PREFER_PREFIX_L],[1],[Prefix wide-character literals with L])
	fi

elif test "${enable_wide_char}" = "no:4" -o test "${enable_wide_char}" = "no:4w"
then
	AC_DEFINE([HAWK_WIDE_CHAR_SIZE],[4],[Wide-character type size])

	if test "${ac_cv_cflags__fno_short_wchar}" = "yes"
	then
		[CFLAGS="$CFLAGS -fno-short-wchar"]
		[CXXFLAGS="$CXXFLAGS -fno-short-wchar"]
	fi

	if test "${enable_wide_char}" = "no:4w"
	then
		AC_DEFINE([HAWK_PREFER_PREFIX_L],[1],[Prefix wide-character literals with L])
	fi
else
	dnl no:2, no:2w, no, or anything else
	AC_DEFINE([HAWK_WIDE_CHAR_SIZE],[2],[Wide-character type size])

	if test "${ac_cv_cflags__fshort_wchar}" = "yes"
	then
		[CFLAGS="$CFLAGS -fshort-wchar"]
		[CXXFLAGS="$CXXFLAGS -fshort-wchar"]
	fi

	if test "${enable_wide_char}" = "no:2w"
	then
		AC_DEFINE([HAWK_PREFER_PREFIX_L],[1],[Prefix wide-character literals with L])
	fi
fi

AC_MSG_CHECKING([for char16_t literal])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([], [const void* x = u"ab cd ef gh";])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAWK_HAVE_PREFIX_SMALL_U], [1], [char16_t literal prefix])],
	[AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING([for char32_t literal])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([], [const void* x = U"ab cd ef gh";])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAWK_HAVE_PREFIX_BIG_U], [1], [char32_t literal prefix])],
	[AC_MSG_RESULT(no)]
)

dnl check some compiler builtins
AC_MSG_CHECKING([for __builtin_memset])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([], [__builtin_memset ((void*)1, ' ', 10);])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAVE___BUILTIN_MEMSET], [1], [__builtin_memset])],
	[AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING([for __builtin_memcpy])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([], [__builtin_memcpy ((void*)1, (void*)2, 10);])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAVE___BUILTIN_MEMCPY], [1], [__builtin_memcpy])],
	[AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING([for __builtin_memmove])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([], [__builtin_memmove ((void*)1, (void*)2, 10);])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAVE___BUILTIN_MEMMOVE], [1], [__builtin_memmove])],
	[AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING([for __builtin_memcmp])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([], [int a = __builtin_memcmp ((void*)1, (void*)2, 10);])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAVE___BUILTIN_MEMCMP], [1], [__builtin_memcmp])],
	[AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING([labels as values])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM( [], [[void* jp[] = { &&jpt1, &&jpt2, &&jpt3 }; goto *jp[1]; jpt1: 1; jpt2: 2; jpt3: 3; ]])],
	[AC_MSG_RESULT(yes)
	 AC_DEFINE([HAVE_LABELS_AS_VALUES], [1], [labels as values])],
	[AC_MSG_RESULT(no)]
)

if test "${HAVE_CXX}" = "yes"  
then
	AC_LANG_PUSH([C++])
	AC_MSG_CHECKING([unsigned int usability as the first parameter type of operator new])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM( [[void* operator new (unsigned int) { return (void*)0; }]], [[ ]])],
		[AC_MSG_RESULT(yes)
		AC_DEFINE([HAWK_PREFER_UNSIGNED_INT_FOR_OPERATOR_NEW], [1], [prefer unsigned int for the first parameter of operator new])],
		[AC_MSG_RESULT(no)]
	)
	AC_LANG_POP([C++])
fi

AC_CHECK_HEADERS([stddef.h wchar.h wctype.h errno.h signal.h fcntl.h dirent.h])
AC_CHECK_HEADERS([time.h sys/time.h utime.h spawn.h execinfo.h ucontext.h])
AC_CHECK_HEADERS([sys/resource.h sys/wait.h sys/syscall.h sys/ioctl.h sys/sysctl.h])
AC_CHECK_HEADERS([dlfcn.h ltdl.h sys/mman.h])
AC_CHECK_HEADERS([sys/devpoll.h sys/epoll.h poll.h sys/event.h])
AC_CHECK_HEADERS([netinet/in.h sys/un.h netpacket/packet.h net/if.h net/if_dl.h net/route.h], [], [], [
	#include <sys/types.h>
	#include <sys/socket.h>])
AC_CHECK_HEADERS([sys/stropts.h sys/macstat.h linux/ethtool.h linux/sockios.h])
AC_CHECK_HEADERS([ffi.h libunwind.h quadmath.h crt_externs.h uci.h libmemcached/memcached.h])

dnl check data types
dnl AC_CHECK_TYPE([wchar_t], 
dnl 	[ AC_DEFINE([HAVE_WCHAR_T_IN_STDDEF_H],
dnl	           [],[wchar_t is available in stddef.h]) ],
dnl	[],
dnl	[#include <stddef.h>])

dnl check functions
AC_CHECK_FUNCS([gettimeofday settimeofday clock_gettime clock_settime getitimer setitimer])
AC_CHECK_FUNCS([timelocal timegm localtime_r gmtime_r])
AC_CHECK_FUNCS([backtrace backtrace_symbols])
AC_CHECK_FUNCS([makecontext swapcontext getcontext setcontext])
AC_CHECK_FUNCS([clock_nanosleep nanosleep usleep select])
AC_CHECK_FUNCS([sigaction signal getpgid getpgrp])
AC_CHECK_FUNCS([snprintf _vsnprintf _vsnwprintf strerror_r initstate_r srandom_r random_r random])
AC_CHECK_FUNCS([accept4 pipe2 epoll_create epoll_create1 kqueue kqueue1])
AC_CHECK_FUNCS([isatty mmap munmap])
AC_CHECK_FUNCS([readdir64 dirfd faccessat])
AC_CHECK_FUNCS([stat64 fstat64 lstat64 fstatat64 fstat fstatat])

OLDLIBS="$LIBS"
LIBS="$LIBM $LIBS"
AC_CHECK_FUNCS([powl fmodl sinl cosl tanl sinhl coshl tanhl asinl acosl atanl atan2l logl log2l log10l expl sqrtl ceill floorl roundl])
AC_CHECK_FUNCS([pow fmod sin cos tan sinh cosh tanh asin acos atan atan2 log log2 log10 exp sqrt ceil floor round])
AC_CHECK_FUNCS([powf fmodf sinf cosf tanf sinhf coshf tanhf asinf acosf atanf atan2f logf log2f log10f expf sqrtf ceilf floorf roundf])
LIBS="$OLDLIBS"

save_LIBS="$LIBS"
AC_SEARCH_LIBS([dlopen], [dl dld], [
	DL_LIBS="$ac_cv_search_dlopen"
	if test "x${DL_LIBS}" = "xnone required"
	then
		DL_LIBS=""
	fi
])
LIBS="$save_LIBS"
AC_SUBST(DL_LIBS)

save_LIBS="$LIBS"
AC_SEARCH_LIBS([lt_dlopen], [ltdl], [
	LTDL_LIBS="$ac_cv_search_lt_dlopen"
	if test "x${LTDL_LIBS}" = "xnone required"
	then
		LTDL_LIBS=""
	fi
])
LIBS="$save_LIBS"
AC_SUBST(LTDL_LIBS)

AC_CHECK_LIB([ffi], [ffi_call], 
	[
		FFI_LIBS="-lffi"
		AC_DEFINE([HAVE_FFI_LIB], [1], [libffi library is available])
	]
)
AC_SUBST(FFI_LIBS)

OLDLIBS="$LIBS"
LIBS="$FFI_LIBS $LIBS"
AC_CHECK_FUNCS([ffi_prep_cif_var])
LIBS="$OLDLIBS"

if test "x${ac_cv_header_libunwind_h}" = "xyes" 
then
	AC_CHECK_LIB([unwind], [unw_backtrace], 
		[
			UNWIND_LIBS="-lunwind"
			AC_DEFINE([HAVE_UNWIND_LIB], [1], [libunwind is available])
		]
	)
	AC_SUBST(UNWIND_LIBS)
fi

dnl OLDLIBS="$LIBS"
dnl AC_SEARCH_LIBS([connect], [socket])
dnl LIBS="$OLDLIBS"
dnl if test "$ac_cv_search_connect" != "none required" -a "$ac_cv_search_connect" != "no"
dnl then
dnl     SOCKET_LIBS="$SOCKET_LIBS -lsocket"
dnl fi
dnl AC_SUBST(SOCKET_LIBS)
AC_CHECK_FUNCS([connect gethostbyname])
if test "x$ac_cv_func_gethostbyname" = "xno"
then
	AC_CHECK_LIB([nsl], [gethostbyname], [
		SOCKET_LIBS="$SOCKET_LIBS -lnsl"
		AC_DEFINE(HAVE_GETHOSTBYNAME, 1)
	])
fi
if test "x$ac_cv_func_connect" = "xno"
then
	AC_CHECK_LIB([socket], [connect], [
		SOCKET_LIBS="$SOCKET_LIBS -lsocket"
		AC_DEFINE(HAVE_CONNECT, 1) ], [
		AC_CHECK_LIB([network], [connect], [
			SOCKET_LIBS="$SOCKET_LIBS -lnetwork"
			AC_DEFINE(HAVE_CONNECT, 1) ])
	])
fi
if test "x${platform_win32}" = "xyes"
then
	SOCKET_LIBS="$SOCKET_LIBS -lws2_32"
fi
AC_SUBST(SOCKET_LIBS)

dnl pthread
AX_PTHREAD()
AC_CHECK_LIB([pthread], [pthread_mutex_timedlock],  [
	AC_DEFINE([HAVE_PTHREAD_MUTEX_TIMEDLOCK],1,[pthreads has pthread_mutex_timedlock()])
])
AC_CHECK_LIB([pthread], [pthread_mutex_trylock],  [
	AC_DEFINE([HAVE_PTHREAD_MUTEX_TRYLOCK],1,[pthreads has pthread_mutex_trylock()])
])

dnl check is the import library for unicows.dll exists
dnl this check doesn't look for a particular symbol
dnl but for the symbol 'main' since i want to check 
dnl the existence of the libaray.
AC_CHECK_LIB([unicows], [main], [UNICOWS_LIBS="-lunicows"])
AC_SUBST(UNICOWS_LIBS)

AC_CHECK_LIB([rt], [clock_gettime], [LIBS="$LIBS -lrt"])


dnl libuci (optional)
AC_CHECK_LIB([uci], [uci_alloc_context], [UCI_LIBS="-luci"])
AC_SUBST(UCI_LIBS)
AM_CONDITIONAL(HAVE_LIBUCI, test "x${ac_cv_lib_uci_uci_alloc_context}" = "xyes" -a "x${ac_cv_header_uci_h}" = "xyes")

dnl libmemcachd (optional)
AC_CHECK_LIB([memcached], [memcached],
	[
		MEMCACHED_LIBS="-lmemcached"
		AC_DEFINE([HAVE_MEMCACHED_LIB], [1], [libmemcached library is available])
	])
AC_SUBST(MEMCACHED_LIBS)

AC_MSG_CHECKING([for va_copy])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdarg.h>]], [[
		va_list x, y;
		va_copy(x,y);
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_VA_COPY], [1], [va_copy is available])
	],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING([for __va_copy])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdarg.h>]], [[
		va_list x, y;
		__va_copy(x,y); 
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE___VA_COPY], [1], [__va_copy is available])
	],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING([for strftime %z])
AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <string.h>
	 #if defined(HAVE_SYS_TIME_H)
	 #include <sys/time.h>
	 #endif
	 #if defined(HAVE_TIME_H)
	 #include <time.h>
	 #endif
	int main ()
	{
		char buf[100];
		time_t t = 0;
		strftime (buf, sizeof(buf), "%z", gmtime(&t));
		return (strcmp(buf, "%z") == 0 || strcmp(buf, "z") == 0)? 1: 0;
	}]])],[
	 	AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_STRFTIME_SMALL_Z], [1], [strftime supports %z])
	],[AC_MSG_RESULT(no)],[
	 	dnl cross-compiling, assume yes
	 	AC_MSG_RESULT(unknown)
		AC_DEFINE([HAVE_STRFTIME_SMALL_Z], [1], [strftime supports %z])
])

AC_MSG_CHECKING([return type of unsetenv()])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM( [#include <stdlib.h>], [
	 	return unsetenv("foo")
	])],

	[AC_MSG_RESULT(int)],

	[AC_MSG_RESULT(void)
	 AC_DEFINE([UNSETENV_RETURNS_VOID], [1], [unsetenv returns int])]
)

dnl check struct members
AC_STRUCT_DIRENT_D_TYPE
AC_CHECK_MEMBERS([DIR.d_fd, DIR.dd_fd],,,[[#include <dirent.h>]])
AC_CHECK_MEMBERS([struct stat.st_birthtime])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_birthtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_birthtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct tm.tm_gmtoff],,,[[#include <time.h>]])
AC_CHECK_MEMBERS([struct tm.__tm_gmtoff],,,[[#include <time.h>]])
AC_CHECK_MEMBERS([struct tm.tm_zone],,,[[#include <time.h>]])
AC_CHECK_MEMBERS([struct tm.__tm_zone],,,[[#include <time.h>]])

AC_CHECK_MEMBERS([struct ifreq.ifr_ifindex, struct ifreq.ifr_mtu], [], [],
	[#include <sys/types.h>
	 #include <sys/socket.h>
	 #ifdef HAVE_NET_IF_H
	 #include <net/if.h>
	 #endif])

AC_CHECK_TYPES([struct lifconf, struct lifreq, struct if_laddrreq], [], [],
	[#include <sys/types.h>
	 #include <sys/socket.h>
	 #ifdef HAVE_NET_IF_H
	 #include <net/if.h>
	 #endif])

dnl check the size of primitive data types
AC_CHECK_SIZEOF(char,,[[]])
AC_CHECK_SIZEOF(short,,[[]])
AC_CHECK_SIZEOF(int,,[[]])
AC_CHECK_SIZEOF(long,,[[]])
AC_CHECK_SIZEOF(long long,,[[]])
AC_CHECK_SIZEOF(__int8,,[[]])
AC_CHECK_SIZEOF(__int16,,[[]])
AC_CHECK_SIZEOF(__int32,,[[]])
AC_CHECK_SIZEOF(__int64,,[[]])
AC_CHECK_SIZEOF(__int128,,[[]])
AC_CHECK_SIZEOF(__int8_t,,[[]])
AC_CHECK_SIZEOF(__int16_t,,[[]])
AC_CHECK_SIZEOF(__int32_t,,[[]])
AC_CHECK_SIZEOF(__int64_t,,[[]])
AC_CHECK_SIZEOF(__int128_t,,[[]])
AC_CHECK_SIZEOF(__uint128_t,,[[]])
dnl AC_CHECK_SIZEOF doesn't work without white-space between void and *
AC_CHECK_SIZEOF(void *,,[[]])
AC_CHECK_SIZEOF(float,,[[]])
AC_CHECK_SIZEOF(double,,[[]])
AC_CHECK_SIZEOF(long double,,[[]])
AC_CHECK_SIZEOF(__float128,,[[]])

AC_CHECK_SIZEOF(wchar_t)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(off64_t)

AC_CHECK_SIZEOF(mbstate_t,,[#include <wchar.h>])
AX_CHECK_NUMVALOF(MB_LEN_MAX,[32],[#include <limits.h>])
AX_CHECK_NUMVALOF(PATH_MAX,[0],[#include <limits.h>])
AX_CHECK_NUMVALOF(NSIG,[32],[#include <signal.h>])

AX_CHECK_NUMVALOF(AF_UNSPEC,   [0],    [#include <sys/types.h>
                                        #include <sys/socket.h>
                                        #include <netinet/in.h>])
AX_CHECK_NUMVALOF(AF_INET,     [-1],   [#include <sys/types.h>
                                        #include <sys/socket.h>
                                        #include <netinet/in.h>])
AX_CHECK_NUMVALOF(AF_INET6,    [-2],   [#include <sys/types.h>
                                        #include <sys/socket.h>
                                        #include <netinet/in.h>])
AX_CHECK_NUMVALOF(AF_PACKET,   [-3],   [#include <sys/types.h>
                                        #include <sys/socket.h>
                                        #include <netinet/in.h>])
AX_CHECK_NUMVALOF(AF_UNIX,     [-4],   [#include <sys/types.h>
                                        #include <sys/socket.h>
                                        #include <sys/un.h>])

if test "x${platform_win32}" = "xyes"
then
	AC_CHECK_SIZEOF(struct sockaddr_in,,[
		#include <winsock2.h>
		#include <ws2tcpip.h>])
	AC_CHECK_SIZEOF(struct sockaddr_in6,,[
		#include <winsock2.h>
		#include <ws2tcpip.h>])
	AC_CHECK_SIZEOF(struct sockaddr_un,,[
		#include <winsock2.h>
		#include <ws2tcpip.h>])
	AC_CHECK_SIZEOF(struct sockaddr_ll,,[
		#include <winsock2.h>
		#include <ws2tcpip.h>])
	AC_CHECK_SIZEOF(struct sockaddr_dl,,[
		#include <winsock2.h>
		#include <ws2tcpip.h>])

	AC_CHECK_SIZEOF(socklen_t,, [
		#include <winsock2.h>
		#include <ws2tcpip.h>])
	if test ${ac_cv_sizeof_socklen_t} -gt 0
	then
		AX_CHECK_SIGN([socklen_t],
			[ AC_DEFINE(HAWK_SOCKLEN_T_IS_SIGNED, 1, [Define if socklen_t is signed]) ],
			[ AS_ECHO_N("") ],
			[#include <winsock2.h>
			 #include <ws2tcpip.h>])
	fi

	AC_CHECK_SIZEOF(sa_family_t,, [
		#include <winsock2.h>
		#include <ws2tcpip.h>])
	if test ${ac_cv_sizeof_sa_family_t} -gt 0
	then
		AX_CHECK_SIGN([sa_family_t],
			[ AC_DEFINE(HAWK_SA_FAMILY_T_IS_SIGNED, 1, [Define if sa_family_t is signed]) ],
			[ AS_ECHO_N("") ],
			[#include <winsock2.h>
			 #include <ws2tcpip.h>])
	fi

	AC_MSG_CHECKING([offset of sa_family in struct sockaddr])
	AC_COMPUTE_INT([ac_cv_offsetof_sa_family],
		[[offsetof(struct sockaddr, sa_family)]],
		[#include <stddef.h>
		 #include <winsock2.h>
		 #include <ws2tcpip.h>], [ac_cv_offsteof_sa_family=0])
	AC_MSG_RESULT([$ac_cv_offsetof_sa_family])
	AC_DEFINE_UNQUOTED(HAWK_OFFSETOF_SA_FAMILY, ${ac_cv_offsetof_sa_family}, [offsetof(struct sockaddr, sa_family)])

else
	AC_CHECK_SIZEOF(struct sockaddr_in,,[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>])
	AC_CHECK_SIZEOF(struct sockaddr_in6,,[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>])
	AC_CHECK_SIZEOF(struct sockaddr_un,,[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <sys/un.h>])
	AC_CHECK_SIZEOF(struct sockaddr_ll,,[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netpacket/packet.h>])
	AC_CHECK_SIZEOF(struct sockaddr_dl,,[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <net/if_dl.h>])


	AC_CHECK_SIZEOF(socklen_t,, [
		#include <sys/types.h>
		#include <sys/socket.h>])
	if test ${ac_cv_sizeof_socklen_t} -gt 0
	then
		AX_CHECK_SIGN([socklen_t],
			[ AC_DEFINE(HAWK_SOCKLEN_T_IS_SIGNED, 1, [Define if socklen_t is signed]) ],
			[ AS_ECHO_N("") ],
			[#include <sys/types.h>
			 #include <sys/socket.h>])
	fi

	AC_CHECK_SIZEOF(sa_family_t,, [
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>])
	if test ${ac_cv_sizeof_sa_family_t} -gt 0
	then
		AX_CHECK_SIGN([sa_family_t],
			[ AC_DEFINE(HAWK_SA_FAMILY_T_IS_SIGNED, 1, [Define if sa_family_t is signed]) ],
			[ AS_ECHO_N("") ],
			[#include <sys/types.h>
			 #include <sys/socket.h>
			 #include <netinet/in.h>])
	fi

	AC_MSG_CHECKING([offset of sa_family in struct sockaddr])
	AC_COMPUTE_INT([ac_cv_offsetof_sa_family],
		[[offsetof(struct sockaddr, sa_family)]],
		[#include <stddef.h>
		 #include <sys/types.h>
		 #include <sys/socket.h>], [ac_cv_offsteof_sa_family=0])
	AC_MSG_RESULT([$ac_cv_offsetof_sa_family])
	AC_DEFINE_UNQUOTED(HAWK_OFFSETOF_SA_FAMILY, ${ac_cv_offsetof_sa_family}, [offsetof(struct sockaddr, sa_family)])
fi

AC_CHECK_SIZEOF(pthread_t,, [#include <pthread.h>])
if test ${ac_cv_sizeof_pthread_t} -gt 0
then
	AX_CHECK_SIGN([pthread_t],
		[AC_DEFINE(HAWK_PTHREAD_T_IS_SIGNED, 1, [Define if pthread_t is signed])],
		[AS_ECHO_N("")],
		[#include <pthread.h>])
fi

AC_CHECK_SIZEOF(pthread_mutex_t,, [#include <pthread.h>])
if test ${ac_cv_sizeof_pthread_mutex_t} -gt 0
then
	AX_CHECK_SIGN([pthread_mutex_t],
		[ AC_DEFINE(HAWK_PTHREAD_MUTEX_T_IS_SIGNED, 1, [Define if pthread_mutex_t is signed]) ],
		[ AS_ECHO_N("") ],
		[#include <pthread.h>])
fi

AC_CHECK_SIZEOF(pthread_cond_t,, [#include <pthread.h>])
if test ${ac_cv_sizeof_pthread_cond_t} -gt 0
then
	AX_CHECK_SIGN([pthread_cond_t],
		[ AC_DEFINE(HAWK_PTHREAD_MUTEX_T_IS_SIGNED, 1, [Define if pthread_cond_t is signed]) ],
		[ AS_ECHO_N("") ],
		[#include <pthread.h>])
fi

dnl gcc 3.4.3 on opensolaris x86 gave this warning without -msse or 
dnl something similar.
dnl    SSE vector argument without SSE enabled changes the ABI
dnl it also ended with an internal compiler error while compiling 
dnl some expressions like 'v % base' where v is of the __int128_t type.
dnl so i've added this check to determine if the compiler has
dnl proper support for __int128_t.
dnl 
dnl Changing ac_cv_sizeof___int128_t here doesn't corret SIZEOF___INT128_T
dnl since it's already generated. It only affects HAWK_SIZEOF___INT128_T below.
if test ${ac_cv_sizeof___int128_t} -gt 0
then
	AC_MSG_CHECKING([__int128_t with % and va_arg])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([#include <stdarg.h>], [
			volatile __int128_t x, base;
			va_list ap;
			va_arg(ap, __int128_t);
			x = (__int128_t)&base;
			base = (__int128_t)&x;
			x = x % base;
			return (int)x;
		])],

		[
			AC_MSG_RESULT(yes)
		],
		[
			AC_MSG_RESULT(no)
			ac_cv_sizeof___int128_t=0
			ac_cv_sizeof___uint128_t=0
		]
	)
fi

dnl Some compilers doesn't seem to have full support for __float128 
dnl even if the type is available.
if test ${ac_cv_sizeof___float128} -gt 0
then
	AC_MSG_CHECKING([__float128 with linking])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[
			__float128 x, y, z;
			z = x * y + x;
			printf ("%p", (char*)(int)z);
		]])],[AC_MSG_RESULT(yes)],[
			AC_MSG_RESULT(no)
			ac_cv_sizeof___float128=0
	])
fi

dnl Quadmath functions may not be in the default c library.
dnl Find a library containing them. Disable the float128 type
dnl if they are not available.
if test ${ac_cv_sizeof___float128} -gt 0
then
	AC_CHECK_FUNCS([quadmath_snprintf])
	if test "$ac_cv_func_quadmath_snprintf" = "no"
	then
		OLDLIBS="$LIBS"
		LIBS="$LIBM $LIBS"
		AC_CHECK_FUNCS([quadmath_snprintf])
		LIBS="$OLDLIBS"
	
		if test "$ac_cv_func_quadmath_snprintf" = "no"
		then
			dnl quadmath_snprintf not avalable in the 
			dnl standard math lib.

			AC_CHECK_LIB([quadmath], [quadmath_snprintf], [
				QUADMATH_LIBS="-lquadmath"
				LIBM="$LIBM -lquadmath"
				AC_DEFINE(HAVE_QUADMATH_SNPRINTF, 1)
			])

			if test "$ac_cv_lib_quadmath_quadmath_snprintf" = "no"
			then
				dnl quadmath_snprintf not avalable in the 
				dnl quadmath lib.

				ac_cv_sizeof___float128=0
			fi

		else
			QUADMATH_LIBS="$LIBM"
		fi
	fi

	if test ${ac_cv_sizeof___float128} -gt 0
	then
		OLDLIBS="$LIBS"
		LIBS="$LIBM $LIBS"
		AC_CHECK_FUNCS([powq fmodq sinq cosq tanq sinhq coshq tanhq asinq acosq atanq atan2q logq log10q expq sqrtq ceilq floorq roundq])
		AC_CHECK_FUNCS([strtoflt128])
		LIBS="$OLDLIBS"
	fi
fi
AC_SUBST(QUADMATH_LIBS)


AC_DEFINE_UNQUOTED(HAWK_SIZEOF_CHAR, ${ac_cv_sizeof_char}, [sizeof(char)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_SHORT, ${ac_cv_sizeof_short}, [sizeof(short)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_INT, ${ac_cv_sizeof_int}, [sizeof(int)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_LONG, ${ac_cv_sizeof_long}, [sizeof(long)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_LONG_LONG, ${ac_cv_sizeof_long_long}, [sizeof(long long)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___INT8, ${ac_cv_sizeof___int8}, [sizeof(__int8)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___INT16, ${ac_cv_sizeof___int16}, [sizeof(__int16)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___INT32, ${ac_cv_sizeof___int32}, [sizeof(__int32)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___INT64, ${ac_cv_sizeof___int64}, [sizeof(__int64)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___INT128, ${ac_cv_sizeof___int128}, [sizeof(__int128)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___INT128_T, ${ac_cv_sizeof___int128_t}, [sizeof(__int128_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___UINT128_T, ${ac_cv_sizeof___uint128_t}, [sizeof(__uint128_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_VOID_P, ${ac_cv_sizeof_void_p}, [sizeof(void*)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_FLOAT, ${ac_cv_sizeof_float}, [sizeof(float)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_DOUBLE, ${ac_cv_sizeof_double}, [sizeof(double)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_LONG_DOUBLE, ${ac_cv_sizeof_long_double}, [sizeof(long double)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF___FLOAT128, ${ac_cv_sizeof___float128}, [sizeof(__float128)])

AC_DEFINE_UNQUOTED(HAWK_SIZEOF_WCHAR_T, ${ac_cv_sizeof_wchar_t}, [sizeof(wchar_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_OFF_T, ${ac_cv_sizeof_off_t}, [sizeof(off_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_OFF64_T, ${ac_cv_sizeof_off64_t}, [sizeof(off64_t)])

AC_DEFINE_UNQUOTED(HAWK_SIZEOF_MBSTATE_T, ${ac_cv_sizeof_mbstate_t}, [sizeof(mbstate_t)])
AC_DEFINE_UNQUOTED(HAWK_MBLEN_MAX, ${ax_cv_numvalof_MB_LEN_MAX}, [MB_LEN_MAX])
if test ${ax_cv_numvalof_PATH_MAX} -gt 0
then
	AC_DEFINE_UNQUOTED(HAWK_PATH_MAX, ${ax_cv_numvalof_PATH_MAX}, [PATH_MAX])
fi
AC_DEFINE_UNQUOTED(HAWK_NSIG, ${ax_cv_numvalof_NSIG}, [NSIG])

AC_DEFINE_UNQUOTED(HAWK_AF_UNSPEC, (${ax_cv_numvalof_AF_UNSPEC}), [AF_UNSPEC])
AC_DEFINE_UNQUOTED(HAWK_AF_INET, (${ax_cv_numvalof_AF_INET}), [AF_INET])
AC_DEFINE_UNQUOTED(HAWK_AF_INET6, (${ax_cv_numvalof_AF_INET6}), [AF_INET6])
AC_DEFINE_UNQUOTED(HAWK_AF_PACKET, (${ax_cv_numvalof_AF_PACKET}), [AF_PACKET])
AC_DEFINE_UNQUOTED(HAWK_AF_UNIX, (${ax_cv_numvalof_AF_UNIX}), [AF_UNIX])

AC_DEFINE_UNQUOTED(HAWK_SIZEOF_STRUCT_SOCKADDR_IN, ${ac_cv_sizeof_struct_sockaddr_in}, [sizeof(struct sockaddr_in)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_STRUCT_SOCKADDR_IN6, ${ac_cv_sizeof_struct_sockaddr_in6}, [sizeof(struct sockaddr_in6)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_STRUCT_SOCKADDR_LL, ${ac_cv_sizeof_struct_sockaddr_ll}, [sizeof(struct sockaddr_ll)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_STRUCT_SOCKADDR_UN, ${ac_cv_sizeof_struct_sockaddr_un}, [sizeof(struct sockaddr_un)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_STRUCT_SOCKADDR_DL, ${ac_cv_sizeof_struct_sockaddr_dl}, [sizeof(struct sockaddr_dl)])

AC_DEFINE_UNQUOTED(HAWK_SIZEOF_SOCKLEN_T, ${ac_cv_sizeof_socklen_t}, [sizeof(socklen_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_SA_FAMILY_T, ${ac_cv_sizeof_sa_family_t}, [sizeof(sa_family_t)])

AC_DEFINE_UNQUOTED(HAWK_SIZEOF_PTHREAD_T, ${ac_cv_sizeof_pthread_t}, [sizeof(pthread_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_PTHREAD_MUTEX_T, ${ac_cv_sizeof_pthread_mutex_t}, [sizeof(pthread_mutex_t)])
AC_DEFINE_UNQUOTED(HAWK_SIZEOF_PTHREAD_COND_T, ${ac_cv_sizeof_pthread_cond_t}, [sizeof(pthread_cond_t)])

hawk_package_version_major="`echo ${PACKAGE_VERSION} | cut -d. -f1`"
hawk_package_version_minor="`echo ${PACKAGE_VERSION} | cut -d. -f2`"
hawk_package_version_patch="`echo ${PACKAGE_VERSION} | cut -d. -f3`"
AC_SUBST(PACKAGE_VERSION_MAJOR, ${hawk_package_version_major})
AC_SUBST(PACKAGE_VERSION_MINOR, ${hawk_package_version_minor})
AC_SUBST(PACKAGE_VERSION_PATCH, ${hawk_package_version_patch})

AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION, "${PACKAGE_VERSION}", [Package version])
dnl AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MAJOR, $(echo ${VERSION} | cut -d. -f1), [Major version number])
dnl AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MINOR, $(echo ${VERSION} | cut -d. -f2), [Minor version number])
dnl AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_PATCH, $(echo ${VERSION} | cut -d. -f3), [Patch level])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MAJOR, ${hawk_package_version_major}, [Major version number])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MINOR, ${hawk_package_version_minor}, [Minor version number])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_PATCH, ${hawk_package_version_patch}, [Patch level])

AC_DEFINE_UNQUOTED(HAWK_PACKAGE_AUTHOR,"${PACKAGE_BUGREPORT}", [Author])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_NAME,"${PACKAGE_NAME}", [package name])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_URL, "${PACKAGE_URL}", [Project URL])


hawk_package_version_major="`echo ${PACKAGE_VERSION} | cut -d. -f1`"
hawk_package_version_minor="`echo ${PACKAGE_VERSION} | cut -d. -f2`"
hawk_package_version_patch="`echo ${PACKAGE_VERSION} | cut -d. -f3`"
AC_SUBST(PACKAGE_VERSION_MAJOR, ${hawk_package_version_major})
AC_SUBST(PACKAGE_VERSION_MINOR, ${hawk_package_version_minor})
AC_SUBST(PACKAGE_VERSION_PATCH, ${hawk_package_version_patch})

AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION, "${PACKAGE_VERSION}", [Package version])
dnl AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MAJOR, $(echo ${VERSION} | cut -d. -f1), [Major version number])
dnl AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MINOR, $(echo ${VERSION} | cut -d. -f2), [Minor version number])
dnl AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_PATCH, $(echo ${VERSION} | cut -d. -f3), [Patch level])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MAJOR, ${hawk_package_version_major}, [Major version number])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_MINOR, ${hawk_package_version_minor}, [Minor version number])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_VERSION_PATCH, ${hawk_package_version_patch}, [Patch level])

AC_DEFINE_UNQUOTED(HAWK_PACKAGE_AUTHOR,"${PACKAGE_BUGREPORT}", [Author])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_NAME,"${PACKAGE_NAME}", [package name])
AC_DEFINE_UNQUOTED(HAWK_PACKAGE_URL, "${PACKAGE_URL}", [Project URL])

AC_C_BIGENDIAN(
	[AC_DEFINE([HAWK_ENDIAN_BIG],[],[Big Endian])],
	[AC_DEFINE([HAWK_ENDIAN_LITTLE],[],[Little Endian])],
	[AC_DEFINE([HAWK_ENDIAN_UNKNOWN],[],[Unknown Endian])])

dnl ===== enable-debug =====
AC_ARG_ENABLE([debug], 
	[AS_HELP_STRING([--enable-debug],[compile the code in the debug mode (default. no)])],
	enable_debug_is=$enableval,
	enable_debug_is=no)
if test "$enable_debug_is" = "yes"
then
	[CFLAGS="$CFLAGS -g -D_DEBUG -UNDEBUG -U_FORTIFY_SOURCE"]
	[CXXFLAGS="$CXXFLAGS -g -D_DEBUG -UNDEBUG -U_FORTIFY_SOURCE"]
	AC_DEFINE([HAWK_BUILD_DEBUG],[1],[build release/debug])
	AC_SUBST(BUILD_MODE, "debug")
else
	[CFLAGS="$CFLAGS -DNDEBUG -U_DEBUG -U_FORTIFY_SOURCE"]
	[CXXFLAGS="$CXXFLAGS -DNDEBUG -U_DEBUG -U_FORTIFY_SOURCE"]
	AC_DEFINE([HAWK_BUILD_RELEASE],[1],[build release/debug])
	AC_SUBST(BUILD_MODE, "release")
fi

dnl ===== enable-intmax ======
AC_ARG_ENABLE([intmax],
	[AS_HELP_STRING([--enable-intmax],[use hawk_intmax_t for integers (default. no)])],
	enable_intmax_is=$enableval,
	enable_intmax_is=no
)
if test "${enable_intmax_is}" = "yes"
then
	AC_DEFINE([HAWK_USE_INTMAX],[],[use hawk_intmax_t for integers])
fi

dnl ===== enable-fltmax ======
AC_ARG_ENABLE([fltmax],
	[AS_HELP_STRING([--enable-fltmax],[use hawk_fltmax_t for floating-point numbers (default. no)])],
	enable_fltmax_is=$enableval,
	enable_fltmax_is=no
)
if test "${enable_fltmax_is}" = "yes"
then
	AC_DEFINE([HAWK_USE_FLTMAX],[],[use hawk_fltmax_t for floating-point numbers])
fi

dnl ===== enable-cxx =====
AC_ARG_ENABLE([cxx], 
	[AS_HELP_STRING([--enable-cxx],[build the library for C++ if a C++ compiler is available (default. yes)])],
	enable_cxx_is=$enableval,
	enable_cxx_is=yes
)
dnl disable c++ if no c++ compiler was found
test "${HAVE_CXX}" = "yes" || enable_cxx_is="no"
dnl disable c++ if the compiler is too old.
test "${ax_cv_cxx_namespaces}" = "yes" || enable_cxx_is="no"

AM_CONDITIONAL(ENABLE_CXX, test "${enable_cxx_is}" = "yes" )

dnl ===== enable-dynamic-module =====
AC_ARG_ENABLE([dynamic-module],
	[AS_HELP_STRING([--enable-dynamic-module],[enable dynamic module capability(default. yes)])],
	enable_dynamic_module_is=$enableval,
	enable_dynamic_module_is=yes
)
if test "x${enable_shared}" = "xno"
then
	enable_dynamic_module_is="no"
fi

if test "x${enable_dynamic_module_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_DYNAMIC_MODULE],[1],[enable dynamic module capability])
fi
AM_CONDITIONAL(ENABLE_DYNAMIC_MODULE, test "x${enable_dynamic_module_is}" = "xyes")

dnl ===== enable-static-module =====
AC_ARG_ENABLE([static-module],
	[AS_HELP_STRING([--enable-static-module],[build modules statically into the main library(default. no)])],
	enable_static_module_is=$enableval,
	enable_static_module_is=no
)
if test "x${enable_shared}" = "xno" -a "x${enable_static}" = "xyes"
then
	enable_static_module_is="yes"
fi

if test "x${enable_static_module_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_STATIC_MODULE],[1],[link modules statically into the main library])
fi
AM_CONDITIONAL(ENABLE_STATIC_MODULE, test "x${enable_static_module_is}" = "xyes")

dnl ===== enable-libltdl =====
AC_ARG_ENABLE([libltdl],
	[AS_HELP_STRING([--enable-libltdl],[use libltdl(default. no)])],
	enable_libltdl_is=$enableval,
	enable_libltdl_is=no
)
if test "x${enable_libltdl_is}" = "xyes"
then
	if test "x${ac_cv_header_ltdl_h}" = "xyes"  -a "${LTDL_LIBS}" != ""
	then
		AC_DEFINE([HAWK_ENABLE_LIBLTDL],[],[use libltdl when loading a dynamic module])
	else
		enable_libltdl_is="no"
	fi
fi
AM_CONDITIONAL(ENABLE_LIBLTDL, test "x${enable_libltdl_is}" = "xyes")

dnl ===== enable-libunwind =====
AC_ARG_ENABLE([libunwind],
	[AS_HELP_STRING([--enable-libunwind],[use libunwind(default. no)])],
	enable_libunwind_is=$enableval,
	enable_libunwind_is=no
)
if test "x${enable_libunwind_is}" = "xyes"
then
	if test "x${ac_cv_header_libunwind_h}" = "xyes"  -a "${UNWIND_LIBS}" != ""
	then
		AC_DEFINE([HAWK_ENABLE_LIBUNWIND],[],[use libunwind for backtracing stack frames])
	else
		enable_libunwind_is="no"
	fi
fi
AM_CONDITIONAL(ENABLE_LIBUNWIND, test "x${enable_libunwind_is}" = "xyes")


dnl ===== enable-mod-ffi =====
AC_ARG_ENABLE([mod-ffi],
	[AS_HELP_STRING([--enable-mod-ffi],[enable mod/ffi. one of auto, yes, no (default. auto)])],
	enable_mod_ffi_is=$enableval,
	enable_mod_ffi_is=auto
)
if test "x${enable_mod_ffi_is}" = "xauto"
then
	if test "x${ac_cv_header_ffi_h}" != "xyes"
	then
		enable_mod_ffi_is="no"
	elif test "x${ac_cv_lib_ffi_ffi_call}" != "xyes"
	then
		enable_mod_ffi_is="no"
	else
		enable_mod_ffi_is="yes"
	fi
fi
if test "x${enable_mod_ffi_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_MOD_FFI],[1],[build mod/ffi])
fi
AM_CONDITIONAL(ENABLE_MOD_FFI, test "${enable_mod_ffi_is}" = "yes")

dnl ===== enable-mod-mysql =====
AC_ARG_ENABLE([mod-mysql],
	[AS_HELP_STRING([--enable-mod-mysql],[enable mod/mysql. one of auto, yes, no (default. auto)])],
	enable_mod_mysql_is=$enableval,
	enable_mod_mysql_is=auto
)

if test "x${enable_mod_mysql_is}" = "xauto" || test "x${enable_mod_mysql_is}" = "xyes" 
then
	if test ${with_mysql+y}
	then
		## as if --with-mysql is specified for AX_LIB_MYSQL below
		with_mysql=yes
	fi

	AX_LIB_MYSQL()
fi

if test "x${enable_mod_mysql_is}" = "xauto"
then
	if test "x${found_mysql}" != "xyes"
	then
		enable_mod_mysql_is="no"
	else
		enable_mod_mysql_is="yes"
	fi
fi
if test "x${enable_mod_mysql_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_MOD_MYSQL],[1],[build mod/mysql])
fi
AM_CONDITIONAL(ENABLE_MOD_MYSQL, test "${enable_mod_mysql_is}" = "yes")

dnl ===== enable-mod-sed =====
AC_ARG_ENABLE([mod-sed],
	[AS_HELP_STRING([--enable-mod-sed],[enable mod/sed. one of yes, no (default. yes)])],
	enable_mod_sed_is=$enableval,
	enable_mod_sed_is=yes
)
if test "x${enable_mod_sed_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_MOD_SED],[1],[build mod/sed])
fi
AM_CONDITIONAL(ENABLE_MOD_SED, test "${enable_mod_sed_is}" = "yes")

dnl ===== enable-mod-uci =====
AC_ARG_ENABLE([mod-uci],
	[AS_HELP_STRING([--enable-mod-uci],[enable mod/uci. one of auto, yes, no (default. auto)])],
	enable_mod_uci_is=$enableval,
	enable_mod_uci_is=auto
)
if test "x${enable_mod_uci_is}" = "xauto"
then
	if test "x${ac_cv_header_uci_h}" != "xyes"
	then
		enable_mod_uci_is="no"
	elif test "x${ac_cv_lib_uci_uci_alloc_context}" != "xyes"
	then
		enable_mod_uci_is="no"
	else
		enable_mod_uci_is="yes"
	fi
fi
if test "x${enable_mod_uci_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_MOD_UCI],[1],[build mod/uci])
fi
AM_CONDITIONAL(ENABLE_MOD_UCI, test "${enable_mod_uci_is}" = "yes")

dnl ===== enable-mod-memc =====
AC_ARG_ENABLE([mod-memc],
	[AS_HELP_STRING([--enable-mod-memc],[enable mod/memc. one of auto, yes, no (default. auto)])],
	enable_mod_memc_is=$enableval,
	enable_mod_memc_is=auto
)
if test "x${enable_mod_memc_is}" = "xauto"
then
	if test "x${ac_cv_header_libmemcached_memcached_h}" != "xyes"
	then
		enable_mod_memc_is="no"
	elif test "x${ac_cv_lib_memcached_memcached}" != "xyes"
	then
		enable_mod_memc_is="no"
	else
		enable_mod_memc_is="yes"
	fi
fi
if test "x${enable_mod_memc_is}" = "xyes"
then
	AC_DEFINE([HAWK_ENABLE_MOD_MEMC],[1],[build mod/memc])
fi
AM_CONDITIONAL(ENABLE_MOD_MEMC, test "${enable_mod_memc_is}" = "yes")


dnl ==== include pthread options to the default flags ====
dnl keep this as  the last option as it changes the default compile flags.                                              dnl otherwise, other tests may get affected if this option is on. 
AC_ARG_ENABLE([pthread-flags], [AS_HELP_STRING([--enable-pthread-flags],[add thread flags to CFLAGS, CXXFLAGS, LIBS(default. yes)])],
        enable_pthread_flags_is=$enableval,enable_pthread_flags_is=yes)
if test "$enable_pthread_flags_is" = "yes"
then
	[CFLAGS="$CFLAGS $PTHREAD_CFLAGS"]
	[CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"]
	[LIBS="$LIBS $PTHREAD_LIBS"]
fi

dnl ====  subsititution of some key items ====
AC_SUBST(HAWK_PROJECT_AUTHOR, "${PACKAGE_BUGREPORT}")
AC_SUBST(HAWK_PROJECT_URL, "${PACKAGE_URL}")

dnl === configure arguments
if test `expr " [$]0" : " '.*"` = 0; then
	hawk_configure_cmd="'[$]0'"
else
	hawk_configure_cmd="[$]0"
fi

AC_DEFINE_UNQUOTED(HAWK_CONFIGURE_ARGS, ["$ac_configure_args"], [configure arguments])
AC_DEFINE_UNQUOTED(HAWK_CONFIGURE_CMD, ["$hawk_configure_cmd"], [configure command])


AC_CONFIG_FILES([
	Makefile 
	tools/Makefile 
	lib/Makefile 
	mod/Makefile 
	bin/Makefile
	samples/Makefile
	t/Makefile
	pkgs/hawk.spec
])
AC_OUTPUT

[
echo 
echo "-[SUMMARY]---------------------------------------------------------------"
echo "Configured for ${host}"
echo "  Build mode : ${BUILD_MODE}"
echo "  Source directory: ${srcdir}"
echo "  Installation directory: ${prefix}"
echo "  C compiler: ${CC} ${CFLAGS}"
echo "  C++ compiler: ${CXX} ${CXXFLAGS}"
echo "  C++ support: ${enable_cxx_is}"
echo "  Math library: ${LIBM}"
echo "  Socket library: ${SOCKET_LIBS}"
echo "  Native function call library: ${FFI_LIBS}"
echo "  Thraed library: ${PTHREAD_LIBS}"
echo "  Memcached library: ${MEMCACHED_LIBS}"
echo "-------------------------------------------------------------------------"
]
