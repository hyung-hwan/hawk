package workbench_test

import "bufio"
import "bytes"
import "encoding/base64"
import "encoding/json"
import "io"
import "net/http"
import "os"
import "os/exec"
import "path/filepath"
import "strconv"
import "strings"
import "testing"
import "time"

type example_t struct {
	Id int `json:"id"`
	Tool string `json:"tool"`
	Title string `json:"title"`
	Program string `json:"program"`
	Input string `json:"input"`
}

type response_t struct {
	Headers http.Header `json:"-"`
	Ok bool `json:"ok"`
	Output string `json:"output"`
	Error string `json:"error"`
	Example example_t `json:"example"`
	Examples []example_t `json:"examples"`
}

type server_t struct {
	command *exec.Cmd
	address string
	logs_done chan struct{}
	logs []string
}

func start_server(t *testing.T, binary string, root string, database string, modules string, extra ...string) *server_t {
	var server *server_t
	var stderr io.ReadCloser
	var ready chan string
	var err error

	t.Helper()
	server = &server_t{logs_done: make(chan struct{})}
	server.command = exec.Command(binary, "--httpd", "--listen", "127.0.0.1:0",
		"--root", filepath.Join(root, "public"), "--script-root", filepath.Join(root, "scripts"),
		"--route", "/app/=workbench.hawk", "--route", "/api/=workbench.hawk",
		"--modlibdirs", modules, "--max-concurrent", "1")
	server.command.Args = append(server.command.Args, extra...)
	server.command.Env = append(os.Environ(), "HAWK_WORKBENCH_DB=" + database)
	stderr, err = server.command.StderrPipe()
	if err != nil { t.Fatal(err) }
	server.command.Stdout = server.command.Stderr
	err = server.command.Start()
	if err != nil { t.Fatal(err) }
	ready = make(chan string, 1)
	go func() {
		var scanner *bufio.Scanner
		var line string

		defer close(server.logs_done)
		scanner = bufio.NewScanner(stderr)
		for scanner.Scan() {
			line = scanner.Text()
			server.logs = append(server.logs, line)
			if strings.Contains(line, "listening on ") {
				ready <- "http://" + strings.SplitN(line, "listening on ", 2)[1]
			}
		}
	}()
	t.Cleanup(func() { server.stop(t) })
	select {
		case server.address = <-ready:
		case <-server.logs_done:
			t.Fatal("server exited before opening its listener")
		case <-time.After(10 * time.Second):
			t.Fatal("server did not open its listener")
	}
	return server
}

func (server *server_t) stop(t *testing.T) {
	var done chan error
	var err error

	t.Helper()
	if server.command == nil { return }
	err = server.command.Process.Signal(os.Interrupt)
	if err != nil {
		// Platforms without process interrupts still terminate the fixture.
		server.command.Process.Kill()
		server.command.Wait()
	} else {
		done = make(chan error, 1)
		go func() { done <- server.command.Wait() }()
		select {
			case err = <-done:
				if err != nil { t.Errorf("graceful shutdown: %v", err) }
			case <-time.After(5 * time.Second):
				server.command.Process.Kill()
				<-done
				t.Error("server did not shut down gracefully")
		}
	}
	<-server.logs_done
	if err != nil { t.Log(strings.Join(server.logs, "\n")) }
	server.command = nil
}

func request(t *testing.T, server *server_t, method string, path string, payload interface{}, expected int) response_t {
	var encoded []byte
	var req *http.Request
	var res *http.Response
	var client *http.Client
	var body []byte
	var result response_t
	var err error

	t.Helper()
	if payload != nil {
		encoded, err = json.Marshal(payload)
		if err != nil { t.Fatal(err) }
	}
	req, err = http.NewRequest(method, server.address + path, bytes.NewReader(encoded))
	if err != nil { t.Fatal(err) }
	req.Header.Set("X-Workbench-Request", "1")
	if payload != nil { req.Header.Set("Content-Type", "application/json") }
	client = &http.Client{Timeout: 5 * time.Second}
	res, err = client.Do(req)
	if err != nil { t.Fatal(err) }
	defer res.Body.Close()
	body, err = io.ReadAll(res.Body)
	if err != nil { t.Fatal(err) }
	if res.StatusCode != expected { t.Fatalf("%s %s: status %d, body %s", method, path, res.StatusCode, body) }
	if strings.HasPrefix(res.Header.Get("Content-Type"), "application/json") {
		err = json.Unmarshal(body, &result)
		if err != nil { t.Fatalf("invalid JSON response: %s: %v", body, err) }
	} else { result.Output = string(body) }
	result.Headers = res.Header.Clone()
	return result
}

