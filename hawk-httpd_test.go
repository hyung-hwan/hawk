package hawk

import "context"
import "crypto/tls"
import "fmt"
import "io"
import "net"
import "net/http"
import "net/http/httptest"
import "os"
import "path/filepath"
import "reflect"
import "strings"
import "sync"
import "testing"
import "time"

func httpd_test_script(t *testing.T, source string) string {
	var path string
	var err error

	t.Helper()
	path = filepath.Join(t.TempDir(), "handler.hawk")
	err = os.WriteFile(path, []byte(source), 0600)
	if err != nil { t.Fatal(err) }
	return path
}

func httpd_test_server(t *testing.T, cfg *HawkHttpdConfig) *HawkHttpd {
	var server *HawkHttpd
	var test_cfg HawkHttpdConfig
	var err error

	t.Helper()
	test_cfg = *cfg
	if test_cfg.ModLibDirs == "" { test_cfg.ModLibDirs = os.Getenv("HAWK_TEST_MODLIBDIRS") }
	server, err = NewHawkHttpd(context.Background(), &test_cfg)
	if err != nil { t.Fatal(err) }
	err = server.Start()
	if err != nil {
		server.ReqStop()
		server.Wait()
		t.Fatal(err)
	}
	t.Cleanup(func() {
		var ctx context.Context
		var cancel context.CancelFunc
		var err error

		ctx, cancel = context.WithTimeout(context.Background(), 5 * time.Second)
		defer cancel()
		err = server.Shutdown(ctx)
		if err != nil { t.Error(err) }
	})
	return server
}

func httpd_test_get(t *testing.T, client *http.Client, address string) (int, string, http.Header) {
	var response *http.Response
	var data []byte
	var err error

	t.Helper()
	response, err = client.Get(address)
	if err != nil { t.Fatal(err) }
	defer response.Body.Close()
	data, err = io.ReadAll(response.Body)
	if err != nil { t.Fatal(err) }
	return response.StatusCode, string(data), response.Header
}

func httpd_test_client(t *testing.T) *http.Client {
	var client *http.Client

	t.Helper()
	client = &http.Client{Timeout: 5 * time.Second, Transport: &http.Transport{}}
	t.Cleanup(client.CloseIdleConnections)
	return client
}

func TestHawkHttpdInstancesAndListeners(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var first *HawkHttpd
	var second *HawkHttpd
	var addresses []net.Addr
	var address net.Addr
	var client *http.Client
	var status int
	var body string
	var headers http.Header
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `@global count;
function handle(request) {
 @local response;
 count++;
 response["status"] = 201;
 response["headers"]["X-Hawk-Test"] = "ok";
 response["headers"]["Set-Cookie"] = hawk::array("a=1", "b=2");
 response["body"] = request["path"] ":" count;
 return response;
}`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}, {Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	first = httpd_test_server(t, &cfg)
	second = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	addresses = first.Addrs()
	if len(addresses) != 2 || len(second.Addrs()) != 2 { t.Fatal("missing listeners") }
	// Changing the caller's configuration must not mutate either server.
	cfg.Routes[0].Prefix = "/changed"
	cfg.Listeners[0].Address = "invalid"
	for _, address = range addresses {
		status, body, headers = httpd_test_get(t, client, "http://" + address.String() + "/one")
		if status != 201 || body != "/one:1" || headers.Get("X-Hawk-Test") != "ok" || len(headers.Values("Set-Cookie")) != 2 {
			t.Fatalf("unexpected response: %d %q %v", status, body, headers)
		}
	}
	err = first.Shutdown(context.Background())
	if err != nil { t.Fatal(err) }
	first.ReqStop()
	if first.Wait() != nil { t.Fatal("repeated stop failed") }
	if first.Start() == nil { t.Fatal("stopped server restarted") }
	status, body, _ = httpd_test_get(t, client, "http://" + second.Addrs()[0].String() + "/two")
	if status != 201 || body != "/two:1" { t.Fatalf("second instance affected: %d %q", status, body) }
}

func TestHawkHttpdStartupRollback(t *testing.T) {
	var probe net.Listener
	var occupied net.Listener
	var server *HawkHttpd
	var cfg HawkHttpdConfig
	var address string
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	probe, err = net.Listen("tcp", "127.0.0.1:0")
	if err != nil { t.Fatal(err) }
	address = probe.Addr().String()
	probe.Close()
	occupied, err = net.Listen("tcp", "127.0.0.1:0")
	if err != nil { t.Fatal(err) }
	defer occupied.Close()
	cfg.Listeners = []HawkHttpdListener{{Address: address}, {Address: occupied.Addr().String()}}
	server, err = NewHawkHttpd(context.Background(), &cfg)
	if err != nil { t.Fatal(err) }
	defer func() { server.ReqStop(); server.Wait() }()
	if server.Start() == nil { t.Fatal("binding an occupied address succeeded") }
	if len(server.Addrs()) != 0 { t.Fatal("failed startup published addresses") }
	probe, err = net.Listen("tcp", address)
	if err != nil { t.Fatalf("first listener leaked: %s", err) }
	probe.Close()
	occupied.Close()
	err = server.Start()
	if err != nil { t.Fatalf("retry failed: %s", err) }
}

