#!/bin/sh

hawk_bin="${HAWK_TEST_COMPILER:-../bin/hawk}"
hawk_lib_path="${LD_LIBRARY_PATH-}"

case "$hawk_bin" in
*/.libs/*)
	libdir=$(cd "$(dirname "$hawk_bin")/../../lib/.libs" 2>/dev/null && pwd)
	if [ -n "${libdir-}" ]
	then
		if [ -n "$hawk_lib_path" ]
		then
			hawk_lib_path="$libdir:$hawk_lib_path"
		else
			hawk_lib_path="$libdir"
		fi
	fi
	;;
esac

export LD_LIBRARY_PATH="$hawk_lib_path"

if [ "${VALGRIND:-0}" = "1" ]
then
	vglog="/tmp/hawk-valgrind-$$.log"
	vgerr="${VALGRIND_ERROR_EXITCODE:-99}"
	trap 'rm -f "$vglog"' EXIT

	${VALGRIND_CMD:-valgrind} ${VALGRIND_FLAGS:-} --log-file="$vglog" "$hawk_bin" "$@"
	status=$?

	if [ -f "$vglog" ]
	then
		awk '
			/^==[0-9]+== HEAP SUMMARY:/ { show = 1 }
			show { print }
			/^==[0-9]+== LEAK SUMMARY:/ { leak = 1; next }
			leak && /^==[0-9]+==[[:space:]]*$/ { exit }
		' "$vglog" >&2

		if [ "$status" -eq 0 ]
		then
			echo "[VALGRIND] LEAK-FREE" >&2
		elif [ "$status" -eq "$vgerr" ]
		then
			echo "[VALGRIND] LEAK DETECTED OR MEMORY ERROR" >&2
		else
			echo "[VALGRIND] TEST FAILED (NO VALGRIND LEAK EXIT)" >&2
		fi
	fi

	exit "$status"
else
	exec "$hawk_bin" "$@"
fi
