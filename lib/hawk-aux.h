/*
 * $Id$
 *
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

#ifndef _HAWK_AUX_H_
#define _HAWK_AUX_H_

#include <hawk.h>

/** Operation callback for an aux service registered under a numeric ID.
 * The service defines the command codes and the protocol for caller-owned
 * data, which may be NULL. Return a nonnegative service result on success,
 * or -1 with a runtime diagnostic on failure. The bridge passes cmd and data
 * unchanged and does not set an intrinsic return value automatically. */
typedef int (*hawk_host_impl_t) (
	hawk_rtx_t* rtx,
	hawk_oow_t cmd,
	void* data,
	void* ctx
);

/** Optional cleanup, called once on replacement, detachment, or runtime close.
 * It may free ctx, but must not execute scripts or close rtx. Aux lookups,
 * dispatch, and registration changes for all IDs on this runtime are rejected
 * with HAWK_EPERM while cleanup runs. During runtime teardown this protection
 * remains active through the rest of runtime destruction. */
typedef void (*hawk_host_close_t) (
	hawk_rtx_t* rtx,
	void* ctx
);

typedef struct hawk_aux_cbs_t hawk_aux_cbs_t;
struct hawk_aux_cbs_t
{
	hawk_host_impl_t impl;   /**< Required operation callback. */
	hawk_host_close_t close; /**< Optional context cleanup callback. */
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Resolve a nonempty, case-sensitive service name to a runtime-local ID,
 * reserving an unused slot on first use. The name is copied. Reservations
 * survive replacement and detachment until runtime close. A call to this
 * function with a new name reserves one slot and allocates space to copy
 * the name. IDs may differ between runtimes; do not cache them in a shared
 * module descriptor. Live unnamed bindings are skipped. Use this API for
 * all cooperating adapters; manually chosen IDs must not overwrite another
 * service's slot.
 *
 * Return 0 on success, -1 on error, leaving id unchanged on failure.
 * Invalid arguments produce HAWK_EINVAL, a full registry HAWK_EBUFFULL,
 * allocation failure HAWK_ENOMEM, and cleanup HAWK_EPERM. Resolving an
 * existing name requires no allocation. Serialize access to the runtime.
 */
HAWK_EXPORT int hawk_rtx_getauxid (
	hawk_rtx_t*        rtx,
	const hawk_ooch_t* name,
	hawk_uint16_t*     id
);

/**
 * Bind an aux service to the runtime slot identified by id. IDs must be below
 * the configured slot count (64 by default, giving IDs 0 through 63). Choose
 * an ID with hawk_rtx_getauxid() to avoid collisions with other adapters.
 *
 * Callbacks are copied; ctx is borrowed until cleanup and may be NULL. Slots
 * are stored in the runtime, so registration itself requires no allocation.
 * Replacement invokes the previous cleanup callback before installing the new
 * registration. NULL callbacks detach the slot; detaching an empty slot is
 * harmless. An executing callback's own slot cannot be replaced or detached.
 *
 * Return 0 on success, or -1 with a runtime diagnostic on failure. Out-of-range
 * IDs or a non-NULL callback table without impl produce HAWK_EINVAL. Failed
 * registration preserves the previous binding and leaves the proposed context
 * with the caller. Aux access during cleanup is rejected with HAWK_EPERM.
 * Access to each runtime must be serialized by the embedding application. */
HAWK_EXPORT int hawk_rtx_setauxctx (
	hawk_rtx_t*           rtx,
	hawk_uint16_t         id,
	const hawk_aux_cbs_t* cbs,
	void*                 ctx
);

/**
 * Retrieve the binding for id: 1 if found, 0 if absent, -1 on error.
 * Non-NULL outputs receive copied callbacks and the borrowed context only
 * when found. Absence leaves outputs and the runtime diagnostic unchanged.
 * A registered NULL context is distinguished from absence by the return value.
 * The context remains valid until replacement, detachment, or runtime close.
 * Out-of-range IDs produce HAWK_EINVAL; aux cleanup produces HAWK_EPERM. */
HAWK_EXPORT int hawk_rtx_getauxctx (
	hawk_rtx_t*     rtx,
	hawk_uint16_t   id,
	hawk_aux_cbs_t* cbs,
	void**          ctx
);

/** Dispatch cmd and data unchanged to the service registered under id;
 * data may be NULL. Command codes belong to the individual service protocol.
 * Return its nonnegative result, or -1 with a runtime diagnostic. The old
 * diagnostic is cleared before dispatch; a failing callback that supplies
 * none receives HAWK_EOTHER. A missing service produces HAWK_ENOENT,
 * an out-of-range ID produces HAWK_EINVAL, and aux cleanup produces HAWK_EPERM. */
HAWK_EXPORT int hawk_rtx_auxcall (
	hawk_rtx_t* rtx,
	hawk_uint16_t id,
	hawk_oow_t cmd,
	void* data
);

#if defined(__cplusplus)
}
#endif

#endif
