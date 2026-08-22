package hawk_test

import "hawk"
import "fmt"
import "os"
import "runtime"
import "runtime/debug"
import "sync"
import "testing"
import "time"

func exit_with_error(msg string, err error) {
	fmt.Printf("ERROR: %s - %s\n", msg, err.Error())
	os.Exit(1)
}

func make_hawk(script string) (*hawk.Hawk, error) {
	var h *hawk.Hawk
	var err error

	h, err = hawk.New()
	if err != nil {
		return nil, err
	}

	err = h.ParseText(script)
	if err != nil {
		h.Close()
		return nil, err
	}

	return h, nil
}

func enbase64(rtx *hawk.Rtx) error {
	fmt.Printf("*** ENBASE64 RTX %p\n", rtx) // << this is 0 from time to time.. TODO: fix it..
	fmt.Printf("****ENBASE64 [%d]\n", rtx.GetFuncArgCount())

	var a0 *hawk.Val = hawk.Must(rtx.GetFuncArg(0))
	var a1 *hawk.Val = hawk.Must(rtx.GetFuncArg(1))
	var a2 *hawk.Val = hawk.Must(rtx.GetFuncArg(2))

	fmt.Printf("[%s] [%s] [%s]\n", a0.String(), a1.String(), a2.String())
	rtx.SetFuncRet(hawk.Must(rtx.NewStrVal("ENBASE64-OUTPUT")))
	return nil
}

func debase64(rtx *hawk.Rtx) error {
	fmt.Printf("****DEBASE64 [%d]\n", rtx.GetFuncArgCount())
	rtx.SetFuncRet(hawk.Must(rtx.NewFltVal(-999.1111)))
	return nil
	// return fmt.Errorf("what the hell.....")
}

func make_hawk_extended(script string) (*hawk.Hawk, error) {
	var h *hawk.Hawk
	var err error

	h, err = hawk.New()
	if err != nil {
		return nil, err
	}

	h.AddFunc("enbase64", 1, 10, "", enbase64)
	h.AddFunc("debase64", 1, 1, "", debase64)

	err = h.ParseText(script)
	if err != nil {
		h.Close()
		return nil, err
	}

	return h, nil
}

func echoint(rtx *hawk.Rtx) error {
	var v *hawk.Val
	var i int
	var err error

	v, err = rtx.GetFuncArg(0)
	if err != nil {
		return err
	}

	i, err = v.ToInt()
	if err != nil {
		return err
	}

	return rtx.SetFuncRetWithInt(i)
}

func force_gc() {
	runtime.GC()
	runtime.Gosched()
}

func force_full_gc() {
	var junk [][]byte
	var i int

	junk = make([][]byte, 32)
	for i = 0; i < len(junk); i++ {
		junk[i] = make([]byte, 1024+(i*17))
	}

	runtime.GC()
	debug.FreeOSMemory()
	runtime.Gosched()
	runtime.KeepAlive(junk)
}

func keep_vals_alive(vals []*hawk.Val) {
	var holder []*hawk.Val

	holder = make([]*hawk.Val, len(vals))
	copy(holder, vals)
	force_full_gc()
	runtime.KeepAlive(holder)
}

func expect_type(t *testing.T, v *hawk.Val, want hawk.ValType, msg string) {
	t.Helper()

	if v.Type() != want {
		t.Fatalf("%s: expected type %s but got %s", msg, want.String(), v.Type().String())
	}
}

func expect_int_val(t *testing.T, v *hawk.Val, want int, msg string) {
	var got int
	var s string
	var err error

	t.Helper()

	got, err = v.ToInt()
	if err != nil {
		t.Fatalf("%s: failed to convert to int - %s", msg, err.Error())
	}
	if got != want {
		t.Fatalf("%s: expected %d but got %d", msg, want, got)
	}

	s, err = v.ToStr()
	if err != nil {
		t.Fatalf("%s: failed to convert to string - %s", msg, err.Error())
	}
	if s != fmt.Sprintf("%d", want) {
		t.Fatalf("%s: expected string %q but got %q", msg, fmt.Sprintf("%d", want), s)
	}
}