func TestHawkHttpdRequestDataAndLimits(t *testing.T) {
	var path string
	var server *HawkHttpd
	var cfg HawkHttpdConfig
	var client *http.Client
	var address string
	var status int
	var body string
	var response *http.Response
	var request *http.Request
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) {
 @local response;
 if (request["method"] == "POST") response["body"] = request["body"];
 else response["body"] = request["query"]["name"][1] ":" request["query"]["name"][2] ":" request["query"][""][1];
 return response;
}`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.MaxBodyBytes = 4
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	status, body, _ = httpd_test_get(t, client, address + "/?name=A&name=B&=empty")
	if status != 200 || body != "A:B:empty" { t.Fatalf("query map: %d %q", status, body) }
	response, err = client.Post(address, "application/octet-stream", strings.NewReader("A\x00B\xff"))
	if err != nil { t.Fatal(err) }
	{
		var data []byte
		data, err = io.ReadAll(response.Body)
		response.Body.Close()
		if err != nil || response.StatusCode != 200 || string(data) != "A\x00B\xff" { t.Fatalf("binary body: %d %q %v", response.StatusCode, data, err) }
	}
	response, err = client.Post(address, "text/plain", strings.NewReader("12345"))
	if err != nil { t.Fatal(err) }
	response.Body.Close()
	if response.StatusCode != 413 { t.Fatalf("body limit: %d", response.StatusCode) }
	status, _, _ = httpd_test_get(t, client, address + "/?bad=%zz")
	if status != 400 { t.Fatalf("malformed query: %d", status) }
	request, err = http.NewRequest(http.MethodHead, address + "/?name=A&name=B&=empty", nil)
	if err != nil { t.Fatal(err) }
	response, err = client.Do(request)
	if err != nil { t.Fatal(err) }
	{
		var data []byte
		data, err = io.ReadAll(response.Body)
		response.Body.Close()
		if err != nil || len(data) != 0 || response.StatusCode != 200 { t.Fatal("HEAD returned a body or failed") }
	}
}

func TestHawkHttpdReloadAndConcurrency(t *testing.T) {
	var path string
	var dependency string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var address string
	var status int
	var body string
	var initial *httpd_script_version
	var replaced *httpd_script_version
	var wg sync.WaitGroup
	var i int
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) { @local response; response["body"] = "old"; return response; }`)
	dependency = filepath.Join(filepath.Dir(path), "dependency.txt")
	err = os.WriteFile(dependency, []byte("one"), 0600)
	if err != nil { t.Fatal(err) }
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path, Dependencies: []string{dependency}}}
	cfg.MaxConcurrent = 4
	cfg.ReloadInterval = time.Nanosecond
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	status, body, _ = httpd_test_get(t, client, address)
	if status != 200 || body != "old" { t.Fatal("initial script failed") }
	initial = server.scripts[path].current
	for i = 0; i < 16; i++ {
		wg.Add(1)
		go func() {
			var response *http.Response
			var err error
			defer wg.Done()
			response, err = client.Get(address)
			if err != nil { t.Error(err); return }
			io.Copy(io.Discard, response.Body)
			response.Body.Close()
			if response.StatusCode != 200 { t.Errorf("concurrent request: %d", response.StatusCode) }
		}()
	}
	wg.Wait()
	if server.scripts[path].current != initial { t.Fatal("unchanged script recompiled") }
	err = os.WriteFile(path, []byte(`function handle(request) { @local response; response["body"] = "new"; return response; }`), 0600)
	if err != nil { t.Fatal(err) }
	_, body, _ = httpd_test_get(t, client, address)
	if body != "new" || server.scripts[path].current == initial { t.Fatal("script change not published") }
	replaced = server.scripts[path].current
	err = os.WriteFile(dependency, []byte("two"), 0600)
	if err != nil { t.Fatal(err) }
	_, body, _ = httpd_test_get(t, client, address)
	if body != "new" || server.scripts[path].current == replaced { t.Fatal("dependency change not detected") }
	err = os.WriteFile(path, []byte("invalid script {"), 0600)
	if err != nil { t.Fatal(err) }
	status, body, _ = httpd_test_get(t, client, address)
	if status != 200 || body != "new" { t.Fatal("failed reload discarded working program") }
}

