/* Deterministic OS failures and resource checks for the sys module. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hawk-prv.h"
#include <hawk-pio.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

static int socket_calls;
static int setsockopt_calls;
static int shutdown_calls;
static int listen_calls;
static int accept_calls;
static int shutdown_how;
static int listen_backlog;
static int log_time;
static int log_timestamp;
static int log_tz_mode;
static struct tm log_tz_tm;
static int native_log_options;
static int native_log_calls;
static int log_mock_send;
static int log_fail_send;
static int log_send_calls;
static hawk_bch_t log_message[2048];
static hawk_oow_t log_message_len;
static int settime_calls;
static int settime_success;
static hawk_ntime_t settime_value;
static int system_wait_mode;
static int system_result_oom;
static int mktime_mode;
static int file_close_error;
static int file_close_calls;

extern void hawk_test_fail_next_allocation (hawk_rtx_t* rtx);

static int log_probe (hawk_rtx_t* rtx, const hawk_bch_t* name, hawk_int_t* n)
{
	if (hawk_comp_bcstr(name, "file-close-eio", 0) == 0) file_close_error = EIO;
	else if (hawk_comp_bcstr(name, "file-close-eintr", 0) == 0) file_close_error = EINTR;
	else if (hawk_comp_bcstr(name, "file-close-normal", 0) == 0) file_close_error = 0;
	else if (hawk_comp_bcstr(name, "file-close-calls", 0) == 0) *n = file_close_calls;
	else if (hawk_comp_bcstr(name, "settime-calls", 0) == 0) *n = settime_calls;
	else if (hawk_comp_bcstr(name, "system-max-status", 0) == 0) system_wait_mode = 1;
	else if (hawk_comp_bcstr(name, "system-result-oom", 0) == 0) system_wait_mode = 2;
	else if (hawk_comp_bcstr(name, "system-wait-error", 0) == 0) system_wait_mode = 3;
	else if (hawk_comp_bcstr(name, "settime-success", 0) == 0) settime_success = 1;
	else if (hawk_comp_bcstr(name, "settime-seconds", 0) == 0) *n = settime_value.sec;
	else if (hawk_comp_bcstr(name, "settime-nanoseconds", 0) == 0) *n = settime_value.nsec;
	else if (hawk_comp_bcstr(name, "time-stale-errno", 0) == 0) errno = EACCES;
	else if (hawk_comp_bcstr(name, "mktime-failure", 0) == 0) mktime_mode = 1;
	else if (hawk_comp_bcstr(name, "mktime-minus-one", 0) == 0) mktime_mode = 2;
	else if (hawk_comp_bcstr(name, "mktime-stale-errno", 0) == 0) mktime_mode = 3;
	else if (hawk_comp_bcstr(name, "mktime-no-errno", 0) == 0) mktime_mode = 4;
	else if (hawk_comp_bcstr(name, "mktime-minus-one-errno", 0) == 0) mktime_mode = 5;
	else if (hawk_comp_bcstr(name, "mktime-time-size", 0) == 0) *n = HAWK_SIZEOF(time_t);
	else if (hawk_comp_bcstr(name, "mktime-normal", 0) == 0) mktime_mode = 0;
	else if (hawk_comp_bcstr(name, "log-mock", 0) == 0) { log_time = log_mock_send = 1; }
	else if (hawk_comp_bcstr(name, "log-real", 0) == 0) { log_time = 1; log_mock_send = 0; }
	else if (hawk_comp_bcstr(name, "log-time-errno", 0) == 0) log_timestamp = 1;
	else if (hawk_comp_bcstr(name, "log-time-no-errno", 0) == 0) log_timestamp = 2;
	else if (hawk_comp_bcstr(name, "log-time-stale-errno", 0) == 0) log_timestamp = 3;
	else if (hawk_comp_bcstr(name, "log-time-normal", 0) == 0) log_timestamp = 0;
	else if (hawk_comp_bcstr(name, "log-tz-east", 0) == 0) log_tz_mode = 1;
	else if (hawk_comp_bcstr(name, "log-tz-west", 0) == 0) log_tz_mode = 2;
	else if (hawk_comp_bcstr(name, "log-tz-zero", 0) == 0) log_tz_mode = 3;
	else if (hawk_comp_bcstr(name, "log-tz-dst", 0) == 0) log_tz_mode = 4;
	else if (hawk_comp_bcstr(name, "log-tz-leap-year", 0) == 0) log_tz_mode = 5;
	else if (hawk_comp_bcstr(name, "log-tz-utc-error", 0) == 0) log_tz_mode = 6;
	else if (hawk_comp_bcstr(name, "log-tz-utc-no-errno", 0) == 0) log_tz_mode = 7;
	else if (hawk_comp_bcstr(name, "log-tz-subminute", 0) == 0) log_tz_mode = 8;
	else if (hawk_comp_bcstr(name, "log-tz-normal", 0) == 0) log_tz_mode = 0;
	else if (hawk_comp_bcstr(name, "native-log-options", 0) == 0) *n = native_log_options;
	else if (hawk_comp_bcstr(name, "native-log-calls", 0) == 0) *n = native_log_calls;
	else if (hawk_comp_bcstr(name, "log-send-failure", 0) == 0) log_fail_send = 1;
	else if (hawk_comp_bcstr(name, "log-send-calls", 0) == 0) *n = log_send_calls;
	else if (hawk_comp_bcstr(name, "log-message", 0) == 0)
	{
		hawk_val_t* val = hawk_rtx_makembsvalwithbchars(rtx, log_message, log_message_len);
		if (!val) return -1;
		hawk_rtx_setretval(rtx, val);
		return 2;
	}
	else return 0;
	return 1;
}

static void reset_log_probe (void)
{
	log_time = log_mock_send = log_fail_send = log_send_calls = 0;
	log_timestamp = 0;
	log_tz_mode = native_log_options = native_log_calls = 0;
	log_message_len = 0;
	settime_calls = settime_success = 0;
	system_wait_mode = system_result_oom = 0;
	mktime_mode = 0;
	file_close_error = file_close_calls = 0;
	HAWK_CLEAR_NTIME(&settime_value);
}

#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
int hawk_test_sys_owner32_narrow (void) { return 0; }
int hawk_test_sys_mode32_narrow (void) { return 0; }
int hawk_test_sys_tcflag32_narrow (void) { return 0; }
#endif

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syslog.h>
#include <netinet/in.h>
#if defined(HAVE_SYS_UN_H)
#include <sys/un.h>
#endif
#include <termios.h>
#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
#include <sys/epoll.h>
static struct epoll_event last_mux_event;
static struct epoll_event saved_mux_event;
static int inject_mux_event;
static int fail_mux_delete;
static int fail_mux_create;
static int fail_mux_add;
static int mux_wait_calls;
static int mux_delete_calls;
static int mux_add_calls;
static int expected_cleanup_delete_calls;

static int test_epoll_ctl (int fd, int cmd, int target, struct epoll_event* ev)
{
	int n;

	if (cmd == EPOLL_CTL_DEL) mux_delete_calls++;
	else if (cmd == EPOLL_CTL_ADD) mux_add_calls++;
	if ((cmd == EPOLL_CTL_DEL && fail_mux_delete) || (cmd == EPOLL_CTL_ADD && fail_mux_add))
	{
		errno = EACCES;
		return -1;
	}
	n = epoll_ctl(fd, cmd, target, ev);
	if (n == 0 && cmd == EPOLL_CTL_ADD) last_mux_event = *ev;
	return n;
}

static int test_epoll_wait (int fd, struct epoll_event* events, int maxevents, int timeout)
{
	mux_wait_calls++;
	if (inject_mux_event)
	{
		inject_mux_event = 0;
		events[0] = saved_mux_event;
		return 1;
	}
	return epoll_wait(fd, events, maxevents, timeout);
}

#if !defined(HAVE_EPOLL_CREATE1) || !defined(EPOLL_CLOEXEC)
static int test_epoll_create (int size)
{
	if (fail_mux_create) { errno = EACCES; return -1; }
	return epoll_create(size);
}
#endif

#if defined(HAVE_EPOLL_CREATE1) && defined(EPOLL_CLOEXEC)
static int test_epoll_create1 (int flags)
{
	if (fail_mux_create) { errno = EACCES; return -1; }
	return epoll_create1(flags);
}
#endif
#endif

static int raw_pipe[2] = { -1, -1 };
static int accepted_fd = -1;
static int log_fd = -1;
static int socket_target_fd = -1;
static int fail_fcntl = 0;
static int fail_fcntl_cmd = -1;
static int fail_termios = 0;
static int tcsetattr_calls;
static int tcflush_calls;
static int fail_sleep = 0;
static int sleep_calls = 0;
static int timeout_ok = 0;
static struct timeval socket_timeout;
static int initial_fds;
static int pipe_fds[2] = { -1, -1 };
static int pipe_calls;
static int recv_no_address = 0;
static int recv_peer_mode;
static int wait_calls = 0;
static pid_t wait_pid;
static int wait_opts;
static int flock_calls = 0;
static int kill_calls = 0;
static pid_t kill_pid;
static int kill_sig;
static int dup_calls;
static int fchown_calls;
static uid_t owner_uid;
static gid_t owner_gid;
static int accept_peer_mode;
static int bind_calls;
static int connect_calls;

int hawk_test_sys_mode32_narrow (void)
{
	return HAWK_SIZEOF(mode_t) <= 4 && HAWK_SIZEOF(hawk_int_t) > 4;
}

int hawk_test_sys_tcflag32_narrow (void)
{
	return HAWK_SIZEOF(tcflag_t) == 4 && HAWK_SIZEOF(hawk_int_t) > 4;
}

int hawk_test_sys_owner32_narrow (void)
{
	return HAWK_SIZEOF(uid_t) == 4 && HAWK_SIZEOF(gid_t) == 4 && HAWK_SIZEOF(hawk_int_t) > 4 &&
	       HAWK_TYPE_IS_UNSIGNED(uid_t) && HAWK_TYPE_IS_UNSIGNED(gid_t);
}

int hawk_test_sys_pid32_narrow (void)
{
	return HAWK_SIZEOF(pid_t) == 4 && HAWK_SIZEOF(hawk_int_t) > 4;
}

int hawk_test_sys_offset32_narrow (void)
{
	return HAWK_SIZEOF(off_t) == 4 && HAWK_SIZEOF(hawk_int_t) > 4;
}

int hawk_test_sys_fd_flags_available (void)
{
#if defined(O_NONBLOCK) && defined(O_CLOEXEC) && defined(FD_CLOEXEC) && defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
	return 1;
#else
	return 0;
#endif
}

static int count_fds (void)
{
	int i, n = 0;
	/* Fixtures allocate only a handful of descriptors. No /proc dependency. */
	for (i = 0; i < 1024; i++) if (fcntl(i, F_GETFD) >= 0) n++;
	return n;
}

