package hawk

/*
#include <hawk-http.h>

extern int hawk_go_http_cmd(hawk_oow_t id, hawk_http_cmd_t cmd, hawk_http_arg_t* arg);

typedef struct go_http_ctx_t go_http_ctx_t;
struct go_http_ctx_t
{
	hawk_oow_t id;
};

static int go_http_impl(hawk_rtx_t* rtx, hawk_oow_t cmd, void* data, void* ctx)
{
	go_http_ctx_t* c = ctx;

	return hawk_go_http_cmd(c->id, (hawk_http_cmd_t)cmd, (hawk_http_arg_t*)data);
}

static void go_http_close(hawk_rtx_t* rtx, void* ctx)
{
	hawk_rtx_freemem(rtx, ctx);
}

static int set_go_http_ctx(hawk_rtx_t* rtx, hawk_oow_t id)
{
	go_http_ctx_t* ctx;
	hawk_aux_cbs_t cbs;
	hawk_uint16_t aux_id;

	if (hawk_rtx_getauxid(rtx, HAWK_HTTP_AUX_NAME, &aux_id) <= -1) return -1;

	ctx = hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*ctx));
	if (!ctx) return -1;

	ctx->id = id;
	cbs.impl = go_http_impl;
	cbs.close = go_http_close;
	if (hawk_rtx_setauxctx(rtx, aux_id, &cbs, ctx) <= -1)
	{
		hawk_rtx_freemem(rtx, ctx);
		return -1;
	}
	return 0;
}

static int http_module_enabled(void)
{
#if defined(HAWK_ENABLE_MOD_HTTP) && (HAWK_ENABLE_MOD_HTTP > 0)
	return 1;
#else
	return 0;
#endif
}

static hawk_bch_t* http_text_to_bytes(hawk_rtx_t* rtx, const hawk_oocs_t* text, hawk_oow_t* len)
{
#if defined(HAWK_OOCH_IS_UCH)
	return hawk_rtx_duputobchars(rtx, text->ptr, text->len, len);
#else
	*len = text->len;
	return hawk_rtx_dupbchars(rtx, text->ptr, text->len);
#endif
}
*/
import "C"

import "fmt"
import "net/http"
import "runtime"
import "strings"
import "unsafe"

func http_module_enabled() bool {
	return C.http_module_enabled() != 0
}

func (rtx *Rtx) set_http_context(ctx *httpd_request_context) error {
	if rtx.c == nil || rtx.execution_started {
		return fmt.Errorf("HTTP context must be attached before execution")
	}

	// bind the go-side rtx instance number to the actual
	// rtx object on the C-side using the aux mechanism.
	if C.set_go_http_ctx(rtx.c, C.hawk_oow_t(rtx.inst_no)) <= -1 {
		return rtx.make_errinfo()
	}

	rtx.http_ctx = ctx
	return nil
}

func http_argument_text(rtx *Rtx, text *C.hawk_oocs_t) (string, error) {
	var ptr *C.hawk_bch_t
	var length C.hawk_oow_t
	var result string

	ptr = C.http_text_to_bytes(rtx.c, text, &length)
	if ptr == nil { return "", rtx.make_errinfo() }
	defer C.hawk_rtx_freemem(rtx.c, unsafe.Pointer(ptr))
	if uint64(length) > 2147483647 { return "", fmt.Errorf("HTTP argument is too large") }
	result = C.GoStringN(ptr, C.int(length))
	return result, nil
}

//export hawk_go_http_cmd
func hawk_go_http_cmd(id C.hawk_oow_t, cmd C.hawk_http_cmd_t, arg *C.hawk_http_arg_t) C.int {
	var instance RtxInstance
	var rtx *Rtx
	var ctx *httpd_request_context
	var name string
	var text string
	var values []string
	var headers http.Header
	var value *Val
	var err error

	instance = rtx_inst_table.slot_to_instance(int(id))
	rtx = instance.g.Value()
	if rtx == nil { return -1 }
	defer runtime.KeepAlive(rtx)
	ctx = rtx.http_ctx
	if ctx == nil {
		rtx.set_errmsg(C.HAWK_EPERM, "HTTP request context is not attached")
		return -1
	}
	if ctx.committed && (cmd == C.HAWK_HTTP_SETSTATUS || cmd == C.HAWK_HTTP_SETHEADER || cmd == C.HAWK_HTTP_ADDHEADER || cmd == C.HAWK_HTTP_DELHEADER) {
		rtx.set_errmsg(C.HAWK_EPERM, "HTTP response headers are already committed")
		return -1
	}
	if cmd == C.HAWK_HTTP_GETHEADER || cmd == C.HAWK_HTTP_SETHEADER || cmd == C.HAWK_HTTP_ADDHEADER || cmd == C.HAWK_HTTP_DELHEADER {
		name, err = http_argument_text(rtx, &arg.name)
		if err != nil { goto oops }
	}
	switch cmd {
	case C.HAWK_HTTP_GETMETHOD:
		err = rtx.SetFuncRetWithStr(ctx.request.Method)
	case C.HAWK_HTTP_GETPATH:
		err = rtx.SetFuncRetWithStr(ctx.request.URL.Path)
	case C.HAWK_HTTP_GETHEADER:
		values = ctx.request.Header.Values(name)
		if strings.EqualFold(name, "Host") { values = []string{ctx.request.Host} }
		if arg.index > 0 && uint64(arg.index) <= uint64(len(values)) { text = values[int(arg.index) - 1] }
		err = rtx.SetFuncRetWithStr(text)
	case C.HAWK_HTTP_GETHEADERS:
		headers = ctx.request.Header.Clone()
		headers.Set("Host", ctx.request.Host)
		value, err = ctx.server.make_string_lists(rtx, map[string][]string(headers))
		if err == nil {
			rtx.SetFuncRet(value)
			value.Close()
		}
	case C.HAWK_HTTP_GETSTATUS:
		err = rtx.SetFuncRetWithInt(ctx.status)
	case C.HAWK_HTTP_SETSTATUS:
		ctx.status = int(arg.status)
	case C.HAWK_HTTP_SETHEADER, C.HAWK_HTTP_ADDHEADER:
		text, err = http_argument_text(rtx, &arg.value)
		if err == nil {
			if cmd == C.HAWK_HTTP_SETHEADER { ctx.headers.Set(name, text) } else { ctx.headers.Add(name, text) }
		}
	case C.HAWK_HTTP_DELHEADER:
		ctx.headers.Del(name)
	case C.HAWK_HTTP_FLUSH:
		err = ctx.flush()
	default:
		err = fmt.Errorf("unknown HTTP operation")
	}
	if err == nil { return 0 }

oops:
	rtx.set_errmsg(C.HAWK_EOTHER, err.Error())
	return -1
}
