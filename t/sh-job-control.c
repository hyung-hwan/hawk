/* Terminal-level regressions invoked by regress-sh-job-control.sh. */
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || \
    defined(__NetBSD__) || defined(__sun) || defined(__APPLE__)
#define HAVE_TEST_PTY 1
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(__APPLE__)
#include <util.h>
#endif

typedef struct terminal_t terminal_t;
struct terminal_t
{
	int master;
	pid_t pid;
	pid_t foreground;
	char output[65536];
};

static double now (void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void pause_briefly (void)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 20000;
	select(0, NULL, NULL, NULL, &tv);
}

static int receive_until (terminal_t* terminal, const char* needle)
{
	double deadline = now() + 10.0;
	size_t len = 0;

	terminal->output[0] = '\0';
	while (!strstr(terminal->output, needle))
	{
		fd_set fds;
		struct timeval tv;
		double remaining = deadline - now();
		ssize_t n;
		int ready;

		if (remaining <= 0 || len == sizeof(terminal->output) - 1) return -1;
		tv.tv_sec = (long)remaining;
		tv.tv_usec = (long)((remaining - tv.tv_sec) * 1000000.0);
		FD_ZERO(&fds);
		FD_SET(terminal->master, &fds);
		ready = select(terminal->master + 1, &fds, NULL, NULL, &tv);
		if (ready < 0 && errno == EINTR) continue;
		if (ready <= 0) return -1;
		n = read(terminal->master, terminal->output + len, sizeof(terminal->output) - len - 1);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) return -1;
		len += n;
		terminal->output[len] = '\0';
	}
	return 0;
}

static int send_text (terminal_t* terminal, const char* text)
{
	size_t len = strlen(text);

	while (len > 0)
	{
		ssize_t n = write(terminal->master, text, len);

		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) return -1;
		text += n;
		len -= n;
	}
	return 0;
}

static int command (terminal_t* terminal, const char* text)
{
	if (send_text(terminal, text) < 0) return -1;
	return receive_until(terminal, "$ ");
}

static int wait_for_exit (terminal_t* terminal, double timeout)
{
	double deadline = now() + timeout;
	int reading = terminal->master >= 0;

	while (now() < deadline)
	{
		int status;
		pid_t pid = waitpid(terminal->pid, &status, WNOHANG);

		if (pid == terminal->pid)
		{
			terminal->pid = 0;
			return WIFEXITED(status) && WEXITSTATUS(status) == 0? 0: -1;
		}
		if (pid < 0 && errno != EINTR) return -1;
		if (reading)
		{
			fd_set fds;
			struct timeval tv;
			char buf[1024];
			ssize_t n;
			int ready;

			/* Keep consuming output while the shell exits. On FreeBSD,
			 * restoring terminal modes with TCSADRAIN waits for unread PTY
			 * output (including the echoed exit command) to be drained. */
			FD_ZERO(&fds);
			FD_SET(terminal->master, &fds);
			tv.tv_sec = 0;
			tv.tv_usec = 20000;
			ready = select(terminal->master + 1, &fds, NULL, NULL, &tv);
			if (ready <= -1)
			{
				if (errno == EINTR) continue;
				return -1;
			}
			if (ready == 0) continue;
			n = read(terminal->master, buf, sizeof(buf));
			if (n <= -1)
			{
				if (errno == EINTR) continue;
				/* Some PTYs report EIO instead of EOF after slave closure. */
				if (errno != EIO) return -1;
				reading = 0;
			}
			else if (n == 0) reading = 0;
		}
		else pause_briefly();
	}
	return -1;
}

static int open_terminal (terminal_t* terminal, const char* hawk)
{
	int slave;
	struct termios modes;

#if defined(__APPLE__)
	if (openpty(&terminal->master, &slave, NULL, NULL, NULL) < 0) return 77;
#else
	char* name;

	terminal->master = posix_openpt(O_RDWR | O_NOCTTY);
	if (terminal->master < 0) return 77;
	if (grantpt(terminal->master) < 0 || unlockpt(terminal->master) < 0) return 77;
	name = ptsname(terminal->master);
	if (!name) return 77;
	slave = open(name, O_RDWR | O_NOCTTY);
	if (slave < 0) return 77;
#endif
	if (terminal->master >= FD_SETSIZE)
	{
		close(slave);
		return 77;
	}
	if (tcgetattr(slave, &modes) < 0)
	{
		close(slave);
		return 77;
	}
	/* Explicit terminal settings make control characters and line endings stable. */
	modes.c_lflag |= ICANON | ISIG | ECHO;
	modes.c_oflag |= OPOST | ONLCR;
	modes.c_cc[VINTR] = 3;
	modes.c_cc[VSUSP] = 26;
	if (tcsetattr(slave, TCSANOW, &modes) < 0)
	{
		close(slave);
		return 77;
	}
	terminal->pid = fork();
	if (terminal->pid == 0)
	{
		close(terminal->master);
		if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) < 0 ||
		    tcsetpgrp(slave, getpgrp()) < 0 ||
		    dup2(slave, 0) < 0 || dup2(slave, 1) < 0 || dup2(slave, 2) < 0) _exit(125);
		if (slave > 2) close(slave);
		execlp(hawk, hawk, "--sh", "-i", (char*)NULL);
		_exit(127);
	}
	close(slave);
	if (terminal->pid < 0)
	{
		terminal->pid = 0;
		return -1;
	}
	return 0;
}

