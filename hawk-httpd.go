package hawk

import "context"
import "crypto/sha256"
import "crypto/tls"
import "fmt"
import "hash"
import "io"
import "log"
import "net"
import "net/http"
import "os"
import "path/filepath"
import "strings"
import "sync"
import "time"

// HawkHttpdListener describes one listening endpoint. A nil TLSConfig means HTTP.
type HawkHttpdListener struct {
	Network string
	Address string
	TLSConfig *tls.Config
}

// HawkHttpdRoute invokes Entry (default: handle) in Script for a URL prefix.
// Includes are tracked automatically. Dependencies names additional files,
// such as templates or runtime configuration, that should trigger reparsing.
// Relative script and dependency paths are resolved against ScriptRoot, or
// the working directory when ScriptRoot is empty.
type HawkHttpdRoute struct {
	Prefix string
	Script string
	Entry string
	Dependencies []string
}

// HawkHttpdConfig is copied by NewHawkHttpd. Zero limits select the defaults.
// Durations are time.Duration values. Logger may be shared explicitly by callers;
// a nil Logger creates an instance-local logger that discards messages.
type HawkHttpdConfig struct {
	Listeners []HawkHttpdListener
	Routes []HawkHttpdRoute
	DocumentRoot string
	ScriptRoot string // Base directory for relative script and dependency paths.
	IncDirs string
	ModLibDirs string
	MaxConcurrent int
	MaxBodyBytes int64
	MaxResponseBytes int64
	ReadHeaderTimeout time.Duration
	ReadTimeout time.Duration
	WriteTimeout time.Duration
	IdleTimeout time.Duration
	ReloadInterval time.Duration
	Logger *log.Logger
}

type httpd_listener struct {
	listener net.Listener
	server *http.Server
}

type httpd_script_version struct {
	hawk *Hawk
	fingerprint [sha256.Size]byte
	dependencies []string // resolved source paths from this successful parse
	include_probes []string // candidate paths whose existence affects resolution
	refs int
	retired bool
	mtx sync.Mutex // protects runtime construction/destruction and version lifetime
}

type httpd_script struct {
	path string
	dependencies []string
	mtx sync.Mutex
	current *httpd_script_version
	checked time.Time
}

type httpd_state int
const (
	httpd_state_new httpd_state = iota
	httpd_state_running
	httpd_state_stopping
	httpd_state_stopped
)

// HawkHttpd owns its listeners, routes, compiled programs, and active requests.
// It must not be copied. Each instance has one start/stop lifecycle.
// No mutable HTTP server state is shared between instances.
type HawkHttpd struct {
	cfg HawkHttpdConfig
	ctx context.Context
	cancel context.CancelFunc
	mtx sync.Mutex
	state httpd_state
	terminal_error error
	listeners []httpd_listener
	addresses []net.Addr
	scripts map[string]*httpd_script
	static_handler http.Handler
	slots chan struct{}
	stop_chan chan struct{}
	done_chan chan struct{}
	serve_wg sync.WaitGroup
	request_wg sync.WaitGroup
	watch_wg sync.WaitGroup
}

