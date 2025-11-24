package main

import "hawk"
import "fmt"
import "os"
import "runtime"
import "runtime/debug"
//import "sync"
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

func main() {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var err error

	debug.SetGCPercent(100) // enable normal GC

	fmt.Printf ("BEGINNING OF TEST3\n")

	h, err = make_hawk(`function main(s) {
print enbase64(s, "hello", 1.289);
print debase64(s);
return x
}`)
	if err != nil {
		fmt.Printf("ERROR: failed to make hawk - %s\n", err.Error())
		return
	}

	rtx, err = h.NewRtx("test3")
	if err != nil {
		//
	} else  {
		var v *hawk.Val

		v, err = rtx.Call("main", hawk.Must(rtx.NewValFromStr("this is a test3 string")))
		if err != nil {
			//
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

