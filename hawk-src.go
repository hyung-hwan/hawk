package hawk

/*
#include <hawk.h>
#include <stdlib.h>

extern int hawk_go_source_observer(hawk_uintptr_t, hawk_t*, hawk_sio_arg_t*);

static int source_observer (hawk_t* hawk, hawk_sio_cmd_t cmd, const hawk_sio_arg_t* arg,
                            const hawk_ooch_t* data, hawk_oow_t size, void* ctx)
{
	if (cmd != HAWK_SIO_CMD_OPEN) return 0;
	return hawk_go_source_observer((hawk_uintptr_t)ctx, hawk, (hawk_sio_arg_t*)arg);
}

static int parse_file_observed (hawk_t* hawk, hawk_bch_t* path, hawk_uintptr_t id)
{
	hawk_parsestd_t input[2] = { { 0 } };

	input[0].type = HAWK_PARSESTD_FILEB;
	input[0].u.fileb.path = path;

	return hawk_parsestdwithobserver(hawk, input, HAWK_NULL, source_observer, (void*)id);
}

static hawk_bch_t* source_path (hawk_t* hawk, const hawk_ooch_t* path)
{
#if defined(HAWK_OOCH_IS_UCH)
	return hawk_duputobcharswithcmgr(hawk, path, hawk_count_oocstr(path), HAWK_NULL, hawk_get_utf8_cmgr());
#else
	return hawk_dupbcstr(hawk, path, HAWK_NULL);
#endif
}

*/
import "C"

import "fmt"
import "os"
import "path/filepath"
import "runtime/cgo"
import "strings"
import "unsafe"

type parse_dependencies struct {
	hawk *Hawk
	files []string
	stats map[string]os.FileInfo
	probes []string
	probe_states map[string]bool
	on_open func(string) error
	err error
}

// ParseFileWithDependencies parses a file and returns the absolute paths of
// all source files opened, including the main file and nested/once includes.
// Paths are deduplicated in opening order. Tracking reads no file contents;
// file identity, size and modification time are checked for changes during
// parsing. No dependency list is returned on failure.
func (hawk *Hawk) ParseFileWithDependencies(file string) ([]string, error) {
	var dependencies *parse_dependencies
	var err error

	dependencies, err = hawk.parse_file_with_dependencies(file, nil)
	if err != nil { return nil, err }
	return dependencies.files, nil
}

func (hawk *Hawk) parse_file_with_dependencies(file string, on_open func(string) error) (*parse_dependencies, error) {
	var dependencies *parse_dependencies
	var handle cgo.Handle
	var cfile *C.hawk_bch_t
	var n C.int
	var err error

	if file == "-" { return nil, fmt.Errorf("dependency tracking requires a source file") }

	file, err = filepath.Abs(file)
	if err != nil { return nil, err }

	dependencies = &parse_dependencies{hawk: hawk, stats: make(map[string]os.FileInfo),
		probe_states: make(map[string]bool), on_open: on_open}

	handle = cgo.NewHandle(dependencies)
	defer handle.Delete()

	cfile = C.CString(file)
	n = C.parse_file_observed(hawk.c, cfile, C.hawk_uintptr_t(handle))
	C.free(unsafe.Pointer(cfile))
	if dependencies.err != nil { return nil, dependencies.err }
	if n <= -1 { return nil, hawk.make_errinfo() }
	if err = dependencies.check(); err != nil { return nil, err }
	return dependencies, nil
}

func same_source(first os.FileInfo, second os.FileInfo) bool {
	return os.SameFile(first, second) && first.Size() == second.Size() && first.ModTime().Equal(second.ModTime())
}

func (dependencies *parse_dependencies) check() error {
	var path string
	var before os.FileInfo
	var after os.FileInfo
	var err error

	for path, before = range dependencies.stats {
		after, err = os.Stat(path)
		if err != nil { return err }
		if !same_source(before, after) { return fmt.Errorf("source changed while parsing %s", path) }
	}
	for _, path = range dependencies.probes {
		if source_probe(path) != dependencies.probe_states[path] {
			return fmt.Errorf("include resolution changed while parsing %s", path)
		}
	}
	return nil
}

func source_string(hawk *C.hawk_t, ptr *C.hawk_ooch_t) (string, error) {
	var text *C.hawk_bch_t
	var result string

	if ptr == nil { return "", nil }

	text = C.source_path(hawk, ptr)
	if text == nil { return "", fmt.Errorf("unable to convert source path") }
	result = C.GoString(text)
	C.hawk_freemem(hawk, unsafe.Pointer(text))

	return result, nil
}

func source_probe(path string) bool {
	var info os.FileInfo
	var err error

	info, err = os.Stat(path)
	return err == nil && !info.IsDir()
}

func (dependencies *parse_dependencies) track_probes(hawk *C.hawk_t, arg *C.hawk_sio_arg_t) error {
	var name string
	var parent string
	var dirs string
	var dir string
	var path string
	var roots []string
	var cdirs *C.hawk_ooch_t
	var present bool
	var previous bool
	var exists bool
	var err error

	if arg.prev == nil { return nil }

	name, err = source_string(hawk, arg.name)
	if err != nil { return err }
	if filepath.IsAbs(name) { return nil }

	parent, err = source_string(hawk, arg.prev.path)
	if err != nil { return err }

	roots = []string{filepath.Dir(parent)}
	if C.hawk_getopt(hawk, C.HAWK_OPT_INCDIRS, unsafe.Pointer(&cdirs)) <= -1 {
		return dependencies.hawk.make_errinfo()
	}

	dirs, err = source_string(hawk, cdirs)
	if err != nil { return err }

	// the standard IO resolver uses colon-separated include directories.
	if dirs != "" {
		roots = append(roots, strings.Split(dirs, ":")...)
	}

	for _, dir = range roots {
		if dir == "" { dir = string(filepath.Separator) }

		path, err = filepath.Abs(filepath.Join(dir, name))
		if err != nil { return err }

		present = source_probe(path)

		previous, exists = dependencies.probe_states[path]
		if exists && previous != present {
			return fmt.Errorf("include resolution changed while parsing %s", name)
		}
		if !exists {
			dependencies.probes = append(dependencies.probes, path)
		}

		dependencies.probe_states[path] = present
	}
	return nil
}

//export hawk_go_source_observer
func hawk_go_source_observer(id C.hawk_uintptr_t, hawk *C.hawk_t, arg *C.hawk_sio_arg_t) C.int {
	var dependencies *parse_dependencies
	var path string
	var info os.FileInfo
	var previous os.FileInfo
	var exists bool
	var err error

	dependencies = cgo.Handle(id).Value().(*parse_dependencies)
	if dependencies.err != nil { return -1 }

	path, err = source_string(hawk, arg.path)
	if err == nil { path, err = filepath.Abs(path) }
	if err == nil { info, err = os.Stat(path) }
	if err == nil { err = dependencies.track_probes(hawk, arg) }
	if err == nil {
		previous, exists = dependencies.stats[path]
		if exists && !same_source(previous, info) {
			err = fmt.Errorf("source changed while parsing %s", path)
		} else if !exists {
			dependencies.files = append(dependencies.files, path)
			dependencies.stats[path] = info
		}
	}
	if err == nil && dependencies.on_open != nil { err = dependencies.on_open(path) }
	if err != nil {
		dependencies.err = err
		C.hawk_seterrnum(hawk, nil, C.HAWK_EIOERR)
		return -1
	}
	return 0
}
