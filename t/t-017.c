/* Sed line-ending policies with single-character reads and partial writes. */
#include <hawk.h>
#include <hawk-sed.h>
#include <hawk-utl.h>
#include <string.h>
#include "tap.h"

typedef struct test_io_t test_io_t;
struct test_io_t
{
	const hawk_ooch_t* script;
	const hawk_ooch_t* input;
	const hawk_ooch_t* file_input;
	hawk_oow_t script_pos;
	hawk_oow_t input_pos;
	hawk_oow_t file_pos;
	hawk_ooch_t output[10000];
	hawk_oow_t output_len;
	hawk_ooch_t file_output[10000];
	hawk_oow_t file_output_len;
	int fail_final_cr;
};

static hawk_ooi_t script_io (hawk_sed_t* sed, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	test_io_t* io = hawk_sed_getxtn(sed);

	if (cmd == HAWK_SED_IO_OPEN) { io->script_pos = 0; return 1; }
	if (cmd == HAWK_SED_IO_CLOSE) return 0;
	if (cmd != HAWK_SED_IO_READ || count == 0) return -1;
	if (io->script[io->script_pos] == HAWK_T('\0')) return 0;
	data[0] = io->script[io->script_pos++];
	return 1;
}

static hawk_ooi_t input_io (hawk_sed_t* sed, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	test_io_t* io = hawk_sed_getxtn(sed);
	const hawk_ooch_t* input = arg->path? io->file_input: io->input;
	hawk_oow_t* pos = arg->path? &io->file_pos: &io->input_pos;

	if (cmd == HAWK_SED_IO_OPEN) { *pos = 0; return 1; }
	if (cmd == HAWK_SED_IO_CLOSE) return 0;
	if (cmd != HAWK_SED_IO_READ || count == 0) return -1;
	if (input[*pos] == HAWK_T('\0')) return 0;
	data[0] = input[(*pos)++];
	return 1;
}

static hawk_ooi_t output_io (hawk_sed_t* sed, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	test_io_t* io = hawk_sed_getxtn(sed);
	hawk_ooch_t* output = arg->path? io->file_output: io->output;
	hawk_oow_t* len = arg->path? &io->file_output_len: &io->output_len;

	if (cmd == HAWK_SED_IO_OPEN) { arg->handle = io; return 1; }
	if (cmd == HAWK_SED_IO_CLOSE) { arg->handle = HAWK_NULL; return 0; }
	if (cmd != HAWK_SED_IO_WRITE || count == 0) return -1;
	if (*len >= HAWK_COUNTOF(io->output) || (io->fail_final_cr && data[0] == HAWK_T('\r')))
	{
		hawk_sed_seterrnum(sed, HAWK_NULL, HAWK_EIOERR);
		return -1;
	}
	output[(*len)++] = data[0];
	return 1;
}

static void check_case (
	const char* name, const hawk_ooch_t* script, const hawk_ooch_t* input,
	hawk_sed_input_line_ending_t in, hawk_sed_output_line_ending_t out,
	const hawk_ooch_t* expected, const hawk_ooch_t* file_input, const hawk_ooch_t* file_expected)
{
	hawk_sed_t* sed;
	test_io_t* io;
	hawk_oow_t len;
	int quiet = HAWK_SED_QUIET;
	int n;

	sed = hawk_sed_open(hawk_get_sys_mmgr(), sizeof(*io), hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), HAWK_NULL);
	if (!sed) { OK(0, name); return; }
	io = hawk_sed_getxtn(sed);
	io->script = script;
	io->input = input;
	io->file_input = file_input? file_input: HAWK_T("");
	if (file_expected) hawk_sed_setopt(sed, HAWK_SED_TRAIT, &quiet);

	n = hawk_sed_setopt(sed, HAWK_SED_INPUT_LINE_ENDING, &in);
	if (n == 0) n = hawk_sed_setopt(sed, HAWK_SED_OUTPUT_LINE_ENDING, &out);
	if (n == 0) n = hawk_sed_comp(sed, script_io);
	if (n == 0) n = hawk_sed_exec(sed, input_io, output_io);
	len = hawk_count_oocstr(expected);
	if (n == 0 && (io->output_len != len || memcmp(io->output, expected, len * sizeof(*expected)) != 0)) n = -1;
	if (n == 0 && file_expected)
	{
		len = hawk_count_oocstr(file_expected);
		if (io->file_output_len != len || memcmp(io->file_output, file_expected, len * sizeof(*file_expected)) != 0) n = -1;
	}
	OK(n == 0, name);
	hawk_sed_close(sed);
}

