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

#if !defined(_GNU_SOURCE)
#	define _GNU_SOURCE
#endif

#include <hawk-mux.h>
#include "hawk-prv.h"

#if defined(_WIN32)
#	include <winsock2.h>
#	include <windows.h>
#	define MUX_HAVE_SELECT
#elif !defined(__OS2__) && !defined(__DOS__)
#	include "syscall.h"
#	if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
#		include <sys/epoll.h>
#		define MUX_HAVE_EPOLL
#	endif
#	if defined(HAVE_SYS_EVENT_H) && defined(HAVE_KQUEUE) && defined(HAVE_KEVENT)
#		include <sys/event.h>
#		define MUX_HAVE_KQUEUE
#	endif
#	if defined(HAVE_POLL_H) && defined(HAVE_POLL)
#		include <poll.h>
#		define MUX_HAVE_POLL
#	endif
#	if defined(HAVE_SELECT)
#		if defined(HAVE_SYS_SELECT_H)
#			include <sys/select.h>
#		endif
#		define MUX_HAVE_SELECT
#	endif
#endif

typedef struct mux_watch_t mux_watch_t;
struct mux_watch_t
{
	hawk_mux_hnd_t hnd;
	hawk_uint32_t mask;
	hawk_uint64_t token;
	hawk_uintptr_t cookie; /* kqueue udata: integer identity, not a pointer */
	hawk_oow_t result;
	int armed;
};

struct hawk_mux_t
{
	hawk_oow_t instsize_;
	hawk_gem_t gem_;
	hawk_mux_backend_t backend;
	int automatic;
	int fd;
	int dirty;
	mux_watch_t* watch;
	hawk_oow_t count;
	hawk_oow_t capacity;
	hawk_uintptr_t cookie;
#if !defined(_WIN32)
	hawk_oow_t* map; /* raw descriptor -> dense watch index + 1 */
	hawk_oow_t map_capacity;
#endif
	void* native_events;
	hawk_oow_t native_capacity;
};

static HAWK_INLINE int fail_mux (hawk_mux_t* mux, hawk_errnum_t num)
{
	hawk_gem_seterrnum(&mux->gem_, HAWK_NULL, num);
	return -1;
}

static int fail_mux_native (hawk_mux_t* mux)
{
#if defined(_WIN32)
	switch (WSAGetLastError())
	{
		case WSAEINTR:
			return fail_mux(mux, HAWK_EINTR);

		case WSAEINVAL:
		case WSAENOTSOCK:
		case WSAEFAULT:
			return fail_mux(mux, HAWK_EINVAL);

		case WSAEACCES:
			return fail_mux(mux, HAWK_EACCES);
		case WSAENOBUFS:
			return fail_mux(mux, HAWK_ENOMEM);

		default:
			return fail_mux(mux, HAWK_ESYSERR);
	}
#elif defined(__OS2__) || defined(__DOS__)
	return fail_mux(mux, HAWK_ENOIMPL);
#else
	return fail_mux(mux, hawk_errno_to_errnum(errno));
#endif
}

static int reserve_mux (hawk_mux_t* mux, void** ptr, hawk_oow_t* capacity, hawk_oow_t count, hawk_oow_t unit)
{
	hawk_oow_t capa;
	void* tmp;

	if (count <= *capacity) return 0;
	if (count > (HAWK_TYPE_MAX(hawk_oow_t) - 63) ||
	    (capa = HAWK_ALIGN(count, 64)) > HAWK_TYPE_MAX(hawk_oow_t) / unit)
	{
		return fail_mux(mux, HAWK_ENOMEM);
	}

	tmp = hawk_gem_reallocmem(&mux->gem_, *ptr, capa * unit);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	*ptr = tmp;
	*capacity = capa;
	return 0;
}

static hawk_oow_t find_watch (hawk_mux_t* mux, hawk_mux_hnd_t hnd)
{
#if defined(_WIN32)
	hawk_oow_t i;

	for (i = 0; i < mux->count; i++)
	{
		if (mux->watch[i].hnd == hnd) return i;
	}
#else
	if (hnd >= 0 && (hawk_oow_t)hnd < mux->map_capacity && mux->map[hnd] != 0)
		return mux->map[hnd] - 1;
#endif
	return HAWK_TYPE_MAX(hawk_oow_t);
}

