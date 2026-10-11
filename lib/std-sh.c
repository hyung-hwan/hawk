/*
    Copyright (c) 2006-2020 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* OS-dependent shell services are deliberately separate from sh.c. */
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "hawk-prv.h"
#include "sh-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>

#define SH_NFD (256)

typedef struct env_t env_t;
struct env_t
{
	struct env_t* next;
	hawk_bch_t* name;
	hawk_bch_t* value; /* NULL = unset */
};

typedef struct env_backup_t env_backup_t;
struct env_backup_t
{
	env_backup_t* next;
	env_t entry;
	int present;
};

typedef struct process_t process_t;
struct process_t
{
	pid_t pid;
	int state; /* 0 running, 1 stopped, 2 done */
	int status;
};

typedef struct job_t job_t;
struct job_t
{
	struct job_t* next;
	unsigned int id;
	pid_t pgid;
	hawk_oow_t count;
	process_t* processes;
	hawk_bch_t* name;
	struct termios modes;
	int have_modes, own_group;
};

typedef struct input_t input_t;
struct input_t
{
	int fd;
	int bol;
	int eof;
};

typedef struct std_t std_t;
struct std_t
{
	hawk_bch_t* cwd;
	env_t* env;
	hawk_ooch_t* env_result;
	job_t* jobs;
	pid_t last_pid, shell_pgid, original_pgid;
	int interactive, tty, input_fd, minfd, in_child;
	int* active_fds; /* logical descriptor map for parent-run callbacks */
	struct termios shell_modes;
	struct sigaction old_signals[5];
};

static int syserror (hawk_sh_t* sh, const hawk_bch_t* context)
{
	int e = errno;

	hawk_gem_seterrbfmt(&sh->gem_, HAWK_NULL, hawk_syserr_to_errnum(e), "%s (OS error %d)", context, e);
	return -1;
}

static hawk_bch_t* tobytes (hawk_sh_t* sh, const hawk_ooch_t* s)
{
#if defined(HAWK_OOCH_IS_UCH)
	return hawk_gem_duputobcstr(&sh->gem_, s, HAWK_NULL);
#else
	return hawk_gem_dupbcstr(&sh->gem_, s, HAWK_NULL);
#endif
}

static hawk_ooch_t* tochars (hawk_sh_t* sh, const hawk_bch_t* s)
{
#if defined(HAWK_OOCH_IS_UCH)
	return hawk_gem_dupbtoucstr(&sh->gem_, s, HAWK_NULL, 0);
#else
	return hawk_gem_dupbcstr(&sh->gem_, s, HAWK_NULL);
#endif
}

static int eq (const hawk_ooch_t* s, const hawk_ooch_t* t)
{
	return hawk_comp_oocstr(s, t, 0) == 0;
}

static hawk_ooi_t std_read (hawk_sh_t* sh, int fd, void* ptr, hawk_oow_t size, void* ctx)
{
	std_t* s = ctx;
	ssize_t n;

	if (fd <= -1 || fd >= SH_NFD)
	{
		errno = EBADF;
		return syserror(sh, "read");
	}

	if (s->active_fds) fd = s->active_fds[fd];

	do
	{
		n = read(fd, ptr, size);
	}
	while (n <= -1 && errno == EINTR);

	return n <= -1? syserror(sh, "read"): n;
}

static hawk_ooi_t std_write (hawk_sh_t* sh, int fd, const void* ptr, hawk_oow_t size, void* ctx)
{
	std_t* s = ctx;
	hawk_oow_t done = 0;

	if (fd <= -1 || fd >= SH_NFD)
	{
		errno = EBADF;
		return syserror(sh, "write");
	}

	if (s->active_fds) fd = s->active_fds[fd];

	while (done < size)
	{
		ssize_t n = write(fd, (const hawk_bch_t*)ptr + done, size - done);

		if (n <= -1)
		{
			if (errno == EINTR) continue;
			return syserror(sh, "write");
		}
		if (n == 0)
		{
			errno = EIO;
			return syserror(sh, "write");
		}
		done += n;
	}

	return done;
}

static int message (hawk_sh_t* sh, int fd, const hawk_bch_t* text)
{
	return hawk_sh_write(sh, fd, text, hawk_count_bcstr(text)) <= -1? -1: 0;
}

static env_t* findenv (std_t* s, const hawk_bch_t* name)
{
	env_t* e;

	for (e = s->env; e; e = e->next)
	{
		if (hawk_comp_bcstr(e->name, name, 0) == 0) return e;
	}

	return HAWK_NULL;
}

static const hawk_bch_t* getenvvalue (std_t* s, const hawk_bch_t* name)
{
	env_t* e = findenv(s, name);

	return e? e->value: getenv(name);
}

static int std_getenv (hawk_sh_t* sh, const hawk_ooch_t* name, const hawk_ooch_t** result, void* ctx)
{
	std_t* s = ctx;
	hawk_bch_t* n;
	const hawk_bch_t* value;
	hawk_bch_t buf[64];

	n = tobytes(sh, name);
	if (HAWK_UNLIKELY(!n)) return -1;

	if (hawk_comp_bcstr(n, "$", 0) == 0 || hawk_comp_bcstr(n, "!", 0) == 0)
	{
		pid_t pid = n[0] == '$'? getpid(): s->last_pid;

		if (pid == 0) buf[0] = '\0';
		else hawk_fmt_uintmax_to_bcstr(buf, HAWK_COUNTOF(buf), pid, 10, -1, 0, HAWK_NULL);
		value = buf;
	}
	else
	{
		value = getenvvalue(s, n);
	}

	hawk_gem_freemem(&sh->gem_, n);
	hawk_gem_freemem(&sh->gem_, s->env_result);
	s->env_result = value? tochars(sh, value): HAWK_NULL;
	if (value && !s->env_result) return -1;
	*result = s->env_result;

	return value? 1: 0;
}

static int validname (const hawk_bch_t* name)
{
	const hawk_bchu_t* p = (const hawk_bchu_t*)name;

	if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return 0;
	while (*++p)
	{
		if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) return 0;
	}

	return 1;
}

