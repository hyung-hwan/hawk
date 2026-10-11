/* Portable shell parser, registration and allocation-failure tests. */
#include <hawk-sh.h>
#include <hawk-str.h>
#include <stdlib.h>
#include "tap.h"

typedef struct context_t context_t;

struct context_t
{
	int calls, background, redirs;
	hawk_oow_t argc, arglen, bodylen;
	hawk_ooch_t arg[80], body[80];
	const hawk_ooch_t* envname;
	const hawk_ooch_t* envvalue;
};

static int run (hawk_sh_t* sh, const hawk_sh_pipeline_t* p, void* ctx)
{
	context_t* x = ctx;
	const hawk_sh_cmd_t* c = p->commands;
	const hawk_sh_redir_t* r;
	int status, n;
	x->calls++; x->background = p->background; x->argc = c->argc;
	if (c->argc > 1)
	{
		x->arglen = hawk_count_oocstr(c->argv[1]);
		hawk_copy_oocstr(x->arg, HAWK_COUNTOF(x->arg), c->argv[1]);
	}
	for (r = c->redirs; r; r = r->next)
	{
		x->redirs++;
		if (r->body)
		{
			x->bodylen = hawk_count_oocstr(r->body);
			hawk_copy_oocstr(x->body, HAWK_COUNTOF(x->body), r->body);
		}
	}
	n = hawk_sh_call(sh, c, &status);
	return n < 0? -1: (n == 1? status: 0);
}

static int get_variable (hawk_sh_t* sh, const hawk_ooch_t* name, const hawk_ooch_t** value, void* ctx)
{
	context_t* x = ctx;

	*value = HAWK_NULL;
	if (hawk_comp_oocstr(name, HAWK_T("FAIL"), 0) == 0)
	{
		hawk_gem_seterrbfmt(hawk_sh_getgem(sh), HAWK_NULL, HAWK_EACCES, "variable access denied");
		return -1;
	}
	if (x->envname && hawk_comp_oocstr(name, x->envname, 0) == 0) *value = x->envvalue;
	return *value? 1: 0;
}

static int registered (hawk_sh_t* sh, hawk_oow_t argc, const hawk_ooch_t* const* argv, void* ctx)
{ (*(int*)ctx)++; return 9; }

static int recursive (hawk_sh_t* sh, hawk_oow_t argc, const hawk_ooch_t* const* argv, void* ctx)
{
	return hawk_sh_execbcs(sh, "true");
}

typedef struct failing_input_t
{
	const hawk_ooch_t* ptr;
	hawk_errnum_t errnum;
	int failures, nul;
} failing_input_t;

static int failing_input (hawk_sh_t* sh, hawk_ooch_t* ch, void* ctx)
{
	failing_input_t* in = ctx;

	if (*in->ptr)
	{
		*ch = *in->ptr++;
		return 1;
	}
	if (in->nul)
	{
		in->nul = 0;
		*ch = '\0';
		return 1;
	}
	in->failures++;
	if (in->errnum != HAWK_ENOERR)
	{
		hawk_loc_t loc;

		loc.line = 77;
		loc.colm = 3;
		loc.file = HAWK_T("input.test");
		hawk_gem_seterrbfmt(hawk_sh_getgem(sh), &loc, in->errnum, "input failure %d", 91);
	}
	return -1;
}

typedef struct allocator_t {
	int calls;
	int fail;
	int live;
	int persistent;
} allocator_t;

static void* allocate (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	void* p;
	if (++a->calls >= a->fail && (a->persistent || a->calls == a->fail)) return HAWK_NULL;
	p = malloc(size); if (p) a->live++; return p;
}

static void* reallocate (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	void* p;
	int fresh = !ptr;
	if (++a->calls >= a->fail && (a->persistent || a->calls == a->fail)) return HAWK_NULL;
	p = realloc(ptr, size); if (p && fresh) a->live++; return p;
}

static void deallocate (hawk_mmgr_t* mmgr, void* ptr)
{
	if (ptr)
	{
		((allocator_t*)mmgr->ctx)->live--;
		free(ptr);
	}
}