#if defined(MUX_HAVE_EPOLL) || defined(MUX_HAVE_KQUEUE)
static int open_native_mux (hawk_mux_t* mux)
{
	int fd = -1;

	switch (mux->backend)
	{
#if defined(MUX_HAVE_EPOLL)
		case HAWK_MUX_BACKEND_EPOLL:
#	if defined(HAVE_EPOLL_CREATE1) && defined(EPOLL_CLOEXEC)
			fd = epoll_create1(EPOLL_CLOEXEC);
			if (fd >= 0 || errno != ENOSYS) return fd;
#	endif
			fd = epoll_create(64);
			break;
#endif
#if defined(MUX_HAVE_KQUEUE)
		case HAWK_MUX_BACKEND_KQUEUE:
			fd = kqueue();
			break;
#endif
		default:
			return -1;
	}

#if defined(MUX_HAVE_EPOLL) || defined(MUX_HAVE_KQUEUE)
	if (fd >= 0)
	{
		int flags = fcntl(fd, F_GETFD);

		if (flags <= -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) <= -1)
		{
			int err = errno;

			close(fd);
			errno = err;
			return -1;
		}
	}
#endif
	return fd;
}
#endif

#if defined(MUX_HAVE_KQUEUE)
static int ctl_kqueue (int fd, hawk_mux_hnd_t hnd, hawk_uint32_t oldmask, hawk_uint32_t mask, hawk_uintptr_t cookie)
{
	struct kevent ev;
	int i;
	const hawk_uint32_t bits[2] = { HAWK_MUX_IN, HAWK_MUX_OUT };
	const short filters[2] = { EVFILT_READ, EVFILT_WRITE };

	for (i = 0; i < 2; i++)
	{
		if (!((oldmask | mask) & bits[i])) continue;
		EV_SET(&ev, hnd, filters[i], (mask & bits[i])? EV_ADD | EV_ENABLE: EV_DELETE,
			0, 0, (void*)cookie);
		if (kevent(fd, &ev, 1, HAWK_NULL, 0, HAWK_NULL) <= -1) return -1;
	}
	return 0;
}

static int repair_kqueue (hawk_mux_t* mux)
{
	hawk_oow_t i;
	int fd;

	if (!mux->dirty) return 0;

	fd = open_native_mux(mux);
	if (fd <= -1) return fail_mux_native(mux);

	for (i = 0; i < mux->count; i++)
	{
		mux_watch_t* w = &mux->watch[i];

		if (w->armed && ctl_kqueue(fd, w->hnd, 0, w->mask, w->cookie) <= -1)
		{
			fail_mux_native(mux);
			close(fd);
			return -1;
		}
	}
	close(mux->fd);
	mux->fd = fd;
	mux->dirty = 0;
	return 0;
}
#endif

