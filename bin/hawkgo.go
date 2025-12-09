package main

import "hawk"
import "flag"
import "fmt"
import "io"
import "os"
import "path/filepath"
import "runtime"
import "runtime/debug"
//import "time"

type Config struct {
	assign string
	call string
	fs string
	show_extra_info bool

	srcstr string
	srcfiles []string
	datfiles []string
}

func exit_with_error(msg string, err error) {
	fmt.Fprintf(os.Stderr, "ERROR: %s - %s\n", msg, err.Error())
	os.Exit(1)
}

func parse_args_to_config(cfg *Config) bool {
	//var flgs *flag.FlagSet
	var flgs *FlagSet
	var err error

	//flgs = flag.NewFlagSet("", flag.ContinueOnError)
	flgs = NewFlagSet("", flag.ContinueOnError)
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
	flgs.Func("file", "set the source file", func(v string) error {
		cfg.srcfiles = append(cfg.srcfiles, v)
		return nil
	})
	flgs.BoolVar(&cfg.show_extra_info, "show-extra-info", false, "show extra information")

	flgs.Alias("c", "call")
	flgs.Alias("D", "show-extra-info")
	flgs.Alias("F", "field-separator")
	flgs.Alias("f", "file")
	flgs.Alias("v", "assign")

	flgs.SetOutput(io.Discard) // prevent usage output
	//err = flgs.Parse(os.Args[1:])
	err = flgs.Parse(os.Args[1:])
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: %s\n", err.Error())
		goto wrong_usage
	}

	//if flgs.NArg() == 0 { goto wrong_usage}
	cfg.datfiles = flgs.Args()
	if len(cfg.srcfiles) <= 0 {
		if len(cfg.datfiles) == 0 { goto wrong_usage }
		cfg.srcstr = cfg.datfiles[0]
		cfg.datfiles = cfg.datfiles[1:]
	}

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

	h, err = hawk.New()
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to make hawk - %s\n", err.Error())
		return
	}

	if (len(cfg.srcfiles) > 0) {
		err = h.ParseFiles(cfg.srcfiles)
	} else {
		err = h.ParseText(cfg.srcstr)
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to make hawk - %s\n", err.Error())
		h.Close()
		return
	}

	rtx, err = h.NewRtx(filepath.Base(os.Args[0]), cfg.datfiles)
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to make rtx - %s\n", err.Error())
	} else  {
		var v *hawk.Val
		if cfg.call != "" {
			v, err = rtx.Call(cfg.call/*, cfg.datfiles*/) // TODO: pass arguments.
		} else {
			//v, err = rtx.Loop()
			v, err = rtx.Exec(cfg.datfiles)
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: failed to run rtx - %s\n", err.Error())
		} else if cfg.show_extra_info {
			fmt.Printf("[RETURN] - [%v]\n", v.String())
			// TODO: print global variables and values
		}
	}

	h.Close()

	runtime.GC()
	runtime.Gosched()
//	time.Sleep(1000 * time.Millisecond) // give finalizer time to print
}

