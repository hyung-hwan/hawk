package hawk

/*
#include <hawk.h>
*/
import "C"

import "fmt"
import "io"
import "runtime"
import "unsafe"

type rtx_console_io struct {
	input io.Reader
	output io.Writer
	flush func() error
	pending_read_error error
	write_error error
	error error
}

// SetConsoleIO replaces only this runtime's console I/O. Explicit file and pipe
// redirections keep their standard handlers. Text uses the runtime's character
// manager; byte strings are transferred unchanged. A nil input means EOF, and a
// nil output discards data. The reader and writer remain owned by the caller.
// An optional flush callback handles the interpreter's flush requests (including
// printf, fflush, and execution completion). Without it, the caller controls any
// buffering in its writer. Set this before Exec, Loop, or Call. A runtime must
// not be used concurrently.
func (rtx *Rtx) SetConsoleIO(input io.Reader, output io.Writer, flush ...func() error) error {
	if rtx.h == nil {
		return fmt.Errorf("runtime is closed")
	}

	if rtx.execution_started {
		return fmt.Errorf("console I/O must be set before runtime execution")
	}

	if len(flush) > 1 {
		return fmt.Errorf("at most one console flush callback is allowed")
	}

	if output == nil { output = io.Discard }

	rtx.console_io = &rtx_console_io{input: input, output: output}
	if len(flush) == 1 { rtx.console_io.flush = flush[0] }

	rtx.set_console_io()
	return nil
}

// ConsoleError reports the first console failure, including failures that a
// script handles through getline's -1 result or the tolerant print/printf mode.
// It remains available after Close.
// Runtime teardown neither closes nor flushes the caller's reader/writer.
// Interpreter flush requests invoke the callback supplied to SetConsoleIO.
func (rtx *Rtx) ConsoleError() error {
	if rtx.console_io == nil { return nil }
	return rtx.console_io.error
}

func (rtx *Rtx) console_error(err error, writing bool) C.hawk_ooi_t {
	if rtx.console_io.error == nil { rtx.console_io.error = err }
	if writing {
		if rtx.console_io.write_error == nil { rtx.console_io.write_error = err }
		rtx.set_errmsg(C.HAWK_EWRITE, err.Error())
	} else {
		rtx.set_errmsg(C.HAWK_EREAD, err.Error())
	}
	return -1
}

//export hawk_go_console_io
func hawk_go_console_io(id C.hawk_oow_t, cmd C.int, data unsafe.Pointer, size C.hawk_oow_t) C.hawk_ooi_t {
	var instance RtxInstance
	var rtx *Rtx
	var console *rtx_console_io
	var buffer []byte
	var n int
	var attempts int
	var err error

	instance = rtx_inst_table.slot_to_instance(int(id))
	rtx = instance.g.Value()
	if rtx == nil || rtx.console_io == nil { return -1 }
	defer runtime.KeepAlive(rtx)
	console = rtx.console_io
	if cmd == 3 {
		err = rtx.make_errinfo()
		if console.error == nil { console.error = err }
		if size != 0 && console.write_error == nil { console.write_error = err }
		return 0
	}
	if cmd == 2 {
		if console.write_error != nil { return rtx.console_error(console.write_error, true) }
		if console.flush != nil {
			err = console.flush()
			if err != nil { return rtx.console_error(err, true) }
		}
		return 0
	}
	if size > C.hawk_oow_t(^uint(0) >> 1) {
		return rtx.console_error(fmt.Errorf("console transfer size is too large"), cmd == 1)
	}
	buffer = unsafe.Slice((*byte)(data), int(size))
	if len(buffer) == 0 { return 0 }
	if cmd == 0 {
		if console.input == nil || console.pending_read_error == io.EOF { return 0 }
		if console.pending_read_error != nil { return rtx.console_error(console.pending_read_error, false) }
		for attempts = 0; attempts < 100; attempts++ {
			n, err = console.input.Read(buffer)
			if n < 0 || n > len(buffer) { return rtx.console_error(fmt.Errorf("console reader returned an invalid byte count"), false) }
			if err != nil {
				console.pending_read_error = err
				if err != io.EOF && console.error == nil { console.error = err }
			}
			if n > 0 { return C.hawk_ooi_t(n) }
			if err == io.EOF { return 0 }
			if err != nil { return rtx.console_error(err, false) }
		}
		return rtx.console_error(io.ErrNoProgress, false)
	}
	if cmd == 1 {
		if console.write_error != nil { return rtx.console_error(console.write_error, true) }
		n, err = console.output.Write(buffer)
		if n < 0 || n > len(buffer) { return rtx.console_error(fmt.Errorf("console writer returned an invalid byte count"), true) }
		if err == nil && n != len(buffer) { err = io.ErrShortWrite }
		if err != nil { return rtx.console_error(err, true) }
		return C.hawk_ooi_t(n)
	}
	return rtx.console_error(fmt.Errorf("unknown console I/O command"), false)
}
