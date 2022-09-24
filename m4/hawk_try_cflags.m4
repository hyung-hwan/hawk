AC_DEFUN([HAWK_TRY_CFLAGS], [
	AS_VAR_PUSHDEF([ac_var], ac_cv_cflags_[[$1]])
	AC_CACHE_CHECK([whether the compiler supports $1], ac_var, [
		ac_hawk_try_cflags_saved=$CFLAGS
		CFLAGS="$CFLAGS $1"
		AC_LINK_IFELSE([AC_LANG_SOURCE([[int main(int argc, char **argv) { return 0; }]])], [AS_VAR_SET(ac_var,yes)], [AS_VAR_SET(ac_var,no)])
		CFLAGS=$ac_hawk_try_cflags_saved])
	AS_IF([test AS_VAR_GET(ac_var) = yes], [m4_default([$2], [EXTRACFLAGS="$EXTRACFLAGS $1"])], [$3])
	AS_VAR_POPDEF([ac_var])
])