static int setenvvalue (hawk_sh_t* sh, std_t* s, const hawk_bch_t* name, const hawk_bch_t* value)
{
	env_t* e = findenv(s, name);
	hawk_bch_t* copy = value? hawk_gem_dupbcstr(&sh->gem_, value, HAWK_NULL): HAWK_NULL;

	if (value && !copy) return -1;

	if (!e)
	{
		e = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(*e));
		if (!e)
		{
			hawk_gem_freemem(&sh->gem_, copy);
			return -1;
		}

		e->name = hawk_gem_dupbcstr(&sh->gem_, name, HAWK_NULL);
		if (!e->name)
		{
			hawk_gem_freemem(&sh->gem_, e);
			hawk_gem_freemem(&sh->gem_, copy);
			return -1;
		}
		e->next = s->env;
		s->env = e;
	}

	hawk_gem_freemem(&sh->gem_, e->value);
	e->value = copy;

	return 0;
}

static hawk_bch_t* fullpath (hawk_sh_t* sh, std_t* s, const hawk_bch_t* path)
{
	hawk_oow_t a, b;
	hawk_bch_t* ptr;

	if (path[0] == '/') return hawk_gem_dupbcstr(&sh->gem_, path, HAWK_NULL);

	a = hawk_count_bcstr(s->cwd);
	b = hawk_count_bcstr(path);
	if (b > HAWK_TYPE_MAX(hawk_oow_t) - a - 2) return HAWK_NULL;

	ptr = hawk_gem_allocmem(&sh->gem_, a + b + 2);
	if (ptr)
	{
		HAWK_MEMCPY(ptr, s->cwd, a);
		ptr[a] = '/';
		HAWK_MEMCPY(ptr + a + 1, path, b + 1);
	}

	return ptr;
}

/* Keep internal descriptors above every user-mentioned descriptor. */
static int movefd (int fd, int minfd)
{
	int n, e;

	if (fd <= -1) return -1;

	n = fcntl(fd, F_DUPFD, minfd);
	if (n >= 0 && fcntl(n, F_SETFD, FD_CLOEXEC) <= -1)
	{
		e = errno;
		close(n);
		n = -1;
		errno = e;
	}

	e = errno;
	close(fd);
	errno = e;
	return n;
}

static int makepipe (int* fds, int minfd)
{
	int p[2];

	if (pipe(p) <= -1) return -1;

	fds[0] = movefd(p[0], minfd);
	fds[1] = movefd(p[1], minfd);
	if (fds[0] <= -1 || fds[1] <= -1)
	{
		int e = errno;
		if (fds[0] >= 0) close(fds[0]);
		if (fds[1] >= 0) close(fds[1]);
		errno = e;
		return -1;
	}

	return 0;
}

static void initmap (int* map)
{
	int i;

	for (i = 0; i < SH_NFD; i++)
	{
		map[i] = fcntl(i, F_GETFD) <= -1? -1: i;
	}
}

static void closemap (int* map)
{
	int i;

	for (i = 0; i < SH_NFD; i++)
	{
		if (map[i] >= 0 && map[i] != i) close(map[i]);
	}
}

static int fdnumber (const hawk_ooch_t* str)
{
	int n = 0;

	if (!*str) return -1;

	while (*str)
	{
		if (*str < '0' || *str > '9' || n > 25) return -1;
		n = n * 10 + *str++ - '0';
	}

	return n < SH_NFD? n: -1;
}

static const hawk_bch_t* temp_dir_getenv (const hawk_bch_t* name, void* ctx)
{
	return getenvvalue((std_t*)ctx, name);
}

static int herefile (hawk_sh_t* sh, std_t* s, const hawk_ooch_t* body)
{
	hawk_bch_t* data;
	hawk_bch_t* dir;
	hawk_bch_t* name;
	hawk_oow_t len;
	int fd = -1;

	data = tobytes(sh, body);
	if (!data) return -1;

	dir = hawk_get_temp_dir_bcstr(&sh->gem_, temp_dir_getenv, s);
	if (!dir) goto done;

	len = hawk_count_bcstr(dir);
	if (len > HAWK_TYPE_MAX(hawk_oow_t) - 24)
	{
		hawk_gem_seterrbfmt(&sh->gem_, HAWK_NULL, HAWK_EINVAL, "temporary directory path too long");
		goto done;
	}
	name = hawk_gem_allocmem(&sh->gem_, len + 24);
	if (!name) goto done;

	HAWK_MEMCPY(name, dir, len);
	hawk_copy_bcstr_unlimited(name + len, "/hawk-sh-XXXXXX"); /* TODO: change this hard-coded template pattern? */

	fd = mkstemp(name);
	if (fd >= 0)
	{
		if (unlink(name) <= -1)
		{
			close(fd);
			fd = -1;
		}
		else
		{
			hawk_oow_t pos, count;

			pos = 0;
			count = hawk_count_bcstr(data);

			while (pos < count)
			{
				ssize_t n;

				n = write(fd, data + pos, count - pos);
				if (n <= 0) /* TODO: write returns 0? should it be treated specially? */
				{
					if (n <= -1 && errno == EINTR) continue;
					close(fd);
					fd = -1;
					break;
				}
				pos += n;
			}

			if (fd >= 0 && lseek(fd, 0, SEEK_SET) <= -1)
			{
				close(fd);
				fd = -1;
			}
		}
	}
	hawk_gem_freemem(&sh->gem_, name);

done:
	if (dir) hawk_gem_freemem(&sh->gem_, dir);
	hawk_gem_freemem(&sh->gem_, data);
	return fd;
}

static int redirect (hawk_sh_t* sh, std_t* s, const hawk_sh_redir_t* r, int* map)
{
	for (; r; r = r->next)
	{
		int fd = -1;

		if (r->type == HAWK_SH_DUP)
		{
			if (!eq(r->target, HAWK_T("-")))
			{
				int source = fdnumber(r->target);

				if (source <= -1 || map[source] <= -1)
				{
					errno = EBADF;
					return syserror(sh, "descriptor duplication");
				}

				fd = fcntl(map[source], F_DUPFD, s->minfd);
				if (fd <= -1 || fcntl(fd, F_SETFD, FD_CLOEXEC) <= -1)
				{
					if (fd >= 0) close(fd);
					return syserror(sh, "descriptor duplication");
				}
			}
		}
		else
		{
			if (r->type == HAWK_SH_HEREDOC)
			{
				fd = herefile(sh, s, r->body);
			}
			else
			{
				hawk_bch_t* name;
				hawk_bch_t* path;
				int flags;

				name = tobytes(sh, r->target);
				if (HAWK_UNLIKELY(!name)) return -1;

				path = fullpath(sh, s, name);
				hawk_gem_freemem(&sh->gem_, name);
				if (!path) return -1;

				flags =
					r->type == HAWK_SH_IN? O_RDONLY:
					r->type == HAWK_SH_INOUT? (O_RDWR | O_CREAT): 
					                          (O_WRONLY | O_CREAT | (r->type == HAWK_SH_APPEND? O_APPEND: O_TRUNC));

				fd = open(path, flags, 0666);
				hawk_gem_freemem(&sh->gem_, path);
			}

			fd = movefd(fd, s->minfd);
			if (fd <= -1) return syserror(sh, "redirection");
		}

		if (map[r->fd] >= 0 && map[r->fd] != r->fd) close(map[r->fd]);
		map[r->fd] = fd;
	}

	return 0;
}

