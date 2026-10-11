#!/bin/sh

[ $# -ge 1 ] && HAWK_BIN="$1"
[ -z "${HAWK_BIN-}" ] && HAWK_BIN="hawk"
set -u

hawk_lib_path="${LD_LIBRARY_PATH-}"
case "$HAWK_BIN" in
*/.libs/*)
	libdir=$(cd "$(dirname "$HAWK_BIN")/../../lib/.libs" 2>/dev/null && pwd)
	[ -n "${libdir-}" ] && hawk_lib_path="$libdir${hawk_lib_path:+:$hawk_lib_path}"
	;;
esac
export LD_LIBRARY_PATH="$hawk_lib_path"

# Use the writable test directory rather than assuming /tmp exists.
tmp_dir="hawk-regress-sed-$$"
if ! mkdir "$tmp_dir"
then
	echo "Bail out! unable to create test directory"
	exit 1
fi
trap 'rm -f "$tmp_dir"/*; rmdir "$tmp_dir"' 0
trap 'exit 1' 1 2 3 15

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
	[ ! -s "$tmp_dir/error" ] || sed 's/^/# /' "$tmp_dir/error"
}
check_run()
{
	desc="$1"
	expected="$2"
	shift 2
	printf '%b' "$expected" > "$tmp_dir/expected"
	if "$HAWK_BIN" --sed "$@" "$tmp_dir/input" > "$tmp_dir/output" 2> "$tmp_dir/error" &&
	   cmp "$tmp_dir/expected" "$tmp_dir/output" >/dev/null 2>&1
	then
		ok "$desc"
	else
		not_ok "$desc"
	fi
}
check_bad_option()
{
	desc="$1"
	shift
	if "$HAWK_BIN" --sed "$@" '' "$tmp_dir/input" > "$tmp_dir/output" 2> "$tmp_dir/error"
	then
		not_ok "$desc"
	elif [ -s "$tmp_dir/error" ]
	then
		ok "$desc"
	else
		not_ok "$desc"
	fi
}

echo "1..17"
printf 'hello\r\nworld\nhello' > "$tmp_dir/input"
check_run "default preserves mixed endings and missing final newline" 'hello\r\nworld\nhello' ''
check_run "explicit defaults preserve input" 'hello\r\nworld\nhello' --input-line-ending=auto --output-line-ending=preserve ''
check_run "force LF without a substitution" 'hello\nworld\nhello' --output-line-ending=lf ''
check_run "force CRLF without doubling existing CRLF" 'hello\r\nworld\r\nhello' --output-line-ending=crlf ''
check_run "space-separated option arguments" 'hello\nworld\nhello' --input-line-ending auto --output-line-ending lf ''

cr=$(printf '\r')
check_run "default hides terminator CR from substitution" 'hello\r\nworld\nhello' "s|$cr||g"
check_run "LF input exposes terminator CR to substitution" 'hello\nworld\nhello' --input-line-ending=lf "s|$cr||g"
check_run "LF input CR removal and CRLF output" 'hello\r\nworld\r\nhello' --input-line-ending=lf --output-line-ending=crlf "s|$cr||g"
check_run "LF input retains content CR without substitution" 'hello\r\r\nworld\r\nhello' --input-line-ending=lf --output-line-ending=crlf ''
check_run "CRLF-only input changes record counting" '1\nhello\r\n2\nworld\nhello' --input-line-ending=crlf '='
check_bad_option "reject invalid input mode" --input-line-ending=native
check_bad_option "reject invalid output mode" --output-line-ending=auto

case "$("$HAWK_BIN" 'BEGIN { sys::getenv("OS", os); print os; }' 2>/dev/null)" in
Windows_NT|OS2|OS/2) native='hello\r\nworld\r\nhello' ;;
*) native='hello\nworld\nhello' ;;
esac
check_run "native output policy" "$native" --output-line-ending=native ''

printf 'hello\nworld\nhello' > "$tmp_dir/expected"
if "$HAWK_BIN" --sed --output-line-ending=lf -o "$tmp_dir/named" '' "$tmp_dir/input" 2> "$tmp_dir/error" &&
   cmp "$tmp_dir/expected" "$tmp_dir/named" >/dev/null 2>&1
then
	ok "output policy applies to -o"
else
	not_ok "output policy applies to -o"
fi
if "$HAWK_BIN" --sed --output-line-ending=lf -i '' "$tmp_dir/input" 2> "$tmp_dir/error" &&
   cmp "$tmp_dir/expected" "$tmp_dir/input" >/dev/null 2>&1
then
	ok "output policy applies to in-place editing"
else
	not_ok "output policy applies to in-place editing"
fi

help=$("$HAWK_BIN" --sed --help)
case "$help" in
*--input-line-ending*'auto(default)'*) ok "help documents input default" ;;
*) not_ok "help documents input default" ;;
esac
case "$help" in
*--output-line-ending*'preserve(default)'*) ok "help documents output default" ;;
*) not_ok "help documents output default" ;;
esac
exit "$failed"
