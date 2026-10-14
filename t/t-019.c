/* ID-based runtime aux services: opaque protocols and binding ownership. */
#include <hawk-aux.h>
#include <hawk-utl.h>
#include "../lib/hawk-prv.h" /* Test the configured aux-slot boundaries. */
#include "tap.h"

#define AUX_ID_ALPHA (10)
#define AUX_ID_BETA  (11)
#define AUX_ID_NULLABLE (12)
#define AUX_ID_MISSING (13)
#define AUX_ID_BORROWED (14)

typedef struct service_t service_t;
struct service_t
{
	int calls;
	int closed;
	int value;
	int result;
	int blocked;
	int close_probe;
	hawk_oow_t last_cmd;
	void* last_data;
};

enum
{
	CMD_ECHO,
	CMD_FAIL,
	CMD_ERROR,
	CMD_REENTER,
	CMD_REBIND
};

static int service_impl (hawk_rtx_t* rtx, hawk_oow_t cmd, void* data, void* ctx)
{
	service_t* service = ctx;
	hawk_aux_cbs_t cbs;
	void* saved;
	int n;

	/* A context may be NULL. Its presence is determined by the binding, not
	 * by its opaque context pointer. Commands/data need not be Hawk values. */
	if (!service) return 17;
	service->calls++;
	service->last_cmd = cmd;
	service->last_data = data;
	switch (cmd)
	{
		case CMD_FAIL:
			return -1;
		case CMD_ERROR:
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EIOERR, "custom service diagnostic");
			return -1;
		case CMD_REENTER:
			n = hawk_rtx_auxcall(rtx, AUX_ID_ALPHA, CMD_ECHO, data);
			if (n != service->result) return -1;
			/* Returning from the nested call must not clear outer activity. */
			if (hawk_rtx_setauxctx(rtx, AUX_ID_ALPHA, HAWK_NULL, HAWK_NULL) <= -1 &&
			    hawk_rtx_geterrnum(rtx) == HAWK_EPERM) service->blocked++;
			return service->result;
		case CMD_REBIND:
			if (hawk_rtx_getauxctx(rtx, AUX_ID_ALPHA, &cbs, &saved) != 1) return -1;
			if (hawk_rtx_setauxctx(rtx, AUX_ID_ALPHA, &cbs, saved) <= -1 &&
			    hawk_rtx_geterrnum(rtx) == HAWK_EPERM) service->blocked++;
			return service->result;
		default:
			if (data) *(int*)data = service->value;
			return service->result;
	}
}

static void service_close (hawk_rtx_t* rtx, void* ctx)
{
	service_t* service = ctx;
	hawk_uint16_t id;

	if (!service) return;
	service->closed++;
	if (service->close_probe)
	{
		/* Probe a different slot before mutating it. Earlier replacement
		 * failures in alpha must not make this cleanup test pass accidentally. */
		if (hawk_rtx_getauxid(rtx, HAWK_T("during-close"), &id) <= -1 &&
		    hawk_rtx_geterrnum(rtx) == HAWK_EPERM) service->blocked++;
		if (hawk_rtx_auxcall(rtx, AUX_ID_NULLABLE, CMD_ECHO, HAWK_NULL) <= -1 &&
		    hawk_rtx_geterrnum(rtx) == HAWK_EPERM) service->blocked++;
		if (hawk_rtx_setauxctx(rtx, AUX_ID_NULLABLE, HAWK_NULL, HAWK_NULL) <= -1 &&
		    hawk_rtx_geterrnum(rtx) == HAWK_EPERM) service->blocked++;
	}
}