func TestHawkHttpdShutdownDrainsRuntime(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var program *Hawk
	var version *httpd_script_version
	var entered chan struct{}
	var release chan struct{}
	var response_done chan error
	var ctx context.Context
	var cancel context.CancelFunc
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) { @local response; hold(); response["body"] = "finished"; return response; }`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.ReloadInterval = time.Hour
	server = httpd_test_server(t, &cfg)
	entered = make(chan struct{})
	release = make(chan struct{})
	defer close(release)
	program, err = New()
	if err != nil { t.Fatal(err) }
	err = program.AddFunc("hold", 0, 0, "", func(rtx *Rtx) error {
		close(entered)
		<-release
		return rtx.SetFuncRetWithInt(0)
	})
	if err == nil { err = program.ParseFile(path) }
	if err != nil { program.Close(); t.Fatal(err) }
	version = &httpd_script_version{hawk: program}
	server.scripts[path].current = version
	server.scripts[path].checked = time.Now()
	response_done = make(chan error, 1)
	go func() {
		var response *http.Response
		var client *http.Client = &http.Client{Timeout: 5 * time.Second}
		var err error

		response, err = client.Get("http://" + server.Addrs()[0].String())
		if err == nil { response.Body.Close() }
		response_done <- err
	}()
	select {
	case <-entered:
	case <-time.After(5 * time.Second):
		t.Fatal("request never entered interpreter")
	}
	ctx, cancel = context.WithTimeout(context.Background(), 20 * time.Millisecond)
	err = server.Shutdown(ctx)
	cancel()
	if err != context.DeadlineExceeded { t.Fatalf("shutdown did not respect deadline: %v", err) }
	if program.c == nil { t.Fatal("active interpreter was freed") }
	select {
	case <-server.done_chan:
		t.Fatal("Wait completed while interpreter was active")
	default:
	}
	release <- struct{}{}
	err = <-response_done
	if err != nil { t.Fatal(err) }
	if server.Wait() != nil || program.c != nil { t.Fatal("interpreter cleanup incomplete") }
}

func TestHawkHttpdStaticTLSAndResponseErrors(t *testing.T) {
	var root string
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var tls_source *httptest.Server
	var client *http.Client
	var status int
	var body string
	var address string
	var err error
	var source string
	var invalid []string

	if !http_module_enabled() { t.Skip("http module is disabled") }
	root = t.TempDir()
	err = os.WriteFile(filepath.Join(root, "hello.txt"), []byte("static"), 0600)
	if err != nil { t.Fatal(err) }
	err = os.WriteFile(filepath.Join(root, "secret.hawk"), []byte("source"), 0600)
	if err != nil { t.Fatal(err) }
	path = httpd_test_script(t, `function handle(request) { @local response; response["body"] = "script"; return response; }`)
	tls_source = httptest.NewTLSServer(http.NotFoundHandler())
	defer tls_source.Close()
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0", TLSConfig: tls_source.TLS.Clone()}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/api/", Script: path}}
	cfg.DocumentRoot = root
	cfg.ReloadInterval = time.Nanosecond
	cfg.MaxResponseBytes = 16
	server = httpd_test_server(t, &cfg)
	client = &http.Client{Timeout: 5 * time.Second, Transport: &http.Transport{TLSClientConfig: &tls.Config{InsecureSkipVerify: true}}}
	defer client.CloseIdleConnections()
	address = "https://" + server.Addrs()[0].String()
	status, body, _ = httpd_test_get(t, client, address + "/hello.txt")
	if status != 200 || body != "static" { t.Fatal("TLS static response failed") }
	status, _, _ = httpd_test_get(t, client, address + "/secret.hawk")
	if status != 404 { t.Fatal("script source served as a static file") }
	status, body, _ = httpd_test_get(t, client, address + "/api/hello.txt")
	if status != 200 || body != "script" { t.Fatal("script route did not override static handler") }
	invalid = []string{
		`response["status"] = 0; return response;`,
		`response["status"] = 4294967496; return response;`,
		`response["headers"]["Bad Name"] = "value"; return response;`,
		`response["headers"]["X-Test"] = "a\r\nb"; return response;`,
		`response["body"] = "01234567890123456"; return response;`,
	}
	for _, source = range invalid {
		err = os.WriteFile(path, []byte(fmt.Sprintf("function handle(request) { @local response; %s }", source)), 0600)
		if err != nil { t.Fatal(err) }
		status, _, _ = httpd_test_get(t, client, address + "/api/")
		if status != 500 { t.Fatalf("invalid response accepted: %s (%d)", source, status) }
	}
}

func TestHawkHttpdParentCancellationAndStopBeforeStart(t *testing.T) {
	var ctx context.Context
	var cancel context.CancelFunc
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var done chan error
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	server, err = NewHawkHttpd(context.Background(), &cfg)
	if err != nil { t.Fatal(err) }
	server.ReqStop()
	server.ReqStop()
	if server.Wait() != nil || server.Start() == nil { t.Fatal("stop before start failed") }
	ctx, cancel = context.WithCancel(context.Background())
	defer cancel()
	server, err = NewHawkHttpd(ctx, &cfg)
	if err != nil { t.Fatal(err) }
	err = server.Start()
	if err != nil { server.ReqStop(); server.Wait(); t.Fatal(err) }
	done = make(chan error, 1)
	go func() { done <- server.Wait() }()
	cancel()
	select {
	case err = <-done:
		if err != nil { t.Fatal(err) }
	case <-time.After(5 * time.Second):
		t.Fatal("parent cancellation did not stop the server")
	}
}

func TestHawkHttpdReloadPreservesActiveVersion(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var program *Hawk
	var version *httpd_script_version
	var entered chan struct{}
	var release chan struct{}
	var response_done chan error
	var client *http.Client
	var address string
	var body string
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) { @local response; hold(); response["body"] = "old"; return response; }`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.ReloadInterval = time.Hour
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	entered = make(chan struct{})
	release = make(chan struct{})
	defer close(release)
	program, err = New()
	if err != nil { t.Fatal(err) }
	err = program.AddFunc("hold", 0, 0, "", func(rtx *Rtx) error {
		close(entered)
		<-release
		return rtx.SetFuncRetWithInt(0)
	})
	if err == nil { err = program.ParseFile(path) }
	if err != nil { program.Close(); t.Fatal(err) }
	version = &httpd_script_version{hawk: program}
	server.scripts[path].current = version
	server.scripts[path].checked = time.Now()
	address = "http://" + server.Addrs()[0].String()
	response_done = make(chan error, 1)
	go func() {
		var response *http.Response
		var data []byte
		var err error

		response, err = client.Get(address)
		if err == nil {
			data, err = io.ReadAll(response.Body)
			response.Body.Close()
			if err == nil && string(data) != "old" { err = fmt.Errorf("active request changed script: %q", data) }
		}
		response_done <- err
	}()
	select {
	case <-entered:
	case <-time.After(5 * time.Second):
		t.Fatal("request never entered old script")
	}
	err = os.WriteFile(path, []byte(`function handle(request) { @local response; response["body"] = "new"; return response; }`), 0600)
	if err != nil { t.Fatal(err) }
	server.scripts[path].mtx.Lock()
	server.scripts[path].checked = time.Time{}
	server.scripts[path].mtx.Unlock()
	_, body, _ = httpd_test_get(t, client, address)
	if body != "new" || program.c == nil { t.Fatal("reload failed or freed an active version") }
	release <- struct{}{}
	err = <-response_done
	if err != nil { t.Fatal(err) }
	version.mtx.Lock()
	if program.c != nil { t.Error("retired version not reclaimed") }
	version.mtx.Unlock()
}

