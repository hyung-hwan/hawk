package hawk

import "bytes"
import "crypto/rand"
import "encoding/base64"
import "fmt"
import "io"
import "net"
import "net/http"
import "net/url"
import "path/filepath"
import "strings"

// Request state belongs to one runtime. Output is buffered until completion
// or an explicit http::flush(); subsequent writes go directly to the response.
type httpd_request_context struct {
	server *HawkHttpd
	request *http.Request
	writer http.ResponseWriter
	rtx *Rtx
	buffer bytes.Buffer
	limit int64
	written int64
	status int
	headers http.Header
	committed bool
	error error
}

func (ctx *httpd_request_context) Write(data []byte) (int, error) {
	var n int
	var err error

	if ctx.error != nil { return 0, ctx.error }
	if int64(len(data)) > ctx.limit - ctx.written {
		ctx.error = fmt.Errorf("response body is too large")
		return 0, ctx.error
	}
	ctx.written += int64(len(data))
	if !ctx.committed { return ctx.buffer.Write(data) }
	if !ctx.send_body() { return len(data), nil }
	n, err = ctx.writer.Write(data)
	if err == nil && n != len(data) { err = io.ErrShortWrite }
	if err != nil { ctx.error = err }
	return n, err
}

func (ctx *httpd_request_context) send_body() bool {
	return ctx.request.Method != http.MethodHead && ctx.status != http.StatusNoContent && ctx.status != http.StatusNotModified
}

func (ctx *httpd_request_context) commit() error {
	var key string
	var values []string
	var data []byte
	var n int
	var err error

	if ctx.error != nil { return ctx.error }
	if ctx.committed { return nil }
	data = ctx.buffer.Bytes()
	if ctx.headers.Get("Content-Type") == "" && len(data) > 0 { ctx.headers.Set("Content-Type", http.DetectContentType(data)) }
	for key, values = range ctx.headers { ctx.writer.Header()[key] = values }
	ctx.committed = true
	ctx.writer.WriteHeader(ctx.status)
	if ctx.send_body() && len(data) > 0 {
		n, err = ctx.writer.Write(data)
		if err == nil && n != len(data) { err = io.ErrShortWrite }
	}
	ctx.buffer.Reset()
	if err != nil { ctx.error = err }
	return err
}

func (ctx *httpd_request_context) flush() error {
	var ok bool
	var err error

	_, ok = ctx.writer.(http.Flusher)
	if !ok { return fmt.Errorf("HTTP response writer does not support flushing") }
	err = ctx.commit()
	if err != nil { return err }
	// ResponseController reports errors from writers implementing FlushError.
	err = http.NewResponseController(ctx.writer).Flush()
	if err != nil { ctx.error = err }
	return err
}