func expect_map_ints_with_gc(t *testing.T, v *hawk.Val, want map[string]int, gc func()) {
	var key string
	var f *hawk.Val
	var got map[string]int
	var itr hawk.ValMapItr
	var err error

	t.Helper()

	if v.Type() != hawk.VAL_MAP {
		t.Fatalf("expected a map but got %s", v.Type().String())
	}

	for key = range want {
		gc()
		f, err = v.GetMapField(key)
		if err != nil {
			t.Fatalf("failed to get map field %q - %s", key, err.Error())
		}
		expect_int_val(t, f, want[key], fmt.Sprintf("map field %q", key))
	}

	got = make(map[string]int)
	key, f = v.GetFirstMapField(&itr)
	for f != nil {
		var iv int

		gc()
		iv, err = f.ToInt()
		if err != nil {
			t.Fatalf("failed to convert iterated map value for key %q - %s", key, err.Error())
		}
		got[key] = iv

		gc()
		key, f = v.GetNextMapField(&itr)
	}

	if len(got) != len(want) {
		t.Fatalf("expected %d iterated map entries but got %d", len(want), len(got))
	}

	for key = range want {
		if got[key] != want[key] {
			t.Fatalf("iterated map field %q: expected %d but got %d", key, want[key], got[key])
		}
	}
}

func expect_map_ints(t *testing.T, v *hawk.Val, want map[string]int) {
	expect_map_ints_with_gc(t, v, want, force_gc)
}

func expect_array_ints_with_gc(t *testing.T, v *hawk.Val, want map[int]int, gc func()) {
	var idx int
	var f *hawk.Val
	var got map[int]int
	var itr hawk.ValArrayItr
	var err error

	t.Helper()

	if v.Type() != hawk.VAL_ARR {
		t.Fatalf("expected an array but got %s", v.Type().String())
	}

	if v.ArrayTally() != len(want) {
		t.Fatalf("expected %d array elements but got %d", len(want), v.ArrayTally())
	}

	for idx = range want {
		gc()
		f, err = v.GetArrayField(idx)
		if err != nil {
			t.Fatalf("failed to get array field %d - %s", idx, err.Error())
		}
		expect_int_val(t, f, want[idx], fmt.Sprintf("array field %d", idx))
	}

	got = make(map[int]int)
	idx, f = v.GetFirstArrayField(&itr)
	for f != nil {
		var iv int

		gc()
		iv, err = f.ToInt()
		if err != nil {
			t.Fatalf("failed to convert iterated array value at %d - %s", idx, err.Error())
		}
		got[idx] = iv

		gc()
		idx, f = v.GetNextArrayField(&itr)
	}

	if len(got) != len(want) {
		t.Fatalf("expected %d iterated array entries but got %d", len(want), len(got))
	}

	for idx = range want {
		if got[idx] != want[idx] {
			t.Fatalf("iterated array field %d: expected %d but got %d", idx, want[idx], got[idx])
		}
	}
}

func expect_array_ints(t *testing.T, v *hawk.Val, want map[int]int) {
	expect_array_ints_with_gc(t, v, want, force_gc)
}

func echo_tagged(rtx *hawk.Rtx) error {
	var v *hawk.Val
	var err error

	force_full_gc()
	v, err = rtx.GetFuncArg(0)
	if err != nil {
		return err
	}
	keep_vals_alive([]*hawk.Val{v})
	rtx.SetFuncRet(v)
	keep_vals_alive([]*hawk.Val{v})
	return nil
}

func sum_tagged(rtx *hawk.Rtx) error {
	var vals []*hawk.Val
	var argc int
	var i int
	var sum int

	argc = rtx.GetFuncArgCount()
	vals = make([]*hawk.Val, argc)

	for i = 0; i < argc; i++ {
		var v *hawk.Val
		var iv int
		var err error

		force_full_gc()
		v, err = rtx.GetFuncArg(i)
		if err != nil {
			return err
		}
		vals[i] = v

		force_full_gc()
		iv, err = v.ToInt()
		if err != nil {
			return err
		}
		sum += iv
	}

	keep_vals_alive(vals)
	return rtx.SetFuncRetWithInt(sum)
}

