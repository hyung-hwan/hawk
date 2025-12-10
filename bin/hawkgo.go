package main

import "hawk"
import "flag"
import "fmt"
import "io"
//import "net/http"
//import _ "net/http/pprof"
import "os"
import "path/filepath"
import "runtime"
import "runtime/debug"
import "strings"
//import "time"

type Assign struct {
	idx int
	value string
}

type Config struct {
	assigns map[string]Assign
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

	cfg.assigns = make(map[string]Assign)

	//flgs = flag.NewFlagSet("", flag.ContinueOnError)
	flgs = NewFlagSet("", flag.ContinueOnError)
	flgs.Func("assign", "set a global variable with a value", func(v string) error {
		var kv []string
		var vv string
		kv = strings.SplitN(v, "=", 2)
		if len(kv) >= 2 { vv = kv[1] }
		cfg.assigns[kv[0]] = Assign{idx: -1, value: vv}
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

func main() {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var cfg Config
	var fs_idx int = -1
	var err error

	// for profiling
	//go http.ListenAndServe("0.0.0.0:6060", nil)

	debug.SetGCPercent(100) // enable normal GC

	if parse_args_to_config(&cfg) == false { os.Exit(99) }

	h, err = hawk.New()
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to make hawk - %s\n", err.Error())
		goto oops
	}

	if len(cfg.assigns) > 0 {
		var k string
		var v Assign
		var idx int
		for k, v = range cfg.assigns {
			idx, err = h.FindGlobal(k, true)
			if err == nil {
				// existing variable found
				v.idx = idx
			} else {
				v.idx, err = h.AddGlobal(k)
				if err != nil {
					fmt.Fprintf(os.Stderr, "ERROR: failed to add global variable '%s' - %s\n", k, err.Error())
					goto oops
				}
			}
			cfg.assigns[k] = v
		}
	}

	if cfg.fs != "" {
		fs_idx, err = h.FindGlobal("FS", true)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: failed to find global variable 'FS' - %s\n", err.Error())
			goto oops
		}
	}

	if (len(cfg.srcfiles) > 0) {
		err = h.ParseFiles(cfg.srcfiles)
	} else {
		err = h.ParseText(cfg.srcstr)
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to make hawk - %s\n", err.Error())
		goto oops
	}

	rtx, err = h.NewRtx(filepath.Base(os.Args[0]), cfg.datfiles)
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to make rtx - %s\n", err.Error())
		goto oops
	} else  {
		var k string
		var v Assign
		var retv *hawk.Val

		for k, v = range cfg.assigns {
			if v.idx >= 0  {
				var vv *hawk.Val
				vv, err = rtx.NewNumOrStrVal(v.value)
				if err != nil {
					fmt.Fprintf(os.Stderr, "ERROR: failed to convert value '%s' for global variable '%s' - %s\n", v.value, k, err.Error())
					goto oops
				}

				err = rtx.SetGlobal(v.idx, vv)
				vv.Close()
				if err != nil {
					fmt.Fprintf(os.Stderr, "ERROR: failed to set global variable '%s' - %s\n", k, err.Error())
					goto oops
				}
			}
		}

		if fs_idx >= 0 {
			var vv *hawk.Val
			vv, err = rtx.NewStrVal(cfg.fs)
			if err != nil {
				fmt.Fprintf(os.Stderr, "ERROR: failed to convert field separator '%s' - %s\n", cfg.fs, err.Error())
				goto oops
			}

			rtx.SetGlobal(fs_idx, vv)
			vv.Close()
			if err != nil {
				fmt.Fprintf(os.Stderr, "ERROR: failed to set field separator to '%s' - %s\n", cfg.fs, err.Error())
				goto oops
			}
		}

		if cfg.call != "" {
			var idx int
			var count int
			var args []*hawk.Val

			count = len(cfg.datfiles)
			args = make([]*hawk.Val, count)
			for idx = 0; idx < count; idx++ {
				args[idx], err = rtx.NewStrVal(cfg.datfiles[idx])
				if err != nil {
					fmt.Fprintf(os.Stderr, "ERROR: failed to convert argument [%s] to value - %s\n", cfg.datfiles[idx], err.Error())
					goto oops
				}
			}
			retv, err = rtx.Call(cfg.call, args...)
			for idx = 0; idx < count; idx++ {
				// it's ok not to call Close() on args as rtx.Close() closes them automatically.
				// if args are re-created repeatedly, Close() on them is always needed not to
				// accumulate too many allocated values.
				args[idx].Close()
			}
		} else {
			//v, err = rtx.Loop()
			retv, err = rtx.Exec(cfg.datfiles)
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: failed to run rtx - %s\n", err.Error())
			goto oops
		}

		if cfg.show_extra_info {
			var named_vars map[string]*hawk.Val
			var vn string
			var vv *hawk.Val

			fmt.Printf("[RETURN] - [%v]\n", retv.String())


			fmt.Printf("NAMED VARIABLES]\n")
			named_vars = make(map[string]*hawk.Val)
			rtx.GetNamedVars(named_vars)
			for vn, vv = range named_vars {
				fmt.Printf("%s = %s\n", vn, vv.String())
			}
			fmt.Printf("END OF NAMED VARIABLES]\n")
		}
	}

	// let's not care about closing args or return values
	// because rtx.Close() will close them automatically
	if rtx != nil { rtx.Close() }
	if h != nil { h.Close() }

	runtime.GC()
	runtime.Gosched()

	//time.Sleep(100000 * time.Millisecond)
	return

oops:
	if rtx != nil { rtx.Close() }
	if h != nil { h.Close() }
	os.Exit(255)
}

