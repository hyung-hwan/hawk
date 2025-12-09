package hawk

/*
#include <hawk.h>
#include <stdlib.h>
#include <string.h>

struct rtx_xtn_t
{
	hawk_oow_t inst_no;
	hawk_oow_t rtx_inst_no;
};

typedef struct rtx_xtn_t rtx_xtn_t;

static void init_parsestd_for_text_in(hawk_parsestd_t* in, hawk_bch_t* ptr, hawk_oow_t len)
{
	in[0].type = HAWK_PARSESTD_BCS;
	in[0].u.bcs.ptr = ptr;
	in[0].u.bcs.len = len;
}

static void init_parsestd_for_file_in(hawk_parsestd_t* in, hawk_bch_t* path)
{
	in[0].type = HAWK_PARSESTD_FILEB;
	in[0].u.fileb.path = path;
	in[0].u.fileb.cmgr = HAWK_NULL;
}

static hawk_ooch_t* valtostr_out_cpldup(hawk_rtx_valtostr_out_t* out, hawk_oow_t* len)
{
	*len =  out->u.cpldup.len;
	return out->u.cpldup.ptr;
}

static hawk_val_t* make_flt_val(hawk_rtx_t* rtx, double v) {
	return hawk_rtx_makefltval(rtx, (hawk_flt_t)v);
}

static int val_to_flt(hawk_rtx_t* rtx, hawk_val_t* v, double* ret) {
	hawk_flt_t fv;
	if (hawk_rtx_valtoflt(rtx, v, &fv) <= -1) return -1;
	*ret = (double)fv;
	return 0;
}

extern int hawk_go_fnc_handler(rtx_xtn_t* rtx_xtn, hawk_bch_t* name, hawk_oow_t len);

static int hawk_fnc_handler_for_go(hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	int n;
	hawk_oow_t namelen;
	hawk_bch_t* name;
	rtx_xtn_t* xtn;

	name = hawk_rtx_duputobchars(rtx, fi->name.ptr, fi->name.len, &namelen);
	if (!name)
	{
		// TODO: set error information
		return -1;
	}

	xtn = hawk_rtx_getxtn(rtx);
	n = hawk_go_fnc_handler(xtn, name, namelen);
	hawk_rtx_freemem(rtx, name);
	return n;
}

static hawk_fnc_t* add_fnc_with_bcstr(hawk_t* hawk, const hawk_bch_t* name, hawk_oow_t min_args, hawk_oow_t max_args, const hawk_bch_t* arg_spec)
{
	hawk_fnc_bspec_t spec;
	memset(&spec, 0, HAWK_SIZEOF(spec));
	spec.arg.min = min_args;
	spec.arg.max = max_args;
	spec.arg.spec = arg_spec;
	spec.impl = hawk_fnc_handler_for_go;
	spec.trait = 0;
	return hawk_addfncwithbcstr(hawk, name, &spec);
}

static void set_errmsg(hawk_t* hawk, hawk_errnum_t errnum, const hawk_bch_t* msg)
{
	hawk_seterrbfmt(hawk, HAWK_NULL, errnum, "%hs", msg);
}

static void set_rtx_errmsg(hawk_rtx_t* rtx, hawk_errnum_t errnum, const hawk_bch_t* msg)
{
	hawk_rtx_seterrbfmt(rtx, HAWK_NULL, errnum, "%hs", msg);
}
*/
import "C"

import "fmt"
import "reflect"
import "runtime"
import "sync"
import "unsafe"

type Fnc func(rtx *Rtx) error

type Err struct {
	Line uint
	Colm uint
	File string
	Msg string
}

type Hawk struct {
	c *C.hawk_t
	inst_no int
	fnctab map[string]Fnc

	rtx_mtx sync.Mutex
	rtx_count int
	rtx_head *Rtx
	rtx_tail *Rtx
}

type HawkExt struct {
	inst_no int
}

type Rtx struct {
	c *C.hawk_rtx_t
	inst_no int
	h *Hawk

	next *Rtx
	prev *Rtx

	val_mtx sync.Mutex
	val_count int
	val_head *Val
	val_tail *Val
}

type Val struct {
	c *C.hawk_val_t
	rtx *Rtx
	next *Val
	prev *Val
}

