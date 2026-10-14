package hawk_test

import "hawk"
import "bufio"
import "bytes"
import "errors"
import "fmt"
import "io"
import "os"
import "path/filepath"
import "runtime"
import "strings"
import "sync"
import "testing"

type console_reader struct {
	data []byte
	position int
	chunk int
	tail_error error
	closed bool
	force_gc bool
}

func (r *console_reader) Read(buffer []byte) (int, error) {
	var n int

	if r.force_gc { runtime.GC(); r.force_gc = false }
	if r.position == len(r.data) {
		if r.tail_error != nil { return 0, r.tail_error }
		return 0, io.EOF
	}
	n = len(r.data) - r.position
	if r.chunk > 0 && n > r.chunk { n = r.chunk }
	if n > len(buffer) { n = len(buffer) }
	copy(buffer, r.data[r.position:r.position+n])
	r.position += n
	if r.position == len(r.data) {
		if r.tail_error != nil { return n, r.tail_error }
		return n, io.EOF
	}
	return n, nil
}

func (r *console_reader) Close() error {
	r.closed = true
	return nil
}

type console_writer struct {
	buffer bytes.Buffer
	failure error
	short bool
	calls int
	flushes int
	closed bool
}

func (w *console_writer) Write(buffer []byte) (int, error) {
	w.calls++
	if w.failure != nil { return 0, w.failure }
	if w.short { return len(buffer) - 1, nil }
	return w.buffer.Write(buffer)
}

func (w *console_writer) Flush() error {
	w.flushes++
	return w.failure
}

func (w *console_writer) Close() error {
	w.closed = true
	return nil
}

func console_test_runtime(t *testing.T, source string) (*hawk.Hawk, *hawk.Rtx) {
	var h *hawk.Hawk
	var rtx *hawk.Rtx
	var err error

	t.Helper()
	h, err = hawk.New()
	if err == nil { err = h.ParseText(source) }
	if err == nil { rtx, err = h.NewRtx("console-test", nil, nil) }
	if err != nil {
		if h != nil { h.Close() }
		t.Fatal(err)
	}
	t.Cleanup(func() { rtx.Close(); h.Close() })
	return h, rtx
}

func console_test_exec(t *testing.T, rtx *hawk.Rtx) {
	var value *hawk.Val
	var err error

	t.Helper()
	value, err = rtx.Exec(nil)
	if err != nil { t.Fatal(err) }
	value.Close()
}

func TestConsoleIOExecUnicode(t *testing.T) {
	var rtx *hawk.Rtx
	var reader *console_reader
	var output console_writer
	var err error

	_, rtx = console_test_runtime(t, `BEGIN { printf "héllo:"; } { print NR, FNR, FILENAME, $0; } END { printf "done"; }`)
	reader = &console_reader{data: []byte("한글\n世界\n"), chunk: 1, force_gc: true}
	err = rtx.SetConsoleIO(reader, &output)
	if err != nil { t.Fatal(err) }
	console_test_exec(t, rtx)
	if output.buffer.String() != "héllo:1 1 - 한글\n2 2 - 世界\ndone" { t.Fatalf("console text: %q", output.buffer.String()) }
	if rtx.ConsoleError() != nil { t.Fatal(rtx.ConsoleError()) }
	rtx.Close()
	if reader.closed || output.closed || output.flushes != 0 { t.Fatal("runtime took ownership of caller's streams") }
}