func make_tagged_map(rtx *hawk.Rtx) error {
	var m *hawk.Val
	var vals []*hawk.Val
	var entry = []struct {
		key string
		val int
	}{
		{"zero", 0},
		{"one", 1},
		{"neg", -1},
		{"big", 777777},
		{"small", -333333},
	}
	var i int
	var err error

	m, err = rtx.NewMapVal()
	if err != nil {
		return err
	}

	vals = make([]*hawk.Val, len(entry))
	for i = 0; i < len(entry); i++ {
		vals[i], err = rtx.NewIntVal(entry[i].val)
		if err != nil {
			return err
		}
		force_full_gc()
		if err = m.SetMapField(entry[i].key, vals[i]); err != nil {
			return err
		}
	}

	keep_vals_alive(vals)
	rtx.SetFuncRet(m)
	force_full_gc()
	runtime.KeepAlive(m)
	return nil
}

func make_tagged_arr(rtx *hawk.Rtx) error {
	var a *hawk.Val
	var vals []*hawk.Val
	var entry = []int{0, 1, -1, 777777, -333333}
	var i int
	var err error

	a, err = rtx.NewArrVal(0)
	if err != nil {
		return err
	}

	vals = make([]*hawk.Val, len(entry))
	for i = 0; i < len(entry); i++ {
		vals[i], err = rtx.NewIntVal(entry[i])
		if err != nil {
			return err
		}
		force_full_gc()
		if err = a.SetArrayField(i+1, vals[i]); err != nil {
			return err
		}
	}

	keep_vals_alive(vals)
	rtx.SetFuncRet(a)
	force_full_gc()
	runtime.KeepAlive(a)
	return nil
}

func run_hawk(h *hawk.Hawk, id int, t *testing.T, wg *sync.WaitGroup) {
	var rtx *hawk.Rtx
	var v *hawk.Val
	//var ret string
	var ret []byte //float64
	var i int
	var err error

	defer wg.Done()

	rtx, err = h.NewRtx(fmt.Sprintf("%d", id), nil, nil)
	if err != nil {
		t.Errorf("failed to create rtx id[%d] - %s", id, err.Error())
		return
	}

	v, err = rtx.Call("x",
		hawk.Must(rtx.NewVal /*FromInt*/ (id+10)),
		hawk.Must(rtx.NewVal([]byte{'A', 66, 67, 68, 69})),
		hawk.Must(rtx.NewVal /*FromStr*/ ("this is cool")))
	if err != nil {
		t.Errorf("failed to invoke function 'x' for rtx id[%d] - %s", id, err.Error())
		rtx.Close()
		return
	}

	ret, err = v.ToByteArr()
	if err != nil {
		t.Errorf("failed to get return value for rtx id[%d] - %s", id, err.Error())
		rtx.Close()
		return
	}
	fmt.Printf("RET[%d] => [%v]\n", id, ret)

	// check if ValCount() returns the right number of values created. 3 explicitly and 1 return value
	i = rtx.ValCount()
	if i != 4 {
		t.Errorf("the number of val objects for rtx id[%d] must be 4. but %d was returned", id, i)
	}

	rtx.Close()

	// it's safe to all ValCount() after Close() has been called.
	i = rtx.ValCount()
	if i != 0 {
		t.Errorf("the number of val objects for rtx id[%d] must be 0. but %d was returned", id, i)
	}
}

