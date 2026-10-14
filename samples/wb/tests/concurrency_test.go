package workbench_test

import "bytes"
import "encoding/json"
import "fmt"
import "io"
import "net/http"
import "os"
import "os/exec"
import "path/filepath"
import "strings"
import "testing"
import "time"

func TestWorkbenchConcurrency(t *testing.T) {
	var hawk_binary string
	var go_binary string
	var modules string
	var root string
	var database string
	var setup *exec.Cmd
	var output []byte
	var server *server_t
	var client *http.Client
	var transport *http.Transport
	var done chan error
	var start chan struct{}
	var worker int
	var err error

	hawk_binary = os.Getenv("HAWK_BIN")
	go_binary = os.Getenv("HAWKGO_BIN")
	if hawk_binary == "" || go_binary == "" { t.Skip("set HAWK_BIN and HAWKGO_BIN to run the concurrent HTTP sample test") }
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
	output, err = setup.CombinedOutput()
	if err != nil { t.Fatalf("setup: %s: %v", output, err) }
	server = start_server(t, go_binary, root, database, modules, "--max-concurrent", "10")
	transport = &http.Transport{MaxIdleConns: 50, MaxIdleConnsPerHost: 50}
	defer transport.CloseIdleConnections()
	client = &http.Client{Transport: transport, Timeout: 30 * time.Second}
	done = make(chan error, 50)
	start = make(chan struct{})
	for worker = 0; worker < 50; worker++ {
		go func(worker int) {
			var i int
			var method string
			var path string
			var payload []byte
			var req *http.Request
			var res *http.Response
			var body []byte
			var result response_t
			var err error

			<-start
			for i = 0; i < 40; i++ {
				method = "GET"
				path = "/app/"
				payload = nil
				switch (worker + i) % 3 {
					case 1: path = "/api/examples"
					case 2:
						method = "POST"
						path = "/api/run"
						payload = []byte(`{"tool":"sed","program":"s/world/Hawk/g","input":"hello world\n"}`)
				}
				req, err = http.NewRequest(method, server.address + path, bytes.NewReader(payload))
				if err != nil { done <- err; return }
				req.Header.Set("X-Workbench-Request", "1")
				if payload != nil { req.Header.Set("Content-Type", "application/json") }
				res, err = client.Do(req)
				if err != nil { done <- err; return }
				body, err = io.ReadAll(res.Body)
				res.Body.Close()
				if err != nil { done <- err; return }
				if res.StatusCode != http.StatusOK {
					done <- fmt.Errorf("%s: status %d: %s", path, res.StatusCode, body)
					return
				}
				if path == "/app/" {
					if !strings.Contains(string(body), "Make text work for you.") {
						done <- fmt.Errorf("missing rendered page: %s", body)
						return
					}
				} else {
					result = response_t{}
					err = json.Unmarshal(body, &result)
					if err != nil || !result.Ok || (path == "/api/examples" && len(result.Examples) != 6) || (path == "/api/run" && result.Output != "hello Hawk\n") {
						done <- fmt.Errorf("%s: unexpected response %s: %v", path, body, err)
						return
					}
				}
			}
			done <- nil
		}(worker)
	}
	close(start)
	for worker = 0; worker < 50; worker++ {
		err = <-done
		if err != nil { t.Error(err) }
	}
}
