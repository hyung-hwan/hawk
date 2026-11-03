#!/bin/sh

[ $# -lt 1 ] || HAWK_REX_COMPILER="$1"
HAWK_REX_COMPILER="${HAWK_REX_COMPILER:-${HAWK_BIN:-hawk}}"
set -u

# Diagnostics are part of these checks; keep them independent of the locale.
LANG=C
LC_ALL=C
export LANG LC_ALL

hawk_lib_path="${LD_LIBRARY_PATH-}"
case "$HAWK_REX_COMPILER" in
*/.libs/*)
	libdir=$(cd "$(dirname "$HAWK_REX_COMPILER")/../../lib/.libs" 2>/dev/null && pwd)
	[ -z "${libdir-}" ] || hawk_lib_path="$libdir${hawk_lib_path:+:$hawk_lib_path}"
	;;
esac
export LD_LIBRARY_PATH="$hawk_lib_path"

test_no=0
failed=0
ok()
{
	test_no=$((test_no + 1))
	echo "ok $test_no - $1"
}
not_ok()
{
	test_no=$((test_no + 1))
	failed=1
	echo "not ok $test_no - $1"
	printf '# status: %s\n' "$status"
	printf '# output: %s\n' "$output"
}
run()
{
	if output=$("$HAWK_REX_COMPILER" "$@" 2>&1)
	then
		status=0
	else
		status=$?
	fi
	# The fixtures contain no CR data; tolerate native Windows CRLF output.
	output=$(printf '%s' "$output" | tr -d '\015')
}
check_run()
{
	desc="$1"
	expected="$2"
	shift 2
	run "$@"
	if [ "$status" -eq 0 ] && [ "$output" = "$expected" ]
	then
		ok "$desc"
	else
		not_ok "$desc"
	fi
}
check_reject()
{
	desc="$1"
	needle="$2"
	shift 2
	run "$@"
	case "$output" in
	*"$needle"*)
		if [ "$status" -ne 0 ]
		then
			ok "$desc"
			return
		fi
		;;
	esac
	not_ok "$desc"
}

echo '1..21'
script='BEGIN {
	print "abc" ~ /a(b|c)/;
	print match("xxabyy", "(a)(b)"), RSTART, RLENGTH;
	x = "ab ab"; print gsub(/ab/, "X", x), x;
	print "[" ~ /^[\[.]/;
	print @b"abc" ~ /a(b|c)/;
	IGNORECASE = 1; print "AB" ~ /ab/;
}'
expected='1
3 3 2
2 X X
1
1
1'
check_run 'default engine handles literals, dynamic patterns, bytes and ignorecase' "$expected" "$script"
check_run 'explicit TRE engine' "$expected" --rex=tre "$script"
check_run 'explicit RGX engine' "$expected" --rex=rgx "$script"
check_run 'space-separated TRE option' "$expected" --rex tre "$script"
check_run 'space-separated RGX option' "$expected" --rex rgx "$script"
check_run 'case-insensitive TRE name' "$expected" --rex=TRE "$script"
check_run 'case-insensitive RGX name' "$expected" --rex=RGX "$script"

# TRE accepts this extension; RGX rejects an interval without a lower bound.
# Exercise both source compilation and runtime compilation to detect a selector
# that is accepted by the CLI but never installed, or installed too late.
literal='BEGIN { print "aaa" ~ /a{,2}/; }'
dynamic='BEGIN { print match("aaa", "a{,2}"); }'
check_run 'default remains TRE' '1' "$literal"
check_run 'TRE is selected before literal compilation' '1' --rex=tre "$literal"
check_reject 'RGX is selected before literal compilation' 'invalid content inside braces' --rex=rgx "$literal"
check_run 'TRE handles runtime-built patterns' '1' --rex=tre "$dynamic"
check_reject 'RGX handles runtime-built patterns' 'invalid content inside braces' --rex=rgx "$dynamic"
check_run 'last engine selection can restore TRE' '1' --rex=rgx --rex=tre "$literal"
check_reject 'last engine selection can enable RGX' 'invalid content inside braces' --rex=tre --rex=rgx "$literal"

check_reject 'reject unavailable engine before running the program' 'unknown regular expression engine' --rex=pcre2 'BEGIN { print "must not run"; }'
check_reject 'reject an empty engine name' 'unknown regular expression engine' --rex= 'BEGIN { print "must not run"; }'
check_reject 'reject a missing engine argument' "bad argument for 'rex'" --rex

run --help
case "$output" in
*--rex*tre/rgx*'default: tre'*)
	if [ "$status" -eq 0 ]
	then
		ok 'help lists engines and the default'
	else
		not_ok 'help lists engines and the default'
	fi
	;;
*) not_ok 'help lists engines and the default' ;;
esac

check_run 'TRE respects disabled bounded repetition' '1' --rex=tre --rexbound=off 'BEGIN { print "a{2}" ~ /a{2}/; }'
check_run 'RGX respects disabled bounded repetition' '1' --rex=rgx --rexbound=off 'BEGIN { print "a{2}" ~ /a{2}/; }'
check_run 'explicit Hawk mode forwards the engine option' '1' --hawk --rex=rgx 'BEGIN { print "ab" ~ /ab/; }'
exit "$failed"
