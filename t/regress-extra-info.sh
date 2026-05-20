#!/bin/sh

[ $# -ge 1 ] && HAWK_BIN="$1"
[ -z "$HAWK_BIN" ] && HAWK_BIN="hawk"

set -u

hawk_lib_path="${LD_LIBRARY_PATH-}"
case "$HAWK_BIN" in
*/.libs/*)
	libdir=$(cd "$(dirname "$HAWK_BIN")/../../lib/.libs" 2>/dev/null && pwd)
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

test_no=0
failed=0

ok() {
	test_no=$((test_no + 1))
	echo "ok $test_no - $1"
}

not_ok() {
	test_no=$((test_no + 1))
	failed=1
	echo "not ok $test_no - $1"
	echo "# expected: $2"
	echo "# actual: $3"
}

check_has_line() {
	desc="$1"
	needle="$2"
	haystack="$3"

	if printf '%s\n' "$haystack" | grep -F -x "$needle" >/dev/null 2>&1
	then
		ok "$desc"
	else
		not_ok "$desc" "$needle" "$haystack"
	fi
}

echo "1..7"

if out=$("$HAWK_BIN" -D -vqqq=20 'BEGIN { aa = 10; bb = "hello"; zz = qqq; }' 2>&1 >/dev/null)
then
	ok "run with -D"
else
	not_ok "run with -D" "exit code 0" "command failed: $out"
	out=""
fi

check_has_line "shows nil return" "[RETURN] - ***nil***" "$out"
check_has_line "shows named variable section start" "[NAMED VARIABLES]" "$out"
check_has_line "shows parser-known named variables" "aa = 10" "$out"
check_has_line "shows parser-known string variables" "bb = hello" "$out"
check_has_line "shows values originating from -v assignments" "zz = 20" "$out"
check_has_line "shows named variable section end" "[END OF NAMED VARIABLES]" "$out"

exit "$failed"
