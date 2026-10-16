package hawk

import "errors"
import "os"
import "path/filepath"
import "reflect"
import "strings"
import "testing"

func TestParseFileWithDependencies(t *testing.T) {
	var root string
	var main string
	var first string
	var second string
	var search string
	var h *Hawk
	var paths []string
	var expected []string
	var result *Val
	var text string
	var err error

	root = t.TempDir()
	main = filepath.Join(root, "main.hawk")
	first = filepath.Join(root, "nested", "first.inc")
	search = filepath.Join(root, "search")
	second = filepath.Join(search, "日本.inc")
	err = os.MkdirAll(filepath.Dir(first), 0700)
	if err != nil { t.Fatal(err) }
	err = os.MkdirAll(search, 0700)
	if err != nil { t.Fatal(err) }
	err = os.WriteFile(second, []byte("@const MESSAGE = \"日本 & Hawk\";\n"), 0600)
	if err != nil { t.Fatal(err) }
	err = os.WriteFile(first, []byte("@include_once \"日本.inc\";\n"), 0600)
	if err != nil { t.Fatal(err) }
	err = os.WriteFile(main, []byte("@include_once \"nested/first.inc\";\n@include_once \"nested/first.inc\";\nfunction message() { return MESSAGE; }\n"), 0600)
	if err != nil { t.Fatal(err) }
	h, err = New()
	if err != nil { t.Fatal(err) }
	defer h.Close()
	err = h.SetIncDirs(search)
	if err != nil { t.Fatal(err) }
	paths, err = h.ParseFileWithDependencies(main)
	if err != nil { t.Fatal(err) }
	expected = []string{main, first, second}
	if !reflect.DeepEqual(paths, expected) { t.Fatalf("source paths: %v, want %v", paths, expected) }
	{
		var rtx *Rtx
		rtx, err = h.NewRtx("sources", nil, nil)
		if err != nil { t.Fatal(err) }
		defer rtx.Close()
		result, err = rtx.Call("message")
		if err != nil { t.Fatal(err) }
		defer result.Close()
		text, err = result.ToStr()
		if err != nil || text != "日本 & Hawk" { t.Fatalf("parsed result %q: %v", text, err) }
	}
}

func TestParseDependenciesRejectChangesAndObserverErrors(t *testing.T) {
	var h *Hawk
	var main string
	var dependencies *parse_dependencies
	var paths []string
	var sentinel error
	var err error

	main = filepath.Join(t.TempDir(), "main.hawk")
	err = os.WriteFile(main, []byte("function message() { return 1; }"), 0600)
	if err != nil { t.Fatal(err) }
	h, err = New()
	if err != nil { t.Fatal(err) }
	defer h.Close()
	dependencies, err = h.parse_file_with_dependencies(main, func(path string) error {
		return os.WriteFile(path, []byte("function message() { return 222; }"), 0600)
	})
	if err == nil || !strings.Contains(err.Error(), "changed while parsing") || dependencies != nil { t.Fatalf("changed source accepted: %v", err) }
	sentinel = errors.New("observer rejected source")
	_, err = h.parse_file_with_dependencies(main, func(path string) error { return sentinel })
	if !errors.Is(err, sentinel) { t.Fatalf("observer failure: %v", err) }
	// The failed observer and its Go handle must not survive a later parse.
	err = h.ParseFile(main)
	if err != nil { t.Fatal(err) }
	paths, err = h.ParseFileWithDependencies(main)
	if err != nil || !reflect.DeepEqual(paths, []string{main}) { t.Fatalf("tracking after failures: %v: %v", paths, err) }
}

func TestParseDependenciesFailedInclude(t *testing.T) {
	var h *Hawk
	var main string
	var paths []string
	var err error

	main = filepath.Join(t.TempDir(), "main.hawk")
	err = os.WriteFile(main, []byte("@include \"missing.inc\";"), 0600)
	if err != nil { t.Fatal(err) }
	h, err = New()
	if err != nil { t.Fatal(err) }
	defer h.Close()
	paths, err = h.ParseFileWithDependencies(main)
	if err == nil || paths != nil { t.Fatalf("missing include accepted: %v: %v", paths, err) }
	err = os.WriteFile(main, []byte("function message() { return 1; }"), 0600)
	if err != nil { t.Fatal(err) }
	paths, err = h.ParseFileWithDependencies(main)
	if err != nil || len(paths) != 1 { t.Fatalf("failed include retained dependencies: %v: %v", paths, err) }
}
