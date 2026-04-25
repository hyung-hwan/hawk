#!/bin/sh

if [ "${VALGRIND:-0}" = "1" ]
then
	vglog="/tmp/hawk-valgrind-$$.log"
	vgerr="${VALGRIND_ERROR_EXITCODE:-99}"
	trap 'rm -f "$vglog"' EXIT

	${VALGRIND_CMD:-valgrind} ${VALGRIND_FLAGS:-} --log-file="$vglog" "${HAWK_TEST_COMPILER:-../bin/hawk}" "$@"
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
	exec "${HAWK_TEST_COMPILER:-../bin/hawk}" "$@"
fi