func Test1(t *testing.T) {
	var h *hawk.Hawk
	var wg sync.WaitGroup
	var i int
	var err error

	debug.SetGCPercent(100) // enable normal GC

	h, err = make_hawk(`function x(a1, a2, a3) {
for (i = 0; i < 10; i++) {
	printf("hello, world [%d] [%s] [%s]\n", a1, a2, a3);
	##if (i == 3) sys::sleep(1);
}
##return "welcome to the jungle 999";
return 1.9923;
}`)
	if err != nil {
		t.Errorf("Failed to make hawk - %s", err.Error())
		return
	}

	for i = 0; i < 10; i++ {
		wg.Add(1)
		go run_hawk(h, i, t, &wg)
	}
	wg.Wait()

	// when rtx objects are all closed, the counter must drop to 0
	i = h.RtxCount()
	if i != 0 {
		t.Errorf("the number of rtx objects must be 0. but %d was returned", i)
	}

	h.Close()

	h = nil
	fmt.Printf("== END of Test1 ==\n")
	runtime.GC()
	runtime.Gosched()
	time.Sleep(1000 * time.Millisecond) // give finalizer time to print
}

func Test2(t *testing.T) {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var err error

	debug.SetGCPercent(100) // enable normal GC

	h, err = make_hawk(`function get_map(s) {
@local x
x["hello"] = s
x[99] = "donkey is running"
x["what"] = "rankey gankey"
x[1.239] = @b"nice value"
return x
}
function get_arr(s) {
@local x;
x = hawk::array(s, (s %% s), 10, 20.99, "what the");
delete(x[3]);
for (i in x) print i, x[i];
return x;
}`)
	if err != nil {
		t.Errorf("Failed to make hawk - %s", err.Error())
		return
	}

	rtx, err = h.NewRtx("test2", nil, nil)
	if err != nil {
		t.Errorf("failed to create rtx - %s", err.Error())
	} else {
		var v *hawk.Val

		v, err = rtx.Call("get_map", hawk.Must(rtx.NewStrVal("hawk flies")))
		if err != nil {
			t.Errorf("failed to call get_map - %s", err.Error())
		} else {
			if v.Type() != hawk.VAL_MAP {
				t.Errorf("the returned value must be %s. but it was %s", hawk.VAL_MAP.String(), v.Type().String())
			} else {
				var f *hawk.Val
				var kk string
				var vv *hawk.Val
				var itr hawk.ValMapItr

				f = hawk.Must(v.GetMapField("hello"))
				if f.Type() != hawk.VAL_STR {
					t.Errorf("the value at the hello field must be a string. but it was %s", f.Type().String())
				} else {
					var sv string
					sv = hawk.Must(f.ToStr())
					if sv != "hawk flies" {
						t.Errorf("the value for the hello field must be 'hawk flies'. but it was %s", sv)
					}
				}

				f, err = v.GetMapField("HELLO")
				if err == nil {
					t.Errorf("the value at the HELLO field must not be found. but it was %s", f.Type().String())
				}

				fmt.Printf("== DUMPING MAP ==\n")
				kk, vv = v.GetFirstMapField(&itr)
				for vv != nil {
					fmt.Printf("key=[%s] value=[%v]\n", kk, vv.String())
					kk, vv = v.GetNextMapField(&itr)
				}

				fmt.Printf("== CHANGING MAP ==\n")
				v.SetMapField("666.666", hawk.Must(rtx.NewFltVal(66666.66666)))
				v.SetMapField("hello", hawk.Must(rtx.NewStrVal("all stars")))

				fmt.Printf("== DUMPING MAP ==\n")
				kk, vv = v.GetFirstMapField(&itr)
				for vv != nil {
					fmt.Printf("key=[%s] value=[%v]\n", kk, vv.String())
					kk, vv = v.GetNextMapField(&itr)
				}
			}
		}

		v, err = rtx.Call("get_arr", hawk.Must(rtx.NewStrVal("hawk flies")))
		if err != nil {
			t.Errorf("failed to call get_arr - %s", err.Error())
		} else {
			if v.Type() != hawk.VAL_ARR {
				t.Errorf("the returned value must be %s. but it was %s", hawk.VAL_ARR.String(), v.Type().String())
			} else {
				var sz int
				var f *hawk.Val

				sz = v.ArrayTally()
				if sz != 4 {
					t.Errorf("the returned value must have 4 elements. but it had %d elements", sz)
				}

				f = hawk.Must(v.GetArrayField(2))
				if f.Type() != hawk.VAL_STR {
					t.Errorf("the value at the hello field must be a string. but it was %s", f.Type().String())
				} else {
					var i int
					var sv string
					var ff *hawk.Val
					var itr hawk.ValArrayItr

					sv = hawk.Must(f.ToStr())
					if sv != "hawk flieshawk flies" {
						t.Errorf("the value for the hello field must be 'hawk flieshawk flies'. but it was %s", sv)
					}

					/*
						for i = 1; i <= sz; i++ {
							fmt.Printf("%d %v\n", i, hawk.Must(v.GetArrayField(i)))
						}
					*/
					fmt.Printf("== CHANGING ARRAY ==\n")
					v.SetArrayField(88, hawk.Must(rtx.NewStrVal("eighty eight")))
					v.SetArrayField(77, hawk.Must(rtx.NewStrVal("seventy seventy")))
					fmt.Printf("== DUMPING ARRAY ==\n")
					i, ff = v.GetFirstArrayField(&itr)
					for ff != nil {
						fmt.Printf("index=[%d] value=[%v]\n", i, ff.String())
						i, ff = v.GetNextArrayField(&itr)
					}
					fmt.Printf("== END OF ARRAY DUMP ==\n")
				}
			}
		}
	}

	h.Close()

	runtime.GC()
	runtime.Gosched()
	time.Sleep(1000 * time.Millisecond) // give finalizer time to print
}

