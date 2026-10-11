#!/bin/sh
[ $# -ge 1 ] && HAWK_BIN="$1"
: "${HAWK_BIN:=hawk}"
set -u
if ! "$HAWK_BIN" --sh -c : >/dev/null 2>&1; then
	echo '1..0 # SKIP POSIX shell backend unavailable'
	exit 0
fi
tmp=$(mktemp -d "${TMPDIR:-/tmp}/hawk-sh-XXXXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
number=0
failed=0
check() {
	number=$((number + 1))
	if [ "x$2" = "x$3" ]; then echo "ok $number - $1";
	else echo "not ok $number - $1"; echo "# expected: $2"; echo "# got: $3"; failed=1; fi
}
check_status() {
	"$HAWK_BIN" --sh -c "$3" >"$tmp/out" 2>"$tmp/err"
	check "$1" "$2" "$?"
}
echo '1..56'
check 'PATH lookup and pipelines' 'HELLO' "$("$HAWK_BIN" --sh -c 'echo hello | tr a-z A-Z | cat')"
check 'quotes, concatenation and empty args' '<ab cd><><a b>' "$("$HAWK_BIN" --sh -c 'printf "<%s>" ab" cd" "" a\ b')"
check 'escaped quote and literal backslash' 'a"b\q' "$("$HAWK_BIN" --sh -c 'echo "a\"b\q"')"
check 'comments at token boundaries' 'a#b' "$("$HAWK_BIN" --sh -c 'echo a#b # ignored')"
check 'backslash newline continuation' 'foobar' "$(printf 'echo foo\\\nbar\n' | "$HAWK_BIN" --sh)"
check 'output/input redirection and append' "$(printf 'one\ntwo')" "$("$HAWK_BIN" --sh -c "echo one > '$tmp/file'; echo two >> '$tmp/file'; cat < '$tmp/file'")"
check 'redirection before command' 'before' "$("$HAWK_BIN" --sh -c "> '$tmp/file' echo before; cat '$tmp/file'")"
check 'redirection-only command' '0' "$("$HAWK_BIN" --sh -c "> '$tmp/file'; wc -c < '$tmp/file'" | tr -d ' ')"
check 'builtin descriptor duplication' 'hello' "$("$HAWK_BIN" --sh -c 'echo hello >&3' 3>&1)"
check 'external descriptor duplication' 'hello' "$("$HAWK_BIN" --sh -c 'printf hello >&3' 3>&1)"
check 'descriptor order retains stdout for stderr' 'diagnostic' "$("$HAWK_BIN" --sh -c "sh -c 'echo diagnostic >&2; echo output' 2>&1 > '$tmp/file'")"
check 'redirected stdout contents' 'output' "$(cat "$tmp/file")"
check 'opposite descriptor order combines streams' "$(printf 'diagnostic\noutput')" "$("$HAWK_BIN" --sh -c "sh -c 'echo diagnostic >&2; echo output' > '$tmp/file' 2>&1; cat '$tmp/file'")"
check 'input duplication defaults to fd zero' 'input' "$(printf input | "$HAWK_BIN" --sh -c 'cat <&3' 3<&0)"
check 'export/expansion at execution time' 'a b|$X|a b' "$("$HAWK_BIN" --sh -c 'export X="a b"; printf "%s|%s|%s" "$X" '\''$X'\'' "${X}"')"
check 'unset hides variables' '<>' "$("$HAWK_BIN" --sh -c 'export X=hello; unset X; printf "<%s>" "$X"')"
check 'command-local assignment reaches external process' 'value' "$("$HAWK_BIN" --sh -c "X=value sh -c 'printf %s \"\$X\"'")"
check 'assignment-only command persists' 'value' "$("$HAWK_BIN" --sh -c 'X=value; echo "$X"')"
check 'external assignment does not persist' '<>' "$("$HAWK_BIN" --sh -c 'unset X; X=value /bin/true; printf "<%s>" "$X"')"
check 'status and logical lists' "$(printf '1\nok\nend')" "$("$HAWK_BIN" --sh -c 'false; echo "$?"; false && echo wrong; false || echo ok; true && echo end')"
check_status 'exit stops remaining list' 7 'exit 7; echo wrong'
check_status 'pipeline status from last stage' 1 'true | false'
check_status 'unknown command status' 127 'hawk-command-that-does-not-exist'
check_status 'redirection failure is command failure' 1 "echo bad > '$tmp/missing/file'"
check_status 'missing redirect operand rejected' 2 'echo >'
check_status 'unterminated quote rejected' 2 'echo "'
check_status 'trailing pipeline rejected' 2 'echo hi |'
check 'mode switching and registered binary' "$(printf 'registered\nabc\npath')" "$("$HAWK_BIN" --sh -c 'addcmd copy /bin/cat; mode registered; mode; echo abc | copy; mode path; mode')"
check_status 'unregistered absolute path rejected' 127 'mode registered; /bin/cat'
check_status 'removed registration rejected' 127 'addcmd copy /bin/cat; delcmd copy; mode registered; copy'
check 'unquoted heredoc expansion' 'expanded' "$(printf 'export X=expanded\ncat <<EOF\n$X\nEOF\n' | "$HAWK_BIN" --sh)"
check 'quoted heredoc preserves text' '$HOME' "$(printf "cat <<'EOF'\n\$HOME\nEOF\n" | "$HAWK_BIN" --sh)"
check 'tab stripping heredoc and pipeline' 'HELLO' "$(printf 'cat <<-EOF | tr a-z A-Z\n\thello\n\tEOF\n' | "$HAWK_BIN" --sh)"
check 'multiple heredocs, last stdin wins' 'second' "$(printf 'cat <<A <<B\nfirst\nA\nsecond\nB\n' | "$HAWK_BIN" --sh)"
check_status 'unterminated heredoc rejected' 2 "$(printf 'cat <<EOF\nunfinished')"
check 'heredoc honors shell-local TMPDIR' 'local' "$(printf "export TMPDIR='%s'; export TEMP=/nonexistent/hawk-temp; cat <<EOF\nlocal\nEOF\n" "$tmp" | TMPDIR=/nonexistent/hawk-tmpdir "$HAWK_BIN" --sh)"
check 'heredoc unset TMPDIR falls back to shell-local TEMP' 'temp' "$(printf "unset TMPDIR; export TEMP='%s'; cat <<EOF\ntemp\nEOF\n" "$tmp" | TMPDIR=/nonexistent/hawk-tmpdir "$HAWK_BIN" --sh)"
check 'heredoc empty TMPDIR and TEMP fall back to shell-local TMP' 'tmp' "$(printf "export TMPDIR= TEMP= TMP='%s'; cat <<EOF\ntmp\nEOF\n" "$tmp" | TMPDIR=/nonexistent/hawk-tmpdir "$HAWK_BIN" --sh)"
check 'large pipeline without deadlock' '200000' "$("$HAWK_BIN" --sh -c 'head -c 200000 /dev/zero | cat | wc -c' | tr -d ' ')"
check 'background wait and status' '1' "$("$HAWK_BIN" --sh -c 'sh -c "exit 1" & wait %1; echo "$?"')"
check 'stop, continue and foreground wait' 'done' "$("$HAWK_BIN" --sh -c 'sleep 10 & kill -STOP %1; sleep 0.1; jobs; bg %1; kill -TERM %1; fg %1; echo done' | tail -1)"
check 'wait accepts last background PID' '0' "$("$HAWK_BIN" --sh -c 'sleep 0.01 & wait "$!"; echo "$?"')"
check_status 'closing stdout fails gracefully' 1 'echo closed >&-'
check 'high-numbered descriptor survives redirection' 'high' "$("$HAWK_BIN" --sh -c "printf high 100> '$tmp/file' >&100; cat '$tmp/file'")"
check 'PATH lookup ignores registered binary alias' 'native' "$("$HAWK_BIN" --sh -c 'addcmd cat /bin/false; mode path; echo native | cat')"
check 'UTF8 command data' '한글' "$("$HAWK_BIN" --sh -c 'echo "한글" | cat')"
check 'heredoc before pipeline continuation' 'CONTINUED' "$(printf 'cat <<EOF |\ncontinued\nEOF\ntr a-z A-Z\n' | "$HAWK_BIN" --sh)"
check_status 'empty heredoc delimiter requires a delimiter line' 2 "$(printf "cat <<''\n")"
{
	printf 'wc -c <<EOF\n'
	head -c 200000 /dev/zero | tr '\000' a
	printf '\nEOF\n'
} > "$tmp/large.sh"
check 'large heredoc does not use a blocking pipe' '200001' "$("$HAWK_BIN" --sh "$tmp/large.sh" | tr -d ' ')"
mkfifo "$tmp/fifo"
"$HAWK_BIN" --sh -c "cat < '$tmp/fifo' > '$tmp/file' | echo fifo > '$tmp/fifo'"
check 'FIFO redirections open concurrently' 'fifo' "$(cat "$tmp/file")"
check 'builtin prefix assignment is temporary' 'old' "$("$HAWK_BIN" --sh -c 'export X=old; X=temp echo quiet > /dev/null; echo "$X"')"
printf 'echo limited | cat\n' > "$tmp/limited.sh"
check 'script and pipeline work with a small descriptor limit' 'limited' "$(ulimit -n 64; "$HAWK_BIN" --sh "$tmp/limited.sh")"
check 'jobs works as a pipeline stage' '[1] Running sleep' "$("$HAWK_BIN" --sh -c 'sleep 10 & jobs | cat; kill -TERM %1; wait %1')"
check 'completed foreground pipelines do not consume job numbers' '0' "$("$HAWK_BIN" --sh -c 'printf "" | cat; sleep 0.01 & wait %1; echo "$?"')"
"$HAWK_BIN" --sh -c "$(printf '\n\necho "')" >"$tmp/out" 2>"$tmp/err"
check 'CLI prints the structured error line' 'ERROR: line 3: unterminated quote' "$(cat "$tmp/err")"
"$HAWK_BIN" --sh -c 'echo ${X:-default}' >"$tmp/out" 2>"$tmp/err"
check 'CLI prints formatted unsupported-operator diagnostic' 'ERROR: line 1: parameter expansion operator is not implemented - :' "$(cat "$tmp/err")"
exit "$failed"
