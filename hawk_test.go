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

func make_hawk() (*hawk.Hawk, error) {
	var h *hawk.Hawk
	var err error

	h, err = hawk.New()
	if err != nil { return nil, err }

	err = h.ParseText(`function x(a1, a2, a3) {
for (i = 0; i < 10; i++) {
	printf("hello, world [%d] [%s] [%s]\n", a1, a2, a3);
	##if (i == 3) sys::sleep(1);
}
##return "welcome to the jungle 999";
return 1.9923;
}`)
	if err != nil { return nil, err }

	return h, nil
}

func run_hawk(h *hawk.Hawk, id int, wg *sync.WaitGroup) {
	var r *hawk.Rtx
	var v *hawk.Val
	//var ret string
	var ret []byte //float64
	var err error

	defer wg.Done()

	r, err = h.NewRtx(fmt.Sprintf("%d", id))	
	if err != nil {
	}

	v, err = r.Call("x",
		hawk.Must(r.NewVal/*FromInt*/(id + 10)),
		hawk.Must(r.NewVal([]byte{'A',66,67,68,69})),
		hawk.Must(r.NewVal/*FromStr*/("this is cool")))
	if err != nil { exit_with_error("rtx call", err) }

	ret, err = v.ToByteArr()
	if err != nil {
		fmt.Printf("failed to get return value - %s\n", err.Error())
	} else {
		fmt.Printf("RET[%d] => [%v]\n", id, ret)
	}

	r.Close()
}

func Test1(t *testing.T) {
	var h *hawk.Hawk
	var wg sync.WaitGroup
	var i int
	var err error

	debug.SetGCPercent(100) // enable normal GC

	h, err = make_hawk()
	if err != nil { exit_with_error("Failed to make hawk", err) }

	for i = 0; i < 10; i++ {
		wg.Add(1)
		go run_hawk(h, i, &wg)
	}
	wg.Wait()

	fmt.Printf ("%d RTX objects\n", h.RtxCount())
	h.Close()
	h = nil

fmt.Printf ("== END of run ==\n")
	runtime.GC()
	runtime.Gosched()
	time.Sleep(1000 * time.Millisecond) // give finalizer time to print
}

