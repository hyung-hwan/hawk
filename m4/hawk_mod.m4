# this file is not supposed to be used standalone
# as it repies on --enable-static-module option value.
#
# HAWK_MOD(name,default-value,auto-decision-action)

AC_DEFUN([HAWK_MOD],
[
	AC_ARG_ENABLE([mod-$1],
		[AS_HELP_STRING([--enable-mod-$1],[enable mod/$1. one of auto, auto:static, yes, yes:static, no (default. $2)])],
		enable_mod_$1_is=$enableval,
		enable_mod_$1_is=$2
	)       
	if test "x${enable_mod_$1_is}" = "xauto" -o "x${enable_mod_$1_is}" = "xauto:static"
	then
		$3

		if test $? -eq 0
		then
			enable_mod_$1_is=`echo "[$]enable_mod_$1_is" | sed 's|^auto|yes|g'`
		else
			enable_mod_$1_is=no
		fi
	fi

	if test "x${enable_mod_$1_is}" = "xyes:static"
	then
		enable_mod_$1_is=yes
		if test "x${enable_static_module_is}" = "xyes"
		then
			enable_mod_$1_static_is="yes"
		fi
	fi

	m4_pushdef([UPNAME], m4_translit([$1], [abcdefghijklmnopqrstuvwxyz], [ABCDEFGHIJKLMNOPQRSTUVWXYZ]))

	if test "x${enable_mod_$1_is}" = "xyes"
	then
		AC_DEFINE([HAWK_ENABLE_MOD_[]UPNAME],[1],[build mod/$1])
	fi
	if test "x${enable_mod_$1_static_is}" = "xyes"
	then
		AC_DEFINE([HAWK_ENABLE_MOD_[]UPNAME[]_STATIC],[1],[build mod/$1 statically linked into the main library])
	fi
	AM_CONDITIONAL(ENABLE_MOD_[]UPNAME, test "${enable_mod_$1_is}" = "yes")
	AM_CONDITIONAL(ENABLE_MOD_[]UPNAME[]_STATIC, test "${enable_mod_$1_static_is}" = "yes")

	m4_popdef([UPNAME])
])