func Test3(t *testing.T) {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var err error

	debug.SetGCPercent(100) // enable normal GC

	fmt.Printf("BEGINNING OF TEST3\n")

	h, err = make_hawk_extended(`function main(s) {
print enbase64(s, "hello", 1.289);
print debase64(s);
return x
}`)
	if err != nil {
		t.Errorf("Failed to make hawk - %s", err.Error())
		return
	}

	rtx, err = h.NewRtx("test3", nil, nil)
	if err != nil {
		t.Errorf("failed to create rtx - %s", err.Error())
	} else {
		var v *hawk.Val

		v, err = rtx.Call("main", hawk.Must(rtx.NewStrVal("this is a test3 string")))
		if err != nil {
			t.Errorf("failed to call main - %s", err.Error())
		} else {
			fmt.Printf("V=>[%v]\n", v.String())
		}
	}

	h.Close()
	fmt.Printf("END OF TEST3\n")

	runtime.GC()
	runtime.Gosched()
	time.Sleep(1000 * time.Millisecond) // give finalizer time to print
}

func Test4(t *testing.T) {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var v *hawk.Val
	var gm *hawk.Val
	var ga *hawk.Val
	var err error
	var round int

	debug.SetGCPercent(100)

	h, err = hawk.New()
	if err != nil {
		t.Fatalf("Failed to make hawk - %s", err.Error())
	}

	h.AddFunc("echoint", 1, 1, "", echoint)

	err = h.ParseText(`function ret_zero() { return 0; }
function ret_one() { return 1; }
function ret_neg() { return -1; }
function call_echo_zero() { return echoint(0); }
function call_echo_neg() { return echoint(-1); }
function make_int_map() {
	@local x;
	x["zero"] = 0;
	x["one"] = 1;
	x["neg"] = -1;
	return x;
}
function make_int_arr() {
	@local x;
	x = hawk::array(0, 1, -1);
	return x;
}`)
	if err != nil {
		h.Close()
		t.Fatalf("Failed to parse hawk script - %s", err.Error())
	}

	rtx, err = h.NewRtx("test4", nil, nil)
	if err != nil {
		h.Close()
		t.Fatalf("failed to create rtx - %s", err.Error())
	}

	for round = 0; round < 32; round++ {
		for _, tc := range []struct {
			name string
			want int
		}{
			{"ret_zero", 0},
			{"ret_one", 1},
			{"ret_neg", -1},
			{"call_echo_zero", 0},
			{"call_echo_neg", -1},
		} {
			force_gc()
			v, err = rtx.Call(tc.name)
			if err != nil {
				rtx.Close()
				h.Close()
				t.Fatalf("failed to call %s - %s", tc.name, err.Error())
			}
			force_gc()
			expect_int_val(t, v, tc.want, tc.name)
		}
	}

	v, err = rtx.Call("make_int_map")
	if err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to call make_int_map - %s", err.Error())
	}
	expect_map_ints(t, v, map[string]int{
		"zero": 0,
		"one":  1,
		"neg":  -1,
	})

	v, err = rtx.Call("make_int_arr")
	if err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to call make_int_arr - %s", err.Error())
	}
	expect_array_ints(t, v, map[int]int{
		1: 0,
		2: 1,
		3: -1,
	})

	gm = hawk.Must(rtx.NewMapVal())
	if err = gm.SetMapFieldWithInt("zero", 0); err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to set zero map field - %s", err.Error())
	}
	if err = gm.SetMapFieldWithInt("one", 1); err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to set one map field - %s", err.Error())
	}
	if err = gm.SetMapFieldWithInt("neg", -1); err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to set neg map field - %s", err.Error())
	}
	expect_map_ints(t, gm, map[string]int{
		"zero": 0,
		"one":  1,
		"neg":  -1,
	})

	ga = hawk.Must(rtx.NewArrVal(0))
	if err = ga.SetArrayFieldWithInt(1, 0); err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to set array field 1 - %s", err.Error())
	}
	if err = ga.SetArrayFieldWithInt(2, 1); err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to set array field 2 - %s", err.Error())
	}
	if err = ga.SetArrayFieldWithInt(3, -1); err != nil {
		rtx.Close()
		h.Close()
		t.Fatalf("failed to set array field 3 - %s", err.Error())
	}
	expect_array_ints(t, ga, map[int]int{
		1: 0,
		2: 1,
		3: -1,
	})

	rtx.Close()
	h.Close()

	force_gc()
	time.Sleep(100 * time.Millisecond)
}

