package hawk

/*
#include <hawk.h>
#include <stdlib.h>

static void init_parsestd_for_text_in(hawk_parsestd_t* in, hawk_bch_t* ptr, hawk_oow_t len)
{
	in[0].type = HAWK_PARSESTD_BCS;
	in[0].u.bcs.ptr = ptr;
	in[0].u.bcs.len = len;
	in[1].type = HAWK_PARSESTD_NULL;
}

static void init_parsestd_for_file_in(hawk_parsestd_t* in, hawk_bch_t* path)
{
	in[0].type = HAWK_PARSESTD_FILEB;
	in[0].u.fileb.path = path;
	in[0].u.fileb.cmgr = HAWK_NULL;
	in[1].type = HAWK_PARSESTD_NULL;
}

static hawk_ooch_t* valtostr_out_cpldup(hawk_rtx_valtostr_out_t* out, hawk_oow_t* len)
{
	*len =  out->u.cpldup.len;
	return out->u.cpldup.ptr;
}
*/
import "C"

import "fmt"
import "runtime"
import "unsafe"

type Hawk struct {
	c *C.hawk_t
	inst_no int

	rtx_head *Rtx
	rtx_tail *Rtx
}

type Ext struct {
	inst_no int
}

type Err struct {
	Line uint
	Colm uint
	File string
	Msg string
}

type Rtx struct {
	c *C.hawk_rtx_t
	h *Hawk

	next *Rtx
	prev *Rtx
}

type Val struct {
	c *C.hawk_val_t
	rtx *Rtx
}

type BitMask C.hawk_bitmask_t

func deregister_instance(g *Hawk) {
	for g.rtx_head != nil {
		g.rtx_head.Close()
	}

	if g.c != nil {
fmt.Printf ("CLOSING g.c\n")
		C.hawk_close(g.c)
		g.c = nil
	}
	if g.inst_no >= 0 {
fmt.Printf ("DELETING instance g\n")
		inst_table.delete_instance(g.inst_no)
		g.inst_no = -1
	}
}

func New() (*Hawk, error) {
	var c *C.hawk_t
	var g *Hawk
	var ext *Ext
	var errinf C.hawk_errinf_t

	c = C.hawk_openstd(C.hawk_oow_t(unsafe.Sizeof(*ext)), &errinf)
	if c == nil {
		var err error
		err = fmt.Errorf("%s", string(ucstr_to_rune_slice(&errinf.msg[0])))
		return nil, err
	}

	ext = (*Ext)(unsafe.Pointer(C.hawk_getxtn(c)))

	g = &Hawk{c: c, inst_no: -1}

	runtime.SetFinalizer(g, deregister_instance)
	g.inst_no = inst_table.add_instance(c, g)
	ext.inst_no = g.inst_no

	return g, nil
}

func (hawk *Hawk) Close() {
// TODO: close all rtx?
	//if hawk.c != nil {
	//	C.hawk_close(hawk.c)
	//	hawk.c = nil
	//}
	deregister_instance(hawk)
}

func (hawk *Hawk) make_errinfo() *Err {
/*
	var errinf C.hawk_erruinf_t
	var err Err

	C.hawk_geterruinf(hawk.c, &errinf)

	err.Line = uint(errinf.loc.line)
	err.Colm = uint(errinf.loc.colm)
	if errinf.loc.file != nil {
		err.File = string(ucstr_to_rune_slice(errinf.loc.file))
	} else {
		// TODO:
		//err.File = hawk.io.cci_main
	}
	err.Msg = string(ucstr_to_rune_slice(&errinf.msg[0]))
	return &err
*/
	var loc *C.hawk_loc_t
	var err Err

	err.Msg = hawk.get_errmsg()

	loc = C.hawk_geterrloc(hawk.c)
	if loc != nil {
		err.Line = uint(loc.line)
		err.Colm = uint(loc.colm)
		if loc.file != nil {
			err.File = string(ucstr_to_rune_slice(loc.file))
		} else {
// TODO:
//			err.File = hawk.io.cci_main
		}
	}
	return &err
}

func (hawk *Hawk) GetTrait() BitMask {
	var x C.int
	var log_mask BitMask = 0

	x = C.hawk_getopt(hawk.c, C.HAWK_OPT_TRAIT, unsafe.Pointer(&log_mask))
	if x <= -1 {
		// this must not happen
		panic(fmt.Errorf("unable to get log mask - %s", hawk.get_errmsg()))
	}

	return log_mask
}

