#
# This is a blindly modified variant of AC_CHECK_SIZEOF.
#

# AX_CHECK_NUMVALOF(TYPE, DEFAULT, [INCLUDES = DEFAULT-INCLUDES])
# ---------------------------------------------------------------
AC_DEFUN([AX_CHECK_NUMVALOF],
	[AS_LITERAL_IF(m4_translit([[$1]], [[$2]], [p]), [],
            [m4_fatal([$0: requires literal arguments])])]

	[
		_AC_CACHE_CHECK_INT(
			[numeric value of $1],
			[AS_TR_SH([ax_cv_numvalof_$1])],
		     [($1)],
			[AC_INCLUDES_DEFAULT([$3])],
			[AS_TR_SH([ax_cv_numvalof_$1])=$2])

		AC_DEFINE_UNQUOTED(
			AS_TR_CPP(numvalof_$1),
			$AS_TR_SH([ax_cv_numvalof_$1]),
			[The size of `$1', as computed by valueof.])
	]
)# AX_CHECK_NUMVALOF