static int statuscode (int status)
{
	return WIFEXITED(status)? WEXITSTATUS(status):
	       WIFSIGNALED(status)? 128 + WTERMSIG(status): 128 + WSTOPSIG(status);
}

static int jobstate (job_t* job)
{
	hawk_oow_t i;
	int live = 0, running = 0;

	for (i = 0; i < job->count; i++)
	{
		if (job->processes[i].state != 2) live = 1;
		if (job->processes[i].state == 0) running = 1;
	}

	return !live? 2: running? 0: 1;
}

static int signal_job (job_t* job, int sig)
{
	hawk_oow_t i;
	int result = 0;

	if (job->own_group) return kill(-job->pgid, sig);
	for (i = 0; i < job->count; i++)
	{
		if (job->processes[i].state != 2 && kill(job->processes[i].pid, sig) <= -1) result = -1;
	}

	return result;
}

static void record_status (job_t* job, pid_t pid, int status)
{
	hawk_oow_t i;

	for (i = 0; i < job->count; i++)
	{
		if (job->processes[i].pid == pid)
		{
			process_t* p = &job->processes[i];

#if defined(WIFCONTINUED)
			if (WIFCONTINUED(status))
			{
				p->state = 0;
				return;
			}
#endif
			p->state = WIFSTOPPED(status)? 1: 2;
			p->status = statuscode(status);
			return;
		}
	}
}

static void poll_jobs (std_t* s)
{
	job_t* j;

	for (j = s->jobs; j; j = j->next)
	{
		hawk_oow_t i;

		for (i = 0; i < j->count; i++)
		{
			if (j->processes[i].state != 2)
			{
				int status, options = WNOHANG | WUNTRACED;
				pid_t pid;

			#if defined(WCONTINUED)
				options |= WCONTINUED;
			#endif
				while ((pid = waitpid(j->processes[i].pid, &status, options)) > 0)
				{
					record_status(j, pid, status);
				}

				if (pid <= -1 && errno == ECHILD && j->processes[i].state != 2)
				{
					/* A host reaper must not leave a stale PID eligible for kill. */
					j->processes[i].state = 2;
					j->processes[i].status = 127;
				}
			}
		}
	}
}

static int wait_job (hawk_sh_t* sh, std_t* s, job_t* j, int foreground)
{
	int result = -1;

	if (foreground && s->interactive)
	{
		if (tcsetpgrp(s->tty, j->pgid) <= -1) return syserror(sh, "foreground terminal");
		if (j->have_modes) tcsetattr(s->tty, TCSADRAIN, &j->modes);
	}

	while (jobstate(j) == 0)
	{
		int status;
		pid_t target = -j->pgid, pid;

		if (!j->own_group)
		{
			hawk_oow_t i;

			for (i = 0; i < j->count; i++)
			{
				if (j->processes[i].state == 0)
				{
					target = j->processes[i].pid;
					break;
				}
			}
		}

		pid = waitpid(target, &status, WUNTRACED);
		if (pid <= -1)
		{
			if (errno == EINTR) continue;
			if (errno == ECHILD)
			{
				hawk_oow_t i;

				for (i = 0; i < j->count; i++)
				{
					if (j->processes[i].state != 2)
					{
						j->processes[i].state = 2;
						j->processes[i].status = 127;
					}
				}
			}
			syserror(sh, "waitpid");
			goto done;
		}

		record_status(j, pid, status);
	}

	result = j->processes[j->count - 1].status;

done:
	if (foreground && s->interactive)
	{
		if (jobstate(j) == 1) j->have_modes = tcgetattr(s->tty, &j->modes) == 0;
		if (tcsetpgrp(s->tty, s->shell_pgid) <= -1)
		{
			syserror(sh, "restore terminal");
			result = -1;
		}
		tcsetattr(s->tty, TCSADRAIN, &s->shell_modes);
	}

	return result;
}

static void freejob (hawk_sh_t* sh, job_t* j)
{
	hawk_gem_freemem(&sh->gem_, j->name);
	hawk_gem_freemem(&sh->gem_, j->processes);
	hawk_gem_freemem(&sh->gem_, j);
}

static void removejob (hawk_sh_t* sh, std_t* s, job_t* j)
{
	job_t** p;

	for (p = &s->jobs; *p; p = &(*p)->next)
	{
		if (*p == j)
		{
			*p = j->next;
			freejob(sh, j);
			return;
		}
	}
}

static job_t* selectjob (std_t* s, const hawk_ooch_t* arg)
{
	unsigned int id = 0;
	job_t* j;
	int jobspec;

	if (!arg) return s->jobs;

	jobspec = *arg == '%';
	if (*arg == '%') arg++;
	if (!*arg) return HAWK_NULL;

	while (*arg)
	{
		if (*arg < '0' || *arg > '9' || id > 100000000) return HAWK_NULL;
		id = id * 10 + *arg++ - '0';
	}

	for (j = s->jobs; j; j = j->next)
	{
		hawk_oow_t i;

		if (jobspec)
		{
			if (j->id == id) return j;
		}
		else
		{
			for (i = 0; i < j->count; i++)
			{
				if ((unsigned long)j->processes[i].pid == id) return j;
			}
		}
	}

	return HAWK_NULL;
}

static unsigned int new_job_id (std_t* s)
{
	unsigned int id = 1;
	job_t* j;

	for (j = s->jobs; j; )
	{
		if (j->id == id)
		{
			id++;
			j = s->jobs;
		}
		else j = j->next;
	}

	return id;
}

