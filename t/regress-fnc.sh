#!/bin/sh

# Every case runs in its own process, including allocation-failure sweeps.
probe=${FNC_REGRESS_BIN:-./fnc-regress}
ulimit -c 0 2>/dev/null || :

if ! cases=$("$probe" --list)
then
	echo 'Bail out! unable to list fnc regression cases; build fnc-regress first'
	exit 1
fi

number=0
failed=0
for name in $cases
do
	number=$((number + 1))
done
echo "1..$number"
number=0

for name in $cases
do
	number=$((number + 1))
	output=$("$probe" "$name" 2>&1)
	status=$?
	case "$status" in
	0)
		echo "ok $number - $name"
		;;
	77)
		echo "ok $number - $name # SKIP $output"
		;;
	*)
		echo "not ok $number - $name"
		echo "# exit status: $status"
		printf '%s\n' "$output" | sed 's/^/# /'
		failed=1
		;;
	esac
done

exit "$failed"
