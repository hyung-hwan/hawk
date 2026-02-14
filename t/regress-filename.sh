#!/bin/sh


[ $# -ge 1 ] && HAWK_BIN="$1"
[ -z "$HAWK_BIN" ] && HAWK_BIN="hawk"

## make referecing an unset variable an error
set -u

tmp_in="/tmp/hawk-regress-stdin-$$.txt"
tmp_f1="/tmp/hawk-regress-file-1-$$.txt"
tmp_f2="/tmp/hawk regress file 2-$$.txt"
trap 'rm -f "$tmp_in" "$tmp_f1" "$tmp_f2"' EXIT

printf 'S1\n' > "$tmp_in"
printf 'A1\nA2\n' > "$tmp_f1"
printf 'B1\n' > "$tmp_f2"

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

check_eq() {
	desc="$1"
	expected="$2"
	actual="$3"
	if [ "x$actual" = "x$expected" ]
	then
		ok "$desc"
	else
		not_ok "$desc" "$expected" "$actual"
	fi
}

## change the ending number depending on the number of test cases
echo "1..8"

if out=$(cat "$tmp_in" | "$HAWK_BIN" '{ print FNR ":" FILENAME ":" $0 }' - "$tmp_f1" "$tmp_f2" 2>&1)
then
	ok "run with stdin + 2 files"
else
	not_ok "run with stdin + 2 files" "exit code 0" "command failed: $out"
	out=""
fi

line1=$(printf '%s\n' "$out" | sed -n '1p')
line2=$(printf '%s\n' "$out" | sed -n '2p')
line3=$(printf '%s\n' "$out" | sed -n '3p')
line4=$(printf '%s\n' "$out" | sed -n '4p')
count=$(printf '%s\n' "$out" | sed -n '$=')

check_eq "stdin '-' filename" "1:-:S1" "$line1"
check_eq "first normal file, first record" "1:${tmp_f1}:A1" "$line2"
check_eq "first normal file, second record" "2:${tmp_f1}:A2" "$line3"
check_eq "space-containing filename" "1:${tmp_f2}:B1" "$line4"
check_eq "record count for stdin + 2 files" "4" "$count"

if out2=$(cat "$tmp_in" | "$HAWK_BIN" '{ print FNR ":" FILENAME ":" $0 }' - 2>&1)
then
	ok "run with stdin only"
else
	not_ok "run with stdin only" "exit code 0" "command failed: $out2"
	out2=""
fi

line_only=$(printf '%s\n' "$out2" | sed -n '1p')
check_eq "stdin-only filename" "1:-:S1" "$line_only"

exit "$failed"
