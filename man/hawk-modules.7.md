# NAME

hawk-modules - Hawk module loading and common module interfaces

# DESCRIPTION

Module symbols use `module::name`, for example `str::trim(text)` and
`sys::getpid()`. A module can be built into the library or loaded dynamically.
Availability depends on the build and, for optional integrations, their
external dependencies.

The standard runner's `--modlibdirs` option selects the dynamic module search
path. The library's module loader handles the platform-specific library prefix
and suffix. Resolved modules and their resources are managed by the interpreter
and runtime lifecycle.

Module symbols are normally resolved during parsing. Use
`@pragma defermodsym on` or `--defermodsym=on` to resolve them at runtime instead.
This postpones lookup; it does not make an unavailable module available.

This page introduces common interfaces. Function error conventions differ by
module: some return a negative code with a module diagnostic, while invalid
values or operations in other modules raise a runtime error. Check the contract
of the function being used rather than interpreting every negative numeric
result as an error.

Language syntax and types are described in
[hawk-language(7)](hawk-language.7.md).

# HAWK

The `hawk::` module provides runtime and value helpers:

- `hawk::array(value, ...)`: construct an array with positions starting at 1.
- `hawk::indexed_array(index, value, ...)`: construct an array from index/value
  pairs, including sparse indexes and zero.
- `hawk::map()`: construct an empty map.
- `hawk::isarray(value)`, `hawk::ismap(value)`, `hawk::isnil(value)`: test value
  categories.
- `hawk::type(value)`: return a numeric type code.
- `hawk::typename(value)`: return a type name, such as `int`, `bool`, or `map`.
- `hawk::gc()`: request garbage collection.

Type constants include `hawk::VAL_NIL`, `hawk::VAL_BOOL`, `hawk::VAL_INT`,
`hawk::VAL_FLT`, `hawk::VAL_STR`, `hawk::VAL_CHAR`, `hawk::VAL_MBS`,
`hawk::VAL_BCHAR`, `hawk::VAL_ARRAY`, `hawk::VAL_MAP`, `hawk::VAL_FUN`, and
`hawk::VAL_BOB`. Compare type codes with these symbols rather than hardcoded
numbers.

```awk
BEGIN {
	@local values = hawk::indexed_array(0, "zero", 3, "three")
	print hawk::typename(values), hawk::type(values) == hawk::VAL_ARRAY
	printf "%v\n", values
}
```

# STR

The `str::` module supplies text conversion, searching, formatting, and byte
encoding. Familiar operations include `str::length`, `str::substr`,
`str::index`, `str::rindex`, `str::match`, `str::split`, `str::sub`, `str::gsub`,
`str::tolower`, `str::toupper`, and `str::trim`. `str::printf` returns formatted
text, like `sprintf`.

`str::tonum(value)` converts text to a number, recognizing numeric prefixes.
`str::tohex(bytes)` produces hexadecimal text. Byte strings preserve binary
data, including embedded NULs, when passed to binary encoding functions.

`str::tobase64(data [, flags])` encodes data and returns a byte string.
`str::frombase64(text [, flags])` decodes it. Flags combine
`str::BASE64_URL` for the URL-safe alphabet and `str::BASE64_NOPAD` for
unpadded operation.

```awk
BEGIN {
	@local flags = (str::BASE64_URL | str::BASE64_NOPAD)
	@local encoded = str::tobase64(@b"hello", flags)
	print encoded
	print str::frombase64(encoded, flags)
}
```

# MATH

The `math::` module provides `sin`, `cos`, `tan`, `asin`, `acos`, `atan`,
`atan2`, `sinh`, `cosh`, `tanh`, `exp`, `log`, `log2`, `log10`, `sqrt`,
`floor`, `ceil`, and `round`.

`math::rand()` returns a pseudorandom value. `math::srand([seed])` seeds the
generator. These routines are for ordinary pseudorandom computation; their
sequence is dependent on the implementation and platform.

```awk
BEGIN { print math::sqrt(81), math::floor(3.75) }
```

# JSON

The optional `json::` module converts JSON text and Hawk values:

- `json::parse(text)`: return maps, arrays, strings, numbers, booleans, and nil.
- `json::stringify(value)`: serialize a supported value as JSON text.

JSON arrays become ordinary one-based Hawk arrays. JSON objects become maps.
Use this serializer rather than `%v` when another program expects JSON;
Hawk's diagnostic representation has different syntax and truncation rules.

