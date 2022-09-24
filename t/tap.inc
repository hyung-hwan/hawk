@global tap;

function tap_end ()
{
	printf ("1..%d\n", tap["total"]);
}

function tap_ok (msg)
{
	tap["total"]++;
	printf ("ok %d - %s\n", tap["total"], msg);
}

function tap_fail (msg)
{
	tap["total"]++;
	printf ("not ok %d - %s\n", tap["total"], msg);
}

function tap_skip (msg)
{
	tap["total"]++;
	printf ("ok %d - # skip%s%s\n", tap["total"], (length(msg) > 0? " ": ""), msg);
}


function tap_ensure (a, b, desc, line)
{
        @local id;
       
	id = sprintf("%s[%d]", desc, line);
        if (a != b) tap_fail (id);
	else tap_ok (id);
}

BEGIN {
	tap["total"] = 0;
}