int main (void)
{
	hawk_sh_t* one;
	hawk_sh_t* two;
	hawk_sh_prm_t prm = { run, HAWK_NULL, HAWK_NULL, get_variable };
	context_t a = { 0 }, b = { 0 };
	int calls = 0, i, leaks = 0;
	int failures = 0, wrong_errors = 0;

	no_plan();

	one = hawk_sh_open(HAWK_NULL, 16, HAWK_NULL, HAWK_NULL);
	two = hawk_sh_open(HAWK_NULL, 0, HAWK_NULL, HAWK_NULL);
	OK(one && two, "independent shell instances");
	if (!one || !two) return 1;

	hawk_sh_setprm(one, &prm, &a); hawk_sh_setprm(two, &prm, &b);
	OK(((unsigned char*)hawk_sh_getxtn(one))[0] == 0, "zeroed extension");

	OK(hawk_sh_execbcs(one, "x ab\" cd\" '' a\\ b 2>&1\n") == 0 && a.argc == 4 && a.redirs == 1 && hawk_comp_oocstr(a.arg, HAWK_T("ab cd"), 0) == 0, "quotes and descriptors");
	OK(hawk_sh_execucs(two, HAWK_UT("x \"\uD55C\uAE00\"\n")) == 0 && b.argc == 2, "UCH input API");
	OK(hawk_sh_execbcs(one, "cat <<'EOF'\n$HOME\nEOF\n") == 0 && hawk_comp_oocstr(a.body, HAWK_T("$HOME\n"), 0) == 0, "quoted heredoc");
	OK(hawk_sh_execbcs(one, "false && x; true || x; x &\n") == 0 && a.background, "logical lists and background flag");
	OK(hawk_sh_addcmd(one, HAWK_T("custom"), HAWK_NULL, registered, &calls) == 0 && hawk_sh_execbcs(one, "custom") == 9 && calls == 1, "registered callback");
	OK(!hawk_sh_isbuiltin(two, HAWK_T("custom")), "instance-specific registrations");
	OK(hawk_sh_execbcs(one, "mode registered") == 0 && hawk_sh_getmode(one) == HAWK_SH_REGISTERED && hawk_sh_getmode(two) == HAWK_SH_PATH, "instance-specific mode");
	OK(hawk_sh_execbcs(one, "x\ncat <<EOF\n") < 0, "failure after success returns error");
	OK(hawk_sh_execbcs(one, "x '\n") < 0 && hawk_sh_execbcs(one, "true") == 0, "reusable after failure");
	OK(hawk_sh_execbcs(one, "echo >") < 0 && hawk_sh_execbcs(one, "echo hi |") < 0, "missing operands rejected");
	OK(hawk_sh_execbcs(one, "x ''") == 0 && a.argc == 2 && a.arglen == 0 && a.arg[0] == 0, "empty quoted argument owns an empty string");
	OK(hawk_sh_execbcs(one, "x \"$UNSET\"") == 0 && a.argc == 2 && a.arglen == 0, "unset expansion retains an empty argument");
	OK(hawk_sh_execbcs(one, "cat <<EOF\nEOF\n") == 0 && a.bodylen == 0 && a.body[0] == 0, "empty heredoc owns an empty string");
	OK(hawk_sh_execbcs(one, "cat <<''\n\n") == 0 && a.bodylen == 0, "empty heredoc delimiter matches an empty line");
	OK(hawk_sh_execbcs(one, "cat <<EOF\nlonger\n\nx\n\nEOF\n") == 0 && hawk_comp_oocstr(a.body, HAWK_T("longer\n\nx\n\n"), 0) == 0, "heredoc buffer clear drops previous line contents");

	{
		hawk_ooch_t value[257], name[129], script[400];
		hawk_oow_t n;

		for (n = 0; n < HAWK_COUNTOF(value) - 1; n++) value[n] = 'a';
		value[n] = '\0';
		script[0] = 'x';
		script[1] = ' ';
		hawk_copy_oocstr(script + 2, HAWK_COUNTOF(script) - 2, value);
		OK(hawk_sh_execoocs(one, script) == 0 && a.arglen == 256, "token and expansion buffers grow beyond initial capacity");

		a.envname = HAWK_T("LONG");
		a.envvalue = value;
		OK(hawk_sh_execbcs(one, "x \"$LONG$UNSET$LONG\"") == 0 && a.arglen == 512, "bulk variable expansion grows and reuses the name buffer");
		for (n = 0; n < HAWK_COUNTOF(name) - 1; n++) name[n] = 'n';
		name[n] = '\0';
		a.envname = name;
		n = hawk_copy_oocstr(script, HAWK_COUNTOF(script), HAWK_T("x \"${"));
		n += hawk_copy_oocstr(script + n, HAWK_COUNTOF(script) - n, name);
		hawk_copy_oocstr(script + n, HAWK_COUNTOF(script) - n, HAWK_T("}$X\""));
		OK(hawk_sh_execoocs(one, script) == 0 && a.arglen == 256, "long variable name followed by a shorter unset name");
		a.envname = HAWK_NULL;
		a.envvalue = HAWK_NULL;
	}

	{
		static const struct
		{
			const hawk_bch_t* script;
			hawk_errnum_t errnum;
			const hawk_bch_t* message;
			const hawk_bch_t* name;
		} cases[] =
		{
			{ "x \\", HAWK_EEOF, "unfinished escape", "unfinished escape reports EOF" },
			{ "x '", HAWK_EEOF, "unterminated quote", "unfinished single quote reports EOF" },
			{ "x \"", HAWK_EEOF, "unterminated quote", "unfinished double quote reports EOF" },
			{ "cat <<EOF\nbody\n", HAWK_EEOF, "unterminated here-document", "unfinished heredoc reports EOF" },
			{ "x >", HAWK_EEOF, "missing redirection operand", "missing operand at EOF reports EOF" },
			{ "x >;", HAWK_EINVAL, "missing redirection operand", "invalid redirection operand reports EINVAL" },
			{ "x |", HAWK_EEOF, "expected command", "missing pipeline stage reports EOF" },
			{ "x $(true)", HAWK_ENOIMPL, "command substitution is not implemented", "dollar command substitution reports ENOIMPL" },
			{ "x `true`", HAWK_ENOIMPL, "command substitution is not implemented", "backtick substitution reports ENOIMPL" },
			{ "x ${X", HAWK_EEOF, "unfinished parameter expansion", "unfinished parameter expansion reports EOF" },
			{ "x ${X:-default}", HAWK_ENOIMPL, "parameter expansion operator is not implemented - :", "unsupported parameter expansion formats its operator" },
			{ "x 256>file", HAWK_EINVAL, "file descriptor must be between 0 and 255", "invalid descriptor reports EINVAL" },
			{ "x $FAIL", HAWK_EACCES, "variable access denied", "variable callback error is preserved" }
		};
		hawk_oow_t n;

		for (n = 0; n < HAWK_COUNTOF(cases); n++)
		{
			OK(hawk_sh_execbcs(one, cases[n].script) < 0 &&
			   hawk_sh_geterrnum(one) == cases[n].errnum &&
			   hawk_comp_bcstr(hawk_sh_geterrbmsg(one), cases[n].message, 0) == 0,
			   cases[n].name);
		}
	}
	OK(hawk_sh_execbcs(one, "\n\nx '") < 0 && hawk_sh_geterrloc(one)->line == 3 &&
	   hawk_comp_bcstr(hawk_sh_geterrbmsg(one), "unterminated quote", 0) == 0,
	   "error line is recorded separately from the message");
	OK(hawk_sh_addcmd(one, HAWK_T(""), HAWK_NULL, registered, &calls) < 0 &&
	   hawk_sh_geterrnum(one) == HAWK_EINVAL, "invalid registration reports EINVAL");
	OK(hawk_sh_exec(one, HAWK_NULL, HAWK_NULL) < 0 && hawk_sh_geterrnum(one) == HAWK_EINVAL,
	   "missing input callback reports EINVAL");

	{
		hawk_sh_prm_t no_exec = { 0 };

		hawk_sh_setprm(one, &no_exec, &a);
		OK(hawk_sh_execbcs(one, "true") < 0 && hawk_sh_geterrnum(one) == HAWK_EINVAL,
		   "missing execution callback reports EINVAL");
		hawk_sh_setprm(one, &prm, &a);
	}

	OK(hawk_sh_addcmd(one, HAWK_T("recursive"), HAWK_NULL, recursive, HAWK_NULL) == 0 &&
	   hawk_sh_execbcs(one, "recursive") < 0 && hawk_sh_geterrnum(one) == HAWK_ESTATE,
	   "recursive execution reports ESTATE");
	{
		static const struct
		{
			const hawk_ooch_t* prefix;
			const hawk_bch_t* name;
		} cases[] =
		{
			{ HAWK_T(""), "read error before the first token is preserved" },
			{ HAWK_T("x"), "read error after a word is preserved" },
			{ HAWK_T("x \\"), "read error during escape is preserved" },
			{ HAWK_T("x 'text"), "read error inside single quotes is preserved" },
			{ HAWK_T("x \"text"), "read error inside double quotes is preserved" },
			{ HAWK_T("# comment"), "read error inside a comment is preserved" },
			{ HAWK_T("x |"), "read error after a pipe is preserved" },
			{ HAWK_T("x &"), "read error after an ampersand is preserved" },
			{ HAWK_T("x >"), "read error after redirection is preserved" },
			{ HAWK_T("cat <<EOF\nbody\n"), "read error inside a heredoc is preserved" },
			{ HAWK_T("cat <<-EOF\n\t"), "read error while stripping heredoc tabs is preserved" }
		};
		hawk_oow_t n;

		for (n = 0; n < HAWK_COUNTOF(cases); n++)
		{
			failing_input_t in = { cases[n].prefix, HAWK_EIOERR, 0, 0 };
			int before = a.calls;

			OK(hawk_sh_exec(one, failing_input, &in) < 0 && in.failures == 1 && a.calls == before &&
			   hawk_sh_geterrnum(one) == HAWK_EIOERR && hawk_sh_geterrloc(one)->line == 77 &&
			   hawk_sh_geterrloc(one)->colm == 3 &&
			   hawk_sh_geterrloc(one)->file != HAWK_NULL &&
			   hawk_comp_oocstr(hawk_sh_geterrloc(one)->file, HAWK_T("input.test"), 0) == 0 &&
			   hawk_comp_bcstr(hawk_sh_geterrbmsg(one), "input failure 91", 0) == 0,
			   cases[n].name);
		}
	}
	{
		failing_input_t in = { HAWK_T("x \"text"), HAWK_EECERR, 0, 0 };

		OK(hawk_sh_exec(one, failing_input, &in) < 0 && hawk_sh_geterrnum(one) == HAWK_EECERR,
		   "encoding error inside a quote is not replaced with EOF");
		in.ptr = HAWK_T("x \\");
		in.errnum = HAWK_ENOERR;
		in.failures = 0;
		OK(hawk_sh_exec(one, failing_input, &in) < 0 && hawk_sh_geterrnum(one) == HAWK_EREAD &&
		   hawk_sh_geterrloc(one)->line == 1 && in.failures == 1,
		   "input failure without an error supplies the default read error");
		in.ptr = HAWK_T("x \"");
		in.nul = 1;
		in.failures = 0;
		OK(hawk_sh_exec(one, failing_input, &in) < 0 && hawk_sh_geterrnum(one) == HAWK_EREAD &&
		   in.nul == 0 && in.failures == 1,
		   "NUL inside a quote is accepted and subsequent read error is preserved");
	}
	OK(hawk_sh_execbcs(one, "true") == 0 && hawk_sh_geterrnum(one) == HAWK_ENOERR,
	   "shell remains reusable and clears the error after failed reads");

	hawk_sh_close(one);
	hawk_sh_close(two);

	for (i = 1; i <= 130; i++)
	{
		allocator_t state = { 0, i, 0, 0 };
		hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &state };
		hawk_sh_t* sh = hawk_sh_open(&mmgr, 0, HAWK_NULL, HAWK_NULL);
		if (sh) { hawk_sh_setprm(sh, &prm, &a); hawk_sh_execbcs(sh, "addcmd copy /bin/cat; x a 2>file | y <<EOF\nbody\nEOF\n"); hawk_sh_close(sh); }
		if (state.live) leaks++;
	}
	OK(leaks == 0, "allocation failures clean up partially built commands");

	leaks = 0;
	for (i = 1; i <= 130; i++)
	{
		allocator_t state = { 0, i, 0, 1 };
		hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &state };
		hawk_sh_t* sh = hawk_sh_open(&mmgr, 0, HAWK_NULL, HAWK_NULL);

		if (sh)
		{
			a.envname = HAWK_T("LONG");
			a.envvalue = HAWK_T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz");
			hawk_sh_setprm(sh, &prm, &a);
			if (hawk_sh_execbcs(sh, "x \"$LONG$UNSET$LONG\" '' | y <<EOF\n$LONG\n\nx\nEOF\n") < 0)
			{
				failures++;
				if (hawk_sh_geterrnum(sh) != HAWK_ENOMEM) wrong_errors++;
			}
			hawk_sh_execbcs(sh, "x '${broken' ${broken");
			hawk_sh_close(sh);
		}
		if (state.live) leaks++;
	}
	OK(leaks == 0, "persistent allocation failures release expansion and heredoc string buffers");
	OK(failures > 0 && wrong_errors == 0, "string buffer allocation failures preserve ENOMEM");
	return exit_status();
}