static void basic_services (void)
{
	hawk_t* hawk;
	hawk_rtx_t* first;
	hawk_rtx_t* second;
	hawk_aux_cbs_t cbs = { service_impl, service_close };
	hawk_aux_cbs_t bad = { HAWK_NULL, service_close };
	hawk_aux_cbs_t snapshot;
	service_t alpha = { 0, 0, 101, 7 };
	service_t beta = { 0, 0, 202, 9 };
	service_t other = { 0, 0, 303, 11 };
	service_t replacement = { 0, 0, 404, 13 };
	void* ctx;
	int value = 0;
	int n;

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	first = hawk_rtx_openstd(hawk, 0, HAWK_T("first"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	second = hawk_rtx_openstd(hawk, 0, HAWK_T("second"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!first || !second) { bail_out("cannot open runtimes"); return; }

	OK(hawk_rtx_setauxctx(first, AUX_ID_ALPHA, &cbs, &alpha) == 0 &&
	   hawk_rtx_setauxctx(first, AUX_ID_BETA, &cbs, &beta) == 0, "multiple services coexist on one runtime");
	cbs.impl = HAWK_NULL;
	OK(hawk_rtx_auxcall(first, AUX_ID_BETA, HAWK_TYPE_MAX(hawk_oow_t), &value) == 9 &&
	   value == 202 && beta.last_cmd == HAWK_TYPE_MAX(hawk_oow_t) && beta.last_data == &value,
	   "callbacks are copied and arbitrary command/data pass unchanged");
	cbs.impl = service_impl;
	OK(hawk_rtx_setauxctx(second, AUX_ID_ALPHA, &cbs, &other) == 0 &&
	   hawk_rtx_auxcall(second, AUX_ID_ALPHA, CMD_ECHO, &value) == 11 && value == 303 && alpha.calls == 0,
	   "same service ID is isolated between runtimes");
	ctx = HAWK_NULL;
	OK(hawk_rtx_getauxctx(first, AUX_ID_ALPHA, &snapshot, &ctx) == 1 &&
	   snapshot.impl == service_impl && snapshot.close == service_close && ctx == &alpha,
	   "lookup returns copied callbacks and borrowed context");
	OK(hawk_rtx_getauxctx(first, AUX_ID_ALPHA, HAWK_NULL, HAWK_NULL) == 1, "lookup supports omitted outputs");
	hawk_rtx_seterrnum(first, HAWK_NULL, HAWK_EIOERR);
	ctx = &replacement;
	snapshot = bad;
	OK(hawk_rtx_getauxctx(first, AUX_ID_MISSING, &snapshot, &ctx) == 0 && ctx == &replacement &&
	   snapshot.impl == HAWK_NULL && hawk_rtx_geterrnum(first) == HAWK_EIOERR,
	   "missing ID lookup preserves outputs and diagnostics");
	OK(hawk_rtx_auxcall(first, AUX_ID_MISSING, CMD_ECHO, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_ENOENT, "missing call reports an error");
	OK(hawk_rtx_setauxctx(first, AUX_ID_NULLABLE, &cbs, HAWK_NULL) == 0 &&
	   hawk_rtx_getauxctx(first, AUX_ID_NULLABLE, HAWK_NULL, &ctx) == 1 && ctx == HAWK_NULL &&
	   hawk_rtx_auxcall(first, AUX_ID_NULLABLE, CMD_ECHO, HAWK_NULL) == 17,
	   "NULL contexts and NULL command data are valid");
	OK(hawk_rtx_setauxctx(first, HAWK_RTX_AUX_MAX, &cbs, &replacement) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_EINVAL &&
	   hawk_rtx_getauxctx(first, HAWK_RTX_AUX_MAX, HAWK_NULL, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_EINVAL &&
	   hawk_rtx_auxcall(first, HAWK_RTX_AUX_MAX, CMD_ECHO, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_EINVAL, "first out-of-range ID is rejected by all APIs");
	OK(hawk_rtx_setauxctx(first, HAWK_TYPE_MAX(hawk_uint16_t), &cbs, &replacement) <= -1 &&
	   hawk_rtx_getauxctx(first, HAWK_TYPE_MAX(hawk_uint16_t), HAWK_NULL, HAWK_NULL) <= -1 &&
	   hawk_rtx_auxcall(first, HAWK_TYPE_MAX(hawk_uint16_t), CMD_ECHO, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_EINVAL, "maximum uint16 ID is rejected without narrowing");
	OK(hawk_rtx_setauxctx(first, 0, &cbs, HAWK_NULL) == 0 &&
	   hawk_rtx_auxcall(first, 0, CMD_ECHO, HAWK_NULL) == 17 &&
	   hawk_rtx_setauxctx(first, HAWK_RTX_AUX_MAX - 1, &cbs, HAWK_NULL) == 0 &&
	   hawk_rtx_auxcall(first, HAWK_RTX_AUX_MAX - 1, CMD_ECHO, HAWK_NULL) == 17,
	   "first and last valid IDs can be registered and dispatched");
	OK(hawk_rtx_setauxctx(first, AUX_ID_ALPHA, &bad, &replacement) <= -1 &&
	   alpha.closed == 0 && replacement.closed == 0 &&
	   hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_ECHO, &value) == 7 && value == 101,
	   "invalid replacement preserves old service and proposed context ownership");
	hawk_rtx_seterrnum(first, HAWK_NULL, HAWK_EIOERR);
	OK(hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_FAIL, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_EOTHER, "failing callback gets a fresh fallback diagnostic");
	OK(hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_ERROR, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(first) == HAWK_EIOERR, "failing callback retains its own diagnostic");
	OK(hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_ECHO, HAWK_NULL) == 7 &&
	   hawk_rtx_geterrnum(first) == HAWK_ENOERR, "successful dispatch clears stale diagnostics");
	OK(hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_REENTER, &value) == 7 && alpha.blocked == 1 &&
	   alpha.closed == 0, "nested calls preserve outer activity and reject detachment");
	OK(hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_REBIND, HAWK_NULL) == 7 && alpha.blocked == 2 &&
	   alpha.closed == 0, "callback cannot replace its active context");
	OK(hawk_rtx_setauxctx(first, AUX_ID_ALPHA, &cbs, &replacement) == 0 && alpha.closed == 1 &&
	   hawk_rtx_auxcall(first, AUX_ID_ALPHA, CMD_ECHO, &value) == 13 && value == 404,
	   "replacement works after callback returns and closes only the old context");
	beta.close_probe = 1;
	OK(hawk_rtx_setauxctx(first, AUX_ID_BETA, HAWK_NULL, HAWK_NULL) == 0 && beta.closed == 1 &&
	   beta.blocked == 3 && replacement.closed == 0, "cleanup cannot detach or dispatch another service");
	OK(hawk_rtx_setauxctx(first, AUX_ID_BETA, HAWK_NULL, HAWK_NULL) == 0 && beta.closed == 1,
	   "repeated detachment is harmless");
	beta.close_probe = 0;
	cbs.close = HAWK_NULL;
	OK(hawk_rtx_setauxctx(first, AUX_ID_BORROWED, &cbs, &beta) == 0 &&
	   hawk_rtx_setauxctx(first, AUX_ID_BORROWED, HAWK_NULL, HAWK_NULL) == 0 && beta.closed == 1,
	   "optional cleanup leaves borrowed context untouched");
	cbs.close = service_close;
	beta.close_probe = 1;
	hawk_rtx_setauxctx(first, AUX_ID_BETA, &cbs, &beta);
	hawk_rtx_close(first);
	hawk_rtx_close(second);
	n = alpha.closed == 1 && replacement.closed == 1 && other.closed == 1 && beta.closed == 2 && beta.blocked == 6;
	OK(n, "runtime teardown closes each aux service once");
	hawk_close(hawk);
}

static void named_services (void)
{
	hawk_t* hawk;
	hawk_rtx_t* first;
	hawk_rtx_t* second;
	hawk_aux_cbs_t cbs = { service_impl, HAWK_NULL };
	hawk_uint16_t http_id;
	hawk_uint16_t db_id;
	hawk_uint16_t other_id;
	hawk_uint16_t saved;
	hawk_ooch_t name[3] = { 'd', 'b', 0 };
	hawk_oow_t i;
	int n;

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	first = hawk_rtx_openstd(hawk, 0, HAWK_T("names-first"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	second = hawk_rtx_openstd(hawk, 0, HAWK_T("names-second"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!first || !second) { bail_out("cannot open runtimes"); return; }

	hawk_rtx_setauxctx(first, 0, &cbs, HAWK_NULL);
	OK(hawk_rtx_getauxid(first, HAWK_T("http"), &http_id) == 0 && http_id != 0,
	   "name reservation skips live unnamed bindings");
	OK(hawk_rtx_getauxid(first, name, &db_id) == 0 && db_id != http_id,
	   "different service names reserve different slots");
	name[0] = 'x';
	OK(hawk_rtx_getauxid(first, HAWK_T("db"), &saved) == 0 && saved == db_id,
	   "registry owns a copy of the service name");
	hawk_rtx_getauxid(second, HAWK_T("db"), &other_id);
	hawk_rtx_getauxid(second, HAWK_T("another"), &other_id);
	OK(hawk_rtx_getauxid(second, HAWK_T("http"), &saved) == 0 && saved != http_id,
	   "runtime-local IDs may differ with reservation order");
	OK(hawk_rtx_setauxctx(first, http_id, &cbs, HAWK_NULL) == 0 &&
	   hawk_rtx_setauxctx(first, http_id, HAWK_NULL, HAWK_NULL) == 0 &&
	   hawk_rtx_getauxid(first, HAWK_T("http"), &saved) == 0 && saved == http_id,
	   "detachment preserves named ID reservation");
	OK(hawk_rtx_getauxid(first, HAWK_T("HTTP"), &saved) == 0 && saved != http_id,
	   "service names are case sensitive");
	saved = HAWK_TYPE_MAX(hawk_uint16_t);
	/* Non-NULL name and output pointers are caller preconditions. */
	OK(hawk_rtx_getauxid(first, HAWK_T(""), &saved) <= -1 &&
	   saved == HAWK_TYPE_MAX(hawk_uint16_t) && hawk_rtx_geterrnum(first) == HAWK_EINVAL,
	   "empty service name is rejected without changing the output ID");
	/* Fill the default registry with distinct two-character names. */
	name[0] = 's';
	for (i = 0; i < HAWK_COUNTOF(first->aux); i++)
	{
		name[1] = (hawk_ooch_t)('!' + i);
		n = hawk_rtx_getauxid(first, name, &saved);
		if (n <= -1) break;
	}
	OK(i < HAWK_COUNTOF(first->aux) && hawk_rtx_geterrnum(first) == HAWK_EBUFFULL,
	   "registry exhaustion reports a diagnostic");
	OK(hawk_rtx_getauxid(first, HAWK_T("http"), &saved) == 0 && saved == http_id,
	   "existing names still resolve when the registry is full");
	hawk_rtx_close(first);
	hawk_rtx_close(second);
	hawk_close(hawk);
}

static int probe_handler (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return 0;
}

static int probe_query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	static hawk_mod_fnc_tab_t table[] =
	{
		{ HAWK_T("run"), { { 0, 0, HAWK_NULL }, probe_handler, 0 } }
	};

	return hawk_findmodsymfnc(hawk, table, HAWK_COUNTOF(table), name, sym);
}

typedef struct probe_state_t probe_state_t;
struct probe_state_t
{
	service_t previous;
	service_t failed;
	int has_previous;
	int fini_calls;
	int unload_calls;
	int unload_after_aux;
	int ecb_calls;
	int ecb_after_aux;
};

static void probe_unload (hawk_mod_t* mod, hawk_t* hawk)
{
	probe_state_t* state = mod->ctx;

	state->unload_calls++;
	state->unload_after_aux = state->failed.closed == 1 &&
	    (!state->has_previous || (state->previous.closed == 1 && state->fini_calls == 1));
}

static int attach_unload_probe (hawk_rtx_t* rtx, probe_state_t* state)
{
	hawk_mod_data_t data;

	HAWK_MEMSET(&data, 0, HAWK_SIZEOF(data));
	data.mod.unload = probe_unload;
	data.mod.ctx = state;
	return hawk_rbt_insert(rtx->modtab, HAWK_T("runtime-probe"), 13, &data, HAWK_SIZEOF(data))? 0: -1;
}

static int probe_init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	probe_state_t* state = mod->ctx;
	hawk_aux_cbs_t cbs = { service_impl, service_close };
	hawk_uint16_t id;

	if (hawk_rtx_getauxid(rtx, HAWK_T("init-reservation"), &id) <= -1) return -1;
	state->failed.close_probe = 1;
	if (hawk_rtx_setauxctx(rtx, id, &cbs, &state->failed) <= -1 ||
	    attach_unload_probe(rtx, state) <= -1) return -1;
	hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EOTHER, "injected module initialization failure");
	return -1;
}

static int probe_ready_init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	probe_state_t* state = mod->ctx;
	hawk_aux_cbs_t cbs = { service_impl, service_close };
	hawk_uint16_t id;

	if (hawk_rtx_getauxid(rtx, HAWK_T("previous-reservation"), &id) <= -1) return -1;
	return hawk_rtx_setauxctx(rtx, id, &cbs, &state->previous);
}

static void probe_ready_fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	probe_state_t* state = mod->ctx;

	state->fini_calls++;
}

static int probe_module (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = probe_query;
	mod->init = probe_init;
	mod->fini = HAWK_NULL;
	mod->unload = HAWK_NULL;
	mod->ctx = hawk_getxtn(hawk);
	return 0;
}

static int probe_ready_module (hawk_mod_t* mod, hawk_t* hawk)
{
	probe_module(mod, hawk);
	mod->init = probe_ready_init;
	mod->fini = probe_ready_fini;
	return 0;
}

static void failed_runtime_open (int previous)
{
	hawk_t* hawk;
	hawk_rtx_t* rtx;
	probe_state_t* state;
	hawk_parsestd_t input[2] = { { 0 }, { 0 } };
	const hawk_bch_t* source = previous?
	    "BEGIN { aauxprobe::run(); zauxprobe::run(); }": "BEGIN { zauxprobe::run(); }";

	hawk = hawk_openstd(HAWK_SIZEOF(*state), HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	state = hawk_getxtn(hawk);
	HAWK_MEMSET(state, 0, HAWK_SIZEOF(*state));
	state->has_previous = previous;
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = hawk_count_bcstr(source);
	if ((previous && hawk_addstaticmodwithbcstr(hawk, "aauxprobe", probe_ready_module) <= -1) ||
	    hawk_addstaticmodwithbcstr(hawk, "zauxprobe", probe_module) <= -1 ||
	    hawk_parsestd(hawk, input, HAWK_NULL) <= -1)
	{
		bail_out("cannot parse module initialization probe");
		hawk_close(hawk);
		return;
	}
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("init-failure"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	OK(rtx == HAWK_NULL, previous? "later module initialization fails": "first module initialization fails");
	if (rtx) hawk_rtx_close(rtx);
	OK(state->failed.closed == 1 && state->failed.blocked == 3,
	   "failed init binding is closed once with aux access blocked");
	OK(state->previous.closed == previous && state->fini_calls == previous,
	   "successfully initialized modules finalize and close their remaining bindings");
	OK(state->unload_calls == 1 && state->unload_after_aux,
	   "failed open closes aux bindings before unloading runtime modules");
	OK(hawk_geterrnum(hawk) == HAWK_EOTHER &&
	   hawk_comp_bcstr(hawk_geterrbmsg(hawk), "injected module initialization failure", 0) == 0,
	   "aux cleanup preserves the original initialization diagnostic");
	hawk_close(hawk);
}

static void probe_ecb_close (hawk_rtx_t* rtx, void* ctx)
{
	probe_state_t* state = ctx;
	hawk_uint16_t id;
	hawk_oow_t i;

	state->ecb_calls++;
	state->ecb_after_aux = state->failed.closed == 1 && state->unload_calls == 1;
	for (i = 0; i < HAWK_COUNTOF(rtx->aux); i++)
	{
		if (rtx->aux[i].cbs.impl || rtx->aux_names[i]) state->ecb_after_aux = 0;
	}
	if (hawk_rtx_getauxid(rtx, HAWK_T("after-aux"), &id) != -1 ||
	    hawk_rtx_geterrnum(rtx) != HAWK_EPERM) state->ecb_after_aux = 0;
}

static void normal_runtime_close (void)
{
	hawk_t* hawk;
	hawk_rtx_t* rtx;
	probe_state_t state;
	hawk_aux_cbs_t cbs = { service_impl, service_close };
	hawk_rtx_ecb_t ecb;
	hawk_uint16_t id;

	HAWK_MEMSET(&state, 0, HAWK_SIZEOF(state));
	HAWK_MEMSET(&ecb, 0, HAWK_SIZEOF(ecb));
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("close-order"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) { bail_out("cannot open runtime"); hawk_close(hawk); return; }
	state.failed.close_probe = 1;
	if (hawk_rtx_getauxid(rtx, HAWK_T("close-order"), &id) <= -1 ||
	    hawk_rtx_setauxctx(rtx, id, &cbs, &state.failed) <= -1 ||
	    attach_unload_probe(rtx, &state) <= -1)
	{
		bail_out("cannot register cleanup probes");
		hawk_rtx_close(rtx);
		hawk_close(hawk);
		return;
	}
	ecb.close = probe_ecb_close;
	ecb.ctx = &state;
	hawk_rtx_pushecb(rtx, &ecb);
	hawk_rtx_close(rtx);
	OK(state.failed.closed == 1 && state.failed.blocked == 3,
	   "normal close does not repeat aux cleanup in fini_rtx");
	OK(state.unload_calls == 1 && state.unload_after_aux,
	   "normal close invokes aux cleanup before module unload");
	OK(state.ecb_calls == 1 && state.ecb_after_aux,
	   "ECB cleanup sees cleared bindings and names with aux access still blocked");
	hawk_close(hawk);
}

static void* allocate (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	if (*(int*)mmgr->ctx) return HAWK_NULL;
	return real->alloc(real, size);
}

static void* reallocate (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	if (*(int*)mmgr->ctx) return HAWK_NULL;
	return real->realloc(real, ptr, size);
}

static void deallocate (hawk_mmgr_t* mmgr, void* ptr)
{
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	real->free(real, ptr);
}

static void allocation_free_registration (void)
{
	int fail = 0;
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &fail };
	hawk_t* hawk;
	hawk_rtx_t* rtx;
	hawk_aux_cbs_t cbs = { service_impl, service_close };
	service_t old = { 0, 0, 1, 7 };
	service_t proposed = { 0, 0, 2, 9 };
	service_t replacement = { 0, 0, 3, 11 };
	int n;
	int value;
	hawk_uint16_t id;
	hawk_uint16_t saved;

	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("allocation"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) { bail_out("cannot open runtime"); return; }
	hawk_rtx_setauxctx(rtx, AUX_ID_ALPHA, &cbs, &old);
	hawk_rtx_getauxid(rtx, HAWK_T("existing"), &id);
	fail = 1;
	saved = HAWK_TYPE_MAX(hawk_uint16_t);
	OK(hawk_rtx_getauxid(rtx, HAWK_T("new"), &saved) <= -1 &&
	   saved == HAWK_TYPE_MAX(hawk_uint16_t) && hawk_rtx_geterrnum(rtx) == HAWK_ENOMEM,
	   "failed name allocation preserves the output and reserves no slot");
	OK(hawk_rtx_getauxid(rtx, HAWK_T("existing"), &saved) == 0 && saved == id,
	   "existing name lookup requires no allocation");
	n = hawk_rtx_setauxctx(rtx, AUX_ID_BETA, &cbs, &proposed);
	OK(n == 0 && proposed.closed == 0, "new aux registration requires no allocation");
	n = hawk_rtx_setauxctx(rtx, AUX_ID_ALPHA, &cbs, &replacement);
	fail = 0;
	OK(hawk_rtx_getauxid(rtx, HAWK_T("new"), &saved) == 0 && saved == id + 1,
	   "failed reservation does not consume an ID");
	OK(n == 0 && old.closed == 1 && replacement.closed == 0,
	   "aux replacement requires no allocation and cleans up the previous context");
	OK(hawk_rtx_auxcall(rtx, AUX_ID_ALPHA, CMD_ECHO, &value) == 11 && value == 3 &&
	   hawk_rtx_auxcall(rtx, AUX_ID_BETA, CMD_ECHO, &value) == 9 && value == 2,
	   "both allocation-free registrations remain usable");
	hawk_rtx_close(rtx);
	OK(old.closed == 1 && proposed.closed == 1 && replacement.closed == 1,
	   "runtime cleanup follows successful allocation-free registration");
	hawk_close(hawk);
}

int main (void)
{
	/* Preserve diagnostics if a library assertion terminates the process. */
	setvbuf(stdout, HAWK_NULL, _IONBF, 0);
	no_plan();
	basic_services();
	named_services();
	failed_runtime_open(0);
	failed_runtime_open(1);
	normal_runtime_close();
	allocation_free_registration();
	return exit_status();
}