#if defined(O_NONBLOCK) && defined(FD_CLOEXEC)
static int fd_has_flags (int fd)
{
	int fl, fdflags;
	fl = fcntl(fd, F_GETFL);
	if (fl <= -1) return 0;
	fdflags = fcntl(fd, F_GETFD);
	return fdflags >= 0 && (fl & O_NONBLOCK) && (fdflags & FD_CLOEXEC);
}
#endif

int hawk_test_sys_begin (void)
{
	/* A broken mux list or timeout must not hang the rest of the suite. */
	alarm(10);
	initial_fds = count_fds();
	reset_log_probe();
	accepted_fd = log_fd = -1;
	socket_target_fd = -1;
	fail_fcntl = timeout_ok = 0;
	HAWK_MEMSET(&socket_timeout, 0, HAWK_SIZEOF(socket_timeout));
	fail_fcntl_cmd = -1;
	fail_termios = tcsetattr_calls = tcflush_calls = 0;
	fail_sleep = sleep_calls = 0;
	pipe_fds[0] = pipe_fds[1] = -1;
	pipe_calls = 0;
	recv_no_address = kill_calls = wait_calls = flock_calls = 0;
	recv_peer_mode = 0;
	accept_peer_mode = bind_calls = connect_calls = 0;
	socket_calls = setsockopt_calls = 0;
	shutdown_calls = listen_calls = accept_calls = dup_calls = fchown_calls = 0;
	shutdown_how = listen_backlog = 0;
	#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
	HAWK_MEMSET(&last_mux_event, 0, HAWK_SIZEOF(last_mux_event));
	HAWK_MEMSET(&saved_mux_event, 0, HAWK_SIZEOF(saved_mux_event));
	inject_mux_event = fail_mux_delete = fail_mux_create = fail_mux_add = mux_wait_calls = 0;
	mux_delete_calls = mux_add_calls = 0;
	expected_cleanup_delete_calls = -1;
	#endif
	kill_pid = 0;
	kill_sig = 0;
	return 0;
}

int hawk_test_sys_end (void)
{
	int n;
	alarm(0);
	if (raw_pipe[0] >= 0) close(raw_pipe[0]);
	if (raw_pipe[1] >= 0) close(raw_pipe[1]);
	raw_pipe[0] = raw_pipe[1] = -1;
	n = count_fds() - initial_fds;
	if (n != 0) fprintf(stderr, "%d file descriptors leaked after runtime close\n", n);
	#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
	if (expected_cleanup_delete_calls >= 0 && mux_delete_calls != expected_cleanup_delete_calls)
	{
		fprintf(stderr, "runtime cleanup unexpectedly deleted a mux registration\n");
		return -1;
	}
	#endif
	return n == 0? 0: -1;
}

static pid_t unfinished_wait (pid_t pid, int* status, int options)
{
	/* waitpid(WNOHANG) leaves status untouched when returning zero. */
	wait_calls++;
	wait_pid = pid;
	wait_opts = options;
	return 0;
}

static int test_socket (int domain, int type, int protocol)
{
	socket_calls++;
	/* Use an ordinary fd: these tests check conversion/ownership, not networking. */
	if (fail_fcntl && (type & ~(SOCK_DGRAM | SOCK_STREAM)))
	{
		errno = EINVAL;
		return -1;
	}
	log_fd = open("/dev/null", O_RDWR);
	if (log_fd >= 0 && socket_target_fd >= 0)
	{
		if (log_fd != socket_target_fd)
		{
			int fd = dup2(log_fd, socket_target_fd);
			int err = errno;
			close(log_fd);
			log_fd = fd;
			errno = err;
		}
		socket_target_fd = -1;
	}
	return log_fd;
}

static int test_setsockopt (int fd, int level, int option, const void* ptr, socklen_t len)
{
	const struct timeval* tv = ptr;
	setsockopt_calls++;
	if ((option == SO_RCVTIMEO || option == SO_SNDTIMEO) && len == sizeof(*tv))
	{
		socket_timeout = *tv;
		timeout_ok = tv->tv_sec == 0 && tv->tv_usec == 500000;
	}
	return 0;
}

static ssize_t test_sendto (int fd, const void* ptr, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen)
{
	if (!log_mock_send) return sendto(fd, ptr, len, flags, addr, addrlen);
	log_send_calls++;
	if (log_fail_send) { errno = EAGAIN; return -1; }
	if (len > HAWK_COUNTOF(log_message)) { errno = EMSGSIZE; return -1; }
	HAWK_MEMCPY(log_message, ptr, len);
	log_message_len = len;
	return len;
}

