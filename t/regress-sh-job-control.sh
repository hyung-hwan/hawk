#!/bin/sh
: "${HAWK_BIN:=hawk}"
: "${SH_JOB_CONTROL_BIN:=./sh-job-control}"
if ! "$HAWK_BIN" --sh -c : >/dev/null 2>&1; then
	echo '1..0 # SKIP POSIX shell backend required'
	exit 0
fi
exec "$SH_JOB_CONTROL_BIN" "$HAWK_BIN"