// ServeHTTP selects the longest configured prefix. Console input reads the
// request body, and console output becomes the response body. A returned map
// may supply status/headers and append a body after the console output.
func (s *HawkHttpd) ServeHTTP(w http.ResponseWriter, request *http.Request) {
	var route *HawkHttpdRoute
	var i int
	var data []byte
	var query url.Values
	var version *httpd_script_version
	var rtx *Rtx
	var args *Val
	var result *Val
	var body []byte
	var output httpd_request_context
	var err error
	var max_error *http.MaxBytesError
	var ok bool

	s.mtx.Lock()
	if s.state != httpd_state_running {
		s.mtx.Unlock()
		http.Error(w, "server is stopping", http.StatusServiceUnavailable)
		return
	}

	s.request_wg.Add(1)
	s.mtx.Unlock()
	defer s.request_wg.Done()

	for i = range s.cfg.Routes {
		if strings.HasPrefix(request.URL.Path, s.cfg.Routes[i].Prefix) &&
		   (route == nil || len(s.cfg.Routes[i].Prefix) > len(route.Prefix)) {
			route = &s.cfg.Routes[i]
		}
	}

	if route == nil {
		if s.static_handler != nil && !strings.EqualFold(filepath.Ext(request.URL.Path), ".hawk") {
			s.static_handler.ServeHTTP(w, request)
		} else {
			http.NotFound(w, request)
		}
		return
	}

	select {
		case s.slots <- struct{}{}:
			defer func() { <-s.slots }()
		case <-request.Context().Done():
			return
		case <-s.stop_chan:
			http.Error(w, "server is stopping", http.StatusServiceUnavailable)
			return
	}

	query, err = url.ParseQuery(request.URL.RawQuery)
	if err != nil {
		http.Error(w, "invalid query string", http.StatusBadRequest)
		return
	}

	request.Body = http.MaxBytesReader(w, request.Body, s.cfg.MaxBodyBytes)
	data, err = io.ReadAll(request.Body)
	if err != nil {
		max_error, ok = err.(*http.MaxBytesError)
		if ok && max_error != nil {
			http.Error(w, "request body is too large", http.StatusRequestEntityTooLarge)
		} else {
			http.Error(w, "unable to read request body", http.StatusBadRequest)
		}
		return
	}
	if request.Context().Err() != nil { return }

	version, err = s.acquire_script(s.scripts[route.Script])
	if err != nil { goto oops }
	defer version.release()

	rtx, err = version.new_rtx()
	if err != nil { goto oops }
	defer version.close_rtx(rtx)

	output.limit = s.cfg.MaxResponseBytes
	output.status = http.StatusOK
	output.headers = make(http.Header)
	output.server = s
	output.request = request
	output.writer = w
	output.rtx = rtx
	err = rtx.set_http_context(&output)
	if err != nil { goto oops }

	err = rtx.SetConsoleIO(bytes.NewReader(data), &output)
	if err != nil { goto oops }

	args, err = s.make_request_value(rtx, request, query, data)
	if err != nil { goto oops }
	defer args.Close()

	// invoke the actual script function
	result, err = rtx.Call(route.Entry, args)
	if err != nil { goto oops }
	defer result.Close()

	err = rtx.ConsoleError()
	if err != nil { goto oops }

	if request.Context().Err() != nil { return }
	if result.Type() == VAL_MAP {
		output.status, output.headers, body, err = s.decode_response(result, &output)
		if err != nil { goto oops }
		_, err = output.Write(body)
		if err != nil { goto oops }
	}

	err = output.commit()
	if err != nil { goto oops }
	return

oops:
	s.log_error(fmt.Errorf("script %s: %w", route.Script, err))
	if !output.committed { http.Error(w, "script execution failed", http.StatusInternalServerError) }
}

func (s *HawkHttpd) make_request_value(rtx *Rtx, request *http.Request, query url.Values, body []byte) (*Val, error) {
	var nonce [16]byte
	var value *Val
	var field *Val
	var scheme string
	var fields map[string]string
	var key string
	var text string
	var local_addr net.Addr
	var ok bool
	var err error

	// A response nonce must be unpredictable and independent of client input.
	_, err = rand.Read(nonce[:])
	if err != nil { return nil, fmt.Errorf("unable to generate CSP nonce: %w", err) }

	value, err = rtx.NewMapVal()
	if err != nil { return nil, err }

	scheme = "http"
	if request.TLS != nil { scheme = "https" }

	fields = map[string]string{
		"method": request.Method,
		"path": request.URL.Path,
		"raw_query": request.URL.RawQuery,
		"host": request.Host,
		"remote_address": request.RemoteAddr,
		"scheme": scheme,
		"csp_nonce": base64.RawURLEncoding.EncodeToString(nonce[:]),
	}

	local_addr, ok = request.Context().Value(http.LocalAddrContextKey).(net.Addr)
	if ok {
		fields["local_address"] = local_addr.String()
	}

	for key, text = range fields {
		err = value.SetMapFieldWithStr(key, text)
		if err != nil { goto oops }
	}

	field, err = rtx.NewByteArrVal(body)
	if err != nil { goto oops }

	err = value.SetMapField("body", field)
	field.Close()
	if err != nil { goto oops }

	field, err = s.make_string_lists(rtx, map[string][]string(request.Header))
	if err != nil { goto oops }

	err = value.SetMapField("headers", field)
	field.Close()
	if err != nil { goto oops }

	field, err = s.make_string_lists(rtx, map[string][]string(query))
	if err != nil { goto oops }

	err = value.SetMapField("query", field)
	field.Close()
	if err != nil { goto oops }

	return value, nil

oops:
	value.Close()
	return nil, err
}