static int test_accept (int fd, struct sockaddr* addr, socklen_t* len)
{
	struct sockaddr_in peer;
	accept_calls++;
	if (accept_peer_mode == 1)
	{
		/* A zero-length result must not expose stale bytes supplied by the fixture. */
		HAWK_MEMSET(addr, 'X', *len);
		*len = 0;
	}
#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
	else if (accept_peer_mode >= 2)
	{
		struct sockaddr_un un;
		hawk_oow_t size = HAWK_SIZEOF(un);

		HAWK_MEMSET(&un, 'X', size);
		un.sun_family = AF_UNIX;
		HAWK_MEMCPY(addr, &un, size);
		if (accept_peer_mode == 2) *len = HAWK_OFFSETOF(struct sockaddr_un, sun_path);
		else if (accept_peer_mode == 3)
		{
			HAWK_MEMCPY(((struct sockaddr_un*)addr)->sun_path, "peer", 4);
			*len = HAWK_OFFSETOF(struct sockaddr_un, sun_path) + 4;
		}
		else
		{
			HAWK_MEMSET(((struct sockaddr_un*)addr)->sun_path, 'p', HAWK_SIZEOF(un.sun_path));
			*len = size;
		}
	}
#endif
	else
	{
		HAWK_MEMSET (&peer, 0, HAWK_SIZEOF(peer));
		peer.sin_family = AF_INET;
		HAWK_MEMCPY(addr, &peer, HAWK_SIZEOF(peer));
		*len = HAWK_SIZEOF(peer);
	}
	/* Use a real socket so nonblocking mode works on platforms whose
	 * /dev/null driver rejects F_SETFL (e.g. Haiku). No connection is needed. */
	accepted_fd = socket(AF_INET, SOCK_STREAM, 0);
	return accepted_fd;
}

static int test_bind (int fd, const struct sockaddr* addr, socklen_t len)
{
	bind_calls++;
	return 0;
}

static int test_connect (int fd, const struct sockaddr* addr, socklen_t len)
{
	connect_calls++;
	return 0;
}

static int test_shutdown (int fd, int how)
{
	shutdown_calls++;
	shutdown_how = how;
	return 0;
}

static int test_listen (int fd, int backlog)
{
	listen_calls++;
	listen_backlog = backlog;
	return 0;
}

static int test_fchown (int fd, uid_t uid, gid_t gid)
{
	fchown_calls++;
	owner_uid = uid;
	owner_gid = gid;
	return 0;
}

static int test_pipe (int* fds)
{
	int n;
	pipe_calls++;
	n = pipe(fds);
	if (n >= 0)
	{
		pipe_fds[0] = fds[0];
		pipe_fds[1] = fds[1];
	}
	return n;
}

static int test_kill (pid_t pid, int sig)
{
	/* Record the converted arguments without delivering any OS signal. */
	kill_calls++;
	kill_pid = pid;
	kill_sig = sig;
	return 0;
}

static ssize_t empty_datagram (int fd, void* buf, size_t size, int flags, struct sockaddr* addr, socklen_t* len)
{
	struct sockaddr_in peer;
#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
	if (recv_peer_mode)
	{
		struct sockaddr_un un;
		hawk_oow_t offset = HAWK_OFFSETOF(struct sockaddr_un, sun_path);

		/* Seed bytes beyond the returned length to expose unbounded formatting. */
		HAWK_MEMSET(&un, 'X', HAWK_SIZEOF(un));
		un.sun_family = AF_UNIX;
		if (recv_peer_mode == 1) *len = offset;
		else if (recv_peer_mode == 2)
		{
			HAWK_MEMCPY(un.sun_path, "peer", 4);
			*len = offset + 4;
		}
		else
		{
			HAWK_MEMSET(un.sun_path, 'p', HAWK_SIZEOF(un.sun_path));
			*len = offset + HAWK_SIZEOF(un.sun_path);
		}
		HAWK_MEMCPY(addr, &un, HAWK_SIZEOF(un));
		((char*)buf)[0] = 'Z';
		return 1;
	}
#endif
	HAWK_MEMSET (&peer, 0, HAWK_SIZEOF(peer));
	peer.sin_family = AF_INET;
	if (recv_no_address)
	{
		/* Seed stale address storage so ignoring a zero address length fails
		 * deterministically, without relying on stack layout or Valgrind. */
		peer.sin_port = htons(1234);
		peer.sin_addr.s_addr = htonl(0xcb007107); /* 203.0.113.7 */
		HAWK_MEMCPY(addr, &peer, HAWK_SIZEOF(peer));
		*len = 0;
		if (recv_no_address == 1) { ((char*)buf)[0] = 'Z'; return 1; }
		return 0;
	}
	HAWK_MEMCPY(addr, &peer, HAWK_SIZEOF(peer));
	*len = HAWK_SIZEOF(peer);
	return 0;
}

int hawk_test_sys_fcntl (int fd, int command, ...)
{
	int flags;
	va_list ap;

	if (fail_fcntl) { errno = (fail_fcntl == 2)? EINVAL: EBADF; return -1; }
	if (command == fail_fcntl_cmd) { errno = EINVAL; return -1; }
	switch (command)
	{
		case F_GETFD:
		case F_GETFL:
			return fcntl(fd, command);
		case F_SETFD:
		case F_SETFL:
			va_start(ap, command);
			flags = va_arg(ap, int);
			va_end(ap);
			return fcntl(fd, command, flags);
		default:
			flock_calls++;
			return 0;
	}
}

static int test_tcgetattr (int fd, struct termios* attr)
{
	if (fail_termios == 1) { errno = EINVAL; return -1; }
	HAWK_MEMSET(attr, 0, HAWK_SIZEOF(*attr));
	return 0;
}

static int test_tcsetattr (int fd, int action, const struct termios* attr)
{
	tcsetattr_calls++;
	if (fail_termios == 2) { errno = EINVAL; return -1; }
	return 0;
}

static int test_tcflush (int fd, int queue)
{
	tcflush_calls++;
	if (fail_termios == 3) { errno = EINVAL; return -1; }
	return 0;
}

#if defined(HAVE_NANOSLEEP)
static int test_nanosleep (const struct timespec* request, struct timespec* remaining)
{
	sleep_calls++;
	if (fail_sleep) { errno = fail_sleep; return -1; }
	return 0;
}
#elif defined(HAVE_SELECT)
static int test_select (int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)
{
	sleep_calls++;
	if (fail_sleep) { errno = fail_sleep; return -1; }
	return 0;
}
#endif

static int test_dup3 (int source, int target, int flags)
{
	dup_calls++;
	if (flags) { errno = EINVAL; return -1; }
	return dup2(source, target);
}