hawk_mux_t* hawk_mux_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_mux_backend_t backend, hawk_errinf_t* errinf)
{
	hawk_mux_t* mux;

	if (xtnsize > HAWK_TYPE_MAX(hawk_oow_t) - HAWK_SIZEOF(*mux)) mux = HAWK_NULL;
	else mux = (hawk_mux_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(*mux) + xtnsize);
	if (!mux)
	{
		if (errinf)
		{
			HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
			errinf->num = HAWK_ENOMEM;
			hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(HAWK_ENOMEM));
		}
		return HAWK_NULL;
	}

	HAWK_MEMSET(mux, 0, HAWK_SIZEOF(*mux) + xtnsize);
	mux->instsize_ = HAWK_SIZEOF(*mux);
	mux->gem_.mmgr = mmgr;
	mux->gem_.cmgr = cmgr;
	mux->gem_.errstr = hawk_dfl_errstr;

	mux->fd = -1;
	mux->automatic = (backend == HAWK_MUX_BACKEND_AUTO);
	if (backend == HAWK_MUX_BACKEND_AUTO)
	{
#if defined(MUX_HAVE_EPOLL)
		backend = HAWK_MUX_BACKEND_EPOLL;
#elif defined(MUX_HAVE_KQUEUE)
		backend = HAWK_MUX_BACKEND_KQUEUE;
#elif defined(MUX_HAVE_POLL)
		backend = HAWK_MUX_BACKEND_POLL;
#elif defined(MUX_HAVE_SELECT)
		backend = HAWK_MUX_BACKEND_SELECT;
#endif
	}
	mux->backend = backend;

	switch (backend)
	{
#if defined(MUX_HAVE_EPOLL)
		case HAWK_MUX_BACKEND_EPOLL:
#endif
#if defined(MUX_HAVE_KQUEUE)
		case HAWK_MUX_BACKEND_KQUEUE:
#endif
#if defined(MUX_HAVE_EPOLL) || defined(MUX_HAVE_KQUEUE)
			mux->fd = open_native_mux(mux);
			if (mux->fd <= -1)
			{
				fail_mux_native(mux);
				goto oops;
			}
			break;
#endif
#if defined(MUX_HAVE_POLL)
		case HAWK_MUX_BACKEND_POLL:
			break;
#endif
#if defined(MUX_HAVE_SELECT)
		case HAWK_MUX_BACKEND_SELECT:
			break;
#endif
		default:
			fail_mux(mux, HAWK_ENOIMPL);
			goto oops;
	}
	return mux;

oops:
	if (errinf) hawk_gem_geterrinf(&mux->gem_, errinf);
	HAWK_MMGR_FREE(mmgr, mux);
	return HAWK_NULL;
}

int hawk_mux_close (hawk_mux_t* mux, hawk_errinf_t* errinf)
{
	int n = 0;
	hawk_mmgr_t* mmgr = mux->gem_.mmgr;

#if defined(MUX_HAVE_EPOLL) || defined(MUX_HAVE_KQUEUE)
	if (mux->fd >= 0 && close(mux->fd) <= -1)
	{
		n = fail_mux_native(mux);
		if (errinf) hawk_gem_geterrinf(&mux->gem_, errinf);
	}
#endif
	if (mux->watch) hawk_gem_freemem(&mux->gem_, mux->watch);
#if !defined(_WIN32)
	if (mux->map) hawk_gem_freemem(&mux->gem_, mux->map);
#endif
	if (mux->native_events) hawk_gem_freemem(&mux->gem_, mux->native_events);

	HAWK_MMGR_FREE(mmgr, mux);
	return n;
}

hawk_gem_t* hawk_mux_getgem (hawk_mux_t* mux)
{
	return &mux->gem_;
}

void* hawk_mux_getxtn (hawk_mux_t* mux)
{
	return (hawk_uint8_t*)mux + mux->instsize_;
}

hawk_mux_backend_t hawk_mux_getbackend (hawk_mux_t* mux)
{
	return mux->backend;
}