static int std_builtin (const hawk_ooch_t* name)
{
	return eq(name, HAWK_T("echo")) || eq(name, HAWK_T("cd")) || eq(name, HAWK_T("pwd")) ||
	       eq(name, HAWK_T("export")) || eq(name, HAWK_T("unset")) || eq(name, HAWK_T("jobs")) ||
	       eq(name, HAWK_T("wait")) || eq(name, HAWK_T("fg")) || eq(name, HAWK_T("bg")) || eq(name, HAWK_T("kill"));
}

static int resumejob (hawk_sh_t* sh, job_t* j)
{
	hawk_oow_t i;

	if (signal_job(j, SIGCONT) <= -1) return syserror(sh, "continue job");
	for (i = 0; i < j->count; i++)
	{
		if (j->processes[i].state == 1) j->processes[i].state = 0;
	}

	return 0;
}

static int call_builtin (hawk_sh_t* sh, std_t* s, const hawk_sh_cmd_t* c, int* status)
{
	const hawk_ooch_t* name;
	hawk_oow_t i;
	int rc = 0, n = hawk_sh_call(sh, c, status);

	if (n != 0) return n;

	name = c->argv[0];
	if (eq(name, HAWK_T("echo")))
	{
		int newline = !(c->argc > 1 && eq(c->argv[1], HAWK_T("-n")));

		for (i = newline? 1: 2; i < c->argc; i++)
		{
			hawk_bch_t* text;

			text = tobytes(sh, c->argv[i]);
			if (!text) return -1;

			if (i > (hawk_oow_t)(newline? 1: 2) && message(sh, 1, " ") <= -1) rc = 1;
			if (message(sh, 1, text) <= -1) rc = 1;
			hawk_gem_freemem(&sh->gem_, text);
		}
		if (newline && message(sh, 1, "\n") <= -1) rc = 1;
	}
	else if (eq(name, HAWK_T("pwd")))
	{
		if (c->argc != 1) rc = 2;
		else if (message(sh, 1, s->cwd) <= -1 || message(sh, 1, "\n") <= -1) rc = 1;
	}
	else if (eq(name, HAWK_T("cd")))
	{
		hawk_bch_t* arg = HAWK_NULL;
		const hawk_bch_t* path;
		hawk_bch_t* joined = HAWK_NULL;
		hawk_bch_t* resolved = HAWK_NULL;
		struct stat st;

		if (c->argc > 2)
		{
			rc = 2;
			goto cd_done;
		}
		if (c->argc == 2)
		{
			arg = tobytes(sh, c->argv[1]);
			if (!arg) return -1;
		}
		path = arg? arg: getenvvalue(s, "HOME");
		if (!path || !*path)
		{
			message(sh, 2, "cd: missing directory or HOME\n");
			rc = 1;
			goto cd_done;
		}
		joined = fullpath(sh, s, path);
		if (!joined)
		{
			hawk_gem_freemem(&sh->gem_, arg);
			return -1;
		}
		resolved = realpath(joined, HAWK_NULL);
		if (!resolved || stat(resolved, &st) <= -1 || !S_ISDIR(st.st_mode) || access(resolved, X_OK) <= -1)
		{
			message(sh, 2, "cd: cannot access directory\n");
			rc = 1;
		}
		else
		{
			hawk_bch_t* copy = hawk_gem_dupbcstr(&sh->gem_, resolved, HAWK_NULL);

			if (!copy) rc = -1;
			else
			{
				hawk_gem_freemem(&sh->gem_, s->cwd);
				s->cwd = copy;
			}
		}

	cd_done:
		if (resolved) free(resolved);
		hawk_gem_freemem(&sh->gem_, joined);
		hawk_gem_freemem(&sh->gem_, arg);
		if (rc <= -1) return -1;
	}
	else if (eq(name, HAWK_T("export")) || eq(name, HAWK_T("unset")))
	{
		int unset = eq(name, HAWK_T("unset"));

		if (c->argc < 2)
		{
			message(sh, 2, "usage: export NAME=VALUE ... / unset NAME ...\n");
			rc = 2;
		}
		for (i = 1; i < c->argc; i++)
		{
			hawk_bch_t* spec;
			hawk_bch_t* equals;
			const hawk_bch_t* value;

			spec = tobytes(sh, c->argv[i]);
			if (!spec) return -1;

			equals = hawk_find_bchar_in_bcstr(spec, '=');
			if (equals) *equals = '\0';
			value = unset? HAWK_NULL: equals? equals + 1: getenvvalue(s, spec);
			if (!validname(spec) || (unset && equals))
			{
				message(sh, 2, "invalid environment variable name\n");
				rc = 2;
			}
			else if (setenvvalue(sh, s, spec, value) <= -1)
			{
				hawk_gem_freemem(&sh->gem_, spec);
				return -1;
			}
			hawk_gem_freemem(&sh->gem_, spec);
		}
	}
	else if (eq(name, HAWK_T("jobs")))
	{
		job_t* j = s->jobs;

		if (!s->in_child) poll_jobs(s);
		while (j)
		{
			job_t* next = j->next;
			hawk_bch_t id[32];
			int state = jobstate(j);

			hawk_fmt_uintmax_to_bcstr(id, HAWK_COUNTOF(id), j->id, 10, -1, 0, HAWK_NULL);
			if (message(sh, 1, "[") <= -1 || message(sh, 1, id) <= -1 || message(sh, 1, "] ") <= -1 ||
			    message(sh, 1, state == 2? "Done ": state == 1? "Stopped ": "Running ") <= -1 ||
			    message(sh, 1, j->name) <= -1 || message(sh, 1, "\n") <= -1) rc = 1;
			if (state == 2) removejob(sh, s, j);
			j = next;
		}
	}
	else if (eq(name, HAWK_T("wait")) || eq(name, HAWK_T("fg")) || eq(name, HAWK_T("bg")))
	{
		int fg = eq(name, HAWK_T("fg")), bg = eq(name, HAWK_T("bg"));
		job_t* j;

		if (s->in_child)
		{
			if (fg || bg) message(sh, 2, "job control requires the owning shell\n");
			*status = fg || bg? 1: 0;
			return 1;
		}
		poll_jobs(s);
		if (c->argc > 2) rc = 2;
		else if (!fg && !bg && c->argc == 1)
		{
			for (j = s->jobs; j; )
			{
				job_t* next = j->next;

				rc = wait_job(sh, s, j, 0);
				if (rc <= -1) return -1;
				if (jobstate(j) == 2) removejob(sh, s, j);
				j = next;
			}
		}
		else
		{
			j = selectjob(s, c->argc == 2? c->argv[1]: HAWK_NULL);
			if (!j)
			{
				message(sh, 2, "no such job\n");
				rc = 127;
			}
			else
			{
				if (fg && s->interactive)
				{
					if (tcsetpgrp(s->tty, j->pgid) <= -1) return syserror(sh, "foreground terminal");
					if (j->have_modes) tcsetattr(s->tty, TCSADRAIN, &j->modes);
				}

				if ((fg || bg) && jobstate(j) != 2 && resumejob(sh, j) <= -1) return -1;

				if (!bg)
				{
					rc = wait_job(sh, s, j, fg);
					if (rc <= -1) return -1;
					if (jobstate(j) == 2) removejob(sh, s, j);
				}
			}
		}
	}
	else if (eq(name, HAWK_T("kill")))
	{
		int sig = SIGTERM;

		i = 1;
		if (i < c->argc && c->argv[i][0] == '-')
		{
			const hawk_ooch_t* opt = c->argv[i++] + 1;

			if (eq(opt, HAWK_T("STOP"))) sig = SIGSTOP;
			else if (eq(opt, HAWK_T("CONT"))) sig = SIGCONT;
			else if (eq(opt, HAWK_T("TERM"))) sig = SIGTERM;
			else if (eq(opt, HAWK_T("KILL"))) sig = SIGKILL;
			else if (eq(opt, HAWK_T("INT"))) sig = SIGINT;
			else if (eq(opt, HAWK_T("HUP"))) sig = SIGHUP;
			else
			{
				sig = fdnumber(opt);
				if (sig <= -1) rc = 2;
			}
		}
		if (i == c->argc) rc = 2;
		for (; !rc && i < c->argc; i++)
		{
			pid_t target;

			if (c->argv[i][0] == '%')
			{
				job_t* j = selectjob(s, c->argv[i]);

				if (!j || jobstate(j) == 2)
				{
					rc = 1;
					break;
				}
				if (sig == SIGCONT) rc = resumejob(sh, j) <= -1? 1: 0;
				else rc = signal_job(j, sig) <= -1? 1: 0;
				continue;
			}
			else
			{
				const hawk_ooch_t* p = c->argv[i];
				hawk_uintmax_t val = 0;

				while (*p >= '0' && *p <= '9' && val < 214748364)
				{
					val = val * 10 + *p++ - '0';
				}
				if (*p || val == 0 || val > 2147483647)
				{
					rc = 2;
					break;
				}
				target = (pid_t)val;
			}
			if (kill(target, sig) <= -1)
			{
				message(sh, 2, "kill: cannot signal target\n");
				rc = 1;
			}
		}
	}
	else return 0;

	*status = rc;
	return 1;
}