int hawk_test_sys_probe (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_bch_t* name;
	hawk_val_t* arg;
	hawk_int_t n = 0;
	int handled;

	arg = hawk_rtx_getarg(rtx, 0);
	name = hawk_rtx_getvalbcstr(rtx, arg, HAWK_NULL);
	if (!name) return -1;
	handled = log_probe(rtx, name, &n);
	if (handled != 0)
	{
		hawk_rtx_freevalbcstr(rtx, arg, name);
		if (handled < 0) return -1;
		if (handled != 2) hawk_rtx_setretval(rtx, hawk_rtx_makeintval_inline(rtx, n));
		return 0;
	}
	if (hawk_comp_bcstr(name, "rawfd", 0) == 0)
	{
		if (pipe(raw_pipe) <= -1) n = -1;
		else n = raw_pipe[0];
	}
	else if (hawk_comp_bcstr(name, "rawfd-open", 0) == 0) n = raw_pipe[0] >= 0 && fcntl(raw_pipe[0], F_GETFD) >= 0;
	else if (hawk_comp_bcstr(name, "rawfd-writer", 0) == 0) n = raw_pipe[1];
	else if (hawk_comp_bcstr(name, "rawfd-release-reader", 0) == 0)
	{
		n = raw_pipe[0] >= 0 && close(raw_pipe[0]) == 0? raw_pipe[0]: -1;
	}
	else if (hawk_comp_bcstr(name, "socket-at-rawfd", 0) == 0)
	{
		/* Force the mock socket onto a number imported before it is open. */
		n = raw_pipe[0] >= 0 && close(raw_pipe[0]) == 0? raw_pipe[0]: -1;
		socket_target_fd = n;
	}
	else if (hawk_comp_bcstr(name, "socket-fd", 0) == 0) n = log_fd;
	else if (hawk_comp_bcstr(name, "pipe-reader", 0) == 0) n = pipe_fds[0];
	else if (hawk_comp_bcstr(name, "rawfd-reuse", 0) == 0)
	{
		/* Reuse the closed reader's number without relying on allocation order. */
		if (raw_pipe[0] < 0 || raw_pipe[1] < 0) n = -1;
		else n = dup2(raw_pipe[1], raw_pipe[0]);
	}
	else if (hawk_comp_bcstr(name, "timeout", 0) == 0) n = timeout_ok;
	else if (hawk_comp_bcstr(name, "timeout-sec", 0) == 0) n = socket_timeout.tv_sec;
	else if (hawk_comp_bcstr(name, "timeout-usec", 0) == 0) n = socket_timeout.tv_usec;
	else if (hawk_comp_bcstr(name, "socket-calls", 0) == 0) n = socket_calls;
	else if (hawk_comp_bcstr(name, "setsockopt-calls", 0) == 0) n = setsockopt_calls;
	else if (hawk_comp_bcstr(name, "shutdown-calls", 0) == 0) n = shutdown_calls;
	else if (hawk_comp_bcstr(name, "listen-calls", 0) == 0) n = listen_calls;
	else if (hawk_comp_bcstr(name, "accept-calls", 0) == 0) n = accept_calls;
	else if (hawk_comp_bcstr(name, "shutdown-how", 0) == 0) n = shutdown_how;
	else if (hawk_comp_bcstr(name, "listen-backlog", 0) == 0) n = listen_backlog;
	else if (hawk_comp_bcstr(name, "dup-calls", 0) == 0) n = dup_calls;
	else if (hawk_comp_bcstr(name, "pipe-calls", 0) == 0) n = pipe_calls;
	else if (hawk_comp_bcstr(name, "fchown-calls", 0) == 0) n = fchown_calls;
	else if (hawk_comp_bcstr(name, "owner-uid", 0) == 0) n = owner_uid;
	else if (hawk_comp_bcstr(name, "owner-gid", 0) == 0) n = owner_gid;
	else if (hawk_comp_bcstr(name, "fail-fcntl", 0) == 0) fail_fcntl = 1;
	else if (hawk_comp_bcstr(name, "fail-fcntl-einval", 0) == 0) fail_fcntl = 2;
	else if (hawk_comp_bcstr(name, "fail-getfd", 0) == 0) fail_fcntl_cmd = F_GETFD;
	else if (hawk_comp_bcstr(name, "fail-setfd", 0) == 0) fail_fcntl_cmd = F_SETFD;
	else if (hawk_comp_bcstr(name, "fail-getfl", 0) == 0) fail_fcntl_cmd = F_GETFL;
	else if (hawk_comp_bcstr(name, "fail-setfl", 0) == 0) fail_fcntl_cmd = F_SETFL;
	else if (hawk_comp_bcstr(name, "accepted-closed", 0) == 0)
		n = accepted_fd >= 0 && fcntl(accepted_fd, F_GETFD) <= -1 && errno == EBADF;
	else if (hawk_comp_bcstr(name, "accept-no-address", 0) == 0) accept_peer_mode = 1;
#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
	else if (hawk_comp_bcstr(name, "accept-unix-unnamed", 0) == 0) accept_peer_mode = 2;
	else if (hawk_comp_bcstr(name, "accept-unix-short", 0) == 0) accept_peer_mode = 3;
	else if (hawk_comp_bcstr(name, "accept-unix-full", 0) == 0) accept_peer_mode = 4;
	else if (hawk_comp_bcstr(name, "recv-unix-unnamed", 0) == 0) recv_peer_mode = 1;
	else if (hawk_comp_bcstr(name, "recv-unix-short", 0) == 0) recv_peer_mode = 2;
	else if (hawk_comp_bcstr(name, "recv-unix-full", 0) == 0) recv_peer_mode = 3;
	else if (hawk_comp_bcstr(name, "unix-path-size", 0) == 0) n = HAWK_SIZEOF(((struct sockaddr_un*)0)->sun_path);
#endif
	else if (hawk_comp_bcstr(name, "bind-calls", 0) == 0) n = bind_calls;
	else if (hawk_comp_bcstr(name, "connect-calls", 0) == 0) n = connect_calls;
	else if (hawk_comp_bcstr(name, "pipe-closed", 0) == 0)
		n = pipe_fds[0] >= 0 && pipe_fds[1] >= 0 &&
		    fcntl(pipe_fds[0], F_GETFD) <= -1 && errno == EBADF &&
		    fcntl(pipe_fds[1], F_GETFD) <= -1 && errno == EBADF;
#if defined(O_NONBLOCK) && defined(FD_CLOEXEC)
	else if (hawk_comp_bcstr(name, "accepted-flags", 0) == 0)
		n = accepted_fd >= 0 && fd_has_flags(accepted_fd);
	else if (hawk_comp_bcstr(name, "pipe-flags", 0) == 0)
		n = pipe_fds[0] >= 0 && pipe_fds[1] >= 0 &&
		    fd_has_flags(pipe_fds[0]) && fd_has_flags(pipe_fds[1]);
#endif
	else if (hawk_comp_bcstr(name, "recv-no-address-data", 0) == 0) recv_no_address = 1;
	else if (hawk_comp_bcstr(name, "recv-no-address-eof", 0) == 0) recv_no_address = 2;
	else if (hawk_comp_bcstr(name, "wait-calls", 0) == 0) n = wait_calls;
	else if (hawk_comp_bcstr(name, "wait-pid", 0) == 0) n = wait_pid;
	else if (hawk_comp_bcstr(name, "wait-options", 0) == 0) n = wait_opts;
	else if (hawk_comp_bcstr(name, "flock-calls", 0) == 0) n = flock_calls;
	else if (hawk_comp_bcstr(name, "kill-calls", 0) == 0) n = kill_calls;
	else if (hawk_comp_bcstr(name, "kill-pid", 0) == 0) n = kill_pid;
	else if (hawk_comp_bcstr(name, "kill-signal", 0) == 0) n = kill_sig;
	else if (hawk_comp_bcstr(name, "tcsetattr-calls", 0) == 0) n = tcsetattr_calls;
	else if (hawk_comp_bcstr(name, "tcflush-calls", 0) == 0) n = tcflush_calls;
	else if (hawk_comp_bcstr(name, "clear-fcntl-failure", 0) == 0) { fail_fcntl = 0; fail_fcntl_cmd = -1; }
#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
	else if (hawk_comp_bcstr(name, "mux-save-event", 0) == 0) saved_mux_event = last_mux_event;
	else if (hawk_comp_bcstr(name, "mux-save-zero-token", 0) == 0) { saved_mux_event = last_mux_event; saved_mux_event.data.u64 = 0; }
	else if (hawk_comp_bcstr(name, "mux-save-invalid-id", 0) == 0) { saved_mux_event = last_mux_event; saved_mux_event.data.u64 = ((hawk_uint64_t)1 << 32) | HAWK_TYPE_MAX(hawk_uint32_t); }
	else if (hawk_comp_bcstr(name, "mux-inject-event", 0) == 0) inject_mux_event = 1;
	else if (hawk_comp_bcstr(name, "mux-fail-delete", 0) == 0) fail_mux_delete = 1;
	else if (hawk_comp_bcstr(name, "mux-fail-create", 0) == 0) fail_mux_create = 1;
	else if (hawk_comp_bcstr(name, "mux-fail-add", 0) == 0) fail_mux_add = 1;
	else if (hawk_comp_bcstr(name, "mux-clear-failure", 0) == 0) fail_mux_delete = fail_mux_create = fail_mux_add = 0;
	else if (hawk_comp_bcstr(name, "mux-wait-calls", 0) == 0) n = mux_wait_calls;
	else if (hawk_comp_bcstr(name, "mux-delete-calls", 0) == 0) n = mux_delete_calls;
	else if (hawk_comp_bcstr(name, "mux-add-calls", 0) == 0) n = mux_add_calls;
	else if (hawk_comp_bcstr(name, "mux-expect-no-cleanup-delete", 0) == 0) expected_cleanup_delete_calls = mux_delete_calls;
	else if (hawk_comp_bcstr(name, "mux-oneshot", 0) == 0) n = EPOLLONESHOT;
#endif
	else if (hawk_comp_bcstr(name, "fail-tcgetattr", 0) == 0) fail_termios = 1;
	else if (hawk_comp_bcstr(name, "fail-tcsetattr", 0) == 0) fail_termios = 2;
	else if (hawk_comp_bcstr(name, "fail-tcflush", 0) == 0) fail_termios = 3;
	else if (hawk_comp_bcstr(name, "sleep-eintr", 0) == 0) fail_sleep = EINTR;
	else if (hawk_comp_bcstr(name, "sleep-einval", 0) == 0) fail_sleep = EINVAL;
	else if (hawk_comp_bcstr(name, "sleep-calls", 0) == 0) n = sleep_calls;
	else if (hawk_comp_bcstr(name, "log-closed", 0) == 0) n = log_fd >= 0 && fcntl(log_fd, F_GETFD) <= -1;
	hawk_rtx_freevalbcstr(rtx, arg, name);
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval_inline(rtx, n));
	return 0;
}