```awk
BEGIN {
	@local data = json::parse("{\"name\":\"Hawk\",\"items\":[1,2,3]}")
	data.items[4] = 4
	print data.name
	print json::stringify(data)
}
```

# DIGEST

The optional `digest::` module implements MD5, SHA-1, and SHA-256:

- `digest::md5(data)`, `digest::sha1(data)`, `digest::sha256(data)`: return the
  raw digest of one input.
- `digest::init(name)`: create a streaming context for `md5`, `sha1`, or
  `sha256`.
- `digest::update(context, data)`: add a chunk and return the context.
- `digest::final(context)`: finalize the context and return the raw digest.

Use `str::tohex()` to display a digest. Streaming contexts are opaque BOB
values and require no explicit close. Assignments alias the same mutable
context. A finalized context cannot be updated or finalized again.

```awk
BEGIN {
	@local context = digest::init("sha256")
	digest::update(context, @b"hello ")
	digest::update(context, @b"world")
	print str::tohex(digest::final(context))
}
```

# TEXT PROCESSORS

The `sed::`, `m4::`, and `mus::` modules use a mode argument to choose whether
input and output arguments represent text or filenames. Each module exports
its own `STR_TO_STR`, `FILE_TO_STR`, `STR_TO_FILE`, and `FILE_TO_FILE` constants.

For a string output mode, pass an output variable directly; the function binds
it by reference. For a file output mode, pass the output path. A successful
processing call returns zero; check nonzero results before using the output.
Each call creates its own processor instance.

## Sed

`sed::process(script, input, output, mode)` applies a sed script. The script is
a string independently of whether the input data comes from a file.

```awk
BEGIN {
	@local output
	if (sed::process("s/foo/bar/g", "foo foo", output, sed::STR_TO_STR) == 0)
		print output
}
```