static int prefix_assignments (hawk_sh_t* sh, std_t* s, const hawk_sh_cmd_t* c, hawk_sh_cmd_t* view)
{
	hawk_oow_t count = 0;

	while (count < c->argc)
	{
		hawk_bch_t* spec;
		hawk_bch_t* equals;
		int assignment;

		spec = tobytes(sh, c->argv[count]);
		if (!spec) return -1;

		equals = hawk_find_bchar_in_bcstr(spec, '=');
		if (equals) *equals = '\0';
		assignment = equals && validname(spec);
		if (assignment && setenvvalue(sh, s, spec, equals + 1) <= -1)
		{
			hawk_gem_freemem(&sh->gem_, spec);
			return -1;
		}
		hawk_gem_freemem(&sh->gem_, spec);
		if (!assignment) break;
		count++;
	}

	*view = *c;
	view->argc -= count;
	if (view->argv) view->argv += count;

	return 0;
}

static hawk_oow_t command_offset (const hawk_sh_cmd_t* c)
{
	hawk_oow_t i;

	for (i = 0; i < c->argc; i++)
	{
		const hawk_ooch_t* p = c->argv[i];

		if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) break;
		while (*++p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		{
		}
		if (*p != '=') break;
	}

	return i;
}

static void restore_prefix_env (hawk_sh_t* sh, std_t* s, env_backup_t* backup)
{
	while (backup)
	{
		env_backup_t* next = backup->next;
		env_t** p;

		for (p = &s->env; *p; p = &(*p)->next)
		{
			if (hawk_comp_bcstr((*p)->name, backup->entry.name, 0) == 0) break;
		}
		if (*p)
		{
			env_t* e = *p;

			hawk_gem_freemem(&sh->gem_, e->value);
			if (backup->present)
			{
				e->value = backup->entry.value;
				backup->entry.value = HAWK_NULL;
			}
			else
			{
				*p = e->next;
				hawk_gem_freemem(&sh->gem_, e->name);
				hawk_gem_freemem(&sh->gem_, e);
			}
		}
		hawk_gem_freemem(&sh->gem_, backup->entry.name);
		hawk_gem_freemem(&sh->gem_, backup->entry.value);
		hawk_gem_freemem(&sh->gem_, backup);
		backup = next;
	}
}

static int save_prefix_env (hawk_sh_t* sh, std_t* s, const hawk_sh_cmd_t* c, env_backup_t** backup)
{
	hawk_oow_t i, count = command_offset(c);

	if (count == c->argc) return 0; /* assignment-only command is persistent */
	for (i = 0; i < count; i++)
	{
		hawk_bch_t* name;
		env_backup_t* b;
		env_t* old;

		name = tobytes(sh, c->argv[i]);
		if (HAWK_UNLIKELY(!name)) return -1;

		*hawk_find_bchar_in_bcstr(name, '=') = 0;
		for (b = *backup; b; b = b->next)
		{
			if (hawk_comp_bcstr(b->entry.name, name, 0) == 0) break;
		}
		if (b)
		{
			hawk_gem_freemem(&sh->gem_, name);
			continue;
		}
		b = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(*b));
		if (!b)
		{
			hawk_gem_freemem(&sh->gem_, name);
			return -1;
		}
		b->entry.name = name;
		old = findenv(s, name);
		b->present = old != HAWK_NULL;
		if (old && old->value)
		{
			b->entry.value = hawk_gem_dupbcstr(&sh->gem_, old->value, HAWK_NULL);
			if (!b->entry.value)
			{
				hawk_gem_freemem(&sh->gem_, name);
				hawk_gem_freemem(&sh->gem_, b);
				return -1;
			}
		}
		b->next = *backup;
		*backup = b;
	}

	return 0;
}

