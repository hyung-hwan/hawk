package main

import "hawk"
import "flag"
import "fmt"
import "io"
//import "net/http"
//import _ "net/http/pprof"
import "os"
import "runtime"
import "runtime/debug"
import "strconv"
import "strings"
import "sync"
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
	fmt.Fprintf(os.Stderr, "     --concurrent=[suffix]        process multiple data files concurrently\n")
	fmt.Fprintf(os.Stderr, "                                  if datafile has two parts delimited by a colon,\n")
	fmt.Fprintf(os.Stderr, "                                  the second part specifies the output file (e.g.\n")
	fmt.Fprintf(os.Stderr, "                                  infile:outfile). The suffix beginning with a\n")
	fmt.Fprintf(os.Stderr, "                                  period is appended to datafile to form the out-\n")
	fmt.Fprintf(os.Stderr, "                                  put file name if the second part is missing\n")
	return false
}

func run_script(h *hawk.Hawk, fs_idx int, data_idx int, cfg* Config, wg *sync.WaitGroup) error {
	var rtx *hawk.Rtx
	var err error

	if wg != nil { defer wg.Done() }

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
				vv, err = rtx.NewNumOrStrVal(v.value)
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
			retv, err = rtx.Call(cfg.call, args...)
			for idx = 0; idx < count; idx++ {
				// it's ok not to call Close() on args as rtx.Close() closes them automatically.
				// if args are re-created repeatedly, Close() on them is always needed not to
				// accumulate too many allocated values.
				args[idx].Close()
			}
		} else {
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

func main() {
	var h *hawk.Hawk
	var cfg Config
	var fs_idx int = -1
	var wg sync.WaitGroup
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

	if cfg.concurrent && len(cfg.data_in_files) >= 1 {
		var n int
		var i int
		n = len(cfg.data_in_files)
		for i = 0; i < n; i++ {
			wg.Add(1)
			go func(idx int) {
				var err error
				err = run_script(h, fs_idx, idx, &cfg, &wg)
				if err != nil {
					fmt.Fprintf(os.Stderr, "ERROR: [%d]%s\n", idx, err.Error())
				}
			}(i)
		}
		wg.Wait()
	} else {
		err = run_script(h, fs_idx, -1, &cfg, nil)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: %s\n", err.Error())
			goto oops
		}
	}

	if h != nil { h.Close() }

	runtime.GC()
	runtime.Gosched()

	//time.Sleep(100000 * time.Millisecond)
	return

oops:
//	if rtx != nil { rtx.Close() }
	if h != nil { h.Close() }
	os.Exit(255)
}