func check_csp_nonce(t *testing.T, result response_t, html bool) string {
	var directive string
	var sources []string
	var nonce string
	var decoded []byte
	var err error

	t.Helper()
	for _, directive = range strings.Split(result.Headers.Get("Content-Security-Policy"), ";") {
		sources = strings.Fields(directive)
		if len(sources) == 0 || sources[0] != "script-src" { continue }
		if len(sources) != 2 || !strings.HasPrefix(sources[1], "'nonce-") || !strings.HasSuffix(sources[1], "'") {
			t.Fatalf("expected a nonce script policy: %q", directive)
		}
		nonce = strings.TrimSuffix(strings.TrimPrefix(sources[1], "'nonce-"), "'")
	}
	decoded, err = base64.RawURLEncoding.DecodeString(nonce)
	if err != nil || len(decoded) != 16 { t.Fatalf("invalid CSP nonce %q: %v", nonce, err) }
	if html && !strings.Contains(result.Output, "<script nonce=\"" + nonce + "\" src=\"/workbench.js\" defer>") {
		t.Fatal("script nonce does not match the CSP header")
	}
	return nonce
}

func TestWorkbench(t *testing.T) {
	var hawk_binary string
	var go_binary string
	var modules string
	var root string
	var database string
	var setup *exec.Cmd
	var setup_output []byte
	var server *server_t
	var result response_t
	var payload example_t
	var original_id int
	var item example_t
	var found bool
	var nonce string
	var err error

	hawk_binary = os.Getenv("HAWK_BIN")
	go_binary = os.Getenv("HAWKGO_BIN")
	if hawk_binary == "" || go_binary == "" { t.Skip("set HAWK_BIN and HAWKGO_BIN to run the HTTP sample test") }
	modules = os.Getenv("HAWK_MODLIBDIRS")
	hawk_binary, err = filepath.Abs(hawk_binary)
	if err != nil { t.Fatal(err) }
	go_binary, err = filepath.Abs(go_binary)
	if err != nil { t.Fatal(err) }
	root, err = filepath.Abs("..")
	if err != nil { t.Fatal(err) }
	database = filepath.Join(t.TempDir(), "workbench.db")
	setup = exec.Command(hawk_binary, "--modlibdirs=" + modules, "-f", filepath.Join(root, "scripts", "setup.hawk"))
	setup.Env = append(os.Environ(), "HAWK_WORKBENCH_DB=" + database)
	setup_output, err = setup.CombinedOutput()
	if err != nil { t.Fatalf("setup: %s: %v", setup_output, err) }
	server = start_server(t, go_binary, root, database, modules)
	result = request(t, server, "GET", "/app/", nil, 200)
	if !strings.Contains(result.Output, "Make text work for you.") || !strings.Contains(result.Output, "workbench.js") { t.Fatal("rendered page or partials are missing") }
	nonce = check_csp_nonce(t, result, true)
	result = request(t, server, "GET", "/app/", nil, 200)
	if check_csp_nonce(t, result, true) == nonce { t.Fatal("CSP nonce was reused across responses") }
	result = request(t, server, "HEAD", "/app/", nil, 200)
	if check_csp_nonce(t, result, false) == nonce || result.Output != "" { t.Fatal("invalid HEAD response nonce or body") }
	request(t, server, "GET", "/workbench.js", nil, 200)
	request(t, server, "GET", "/workbench.css", nil, 200)
	request(t, server, "GET", "/data/workbench.db", nil, 404)
	request(t, server, "GET", "/scripts/workbench.hawk", nil, 404)
	result = request(t, server, "GET", "/api/examples", nil, 200)
	if !result.Ok || len(result.Examples) != 6 { t.Fatalf("starter examples: %+v", result) }
	payload = example_t{Tool: "sed", Program: "s/world/Hawk/g", Input: "hello world\n"}
	result = request(t, server, "POST", "/api/run", payload, 200)
	if !result.Ok || result.Output != "hello Hawk\n" { t.Fatalf("sed: %+v", result) }
	payload = example_t{Tool: "m4", Program: "define(`NAME', `Hawk')dnl", Input: "Hello NAME!"}
	result = request(t, server, "POST", "/api/run", payload, 200)
	if result.Output != "Hello Hawk!" { t.Fatalf("m4: %+v", result) }
	payload = example_t{Tool: "mus", Program: "{{name}} {{#items}}[{{.}}]{{/items}}", Input: "{\"name\":\"日本 & <Hawk>\",\"items\":[1,2]}"}
	result = request(t, server, "POST", "/api/run", payload, 200)
	if result.Output != "日本 &amp; &lt;Hawk&gt; [1][2]" { t.Fatalf("mustache: %+v", result) }
	payload.Program = "{{#broken}}"
	result = request(t, server, "POST", "/api/run", payload, 422)
	if result.Ok || result.Error == "" { t.Fatal("missing template error") }
	payload = example_t{Tool: "sed", Program: "s/[//", Input: "abc"}
	request(t, server, "POST", "/api/run", payload, 422)
	request(t, server, "POST", "/api/run", []string{}, 400)
	request(t, server, "GET", "/api/run", nil, 405)
	request(t, server, "GET", "/api/missing", nil, 404)
	payload = example_t{Tool: "sed", Title: "O'Reilly </script><script>bad()</script>", Program: "s/a/b/g", Input: "aaa\n日本\u0000tail"}
	result = request(t, server, "POST", "/api/examples", payload, 200)
	original_id = result.Example.Id
	if original_id <= 0 { t.Fatal("saved example lacks an ID") }
	result = request(t, server, "GET", "/app/", nil, 200)
	if strings.Contains(result.Output, "<script>bad()</script>") || !strings.Contains(result.Output, "&lt;script&gt;bad()&lt;/script&gt;") { t.Fatal("saved title is not escaped") }
	request(t, server, "POST", "/api/examples", payload, 409)
	payload.Id = original_id
	payload.Title = "Updated example"
	request(t, server, "POST", "/api/examples", payload, 200)
	server.stop(t)
	server = start_server(t, go_binary, root, database, modules)
	result = request(t, server, "GET", "/api/examples", nil, 200)
	for _, item = range result.Examples {
		if item.Id == original_id {
			found = item.Title == payload.Title && item.Input == payload.Input
		}
	}
	if !found { t.Fatal("saved example did not survive server restart") }
	request(t, server, "DELETE", "/api/examples?id=" + strconv.Itoa(original_id), nil, 200)
	request(t, server, "DELETE", "/api/examples?id=" + strconv.Itoa(original_id), nil, 404)
	request(t, server, "DELETE", "/api/examples?id=1%20OR%201=1", nil, 400)
	result = request(t, server, "GET", "/api/examples", nil, 200)
	if len(result.Examples) != 6 { t.Fatal("delete did not preserve the remaining examples") }
}
func TestHttpdOptions(t *testing.T) {
	var binary string
	var root string
	var server *server_t
	var result response_t
	var err error

	binary = os.Getenv("HAWKGO_BIN")
	if binary == "" { t.Skip("set HAWKGO_BIN to check HTTP CLI options") }
	binary, err = filepath.Abs(binary)
	if err != nil { t.Fatal(err) }
	root = t.TempDir()
	err = os.MkdirAll(filepath.Join(root, "scripts"), 0700)
	if err != nil { t.Fatal(err) }
	err = os.MkdirAll(filepath.Join(root, "public"), 0700)
	if err != nil { t.Fatal(err) }
	err = os.WriteFile(filepath.Join(root, "scripts", "workbench.hawk"), []byte("function alternate(request) { print \"custom entry\"; }"), 0600)
	if err != nil { t.Fatal(err) }
	server = start_server(t, binary, root, "", os.Getenv("HAWK_MODLIBDIRS"), "--entry", "alternate", "--shutdown-timeout", "2s")
	result = request(t, server, "GET", "/app/", nil, 200)
	if result.Output != "custom entry\n" { t.Fatalf("custom entry: %q", result.Output) }
	server.stop(t)
}