int hawk_mux_ctl (hawk_mux_t* mux, hawk_mux_cmd_t cmd, hawk_mux_hnd_t hnd, hawk_uint32_t mask, hawk_uint64_t token)
{
	hawk_oow_t index;
	hawk_uintptr_t cookie;
	mux_watch_t* w;

#if defined(_WIN32)
	if (hnd == (hawk_mux_hnd_t)INVALID_SOCKET) return fail_mux(mux, HAWK_EINVAL);
#else
	if (hnd < 0) return fail_mux(mux, HAWK_EINVAL);
#endif

	if (cmd != HAWK_MUX_ADD && cmd != HAWK_MUX_MOD && cmd != HAWK_MUX_DEL)
		return fail_mux(mux, HAWK_EINVAL);

	if (cmd != HAWK_MUX_DEL && (mask & ~(hawk_uint32_t)(HAWK_MUX_IN | HAWK_MUX_OUT |
	    HAWK_MUX_PRI | HAWK_MUX_ERR | HAWK_MUX_HUP | HAWK_MUX_RDHUP | HAWK_MUX_ONESHOT)))
		return fail_mux(mux, HAWK_EINVAL);

	if (mux->backend == HAWK_MUX_BACKEND_KQUEUE && cmd != HAWK_MUX_DEL && (mask & HAWK_MUX_PRI))
		return fail_mux(mux, HAWK_ENOIMPL);

	index = find_watch(mux, hnd);
	if (cmd == HAWK_MUX_ADD)
	{
		if (index != HAWK_TYPE_MAX(hawk_oow_t)) return fail_mux(mux, HAWK_EEXIST);
#if defined(MUX_HAVE_SELECT)
		if (mux->backend == HAWK_MUX_BACKEND_SELECT)
		{
#if defined(_WIN32)
			if (mux->count >= FD_SETSIZE) return fail_mux(mux, HAWK_EINVAL);
#else
			if (hnd >= FD_SETSIZE) return fail_mux(mux, HAWK_EINVAL);
#endif
		}
#endif
#if !defined(_WIN32)
		{
			hawk_oow_t old = mux->map_capacity;
			void* tmp = mux->map;

			if (reserve_mux(mux, &tmp, &mux->map_capacity, (hawk_oow_t)hnd + 1, HAWK_SIZEOF(*mux->map)) <= -1)
				return -1;
			mux->map = tmp;
			HAWK_MEMSET(mux->map + old, 0, (mux->map_capacity - old) * HAWK_SIZEOF(*mux->map));
		}
#endif
		{
			void* tmp = mux->watch;

			if (reserve_mux(mux, &tmp, &mux->capacity, mux->count + 1, HAWK_SIZEOF(*mux->watch)) <= -1)
				return -1;
			mux->watch = tmp;
		}
		index = mux->count;
	}
	else if (index == HAWK_TYPE_MAX(hawk_oow_t))
	{
		return fail_mux(mux, HAWK_ENOENT);
	}

	if (cmd != HAWK_MUX_DEL && mux->cookie >= HAWK_TYPE_MAX(hawk_uint32_t))
		return fail_mux(mux, HAWK_ENOMEM);

	cookie = mux->cookie;
	if (cmd != HAWK_MUX_DEL) cookie = ++mux->cookie;
	w = &mux->watch[index];

	switch (mux->backend)
	{
#if defined(MUX_HAVE_EPOLL)
		case HAWK_MUX_BACKEND_EPOLL:
		{
			struct epoll_event ev;
			int op;

			HAWK_MEMSET(&ev, 0, HAWK_SIZEOF(ev));
			ev.events = mask;
			/* Decode the descriptor and generation without a pointer, even on
			 * 32-bit targets. The caller's full 64-bit token stays in the watch. */
			ev.data.u64 = ((hawk_uint64_t)cookie << 32) | (hawk_uint32_t)hnd;
			op = (cmd == HAWK_MUX_ADD)? EPOLL_CTL_ADD: (cmd == HAWK_MUX_DEL)? EPOLL_CTL_DEL: EPOLL_CTL_MOD;
			if (epoll_ctl(mux->fd, op, hnd, &ev) <= -1) return fail_mux_native(mux);
			break;
		}
#endif
#if defined(MUX_HAVE_KQUEUE)
		case HAWK_MUX_BACKEND_KQUEUE:
			if (repair_kqueue(mux) <= -1) return -1;
			if (ctl_kqueue(mux->fd, hnd, cmd == HAWK_MUX_ADD || !w->armed? 0: w->mask, cmd == HAWK_MUX_DEL? 0: mask, cookie) <= -1)
			{
				/* a two-filter change may have partially succeeded. rebuild from
				 * the unchanged logical watches before any further control/wait. */
				mux->dirty = 1;
			#if defined(MUX_HAVE_SELECT) && defined(EOPNOTSUPP)
				if (mux->automatic && cmd == HAWK_MUX_ADD && errno == EOPNOTSUPP && hnd < FD_SETSIZE)
				{
					hawk_oow_t i;

					/* older darwin/macosx kqueue rejects ptys. preserve the watches and
					 * use select; poll on those systems also lacks pty support. */
					for (i = 0; i < mux->count && mux->watch[i].hnd < FD_SETSIZE; i++) /* loop */;
					if (i == mux->count)
					{
						close(mux->fd);
						mux->fd = -1;
						/* all watch descriptors are in mux->watch.
						 * the mux loop will just use select() if the backend
						 * type is set to SELECT */
						mux->backend = HAWK_MUX_BACKEND_SELECT;
						mux->dirty = 0;
						break;
					}
				}
			#endif
				return fail_mux_native(mux);
			}
			break;
#endif
		default:
			break;
	}

	if (cmd == HAWK_MUX_DEL)
	{
#if !defined(_WIN32)
		mux->map[hnd] = 0;
#endif
		mux->count--;
		if (index != mux->count)
		{
			*w = mux->watch[mux->count];
#if !defined(_WIN32)
			mux->map[w->hnd] = index + 1;
#endif
		}
	}
	else
	{
		w->hnd = hnd;
		w->mask = mask;
		w->token = token;
		w->cookie = cookie;
		w->armed = 1;
		if (cmd == HAWK_MUX_ADD)
		{
			mux->count++;
#if !defined(_WIN32)
			mux->map[hnd] = index + 1;
#endif
		}
	}
	return 0;
}

