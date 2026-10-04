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

tmp_base="${TMPDIR-/tmp}/hawk-regress-m4-$$"
tmp_main="$tmp_base-main.m4"
tmp_inc="$tmp_base-inc.m4"
tmp_first="$tmp_base-first.m4"
tmp_second="$tmp_base-second.m4"
tmp_idir1="$tmp_base-id1"
tmp_idir2="$tmp_base-id2"
tmp_iname1="hawk-regress-m4-$$-shared.m4"
tmp_iname2="hawk-regress-m4-$$-only.m4"
trap 'rm -f "$tmp_main" "$tmp_inc" "$tmp_first" "$tmp_second" "$tmp_idir1/$tmp_iname1" "$tmp_idir1/$tmp_iname2" "$tmp_idir2/$tmp_iname1" "$tmp_idir2/$tmp_iname2"; rmdir "$tmp_idir1" "$tmp_idir2" 2>/dev/null || :' EXIT

test_no=0
failed=0
ok() { test_no=$((test_no + 1)); echo "ok $test_no - $1"; }
not_ok() { test_no=$((test_no + 1)); failed=1; echo "not ok $test_no - $1"; echo "# expected: $2"; echo "# actual: $3"; }
check_eq() { if [ "x$2" = "x$3" ]; then ok "$1"; else not_ok "$1" "$2" "$3"; fi; }

echo "1..40"

printf "define(\`twice', \`\$1\$1')dnl\ntwice(\`ab')|eval(\`2 + 3 * 4')|ifelse(\`x', \`x', \`yes', \`no')|translit(\`abc', \`ac', \`XY')|substr(\`abcdef', \`2', \`3')|index(\`abcdef', \`cd')\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "macros and string/arithmetic builtins" "abab|14|yes|XbY|cde|2" "$out"

printf "define(\`saved', \`PERSISTED')dnl\n" > "$tmp_first"
printf "saved\n" > "$tmp_second"
out=$("$HAWK_BIN" --m4 "$tmp_first" "$tmp_second")
check_eq "definitions persist across input files" "PERSISTED" "$out"

printf "INCLUDED\n" > "$tmp_inc"
{ printf 'include(`'; printf '%s' "$tmp_inc"; printf "')dnl\nafter\n"; } > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
expected=$(printf 'INCLUDED\nafter')
check_eq "include uses the standard input callback" "$expected" "$out"

printf "sinclude(\`%s-does-not-exist')dnl\nok\n" "$tmp_base" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "sinclude ignores an unavailable file" "ok" "$out"

printf "divert(\`1')ONE\ndivert(\`0')ZERO\nundivert(\`1')dnl\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
expected=$(printf 'ZERO\nONE')
check_eq "diversions retain output ordering" "$expected" "$out"

printf "errprint(\`diagnostic')dnl\n" > "$tmp_main"
err=$("$HAWK_BIN" --m4 "$tmp_main" 2>&1 >/dev/null)
check_eq "errprint uses the diagnostic callback" "diagnostic" "$err"

out=$(printf "define(\`x', \`stdin')dnl\nx\n" | "$HAWK_BIN" --m4)
check_eq "standard input processing" "stdin" "$out"

printf "include(\`%s-does-not-exist')\n" "$tmp_base" > "$tmp_main"
if "$HAWK_BIN" --m4 "$tmp_main" >/dev/null 2>&1
then
	not_ok "include failure is reported" "non-zero exit" "zero exit"
else
	ok "include failure is reported"
fi

printf "define(\`args', \`<\$1>|<\$2>|<\$3>')dnl\nargs(,b,)\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "empty macro arguments are retained by position" "<>|<b>|<>" "$out"

printf "define(\`args', \`<\$1>|<\$2>')dnl\nargs((a,b),c)\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "commas in nested parentheses stay in an argument" "<(a,b)>|<c>" "$out"

printf "define(\`args', \`<\$1>|<\$32>')dnl\nargs(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34)|done\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "excess macro arguments are consumed safely" "<1>|<32>|done" "$out"

