#!/bin/sh

for i in $@; do :; done
script="$i"

escape_regex() {
	local str="$1"
	## escape [, ], {, (, \, $, *, . ^, ), }
	printf '%s\n' "$str" | sed -r -e 's/[][{(\$*.^)}]/\\&/g'
}

run_partfile() {
	l_cmd="";
	l_nargs=$#

	while [ $# -gt 3 ]
	do
		l_cmd="$l_cmd $1"
		shift
	done

	l_script="$1"
	shift ## skip the original script.
	l_partno="$1"
	shift ## partno

	l_partfile="$1"
	l_cmd="$l_cmd $l_partfile"

        ## old greps don't support -o. use sed to mimic it.
	##l_expected_errinfo=$(grep -n -o -E "##ERROR: .+" "$l_partfile" 2>/dev/null)
        l_expected_errinfo=$(grep -n -E "##ERROR: .+" "$l_partfile" 2>/dev/null | sed -r -n 's/^([0-9]*):.*(##ERROR: .*)/\1:\2/p')
	[ -z "$l_expected_errinfo" ] && {
		echo "ERROR: INVALID TESTER - $l_script($l_partno) contains no ERROR information"
		return 1
	}

	l_expected_errline=$(echo $l_expected_errinfo | cut -d: -f1)
	l_xlen=$(echo $l_expected_errline | wc -c)
	l_xlen=$(expr $l_xlen + 10)
	l_expected_errmsg=$(echo $l_expected_errinfo | cut -c${l_xlen}-)
	l_output=`$l_cmd 2>&1`
	## the regular expression is not escaped properly. the error information must not
	## include special regex characters to avoid problems.
	l_expected_errmsg_esc=$(escape_regex "${l_expected_errmsg}")
	echo "$l_output" | grep -E "ERROR: .+ LINE ${l_expected_errline} .+ FILE ${l_partfile} - ${l_expected_errmsg_esc}" >/dev/null 2>&1 || {
		echo "ERROR: error not raised at line $l_expected_errline - $l_script($l_partno) - $l_output"
		return 1
	}

	echo "OK"
	return 0
}


ever_failed=0
## [NOTE]
##   some old mktemp(e.g mktemp 1.5 on rhel 2.1) always required the template. 
##   these days, newer ones don't need it.
partfile=`mktemp tmp.XXXXXXXXXX`
partno=0
partlines=0
> "$partfile"

## dash behaves differently for read -r.
## while \n is read in literally by bash or other shells, dash converts it to a new-line
while IFS= read -r line
do
	if [ "$line" = "---" ]
	then
		[ $partlines -gt 0 ] && {
			run_partfile "$@" "$partno" "$partfile" || ever_failed=1
		}
		partno=`expr $partno + 1`
		partlines=0
		> "$partfile"
	else
		echo "$line" >> "$partfile"
		partlines=`expr $partlines + 1`
	fi
done < "$script"

[ $partlines -gt 0 ] && {
	run_partfile "$@" "$partno" "$partfile" || ever_failed=1
}

rm -f "$partfile"
exit $ever_failed

