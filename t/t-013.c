/* Test the embeddable M4 processor and per-instance state. */

#include <hawk-m4.h>
#include <hawk-str.h>
#include "tap.h"

struct m4_xtn_t
{
	const hawk_ooch_t* input;
	hawk_oow_t input_pos;
	hawk_ooch_t output[128];
	hawk_oow_t output_len;
	int syscmds;
};

static hawk_ooi_t m4_io (hawk_m4_t* m4, hawk_m4_io_cmd_t cmd, hawk_m4_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	struct m4_xtn_t* xtn = (struct m4_xtn_t*)hawk_m4_getxtn(m4);
	hawk_oow_t i, len, rem;

	switch (cmd)
	{
		case HAWK_M4_IO_OPEN:
			if (arg->kind == HAWK_M4_IO_INPUT) xtn->input_pos = 0;
			return 0;

		case HAWK_M4_IO_CLOSE:
			return 0;

		case HAWK_M4_IO_READ:
			len = hawk_count_oocstr(xtn->input);
			rem = len - xtn->input_pos;
			if (count > rem) count = rem;
			if (count > 0)
			{
				for (i = 0; i < count; i++) data[i] = xtn->input[xtn->input_pos + i];
				xtn->input_pos += count;
			}
			return count;

		case HAWK_M4_IO_WRITE:
			if (arg->kind == HAWK_M4_IO_ERROR) return count;
			if (count > HAWK_COUNTOF(xtn->output) - xtn->output_len - 1) return -1;
			for (i = 0; i < count; i++) xtn->output[xtn->output_len + i] = data[i];
			xtn->output_len += count;
			xtn->output[xtn->output_len] = HAWK_T('\0');
			return count;

		case HAWK_M4_IO_SYSCMD:
			xtn->syscmds++;
			return 0;
	}

	return -1;
}

int main (void)
{
	hawk_m4_t* one;
	hawk_m4_t* two;
	struct m4_xtn_t* xone;
	struct m4_xtn_t* xtwo;

	no_plan();
	one = hawk_m4_open(HAWK_NULL, HAWK_SIZEOF(*xone), HAWK_NULL, HAWK_NULL);
	two = hawk_m4_open(HAWK_NULL, HAWK_SIZEOF(*xtwo), HAWK_NULL, HAWK_NULL);
	OK(one != HAWK_NULL && two != HAWK_NULL, "open two independent M4 instances");
	if (one && two)
	{
		const hawk_ooch_t* includedirs;

		xone = (struct m4_xtn_t*)hawk_m4_getxtn(one);
		xtwo = (struct m4_xtn_t*)hawk_m4_getxtn(two);
		OK(xone->input == HAWK_NULL && xone->output_len == 0, "extension area is zero initialized");
		xone->input = HAWK_T("define(`value', `one')dnl\nvalue\n");
		xtwo->input = HAWK_T("define(`value', `two')dnl\nvalue\n");
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) == 0, "execute first instance through custom I/O");
		OK(hawk_m4_exec(two, HAWK_NULL, m4_io) == 0, "execute second instance through custom I/O");
		OK(hawk_comp_oocstr(xone->output, HAWK_T("one\n"), 0) == 0, "first instance keeps its own macro table");
		OK(hawk_comp_oocstr(xtwo->output, HAWK_T("two\n"), 0) == 0, "second instance keeps its own macro table");

		OK(hawk_m4_setopt(one, HAWK_M4_OPT_INCDIRS, HAWK_T("dir1:dir2")) == 0 &&
		   hawk_m4_getopt(one, HAWK_M4_OPT_INCDIRS, &includedirs) == 0 &&
		   hawk_comp_oocstr(includedirs, HAWK_T("dir1:dir2"), 0) == 0,
		   "set and get the include-directory option");
		OK(hawk_m4_define(one, HAWK_T("external"), HAWK_T("from-api")) == 0,
		   "define a macro through the public API");
		xone->input = HAWK_T("external\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) == 0 &&
		   hawk_comp_oocstr(xone->output, HAWK_T("from-api\n"), 0) == 0,
		   "expand a macro defined through the public API");
		OK(hawk_m4_undefine(one, HAWK_T("external")) == 0,
		   "undefine a macro through the public API");
		xone->input = HAWK_T("ifdef(`external', `present', `absent')\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) == 0 &&
		   hawk_comp_oocstr(xone->output, HAWK_T("absent\n"), 0) == 0,
		   "observe API undefinition during execution");

		xone->input = HAWK_T("define(`word', `한글')dnl\nword\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) == 0, "execute non-ASCII text through generic-character I/O");
		OK(hawk_comp_oocstr(xone->output, HAWK_T("한글\n"), 0) == 0, "preserve non-ASCII macro text");

		xone->input = HAWK_T("define(`empty', `')dnl\nempty\ndefine(`bad-name', `x')\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) <= -1, "reject an invalid macro name");
		OK(hawk_m4_geterrloc(one)->line == 3, "track lines across an ungot newline");

		xone->input = HAWK_T("changequote(`abcdefghijklmnopqrstuvwxyzABCDEFG', `]')\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) <= -1, "reject an oversized quote delimiter");
		OK(hawk_comp_oocstr(hawk_m4_geterrmsg(one), HAWK_T("beginning quote too long"), 0) == 0, "retain a formatted quote error");

		xone->input = HAWK_T("define(`after_quote_error', `ok')dnl\nafter_quote_error\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) == 0, "reuse an instance after a rejected quote delimiter");
		OK(hawk_comp_oocstr(xone->output, HAWK_T("ok\n"), 0) == 0, "failed changequote leaves both delimiters unchanged");

		xone->input = HAWK_T("syscmd(`ignored')done\n");
		xone->input_pos = 0;
		xone->output_len = 0;
		xone->output[0] = HAWK_T('\0');
		OK(hawk_m4_exec(one, HAWK_NULL, m4_io) == 0 && xone->syscmds == 1, "delegate syscmd through the I/O callback");
		OK(hawk_comp_oocstr(xone->output, HAWK_T("done\n"), 0) == 0, "continue processing after delegated syscmd");
	}
	if (one) hawk_m4_close(one);
	if (two) hawk_m4_close(two);
	return exit_status();
}