printf "define(\`args', \`<\$9>|<\$10>|<\$11>')dnl\nargs(1,2,3,4,5,6,7,8,9,10,11)\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "multi-digit macro references select the full index" "<9>|<10>|<11>" "$out"

printf "define(\`args', \`<\$32>')dnl\nargs(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31)\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "a missing in-range macro argument expands to empty" "<>" "$out"

printf "define(\`args', \`<\$33>')dnl\nargs(1)\n" > "$tmp_main"
if err=$("$HAWK_BIN" --m4 "$tmp_main" 2>&1 >/dev/null)
then
	not_ok "an out-of-range macro reference is rejected" "non-zero exit" "zero exit"
else
	case "$err" in
	*"invalid reference index \$33"*) ok "an out-of-range macro reference is rejected" ;;
	*) not_ok "an out-of-range macro reference is rejected" "invalid reference index \$33" "$err" ;;
	esac
fi

printf "define(\`args', \`<\$1>')dnl\nargs(a" > "$tmp_main"
if "$HAWK_BIN" --m4 "$tmp_main" >/dev/null 2>&1
then
	not_ok "unterminated macro arguments are rejected" "non-zero exit" "zero exit"
else
	ok "unterminated macro arguments are rejected"
fi

printf "changequote(\`[[', \`]]')dnl\n[[a[[b]]c]]\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "multi-character quotes nest" "a[[b]]c" "$out"

printf "changequote(\`[[', \`]]')dnl\n[define([[x]],[[Y]])x\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "failed quote prefix does not hide a macro" "[Y" "$out"

printf "changequote(\`[[', \`]]')dnl\ndefine([[f]],[[<\$1>|<\$2>]])dnl\nf([,b)\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "failed quote prefix does not hide an argument comma" "<[>|<b>" "$out"

printf "changequote(\`[[', \`]]')dnl\n[" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "partial quote prefix at EOF stays literal" "[" "$out"

printf "changequote(\`[[', \`[]')dnl\n[[a[[b[]c[]\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "opening and ending quotes may share a prefix" "a[[b[]c" "$out"

printf "changequote(\`<<>', \`]')dnl\n<<<>hello]\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "overlapping quote prefixes are reconsidered" "<hello" "$out"

printf "changequote(\`<B', \`>E')dnl\n<Bfoo>Edefine(<Bx>E,<BY>E)x\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "ending quote establishes a token boundary" "fooY" "$out"

printf "changequote(\`abcdefghijklmnopqrstuvwxyzABCDEFG', \`]')\n" > "$tmp_main"
if err=$("$HAWK_BIN" --m4 "$tmp_main" 2>&1 >/dev/null)
then
	not_ok "oversized quotes are rejected with detail" "non-zero exit" "zero exit"
else
	case "$err" in
	*"beginning quote too long"*) ok "oversized quotes are rejected with detail" ;;
	*) not_ok "oversized quotes are rejected with detail" "beginning quote too long" "$err" ;;
	esac
fi

printf "changequote(\`dnl garbageX', \`!')dnl\ndnl garbageY\nok\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "replayed quote lookahead honors dnl" "ok" "$out"

printf "define(\`x',\`one')pushdef(\`x',\`two')x|popdef(\`x')x|popdef(\`x')ifdef(\`x',\`yes',\`no')\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "pushdef stacks and popdef restores definitions" "two|one|no" "$out"

printf "pushdef(\`len',\`shadow')len|popdef(\`len')len(\`abc')\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "popdef restores a shadowed builtin" "shadow|3" "$out"

printf "define(\`a',\`A')define(\`b',\`B')pushdef(\`a',\`AA')pushdef(\`b',\`BB')popdef(\`a',\`b')a|b\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "popdef accepts multiple macro names" "A|B" "$out"

printf "define(\`x',\`one')pushdef(\`x',\`two')define(\`x',\`three')x|popdef(\`x')x\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "define replaces only the top definition" "three|one" "$out"

printf "define(\`x',\`one')pushdef(\`x',\`two')undefine(\`x')ifdef(\`x',\`yes',\`no')|popdef(\`missing')OK\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "undefine removes the stack and missing popdef is harmless" "no|OK" "$out"

printf "#include <stdio.h>\ninclude without-parentheses\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
expected=$(printf '#include <stdio.h>\ninclude without-parentheses')
check_eq "blind builtins stay literal without parentheses" "$expected" "$out"

printf "define(\`include',\`replacement')include\n" > "$tmp_main"
out=$("$HAWK_BIN" --m4 "$tmp_main")
check_eq "a user macro replacing a blind builtin expands without parentheses" "replacement" "$out"

printf "eval(\`1 @ 2')\n" > "$tmp_main"
if err=$("$HAWK_BIN" --m4 "$tmp_main" 2>&1 >/dev/null)
then
	not_ok "invalid expression operators are diagnosed" "non-zero exit" "zero exit"
else
	case "$err" in
	*"invalid operator in expression - @"*) ok "invalid expression operators are diagnosed" ;;
	*) not_ok "invalid expression operators are diagnosed" "invalid operator in expression - @" "$err" ;;
	esac