func Test5TaggedPointerStress(t *testing.T) {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var v *hawk.Val
	var args []*hawk.Val
	var gzero int
	var gone int
	var gneg int
	var round int
	var old_gc_percent int
	var err error

	old_gc_percent = debug.SetGCPercent(1)
	defer debug.SetGCPercent(old_gc_percent)

	h, err = hawk.New()
	if err != nil {
		t.Fatalf("Failed to make hawk - %s", err.Error())
	}

	h.AddFunc("go_echo_tagged", 1, 1, "", echo_tagged)
	h.AddFunc("go_sum_tagged", 1, 16, "", sum_tagged)
	h.AddFunc("go_make_tagged_map", 0, 0, "", make_tagged_map)
	h.AddFunc("go_make_tagged_arr", 0, 0, "", make_tagged_arr)

	gzero, err = h.AddGlobal("gzero")
	if err != nil {
		h.Close()
		t.Fatalf("failed to add global gzero - %s", err.Error())
	}
	gone, err = h.AddGlobal("gone")
	if err != nil {
		h.Close()
		t.Fatalf("failed to add global gone - %s", err.Error())
	}
	gneg, err = h.AddGlobal("gneg")
	if err != nil {
		h.Close()
		t.Fatalf("failed to add global gneg - %s", err.Error())
	}

	err = h.ParseText(`function ret_arg(a) { return a; }
function wrap_echo(a) { return go_echo_tagged(a); }
function wrap_sum(a, b, c, d, e) { return go_sum_tagged(a, b, c, d, e); }
function wrap_map() { return go_make_tagged_map(); }
function wrap_arr() { return go_make_tagged_arr(); }
function make_hawk_map() {
	@local x;
	x["zero"] = 0;
	x["one"] = 1;
	x["neg"] = -1;
	x["big"] = 777777;
	x["small"] = -333333;
	return x;
}
function make_hawk_arr() {
	@local x;
	x = hawk::array(0, 1, -1, 777777, -333333);
	return x;
}
function sum_globals() { return gzero + gone + gneg; }`)
	if err != nil {
		h.Close()
		t.Fatalf("Failed to parse hawk script - %s", err.Error())
	}

	rtx, err = h.NewRtx("test5", nil, nil)
	if err != nil {
		h.Close()
		t.Fatalf("failed to create rtx - %s", err.Error())
	}

	for round = 0; round < 128; round++ {
		args = []*hawk.Val{
			hawk.Must(rtx.NewIntVal(0)),
			hawk.Must(rtx.NewIntVal(1)),
			hawk.Must(rtx.NewIntVal(-1)),
			hawk.Must(rtx.NewIntVal(round)),
			hawk.Must(rtx.NewIntVal(-round - 1)),
			hawk.Must(rtx.NewByteVal(byte('A' + (round % 26)))),
			hawk.Must(rtx.NewCharVal(rune('a' + (round % 26)))),
		}

		keep_vals_alive(args)

		if err = rtx.SetGlobal(gzero, args[0]); err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to set gzero - %s", err.Error())
		}
		if err = rtx.SetGlobal(gone, args[1]); err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to set gone - %s", err.Error())
		}
		if err = rtx.SetGlobal(gneg, args[2]); err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to set gneg - %s", err.Error())
		}

		force_full_gc()
		v, err = rtx.Call("sum_globals")
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call sum_globals - %s", err.Error())
		}
		force_full_gc()
		expect_int_val(t, v, 0, "sum_globals")

		v, err = rtx.Call("ret_arg", args[0])
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call ret_arg - %s", err.Error())
		}
		force_full_gc()
		expect_int_val(t, v, 0, "ret_arg(0)")

		v, err = rtx.Call("wrap_echo", args[1])
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call wrap_echo for int - %s", err.Error())
		}
		force_full_gc()
		expect_int_val(t, v, 1, "wrap_echo(1)")

		v, err = rtx.Call("wrap_sum", args[0], args[1], args[2], args[3], args[4])
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call wrap_sum - %s", err.Error())
		}
		force_full_gc()
		expect_int_val(t, v, -1, "wrap_sum")

		v, err = rtx.Call("wrap_arr")
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call wrap_arr - %s", err.Error())
		}
		expect_array_ints_with_gc(t, v, map[int]int{
			1: 0,
			2: 1,
			3: -1,
			4: 777777,
			5: -333333,
		}, force_full_gc)

		v, err = rtx.Call("wrap_map")
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call wrap_map - %s", err.Error())
		}
		expect_map_ints_with_gc(t, v, map[string]int{
			"zero":  0,
			"one":   1,
			"neg":   -1,
			"big":   777777,
			"small": -333333,
		}, force_full_gc)

		v, err = rtx.Call("make_hawk_arr")
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call make_hawk_arr - %s", err.Error())
		}
		expect_array_ints_with_gc(t, v, map[int]int{
			1: 0,
			2: 1,
			3: -1,
			4: 777777,
			5: -333333,
		}, force_full_gc)

		v, err = rtx.Call("make_hawk_map")
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call make_hawk_map - %s", err.Error())
		}
		expect_map_ints_with_gc(t, v, map[string]int{
			"zero":  0,
			"one":   1,
			"neg":   -1,
			"big":   777777,
			"small": -333333,
		}, force_full_gc)

		v, err = rtx.Call("wrap_echo", args[5])
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call wrap_echo for byte value - %s", err.Error())
		}
		force_full_gc()
		expect_type(t, v, hawk.VAL_BCHR, "wrap_echo(byte)")

		v, err = rtx.Call("wrap_echo", args[6])
		if err != nil {
			rtx.Close()
			h.Close()
			t.Fatalf("failed to call wrap_echo for char value - %s", err.Error())
		}
		force_full_gc()
		expect_type(t, v, hawk.VAL_CHAR, "wrap_echo(char)")

		keep_vals_alive(args)
	}

	rtx.Close()
	h.Close()

	force_full_gc()
	time.Sleep(100 * time.Millisecond)
}