type ValType int
const (
	VAL_NIL  ValType = C.HAWK_VAL_NIL
	VAL_CHAR ValType = C.HAWK_VAL_CHAR
	VAL_BCHR ValType = C.HAWK_VAL_BCHR
	VAL_INT  ValType = C.HAWK_VAL_INT
	VAL_FLT  ValType = C.HAWK_VAL_FLT
	VAL_STR  ValType = C.HAWK_VAL_STR
	VAL_MBS  ValType = C.HAWK_VAL_MBS
	VAL_FUN  ValType = C.HAWK_VAL_FUN
	VAL_MAP  ValType = C.HAWK_VAL_MAP
	VAL_ARR  ValType = C.HAWK_VAL_ARR
	VAL_REX  ValType = C.HAWK_VAL_REX
	VAL_REF  ValType = C.HAWK_VAL_REF
	VAL_BOB  ValType = C.HAWK_VAL_BOB
)

type ValArrayItr struct {
	c C.hawk_val_arr_itr_t
}

type ValMapItr struct {
	c C.hawk_val_map_itr_t
}

type BitMask C.hawk_bitmask_t

func deregister_instance(h *Hawk) {
//fmt.Printf ("DEREGISER INSTANCE %p\n", h)
	for h.rtx_head != nil {
//fmt.Printf ("DEREGISER CLOSING RTX %p\n", h.rtx_head)
		h.rtx_head.Close()
	}

	if h.c != nil {
//fmt.Printf ("CLOSING h.c\n")
		C.hawk_close(h.c)
		h.c = nil
	}

	if h.inst_no >= 0 {
//fmt.Printf ("DELETING instance h\n")
		inst_table.delete_instance(h.inst_no)
		h.inst_no = -1
	}
//fmt.Printf ("END DEREGISER INSTANCE %p\n", h)
}

func New() (*Hawk, error) {
	var c *C.hawk_t
	var g *Hawk
	var ext *HawkExt
	var errinf C.hawk_errinf_t

	c = C.hawk_openstd(C.hawk_oow_t(unsafe.Sizeof(*ext)), &errinf)
	if c == nil {
		var err error
		err = fmt.Errorf("%s", string(ucstr_to_rune_slice(&errinf.msg[0])))
		return nil, err
	}

	ext = (*HawkExt)(unsafe.Pointer(C.hawk_getxtn(c)))

	g = &Hawk{c: c, inst_no: -1}

	g.fnctab = make(map[string]Fnc)
	g.rtx_count = 0
	g.rtx_head = nil
	g.rtx_tail = nil

	runtime.SetFinalizer(g, deregister_instance)
	g.inst_no = inst_table.add_instance(c, g)
	ext.inst_no = g.inst_no

	return g, nil
}

