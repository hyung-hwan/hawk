/* POSIX backend, embedding isolation, callback streams, and pio integration. */
#include <hawk-sh.h>
#include <hawk-pio.h>
#include <hawk-str.h>
#include "tap.h"
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct capture_t capture_t;

struct capture_t
{
	hawk_ooch_t text[200];
	int calls;
};

static int capture (hawk_sh_t* sh, hawk_oow_t argc, const hawk_ooch_t* const* argv, void* ctx)
{
	capture_t* c = ctx;
	c->calls++;
	if (argc > 1) hawk_copy_oocstr(c->text, HAWK_COUNTOF(c->text), argv[1]);
	return 0;
}

static int emit (hawk_sh_t* sh, hawk_oow_t argc, const hawk_ooch_t* const* argv, void* ctx)
{
	return hawk_sh_write(sh, 1, "callback\n", 9) == 9? 0: 1;
}


static int consume (hawk_sh_t* sh, hawk_oow_t argc, const hawk_ooch_t* const* argv, void* ctx)
{
	char buf[64];
	hawk_ooi_t n;

	while ((n = hawk_sh_read(sh, 0, buf, sizeof(buf))) > 0)
	{
		if (hawk_sh_write(sh, 1, buf, n) != n) return 1;
	}

	return n < 0? 1: 0;
}

static int check_group (hawk_sh_t* sh, hawk_oow_t argc, const hawk_ooch_t* const* argv, void* ctx)
{
	return getpgrp() == *(pid_t*)ctx? 0: 1;
}

static void* environment (hawk_pio_env_mk_type_t type, void* ctx)
{
	static char* vars[] = { "PATH=/bin:/usr/bin", "HAWK_SH_PIO_VALUE=from-pio", HAWK_NULL };
	return type == HAWK_PIO_ENV_MK_BPP? vars: HAWK_NULL;
}

static int pipe_output (hawk_sh_t* sh, const hawk_ooch_t* command, int flags, const char* expected, hawk_pio_env_mk_t env)
{
	hawk_pio_t* pio;

	char buf[300];
	hawk_oow_t count = 0;
	hawk_ooi_t n;
	int status;

	pio = hawk_pio_open(hawk_sh_getgem(sh), 0, command, flags | HAWK_PIO_HAWKSH | HAWK_PIO_READOUT, env, HAWK_NULL);
	if (!pio) return 0;

	while (count < sizeof(buf) - 1 && (n = hawk_pio_readbytes(pio, HAWK_PIO_OUT, buf + count, sizeof(buf) - count - 1)) > 0) count += n;
	buf[count] = 0;
	status = hawk_pio_wait(pio);
	hawk_pio_close(pio);

	if (status != 0 || strcmp(buf, expected) != 0)
		fprintf(stderr, "# pio status=%d, bytes=%lu, output=<%s>\n", status, (unsigned long)count, buf);
	return status == 0 && strcmp(buf, expected) == 0;
}
#endif