#define AUTO HAWK_SED_INPUT_LINE_ENDING_AUTO
#define LF HAWK_SED_INPUT_LINE_ENDING_LF
#define CRLF HAWK_SED_INPUT_LINE_ENDING_CRLF
#define PRESERVE HAWK_SED_OUTPUT_LINE_ENDING_PRESERVE
#define OUT_LF HAWK_SED_OUTPUT_LINE_ENDING_LF
#define OUT_CRLF HAWK_SED_OUTPUT_LINE_ENDING_CRLF
#define CASE(name, script, input, in, out, expected) \
	check_case(name, HAWK_T(script), HAWK_T(input), in, out, HAWK_T(expected), HAWK_NULL, HAWK_NULL)

int main ()
{
	hawk_sed_t* sed;
	test_io_t* io;
	hawk_sed_input_line_ending_t in;
	hawk_sed_output_line_ending_t out;
	hawk_ooch_t long_input[4100], long_output[4100];
	hawk_ooch_t non_ascii_input[] = { (hawk_ooch_t)0xE9, HAWK_T('\r'), HAWK_T('\n'), HAWK_T('\0') };
	hawk_ooch_t non_ascii_output[] = { (hawk_ooch_t)0xE9, HAWK_T('\n'), HAWK_T('\0') };
	hawk_oow_t i;
	int n;

	no_plan();
	sed = hawk_sed_open(hawk_get_sys_mmgr(), sizeof(*io), hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), HAWK_NULL);
	if (!sed) { OK(0, "open sed"); return exit_status(); }
	OK(hawk_sed_getopt(sed, HAWK_SED_INPUT_LINE_ENDING, &in) == 0 && in == AUTO, "default input is auto");
	OK(hawk_sed_getopt(sed, HAWK_SED_OUTPUT_LINE_ENDING, &out) == 0 && out == PRESERVE, "default output is preserve");
	in = (hawk_sed_input_line_ending_t)99;
	OK(hawk_sed_setopt(sed, HAWK_SED_INPUT_LINE_ENDING, &in) <= -1, "reject invalid input policy");
	OK(hawk_sed_getopt(sed, HAWK_SED_INPUT_LINE_ENDING, &in) == 0 && in == AUTO, "invalid input policy leaves previous setting");
	out = (hawk_sed_output_line_ending_t)99;
	OK(hawk_sed_setopt(sed, HAWK_SED_OUTPUT_LINE_ENDING, &out) <= -1, "reject invalid output policy");
	OK(hawk_sed_getopt(sed, HAWK_SED_OUTPUT_LINE_ENDING, &out) == 0 && out == PRESERVE, "invalid output policy leaves previous setting");
	hawk_sed_close(sed);

	CASE("preserve mixed endings and missing final newline", "", "a\r\nb\nc\r", AUTO, PRESERVE, "a\r\nb\nc\r");
	CASE("auto hides terminal CR from substitution", "s|\r||g", "a\r\nb\n", AUTO, PRESERVE, "a\r\nb\n");
	CASE("LF exposes CR to substitution", "s|\r||g", "a\r\nb\n", LF, PRESERVE, "a\nb\n");
	CASE("LF exposes CR to address matching", "/\r$/s/a/A/", "a\r\nb\n", LF, PRESERVE, "A\r\nb\n");
	CASE("LF exposes CR to translation", "y|\r|X|", "a\r\nb\n", LF, PRESERVE, "aX\nb\n");
	CASE("auto excludes CR from translation", "y|\r|X|", "a\r\nb\n", AUTO, PRESERVE, "a\r\nb\n");
	CASE("CRLF treats lone LF as record content", "=", "a\nb\r\nc\n", CRLF, PRESERVE, "1\na\nb\r\n2\nc\n");
	CASE("CRLF leaves final lone LF searchable", "s|\\n|X|g", "a\nb\n", CRLF, PRESERVE, "aXbX");
	CASE("empty script normalizes LF", "", "a\r\nb\nc", AUTO, OUT_LF, "a\nb\nc");
	CASE("empty script normalizes CRLF", "", "a\r\nb\nc", AUTO, OUT_CRLF, "a\r\nb\r\nc");
	CASE("empty input stays empty", "", "", AUTO, OUT_CRLF, "");
#if defined(__WIN32__) || defined(__OS2__) || defined(__DOS__)
	CASE("native output uses CRLF", "", "a\r\nb\nc", AUTO, HAWK_SED_OUTPUT_LINE_ENDING_NATIVE, "a\r\nb\r\nc");
#else
	CASE("native output uses LF", "", "a\r\nb\nc", AUTO, HAWK_SED_OUTPUT_LINE_ENDING_NATIVE, "a\nb\nc");
