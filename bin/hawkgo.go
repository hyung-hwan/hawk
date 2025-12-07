package main

import "hawk"
import "flag"
import "fmt"
import "io"
import "os"
import "runtime"
import "runtime/debug"
//import "sync"
import "time"

type Config struct {
	assign string
	call string
	fs string
	srcstr string
	srcfile string

	files []string
}

func exit_with_error(msg string, err error) {
	fmt.Printf("ERROR: %s - %s\n", msg, err.Error())
	os.Exit(1)
}

func parse_args_to_config(cfg *Config) bool {
	var flgs *flag.FlagSet
	var err error

	flgs = flag.NewFlagSet("", flag.ContinueOnError)
	flgs.Func("assign", "set a global variable with a value", func(v string) error {
		cfg.assign = v
		return nil
	})
	flgs.Func("call", "call a function instead of the pattern-action block)", func(v string) error {
		cfg.call = v
		return nil
	})
	flgs.Func("field-separator", "set the field separator(FS)", func(v string) error {
		cfg.fs = v
		return nil
	})

	flgs.SetOutput(io.Discard) // prevent usage output
	err = flgs.Parse(os.Args[1:])
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: %s\n", err.Error())
		goto wrong_usage
	}

	if flgs.NArg() == 0 { goto wrong_usage}
	cfg.files = flgs.Args()

	return true

wrong_usage:
	fmt.Fprintf(os.Stderr, "USAGE: %s [options] sourcestring [datafile]*\n", os.Args[0])
	fmt.Fprintf(os.Stderr, "       %s [options] -f sourcefile [datafile]*\n", os.Args[0])
	return false
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
	var cfg Config
	var err error

	debug.SetGCPercent(100) // enable normal GC

	if parse_args_to_config(&cfg) == false { os.Exit(99) }
fmt.Printf("config [%+v]\n", cfg)

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
		fmt.Printf("ERROR: failed to make rtx - %s\n", err.Error())
	} else  {
		var v *hawk.Val
		v, err = rtx.Call("main", hawk.Must(rtx.NewValFromStr("this is a test3 string")))
		if err != nil {
			fmt.Printf("ERROR: failed to make rtx - %s\n", err.Error())
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

