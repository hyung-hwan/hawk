package main

import "hawk"
import "flag"
import "fmt"
import "io"
//import "net/http"
//import _ "net/http/pprof"
import "os"
import "os/signal"
import "runtime"
import "runtime/debug"
import "strconv"
import "strings"
import "sync"
import "syscall"
//import "time"

type Assign struct {
	idx int
	value string
}

type Config struct {
	assigns map[string]Assign
	call string
	fs string
	modlibdirs string
	show_extra_info bool
	concurrent bool
	suffix string

	srcstr string
	srcfiles []string
	data_in_files []string
	data_out_files []string
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
	flgs.BoolFunc("concurrent", "run the script over multiple data files concurrently", func(v string) error {
		if v[0] == '.' {
			// treat it as a suffix
			cfg.suffix = v
			cfg.concurrent = true
		} else {
			cfg.suffix = ""
			cfg.concurrent, _ = strconv.ParseBool(v)
		}
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
	flgs.Func("modlibdirs", "set module library directories", func(v string) error {
		cfg.modlibdirs = v
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
	cfg.data_in_files = flgs.Args()
	if len(cfg.srcfiles) <= 0 {
		if len(cfg.data_in_files) == 0 { goto wrong_usage }
		cfg.srcstr = cfg.data_in_files[0]
		cfg.data_in_files = cfg.data_in_files[1:]
	}

	if cfg.concurrent && len(cfg.data_in_files) > 0 {
		var i int
		var n int
		var f []string
		n = len(cfg.data_in_files)
		cfg.data_out_files = make([]string, n)
		for i = 0; i < n; i++ {
			f = strings.SplitN(cfg.data_in_files[i], ":", 2)
			cfg.data_in_files[i] = f[0]
			if len(f) >= 2 {
				cfg.data_out_files[i] = f[1]
			} else if cfg.suffix != "" && cfg.data_in_files[i] != "" && cfg.data_in_files[i] != "-" {
				cfg.data_out_files[i] = cfg.data_in_files[i] + cfg.suffix
			}
		}
	}
	return true

wrong_usage:
	fmt.Fprintf(os.Stderr, "USAGE: %s [options] sourcestring [datafile]*\n", os.Args[0])
	fmt.Fprintf(os.Stderr, "       %s [options] -f sourcefile [datafile]*\n", os.Args[0])
	fmt.Fprintf(os.Stderr, "Options are:\n")
	fmt.Fprintf(os.Stderr, " -F, --field-separator string     specify the field seperator\n")
	fmt.Fprintf(os.Stderr, " -v, --assign          name=value set global variable value\n")
	fmt.Fprintf(os.Stderr, " -c, --call            string     call a named function instead of running the\n")
	fmt.Fprintf(os.Stderr, "                                  normal loop\n")
	fmt.Fprintf(os.Stderr, "     --modlibdirs      string     set module library directories\n")
	fmt.Fprintf(os.Stderr, "     --concurrent=[suffix]        process multiple data files concurrently\n")
	fmt.Fprintf(os.Stderr, "                                  if datafile has two parts delimited by a colon,\n")
	fmt.Fprintf(os.Stderr, "                                  the second part specifies the output file (e.g.\n")
	fmt.Fprintf(os.Stderr, "                                  infile:outfile). The suffix beginning with a\n")
	fmt.Fprintf(os.Stderr, "                                  period is appended to datafile to form the out-\n")
	fmt.Fprintf(os.Stderr, "                                  put file name if the second part is missing\n")
	return false
}

func run_script(h *hawk.Hawk, fs_idx int, data_idx int, cfg* Config, rtx_chan chan *hawk.Rtx, sig_chan chan os.Signal) error {
	var rtx *hawk.Rtx
	var err error

	if data_idx <= -1 {
		rtx, err = h.NewRtx(os.Args[0], cfg.data_in_files, nil)
	} else {
		var out_idx_end int = data_idx

		if cfg.data_out_files[data_idx] != "" { out_idx_end++ }
		rtx, err = h.NewRtx(
			fmt.Sprintf("%s.%d", os.Args[0], data_idx),
			cfg.data_in_files[data_idx: data_idx + 1],
			cfg.data_out_files[data_idx: out_idx_end],
		)
	}
	if err != nil {
		err = fmt.Errorf("failed to make rtx - %s", err.Error())
		goto oops
	} else  {
		var k string
		var v Assign
		var retv *hawk.Val

		for k, v = range cfg.assigns {
			if v.idx >= 0  {
				var vv *hawk.Val
				vv, err = rtx.NewNumOrStrVal(v.value, 1)
				if err != nil {
					err = fmt.Errorf("failed to convert value '%s' for global variable '%s' - %s", v.value, k, err.Error())
					goto oops
				}

				err = rtx.SetGlobal(v.idx, vv)
				vv.Close()
				if err != nil {
					err = fmt.Errorf("failed to set global variable '%s' - %s", k, err.Error())
					goto oops
				}
			}
		}

		if fs_idx >= 0 {
			var vv *hawk.Val
			vv, err = rtx.NewStrVal(cfg.fs)
			if err != nil {
				err = fmt.Errorf("failed to convert field separator '%s' - %s", cfg.fs, err.Error())
				goto oops
			}

			rtx.SetGlobal(fs_idx, vv)
			vv.Close()
			if err != nil {
				err = fmt.Errorf("failed to set field separator to '%s' - %s", cfg.fs, err.Error())
				goto oops
			}
		}

		if cfg.call != "" {
			var idx int
			var count int
			var args []*hawk.Val

			count = len(cfg.data_in_files)
			args = make([]*hawk.Val, count)
			for idx = 0; idx < count; idx++ {
				args[idx], err = rtx.NewStrVal(cfg.data_in_files[idx])
				if err != nil {
					err = fmt.Errorf("failed to convert argument [%s] to value - %s", cfg.data_in_files[idx], err.Error())
					goto oops
				}
			}

			rtx.OnSigset(func(rtx *hawk.Rtx, signo int, reset bool) {
				var s os.Signal
				s = syscall.Signal(signo)
				if reset {
					signal.Reset(s)
				} else {
					signal.Notify(sig_chan, s)
				}
			})
			rtx_chan <- rtx
			retv, err = rtx.Call(cfg.call, args...)
			for idx = 0; idx < count; idx++ {
				// it's ok not to call Close() on args as rtx.Close() closes them automatically.
				// if args are re-created repeatedly, Close() on them is always needed not to
				// accumulate too many allocated values.
				args[idx].Close()
			}
		} else {
			rtx.OnSigset(func(rtx *hawk.Rtx, signo int, reset bool) {
				var s os.Signal
				s = syscall.Signal(signo)
				if reset {
					signal.Reset(s)
				} else {
					signal.Notify(sig_chan, s)
				}
			})
			rtx_chan <- rtx
			//v, err = rtx.Loop()
			retv, err = rtx.Exec(cfg.data_in_files)
		}
		if err != nil {
			err = fmt.Errorf("failed to run - %s", err.Error())
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
	return nil

oops:
	if rtx != nil { rtx.Close() }
	return err
}

func run_signal_handler(rtx_chan chan *hawk.Rtx, sig_chan chan os.Signal, wg *sync.WaitGroup) {
	var sig os.Signal
	var rtx *hawk.Rtx
	var rtx_bag map[*hawk.Rtx]struct{}

	defer wg.Done()
	rtx_bag = make(map[*hawk.Rtx]struct{})

	//signal.Notify(sig_chan, syscall.SIGHUP, syscall.SIGTERM, os.Interrupt)

chan_loop:
	for {
		select {
		case rtx = <-rtx_chan:
			if rtx == nil {
				// terminate the loop for the value of nil
				break chan_loop
			}

			// remember all the rtx object created for signal broadcasting
			rtx_bag[rtx] = struct{}{}

		case sig = <- sig_chan:
			var signo syscall.Signal
			var ok bool
			signo, ok = sig.(syscall.Signal)
			if ok {
				// broadcast the signal
				for rtx, _ = range rtx_bag {
					rtx.RaiseSignal(int(signo))
				}
			}
		}
	}
}

func main() {
	var h *hawk.Hawk
	var cfg Config
	var fs_idx int = -1
	var rtx_chan chan *hawk.Rtx
	var sig_chan chan os.Signal
	var sig_wg sync.WaitGroup
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

	if cfg.modlibdirs != "" {
		err = h.SetModLibDirs(cfg.modlibdirs)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: failed to set module library directories '%s' - %s\n", cfg.modlibdirs, err.Error())
			goto oops
		}
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

	err = h.AddMod("go")
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to builtin 'go' module - %s\n", err.Error())
		goto oops
	}

	if (len(cfg.srcfiles) > 0) {
		err = h.ParseFiles(cfg.srcfiles)
	} else {
		err = h.ParseText(cfg.srcstr)
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: failed to parse - %s\n", err.Error())
		goto oops
	}

	rtx_chan = make(chan *hawk.Rtx, 10)
	sig_chan = make(chan os.Signal, 5)
	sig_wg.Add(1)
	go run_signal_handler(rtx_chan, sig_chan, &sig_wg)

	if cfg.concurrent && len(cfg.data_in_files) >= 1 {
		var n int
		var i int
		var wg sync.WaitGroup

		n = len(cfg.data_in_files)
		for i = 0; i < n; i++ {
			wg.Add(1)
			go func(idx int) {
				var err error
				defer wg.Done()
				err = run_script(h, fs_idx, idx, &cfg, rtx_chan, sig_chan)
				if err != nil {
					fmt.Fprintf(os.Stderr, "ERROR: [%d]%s\n", idx, err.Error())
				}
			}(i)
		}
		wg.Wait()
	} else {
		err = run_script(h, fs_idx, -1, &cfg, rtx_chan, sig_chan)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: %s\n", err.Error())
			goto oops
		}
	}

	rtx_chan <- nil // to terminate the signal handler
	sig_wg.Wait()
	h.Close()

	runtime.GC()
	runtime.Gosched()

	//time.Sleep(100000 * time.Millisecond)
	return

oops:
//	if rtx != nil { rtx.Close() }
	if rtx_chan != nil {
		rtx_chan <- nil
		sig_wg.Wait()
	}
	if h != nil { h.Close() }
	os.Exit(255)
}
