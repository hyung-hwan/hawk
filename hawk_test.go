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

func expect_map_ints(t *testing.T, v *hawk.Val, want map[string]int) {
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
		force_gc()
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

		force_gc()
		iv, err = f.ToInt()
		if err != nil {
			t.Fatalf("failed to convert iterated map value for key %q - %s", key, err.Error())
		}
		got[key] = iv

		force_gc()
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

func expect_array_ints(t *testing.T, v *hawk.Val, want map[int]int) {
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
		force_gc()
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

		force_gc()
		iv, err = f.ToInt()
		if err != nil {
			t.Fatalf("failed to convert iterated array value at %d - %s", idx, err.Error())
		}
		got[idx] = iv

		force_gc()
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
