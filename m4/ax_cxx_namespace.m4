dnl @synopsis AX_CXX_NAMESPACE_STD
dnl
dnl If the compiler supports namespace std, define
dnl HAVE_CXX_NAMESPACE_STD.
dnl
dnl @category Cxx
dnl @author Todd Veldhuizen
dnl @author Luc Maisonobe <luc@spaceroots.org>
dnl @version 2004-02-04
dnl @license AllPermissive

AC_DEFUN([AX_CXX_NAMESPACE_STD], [
  AC_CACHE_CHECK(if c++ supports namespace std,
  ax_cv_cxx_have_std_namespace,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([#include <iostream>
		  std::istream& is = std::cin;],,
  ax_cv_cxx_have_std_namespace=yes, ax_cv_cxx_have_std_namespace=no)
  AC_LANG_RESTORE
  ])
  if test "$ax_cv_cxx_have_std_namespace" = yes; then
    AC_DEFINE(HAVE_CXX_NAMESPACE_STD,,[Define if c++ supports namespace std. ])
  fi
])