func NewHawkHttpd(ctx context.Context, cfg *HawkHttpdConfig) (*HawkHttpd, error) {
	var s *HawkHttpd
	var i int
	var j int
	var route *HawkHttpdRoute
	var entry *httpd_script
	var err error

	if !http_module_enabled() {
		return nil, fmt.Errorf("HTTP server is unavailable: the http module is disabled in this build")
	}

	if ctx == nil || cfg == nil {
		return nil, fmt.Errorf("context and HTTP server configuration are required")
	}

	if len(cfg.Listeners) == 0 {
		return nil, fmt.Errorf("at least one listening address is required")
	}

	s = &HawkHttpd{cfg: *cfg}
	s.cfg.Listeners = append([]HawkHttpdListener(nil), cfg.Listeners...)
	s.cfg.Routes = append([]HawkHttpdRoute(nil), cfg.Routes...)

	if s.cfg.ScriptRoot != "" {
		s.cfg.ScriptRoot, err = filepath.Abs(s.cfg.ScriptRoot)
		if err != nil { return nil, err }
	}

	for i = range s.cfg.Listeners {
		if s.cfg.Listeners[i].Network == "" {
			s.cfg.Listeners[i].Network = "tcp"
		}

		if s.cfg.Listeners[i].Address == "" {
			return nil, fmt.Errorf("listener %d has no address", i)
		}

		if s.cfg.Listeners[i].TLSConfig != nil {
			s.cfg.Listeners[i].TLSConfig = s.cfg.Listeners[i].TLSConfig.Clone()
			if len(s.cfg.Listeners[i].TLSConfig.Certificates) == 0 &&
			   s.cfg.Listeners[i].TLSConfig.GetCertificate == nil &&
			   s.cfg.Listeners[i].TLSConfig.GetConfigForClient == nil {
				return nil, fmt.Errorf("TLS listener %d has no certificate provider", i)
			}
		}
	}

	if s.cfg.MaxConcurrent < 0 || s.cfg.MaxBodyBytes < 0 ||
	   s.cfg.MaxResponseBytes < 0 || s.cfg.ReadHeaderTimeout < 0 ||
	   s.cfg.ReadTimeout < 0 || s.cfg.WriteTimeout < 0 ||
	   s.cfg.IdleTimeout < 0 || s.cfg.ReloadInterval < 0 {
		return nil, fmt.Errorf("HTTP server limits and durations must not be negative")
	}

	if s.cfg.MaxConcurrent == 0 { s.cfg.MaxConcurrent = 64 }
	if s.cfg.MaxBodyBytes == 0 { s.cfg.MaxBodyBytes = 1024 * 1024 }
	if s.cfg.MaxResponseBytes == 0 { s.cfg.MaxResponseBytes = 8 * 1024 * 1024 }
	if s.cfg.ReadHeaderTimeout == 0 { s.cfg.ReadHeaderTimeout = 10 * time.Second }
	if s.cfg.ReadTimeout == 0 { s.cfg.ReadTimeout = 30 * time.Second }
	if s.cfg.WriteTimeout == 0 { s.cfg.WriteTimeout = 30 * time.Second }
	if s.cfg.IdleTimeout == 0 { s.cfg.IdleTimeout = 60 * time.Second }
	if s.cfg.ReloadInterval == 0 { s.cfg.ReloadInterval = time.Second }
	if s.cfg.Logger == nil { s.cfg.Logger = log.New(io.Discard, "", 0) }

	s.scripts = make(map[string]*httpd_script)

	for i = range s.cfg.Routes {
		route = &s.cfg.Routes[i]

		if !strings.HasPrefix(route.Prefix, "/") || route.Script == "" {
			return nil, fmt.Errorf("route %d requires an absolute URL prefix and a script", i)
		}

		for j = 0; j < i; j++ {
			if route.Prefix == s.cfg.Routes[j].Prefix {
				return nil, fmt.Errorf("duplicate route prefix %q", route.Prefix)
			}
		}

		if route.Entry == "" { route.Entry = "handle" }

		if strings.IndexByte(route.Entry, 0) >= 0 {
			return nil, fmt.Errorf("route entry contains NUL")
		}

		if s.cfg.ScriptRoot != "" && !filepath.IsAbs(route.Script) {
			route.Script = filepath.Join(s.cfg.ScriptRoot, route.Script)
		}
		route.Script, err = filepath.Abs(route.Script)
		if err != nil { return nil, err }

		route.Dependencies = append([]string(nil), route.Dependencies...)

		// register the each unique script path in the server's scripts registry
		entry = s.scripts[route.Script]
		if entry == nil {
			entry = &httpd_script{path: route.Script}
			s.scripts[route.Script] = entry
		}

		// if the embedder provides a route configuration entry like this:
		// 
		// cfg.Routes = []HawkHttpdRoute{
		//    {
		//       Prefix: "/api/",
		//       Script: "/srv/app/handler.hawk",
		//       Dependencies: []string{
		//          "/srv/app/common.hawk",
		//          "/srv/app/settings.hawk",
		//       },
		//    },
		// }
		// 
		// it traverse the dependencies and polulate them to the actual script entry
		for j = range route.Dependencies {
			if s.cfg.ScriptRoot != "" && !filepath.IsAbs(route.Dependencies[j]) {
				route.Dependencies[j] = filepath.Join(s.cfg.ScriptRoot, route.Dependencies[j])
			}
			route.Dependencies[j], err = filepath.Abs(route.Dependencies[j])
			if err != nil { return nil, err }
			entry.dependencies = append(entry.dependencies, route.Dependencies[j])
		}
	}

	if s.cfg.DocumentRoot != "" {
		s.cfg.DocumentRoot, err = filepath.Abs(s.cfg.DocumentRoot)
		if err != nil { return nil, err }
		s.static_handler = http.FileServer(http.Dir(s.cfg.DocumentRoot))
	}

	s.ctx, s.cancel = context.WithCancel(ctx)
	s.slots = make(chan struct{}, s.cfg.MaxConcurrent)
	s.stop_chan = make(chan struct{})
	s.done_chan = make(chan struct{})

	return s, nil
}