func TestConsoleIOCallRecordsAndBytes(t *testing.T) {
	var rtx *hawk.Rtx
	var output bytes.Buffer
	var value *hawk.Val
	var err error

	_, rtx = console_test_runtime(t, `function serve() { @local line; RS="|"; ORS="/"; while ((getline line)>0) printf "[%s]", line; print @b"A\0\xFF"; }`)
	err = rtx.SetConsoleIO(strings.NewReader("one|two|三"), &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Call("serve")
	if err != nil { t.Fatal(err) }
	value.Close()
	if output.String() != "[one][two][三]A\x00\xff/" { t.Fatalf("record/binary output: %q", output.String()) }
	if rtx.SetConsoleIO(nil, nil) == nil { t.Fatal("console I/O changed after execution") }
	rtx.Close()
	if rtx.SetConsoleIO(nil, nil) == nil { t.Fatal("closed runtime accepted console I/O") }
	_, rtx = console_test_runtime(t, `function serve() { @local line; while ((getbline line)>0) print line; }`)
	output.Reset()
	err = rtx.SetConsoleIO(bytes.NewReader([]byte("A\x00\xff\n")), &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Call("serve")
	if err != nil { t.Fatal(err) }
	value.Close()
	if output.String() != "A\x00\xff\n" || rtx.ConsoleError() != nil { t.Fatalf("binary records: %q %v", output.String(), rtx.ConsoleError()) }
}

func TestConsoleIOExplicitFlushAndHostBuffer(t *testing.T) {
	var rtx *hawk.Rtx
	var output console_writer
	var pending bytes.Buffer
	var writer *bufio.Writer
	var err error

	_, rtx = console_test_runtime(t, `BEGIN { printf "first"; fflush(); printf "last"; }`)
	err = rtx.SetConsoleIO(nil, &output, output.Flush)
	if err != nil { t.Fatal(err) }
	console_test_exec(t, rtx)
	if output.buffer.String() != "firstlast" || output.flushes <= 0 { t.Fatalf("explicit flush: %q %d", output.buffer.String(), output.flushes) }
	{
		var flushes int
		flushes = output.flushes
		rtx.Close()
		if output.flushes != flushes { t.Fatal("cleanup flushed host output") }
	}
	_, rtx = console_test_runtime(t, `BEGIN { printf "pending"; }`)
	writer = bufio.NewWriter(&pending)
	err = rtx.SetConsoleIO(nil, writer)
	if err != nil { t.Fatal(err) }
	console_test_exec(t, rtx)
	rtx.Close()
	if pending.Len() != 0 { t.Fatal("host buffer committed during runtime cleanup") }
	err = writer.Flush()
	if err != nil || pending.String() != "pending" { t.Fatalf("host flush: %q %v", pending.String(), err) }
}

func TestConsoleIONilStreamsAndLoop(t *testing.T) {
	var rtx *hawk.Rtx
	var output bytes.Buffer
	var value *hawk.Val
	var err error

	_, rtx = console_test_runtime(t, `BEGIN { print "discarded"; if ((getline x) != 0) exit 1; }`)
	err = rtx.SetConsoleIO(nil, nil)
	if err != nil { t.Fatal(err) }
	console_test_exec(t, rtx)
	if rtx.ConsoleError() != nil { t.Fatal(rtx.ConsoleError()) }
	_, rtx = console_test_runtime(t, `{ print "record:" $0; }`)
	err = rtx.SetConsoleIO(strings.NewReader("a\nb"), &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Loop()
	if err != nil { t.Fatal(err) }
	value.Close()
	if output.String() != "record:a\nrecord:b\n" { t.Fatalf("Loop output: %q", output.String()) }
}

func TestConsoleIOFailures(t *testing.T) {
	var rtx *hawk.Rtx
	var reader *console_reader
	var output console_writer
	var read_error error = errors.New("reader failed")
	var write_error error = errors.New("writer failed")
	var value *hawk.Val
	var err error

	_, rtx = console_test_runtime(t, `BEGIN { getline line; printf "%s", line; fflush(); }`)
	reader = &console_reader{data: []byte("valid\n"), tail_error: read_error}
	err = rtx.SetConsoleIO(reader, &output)
	if err != nil { t.Fatal(err) }
	console_test_exec(t, rtx)
	if output.buffer.String() != "valid" || rtx.ConsoleError() != read_error { t.Fatalf("bytes plus error lost: %q %v", output.buffer.String(), rtx.ConsoleError()) }
	_, rtx = console_test_runtime(t, `BEGIN { print "failure"; }`)
	output = console_writer{failure: write_error}
	err = rtx.SetConsoleIO(nil, &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Exec(nil)
	if value != nil { value.Close() }
	if rtx.ConsoleError() != write_error { t.Fatalf("write failure hidden: %v %v", err, rtx.ConsoleError()) }
	rtx.Close()
	if output.calls != 1 { t.Fatalf("failed write retried during cleanup: %d", output.calls) }
	_, rtx = console_test_runtime(t, `BEGIN { print "short"; }`)
	output = console_writer{short: true}
	err = rtx.SetConsoleIO(nil, &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Exec(nil)
	if value != nil { value.Close() }
	if rtx.ConsoleError() != io.ErrShortWrite { t.Fatalf("short write hidden: %v %v", err, rtx.ConsoleError()) }
}

func TestConsoleIORedirection(t *testing.T) {
	var rtx *hawk.Rtx
	var output bytes.Buffer
	var path string
	var source string
	var data []byte
	var err error

	path = filepath.Join(t.TempDir(), "redirect.txt")
	source = fmt.Sprintf(`BEGIN { print "file" > "%s"; close("%s"); print "console"; }`, filepath.ToSlash(path), filepath.ToSlash(path))
	_, rtx = console_test_runtime(t, source)
	err = rtx.SetConsoleIO(nil, &output)
	if err != nil { t.Fatal(err) }
	console_test_exec(t, rtx)
	data, err = os.ReadFile(path)
	if err != nil || string(data) != "file\n" || output.String() != "console\n" { t.Fatalf("redirection: %q %q %v", data, output.String(), err) }
}

func TestConsoleIOConcurrentRuntimes(t *testing.T) {
	var h *hawk.Hawk
	var runtimes []*hawk.Rtx
	var outputs []bytes.Buffer
	var wg sync.WaitGroup
	var i int
	var err error

	h, err = hawk.New()
	if err != nil { t.Fatal(err) }
	defer h.Close()
	err = h.ParseText(`function serve() { @local line; while ((getline line)>0) print line; }`)
	if err != nil { t.Fatal(err) }
	runtimes = make([]*hawk.Rtx, 16)
	outputs = make([]bytes.Buffer, len(runtimes))
	for i = range runtimes {
		runtimes[i], err = h.NewRtx("concurrent-console", nil, nil)
		if err != nil { t.Fatal(err) }
		err = runtimes[i].SetConsoleIO(strings.NewReader(fmt.Sprintf("runtime-%d", i)), &outputs[i])
		if err != nil { t.Fatal(err) }
	}
	for i = range runtimes {
		wg.Add(1)
		go func(index int) {
			var value *hawk.Val
			var err error

			defer wg.Done()
			value, err = runtimes[index].Call("serve")
			if err != nil { t.Error(err); return }
			value.Close()
		}(i)
	}
	wg.Wait()
	for i = range runtimes {
		if outputs[i].String() != fmt.Sprintf("runtime-%d\n", i) { t.Fatalf("runtime %d received another runtime's output: %q", i, outputs[i].String()) }
		runtimes[i].Close()
	}
}

func TestConsoleIOFlushFailure(t *testing.T) {
	var rtx *hawk.Rtx
	var output bytes.Buffer
	var flush_error error = errors.New("flush failed")
	var value *hawk.Val
	var err error

	_, rtx = console_test_runtime(t, `BEGIN { printf "data"; }`)
	err = rtx.SetConsoleIO(nil, &output, func() error { return flush_error })
	if err != nil { t.Fatal(err) }
	value, err = rtx.Exec(nil)
	if value != nil { value.Close() }
	if rtx.ConsoleError() != flush_error { t.Fatalf("flush failure hidden: %v %v", err, rtx.ConsoleError()) }
	rtx.Close()
	if rtx.ConsoleError() != flush_error { t.Fatal("cleanup lost the console error") }
}

type console_bad_reader struct {
	invalid_count bool
	calls int
}

func (r *console_bad_reader) Read(buffer []byte) (int, error) {
	r.calls++
	if r.invalid_count { return len(buffer) + 1, nil }
	return 0, nil
}

func TestConsoleIOReaderFailures(t *testing.T) {
	var rtx *hawk.Rtx
	var reader console_bad_reader
	var output bytes.Buffer
	var value *hawk.Val
	var err error

	_, rtx = console_test_runtime(t, `BEGIN { getline line; }`)
	err = rtx.SetConsoleIO(&reader, &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Exec(nil)
	if value != nil { value.Close() }
	if rtx.ConsoleError() != io.ErrNoProgress || reader.calls > 100 { t.Fatalf("non-progressing reader: %v (%d calls)", rtx.ConsoleError(), reader.calls) }
	_, rtx = console_test_runtime(t, `BEGIN { getline line; }`)
	reader = console_bad_reader{invalid_count: true}
	err = rtx.SetConsoleIO(&reader, &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Exec(nil)
	if value != nil { value.Close() }
	if rtx.ConsoleError() == nil { t.Fatal("invalid reader byte count was accepted") }
	_, rtx = console_test_runtime(t, `BEGIN { getline line; }`)
	err = rtx.SetConsoleIO(strings.NewReader("\xe2\x82"), &output)
	if err != nil { t.Fatal(err) }
	value, err = rtx.Exec(nil)
	if value != nil { value.Close() }
	if rtx.ConsoleError() == nil { t.Fatal("incomplete multibyte sequence was accepted") }
}