func TestHawkHttpdEmptyValueConversions(t *testing.T) {
	var program *Hawk
	var rtx *Rtx
	var value *Val
	var container *Val
	var field *Val
	var data []byte
	var text string
	var err error

	program, err = New()
	if err != nil { t.Fatal(err) }
	defer program.Close()
	err = program.ParseText("BEGIN {}")
	if err != nil { t.Fatal(err) }
	rtx, err = program.NewRtx("empty-http-values", nil, nil)
	if err != nil { t.Fatal(err) }
	defer rtx.Close()
	value, err = rtx.NewByteArrVal(nil)
	if err != nil { t.Fatal(err) }
	defer value.Close()
	data, err = value.ToByteArr()
	if err != nil || len(data) != 0 { t.Fatalf("empty bytes: %q %v", data, err) }
	container, err = rtx.NewMapVal()
	if err != nil { t.Fatal(err) }
	defer container.Close()
	err = container.SetMapField("", value)
	if err != nil { t.Fatal(err) }
	field, err = container.GetMapField("")
	if err != nil { t.Fatal(err) }
	defer field.Close()
	text, err = field.ToStr()
	if err != nil || text != "" { t.Fatalf("empty-key value: %q %v", text, err) }
}

func TestHawkHttpdConsoleIO(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var response *http.Response
	var data []byte
	var address string
	var i int
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `@global count;
BEGIN { print "BEGIN must not run"; }
END { print "END must not run"; }
function serve(request) {
 @local line;
 count++;
 RS = "|"; ORS = "/";
 printf "%s:%d:", request["path"], count;
 while ((getline line)>0) print toupper(line);
 return 123;
}`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path, Entry: "serve"}}
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String() + "/console"
	for i = 0; i < 2; i++ {
		response, err = client.Post(address, "text/plain; charset=utf-8", strings.NewReader("hello|한글|world"))
		if err != nil { t.Fatal(err) }
		data, err = io.ReadAll(response.Body)
		response.Body.Close()
		if err != nil || response.StatusCode != 200 || string(data) != "/console:1:HELLO/한글/WORLD/" {
			t.Fatalf("console response: %d %q %v", response.StatusCode, data, err)
		}
		if response.Header.Get("Content-Type") != "text/plain; charset=utf-8" { t.Fatal("console content type not detected") }
	}
}

func TestHawkHttpdConsoleBinaryAndMetadata(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var response *http.Response
	var request *http.Request
	var data []byte
	var address string
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) {
 @local response, line;
 while ((getbline line)>0) print line;
 response["status"] = 201;
 response["headers"]["Content-Type"] = "application/octet-stream";
 response["body"] = @b"tail";
 return response;
}`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	response, err = client.Post(address, "application/octet-stream", strings.NewReader("A\x00\xff\n"))
	if err != nil { t.Fatal(err) }
	data, err = io.ReadAll(response.Body)
	response.Body.Close()
	if err != nil || response.StatusCode != 201 || string(data) != "A\x00\xff\ntail" || response.Header.Get("Content-Type") != "application/octet-stream" {
		t.Fatalf("binary console response: %d %q %v", response.StatusCode, data, err)
	}
	request, err = http.NewRequest(http.MethodHead, address, nil)
	if err != nil { t.Fatal(err) }
	response, err = client.Do(request)
	if err != nil { t.Fatal(err) }
	data, err = io.ReadAll(response.Body)
	response.Body.Close()
	if err != nil || response.StatusCode != 201 || len(data) != 0 { t.Fatal("HEAD returned console/response-map body bytes") }
}

func TestHawkHttpdConsoleErrorsDiscardOutput(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var address string
	var source string
	var cases []string
	var status int
	var body string
	var response *http.Response
	var data []byte
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) { printf "ok"; }`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.MaxResponseBytes = 4
	cfg.MaxBodyBytes = 4
	cfg.ReloadInterval = time.Nanosecond
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	status, body, _ = httpd_test_get(t, client, address)
	if status != 200 || body != "ok" { t.Fatal("void return was not accepted") }
	cases = []string{
		`printf "12345";`,
		`printf "12"; printf "345";`,
		`printf "12"; response["body"] = "345"; return response;`,
		`printf "12"; response["status"] = 0; return response;`,
		`printf "12"; zero=0; zero=1/zero;`,
	}
	for _, source = range cases {
		err = os.WriteFile(path, []byte("function handle(request) { @local response, zero; " + source + " }"), 0600)
		if err != nil { t.Fatal(err) }
		status, body, _ = httpd_test_get(t, client, address)
		if status != 500 || body != "script execution failed\n" { t.Fatalf("failed script committed output: %d %q (%s)", status, body, source) }
	}
	// Invalid text conversion can be caught by getline. ConsoleError must still
	// prevent the handler from turning that failed read into a success response.
	err = os.WriteFile(path, []byte(`function handle(request) { @local line; printf "12"; getline line; }`), 0600)
	if err != nil { t.Fatal(err) }
	response, err = client.Post(address, "text/plain", strings.NewReader("\xe2\x82"))
	if err != nil { t.Fatal(err) }
	data, err = io.ReadAll(response.Body)
	response.Body.Close()
	if err != nil || response.StatusCode != 500 || string(data) != "script execution failed\n" { t.Fatalf("console read failure hidden: %d %q %v", response.StatusCode, data, err) }
	response, err = client.Post(address, "text/plain", strings.NewReader("12345"))
	if err != nil { t.Fatal(err) }
	response.Body.Close()
	if response.StatusCode != 413 { t.Fatal("console request body exceeded its limit") }
}