#define waitpid unfinished_wait
#define socket test_socket
#define pipe test_pipe
#define kill test_kill
#define setsockopt test_setsockopt
#define sendto test_sendto
#define accept test_accept
#define bind test_bind
#define connect test_connect
#define shutdown test_shutdown
#define listen test_listen
#define fchown test_fchown
#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
#define epoll_ctl test_epoll_ctl
#define epoll_wait test_epoll_wait
#if !defined(HAVE_EPOLL_CREATE1) || !defined(EPOLL_CLOEXEC)
#define epoll_create test_epoll_create
#else
#define epoll_create1 test_epoll_create1
#endif
#endif
/* Exercise flag setup even on hosts that provide the atomic APIs. */
#undef HAVE_ACCEPT4
#undef HAVE_PIPE2
#define fcntl hawk_test_sys_fcntl
#define tcgetattr test_tcgetattr
#define tcsetattr test_tcsetattr
#define tcflush test_tcflush
#define recvfrom empty_datagram
#if defined(HAVE_NANOSLEEP)
#define nanosleep test_nanosleep
#elif defined(HAVE_SELECT)
#define select test_select
#endif
/* Exercise dup3's failure path even when configure selected dup2. */
#ifndef HAVE_DUP3
#define HAVE_DUP3 1
#endif
#define dup3 test_dup3
#elif defined(_WIN32)
#include <io.h>
#include <winsock2.h>
#include <windows.h>
#include <errno.h>

/* Track native socket ownership, including failure during node allocation. */
static SOCKET socket_handles[128];
static int socket_count;
static int socket_closed;
static int socket_close_error;
static int socket_failure;
static int socket_high;
static int socket_io;
static int socket_io_bad;
static int socket_mock_io;
static int file_mock_io;
static unsigned int file_io_count;
static int file_last_fd;
static int win_sleep_calls;
static DWORD win_sleep_ms;
static int log_fail_config;
static int log_config_calls;
static int log_nonblock;

static SOCKET tracked_socket (SOCKET s)
{
	if (s != INVALID_SOCKET && socket_count < HAWK_COUNTOF(socket_handles)) socket_handles[socket_count++] = s;
	return s;
}

static SOCKET WSAAPI test_win_socket (int domain, int type, int protocol)
{
	socket_calls++;
	if (socket_failure) { errno = EEXIST; WSASetLastError(WSAEACCES); return INVALID_SOCKET; }
	if (socket_high) return tracked_socket(((SOCKET)1 << (HAWK_SIZEOF(SOCKET) * 8 - 1)) | 1234);
	return tracked_socket(socket(domain, type, protocol));
}

static int WSAAPI test_win_setsockopt (SOCKET s, int level, int option, const char* ptr, int len)
{
	setsockopt_calls++;
	return setsockopt(s, level, option, ptr, len);
}

static int WSAAPI test_win_sendto (SOCKET s, const char* ptr, int len, int flags, const struct sockaddr* addr, int addrlen)
{
	if (!log_mock_send) return sendto(s, ptr, len, flags, addr, addrlen);
	log_send_calls++;
	if (log_fail_send) { errno = EEXIST; WSASetLastError(WSAEWOULDBLOCK); return SOCKET_ERROR; }
	if (len < 0 || len > HAWK_COUNTOF(log_message)) { WSASetLastError(WSAEMSGSIZE); return SOCKET_ERROR; }
	HAWK_MEMCPY(log_message, ptr, len);
	log_message_len = len;
	return len;
}

static int WSAAPI test_win_ioctlsocket (SOCKET s, long cmd, u_long* arg)
{
	log_config_calls++;
	if (log_fail_config == 1) { errno = EEXIST; WSASetLastError(WSAEACCES); return SOCKET_ERROR; }
	log_nonblock = cmd == FIONBIO && *arg == 1;
	return ioctlsocket(s, cmd, arg);
}

static BOOL WINAPI test_win_sethandleinformation (HANDLE handle, DWORD mask, DWORD flags)
{
	if (log_fail_config == 2) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
	return SetHandleInformation(handle, mask, flags);
}

static int WSAAPI test_win_closesocket (SOCKET s)
{
	int i, rc;
	rc = socket_high? 0: closesocket(s);
	for (i = 0; i < socket_count; i++)
	{
		if (socket_handles[i] == s && rc == 0) { socket_handles[i] = INVALID_SOCKET; socket_closed++; break; }
	}
	if (i == socket_count) socket_io_bad = 1;
	if (rc == 0 && socket_close_error)
	{
		errno = EIO; /* The production socket path must use the Winsock error. */
		WSASetLastError(socket_close_error);
		return SOCKET_ERROR;
	}
	WSASetLastError(WSAEINVAL);
	return rc;
}

static SOCKET WSAAPI test_win_wsasocket (int af, int type, int protocol, LPWSAPROTOCOL_INFOA info, GROUP group, DWORD flags)
{
	return tracked_socket(WSASocketA(af, type, protocol, info, group, flags));
}

static SOCKET WSAAPI test_win_accept (SOCKET s, struct sockaddr* addr, int* len)
{
	struct sockaddr_in peer;
	accept_calls++;
	HAWK_MEMSET(&peer, 0, HAWK_SIZEOF(peer));
	peer.sin_family = AF_INET;
	HAWK_MEMCPY(addr, &peer, HAWK_SIZEOF(peer));
	*len = HAWK_SIZEOF(peer);
	return tracked_socket(socket(AF_INET, SOCK_DGRAM, 0));
}