static void close_terminal (terminal_t* terminal)
{
	if (terminal->pid > 0)
	{
		/* Let the shell reap its jobs; kill a stuck foreground group first. */
		if (terminal->foreground > 0 && terminal->foreground != terminal->pid)
			kill(-terminal->foreground, SIGKILL);
		send_text(terminal, "exit\n");
		if (wait_for_exit(terminal, 2.0) < 0 && terminal->pid > 0)
		{
			kill(terminal->pid, SIGKILL);
			while (waitpid(terminal->pid, NULL, 0) < 0 && errno == EINTR) {}
		}
	}
	if (terminal->master >= 0) close(terminal->master);
}

static int run_tests (terminal_t* terminal)
{
	double deadline;
	pid_t group;

	if (receive_until(terminal, "$ ") < 0 ||
	    send_text(terminal, "sh -c 'echo READY; sleep 30' | cat\n") < 0 ||
	    receive_until(terminal, "READY\r\n") < 0) return -1;
	terminal->foreground = tcgetpgrp(terminal->master);
	if (terminal->foreground <= 0 || terminal->foreground == terminal->pid) return -1;
	if (send_text(terminal, "\032") < 0 || receive_until(terminal, "$ ") < 0 ||
	    command(terminal, "jobs\n") < 0 || !strstr(terminal->output, "Stopped")) return -1;
	puts("ok 1 - Ctrl-Z stops foreground pipeline and returns terminal");

	if (command(terminal, "bg %1\n") < 0 || command(terminal, "jobs\n") < 0 ||
	    !strstr(terminal->output, "Running")) return -1;
	puts("ok 2 - bg resumes stopped process group");

	if (send_text(terminal, "fg %1\n") < 0) return -1;
	/* Wait for terminal handoff rather than assuming a fixed scheduling delay. */
	deadline = now() + 10.0;
	do
	{
		group = tcgetpgrp(terminal->master);
		if (group == terminal->foreground) break;
		if (group < 0 || now() >= deadline) return -1;
		pause_briefly();
	}
	while (1);
	if (send_text(terminal, "\003") < 0 || receive_until(terminal, "$ ") < 0 ||
	    command(terminal, "echo \"$?\"\n") < 0 || !strstr(terminal->output, "\r\n130\r\n")) return -1;
	terminal->foreground = 0;
	puts("ok 3 - fg and Ctrl-C kill the pipeline without killing shell");

	if (command(terminal, "cat &\n") < 0) return -1;
	deadline = now() + 10.0;
	do
	{
		if (command(terminal, "jobs\n") < 0) return -1;
		if (strstr(terminal->output, "Stopped")) break;
		if (now() >= deadline) return -1;
		pause_briefly();
	}
	while (1);
	puts("ok 4 - background terminal reader stops on SIGTTIN");

	if (command(terminal, "echo >\n") < 0 || !strstr(terminal->output, "line ") ||
	    !strstr(terminal->output, "missing redirection operand") ||
	    command(terminal, "echo RECOVERED\n") < 0 || !strstr(terminal->output, "\r\nRECOVERED\r\n")) return -1;
	puts("ok 5 - syntax error doesn't discard the next interactive command");
	if (send_text(terminal, "exit\n") < 0) return -1;
	return wait_for_exit(terminal, 10.0);
}
#endif

int main (int argc, char* argv[])
{
#if defined(HAVE_TEST_PTY)
	terminal_t terminal;
	int n;

	if (argc != 2)
	{
		puts("Bail out! expected Hawk executable argument");
		return 1;
	}
	memset(&terminal, 0, sizeof(terminal));
	terminal.master = -1;
	setvbuf(stdout, NULL, _IONBF, 0);
	n = open_terminal(&terminal, argv[1]);
	if (n == 77)
	{
		puts("1..0 # SKIP pseudo-terminal unavailable");
		close_terminal(&terminal);
		return 0;
	}
	puts("1..5");
	if (n == 0) n = run_tests(&terminal);
	if (n < 0)
	{
		puts("Bail out! terminal interaction failed");
		fprintf(stderr, "# last terminal output: <%s>\n", terminal.output);
	}
	close_terminal(&terminal);
	return n < 0? 1: 0;
#else
	(void)argc;
	(void)argv;
	puts("1..0 # SKIP pseudo-terminal backend unavailable");
	return 0;
#endif
}