func (s *HawkHttpd) make_string_lists(rtx *Rtx, fields map[string][]string) (*Val, error) {
	var value *Val
	var array *Val
	var key string
	var texts []string
	var text string
	var i int
	var err error

	value, err = rtx.NewMapVal()
	if err != nil { return nil, err }

	for key, texts = range fields {
		array, err = rtx.NewArrVal(len(texts))
		if err != nil { goto oops }

		for i, text = range texts {
			err = array.SetArrayFieldWithStr(i + 1, text)
			if err != nil { break }
		}
		if err == nil { err = value.SetMapField(key, array) }

		array.Close()
		if err != nil { goto oops }
	}

	return value, nil

oops:
	value.Close()
	return nil, err
}

func (s *HawkHttpd) decode_response(result *Val, ctx *httpd_request_context) (int, http.Header, []byte, error) {
	var status int = ctx.status
	var status_value float64
	var headers http.Header = ctx.headers.Clone()
	var body []byte
	var itr ValMapItr
	var key string
	var field *Val
	var err error

	if result.Type() != VAL_MAP {
		return 0, nil, nil, fmt.Errorf("HTTP entry function must return a response map")
	}

	key, field = result.GetFirstMapField(&itr)
	for field != nil {
		switch key {
			case "status":
				if ctx.committed {
					err = fmt.Errorf("HTTP response headers are already committed")
				} else if field.Type() != VAL_INT {
					err = fmt.Errorf("response status must be an integer")
				} else {
					// Validate before narrowing Hawk's integer to Go's int on 32-bit hosts.
					status_value, err = field.ToFlt()
					if err == nil {
						if status_value < 200 || status_value > 599 {
							err = fmt.Errorf("response status is outside 200..599")
						} else {
							status = int(status_value)
						}
					}
				}

			case "headers":
				if ctx.committed {
					err = fmt.Errorf("HTTP response headers are already committed")
				} else {
					err = s.decode_headers(field, headers)
				}

			case "body":
				switch field.Type() {
					case VAL_NIL:
					case VAL_STR, VAL_MBS, VAL_CHAR, VAL_BCHR:
						body, err = field.ToByteArr()
					default:
						err = fmt.Errorf("response body must be a string or byte string")
				}
		}
		field.Close()
		if err != nil { return 0, nil, nil, err }

		key, field = result.GetNextMapField(&itr)
	}

	if int64(len(body)) > s.cfg.MaxResponseBytes { return 0, nil, nil, fmt.Errorf("response body is too large") }
	return status, headers, body, nil
}

func (s *HawkHttpd) decode_headers(value *Val, headers http.Header) error {
	var itr ValMapItr
	var array_itr ValArrayItr
	var key string
	var field *Val
	var item *Val
	var err error

	if value.Type() != VAL_MAP {
		return fmt.Errorf("response headers must be a map")
	}

	key, field = value.GetFirstMapField(&itr)
	for field != nil {
		if !httpd_header_name_valid(key) {
			err = fmt.Errorf("invalid response header name %q", key)
		} else if field.Type() == VAL_ARR {
			_, item = field.GetFirstArrayField(&array_itr)
			for item != nil {
				err = httpd_add_header(headers, key, item)
				item.Close()
				if err != nil { break }
				_, item = field.GetNextArrayField(&array_itr)
			}
		} else {
			err = httpd_add_header(headers, key, field)
		}
		field.Close()
		if err != nil { return err }

		key, field = value.GetNextMapField(&itr)
	}
	return nil
}

func httpd_header_name_valid(name string) bool {
	var i int

	if len(name) == 0 { return false }
	for i = 0; i < len(name); i++ {
		if name[i] <= 32 || name[i] >= 127 || strings.ContainsRune("()<>@,;:\\\"/[]?={}", rune(name[i])) { return false }
	}
	return true
}

func httpd_add_header(headers http.Header, key string, value *Val) error {
	var text string
	var i int
	var err error

	if value.Type() != VAL_STR && value.Type() != VAL_MBS {
		return fmt.Errorf("response header values must be strings")
	}

	text, err = value.ToStr()
	if err != nil { return err }

	for i = 0; i < len(text); i++ {
		if (text[i] < 32 && text[i] != '\t') || text[i] == 127 {
			return fmt.Errorf("invalid response header value for %q", key)
		}
	}
	headers.Add(key, text)
	return nil
}
