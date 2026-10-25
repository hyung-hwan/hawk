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

#include "hawk-prv.h"
#include "hawk-aux.h"

int hawk_rtx_getauxid (hawk_rtx_t* rtx, const hawk_ooch_t* name, hawk_uint16_t* id)
{
	hawk_oow_t i;
	hawk_oow_t unused = HAWK_COUNTOF(rtx->aux);
	hawk_ooch_t* copy;

	HAWK_STATIC_ASSERT(HAWK_COUNTOF(rtx->aux) == HAWK_COUNTOF(rtx->aux_names));

	if (rtx->aux_closing)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EPERM, "aux service cleanup is in progress");
		return -1;
	}

	HAWK_ASSERT(name != HAWK_NULL);
	HAWK_ASSERT(id != HAWK_NULL);
	if (name[0] == '\0')
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "aux service name and ID output are required");
		return -1;
	}

	for (i = 0; i < HAWK_COUNTOF(rtx->aux); i++)
	{
		if (rtx->aux_names[i])
		{
			if (hawk_comp_oocstr(rtx->aux_names[i], name, 0) == 0)
			{
				*id = (hawk_uint16_t)i;
				return 0;
			}
		}
		else if (!rtx->aux[i].cbs.impl && unused == HAWK_COUNTOF(rtx->aux)) unused = i;
	}
	if (unused == HAWK_COUNTOF(rtx->aux))
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EBUFFULL, "aux service registry is full");
		return -1;
	}

	copy = hawk_rtx_dupoocstr(rtx, name, HAWK_NULL);
	if (HAWK_UNLIKELY(!copy)) return -1;
	rtx->aux_names[unused] = copy;
	*id = (hawk_uint16_t)unused;

	return 0;
}

static int find_binding (hawk_rtx_t* rtx, hawk_uint16_t id, hawk_rtx_aux_t** result)
{
	HAWK_ASSERT(id >= 0 && id < HAWK_COUNTOF(rtx->aux));

	*result = HAWK_NULL;

	if (rtx->aux_closing)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EPERM, "aux service cleanup is in progress");
		return -1;
	}

	if (rtx->aux[id].cbs.impl) *result = &rtx->aux[id];
	return 0;
}

int hawk_rtx_setauxctx (hawk_rtx_t* rtx, hawk_uint16_t id, const hawk_aux_cbs_t* cbs, void* ctx)
{
	hawk_rtx_aux_t* old;
	hawk_aux_cbs_t new_cbs;

	if (id >= HAWK_COUNTOF(rtx->aux))
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "aux service id out of range");
		return -1;
	}

	if (find_binding(rtx, id, &old) <= -1) return -1;

	if (old)
	{
		HAWK_ASSERT(old == &rtx->aux[id]);
		if (old->active)
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EPERM, "aux service '%u' is active", (unsigned int)id);
			return -1;
		}
	}

	if (cbs) 
	{
		if (!cbs->impl)
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "aux service callback is missing");
			return -1;
		}

		/* snapshot this in case this is overwritten by old->cbs.close() */
		new_cbs = *cbs;
	}

	if (old && old->cbs.close)
	{
		HAWK_ASSERT(old->cbs.impl != HAWK_NULL);
		rtx->aux_closing = 1;
		old->cbs.close(rtx, old->ctx);
		rtx->aux_closing = 0;
	}

	HAWK_MEMSET(&rtx->aux[id], 0, HAWK_SIZEOF(rtx->aux[id]));
	if (cbs)
	{
		rtx->aux[id].id = id;
		rtx->aux[id].cbs = new_cbs;
		rtx->aux[id].ctx = ctx;
	}

	return 0;
}

int hawk_rtx_getauxctx (hawk_rtx_t* rtx, hawk_uint16_t id, hawk_aux_cbs_t* cbs, void** ctx)
{
	hawk_rtx_aux_t* binding;

	if (id < 0 || id >= HAWK_COUNTOF(rtx->aux))
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "aux service id out of range");
		return -1;
	}

	if (find_binding(rtx, id, &binding) <= -1) return -1;
	if (!binding) return 0;

	if (cbs) *cbs = binding->cbs;
	if (ctx) *ctx = binding->ctx;

	return 1;
}

int hawk_rtx_auxcall (hawk_rtx_t* rtx, hawk_uint16_t id, hawk_oow_t cmd, void* data)
{
	hawk_rtx_aux_t* binding;
	int active;
	int n;

	if (id < 0 || id >= HAWK_COUNTOF(rtx->aux))
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "aux service id out of range");
		return -1;
	}

	if (find_binding(rtx, id, &binding) <= -1) return -1;

	if (!binding)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_ENOENT, "aux service '%u' is not attached", (unsigned int)id);
		return -1;
	}

	active = binding->active;
	binding->active = 1;
	hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENOERR);
	n = binding->cbs.impl(rtx, cmd, data, binding->ctx);
	binding->active = active;
	if (n <= -1)
	{
		if (hawk_rtx_geterrnum(rtx) == HAWK_ENOERR)
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EOTHER, "aux service '%u' operation failed", (unsigned int)binding->id);
		return -1;
	}
	return n;
}
