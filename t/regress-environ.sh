#!/bin/sh

[ $# -ge 1 ] && HAWK_BIN="$1"
[ -z "$HAWK_BIN" ] && HAWK_BIN="hawk"

set -u

tmp_prog="/tmp/hawk-regress-environ-$$.hawk"
tmp_pipe="/tmp/hawk-regress-environ-$$.pipe"
trap 'rm -f "$tmp_prog" "$tmp_pipe"' EXIT

cat > "$tmp_prog" <<'EOF'
function emit_env(tag, cmd)
{
	cmd = "sh -c 'printf \"" tag ":%s\\n\" \"${HAWK_TEST_ENV_PIPE-unset}\" >&3'"
	print "" | cmd
	close(cmd)
}

BEGIN {
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_INT"]) ":" ENVIRON["HAWK_TEST_ENV_INT"];
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_NEG"]) ":" ENVIRON["HAWK_TEST_ENV_NEG"];
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_FLT"]) ":" sprintf("%.2f", ENVIRON["HAWK_TEST_ENV_FLT"]);
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_STR"]) ":" ENVIRON["HAWK_TEST_ENV_STR"];
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_EMPTY"]) ":" length(ENVIRON["HAWK_TEST_ENV_EMPTY"]);
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_MISSING"]) ":" hawk::isnil(ENVIRON["HAWK_TEST_ENV_MISSING"]);

	ENVIRON["HAWK_TEST_ENV_NEW"] = 456;
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_NEW"]) ":" ENVIRON["HAWK_TEST_ENV_NEW"];

	ENVIRON["HAWK_TEST_ENV_NEW"] = 7.5;
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_NEW"]) ":" sprintf("%.1f", ENVIRON["HAWK_TEST_ENV_NEW"]);

	ENVIRON["HAWK_TEST_ENV_NEW"] = "xyz";
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_NEW"]) ":" ENVIRON["HAWK_TEST_ENV_NEW"];

	delete ENVIRON["HAWK_TEST_ENV_STR"];
	print hawk::typename(ENVIRON["HAWK_TEST_ENV_STR"]) ":" hawk::isnil(ENVIRON["HAWK_TEST_ENV_STR"]);

	emit_env("initial");
	ENVIRON["HAWK_TEST_ENV_PIPE"] = "changed";
	emit_env("changed");
	delete ENVIRON["HAWK_TEST_ENV_PIPE"];
	emit_env("deleted");
	ENVIRON = @{};
	ENVIRON["HAWK_TEST_ENV_PIPE"] = "replaced";
	emit_env("replaced");
	ENVIRON = 123;
	emit_env("scalar");
}
EOF

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

echo "1..16"

if out=$(env -i \
	HAWK_TEST_ENV_INT=123 \
	HAWK_TEST_ENV_NEG=-7 \
	HAWK_TEST_ENV_FLT=1.25 \
	HAWK_TEST_ENV_STR=abc123 \
	HAWK_TEST_ENV_EMPTY= \
	HAWK_TEST_ENV_PIPE=initial \
	"$HAWK_BIN" -f "$tmp_prog" 3>"$tmp_pipe" 2>&1)
then
	ok "run environ regression"
else
	not_ok "run environ regression" "exit code 0" "command failed: $out"
	out=""
fi

line1=$(printf '%s\n' "$out" | sed -n '1p')
line2=$(printf '%s\n' "$out" | sed -n '2p')
line3=$(printf '%s\n' "$out" | sed -n '3p')
line4=$(printf '%s\n' "$out" | sed -n '4p')
line5=$(printf '%s\n' "$out" | sed -n '5p')
line6=$(printf '%s\n' "$out" | sed -n '6p')
line7=$(printf '%s\n' "$out" | sed -n '7p')
line8=$(printf '%s\n' "$out" | sed -n '8p')
line9=$(printf '%s\n' "$out" | sed -n '9p')
line10=$(printf '%s\n' "$out" | sed -n '10p')
pipe1=$(sed -n '1p' "$tmp_pipe")
pipe2=$(sed -n '2p' "$tmp_pipe")
pipe3=$(sed -n '3p' "$tmp_pipe")
pipe4=$(sed -n '4p' "$tmp_pipe")
pipe5=$(sed -n '5p' "$tmp_pipe")

check_eq "int env imported as int" "int:123" "$line1"
check_eq "negative int env imported as int" "int:-7" "$line2"
check_eq "float env imported as flt" "flt:1.25" "$line3"
check_eq "string env imported as str" "str:abc123" "$line4"
check_eq "empty env imported as empty string" "str:0" "$line5"
check_eq "missing env yields nil" "nil:1" "$line6"
check_eq "assigned int remains int" "int:456" "$line7"
check_eq "assigned float remains flt" "flt:7.5" "$line8"
check_eq "assigned string remains str" "str:xyz" "$line9"
check_eq "deleted env entry becomes nil" "nil:1" "$line10"
check_eq "pipe sees initial imported env" "initial:initial" "$pipe1"
check_eq "pipe sees in-place env update" "changed:changed" "$pipe2"
check_eq "pipe sees deleted env entry" "deleted:unset" "$pipe3"
check_eq "pipe sees replaced environ map" "replaced:replaced" "$pipe4"
check_eq "pipe sees scalar environ as empty" "scalar:unset" "$pipe5"

exit "$failed"
