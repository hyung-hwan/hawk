#!/bin/sh
## use a subshell to execute the actual program in case the
## program crashes or exits without an error message.

cleanup() {
	[ -n "$logfile" && -f "$logfile" ] && rm -f "$logfile"
}

trap 'cleanup' EXIT

echo RUN "[$@]"
if [ "x$MEMCHECK" = "xyes" -a -x /usr/bin/valgrind ]
then
	hawk="$1"
	shift
	xhawk=$(echo "$hawk" | sed -r 's|/hawk|/.libs/hawk|g')
	[ -x "$xhawk" ] && hawk="$xhawk"
	logfile="/tmp/hawk-t-run.$$"
	cmd="valgrind --leak-check=full --show-reachable=yes --track-fds=yes --log-file=$logfile $hawk $@"
	valgrind --leak-check=full --show-reachable=yes --track-fds=yes --log-file="$logfile" "$hawk" "$@" 2>&1
	## TODO: how to suppress the following tap deriver warning...
	#ERROR: h-002.hawk 475 : memory leak # AFTER LATE PLAN
	#ERROR: h-002.hawk - too many tests run (expected 474, got 475)
	grep "LEAK SUMMARY" "$logfile" && echo "not ok: MEMORY LEAK - $cmd"
	rm -f "$logfile"
else
	"$@"
fi

exit 0