static int timeout_msec (hawk_mux_t* mux, const hawk_ntime_t* timeout, int* msec)
{
	hawk_oow_t fraction;

	if (!timeout)
	{
		*msec = -1;
		return 0;
	}

	fraction = (timeout->nsec + HAWK_NSECS_PER_MSEC - 1) / HAWK_NSECS_PER_MSEC;
	if (timeout->sec > HAWK_TYPE_MAX(int) / HAWK_MSECS_PER_SEC ||
	    (timeout->sec == HAWK_TYPE_MAX(int) / HAWK_MSECS_PER_SEC &&
	     fraction > HAWK_TYPE_MAX(int) % HAWK_MSECS_PER_SEC))
	{
		return fail_mux(mux, HAWK_EINVAL);
	}
	*msec = timeout->sec * HAWK_MSECS_PER_SEC + fraction;
	return 0;
}

hawk_ooi_t hawk_mux_wait (hawk_mux_t* mux, hawk_mux_evt_t* events, hawk_oow_t capacity, const hawk_ntime_t* timeout)
{
	hawk_oow_t i, count = 0;
	int n;

	if (!events || capacity == 0 || capacity > HAWK_TYPE_MAX(int) ||
	    (timeout && (timeout->sec < 0 || timeout->nsec < 0 || timeout->nsec >= HAWK_NSECS_PER_SEC)))
	{
		return fail_mux(mux, HAWK_EINVAL);
	}

	switch (mux->backend)
	{
#if defined(MUX_HAVE_EPOLL)
		case HAWK_MUX_BACKEND_EPOLL:
		{
			struct epoll_event* ev;
			int msec;

			if (timeout_msec(mux, timeout, &msec) <= -1 ||
			    reserve_mux(mux, &mux->native_events, &mux->native_capacity, capacity, HAWK_SIZEOF(*ev)) <= -1)
			{
				return -1;
			}

			ev = mux->native_events;
			n = epoll_wait(mux->fd, ev, capacity, msec);
			if (n <= -1) return fail_mux_native(mux);

			for (i = 0; i < (hawk_oow_t)n; i++)
			{
				hawk_uint32_t hnd = (hawk_uint32_t)ev[i].data.u64;
				hawk_oow_t index;
				mux_watch_t* w;

				if (hnd > HAWK_TYPE_MAX(int)) continue;

				index = find_watch(mux, (int)hnd);
				if (index == HAWK_TYPE_MAX(hawk_oow_t)) continue;

				w = &mux->watch[index];
				if (w->cookie != (ev[i].data.u64 >> 32) || !w->armed) continue;

				events[count].hnd = w->hnd;
				events[count].mask = ev[i].events;
				events[count++].token = w->token;
				if (w->mask & HAWK_MUX_ONESHOT) w->armed = 0;
			}
			return count;
		}
#endif
#if defined(MUX_HAVE_KQUEUE)
		case HAWK_MUX_BACKEND_KQUEUE:
		{
			struct kevent* ev;
			struct timespec ts;
			struct timespec* tsp = HAWK_NULL;

			if (repair_kqueue(mux) <= -1 ||
			    reserve_mux(mux, &mux->native_events, &mux->native_capacity, capacity, HAWK_SIZEOF(*ev)) <= -1)
			{
				return -1;
			}

			if (timeout)
			{
				ts.tv_sec = timeout->sec;
				ts.tv_nsec = timeout->nsec;
				if ((hawk_intmax_t)ts.tv_sec != timeout->sec) return fail_mux(mux, HAWK_EINVAL);
				tsp = &ts;
			}
			ev = mux->native_events;
			n = kevent(mux->fd, HAWK_NULL, 0, ev, capacity, tsp);
			if (n <= -1) return fail_mux_native(mux);

			for (i = 0; i < mux->count; i++) mux->watch[i].result = capacity;

			for (i = 0; i < (hawk_oow_t)n; i++)
			{
				hawk_oow_t index;
				mux_watch_t* w;
				hawk_uint32_t mask;

				if (ev[i].ident > HAWK_TYPE_MAX(int)) continue;

				index = find_watch(mux, (int)ev[i].ident);
				if (index == HAWK_TYPE_MAX(hawk_oow_t)) continue;

				w = &mux->watch[index];
				if ((hawk_uintptr_t)ev[i].udata != w->cookie || !w->armed) continue;

				mask = ev[i].filter == EVFILT_READ? HAWK_MUX_IN: HAWK_MUX_OUT;
				if (ev[i].flags & EV_EOF) mask |= HAWK_MUX_HUP;
				if (ev[i].flags & EV_ERROR) mask |= HAWK_MUX_ERR;
				if (w->result == capacity)
				{
					w->result = count;
					events[count].hnd = w->hnd;
					events[count].mask = mask;
					events[count++].token = w->token;
				}
				else
				{
					events[w->result].mask |= mask;
				}
			}
			/* disable both filters together for a portable whole-watch one-shot. */
			for (i = 0; i < mux->count; i++)
			{
				mux_watch_t* w = &mux->watch[i];

				if (w->result != capacity && (w->mask & HAWK_MUX_ONESHOT))
				{
					w->armed = 0;
					if (ctl_kqueue(mux->fd, w->hnd, w->mask, 0, 0) <= -1) mux->dirty = 1;
				}
			}
			return count;
		}
#endif
#if defined(MUX_HAVE_POLL)
		case HAWK_MUX_BACKEND_POLL:
		{
			struct pollfd* ev;
			int msec;

			if (timeout_msec(mux, timeout, &msec) <= -1 ||
			    reserve_mux(mux, &mux->native_events, &mux->native_capacity, mux->count + 1, HAWK_SIZEOF(*ev)) <= -1)
			{
				return -1;
			}

			ev = mux->native_events;
			for (i = 0; i < mux->count; i++)
			{
				mux_watch_t* w = &mux->watch[i];

				ev[i].fd = w->armed? w->hnd: -1;
				ev[i].events = 0;
				ev[i].revents = 0;

				if (w->mask & HAWK_MUX_IN) ev[i].events |= POLLIN;
				if (w->mask & HAWK_MUX_OUT) ev[i].events |= POLLOUT;
				if (w->mask & HAWK_MUX_PRI) ev[i].events |= POLLPRI;
			#if defined(POLLRDHUP)
				if (w->mask & HAWK_MUX_RDHUP) ev[i].events |= POLLRDHUP;
			#endif
			}
			n = poll(ev, mux->count, msec);
			if (n <= -1) return fail_mux_native(mux);

			for (i = 0; i < mux->count && count < capacity; i++)
			{
				mux_watch_t* w = &mux->watch[i];
				hawk_uint32_t mask = 0;

				if (ev[i].revents & POLLIN) mask |= HAWK_MUX_IN;
				if (ev[i].revents & POLLOUT) mask |= HAWK_MUX_OUT;
				if (ev[i].revents & POLLPRI) mask |= HAWK_MUX_PRI;
				if (ev[i].revents & (POLLERR | POLLNVAL)) mask |= HAWK_MUX_ERR;
				if (ev[i].revents & POLLHUP) mask |= HAWK_MUX_HUP;
			#if defined(POLLRDHUP)
				if (ev[i].revents & POLLRDHUP) mask |= HAWK_MUX_RDHUP;
			#endif
				if (!mask) continue;

				events[count].hnd = w->hnd;
				events[count].mask = mask;
				events[count++].token = w->token;

				if (w->mask & HAWK_MUX_ONESHOT) w->armed = 0;
			}
			return count;
		}
#endif
#if defined(MUX_HAVE_SELECT)
		case HAWK_MUX_BACKEND_SELECT:
		{
			fd_set rset, wset, xset;
			struct timeval tv;
			struct timeval* tvp = HAWK_NULL;
			hawk_mux_hnd_t maxhnd = 0;
			int active = 0;

			FD_ZERO(&rset);
			FD_ZERO(&wset);
			FD_ZERO(&xset);

			if (timeout)
			{
				tv.tv_sec = timeout->sec;
				tv.tv_usec = (timeout->nsec + HAWK_NSECS_PER_USEC - 1) / HAWK_NSECS_PER_USEC;
				if ((hawk_intmax_t)tv.tv_sec != timeout->sec) return fail_mux(mux, HAWK_EINVAL);

				if (tv.tv_usec >= HAWK_USECS_PER_SEC)
				{
					if (tv.tv_sec == HAWK_TYPE_MAX(long)) return fail_mux(mux, HAWK_EINVAL);
					tv.tv_sec++;
					tv.tv_usec = 0;
				}
				tvp = &tv;
			}
			for (i = 0; i < mux->count; i++)
			{
				mux_watch_t* w = &mux->watch[i];

				if (!w->armed) continue;
				if (w->mask & HAWK_MUX_IN)
				{
					FD_SET(w->hnd, &rset);
					active = 1;
				}
				if (w->mask & HAWK_MUX_OUT)
				{
					FD_SET(w->hnd, &wset);
					active = 1;
				}
				if (w->mask & HAWK_MUX_PRI)
				{
					FD_SET(w->hnd, &xset);
					active = 1;
				}
				if (w->hnd > maxhnd) maxhnd = w->hnd;
			}

		#if defined(_WIN32)
			/* Winsock rejects three empty sets. Sleep is the equivalent wait. */
			if (!active)
			{
				int msec;

				if (timeout_msec(mux, timeout, &msec) <= -1) return -1;
				Sleep(msec < 0? INFINITE: (DWORD)msec);
				return 0;
			}
			n = select(0, &rset, &wset, &xset, tvp);
		#else
			(void)active;
			if (maxhnd == HAWK_TYPE_MAX(int)) return fail_mux(mux, HAWK_EINVAL);
			n = select(maxhnd + 1, &rset, &wset, &xset, tvp);
		#endif
			if (n <= -1) return fail_mux_native(mux);
			for (i = 0; i < mux->count && count < capacity; i++)
			{
				mux_watch_t* w = &mux->watch[i];
				hawk_uint32_t mask = 0;

				if (FD_ISSET(w->hnd, &rset)) mask |= HAWK_MUX_IN;
				if (FD_ISSET(w->hnd, &wset)) mask |= HAWK_MUX_OUT;
				if (FD_ISSET(w->hnd, &xset)) mask |= HAWK_MUX_PRI;
				if (!mask) continue;

				events[count].hnd = w->hnd;
				events[count].mask = mask;
				events[count++].token = w->token;

				if (w->mask & HAWK_MUX_ONESHOT) w->armed = 0;
			}

			return count;
		}
#endif
		default:
			return fail_mux(mux, HAWK_ENOIMPL);
	}
}