static int WSAAPI test_win_shutdown (SOCKET s, int how)
{
	shutdown_calls++;
	shutdown_how = how;
	return 0;
}

static int WSAAPI test_win_listen (SOCKET s, int backlog)
{
	listen_calls++;
	listen_backlog = backlog;
	return 0;
}

static int WSAAPI test_win_recv (SOCKET s, char* buf, int len, int flags)
{
	if (!socket_mock_io) return recv(s, buf, len, flags);
	socket_io++;
	if (socket_count == 0 || s != socket_handles[0]) { socket_io_bad = 1; WSASetLastError(WSAENOTSOCK); return SOCKET_ERROR; }
	if (socket_failure) { errno = EEXIST; WSASetLastError(WSAEWOULDBLOCK); return SOCKET_ERROR; }
	if (len < 3) return SOCKET_ERROR;
	HAWK_MEMCPY(buf, "ABC", 3);
	return 3;
}

static int WSAAPI test_win_send (SOCKET s, const char* buf, int len, int flags)
{
	if (!socket_mock_io) return send(s, buf, len, flags);
	socket_io++;
	if (socket_count == 0 || s != socket_handles[0]) { socket_io_bad = 1; WSASetLastError(WSAENOTSOCK); return SOCKET_ERROR; }
	if (socket_failure) { errno = EEXIST; WSASetLastError(WSAEWOULDBLOCK); return SOCKET_ERROR; }
	return len;
}

static int test_win_read (int fd, void* ptr, unsigned int count)
{
	file_last_fd = fd;
	if (!file_mock_io) return read(fd, ptr, count);
	file_io_count = count;
	if (count < 3) return 0;
	HAWK_MEMCPY(ptr, "ABC", 3);
	return 3;
}

static int test_win_write (int fd, const void* ptr, unsigned int count)
{
	file_last_fd = fd;
	if (!file_mock_io) return write(fd, ptr, count);
	file_io_count = count;
	return count > 0? 1: 0;
}

static void WINAPI test_win_sleep (DWORD ms)
{
	win_sleep_calls++;
	win_sleep_ms = ms;
}

static HANDLE kill_handle;
static HANDLE kill_child;
static int kill_handles;
static int kill_failure;

int hawk_test_sys_pid32_narrow (void) { return 0; }
int hawk_test_sys_offset32_narrow (void) { return 0; }
int hawk_test_sys_fd_flags_available (void) { return 0; }
int hawk_test_sys_begin (void) { return 77; }
int hawk_test_sys_end (void) { return 0; }

static HANDLE WINAPI test_open_process (DWORD access, BOOL inherit, DWORD pid)
{
	HANDLE proc;
	if (kill_failure == 1 || kill_failure == 2)
	{
		SetLastError(kill_failure == 1? ERROR_INVALID_PARAMETER: ERROR_ACCESS_DENIED);
		return NULL;
	}
	proc = OpenProcess(access, inherit, pid);
	if (proc) { kill_handle = proc; kill_handles++; }
	return proc;
}

static DWORD WINAPI test_wait_process (HANDLE proc, DWORD timeout)
{
	if (kill_failure == 3 && proc == kill_handle)
	{
		SetLastError(ERROR_ACCESS_DENIED);
		return WAIT_FAILED;
	}
	return WaitForSingleObject(proc, timeout);
}

static BOOL WINAPI test_terminate_process (HANDLE proc, UINT code)
{
	if (kill_failure == 4 && proc == kill_handle)
	{
		SetLastError(ERROR_ACCESS_DENIED);
		return FALSE;
	}
	return TerminateProcess(proc, code);
}

static BOOL WINAPI test_close_process (HANDLE proc)
{
	BOOL rc = CloseHandle(proc);
	if (rc && proc == kill_handle) { kill_handle = NULL; kill_handles--; }
	/* Ensure an error was saved before closing the process handle. */
	SetLastError(ERROR_INVALID_HANDLE);
	return rc;
}

int hawk_test_sys_native_kill_begin (void)
{
	reset_log_probe();
	log_fail_config = log_config_calls = log_nonblock = 0;
	kill_handle = NULL;
	kill_child = NULL;
	kill_handles = 0;
	kill_failure = 0;
	socket_count = socket_closed = socket_failure = socket_high = socket_io = socket_io_bad = 0;
	socket_close_error = 0;
	socket_calls = setsockopt_calls = 0;
	shutdown_calls = listen_calls = accept_calls = 0;
	shutdown_how = listen_backlog = 0;
	socket_mock_io = file_mock_io = 0;
	file_io_count = 0;
	file_last_fd = -1;
	win_sleep_calls = 0;
	win_sleep_ms = 0;
	{
		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 77;
	}
	return 0;
}

int hawk_test_sys_native_kill_end (void)
{
	int ok = kill_handles == 0;
	int i;
	for (i = 0; i < socket_count; i++)
	{
		if (socket_handles[i] != INVALID_SOCKET)
		{
			fprintf(stderr, "native socket leaked after runtime close\n");
			if (!socket_high) closesocket(socket_handles[i]);
			ok = 0;
		}
	}
	if (socket_io_bad) ok = 0;
	WSACleanup();
	if (kill_handle) CloseHandle(kill_handle);
	if (kill_child)
	{
		TerminateProcess(kill_child, 1);
		if (WaitForSingleObject(kill_child, 10000) != WAIT_OBJECT_0) ok = 0;
		CloseHandle(kill_child);
	}
	return ok? 0: -1;
}

