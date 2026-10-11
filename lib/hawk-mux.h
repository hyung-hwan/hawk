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

#ifndef _HAWK_MUX_H_
#define _HAWK_MUX_H_

#include <hawk-gem.h>

typedef struct hawk_mux_t hawk_mux_t;

#if defined(_WIN32)
/* A native SOCKET, not a CRT file descriptor. */
typedef hawk_uintptr_t hawk_mux_hnd_t;
#else
typedef int hawk_mux_hnd_t;
#endif

enum hawk_mux_backend_t
{
	HAWK_MUX_BACKEND_AUTO,
	HAWK_MUX_BACKEND_EPOLL,
	HAWK_MUX_BACKEND_KQUEUE,
	HAWK_MUX_BACKEND_POLL,
	HAWK_MUX_BACKEND_SELECT
};
typedef enum hawk_mux_backend_t hawk_mux_backend_t;

enum hawk_mux_cmd_t
{
	HAWK_MUX_ADD,
	HAWK_MUX_DEL,
	HAWK_MUX_MOD
};
typedef enum hawk_mux_cmd_t hawk_mux_cmd_t;

enum hawk_mux_evtmask_t
{
	/* Keep the existing Linux sys::MUX_EVT_* values. */
	HAWK_MUX_IN = 0x01,
	HAWK_MUX_PRI = 0x02,
	HAWK_MUX_OUT = 0x04,
	HAWK_MUX_ERR = 0x08,
	HAWK_MUX_HUP = 0x10,
	HAWK_MUX_RDHUP = 0x2000,
	HAWK_MUX_ONESHOT = 0x40000000
};

struct hawk_mux_evt_t
{
	hawk_mux_hnd_t hnd;
	hawk_uint32_t mask;
	hawk_uint64_t token; /* opaque registration identity, never a node pointer */
};
typedef struct hawk_mux_evt_t hawk_mux_evt_t;

#if defined(__cplusplus)
extern "C" {
#endif

/** AUTO selects epoll, kqueue, poll, then select. An explicitly requested
 * unavailable backend fails with HAWK_ENOIMPL. Instances are not thread safe.
 * Windows select accepts sockets only; the caller initializes Winsock. */
HAWK_EXPORT hawk_mux_t* hawk_mux_open (
	hawk_mmgr_t*       mmgr,
	hawk_oow_t         xtnsize,
	hawk_cmgr_t*       cmgr,
	hawk_mux_backend_t backend,
	hawk_errinf_t*     errinf
);

/** Release the instance even on a native close failure. Never close watched
 * handles or explicitly delete their watches here. errinf may be NULL. */
HAWK_EXPORT int hawk_mux_close (
	hawk_mux_t*    mux,
	hawk_errinf_t* errinf
);

HAWK_EXPORT hawk_gem_t* hawk_mux_getgem (
	hawk_mux_t* mux
);

HAWK_EXPORT void* hawk_mux_getxtn (
	hawk_mux_t* mux
);

HAWK_EXPORT hawk_mux_backend_t hawk_mux_getbackend (
	hawk_mux_t* mux
);

/** ADD rejects duplicates; MOD rearms a one-shot watch. DEL ignores mask and
 * token. Failed controls leave the logical registration unchanged. Handles
 * must remain valid until deleted. ERR/HUP are reported where supported;
 * select cannot distinguish EOF from readable input. */
HAWK_EXPORT int hawk_mux_ctl (
	hawk_mux_t*    mux,
	hawk_mux_cmd_t cmd,
	hawk_mux_hnd_t hnd,
	hawk_uint32_t  mask,
	hawk_uint64_t  token
);

/** Return the number of events, or -1 with the error in getgem(). NULL timeout
 * waits indefinitely. Otherwise sec >= 0 and 0 <= nsec < 1000000000 are
 * required. Positive fractions round upward to the backend's resolution.
 * EINTR is reported. Events are copied values and survive later controls.
 * ONESHOT disables the entire registration after its first returned event. */
HAWK_EXPORT hawk_ooi_t hawk_mux_wait (
	hawk_mux_t*         mux,
	hawk_mux_evt_t*     events,
	hawk_oow_t          capacity,
	const hawk_ntime_t* timeout
);

#if defined(__cplusplus)
}
#endif

#endif