int main (void)
{
#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
	printf("1..0 # SKIP POSIX shell backend unavailable\n");
	return 0;
#else
	hawk_sh_t* one;
	hawk_sh_t* two;
	capture_t a = { { 0 }, 0 }, b = { { 0 }, 0 };
	char before[4096], after[4096];
	int fd_before, fd_after;
	pid_t group = getpgrp();


	/* Instrumented fork/exit paths must not replay buffered TAP into pipes. */
	setvbuf(stdout, HAWK_NULL, _IONBF, 0);
	no_plan();

	one = hawk_sh_openstd(HAWK_NULL, 0, HAWK_NULL, HAWK_NULL);
	two = hawk_sh_openstd(HAWK_NULL, 0, HAWK_NULL, HAWK_NULL);
	OK(one && two, "open independent standard shells");
	if (!one || !two) return 1;

	hawk_sh_addcmd(one, HAWK_T("capture"), HAWK_NULL, capture, &a);
	hawk_sh_addcmd(two, HAWK_T("capture"), HAWK_NULL, capture, &b);
	getcwd(before, sizeof(before));
	OK(hawk_sh_execbcs(one, "cd /; export HAWK_SH_PRIVATE=one; capture \"$HAWK_SH_PRIVATE\"") == 0 &&
	   hawk_comp_oocstr(a.text, HAWK_T("one"), 0) == 0, "callback sees per-instance environment");
	OK(hawk_sh_execbcs(two, "capture \"$HAWK_SH_PRIVATE\"") == 0 && b.text[0] == 0, "other instance's environment unchanged");

	getcwd(after, sizeof(after));
	OK(strcmp(before, after) == 0 && !getenv("HAWK_SH_PRIVATE"), "parent cwd/environment unchanged");
	fd_before = dup(1);
	OK(hawk_sh_execbcs(one, "capture redirected > /dev/null") == 0 && a.calls == 2, "foreground callback executes in parent");
	fd_after = dup(1);
	OK(fd_before >= 0 && fd_after >= 0, "parent stdout remains open");
	close(fd_before);
	close(fd_after);


	hawk_sh_addcmd(one, HAWK_T("emit"), HAWK_NULL, emit, HAWK_NULL);
	hawk_sh_addcmd(one, HAWK_T("consume"), HAWK_NULL, consume, HAWK_NULL);
	OK(hawk_sh_execbcs(one, "emit | consume > /dev/null") == 0, "registered callbacks run as concurrent pipeline stages");
	hawk_sh_addcmd(one, HAWK_T("check_group"), HAWK_NULL, check_group, &group);
	OK(hawk_sh_execbcs(one, "emit | check_group") == 0, "batch foreground stages inherit caller process group");

	hawk_sh_setmode(one, HAWK_SH_REGISTERED);
	OK(hawk_sh_execbcs(one, "capture allowed") == 0, "registered mode supports callbacks");
	OK(pipe_output(two, HAWK_T("echo abc | tr a-z A-Z"), 0, "ABC\n", HAWK_NULL), "pio runs Hawk shell rather than /bin/sh");
	OK(pipe_output(two, (const hawk_ooch_t*)"echo bytes", HAWK_PIO_BCSTRCMD, "bytes\n", HAWK_NULL), "pio byte-string command flag");
	OK(pipe_output(two, HAWK_T("echo \"$HAWK_SH_PIO_VALUE\""), 0, "from-pio\n", environment), "pio passes explicit environment to embedded shell");
	OK(pipe_output(two, HAWK_T("cat <<EOF\nheredoc\nEOF\n"), 0, "heredoc\n", HAWK_NULL), "pio embedded-shell heredoc");

	{
		hawk_pio_t* pio = hawk_pio_open(hawk_sh_getgem(two), 0, HAWK_T("cat | cat"),
			HAWK_PIO_HAWKSH | HAWK_PIO_READOUT | HAWK_PIO_WRITEIN, HAWK_NULL, HAWK_NULL);
		char buf[16];
			hawk_ooi_t n = -1;
			hawk_oow_t count = 0;
		int status = -1;
		if (pio)
		{
			hawk_pio_writebytes(pio, HAWK_PIO_IN, "input\n", 6);
			hawk_pio_end(pio, HAWK_PIO_IN);
				while (count < sizeof(buf) && (n = hawk_pio_readbytes(pio, HAWK_PIO_OUT, buf + count, sizeof(buf) - count)) > 0) count += n;
			status = hawk_pio_wait(pio);
			hawk_pio_close(pio);
		}
			OK(count == 6 && n == 0 && memcmp(buf, "input\n", 6) == 0 && status == 0, "pio input and output through embedded pipeline");
	}
	hawk_sh_close(one); hawk_sh_close(two);

	{
		hawk_sh_prm_t custom = { 0 };
		hawk_sh_t* sh = hawk_sh_openstd(HAWK_NULL, 0, HAWK_NULL, HAWK_NULL);
		if (sh) { hawk_sh_setprm(sh, &custom, &a); hawk_sh_close(sh); }
		OK(sh != HAWK_NULL, "replacing callbacks preserves standard backend ownership");
	}
	return exit_status();
#endif
}