func (hawk *Hawk) Close() {
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
	var cname *C.hawk_bch_t
	cname = C.CString(name)
	x = C.hawk_addgblwithbcstr(hawk.c, cname)
	C.free(unsafe.Pointer(cname))
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

//export hawk_go_fnc_handler
func hawk_go_fnc_handler(rtx_xtn *C.rtx_xtn_t, name *C.hawk_bch_t, namelen C.hawk_oow_t) C.int {
	var fn Fnc
	var inst HawkInstance
	var rtx_inst RtxInstance
	var rtx *Rtx
	var fnname string
	var ok bool
	var err error

	inst = inst_table.slot_to_instance(int(rtx_xtn.inst_no))
	rtx_inst = rtx_inst_table.slot_to_instance(int(rtx_xtn.rtx_inst_no))
	rtx = rtx_inst.g.Value()

	fnname = C.GoStringN(name, C.int(namelen))
	fn, ok = inst.g.Value().fnctab[fnname]
	if !ok {
		rtx.set_errmsg(C.HAWK_ENOENT, fmt.Sprintf("function '%s' not found", fnname))
		return -1;
	}

	err = fn(rtx_inst.g.Value())
	if err != nil {
		rtx.set_errmsg(C.HAWK_EOTHER, fmt.Sprintf("function '%s' failure - %s", fnname, err.Error()))
		return -1
	}

	return 0
}

func (hawk *Hawk) AddFunc(name string, min_args uint, max_args uint, spec string, fn Fnc) error {
	var fnc *C.hawk_fnc_t
	var cname *C.hawk_bch_t
	var cspec *C.hawk_bch_t

	cname = C.CString(name)
	cspec = C.CString(spec)
	fnc = C.add_fnc_with_bcstr(hawk.c, cname, C.hawk_oow_t(min_args), C.hawk_oow_t(max_args), cspec);
	C.free(unsafe.Pointer(cspec))
	C.free(unsafe.Pointer(cname))
	if fnc == nil { return hawk.make_errinfo() }

	hawk.fnctab[name] = fn;
	return nil
}

func (hawk *Hawk) ParseFile(file string) error {
	var x C.int
	var in [2]C.hawk_parsestd_t
	var cfile *C.hawk_bch_t

	cfile = C.CString(file)
	C.init_parsestd_for_file_in(&in[0], cfile)
	x = C.hawk_parsestd(hawk.c, &in[0], nil)
	C.free(unsafe.Pointer(cfile))
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

func (hawk *Hawk) ParseFiles(files []string) error {
	var x C.int
	var in []C.hawk_parsestd_t
	var idx int
	var count int
	var cfiles []*C.hawk_bch_t

	count = len(files)
	in = make([]C.hawk_parsestd_t, count + 1)
	for idx = 0; idx < count; idx++ {
		cfiles[idx] = C.CString(files[idx])
		C.init_parsestd_for_file_in(&in[idx], cfiles[idx])
	}
	in[idx]._type = C.HAWK_PARSESTD_NULL
	x = C.hawk_parsestd(hawk.c, &in[0], nil)
	for idx = 0; idx < count; idx++ {
		C.free(unsafe.Pointer(cfiles[idx]))
	}
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

func (hawk *Hawk) ParseText(file string) error {
	var x C.int
	var in [2]C.hawk_parsestd_t
	var cfile *C.hawk_bch_t

	cfile = C.CString(file)
	C.init_parsestd_for_text_in(&in[0], cfile, C.hawk_oow_t(len(file)))
	in[1]._type = C.HAWK_PARSESTD_NULL
	x = C.hawk_parsestd(hawk.c, &in[0], nil)
	C.free(unsafe.Pointer(cfile))
	if x <= -1 { return hawk.make_errinfo() }
	return nil
}

func (hawk *Hawk) RtxCount() int {
	hawk.rtx_mtx.Lock()
	defer hawk.rtx_mtx.Unlock()
	return hawk.rtx_count
}

func (hawk *Hawk) chain_rtx(rtx *Rtx) {
	hawk.rtx_mtx.Lock()
	rtx.h = hawk
	if hawk.rtx_head == nil {
		rtx.prev = nil
		hawk.rtx_head = rtx
	} else {
		rtx.prev = hawk.rtx_tail
		hawk.rtx_tail.next = rtx
	}
	rtx.next = nil
	hawk.rtx_tail = rtx
	hawk.rtx_count++
//fmt.Printf(">>>> %d\n", hawk.rtx_count)
	hawk.rtx_mtx.Unlock()
}

func (hawk *Hawk) unchain_rtx(rtx *Rtx) {
	hawk.rtx_mtx.Lock()
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
//fmt.Printf("head %p tail %p\n", hawk.rtx_tail, hawk.rtx_tail)

	rtx.h = nil
	rtx.next = nil
	rtx.prev = nil
	hawk.rtx_count--
//fmt.Printf(">>>> %d\n", hawk.rtx_count)
	hawk.rtx_mtx.Unlock()
}

// -----------------------------------------------------------
func deregister_rtx_instance(rtx *Rtx) {
	if rtx.h != nil {
//fmt.Printf("RTX CLOSING UNCHAIN %p\n", rtx)
		rtx.h.unchain_rtx(rtx)

		for rtx.val_head != nil {
//fmt.Printf ("****** DEREGISER CLOSING VAL %p\n", rtx.val_head)
			rtx.val_head.Close()
		}

		C.hawk_rtx_close(rtx.c)
//fmt.Printf("RTX CLOSING %p\n", rtx)
	}

	if rtx.inst_no >= 0 {
		rtx_inst_table.delete_instance(rtx.inst_no)
		rtx.inst_no = -1
	}
}

func (hawk *Hawk) NewRtx(id string, in []string) (*Rtx, error) {
	var rtx *C.hawk_rtx_t
	var g *Rtx
	var xtn *C.rtx_xtn_t
	var cid *C.hawk_bch_t
	var cin []*C.hawk_bch_t
	var idx int
	var in_count int

	in_count = len(in)
	cin = make([]*C.hawk_bch_t, in_count + 1)
	for idx = 0; idx < in_count; idx++ {
		cin[idx] = C.CString(in[idx])
	}
	cin[idx] = (*C.hawk_bch_t)(nil)
	cid = C.CString(id)
	if in_count > 0 {
		rtx = C.hawk_rtx_openstdwithbcstr(hawk.c, C.hawk_oow_t(unsafe.Sizeof(*xtn)), cid, &cin[0], nil, nil)
	} else {
		rtx = C.hawk_rtx_openstdwithbcstr(hawk.c, C.hawk_oow_t(unsafe.Sizeof(*xtn)), cid, nil, nil, nil)
	}
	for idx = 0; idx < in_count; idx++ {
		C.free(unsafe.Pointer(cin[idx]))
	}
	C.free(unsafe.Pointer(cid))

	if rtx == nil { return nil, hawk.make_errinfo() }

	g = &Rtx{c: rtx}
	hawk.chain_rtx(g)

	// [NOTE]
	// if the owning hawk is not garbaged-collected, this rtx is never
	// garbaged collected as a strong pointer to a rtx object is maintained
	// in the owner.

	runtime.SetFinalizer(g, deregister_rtx_instance)
	g.inst_no = rtx_inst_table.add_instance(rtx, g)

	xtn = (*C.rtx_xtn_t)(unsafe.Pointer(C.hawk_rtx_getxtn(rtx)))
	xtn.inst_no = C.hawk_oow_t(hawk.inst_no)
	xtn.rtx_inst_no = C.hawk_oow_t(g.inst_no)

	return g, nil
}

func (rtx* Rtx) Close() {
	deregister_rtx_instance(rtx)
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

func (rtx *Rtx) Exec(args []string) (*Val, error) {
	var val *C.hawk_val_t
	var cargs []*C.hawk_bch_t
	var idx int
	var count int

	count = len(args)
	cargs = make([]*C.hawk_bch_t, count)
	for idx = 0; idx < count; idx++ {
		cargs[idx] = C.CString(args[idx])
	}

	if count > 0 {
		val = C.hawk_rtx_execwithbcstrarr(rtx.c, &cargs[0], C.hawk_oow_t(count))
	} else {
		val = C.hawk_rtx_execwithbcstrarr(rtx.c, (**C.hawk_bch_t)(nil), C.hawk_oow_t(count))
	}

	for idx = 0; idx < count; idx++ {
		C.free(unsafe.Pointer(cargs[idx]))
	}
	if val == nil { return nil, rtx.make_errinfo() }
	return &Val{rtx: rtx, c: val}, nil
}

func (rtx *Rtx) Loop() (*Val, error) {
	var val *C.hawk_val_t
	val = C.hawk_rtx_loop(rtx.c)
	if val == nil { return nil, rtx.make_errinfo() }
	return &Val{rtx: rtx, c: val}, nil
}

func (rtx *Rtx) Call(name string, args ...*Val) (*Val, error) {
	var fun *C.hawk_fun_t
	var val *C.hawk_val_t
	var nargs int

	fun = C.hawk_rtx_findfunwithbcstr(rtx.c, C.CString(name))
	if fun == nil { return nil, rtx.make_errinfo() }

	nargs = len(args)
	if nargs > 0 {
		var argv []*C.hawk_val_t
		var v *Val
		var i int
		argv = make([]*C.hawk_val_t, nargs)
		for i, v = range args { argv[i] = v.c }
		val = C.hawk_rtx_callfun(rtx.c, fun, &argv[0], C.hawk_oow_t(nargs))
	} else {
		val = C.hawk_rtx_callfun(rtx.c, fun, nil, 0)
	}
	if val == nil { return nil, rtx.make_errinfo() }

	// hawk_rtx_callfun() returns a value with the reference count incremented.
	// i create a Val object without incrementing the reference count of val.
	return &Val{rtx: rtx, c: val}, nil
}

func (rtx *Rtx) ValCount() int {
	rtx.val_mtx.Lock()
	defer rtx.val_mtx.Unlock()
	return rtx.val_count
}

func (rtx *Rtx) chain_val(val *Val) {
	rtx.val_mtx.Lock()
	val.rtx = rtx
	if rtx.val_head == nil {
		val.prev = nil
		rtx.val_head = val
	} else {
		val.prev = rtx.val_tail
		rtx.val_tail.next = val
	}
	val.next = nil
	rtx.val_tail = val
	rtx.val_count++
	rtx.val_mtx.Unlock()
}

func (rtx *Rtx) unchain_val(val *Val) {
	rtx.val_mtx.Lock()
	if val.prev == nil  {
		rtx.val_head = val.next
	} else {
		val.prev.next = val.next
	}

	if val.next == nil {
		rtx.val_tail = val.prev
	} else {
		val.next.prev = val.prev
	}
//fmt.Printf("head %p tail %p\n", rtx.val_tail, rtx.val_tail)

	val.rtx = nil
	val.next = nil
	val.prev = nil
	rtx.val_count--
	rtx.val_mtx.Unlock()
}

func (rtx *Rtx) GetFuncArgCount() int {
	var nargs C.hawk_oow_t
	nargs = C.hawk_rtx_getnargs(rtx.c)
	return int(nargs)
}

func (rtx *Rtx) GetFuncArg(idx int) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_getarg(rtx.c, C.hawk_oow_t(idx))
	})
}

func (rtx *Rtx) SetFuncRet(v *Val) {
	C.hawk_rtx_setretval(rtx.c, v.c)
}

// -----------------------------------------------------------

func (hawk *Hawk) get_errmsg() string {
	return C.GoString(C.hawk_geterrbmsg(hawk.c))
}

func (hawk *Hawk) set_errmsg(num C.hawk_errnum_t, msg string) {
	var ptr *C.char
	ptr = C.CString(msg)
	defer C.free(unsafe.Pointer(ptr))
	C.set_errmsg(hawk.c, num, ptr)
}

func (rtx *Rtx) get_errmsg() string {
	return C.GoString(C.hawk_rtx_geterrbmsg(rtx.c))
}

func (rtx *Rtx) set_errmsg(num C.hawk_errnum_t, msg string) {
	var ptr *C.char
	ptr = C.CString(msg)
	defer C.free(unsafe.Pointer(ptr))
	C.set_rtx_errmsg(rtx.c, num, ptr)
}

// -----------------------------------------------------------

func (err* Err) Error() string {
	return fmt.Sprintf("%s[%d,%d] %s", err.File, err.Line, err.Colm, err.Msg)
}

// -----------------------------------------------------------

func (rtx *Rtx) NewVal(v interface{}) (*Val, error) {
	var _type reflect.Type
	_type = reflect.TypeOf(v)
	switch _type.Kind() {
		case reflect.Int:
			return rtx.NewValFromInt(v.(int))
		case reflect.Float32:
			return rtx.NewValFromFlt(float64(v.(float32)))
		case reflect.Float64:
			return rtx.NewValFromFlt(v.(float64))
		case reflect.String:
			return rtx.NewValFromStr(v.(string))

		case reflect.Array:
			fallthrough
		case reflect.Slice:
			var _kind reflect.Kind
			_kind = _type.Elem().Kind()
			if _kind == reflect.Uint8 {
				return rtx.NewValFromByteArr(v.([]byte))
			}
			fallthrough

		default:
			return nil, fmt.Errorf("invalid argument")
	}
}

func (rtx* Rtx) make_val(vmaker func() *C.hawk_val_t) (*Val, error) {
	var c *C.hawk_val_t
	var vv *Val

	c = vmaker()
	if c == nil { return nil, rtx.make_errinfo() }

	C.hawk_rtx_refupval(rtx.c, c)
	vv = &Val{rtx: rtx, c: c}
	rtx.chain_val(vv)
	return vv, nil
}

func (rtx *Rtx) NewValFromByte(v byte) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makebchrval(rtx.c, C.hawk_bch_t(v))
	})
}

