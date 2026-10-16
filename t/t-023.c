/* Standard source IO observation, including mixed inputs and rejection cleanup. */
#include "hawk-prv.h"
#include "tap.h"

typedef struct observer_t observer_t;
struct observer_t
{
	int opens;
	int closes;
	int reads;
	int reject;
	int bad;
};

static int observe (hawk_t* hawk, hawk_sio_cmd_t cmd, const hawk_sio_arg_t* arg,
                    const hawk_ooch_t* data, hawk_oow_t size, void* ctx)
{
	observer_t* observer = ctx;

	if (!arg->path || !arg->handle) observer->bad = 1;
	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN: observer->opens++; break;
		case HAWK_SIO_CMD_CLOSE: observer->closes++; break;
		case HAWK_SIO_CMD_READ:
			observer->reads++;
			if (size > 0 && !data) observer->bad = 1;
			break;
		default: observer->bad = 1; break;
	}
	if (observer->reject == cmd)
	{
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EIOERR);
		return -1;
	}
	return 0;
}

int main (void)
{
	hawk_t* hawk;
	hawk_parsestd_t input[4];
	observer_t observer;
	hawk_bch_t includes[4096];
	const hawk_bch_t* middle = "function source_middle() { return 3; }\n";
	int n;

	plan(8);
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return 1; }
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_FILEB;
	input[0].u.fileb.path = TEST_SRCDIR "/parse-source-a.inc";
	input[1].type = HAWK_PARSESTD_BCS;
	input[1].u.bcs.ptr = middle;
	input[1].u.bcs.len = hawk_count_bcstr(middle);
	input[2].type = HAWK_PARSESTD_FILEB;
	input[2].u.fileb.path = TEST_SRCDIR "/parse-source-b.inc";
	HAWK_MEMSET(&observer, 0, HAWK_SIZEOF(observer));
	observer.reject = -1;
	n = hawk_parsestdwithobserver(hawk, input, HAWK_NULL, observe, &observer);
	OK(n == 0, "observer supports file/string/file input sequences");
	OK(!observer.bad && observer.opens == 2 && observer.closes == 2 && observer.reads > 0,
		"only files are reported and every successful open is closed once");

	snprintf(includes, HAWK_COUNTOF(includes),
		"@include_once \"%s/parse-source-b.inc\";\n@include_once \"%s/parse-source-b.inc\";\n", TEST_SRCDIR, TEST_SRCDIR);
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = includes;
	input[0].u.bcs.len = hawk_count_bcstr(includes);
	HAWK_MEMSET(&observer, 0, HAWK_SIZEOF(observer));
	observer.reject = -1;
	OK(hawk_parsestdwithobserver(hawk, input, HAWK_NULL, observe, &observer) == 0,
		"observer supports includes from string input and include_once");
	OK(!observer.bad && observer.opens == 2 && observer.closes == 2,
		"include_once reports resolved opens even when duplicate contents are skipped");

	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_FILEB;
	input[0].u.fileb.path = TEST_SRCDIR "/parse-source-a.inc";
	HAWK_MEMSET(&observer, 0, HAWK_SIZEOF(observer));
	observer.reject = HAWK_SIO_CMD_OPEN;
	OK(hawk_parsestdwithobserver(hawk, input, HAWK_NULL, observe, &observer) <= -1 &&
	   hawk_geterrnum(hawk) == HAWK_EIOERR, "OPEN rejection propagates its diagnostic");
	OK(hawk_parsestd(hawk, input, HAWK_NULL) == 0, "observer context is cleared before a later ordinary parse");
	HAWK_MEMSET(&observer, 0, HAWK_SIZEOF(observer));
	observer.reject = HAWK_SIO_CMD_READ;
	OK(hawk_parsestdwithobserver(hawk, input, HAWK_NULL, observe, &observer) <= -1 &&
	   observer.opens == 1 && observer.closes == 1 && hawk_geterrnum(hawk) == HAWK_EIOERR,
		"READ rejection closes its stream and preserves the diagnostic");
	HAWK_MEMSET(&observer, 0, HAWK_SIZEOF(observer));
	observer.reject = HAWK_SIO_CMD_CLOSE;
	OK(hawk_parsestdwithobserver(hawk, input, HAWK_NULL, observe, &observer) <= -1 &&
	   observer.closes == 1 && hawk_geterrnum(hawk) == HAWK_EIOERR,
		"CLOSE rejection reports failure while still releasing its stream");
	hawk_close(hawk);
	return exit_status();
}