// Start binds every endpoint before starting any serving task. Binding failures
// close endpoints already opened; the caller may correct the cause and retry.
func (s *HawkHttpd) Start() error {
	var bound []httpd_listener
	var spec HawkHttpdListener
	var lc net.ListenConfig
	var listener net.Listener
	var server *http.Server
	var err error
	var i int

	s.mtx.Lock()
	defer s.mtx.Unlock()

	if s.state != httpd_state_new { return fmt.Errorf("HTTP server has already been started or stopped") }
	if s.ctx.Err() != nil { return s.ctx.Err() }

	for _, spec = range s.cfg.Listeners {
		listener, err = lc.Listen(s.ctx, spec.Network, spec.Address)
		if err != nil {
			for i = range bound { bound[i].listener.Close() }
			return err
		}

		server = &http.Server{
			Handler: s,
			TLSConfig: spec.TLSConfig,
			ReadHeaderTimeout: s.cfg.ReadHeaderTimeout,
			ReadTimeout: s.cfg.ReadTimeout,
			WriteTimeout: s.cfg.WriteTimeout,
			IdleTimeout: s.cfg.IdleTimeout,
			ErrorLog: s.cfg.Logger,
			BaseContext: func(_ net.Listener) context.Context { return s.ctx },
		}
		bound = append(bound, httpd_listener{listener: listener, server: server})
	}

	s.listeners = bound
	s.addresses = make([]net.Addr, len(bound))
	s.state = httpd_state_running

	for i = range bound {
		s.addresses[i] = bound[i].listener.Addr()
		s.serve_wg.Add(1)
		go s.serve_listener(bound[i])
	}

	s.watch_wg.Add(1)
	go s.watch_context()
	return nil
}

func (s *HawkHttpd) serve_listener(endpoint httpd_listener) {
	var err error

	defer s.serve_wg.Done()
	if endpoint.server.TLSConfig == nil {
		err = endpoint.server.Serve(endpoint.listener)
	} else {
		err = endpoint.server.ServeTLS(endpoint.listener, "", "")
	}
	if err != nil && err != http.ErrServerClosed {
		s.set_terminal_error(err)
		s.ReqStop()
	}
}

func (s *HawkHttpd) watch_context() {
	defer s.watch_wg.Done()
	select {
		case <-s.ctx.Done():
			s.ReqStop()
		case <-s.stop_chan:
	}
}

func (s *HawkHttpd) set_terminal_error(err error) {
	s.mtx.Lock()
	if s.terminal_error == nil { s.terminal_error = err }
	s.mtx.Unlock()
}

// ReqStop requests graceful draining. It is nonblocking and idempotent.
func (s *HawkHttpd) ReqStop() {
	s.mtx.Lock()
	if s.state >= httpd_state_stopping {
		s.mtx.Unlock()
		return
	}
	s.state = httpd_state_stopping
	close(s.stop_chan)
	s.mtx.Unlock()
	go s.finish_shutdown()
}

func (s *HawkHttpd) finish_shutdown() {
	var wg sync.WaitGroup
	var endpoint httpd_listener
	var entry *httpd_script

	// each listener stops concurrently. this context is deliberately independent
	// of the request context, which must remain usable while requests drain.
	for _, endpoint = range s.listeners {
		wg.Add(1)
		go func(server *http.Server) {
			var err error
			defer wg.Done()
			err = server.Shutdown(context.Background())
			if err != nil { s.set_terminal_error(err) }
		}(endpoint.server)
	}

	wg.Wait()
	s.serve_wg.Wait()
	s.request_wg.Wait()
	s.watch_wg.Wait()

	for _, entry = range s.scripts {
		entry.mtx.Lock()
		if entry.current != nil {
			entry.current.retire()
			entry.current = nil
		}
		entry.mtx.Unlock()
	}
	s.cancel()

	s.mtx.Lock()
	s.state = httpd_state_stopped
	close(s.done_chan)
	s.mtx.Unlock()
}

// shutdown requests draining and waits up to ctx's deadline. A timeout does not
// free running interpreters; Wait still waits for actual termination and cleanup.
func (s *HawkHttpd) Shutdown(ctx context.Context) error {
	if ctx == nil { return fmt.Errorf("shutdown context is required") }

	s.ReqStop()
	select {
		case <-s.done_chan:
			return s.Wait()
		case <-ctx.Done():
			return ctx.Err()
	}
}

// Wait waits for listener tasks, requests, and program cleanup to finish.
func (s *HawkHttpd) Wait() error {
	<-s.done_chan
	s.mtx.Lock()
	defer s.mtx.Unlock()
	return s.terminal_error
}

// Addrs returns a copy of the endpoint list established by Start.
func (s *HawkHttpd) Addrs() []net.Addr {
	s.mtx.Lock()
	defer s.mtx.Unlock()
	return append([]net.Addr(nil), s.addresses...)
}

