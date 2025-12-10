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
	if err != nil { return nil, err }

	err = h.ParseText(script)
	if err != nil {
		h.Close()
		return nil, err
	}

	return h, nil
}

func enbase64(rtx *hawk.Rtx) error {
	fmt.Printf ("*** ENABLE64 RTX %p\n", rtx) // << this is 0 from time to time.. TODO: fix it..
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
//return fmt.Errorf("what the hell.....")
}

func make_hawk_extended(script string) (*hawk.Hawk, error) {
	var h *hawk.Hawk
	var err error

	h, err = hawk.New()
	if err != nil { return nil, err }

	h.AddFunc("enbase64", 1, 10, "", enbase64)
	h.AddFunc("debase64", 1, 1, "", debase64)

	err = h.ParseText(script)
	if err != nil {
		h.Close()
		return nil, err
	}

	return h, nil
}

func run_hawk(h *hawk.Hawk, id int, t *testing.T, wg *sync.WaitGroup) {
	var rtx *hawk.Rtx
	var v *hawk.Val
	//var ret string
	var ret []byte //float64
	var i int
	var err error

	defer wg.Done()

	rtx, err = h.NewRtx(fmt.Sprintf("%d", id), nil)
	if err != nil {
		t.Errorf("failed to create rtx id[%d] - %s", id, err.Error())
		return
	}

	v, err = rtx.Call("x",
		hawk.Must(rtx.NewVal/*FromInt*/(id + 10)),
		hawk.Must(rtx.NewVal([]byte{'A',66,67,68,69})),
		hawk.Must(rtx.NewVal/*FromStr*/("this is cool")))
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

	// check if ValCount() returns the right number of values created explicitly
	i = rtx.ValCount()
	if i != 3 { t.Errorf("the number of val objects for rtx id[%d] must be 3. but %d was returned", id, i) }

	rtx.Close()

	// it's safe to all ValCount() after Close() has been called.
	i = rtx.ValCount()
	if i != 0 { t.Errorf("the number of val objects for rtx id[%d] must be 0. but %d was returned", id, i) }
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
	if i != 0 { t.Errorf("the number of rtx objects must be 0. but %d was returned", i) }

	h.Close()

	h = nil
fmt.Printf ("== END of Test1 ==\n")
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

	rtx, err = h.NewRtx("test2", nil)
	if err != nil {
		t.Errorf("failed to create rtx - %s", err.Error())
	} else  {
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

				f = hawk.Must(v.MapField("hello"))
				if f.Type() != hawk.VAL_STR {
					t.Errorf("the value at the hello field must be a string. but it was %s", f.Type().String())
				} else {
					var sv string
					sv = hawk.Must(f.ToStr())
					if sv != "hawk flies" {
						t.Errorf("the value for the hello field must be 'hawk flies'. but it was %s", sv)
					}
				}

				f, err = v.MapField("HELLO")
				if err == nil {
					t.Errorf("the value at the HELLO field must not be found. but it was %s", f.Type().String())
				}

				fmt.Printf("== DUMPING MAP ==\n")
				kk, vv = v.MapFirstField(&itr)
				for vv != nil {
					fmt.Printf("key=[%s] value=[%v]\n", kk, vv.String())
					kk, vv = v.MapNextField(&itr)
				}

				fmt.Printf("== CHANGING MAP ==\n")
				v.MapSetField("666.666", hawk.Must(rtx.NewFltVal(66666.66666)))
				v.MapSetField("hello", hawk.Must(rtx.NewStrVal("all stars")))

				fmt.Printf("== DUMPING MAP ==\n")
				kk, vv = v.MapFirstField(&itr)
				for vv != nil {
					fmt.Printf("key=[%s] value=[%v]\n", kk, vv.String())
					kk, vv = v.MapNextField(&itr)
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

				f = hawk.Must(v.ArrayField(2))
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
						fmt.Printf("%d %v\n", i, hawk.Must(v.ArrayField(i)))
					}
					*/
					fmt.Printf("== CHANGING ARRAY ==\n")
					v.ArraySetField(88, hawk.Must(rtx.NewStrVal("eighty eight")))
					v.ArraySetField(77, hawk.Must(rtx.NewStrVal("seventy seventy")))
					fmt.Printf("== DUMPING ARRAY ==\n")
					i, ff = v.ArrayFirstField(&itr)
					for ff != nil {
						fmt.Printf("index=[%d] value=[%v]\n", i, ff.String())
						i, ff = v.ArrayNextField(&itr)
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

	fmt.Printf ("BEGINNING OF TEST3\n")

	h, err = make_hawk_extended(`function main(s) {
print enbase64(s, "hello", 1.289);
print debase64(s);
return x
}`)
	if err != nil {
		t.Errorf("Failed to make hawk - %s", err.Error())
		return
	}

	rtx, err = h.NewRtx("test3", nil)
	if err != nil {
		t.Errorf("failed to create rtx - %s", err.Error())
	} else  {
		var v *hawk.Val

		v, err = rtx.Call("main", hawk.Must(rtx.NewStrVal("this is a test3 string")))
		if err != nil {
			t.Errorf("failed to call main - %s", err.Error())
		} else {
			fmt.Printf("V=>[%v]\n", v.String())
		}
	}

	h.Close()
	fmt.Printf ("END OF TEST3\n")

	runtime.GC()
	runtime.Gosched()
	time.Sleep(1000 * time.Millisecond) // give finalizer time to print
}