For the standalone stream-editor command and its line-ending options, see
[hawk(1)](hawk.1.md#sed-mode).

## M4

`m4::process(input, output, mode [, include_directories])` expands m4 source.
The input contains both macro definitions and the text being processed.
Include directories form a platform-separated path list used by `include`
and `sinclude`.

```awk
BEGIN {
	@local output
	@local source = "define(`NAME', `Hawk')dnl\nNAME"
	if (m4::process(source, output, m4::STR_TO_STR) == 0) print output
}
```

The engine supports multi-character quotes, `pushdef`, `popdef`, includes,
conditionals implemented as m4 builtins, and diversions. The command-line
interface is described in [hawk(1)](hawk.1.md#m4-mode).

## Mustache

`mus::process(template, data, output, mode [, partial_directories])` renders a
Mustache template using a Hawk value as data. `mus::errmsg()` returns the last
processing error and is cleared by success. The optional partial-directory
list uses the platform path-list separator.

```awk
BEGIN {
	@local data = @{"name": "Hawk", "items": @["one", "two"]}
	@local output
	if (mus::process("Hello {{name}}:{{#items}} {{.}}{{/items}}",
	                 data, output, mus::STR_TO_STR) == 0)
		print output
	else print mus::errmsg()
}
```

Ordinary variables are HTML escaped. `{{{name}}}` and `{{&name}}` insert raw
text. The engine supports dotted names, sections, inverted sections, comments,
changed delimiters, and partials. Missing partials produce empty output.
Optional lambdas, inheritance, and dynamic partial names are not implemented.

Maps supply named fields. Arrays iterate occupied indexes in ascending order.
Nil, false, and empty arrays are false in templates; numeric zero and empty
strings are true. Booleans interpolate as `1` and `0`. Interpolation of an array
or map as a scalar, or use of a function value, is rejected.

String output is replaced only after successful rendering. File output may be
partial after an error. Templates are compiled for each call.

# SYS

The `sys::` module provides operating-system interfaces. Available operations
depend on the platform. The module commonly returns negative result codes and
stores a diagnostic retrievable with `sys::errmsg()`.

Common groups include:

- Paths and files: `basename`, `dirname`, `open`, `openfd`, `close`, `read`,
  `write`, `fseek`, `unlink`, `mkdir`, `chmod`.
- Directories: `opendir`, `readdir`, `closedir`.
- Processes: `system`, `fork`, `wait`, `kill`, `getpid`, `getppid`.
- Time: `gettime`, `mktime`, `strftime`, `sleep`.
- Descriptors and readiness: `pipe`, `dup`, `openmux`, `addtomux`, `delfrommux`,
  `waitonmux`.
- Pseudo-terminals: `openpty`, `openpt`, `grantpt`, `unlockpt`, `ptsname`,
  `setsid`, `login_tty`.
- Binary layouts: `pack`, `unpack`.

The handle returned by a module function such as `sys::open()` is a Hawk module
handle, not generally the raw operating-system descriptor. Use `sys::openfd()`
only when deliberately wrapping a raw descriptor. Wrappers sharing a raw
descriptor also share its underlying position and lifetime; closing it can
invalidate the others. Buffered input may have read ahead.

`sys::tempname([prefix])` returns a temporary pathname without leaving the file
created or open. It is useful for later Hawk redirection. It does not reserve
the name for a future open.

`sys::gettime([nanoseconds])` returns epoch seconds; the optional output variable
receives the nanosecond part. It returns a negative result on failure.
`sys::mktime()` accepts a broken-down local time string such as
`"2026 01 01 00 00 00 -1"`. Negative epoch times can be valid: use
`sys::errmsg()` to distinguish a conversion error; successful `mktime()` clears
that diagnostic.

`sys::strftime(format, seconds [, flags])` formats local time.
`sys::STRFTIME_UTC` selects UTC. Formats containing embedded NULs are rejected.

```awk
BEGIN {
	@local now = sys::gettime()
	if (now >= 0) print sys::strftime("%Y-%m-%d %H:%M:%S", now)
	else print sys::errmsg()
}
```

# SQLITE

When built with SQLite support, `sqlite::` provides database handles and
prepared statements. The typical sequence is:

1. `sqlite::open()` allocates a database handle.
2. `sqlite::connect(database, path [, flags [, vfs]])` opens the database.
3. `sqlite::prepare(database, sql)` creates a statement.
4. `sqlite::bind(statement, position, value)` binds a one-based parameter.
5. `sqlite::fetch_row(statement, row [, mode])` advances and writes a row.
6. `sqlite::finalize(statement)` releases the statement.
7. `sqlite::close(database)` releases the database handle.

`sqlite::exec(database, sql)` executes SQL without a result-row loop.
`sqlite::errmsg()` returns the module diagnostic. `FETCH_ROW_ARRAY` produces
rows indexed from 1; `FETCH_ROW_MAP` uses column names.

```awk
BEGIN {
	@local db = sqlite::open()
	@local statement, row
	if (db < 0) { print sqlite::errmsg(); exit 1 }
	if (sqlite::connect(db, ":memory:") < 0) {
		print sqlite::errmsg(); sqlite::close(db); exit 1
	}
	statement = sqlite::prepare(db, "select ? as message")
	if (statement >= 0) {
		if (sqlite::bind(statement, 1, "Hello") >= 0 &&
		    sqlite::fetch_row(statement, row, sqlite::FETCH_ROW_ARRAY) > 0)
			print row[1]
		sqlite::finalize(statement)
	}
	else print sqlite::errmsg()
	sqlite::close(db)
}
```

# HTTP

The optional C `http::` module communicates with an HTTP context attached by
the host. HawkHttpd supplies this context; embedding applications can provide
their own implementation through the auxiliary-service interface. Loading the
module alone does not create an HTTP server or request context.

- `http::getmethod()`, `http::getpath()`: request method and path.
- `http::getheader(name [, index])`: one request-header value, with indexes
  starting at 1; a missing value is an empty string.
- `http::getheaders()`: a map of header names to arrays of values.
- `http::setstatus(code)`, `http::getstatus()`: set or read response status;
  accepted integer status codes range from 200 to 599.
- `http::setheader(name, value)`: replace a response header.
- `http::addheader(name, value)`: append a response-header value.
- `http::delheader(name)`: remove a response header.
- `http::flush()`: commit status and headers and flush console output.

Setters and `flush()` return zero on success. Missing host contexts, invalid
arguments, host failures, and header/status changes after commitment raise a
runtime error. Header names must be nonempty ASCII tokens. Header values must
be strings without NUL or control characters other than tab.

See [hawkgo(1)](hawkgo.1.md#request-handlers) for complete request-handler and
server examples.

# OTHER MODULES

Depending on configuration and installed dependencies, builds can provide
additional modules such as `ffi::` and `mysql::`. Their interfaces and
availability are separate from those of the core interpreter. Consult the
module source and the project README for these optional integrations.

# SEE ALSO

[hawk(1)](hawk.1.md), [hawkgo(1)](hawkgo.1.md),
[hawk-language(7)](hawk-language.7.md).