func TestHawkHttpdConsoleConcurrency(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var address string
	var wg sync.WaitGroup
	var i int

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) { @local line; getline line; printf "%s:%s", request["path"], line; }`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.MaxConcurrent = 4
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	for i = 0; i < 16; i++ {
		wg.Add(1)
		go func(index int) {
			var response *http.Response
			var data []byte
			var body string
			var path string
			var err error

			defer wg.Done()
			body = fmt.Sprintf("body-%d", index)
			path = fmt.Sprintf("/request-%d", index)
			response, err = client.Post(address + path, "text/plain", strings.NewReader(body))
			if err != nil { t.Error(err); return }
			data, err = io.ReadAll(response.Body)
			response.Body.Close()
			if err != nil || response.StatusCode != 200 || string(data) != path + ":" + body { t.Errorf("request %d received another request's data: %d %q %v", index, response.StatusCode, data, err) }
		}(i)
	}
	wg.Wait()
}

func TestHawkHttpdHttpModule(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var request *http.Request
	var response *http.Response
	var data []byte
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request) {
 @local headers, response;
 headers = http::getheaders();
 if (http::getmethod() != "POST" || http::getpath() != "/metadata" ||
     http::getheader("x-test", 2) != "second" || headers["X-Test"][1] != "first" ||
     http::getheader("X-Missing") != "" || http::getheader("X-Test", 3) != "" ||
     http::getheader("Host") != request["host"] || http::getstatus() != 200) exit 1;
 http::setstatus(202);
 http::setheader("Content-Type", "text/plain; charset=utf-8");
 http::setheader("X-Replaced", "old");
 http::setheader("X-Replaced", "new");
 http::addheader("Set-Cookie", "a=1");
 http::addheader("Set-Cookie", "b=2");
 http::setheader("X-Deleted", "unused");
 http::delheader("x-deleted");
 printf "%s:%d:", http::getpath(), http::getstatus();
 response["body"] = "tail";
 return response;
}`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	request, err = http.NewRequest("POST", "http://" + server.Addrs()[0].String() + "/metadata", nil)
	if err != nil { t.Fatal(err) }
	request.Header.Add("X-Test", "first")
	request.Header.Add("X-Test", "second")
	response, err = client.Do(request)
	if err != nil { t.Fatal(err) }
	defer response.Body.Close()
	data, err = io.ReadAll(response.Body)
	if err != nil { t.Fatal(err) }
	if response.StatusCode != 202 || string(data) != "/metadata:202:tail" || response.Header.Get("X-Replaced") != "new" || response.Header.Get("X-Deleted") != "" || len(response.Header.Values("Set-Cookie")) != 2 {
		t.Fatalf("unexpected HTTP module response: %d %q %v", response.StatusCode, data, response.Header)
	}
}

func TestHawkHttpdHttpModuleErrors(t *testing.T) {
	var scripts []string
	var source string
	var i int

	if !http_module_enabled() { t.Skip("http module is disabled") }
	scripts = []string{
		`http::setheader("Bad:Name", "value");`,
		`http::setheader("X-Test", "left\0right");`,
		`http::addheader("X-Test", "left\r\nright");`,
		`http::setstatus(600);`,
		`http::setstatus(200.5);`,
		`http::getheader("X-Test", 0);`,
	}
	for i, source = range scripts {
		t.Run(fmt.Sprintf("invalid-%d", i), func(t *testing.T) {
			var cfg HawkHttpdConfig
			var server *HawkHttpd
			var status int
			var body string

			cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
			cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: httpd_test_script(t, "function handle(request){ printf \"discarded\"; " + source + " }")}}
			server = httpd_test_server(t, &cfg)
			status, body, _ = httpd_test_get(t, httpd_test_client(t), "http://" + server.Addrs()[0].String())
			if status != 500 || strings.Contains(body, "discarded") { t.Fatalf("invalid module argument leaked output: %d %q", status, body) }
		})
	}
}