#endif
	CASE("LF input retains content CR when output is LF", "", "a\r\n", LF, OUT_LF, "a\r\n");
	CASE("LF input retains content CR when output is CRLF", "", "a\r\n", LF, OUT_CRLF, "a\r\r\n");
	CASE("normalization preserves trailing bare CR", "", "a\r", AUTO, OUT_LF, "a\r");
	CASE("normalization preserves consecutive CR", "", "a\r\r\nb\r\rc", AUTO, OUT_LF, "a\r\nb\r\rc");
	CASE("N normalizes all accumulated lines", "N", "a\r\nb\nc\r\nd\n", AUTO, OUT_CRLF, "a\r\nb\r\nc\r\nd\r\n");
	CASE("P honors CRLF-only record boundaries", "N;P;d", "a\nb\r\nc\r\n", CRLF, PRESERVE, "a\nb\r\n");
	CASE("D honors CRLF-only record boundaries", "N;P;D", "a\nb\r\nc\r\n", CRLF, PRESERVE, "a\nb\r\nc\r\n");
	CASE("hold space output is normalized", "h;g", "a\r\nb\n", AUTO, OUT_LF, "a\nb\n");
	CASE("generated line numbers use output policy", "=", "a\n", AUTO, OUT_CRLF, "1\r\na\r\n");
	CASE("append text uses output policy", "a\\\nx\n", "a\n", AUTO, OUT_CRLF, "a\r\nx\r\n");
	CASE("insert text uses output policy", "i\\\nx\n", "a\n", AUTO, OUT_CRLF, "x\r\na\r\n");
	CASE("change text uses output policy", "c\\\nx\n", "a\r\n", AUTO, OUT_CRLF, "x\r\n");
	check_case("w normalizes named output with partial writes", HAWK_T("w out\n"), HAWK_T("a\r\nb\nc\r"), AUTO, OUT_LF, HAWK_T(""), HAWK_NULL, HAWK_T("a\nb\nc\r"));
	check_case("substitution w uses output policy", HAWK_T("s/a/A/w out\n"), HAWK_T("a\r\n"), AUTO, OUT_LF, HAWK_T(""), HAWK_NULL, HAWK_T("A\n"));
	check_case("W honors CRLF-only boundaries", HAWK_T("N;W out\n"), HAWK_T("a\nb\r\nc\r\n"), CRLF, PRESERVE, HAWK_T(""), HAWK_NULL, HAWK_T("a\nb\r\n"));
	check_case("r converts CRLF split across reads", HAWK_T("r in\n"), HAWK_T("a\n"), AUTO, OUT_LF, HAWK_T("a\nx\ny\r"), HAWK_T("x\r\ny\r"), HAWK_NULL);
	check_case("R honors CRLF-only boundaries across reads", HAWK_T("R in\n"), HAWK_T("a\r\n"), CRLF, PRESERVE, HAWK_T("a\r\nx\ny\r\n"), HAWK_T("x\ny\r\nz\r\n"), HAWK_NULL);

	for (i = 0; i < 4096; i++) long_input[i] = long_output[i] = HAWK_T('x');
	long_input[2047] = HAWK_T('\r');
	long_input[2048] = HAWK_T('\n');
	long_input[4096] = HAWK_T('\0');
	long_output[2047] = HAWK_T('\n');
	for (i = 2048; i < 4095; i++) long_output[i] = HAWK_T('x');
	long_output[4095] = HAWK_T('\0');
	check_case("CRLF across the output buffer boundary", HAWK_T("N"), long_input, AUTO, OUT_LF, long_output, HAWK_NULL, HAWK_NULL);
	check_case("conversion preserves non-ASCII characters", HAWK_T(""), non_ascii_input, AUTO, OUT_LF, non_ascii_output, HAWK_NULL, HAWK_NULL);

	sed = hawk_sed_open(hawk_get_sys_mmgr(), sizeof(*io), hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), HAWK_NULL);
	if (!sed) { OK(0, "open sed for write failure"); return exit_status(); }
	io = hawk_sed_getxtn(sed);
	io->script = HAWK_T("");
	io->input = HAWK_T("a\r");
	io->fail_final_cr = 1;
	out = OUT_LF;
	n = hawk_sed_setopt(sed, HAWK_SED_OUTPUT_LINE_ENDING, &out);
	if (n == 0) n = hawk_sed_comp(sed, script_io);
	if (n == 0) n = hawk_sed_exec(sed, input_io, output_io);
	OK(n <= -1 && hawk_sed_geterrnum(sed) == HAWK_EIOERR, "final pending CR write failure is reported");
	io->script = HAWK_T("w out\n");
	io->output_len = 0;
	io->file_output_len = 0;
	n = HAWK_SED_QUIET;
	hawk_sed_setopt(sed, HAWK_SED_TRAIT, &n);
	n = hawk_sed_comp(sed, script_io);
	if (n == 0) n = hawk_sed_exec(sed, input_io, output_io);
	OK(n <= -1 && hawk_sed_geterrnum(sed) == HAWK_EIOERR, "named output final pending CR write failure is reported");
	io->fail_final_cr = 0;
	io->file_output_len = 0;
	n = hawk_sed_exec(sed, input_io, output_io);
	OK(n == 0 && io->file_output_len == 2 && io->file_output[0] == HAWK_T('a') && io->file_output[1] == HAWK_T('\r'), "execution after failure resets pending CR state");
	hawk_sed_close(sed);
	return exit_status();
}