static void execute_child (hawk_sh_t* sh, std_t* s, const hawk_sh_cmd_t* c)
{
	hawk_sh_cmd_t view;
	int status, handled;
	hawk_bch_t** argv;
	hawk_bch_t* executable = HAWK_NULL;
	hawk_oow_t i;
	env_t* e;
	const hawk_ooch_t* registered;

	if (prefix_assignments(sh, s, c, &view) <= -1) _exit(125);

	handled = call_builtin(sh, s, &view, &status);
	if (handled) _exit(handled <= -1? 125: status & 255);

	registered = sh->mode == HAWK_SH_REGISTERED? hawk_sh_findcmd(sh, view.argv[0]): HAWK_NULL;
	if (sh->mode == HAWK_SH_REGISTERED && !registered)
	{
		message(sh, 2, "command is not registered\n");
		_exit(127);
	}

	argv = hawk_gem_callocmem(&sh->gem_, (view.argc + 1) * HAWK_SIZEOF(*argv));
	if (!argv) _exit(125);
	for (i = 0; i < view.argc; i++)
	{
		argv[i] = tobytes(sh, view.argv[i]);
		if (!argv[i]) _exit(125);
	}

	if (registered)
	{
		executable = tobytes(sh, registered);
		if (!executable) _exit(125);
	}

	for (e = s->env; e; e = e->next)
	{
		int rc = e->value? setenv(e->name, e->value, 1): unsetenv(e->name);

		if (rc <= -1) _exit(125);
	}

	if (chdir(s->cwd) <= -1)
	{
		message(sh, 2, "cannot enter working directory\n");
		_exit(126);
	}

	if (executable) execv(executable, argv);
	else execvp(argv[0], argv);

	status = errno == ENOENT? 127: 126;
	message(sh, 2, argv[0]);
	message(sh, 2, ": cannot execute\n");
	_exit(status);
}

static int std_exec (hawk_sh_t* sh, const hawk_sh_pipeline_t* pipeline, void* ctx)
{
	std_t* s = ctx;
	const hawk_sh_cmd_t* c;
	hawk_oow_t count = 0, index = 0, k;
	int initial[SH_NFD], barrier[2] = { -1, -1 }, previous = -1, status = -1;
	job_t* job = HAWK_NULL;

	initmap(initial);
	if (s->tty >= 0 && s->tty < SH_NFD) initial[s->tty] = -1;
	if (s->input_fd >= 0 && s->input_fd < SH_NFD) initial[s->input_fd] = -1;

	s->minfd = 10;
	for (c = pipeline->commands; c; c = c->next)
	{
		const hawk_sh_redir_t* r;

		count++;
		for (r = c->redirs; r; r = r->next)
		{
			if (r->fd >= s->minfd) s->minfd = r->fd + 1;
			if (r->type == HAWK_SH_DUP)
			{
				int n = fdnumber(r->target);

				if (n >= s->minfd) s->minfd = n + 1;
			}
		}
	}

	poll_jobs(s);
	c = pipeline->commands;

	/* Stateful builtins/callbacks run in the parent only outside pipelines/jobs.
	 * Redirect their callback I/O, never the embedding application's descriptors. */
	if (count == 1 && !pipeline->background &&
	    (command_offset(c) == c->argc || hawk_sh_isbuiltin(sh, c->argv[command_offset(c)]) || std_builtin(c->argv[command_offset(c)])))
	{
		int map[SH_NFD];
		hawk_sh_cmd_t view;
		env_backup_t* backup = HAWK_NULL;

		HAWK_MEMCPY(map, initial, HAWK_SIZEOF(map));
		s->active_fds = map;
		if (redirect(sh, s, c->redirs, map) <= -1)
		{
			message(sh, 2, hawk_sh_geterrbmsg(sh));
			message(sh, 2, "\n");
			status = 1;
		}
		else if (save_prefix_env(sh, s, c, &backup) <= -1) status = -1;
		else if (prefix_assignments(sh, s, c, &view) <= -1) status = -1;
		else
		{
			int handled = call_builtin(sh, s, &view, &status);

			if (handled <= 0) status = -1;
		}

		restore_prefix_env(sh, s, backup);
		s->active_fds = HAWK_NULL;
		closemap(map);
		return status;
	}

	job = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(*job));
	if (!job) return -1;

	job->processes = hawk_gem_callocmem(&sh->gem_, count * HAWK_SIZEOF(*job->processes));
	job->name = c->argc? tobytes(sh, c->argv[0]): hawk_gem_dupbcstr(&sh->gem_, "redirection", HAWK_NULL);
	if (!job->processes || !job->name) goto fail;

	job->id = pipeline->background? new_job_id(s): 0;

	/* Batch foreground children retain the caller's process group so terminal
	 * signals reach both the coordinator and its children, without installing
	 * process-global forwarding handlers in an embedding application. */
	job->own_group = s->interactive || pipeline->background;
	if (makepipe(barrier, s->minfd) <= -1)
	{
		syserror(sh, "launch barrier");
		goto fail;
	}

	for (c = pipeline->commands; c; c = c->next, index++)
	{
		int pipes[2] = { -1, -1 }, map[SH_NFD];
		pid_t pid;

		HAWK_MEMCPY(map, initial, HAWK_SIZEOF(map));
		if (c->next && makepipe(pipes, s->minfd) <= -1)
		{
			syserror(sh, "pipeline");
			goto fail;
		}
		if (previous >= 0)
		{
			map[0] = previous;
			previous = -1;
		}
		else if (pipeline->background && !s->interactive)
		{
			map[0] = movefd(open("/dev/null", O_RDONLY), s->minfd);
			if (map[0] <= -1)
			{
				if (pipes[0] >= 0) close(pipes[0]);
				if (pipes[1] >= 0) close(pipes[1]);
				syserror(sh, "background stdin");
				goto fail;
			}
		}
		if (pipes[1] >= 0) map[1] = pipes[1];

		pid = fork();
		if (pid == 0)
		{
			hawk_bch_t dummy;
			struct sigaction dfl;

			HAWK_MEMSET(&dfl, 0, HAWK_SIZEOF(dfl));
			dfl.sa_handler = SIG_DFL;
			sigemptyset(&dfl.sa_mask);
			sigaction(SIGINT, &dfl, HAWK_NULL);
			sigaction(SIGQUIT, &dfl, HAWK_NULL);
			sigaction(SIGTSTP, &dfl, HAWK_NULL);
			sigaction(SIGTTIN, &dfl, HAWK_NULL);
			sigaction(SIGTTOU, &dfl, HAWK_NULL);
			sigaction(SIGPIPE, &dfl, HAWK_NULL);
			sigaction(SIGCHLD, &dfl, HAWK_NULL);
			sigprocmask(SIG_SETMASK, &dfl.sa_mask, HAWK_NULL);
			if (job->own_group && setpgid(0, job->pgid? job->pgid: 0) <= -1) _exit(125);
			close(barrier[1]);
			if (pipes[0] >= 0) close(pipes[0]);
			while (read(barrier[0], &dummy, 1) <= -1 && errno == EINTR)
			{
			}
			close(barrier[0]);
			if (s->tty >= 0)
			{
				close(s->tty);
				s->tty = -1;
			}
			if (s->input_fd >= 0)
			{
				close(s->input_fd);
				s->input_fd = -1;
			}
			/* Open redirections only after every stage exists. Opening a FIFO
			 * in the parent would block before its peer could be launched. */
			s->active_fds = map;
			if (redirect(sh, s, c->redirs, map) <= -1)
			{
				message(sh, 2, hawk_sh_geterrbmsg(sh));
				message(sh, 2, "\n");
				_exit(1);
			}
			s->active_fds = HAWK_NULL;
			for (k = 0; k < SH_NFD; k++)
			{
				if (map[k] == (int)k) continue;
				if (map[k] <= -1) close(k);
				else if (dup2(map[k], k) <= -1) _exit(125);
			}
			closemap(map);
			s->active_fds = HAWK_NULL;
			s->interactive = 0;
			s->in_child = 1;
			execute_child(sh, s, c);
			_exit(125);
		}

		closemap(map);
		if (pid <= -1)
		{
			if (pipes[0] >= 0) close(pipes[0]);
			syserror(sh, "fork");
			goto fail;
		}
		job->processes[index].pid = pid;
		job->count++;
		if (!job->pgid) job->pgid = pid;
		if (job->own_group && setpgid(pid, job->pgid) <= -1 && errno != EACCES && errno != ESRCH)
		{
			if (pipes[0] >= 0) close(pipes[0]);
			syserror(sh, "process group");
			goto fail;
		}
		previous = pipes[0];
	}

	if (!pipeline->background && s->interactive && tcsetpgrp(s->tty, job->pgid) <= -1)
	{
		syserror(sh, "foreground terminal");
		goto fail;
	}

	close(barrier[0]);
	barrier[0] = -1;
	close(barrier[1]);
	barrier[1] = -1;
	job->next = s->jobs;
	s->jobs = job;

	if (pipeline->background)
	{
		s->last_pid = job->processes[count - 1].pid;
		return 0;
	}

	status = wait_job(sh, s, job, 1);
	if (jobstate(job) == 2) removejob(sh, s, job);
	else job->id = new_job_id(s);
	return status;