func TestHawkHttpdHttpModuleCommittedErrors(t *testing.T) {
	var scripts []string
	var source string
	var i int

	if !http_module_enabled() { t.Skip("http module is disabled") }
	scripts = []string{
		`http::setstatus(201);`,
		`http::setheader("X-Test", "value");`,
		`http::addheader("X-Test", "value");`,
		`http::delheader("Content-Type");`,
		`response["status"]=201; return response;`,
		`response["headers"]["X-Test"]="value"; return response;`,
		`printf "too-large";`,
	}
	for i, source = range scripts {
		t.Run(fmt.Sprintf("committed-%d", i), func(t *testing.T) {
			var cfg HawkHttpdConfig
			var server *HawkHttpd
			var status int
			var body string

			cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
			cfg.MaxResponseBytes = 7
			cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: httpd_test_script(t, "function handle(request){ @local response; http::setstatus(202); printf \"prefix\"; http::flush(); " + source + " }")}}
			server = httpd_test_server(t, &cfg)
			status, body, _ = httpd_test_get(t, httpd_test_client(t), "http://" + server.Addrs()[0].String())
			if status != 202 || body != "prefix" { t.Fatalf("committed response was replaced: %d %q", status, body) }
		})
	}
}

func TestHawkHttpdHttpModuleStreaming(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var program *Hawk
	var version *httpd_script_version
	var entered chan struct{}
	var release chan struct{}
	var prefix_done chan error
	var response *http.Response
	var data []byte
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request){ @local response; http::setstatus(202); printf "prefix"; http::flush(); hold(); printf ":after"; response["body"] = ":tail"; return response; }`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.ReloadInterval = time.Hour
	server = httpd_test_server(t, &cfg)
	entered = make(chan struct{})
	release = make(chan struct{})
	// Register before injecting this generation into the server's cache.
	program, err = New()
	if err != nil { t.Fatal(err) }
	err = program.SetModLibDirs(server.cfg.ModLibDirs)
	if err != nil { program.Close(); t.Fatal(err) }
	err = program.AddFunc("hold", 0, 0, "", func(rtx *Rtx) error {
		close(entered)
		<-release
		return rtx.SetFuncRetWithInt(0)
	})
	if err == nil { err = program.ParseFile(path) }
	if err != nil { program.Close(); t.Fatal(err) }
	version = &httpd_script_version{hawk: program}
	server.scripts[path].current = version
	server.scripts[path].checked = time.Now()
	prefix_done = make(chan error, 1)
	go func() {
		var prefix []byte = make([]byte, 6)
		var client *http.Client = httpd_test_client(t)
		var read_error error

		response, read_error = client.Get("http://" + server.Addrs()[0].String())
		if read_error == nil {
			_, read_error = io.ReadFull(response.Body, prefix)
			if read_error == nil && (string(prefix) != "prefix" || response.StatusCode != 202) { read_error = fmt.Errorf("unexpected flushed prefix %q", prefix) }
		}
		prefix_done <- read_error
	}()
	select {
	case err = <-prefix_done:
	case <-time.After(5 * time.Second):
		err = fmt.Errorf("flush did not reach the client before execution completed")
	}
	// Always release the native callback, including a failed streaming assertion.
	close(release)
	if err != nil { t.Fatal(err) }
	defer response.Body.Close()
	<-entered
	data, err = io.ReadAll(response.Body)
	if err != nil || string(data) != ":after:tail" { t.Fatalf("stream tail: %q %v", data, err) }
}

func TestHawkHttpdHttpModuleConcurrency(t *testing.T) {
	var path string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var address string
	var wg sync.WaitGroup
	var i int

	if !http_module_enabled() { t.Skip("http module is disabled") }
	path = httpd_test_script(t, `function handle(request){ http::setheader("X-Request",http::getheader("X-Request")); http::setstatus(201); printf "%s",http::getpath(); http::flush(); printf ":%s",http::getheader("X-Request"); }`)
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: path}}
	cfg.MaxConcurrent = 4
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	for i = 0; i < 16; i++ {
		wg.Add(1)
		go func(index int) {
			var request *http.Request
			var response *http.Response
			var data []byte
			var key string = fmt.Sprintf("request-%d", index)
			var err error

			defer wg.Done()
			request, err = http.NewRequest("GET", address + "/" + key, nil)
			if err == nil {
				request.Header.Set("X-Request", key)
				response, err = client.Do(request)
			}
			if err != nil { t.Error(err); return }
			data, err = io.ReadAll(response.Body)
			response.Body.Close()
			if err != nil || response.StatusCode != 201 || string(data) != "/" + key + ":" + key || response.Header.Get("X-Request") != key { t.Errorf("request context mixed: %d %q %v", response.StatusCode, data, err) }
		}(i)
	}
	wg.Wait()
}

type httpd_test_flush_error_writer struct {
	*httptest.ResponseRecorder
}

func (writer *httpd_test_flush_error_writer) FlushError() error {
	return fmt.Errorf("injected HTTP flush failure")
}

func TestHawkHttpdHttpModuleFlushFailure(t *testing.T) {
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var recorder *httptest.ResponseRecorder
	var writer *httpd_test_flush_error_writer

	if !http_module_enabled() { t.Skip("http module is disabled") }
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: httpd_test_script(t, `function handle(request){ http::setstatus(202); printf "prefix"; http::flush(); printf "discarded"; }`)}}
	server = httpd_test_server(t, &cfg)
	recorder = httptest.NewRecorder()
	writer = &httpd_test_flush_error_writer{ResponseRecorder: recorder}
	server.ServeHTTP(writer, httptest.NewRequest("GET", "http://localhost/", nil))
	if recorder.Code != 202 || recorder.Body.String() != "prefix" { t.Fatalf("flush failure replaced committed response: %d %q", recorder.Code, recorder.Body.String()) }
}

type httpd_test_body_case struct {
	method string
	status int
}

func TestHawkHttpdHttpModuleSuppressedBodies(t *testing.T) {
	var cases []httpd_test_body_case
	var item httpd_test_body_case

	if !http_module_enabled() { t.Skip("http module is disabled") }
	cases = []httpd_test_body_case{{"HEAD",200}, {"GET",204}, {"GET",304}}
	for _, item = range cases {
		t.Run(fmt.Sprintf("%s-%d", item.method, item.status), func(t *testing.T) {
			var cfg HawkHttpdConfig
			var server *HawkHttpd
			var request *http.Request
			var response *http.Response
			var data []byte
			var err error

			cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
			cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: httpd_test_script(t, fmt.Sprintf(`function handle(request){ http::setstatus(%d); printf "before"; http::flush(); printf "after"; }`, item.status))}}
			server = httpd_test_server(t, &cfg)
			request, err = http.NewRequest(item.method, "http://" + server.Addrs()[0].String(), nil)
			if err != nil { t.Fatal(err) }
			response, err = httpd_test_client(t).Do(request)
			if err != nil { t.Fatal(err) }
			defer response.Body.Close()
			data, err = io.ReadAll(response.Body)
			if err != nil || response.StatusCode != item.status || len(data) != 0 { t.Fatalf("suppressed response body: %d %q %v", response.StatusCode, data, err) }
		})
	}
}

func TestHawkHttpdAvailability(t *testing.T) {
	var server *HawkHttpd
	var cfg HawkHttpdConfig
	var err error

	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	server, err = NewHawkHttpd(context.Background(), &cfg)
	if !http_module_enabled() {
		if server != nil || err == nil || !strings.Contains(err.Error(), "http module is disabled") {
			t.Fatalf("disabled constructor returned server=%v error=%v", server, err)
		}
		return
	}
	if err != nil { t.Fatal(err) }
	server.ReqStop()
	if err = server.Wait(); err != nil { t.Fatal(err) }
}

func TestHawkHttpdScriptRoot(t *testing.T) {
	var script_root string
	var document_root string
	var script string
	var dependency string
	var cwd string
	var relative_root string
	var relative_script string
	var relative_dependency string
	var cases []struct {
		name string
		root string
		script string
		dependency string
		resolved_root string
	}
	var item struct {
		name string
		root string
		script string
		dependency string
		resolved_root string
	}
	var err error

	if !http_module_enabled() { t.Skip("http module is disabled") }
	script_root = filepath.Join(t.TempDir(), "scripts")
	document_root = t.TempDir()
	err = os.MkdirAll(filepath.Join(script_root, "handlers"), 0700)
	if err != nil { t.Fatal(err) }
	script = filepath.Join(script_root, "handlers", "main.hawk")
	dependency = filepath.Join(script_root, "settings.hawk")
	err = os.WriteFile(script, []byte(`function handle(request) { printf "script"; }`), 0600)
	if err != nil { t.Fatal(err) }
	err = os.WriteFile(filepath.Join(document_root, "static.txt"), []byte("static"), 0600)
	if err != nil { t.Fatal(err) }
	cwd, err = os.Getwd()
	if err != nil { t.Fatal(err) }
	relative_root, err = filepath.Rel(cwd, script_root)
	if err != nil { t.Fatal(err) }
	relative_script, err = filepath.Rel(cwd, script)
	if err != nil { t.Fatal(err) }
	relative_dependency, err = filepath.Rel(cwd, dependency)
	if err != nil { t.Fatal(err) }
	cases = []struct {
		name string
		root string
		script string
		dependency string
		resolved_root string
	}{
		{"blank root", "", relative_script, relative_dependency, ""},
		{"absolute root", script_root, filepath.Join("handlers", "main.hawk"), "settings.hawk", script_root},
		{"relative root", relative_root, filepath.Join("handlers", "main.hawk"), "settings.hawk", script_root},
		{"absolute paths", document_root, script, dependency, document_root},
	}
	for _, item = range cases {
		t.Run(item.name, func(t *testing.T) {
			var cfg HawkHttpdConfig
			var server *HawkHttpd
			var entry *httpd_script
			var version *httpd_script_version
			var client *http.Client
			var address string
			var body string
			var status int
			var err error

			err = os.WriteFile(dependency, []byte("before"), 0600)
			if err != nil { t.Fatal(err) }
			cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
			cfg.DocumentRoot = document_root
			cfg.ScriptRoot = item.root
			cfg.Routes = []HawkHttpdRoute{{Prefix: "/api/", Script: item.script, Dependencies: []string{item.dependency}}}
			cfg.ReloadInterval = time.Nanosecond
			server = httpd_test_server(t, &cfg)
			if server.cfg.ScriptRoot != item.resolved_root || server.cfg.Routes[0].Script != script || server.cfg.Routes[0].Dependencies[0] != dependency {
				t.Fatalf("unexpected resolved paths: root=%q route=%v", server.cfg.ScriptRoot, server.cfg.Routes[0])
			}
			if cfg.ScriptRoot != item.root || cfg.Routes[0].Script != item.script || cfg.Routes[0].Dependencies[0] != item.dependency {
				t.Fatal("server creation mutated the caller's paths")
			}
			client = httpd_test_client(t)
			address = "http://" + server.Addrs()[0].String()
			status, body, _ = httpd_test_get(t, client, address + "/api/test")
			if status != 200 || body != "script" { t.Fatalf("script response: %d %q", status, body) }
			status, body, _ = httpd_test_get(t, client, address + "/static.txt")
			if status != 200 || body != "static" { t.Fatalf("static response: %d %q", status, body) }
			entry = server.scripts[script]
			if entry == nil || len(entry.dependencies) != 1 || entry.dependencies[0] != dependency { t.Fatal("dependency registered under the wrong path") }
			version = entry.current
			err = os.WriteFile(dependency, []byte("after"), 0600)
			if err != nil { t.Fatal(err) }
			status, body, _ = httpd_test_get(t, client, address + "/api/test")
			if status != 200 || body != "script" || entry.current == version { t.Fatal("resolved dependency did not trigger script reload") }
		})
	}
}

func TestHawkHttpdAutomaticIncludes(t *testing.T) {
	var root string
	var main string
	var common string
	var nested string
	var replacement string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var entry *httpd_script
	var version *httpd_script_version
	var address string
	var status int
	var body string
	var err error
	var write func(string, string)
	var get func(string)

	if !http_module_enabled() { t.Skip("http module is disabled") }
	root = t.TempDir()
	main = filepath.Join(root, "main.hawk")
	common = filepath.Join(root, "common.inc")
	nested = filepath.Join(root, "nested.inc")
	replacement = filepath.Join(root, "replacement.inc")
	write = func(path string, source string) {
		var err error
		t.Helper()
		err = os.WriteFile(path, []byte(source), 0600)
		if err != nil { t.Fatal(err) }
	}
	write(main, "@include_once \"common.inc\";\n@include_once \"common.inc\";\nfunction handle(request) { printf message(); }")
	write(common, "@include \"nested.inc\";\n")
	write(nested, "function message() { return \"one\"; }")
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: main}}
	cfg.ReloadInterval = time.Nanosecond
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	get = func(expected string) {
		t.Helper()
		status, body, _ = httpd_test_get(t, client, address)
		if status != 200 || body != expected { t.Fatalf("response: %d %q, want %q", status, body, expected) }
	}
	get("one")
	entry = server.scripts[main]
	if !reflect.DeepEqual(entry.current.dependencies, []string{main, common, nested}) { t.Fatalf("discovered sources: %v", entry.current.dependencies) }
	version = entry.current
	get("one")
	if entry.current != version { t.Fatal("unchanged includes recompiled") }
	write(nested, "function message() { return \"two\"; }")
	get("two")
	if entry.current == version { t.Fatal("nested include did not reload") }
	version = entry.current
	write(nested, "invalid included source {")
	get("two")
	if entry.current != version || !reflect.DeepEqual(entry.current.dependencies, version.dependencies) { t.Fatal("failed reload replaced program or dependencies") }
	write(nested, "function message() { return \"recovered\"; }")
	get("recovered")
	// A deleted old include must not block a program that stopped using it.
	write(common, "function message() { return \"removed\"; }")
	err = os.Remove(nested)
	if err != nil { t.Fatal(err) }
	get("removed")
	if !reflect.DeepEqual(entry.current.dependencies, []string{main, common}) { t.Fatalf("stale sources: %v", entry.current.dependencies) }
	version = entry.current
	write(nested, "invalid unused source {")
	get("removed")
	if entry.current != version { t.Fatal("unused old include still triggers recompilation") }
	write(replacement, "function message() { return \"replacement\"; }")
	write(common, "@include \"replacement.inc\";\n")
	get("replacement")
	write(replacement, "function message() { return \"updated\"; }")
	get("updated")
}

func TestHawkHttpdIncludeSearchChanges(t *testing.T) {
	var root string
	var main string
	var first string
	var second string
	var local string
	var cfg HawkHttpdConfig
	var server *HawkHttpd
	var client *http.Client
	var address string
	var status int
	var body string
	var err error
	var write func(string, string)
	var get func(string)

	if !http_module_enabled() { t.Skip("http module is disabled") }
	root = t.TempDir()
	main = filepath.Join(root, "main.hawk")
	first = filepath.Join(root, "first")
	second = filepath.Join(root, "second")
	local = filepath.Join(root, "helpers", "value.inc")
	write = func(path string, value string) {
		var err error
		t.Helper()
		err = os.MkdirAll(filepath.Dir(path), 0700)
		if err != nil { t.Fatal(err) }
		err = os.WriteFile(path, []byte("function message() { return \"" + value + "\"; }"), 0600)
		if err != nil { t.Fatal(err) }
	}
	err = os.WriteFile(main, []byte("@include \"helpers/value.inc\";\nfunction handle(request) { printf message(); }"), 0600)
	if err != nil { t.Fatal(err) }
	write(filepath.Join(second, "helpers", "value.inc"), "second")
	cfg.Listeners = []HawkHttpdListener{{Address: "127.0.0.1:0"}}
	cfg.Routes = []HawkHttpdRoute{{Prefix: "/", Script: main}}
	cfg.IncDirs = first + ":" + second
	cfg.ReloadInterval = time.Nanosecond
	server = httpd_test_server(t, &cfg)
	client = httpd_test_client(t)
	address = "http://" + server.Addrs()[0].String()
	get = func(expected string) {
		t.Helper()
		status, body, _ = httpd_test_get(t, client, address)
		if status != 200 || body != expected { t.Fatalf("response: %d %q, want %q", status, body, expected) }
	}
	get("second")
	write(filepath.Join(first, "helpers", "value.inc"), "first")
	get("first")
	write(local, "local")
	get("local")
	err = os.Remove(local)
	if err != nil { t.Fatal(err) }
	get("first")
}