func (s *HawkHttpd) log_error(err error) {
	if s.cfg.Logger != nil { s.cfg.Logger.Printf("hawk httpd: %s", err) }
}

func (entry *httpd_script) fingerprint() ([sha256.Size]byte, map[string][sha256.Size]byte, error) {
	var dependencies []string
	var probes []string

	if entry.current != nil {
		dependencies = entry.current.dependencies
		probes = entry.current.include_probes
	}
	return entry.fingerprint_with(dependencies, probes)
}

func (entry *httpd_script) fingerprint_with(dependencies []string, probes []string) ([sha256.Size]byte, map[string][sha256.Size]byte, error) {
	var paths []string
	var path string
	var data []byte
	var content [sha256.Size]byte
	var digest [sha256.Size]byte
	var err error
	var h hash.Hash
	var seen map[string]bool
	var contents map[string][sha256.Size]byte

	h = sha256.New()
	paths = append([]string{entry.path}, entry.dependencies...)
	paths = append(paths, dependencies...)
	seen = make(map[string]bool)
	contents = make(map[string][sha256.Size]byte)
	for _, path = range paths {
		if seen[path] { continue }
		seen[path] = true
		data, err = os.ReadFile(path)
		if err != nil { return digest, contents, err }
		content = sha256.Sum256(data)
		contents[path] = content
		fmt.Fprintf(h, "%d:%s:", len(path), path)
		h.Write(content[:])
	}
	for _, path = range probes {
		fmt.Fprintf(h, "probe:%d:%s:%t:", len(path), path, source_probe(path))
	}
	copy(digest[:], h.Sum(nil))
	return digest, contents, nil
}

func (s *HawkHttpd) acquire_script(entry *httpd_script) (*httpd_script_version, error) {
	var fingerprint [sha256.Size]byte
	var before map[string][sha256.Size]byte
	var after map[string][sha256.Size]byte
	var path string
	var content [sha256.Size]byte
	var updated [sha256.Size]byte
	var exists bool
	var compiled [sha256.Size]byte
	var dependencies *parse_dependencies
	var h *Hawk
	var version *httpd_script_version
	var err error

	entry.mtx.Lock()
	defer entry.mtx.Unlock()

	if entry.current == nil || time.Since(entry.checked) >= s.cfg.ReloadInterval {
		entry.checked = time.Now()

		fingerprint, before, err = entry.fingerprint()
		if err != nil || entry.current == nil || entry.current.fingerprint != fingerprint {
			// a vanished old include must not prevent reparsing a main file
			// that no longer includes it. explicit dependencies remain required.
			h, err = New()

			if err == nil && s.cfg.IncDirs != "" {
				err = h.SetIncDirs(s.cfg.IncDirs)
			}

			if err == nil && s.cfg.ModLibDirs != "" {
				err = h.SetModLibDirs(s.cfg.ModLibDirs)
			}

			if err == nil {
				dependencies, err = h.parse_file_with_dependencies(entry.path, nil)
			}

			if err == nil {
				compiled, after, err = entry.fingerprint_with(dependencies.files, dependencies.probes)
				// Reuse the reload check's hashes instead of reading known files
				// again before parsing. Removed includes no longer matter.
				if err == nil {
					for path, content = range before {
						updated, exists = after[path]
						if exists && content != updated {
							err = fmt.Errorf("script files changed while parsing %s", path)
							break
						}
					}
				}
				if err == nil { err = dependencies.check() }
			}

			if err == nil {
				version = &httpd_script_version{hawk: h, fingerprint: compiled,
					dependencies: dependencies.files, include_probes: dependencies.probes}
				if entry.current != nil {
					entry.current.retire()
				}
				entry.current = version
			} else if h != nil {
				h.Close()
			}
		}
		if err != nil {
			if entry.current == nil { return nil, err }
			s.log_error(fmt.Errorf("retaining previous script %s: %w", entry.path, err))
		}
	}

	version = entry.current
	version.mtx.Lock()
	version.refs++
	version.mtx.Unlock()
	return version, nil
}

func (version *httpd_script_version) retire() {
	version.mtx.Lock()

	version.retired = true
	if version.refs == 0 {
		version.hawk.Close()
	}

	version.mtx.Unlock()
}

func (version *httpd_script_version) release() {
	version.mtx.Lock()

	version.refs--
	if version.retired && version.refs == 0 {
		version.hawk.Close()
	}

	version.mtx.Unlock()
}

func (version *httpd_script_version) new_rtx() (*Rtx, error) {
	version.mtx.Lock()
	defer version.mtx.Unlock()
	return version.hawk.NewRtx("httpd", nil, nil)
}

func (version *httpd_script_version) close_rtx(rtx *Rtx) {
	version.mtx.Lock()
	rtx.Close()
	version.mtx.Unlock()
}