int hawk_test_sys_probe (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* arg = hawk_rtx_getarg(rtx, 0);
	hawk_bch_t* name;
	hawk_int_t n = 0;
	int handled;
	name = hawk_rtx_getvalbcstr(rtx, arg, HAWK_NULL);
	if (!name) return -1;
	handled = log_probe(rtx, name, &n);
	if (handled != 0)
	{
		hawk_rtx_freevalbcstr(rtx, arg, name);
		if (handled < 0) return -1;
		if (handled != 2) hawk_rtx_setretval(rtx, hawk_rtx_makeintval_inline(rtx, n));
		return 0;
	}
	if (hawk_comp_bcstr(name, "file-mock", 0) == 0) file_mock_io = 1;
	else if (hawk_comp_bcstr(name, "file-count", 0) == 0) n = file_io_count;
	else if (hawk_comp_bcstr(name, "file-fd", 0) == 0) n = file_last_fd;
	else if (hawk_comp_bcstr(name, "socket-closed", 0) == 0) n = socket_closed;
	else if (hawk_comp_bcstr(name, "socket-close-error", 0) == 0) socket_close_error = WSAEACCES;
	else if (hawk_comp_bcstr(name, "socket-close-normal", 0) == 0) socket_close_error = 0;
	else if (hawk_comp_bcstr(name, "socket-calls", 0) == 0) n = socket_calls;
	else if (hawk_comp_bcstr(name, "setsockopt-calls", 0) == 0) n = setsockopt_calls;
	else if (hawk_comp_bcstr(name, "shutdown-calls", 0) == 0) n = shutdown_calls;
	else if (hawk_comp_bcstr(name, "listen-calls", 0) == 0) n = listen_calls;
	else if (hawk_comp_bcstr(name, "accept-calls", 0) == 0) n = accept_calls;
	else if (hawk_comp_bcstr(name, "shutdown-how", 0) == 0) n = shutdown_how;
	else if (hawk_comp_bcstr(name, "listen-backlog", 0) == 0) n = listen_backlog;
	else if (hawk_comp_bcstr(name, "socket-high", 0) == 0) socket_high = 1;
	else if (hawk_comp_bcstr(name, "socket-failure", 0) == 0) socket_failure = 1;
	else if (hawk_comp_bcstr(name, "socket-mock-io", 0) == 0) socket_mock_io = 1;
	else if (hawk_comp_bcstr(name, "socket-io", 0) == 0) n = socket_io;
	else if (hawk_comp_bcstr(name, "log-fail-nonblock", 0) == 0) log_fail_config = 1;
	else if (hawk_comp_bcstr(name, "log-fail-inheritance", 0) == 0) log_fail_config = 2;
	else if (hawk_comp_bcstr(name, "log-config-calls", 0) == 0) n = log_config_calls;
	else if (hawk_comp_bcstr(name, "log-nonblock", 0) == 0) n = log_nonblock;
	else if (hawk_comp_bcstr(name, "log-inheritance", 0) == 0)
	{
		DWORD flags;
		n = socket_count > 0 && GetHandleInformation((HANDLE)socket_handles[socket_count - 1], &flags) && !(flags & HANDLE_FLAG_INHERIT);
	}
	else if (hawk_comp_bcstr(name, "socket-address", 0) == 0)
	{
		struct sockaddr_in addr;
		int len = HAWK_SIZEOF(addr);
		hawk_ooch_t buf[128];
		hawk_val_t* val;
		if (socket_count == 0 || getsockname(socket_handles[socket_count - 1], (struct sockaddr*)&addr, &len) != 0) goto address_error;
		/* This fixture binds IPv4 loopback. Avoid depending on cross-configure's
		 * HAWK_AF_* values when reporting the dynamically assigned port. */
		hawk_rtx_fmttooocstr(rtx, buf, HAWK_COUNTOF(buf), HAWK_T("127.0.0.1:%u"), (unsigned int)ntohs(addr.sin_port));
		val = hawk_rtx_makestrvalwithoocstr(rtx, buf);
		hawk_rtx_freevalbcstr(rtx, arg, name);
		if (!val) return -1;
		hawk_rtx_setretval(rtx, val);
		return 0;
	address_error:
		n = -1;
	}
	else if (hawk_comp_bcstr(name, "socket-timeout", 0) == 0)
	{
		DWORD ms = 0;
		int len = HAWK_SIZEOF(ms);
		n = socket_count > 0 && getsockopt(socket_handles[0], SOL_SOCKET, SO_RCVTIMEO, (char*)&ms, &len) == 0? ms: -1;
	}
	else if (hawk_comp_bcstr(name, "sleep-calls", 0) == 0) n = win_sleep_calls;
	else if (hawk_comp_bcstr(name, "sleep-ms", 0) == 0) n = win_sleep_ms;
	else if (hawk_comp_bcstr(name, "kill-handles", 0) == 0) n = kill_handles;
	else if (hawk_comp_bcstr(name, "kill-missing", 0) == 0) kill_failure = 1;
	else if (hawk_comp_bcstr(name, "kill-denied", 0) == 0) kill_failure = 2;
	else if (hawk_comp_bcstr(name, "kill-wait-failure", 0) == 0) kill_failure = 3;
	else if (hawk_comp_bcstr(name, "kill-terminate-failure", 0) == 0) kill_failure = 4;
	else if (hawk_comp_bcstr(name, "kill-child", 0) == 0)
	{
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		char path[MAX_PATH], command[MAX_PATH + 32];
		DWORD len;

		HAWK_MEMSET(&si, 0, HAWK_SIZEOF(si));
		si.cb = HAWK_SIZEOF(si);
		len = GetModuleFileNameA(NULL, path, HAWK_COUNTOF(path));
		n = -1;
		if (!kill_child && len > 0 && len < HAWK_COUNTOF(path))
		{
			snprintf(command, HAWK_COUNTOF(command), "\"%s\" --kill-child", path);
			if (CreateProcessA(path, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				kill_child = pi.hProcess;
				n = pi.dwProcessId;
				CloseHandle(pi.hThread);
			}
		}
	}
	else if (hawk_comp_bcstr(name, "kill-child-exited", 0) == 0)
	{
		DWORD code;
		n = kill_child && WaitForSingleObject(kill_child, 10000) == WAIT_OBJECT_0 &&
		    GetExitCodeProcess(kill_child, &code) && code == 255 + 1 + 9;
	}
	hawk_rtx_freevalbcstr(rtx, arg, name);
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval_inline(rtx, n));
	return 0;
}

#define socket test_win_socket
#define setsockopt test_win_setsockopt
#define sendto test_win_sendto
#define ioctlsocket test_win_ioctlsocket
#define SetHandleInformation test_win_sethandleinformation
#define closesocket test_win_closesocket
#define WSASocketA test_win_wsasocket
#define accept test_win_accept
#define shutdown test_win_shutdown
#define listen test_win_listen
#define recv test_win_recv
#define send test_win_send
#define read test_win_read
#define write test_win_write
#define Sleep test_win_sleep
#define OpenProcess test_open_process
#define WaitForSingleObject test_wait_process
#define TerminateProcess test_terminate_process
#define CloseHandle test_close_process
#else
int hawk_test_sys_pid32_narrow (void) { return 0; }
int hawk_test_sys_offset32_narrow (void) { return 0; }
int hawk_test_sys_fd_flags_available (void) { return 0; }
int hawk_test_sys_begin (void) { return 77; }
int hawk_test_sys_end (void) { return 0; }
int hawk_test_sys_probe (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi) { return -1; }
#endif

#if !defined(_WIN32)
int hawk_test_sys_native_kill_begin (void)
{
#if defined(__OS2__)
	return 0;
#else
	return 77;
#endif
}
int hawk_test_sys_native_kill_end (void) { return 0; }
#endif

static int test_set_ntime (const hawk_ntime_t* now)
{
	/* Never change the system clock, even when the test process is privileged. */
	settime_calls++;
	settime_value = *now;
	if (settime_success) return 0;
#if defined(_WIN32)
	SetLastError(ERROR_ACCESS_DENIED);
	errno = EINVAL; /* the Windows path must use GetLastError(), not errno. */
#else
	errno = EACCES;
#endif
	return -1;
}

static int fail_time (hawk_ntime_t* now)
{
	if (log_time && log_timestamp)
	{
		now->sec = log_timestamp;
		now->nsec = 0;
		if (log_timestamp == 3) errno = EACCES;
		return 0;
	}
	if (log_time && log_tz_mode)
	{
		now->sec = 1704067200;
		now->nsec = 0;
		return 0;
	}
	if (log_time) return hawk_get_ntime(now);
	return -1;
}

static int test_tm_conversion (hawk_intmax_t t)
{
	/* Small, representable timestamps distinguish OS errors from a failed
	 * conversion without errno, and check clearing a preceding stale errno. */
	if (t == 1) { errno = EIO; return -1; }
	if (t == 2) { errno = 0; return -1; }
	if (t == 3 && errno != 0) { errno = EINVAL; return -1; }
	return 0;
}