func (rtx *Rtx) NewValFromRune(v rune) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makecharval(rtx.c, C.hawk_ooch_t(v))
	})
}

func (rtx *Rtx) NewValFromInt(v int) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makeintval(rtx.c, C.hawk_int_t(v))
	})
}

func (rtx *Rtx) NewValFromFlt(v float64) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.make_flt_val(rtx.c, C.double(v))
	})
}

func (rtx *Rtx) NewValFromStr(v string) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		var vv *C.hawk_val_t
		var cv *C.hawk_bch_t
		cv = C.CString(v)
		vv = C.hawk_rtx_makestrvalwithbchars(rtx.c, cv, C.hawk_oow_t(len(v)))
		C.free(unsafe.Pointer(cv))
		return vv
	})
}

func (rtx *Rtx) NewValFromByteArr(v []byte) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makembsvalwithbchars(rtx.c, (*C.hawk_bch_t)(unsafe.Pointer(&v[0])), C.hawk_oow_t(len(v)))
	})
}

func (rtx *Rtx) NewBobVal(v []byte) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makebobval(rtx.c, unsafe.Pointer(&v[0]), C.hawk_oow_t(len(v)))
	})
}

func (rtx *Rtx) NewMapVal() (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makemapval(rtx.c)
	})
}

func (rtx *Rtx) NewArrVal(init_capa int) (*Val, error) {
	return rtx.make_val(func() *C.hawk_val_t {
		return C.hawk_rtx_makearrval(rtx.c, C.hawk_ooi_t(init_capa))
	})
}