fail:
	if (previous >= 0) close(previous);
	if (job)
	{
		/* Kill individually too: a failure may precede process-group setup. */
		for (k = 0; k < job->count; k++)
		{
			kill(job->processes[k].pid, SIGKILL);
		}
	}
	if (barrier[0] >= 0) close(barrier[0]);
	if (barrier[1] >= 0) close(barrier[1]);
	if (job)
	{
		for (k = 0; k < job->count; k++)
		{
			int st;

			while (waitpid(job->processes[k].pid, &st, 0) <= -1 && errno == EINTR)
			{
				/* wait again */
			}
		}
		freejob(sh, job);
	}
	if (s->interactive) tcsetpgrp(s->tty, s->shell_pgid);
	return status;
}

int hawk_sh_setinteractivestd (hawk_sh_t* sh, int enabled)
{
	static const int signals[5] = { SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU };
	std_t* s = sh->std_ctx;
	int i;
	struct sigaction ignore;

	if (!sh->std_close)
	{
		hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}
	if (!!enabled == s->interactive) return 0;
	if (!enabled)
	{
		tcsetpgrp(s->tty, s->original_pgid);
		tcsetattr(s->tty, TCSADRAIN, &s->shell_modes);
		for (i = 0; i < 5; i++)
		{
			sigaction(signals[i], &s->old_signals[i], HAWK_NULL);
		}
		close(s->tty);
		s->tty = -1;
		s->interactive = 0;
		return 0;
	}

	s->tty = movefd(open("/dev/tty", O_RDWR), 10);
	if (s->tty <= -1) return syserror(sh, "open controlling terminal");

	s->shell_pgid = getpgrp();
	s->original_pgid = tcgetpgrp(s->tty);
	if (s->original_pgid != s->shell_pgid || tcgetattr(s->tty, &s->shell_modes) <= -1)
	{
		close(s->tty);
		s->tty = -1;
		errno = EPERM;
		return syserror(sh, "shell must own the foreground terminal");
	}
	HAWK_MEMSET(&ignore, 0, HAWK_SIZEOF(ignore));
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	for (i = 0; i < 5; i++)
	{
		if (sigaction(signals[i], &ignore, &s->old_signals[i]) <= -1)
		{
			int e = errno;

			while (i > 0)
			{
				i--;
				sigaction(signals[i], &s->old_signals[i], HAWK_NULL);
			}
			close(s->tty);
			s->tty = -1;
			errno = e;
			return syserror(sh, "interactive signals");
		}
	}
	s->interactive = 1;
	return 0;
}