static struct tm* test_log_tm (int utc)
{
	/* Use shared static storage for both conversions to exercise the caller's
	 * local-time copy as well as positive, negative, DST and year-boundary offsets. */
	HAWK_MEMSET(&log_tz_tm, 0, HAWK_SIZEOF(log_tz_tm));
	log_tz_tm.tm_year = 123;
	log_tz_tm.tm_mon = 11;
	log_tz_tm.tm_mday = 31;
	log_tz_tm.tm_yday = 364;
	log_tz_tm.tm_hour = 23;
	log_tz_tm.tm_min = 45;
	log_tz_tm.tm_sec = 30;
	if (utc)
	{
		if (log_tz_mode == 2)
		{
			log_tz_tm.tm_year = 124;
			log_tz_tm.tm_mon = log_tz_tm.tm_yday = log_tz_tm.tm_hour = 0;
			log_tz_tm.tm_mday = 1;
			log_tz_tm.tm_min = 15;
		}
		else if (log_tz_mode == 5)
		{
			log_tz_tm.tm_year = 124;
			log_tz_tm.tm_yday = 365;
		}
		else if (log_tz_mode == 6 || log_tz_mode == 7)
		{
			errno = (log_tz_mode == 6)? EIO: 0;
			return HAWK_NULL;
		}
	}
	else if (log_tz_mode == 1 || log_tz_mode == 4 || log_tz_mode == 5)
	{
		log_tz_tm.tm_year = (log_tz_mode == 5)? 125: 124;
		log_tz_tm.tm_mon = log_tz_tm.tm_yday = 0;
		log_tz_tm.tm_mday = 1;
		log_tz_tm.tm_hour = (log_tz_mode == 4)? 1: 5;
		log_tz_tm.tm_min = (log_tz_mode == 4)? 45: (log_tz_mode == 5)? 30: 15;
		log_tz_tm.tm_isdst = (log_tz_mode == 4);
	}
	else if (log_tz_mode == 2) log_tz_tm.tm_hour = 20;
	else if (log_tz_mode == 8) log_tz_tm.tm_sec++;
	return &log_tz_tm;
}

static struct tm* test_gmtime (const time_t* t)
{
	if (test_tm_conversion(*t) <= -1) return HAWK_NULL;
	if (log_tz_mode) return test_log_tm(1);
	return gmtime(t);
}

static struct tm* test_localtime (const time_t* t)
{
	if (test_tm_conversion(*t) <= -1) return HAWK_NULL;
	if (log_tz_mode) return test_log_tm(0);
	return localtime(t);
}

#if defined(_WIN32)
static errno_t test_tm64_s (struct tm* tm, const __time64_t* t, int utc)
{
	struct tm* tmx;

	/* Keep the full timestamp even when the build's time_t is only 32 bits.
	 * A nonzero return without errno exercises the fallback diagnostic. */
	if (test_tm_conversion(*t) <= -1) return errno? errno: EINVAL;
	if (log_tz_mode)
	{
		tmx = test_log_tm(utc);
		if (!tmx) return errno? errno: EINVAL;
		*tm = *tmx;
		return 0;
	}
	return utc? _gmtime64_s(tm, t): _localtime64_s(tm, t);
}

static errno_t __cdecl test_gmtime64_s (struct tm* tm, const __time64_t* t)
{
	return test_tm64_s(tm, t, 1);
}

static errno_t __cdecl test_localtime64_s (struct tm* tm, const __time64_t* t)
{
	return test_tm64_s(tm, t, 0);
}
#endif

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
static void test_openlog (const char* ident, int options, int facility)
{
	native_log_options = options;
	native_log_calls++;
}
#define openlog test_openlog
#endif

static size_t test_strftime (char* buf, size_t size, const char* fmt, const struct tm* tm)
{
	/* Permit a second call to succeed so the unfixed retry loop fails the
	 * error assertion without exhausting memory. */
	if (size == 64 && hawk_comp_bcstr(fmt, " invalid-format", 0) == 0)
	{
		errno = EINVAL;
		return 0;
	}
	if (size == 64 && hawk_comp_bcstr(fmt, " failed-format", 0) == 0)
	{
		errno = EIO;
		return 0;
	}
	if (hawk_comp_bcstr(fmt, " range-format", 0) == 0)
	{
		/* Each attempt must clear errno, including the ERANGE from a retry. */
		if (errno != 0) { errno = EIO; return 0; }
		if (size == 64) { errno = ERANGE; return 0; }
	}
	if (hawk_comp_bcstr(fmt, " stale-errno", 0) == 0) errno = EINVAL;
	return strftime(buf, size, fmt, tm);
}

static time_t test_mktime (struct tm* tm)
{
	if (mktime_mode)
	{
		if (errno != 0 || mktime_mode == 1) { errno = EIO; return (time_t)-1; }
		/* Failure without errno must leave the invalid weekday untouched. */
		if (mktime_mode == 4) return (time_t)-1;
		/* Successful conversions replace the weekday even when returning -1. */
		tm->tm_wday = 3;
		if (mktime_mode == 2) return (time_t)-1;
		if (mktime_mode == 5) { errno = ENOENT; return (time_t)-1; }
		/* A successful conversion must ignore errno even if libc leaves it set. */
		errno = EACCES;
		return (time_t)123;
	}
	return mktime(tm);
}

static int test_pio_waitex (hawk_pio_t* pio, hawk_uintmax_t* status)
{
	int n = hawk_pio_waitex(pio, status);
	if (n != 1 || system_wait_mode == 0) return n;
	if (system_wait_mode == 3)
	{
		hawk_gem_seterrnum(pio->gem, HAWK_NULL, HAWK_EIOERR);
		return -1;
	}
	*status = HAWK_TYPE_MAX(hawk_uint_t) >> 1;
	system_result_oom = system_wait_mode == 2;
	return 1;
}

static hawk_val_t* test_makeintval (hawk_rtx_t* rtx, hawk_int_t v)
{
	hawk_val_int_t* saved;
	hawk_val_t* val;

	if (!system_result_oom || v != (hawk_int_t)(HAWK_TYPE_MAX(hawk_uint_t) >> 1))
		return hawk_rtx_makeintval_inline(rtx, v);
	system_result_oom = 0;
	/* Force the real integer constructor to allocate a chunk, even if earlier
	 * interpreter work left cached integers available. Fail only that allocation. */
	saved = rtx->vmgr.ifree;
	rtx->vmgr.ifree = HAWK_NULL;
	hawk_test_fail_next_allocation(rtx);
	val = hawk_rtx_makeintval_inline(rtx, v);
	rtx->vmgr.ifree = saved;
	return val;
}

#if defined(_WIN32) || (!defined(__OS2__) && !defined(__DOS__))
static int test_file_close (int fd)
{
	int n;

	file_close_calls++;
	n = close(fd);
	/* Model a close that releases the descriptor but reports a delayed I/O
	 * error or interruption. Repeating it would close a reused descriptor. */
	if (n == 0 && file_close_error)
	{
		errno = file_close_error;
		return -1;
	}
	return n;
}
#define close test_file_close
#endif

/* Exercise the fallback that returns static struct tm storage as well. */
#undef HAVE_GMTIME_R
#undef HAVE_LOCALTIME_R
#define hawk_get_ntime fail_time
#define hawk_set_ntime test_set_ntime
#define gmtime test_gmtime
#define localtime test_localtime
#if defined(_WIN32)
#define _gmtime64_s test_gmtime64_s
#define _localtime64_s test_localtime64_s
#endif
#define strftime test_strftime
#define mktime test_mktime
#define timelocal test_mktime
#define hawk_pio_waitex test_pio_waitex
#define hawk_rtx_makeintval_inline test_makeintval
/* Keep the mux syscall fault injection after moving OS operations out of
 * mod-sys.c. This private copy exercises the same standalone implementation. */
#define hawk_mux_open hawk_test_mux_open
#define hawk_mux_close hawk_test_mux_close
#define hawk_mux_getgem hawk_test_mux_getgem
#define hawk_mux_getxtn hawk_test_mux_getxtn
#define hawk_mux_getbackend hawk_test_mux_getbackend
#define hawk_mux_ctl hawk_test_mux_ctl
#define hawk_mux_wait hawk_test_mux_wait
#include "../lib/mux.c"
#define hawk_mod_sys hawk_test_mod_sys_fault
#include "../lib/mod-sys.c"