func (val *Val) Close() {
	if val.rtx != nil {
		var rtx *C.hawk_rtx_t
		rtx = val.rtx.c // store this field as unchain_val() resets it to nil
		val.rtx.unchain_val(val)
		C.hawk_rtx_refdownval(rtx, val.c)
	}
}

func (val *Val) Type() ValType {
	var x C.int
	x = C.hawk_rtx_getvaltype(val.rtx.c, val.c)
	return ValType(x)
}

/*func (val *Val) ToByte() (byte, error) {
}

func (val *Val) ToRune() (rune, error) {
}*/

func (val *Val) ToInt() (int, error) {
	var v C.hawk_int_t
	var x C.int

	x = C.hawk_rtx_valtoint(val.rtx.c, val.c, &v)
	if x <= -1 { return 0, val.rtx.make_errinfo() }

	return int(v), nil
}

func (val *Val) ToFlt() (float64, error) {
	var v float64
	var x C.int

	//x = C.hawk_rtx_valtoflt(val.rtx.c, val.c, &v)
	x = C.val_to_flt(val.rtx.c, val.c, (*C.double)(&v))
	if x <= -1 { return 0, val.rtx.make_errinfo() }

	return v, nil
}

func (val *Val) ToStr() (string, error) {
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

func (val *Val) ToByteArr() ([]byte, error) {
	var ptr *C.hawk_bch_t
	var len C.hawk_oow_t
	var v []byte

	ptr = C.hawk_rtx_valtobcstrdupwithcmgr(val.rtx.c, val.c, &len, C.hawk_rtx_getcmgr(val.rtx.c))
	if ptr == nil { return nil, val.rtx.make_errinfo() }

	v = C.GoBytes(unsafe.Pointer(ptr), C.int(len))
	C.hawk_rtx_freemem(val.rtx.c, unsafe.Pointer(ptr))

	return v, nil
}

func (val *Val) ArrayTally() int {
	var v C.hawk_ooi_t
// TODO: if not array .. panic or return -1 or 0?
	v = C.hawk_rtx_getarrvaltally(val.rtx.c, val.c)
	return int(v)
}

// TODO: function get the first index and last index or the capacity
//       function to traverse?
func (val *Val) ArrayField(index int) (*Val, error) {
	var v *C.hawk_val_t
	v = C.hawk_rtx_getarrvalfld(val.rtx.c, val.c, C.hawk_ooi_t(index))
	if v == nil { return nil, val.rtx.make_errinfo() }
	return val.rtx.make_val(func() *C.hawk_val_t { return v })
}

func (val *Val) ArraySetField(index int, v *Val) error {
	var vv *C.hawk_val_t
	vv = C.hawk_rtx_setarrvalfld(val.rtx.c, val.c, C.hawk_ooi_t(index), v.c)
	if vv == nil { return val.rtx.make_errinfo() }
	return nil
}

func (val *Val) ArrayFirstField(itr *ValArrayItr) (int, *Val) {
	var i *C.hawk_val_arr_itr_t
	var v *Val
	var err error
	i = C.hawk_rtx_getfirstarrvalitr(val.rtx.c, val.c, &itr.c)
	if i == nil { return -1, nil }
	v, err = val.rtx.make_val(func() *C.hawk_val_t { return itr.c.elem })
	if err != nil { return -1, nil }
	return int(itr.c.itr.idx), v;
}

func (val *Val) ArrayNextField(itr *ValArrayItr) (int, *Val) {
	var i *C.hawk_val_arr_itr_t
	var v *Val
	var err error
	i = C.hawk_rtx_getnextarrvalitr(val.rtx.c, val.c, &itr.c)
	if i == nil { return -1, nil }
	v, err = val.rtx.make_val(func() *C.hawk_val_t { return itr.c.elem })
	if err != nil { return -1, nil }
	return int(itr.c.itr.idx), v;
}

func (val *Val) MapField(key string) (*Val, error) {
	var v *C.hawk_val_t
	var uc []C.hawk_uch_t
	uc = string_to_uchars(key)
	v = C.hawk_rtx_getmapvalfld(val.rtx.c, val.c, &uc[0], C.hawk_oow_t(len(uc)))
	if v == nil { return nil, val.rtx.make_errinfo() }
	return val.rtx.make_val(func() *C.hawk_val_t { return v })
}

func (val *Val) MapSetField(key string, v *Val) error {
	var vv *C.hawk_val_t
	var kk []C.hawk_uch_t

	kk = string_to_uchars(key)
	vv = C.hawk_rtx_setmapvalfld(val.rtx.c, val.c, &kk[0], C.hawk_oow_t(len(kk)), v.c)
	if vv == nil { return val.rtx.make_errinfo() }
	return nil
}

func (val *Val) MapFirstField(itr *ValMapItr) (string, *Val) {
	var i *C.hawk_val_map_itr_t
	var k string
	var v *Val
	var err error
	i = C.hawk_rtx_getfirstmapvalitr(val.rtx.c, val.c, &itr.c)
	if i == nil { return "", nil }
	k = string(uchars_to_rune_slice((*C.hawk_uch_t)(itr.c.pair.key.ptr), uintptr(itr.c.pair.key.len)))
	v, err = val.rtx.make_val(func() *C.hawk_val_t { return (*C.hawk_val_t)(itr.c.pair.val.ptr) })
	if err != nil { return "", nil }
	return k, v;
}

func (val *Val) MapNextField(itr *ValMapItr) (string, *Val) {
	var i *C.hawk_val_map_itr_t
	var k string
	var v *Val
	var err error
	i = C.hawk_rtx_getnextmapvalitr(val.rtx.c, val.c, &itr.c)
	if i == nil { return "", nil }
	k = string(uchars_to_rune_slice((*C.hawk_uch_t)(itr.c.pair.key.ptr), uintptr(itr.c.pair.key.len)))
	v, err = val.rtx.make_val(func() *C.hawk_val_t { return (*C.hawk_val_t)(itr.c.pair.val.ptr) })
	if err != nil { return "", nil }
	return k, v;
}

func (val *Val) String() string {
	var s string
	s, _ = val.ToStr()
	return s
}

// -----------------------------------------------------------

var val_type []string = []string{
	VAL_NIL: "NIL",
	VAL_CHAR: "CHAR",
	VAL_BCHR: "BCHR",
	VAL_INT: "INT",
	VAL_FLT: "FLT",
	VAL_STR: "STR",
	VAL_MBS: "MBS",
	VAL_FUN: "FUN",
	VAL_MAP: "MAP",
	VAL_ARR: "ARR",
	VAL_REX: "REX",
	VAL_REF: "REF",
	VAL_BOB: "BOB",
}

func (t ValType) String() string {
	return val_type[t]
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
	var ext *HawkExt
	var inst HawkInstance

	ext = (*HawkExt)(unsafe.Pointer(C.hawk_getxtn(c)))
	inst = inst_table.slot_to_instance(ext.inst_no)
	return inst.g.Value()
}

func rtx_to_go(rtx *C.hawk_rtx_t) *Rtx {
	var xtn *C.rtx_xtn_t
	var inst RtxInstance

	xtn = (*C.rtx_xtn_t)(unsafe.Pointer(C.hawk_rtx_getxtn(rtx)))
	inst = rtx_inst_table.slot_to_instance(int(xtn.rtx_inst_no))
	return inst.g.Value()
}

func Must[T any](v T, err error) T {
	if err != nil { panic(err) }
	return v
}