func (hawk *Hawk) SetTrait(log_mask BitMask) {
	var x C.int

	x = C.hawk_setopt(hawk.c, C.HAWK_OPT_TRAIT, unsafe.Pointer(&log_mask))
	if x <= -1 {
		// this must not happen
		panic(fmt.Errorf("unable to set log mask - %s", hawk.get_errmsg()))
	}
}

func (hawk *Hawk) GetLogMask() BitMask {
	var x C.int
	var log_mask BitMask = 0

	x = C.hawk_getopt(hawk.c, C.HAWK_OPT_LOG_MASK, unsafe.Pointer(&log_mask))
	if x <= -1 {
		// this must not happen
		panic(fmt.Errorf("unable to get log mask - %s", hawk.get_errmsg()))
	}

	return log_mask
}

func (hawk *Hawk) SetLogMask(log_mask BitMask) {
	var x C.int

	x = C.hawk_setopt(hawk.c, C.HAWK_OPT_LOG_MASK, unsafe.Pointer(&log_mask))
	if x <= -1 {
		// this must not happen
		panic(fmt.Errorf("unable to set log mask - %s", hawk.get_errmsg()))
	}
}

func (hawk *Hawk) AddGlobal(name string) error {
	var x C.int
	x = C.hawk_addgblwithbcstr(hawk.c, C.CString(name))
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

func (hawk *Hawk) ParseFile(text string) error {
	var x C.int
	var in [2]C.hawk_parsestd_t
	C.init_parsestd_for_file_in(&in[0], C.CString(text))
	x = C.hawk_parsestd(hawk.c, &in[0], nil)
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

func (hawk *Hawk) ParseText(text string) error {
	var x C.int
	var in [2]C.hawk_parsestd_t

	C.init_parsestd_for_text_in(&in[0], C.CString(text), C.hawk_oow_t(len(text)))
	x = C.hawk_parsestd(hawk.c, &in[0], nil)
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

func (hawk *Hawk) chain_rtx(rtx *Rtx) {
	if hawk.rtx_head == nil {
		rtx.prev = nil
		hawk.rtx_head = rtx
	} else {
		rtx.prev = hawk.rtx_tail
		hawk.rtx_tail.next = rtx
	}
	rtx.next = nil
	hawk.rtx_tail = rtx
}

func (hawk *Hawk) unchain_rtx(rtx *Rtx) {
	if rtx.prev == nil  {
		hawk.rtx_head = rtx.next
	} else {
		rtx.prev.next = rtx.next
	}

	if rtx.next == nil {
		hawk.rtx_tail = rtx.prev
	} else {
		rtx.next.prev = rtx.prev
	}

	rtx.next = nil
	rtx.prev = nil
}

// -----------------------------------------------------------

func (hawk *Hawk) NewRtx(id string) (*Rtx, error) {
	var rtx *C.hawk_rtx_t
	var g *Rtx

	rtx = C.hawk_rtx_openstdwithbcstr(hawk.c, 0, C.CString(id), nil, nil, nil)
	if rtx == nil { return nil, hawk.make_errinfo() }

	g = &Rtx{c: rtx, h: hawk}
	hawk.chain_rtx(g)
	return g, nil
}

func (rtx* Rtx) Close() {
	rtx.h.unchain_rtx(rtx)
	C.hawk_rtx_close(rtx.c)
fmt.Printf("RTX CLOSING %p\n", rtx)
}

func (rtx *Rtx) make_errinfo() *Err {
	var loc *C.hawk_loc_t
	var err Err

	err.Msg = rtx.get_errmsg()

	loc = C.hawk_rtx_geterrloc(rtx.c)
	if loc != nil {
		err.Line = uint(loc.line)
		err.Colm = uint(loc.colm)
		if loc.file != nil {
			err.File = string(ucstr_to_rune_slice(loc.file))
		} else {
// TODO:
//			err.File = hawk.io.cci_main
		}
	}
	return &err
}

func (rtx *Rtx) Call(name string) (*Val, error) {
	var fun *C.hawk_fun_t
	var val *C.hawk_val_t

	fun = C.hawk_rtx_findfunwithbcstr(rtx.c, C.CString(name))
	if fun == nil { return nil, rtx.make_errinfo() }
	val = C.hawk_rtx_callfun(rtx.c, fun, nil, 0)
	if val == nil { return nil, rtx.make_errinfo() }

	// hawk_rtx_callfun() returns a value with the reference count incremented.
	// i create a Val object without incrementing the reference count of val.
	return &Val{rtx: rtx, c: val}, nil
}

// -----------------------------------------------------------

func (hawk *Hawk) get_errmsg() string {
	return C.GoString(C.hawk_geterrbmsg(hawk.c))
}

func (hawk *Hawk) set_errmsg(num C.hawk_errnum_t, msg string) {
	var ptr *C.char
	ptr = C.CString(msg)
	defer C.free(unsafe.Pointer(ptr))
	C.hawk_seterrbmsg(hawk.c, nil, num, ptr)
}

func (rtx *Rtx) get_errmsg() string {
	return C.GoString(C.hawk_rtx_geterrbmsg(rtx.c))
}

// -----------------------------------------------------------

func ucstr_to_rune_slice(str *C.hawk_uch_t) []rune {
	return uchars_to_rune_slice(str, uintptr(C.hawk_count_ucstr(str)))
}

func uchars_to_rune_slice(str *C.hawk_uch_t, len uintptr) []rune {
	var res []rune
	var i uintptr
	var ptr uintptr

	// TODO: proper encoding...
	ptr = uintptr(unsafe.Pointer(str))
	res = make([]rune, len)
	for i = 0; i < len; i++ {
		res[i] = rune(*(*C.hawk_uch_t)(unsafe.Pointer(ptr)))
		ptr += unsafe.Sizeof(*str)
	}
	return res
}

func string_to_uchars(str string) []C.hawk_uch_t {
	var r []rune
	var c []C.hawk_uch_t
	var i int

	// TODO: proper encoding
	r = []rune(str)
	c = make([]C.hawk_uch_t, len(r), len(r))
	for i = 0; i < len(r); i++ {
		c[i] = C.hawk_uch_t(r[i])
	}

	return c
}

func rune_slice_to_uchars(r []rune) []C.hawk_uch_t {
	var c []C.hawk_uch_t
	var i int

	// TODO: proper encoding
	c = make([]C.hawk_uch_t, len(r), len(r))
	for i = 0; i < len(r); i++ {
		c[i] = C.hawk_uch_t(r[i])
	}
	return c
}

func c_to_go(c *C.hawk_t) *Hawk {
	var ext *Ext
	var inst Instance

	ext = (*Ext)(unsafe.Pointer(C.hawk_getxtn(c)))
	inst = inst_table.slot_to_instance(ext.inst_no)
	return inst.g.Value()
}

// -----------------------------------------------------------

func (err* Err) Error() string {
	return fmt.Sprintf("%s[%d,%d] %s", err.File, err.Line, err.Colm, err.Msg)
}

// -----------------------------------------------------------

func deref_val(v *Val) {
	C.hawk_rtx_refdownval(v.rtx.c, v.c)
}

func (rtx *Rtx) NewValFromInt(v int) (*Val, error) {
	var c *C.hawk_val_t
	var vv *Val

	c = C.hawk_rtx_makeintval(rtx.c, C.hawk_int_t(v))
	if c == nil { return nil, rtx.make_errinfo() }

	C.hawk_rtx_refupval(rtx.c, c)
	vv = &Val{rtx: rtx, c: c}
	runtime.SetFinalizer(vv, deref_val)

	return vv, nil
}

func (val* Val) ToInt() (int, error) {
	var v C.hawk_int_t
	var x C.int

	x = C.hawk_rtx_valtoint(val.rtx.c, val.c, &v)
	if x <= -1 { return 0, val.rtx.make_errinfo() }

	return int(v), nil
}

/*
func (val* Val) ToFlt() (double, error) {
	var v C.hawk_flt_t
	var x C.int

	x = C.hawk_rtx_valtoflt(val.rtx.c, val.c, &v)
	if x <= -1 { return 0, val.rtx.make_errinfo() }

	return double(v), nil
}*/

func (val* Val) ToStr() (string, error) {
	var out C.hawk_rtx_valtostr_out_t
	var ptr *C.hawk_ooch_t
	var len C.hawk_oow_t
	var v string
	var x C.int

	out._type = C.HAWK_RTX_VALTOSTR_CPLDUP
	x = C.hawk_rtx_valtostr(val.rtx.c, val.c, &out)
	if x <= -1 { return "", val.rtx.make_errinfo() }

	ptr = C.valtostr_out_cpldup(&out, &len)
	v = string(uchars_to_rune_slice(ptr, uintptr(len)))
	C.hawk_rtx_freemem(val.rtx.c, unsafe.Pointer(ptr))

	return v, nil
}