static void std_close (hawk_sh_t* sh)
{
	std_t* s = sh->std_ctx;
	job_t* j;

	poll_jobs(s);
	for (j = s->jobs; j; j = j->next)
	{
		if (jobstate(j) != 2) signal_job(j, SIGKILL);
	}

	while (s->jobs)
	{
		hawk_oow_t i;

		j = s->jobs;
		for (i = 0; i < j->count; i++)
		{
			if (j->processes[i].state != 2)
			{
				int st;

				while (waitpid(j->processes[i].pid, &st, 0) <= -1 && errno == EINTR)
				{
					/* wait again*/
				}
			}
		}
		removejob(sh, s, j);
	}

	if (s->interactive) hawk_sh_setinteractivestd(sh, 0);

	while (s->env)
	{
		env_t* e = s->env;

		s->env = e->next;
		hawk_gem_freemem(&sh->gem_, e->name);
		hawk_gem_freemem(&sh->gem_, e->value);
		hawk_gem_freemem(&sh->gem_, e);
	}

	hawk_gem_freemem(&sh->gem_, s->env_result);
	hawk_gem_freemem(&sh->gem_, s->cwd);
	hawk_gem_freemem(&sh->gem_, s);
}

hawk_sh_t* hawk_sh_openstd (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	hawk_sh_t* sh;
	std_t* s;
	hawk_bch_t* cwd;
	hawk_oow_t size = 256;

	sh = hawk_sh_open(mmgr, xtnsize, cmgr, errinf);
	if (HAWK_UNLIKELY(!sh)) return HAWK_NULL;

	s = hawk_gem_callocmem(&sh->gem_, HAWK_SIZEOF(*s));
	if (HAWK_UNLIKELY(!s)) goto fail;

	s->tty = -1;
	s->input_fd = -1;
	sh->std_ctx = s;
	sh->prm_ctx = s;
	sh->std_close = std_close;

	while (1)
	{
		cwd = hawk_gem_allocmem(&sh->gem_, size);
		if (HAWK_UNLIKELY(!cwd)) goto fail;

		if (getcwd(cwd, size))
		{
			s->cwd = cwd;
			break;
		}

		hawk_gem_freemem(&sh->gem_, cwd);
		if (errno != ERANGE || size > HAWK_TYPE_MAX(hawk_oow_t) / 2)
		{
			syserror(sh, "working directory");
			goto fail;
		}
		size *= 2;
	}

	sh->prm.exec = std_exec;
	sh->prm.read = std_read;
	sh->prm.write = std_write;
	sh->prm.getenv = std_getenv;

	return sh;

fail:
	if (errinf) hawk_gem_geterrinf(&sh->gem_, errinf);
	hawk_sh_close(sh);
	return HAWK_NULL;
}

static int input_char (hawk_sh_t* sh, hawk_ooch_t* ch, void* ctx)
{
	input_t* in = ctx;
	std_t* s = sh->std_ctx;
	hawk_bch_t bytes[HAWK_BCSIZE_MAX];
	hawk_oow_t len = 0;

	if (s->interactive && in->bol)
	{
		message(sh, 2, sh->reading_heredoc? "> ": "$ ");
		in->bol = 0;
	}

	while (1)
	{
		ssize_t n;

		do
		{
			n = read(in->fd, bytes + len, 1);
		}
		while (n <= -1 && errno == EINTR);
		if (n <= -1)
		{
			return syserror(sh, "shell input");
		}
		else if (n == 0)
		{
			in->eof = 1;
			if (len) goto encoding_error;
			return 0;
		}
		len++;
	#if defined(HAWK_OOCH_IS_UCH)
		{
			hawk_oow_t used = sh->gem_.cmgr->bctouc(bytes, len, ch);

			if (used == 0) goto encoding_error;
			if (used > len)
			{
				if (len == HAWK_COUNTOF(bytes)) goto encoding_error;
				continue;
			}
		}
	#else
		*ch = (hawk_bchu_t)bytes[0];
	#endif
		in->bol = *ch == '\n';
		return 1;
	}

encoding_error:
	hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_EECERR);
	return -1;
}

int hawk_sh_execstd (hawk_sh_t* sh, const hawk_ooch_t* path)
{
	input_t input;
	std_t* s = sh->std_ctx;
	int rc;

	if (sh->running)
	{
		hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_ESTATE);
		return -1;
	}
	if (!sh->std_close)
	{
		hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	input.fd = 0;
	input.bol = 1;
	input.eof = 0;

	if (path)
	{
		hawk_bch_t* bytes;
		hawk_bch_t* absolute;

		bytes = tobytes(sh, path);
		if (!bytes) return -1;

		absolute = fullpath(sh, s, bytes);
		hawk_gem_freemem(&sh->gem_, bytes);
		if (!absolute) return -1;

		input.fd = movefd(open(absolute, O_RDONLY), 10);
		hawk_gem_freemem(&sh->gem_, absolute);
		if (input.fd <= -1) return syserror(sh, "open shell input");

		s->input_fd = input.fd;
	}

	rc = hawk_sh_exec(sh, input_char, &input);
	while (rc <= -1 && s->interactive && !input.eof)
	{
		hawk_ooch_t ch;
		const hawk_loc_t* loc;

		loc = hawk_sh_geterrloc(sh);
		if (loc->line > 0)
		{
			hawk_bch_t line[HAWK_SIZEOF(hawk_oow_t) * 3 + 1];

			hawk_fmt_uintmax_to_bcstr(line, HAWK_COUNTOF(line), loc->line, 10, -1, 0, HAWK_NULL);
			message(sh, 2, "line ");
			message(sh, 2, line);
			message(sh, 2, ": ");
		}
		message(sh, 2, hawk_sh_geterrbmsg(sh));
		message(sh, 2, "\n");
		/* The lexer may have already read the erroneous line's newline as
		 * lookahead. Don't discard the following command in that case. */
		while (!input.bol && !input.eof)
		{
			if (input_char(sh, &ch, &input) <= -1) goto done;
		}
		sh->status = 2;
		rc = hawk_sh_exec(sh, input_char, &input);
	}

done:
	if (path)
	{
		close(input.fd);
		s->input_fd = -1;
	}

	return rc;
}

#else

hawk_sh_t* hawk_sh_openstd (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	if (errinf)
	{
		HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
		errinf->num = HAWK_ENOIMPL;
		hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(HAWK_ENOIMPL));
	}

	return HAWK_NULL;
}

int hawk_sh_setinteractivestd (hawk_sh_t* sh, int enabled)
{
	hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
}

int hawk_sh_execstd (hawk_sh_t* sh, const hawk_ooch_t* path)
{
	hawk_gem_seterrnum(&sh->gem_, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
}
#endif