fi

printf "eval(\`1)trailing')\n" > "$tmp_main"
if err=$("$HAWK_BIN" --m4 "$tmp_main" 2>&1 >/dev/null)
then
	not_ok "trailing expression data is diagnosed" "non-zero exit" "zero exit"
else
	case "$err" in
	*"invalid trailing data in expression - trailing"*) ok "trailing expression data is diagnosed" ;;
	*) not_ok "trailing expression data is diagnosed" "invalid trailing data in expression - trailing" "$err" ;;
	esac
fi

printf "include()\n" > "$tmp_main"
if err=$("$HAWK_BIN" --m4 "$tmp_main" 2>&1 >/dev/null)
then
	not_ok "a missing include name is diagnosed" "non-zero exit" "zero exit"
else
	case "$err" in
	*"file name required for include"*) ok "a missing include name is diagnosed" ;;
	*) not_ok "a missing include name is diagnosed" "file name required for include" "$err" ;;
	esac
fi

out=$(printf "OPT\n" | "$HAWK_BIN" --m4 -DOPT=first -DOPT=second)
check_eq "multiple -D options are applied in order" "second" "$out"

out1=$(printf "OPT\n" | "$HAWK_BIN" --m4 -DOPT=value -UOPT)
out2=$(printf "OPT\n" | "$HAWK_BIN" --m4 -UOPT -DOPT=value)
check_eq "mixed -D and -U options retain their order" "OPT|value" "$out1|$out2"

out=$(printf "A|B\n" | "$HAWK_BIN" --m4 --define=A=one --define=B=two --undefine=A --undefine=B)
check_eq "multiple long-form -U options are accepted" "A|B" "$out"

out=$(printf "<EMPTY>\n" | "$HAWK_BIN" --m4 -DEMPTY)
check_eq "-D without a value defines an empty macro" "<>" "$out"

mkdir "$tmp_idir1" "$tmp_idir2"
printf "FIRST\n" > "$tmp_idir1/$tmp_iname1"
printf "SECOND\n" > "$tmp_idir2/$tmp_iname1"
printf "ONLY\n" > "$tmp_idir2/$tmp_iname2"
out=$("$HAWK_BIN" --m4 -I "$tmp_idir1" --incdirs "$tmp_idir2" "$tmp_iname1" "$tmp_iname2")
expected=$(printf 'FIRST\nONLY')
check_eq "multiple include directories locate top-level inputs in order" "$expected" "$out"

printf "include(\`%s')include(\`%s')" "$tmp_iname1" "$tmp_iname2" > "$tmp_main"
out=$("$HAWK_BIN" --m4 -I "$tmp_idir1" --incdirs "$tmp_idir2" "$tmp_main")
expected=$(printf 'FIRST\nONLY')
check_eq "multiple include directories locate include builtin inputs" "$expected" "$out"

exit "$failed"
