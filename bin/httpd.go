package main

import "hawk"
import "context"
import "crypto/tls"
import "flag"
import "fmt"
import "log"
import "net"
import "os"
import "os/signal"
import "strings"
import "syscall"
import "time"

func parse_cmdline(args []string, cfg *hawk.HawkHttpdConfig) (bool, error) {

	var flgs *FlagSet
	var help bool
	var key string
	var entry string
	var certificate string
	var tls_addresses []string
	var shutdown_timeout time.Duration
	var err error

	flgs = NewFlagSet("httpd", flag.ContinueOnError)

	flgs.SetOutput(os.Stderr)

	flgs.Usage = func() {
		fmt.Fprintf(os.Stderr, "USAGE: %s --httpd [options]\n", os.Args[0])
		flgs.PrintDefaults()
	}

	flgs.Func("listen", "HTTP listening address (repeatable)", func(value string) error {
		cfg.Listeners = append(cfg.Listeners, hawk.HawkHttpdListener{Address: value})
		return nil
	})
	flgs.Func("tls-listen", "HTTPS listening address (repeatable)", func(value string) error {
		tls_addresses = append(tls_addresses, value)
		return nil
	})
	flgs.StringVar(&certificate, "tls-cert", "", "TLS certificate PEM file")
	flgs.StringVar(&key, "tls-key", "", "TLS private key PEM file")
	flgs.StringVar(&cfg.DocumentRoot, "root", "", "document root for static files")
	flgs.StringVar(&cfg.ScriptRoot, "script-root", "", "base directory for relative script paths (default: working directory)")
	flgs.Func("route", "Hawk route prefix=script (repeatable)", func(value string) error {
		var parts []string
		parts = strings.SplitN(value, "=", 2)
		if len(parts) != 2 { return fmt.Errorf("route must have the form prefix=script") }
		cfg.Routes = append(cfg.Routes, hawk.HawkHttpdRoute{Prefix: parts[0], Script: parts[1]})
		return nil
	})
	flgs.StringVar(&entry, "entry", "handle", "script entry function")
	flgs.BoolVar(&help, "help", false, "show HTTP server options")
	flgs.Alias("h", "help")
	flgs.StringVar(&cfg.IncDirs, "incdirs", "", "include directories")
	flgs.StringVar(&cfg.ModLibDirs, "modlibdirs", "", "module library directories")
	flgs.IntVar(&cfg.MaxConcurrent, "max-concurrent", 64, "maximum simultaneous script requests")
	flgs.Int64Var(&cfg.MaxBodyBytes, "max-body-bytes", 1024 * 1024, "maximum request body size")
	flgs.Int64Var(&cfg.MaxResponseBytes, "max-response-bytes", 8 * 1024 * 1024, "maximum script response size")
	flgs.DurationVar(&cfg.ReloadInterval, "reload-interval", time.Second, "minimum interval between script change checks")
	flgs.DurationVar(&shutdown_timeout, "shutdown-timeout", 30 * time.Second, "graceful shutdown waiting limit")

	err = flgs.Parse(args)
	if err != nil { return true, err; }

	if help { // explict help
		flgs.Usage()
		return true, nil
	}

	if flgs.NArg() != 0 || shutdown_timeout <= 0 {
		return true, fmt.Errorf("unexpected arguments or invalid shutdown timeout")
	}

	if len(tls_addresses) > 0 {
		var pair tls.Certificate
		var tls_config *tls.Config
		var address string

		pair, err = tls.LoadX509KeyPair(certificate, key)
		if err != nil { return true, err }

		tls_config = &tls.Config{MinVersion: tls.VersionTLS12, Certificates: []tls.Certificate{pair}}
		for _, address = range tls_addresses {
			cfg.Listeners = append(cfg.Listeners, hawk.HawkHttpdListener{Address: address, TLSConfig: tls_config})
		}
	}

	return false, nil
}

func main_httpd(args []string) int {
	var cfg hawk.HawkHttpdConfig
	var server *hawk.HawkHttpd
	var bound_address net.Addr
	var entry string
	var shutdown_timeout time.Duration
	var signal_ctx context.Context
	var signal_cancel context.CancelFunc
	var shutdown_ctx context.Context
	var shutdown_cancel context.CancelFunc
	var done chan error
	var exit bool
	var err error
	var i int

	exit, err = parse_cmdline(args, &cfg)
	if err != nil { goto oops }
	if (exit) { return 0 }

	if len(cfg.Listeners) == 0 {
		cfg.Listeners = []hawk.HawkHttpdListener{{Address: "127.0.0.1:8080"}}
	}

	for i = range cfg.Routes { cfg.Routes[i].Entry = entry }
	cfg.Logger = log.New(os.Stderr, "", log.LstdFlags)

	server, err = hawk.NewHawkHttpd(context.Background(), &cfg)
	if err != nil { goto oops }

	err = server.Start()
	if err != nil {
		server.ReqStop()
		server.Wait()
		goto oops
	}

	for _, bound_address = range server.Addrs() { cfg.Logger.Printf("listening on %s", bound_address) }

	signal_ctx, signal_cancel = signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer signal_cancel()

	done = make(chan error, 1)

	go func() { done <- server.Wait() }()

	select {
		case err = <-done:
			if err != nil { goto oops }

		case <-signal_ctx.Done():
			shutdown_ctx, shutdown_cancel = context.WithTimeout(context.Background(), shutdown_timeout)
			err = server.Shutdown(shutdown_ctx)
			shutdown_cancel()
			if err != nil { goto oops }
	}
	return 0

oops:
	fmt.Fprintf(os.Stderr, "ERROR: HTTP server - %s\n", err)
	return 1
}
