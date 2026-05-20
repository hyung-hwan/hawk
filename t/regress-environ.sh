#!/bin/sh

[ $# -ge 1 ] && HAWK_BIN="$1"
[ -z "$HAWK_BIN" ] && HAWK_BIN="hawk"

set -u

tmp_prog="/tmp/hawk-regress-environ-$$.hawk"
tmp_pipe="/tmp/hawk-regress-environ-$$.pipe"
tmp_sys="/tmp/hawk-regress-environ-$$.sys"
trap 'rm -f "$tmp_prog" "$tmp_pipe" "$tmp_sys"' EXIT

cat > "$tmp_prog" <<EOF
function emit_env(tag, cmd)
{
	cmd = "sh -c 'printf \"" tag ":%s\\n\" \"\${HAWK_TEST_ENV_PIPE-unset}\" >&3'"
	print "" | cmd
	close(cmd)
}

function emit_env_system(tag, cmd)
{
	cmd = "sh -c 'printf \"" tag ":%s\\n\" \"\${HAWK_TEST_ENV_SYS-unset}\" >> $tmp_sys'"
	return sys::system(cmd)
}

function emit_env_system_fd3(tag, cmd)
{
	cmd = "sh -c 'printf \"" tag ":%s\\n\" \"\${HAWK_TEST_ENV_SYS-unset}\" >&3'"
	return sys::system(cmd)
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
	print emit_env_system("initial");
	ENVIRON["HAWK_TEST_ENV_PIPE"] = "changed";
	ENVIRON["HAWK_TEST_ENV_SYS"] = "changed";
	emit_env("changed");
	print emit_env_system("changed");
	delete ENVIRON["HAWK_TEST_ENV_PIPE"];
	delete ENVIRON["HAWK_TEST_ENV_SYS"];
	emit_env("deleted");
	print emit_env_system("deleted");
	ENVIRON = @{};
	ENVIRON["HAWK_TEST_ENV_PIPE"] = "replaced";
	ENVIRON["HAWK_TEST_ENV_SYS"] = "replaced";
	emit_env("replaced");
	print emit_env_system("replaced");
	ENVIRON = 123;
	emit_env("scalar");
	print emit_env_system("scalar");
	print emit_env_system_fd3("fd3");
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

echo "1..28"

if out=$(env -i \
	HAWK_TEST_ENV_INT=123 \
	HAWK_TEST_ENV_NEG=-7 \
	HAWK_TEST_ENV_FLT=1.25 \
	HAWK_TEST_ENV_STR=abc123 \
	HAWK_TEST_ENV_EMPTY= \
	HAWK_TEST_ENV_PIPE=initial \
	HAWK_TEST_ENV_SYS=initial \
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
line11=$(printf '%s\n' "$out" | sed -n '11p')
line12=$(printf '%s\n' "$out" | sed -n '12p')
line13=$(printf '%s\n' "$out" | sed -n '13p')
line14=$(printf '%s\n' "$out" | sed -n '14p')
line15=$(printf '%s\n' "$out" | sed -n '15p')
line16=$(printf '%s\n' "$out" | sed -n '16p')
pipe1=$(sed -n '1p' "$tmp_pipe")
pipe2=$(sed -n '2p' "$tmp_pipe")
pipe3=$(sed -n '3p' "$tmp_pipe")
pipe4=$(sed -n '4p' "$tmp_pipe")
pipe5=$(sed -n '5p' "$tmp_pipe")
pipe6=$(sed -n '6p' "$tmp_pipe")
sys1=$(sed -n '1p' "$tmp_sys")
sys2=$(sed -n '2p' "$tmp_sys")
sys3=$(sed -n '3p' "$tmp_sys")
sys4=$(sed -n '4p' "$tmp_sys")
sys5=$(sed -n '5p' "$tmp_sys")

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
check_eq "system initial returned success" "0" "$line11"
check_eq "system changed returned success" "0" "$line12"
check_eq "system deleted returned success" "0" "$line13"
check_eq "system replaced returned success" "0" "$line14"
check_eq "system scalar returned success" "0" "$line15"
check_eq "system fd3 returned success" "0" "$line16"
check_eq "pipe sees initial imported env" "initial:initial" "$pipe1"
check_eq "pipe sees in-place env update" "changed:changed" "$pipe2"
check_eq "pipe sees deleted env entry" "deleted:unset" "$pipe3"
check_eq "pipe sees replaced environ map" "replaced:replaced" "$pipe4"
check_eq "pipe sees scalar environ as empty" "scalar:unset" "$pipe5"
check_eq "system preserves fd3 inheritance" "fd3:unset" "$pipe6"
check_eq "system sees initial imported env" "initial:initial" "$sys1"
check_eq "system sees in-place env update" "changed:changed" "$sys2"
check_eq "system sees deleted env entry" "deleted:unset" "$sys3"
check_eq "system sees replaced environ map" "replaced:replaced" "$sys4"
check_eq "system sees scalar environ as empty" "scalar:unset" "$sys5"

exit "$failed"
