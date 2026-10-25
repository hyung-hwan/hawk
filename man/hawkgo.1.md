# NAME

hawkgo - Go-based Hawk script runner and HTTP application server

# SYNOPSIS

**hawkgo** [*options*] *program* [*datafile* ...]

**hawkgo** [*options*] **-f** *sourcefile* [*datafile* ...]

**hawkgo** **--httpd** [*server-options*]

# DESCRIPTION

`hawkgo` runs Hawk programs using the Go bindings to the Hawk library. Its
normal command-line mode can process input files sequentially or concurrently.
Its HTTP mode serves static files and runs Hawk functions for configured URL
routes.

Building the executable is optional; configure Hawk with `--enable-hawkgo`.
The HTTP server additionally needs the `http::` module. A build without that
module reports HTTP server unavailability when a server is created.

The script language is described in [hawk-language(7)](hawk-language.7.md).
For the C executable and its text-processor modes, see [hawk(1)](hawk.1.md).

# SCRIPT RUNNER OPTIONS

Options precede the source string and data operands. Without `-f`, the first
operand is program text. Without data files, the normal input stream is standard
input. Script output goes to standard output unless redirected or configured
per file in concurrent mode.

- **-f** *file*, **--file** *file*: read a source file. Repeat to provide
  several source files.
- **-c** *name*, **--call** *name*: call a function directly. Remaining
  operands are its string arguments rather than normal data-file operands.
- **-v** *name=value*, **--assign** *name=value*: initialize a global variable.
  Repeat for several variables.
- **-F** *separator*, **--field-separator** *separator*: initialize `FS`.
- **--incdirs** *directories*: accept an include-directory setting; see the
  implementation note below.
- **--modlibdirs** *directories*: set the dynamic-module search path.
- **-D**, **--show-extra-info**: print additional execution information.
- **--concurrent=true**: process data files concurrently with separate runtime
  contexts.
- **--concurrent=.suffix**: process files concurrently and write each result to
  the input filename plus the suffix, such as `.out`.

Directory lists use the platform's path-list separator. In concurrent mode a
data operand can be `input:output` to specify its destination explicitly.
Without a destination or suffix, outputs share standard output and can
interleave. Runtime globals are separate for each input job.

Use `-h` or `--help` to print the normal runner's usage.

The current normal runner passes `--incdirs` to the module-directory setter,
so it does not configure include lookup as intended. The HTTP server's
`--incdirs` option sets the include path correctly. Use the C runner when a
command-line script needs a configured include search path.

```sh
hawkgo 'BEGIN { print "Hello from Hawk" }'
hawkgo -F, -f report.hawk records.csv
hawkgo --concurrent=.out -f transform.hawk first.txt second.txt
```

# HTTP SERVER OPTIONS

`--httpd` must be the first argument. Server options follow it. Normal runner
options are not automatically HTTP server options.

- **--listen** *address*: add an HTTP listening address. Repeat for several
  listeners. Without HTTP or HTTPS listeners, use `127.0.0.1:8080`.
- **--tls-listen** *address*: add an HTTPS listener. Repeat as needed; requires
  the certificate and key options below.
- **--tls-cert** *file*: PEM certificate file for HTTPS listeners.
- **--tls-key** *file*: PEM private key file for HTTPS listeners.
- **--root** *directory*: document root for static files.
- **--script-root** *directory*: base for relative route script and explicit
  dependency paths. Default: the working directory. Absolute paths are used
  directly.
- **--route** *prefix=script*: map a URL prefix to a Hawk script. Repeat for
  several routes. The longest matching route takes precedence over static
  content.
- **--entry** *name*: function invoked for each script request. Default:
  `handle`.
- **--incdirs** *directories*: script include-file search path.
- **--modlibdirs** *directories*: dynamic-module search path.
- **--max-concurrent** *number*: maximum simultaneous script requests.
  Default: 64.
- **--max-body-bytes** *number*: maximum request body size. Default: 1048576.
- **--max-response-bytes** *number*: maximum script response size, including
  output already flushed. Default: 8388608.
- **--reload-interval** *duration*: minimum interval between script-change
  checks. Default: `1s`.
- **--shutdown-timeout** *duration*: graceful shutdown waiting limit.
  Default: `30s`.
- **-h**, **--help**: show HTTP server options.

Duration values use Go duration notation, for example `500ms`, `2s`, or `1m`.
HTTP and HTTPS listeners may coexist in one server. The command logs bound
addresses to standard error.

# REQUEST HANDLERS

A route invokes its entry function directly with a request map. It does not
execute the script's `BEGIN`, pattern-action, or `END` blocks first.

The request map has these fields:

- `method`, `path`, `raw_query`, `host`, `remote_address`, and `scheme`.
- `headers`: a map of canonical header names to arrays of values.
- `query`: a map of query parameter names to arrays of values.
- `body`: a byte string holding the request body.
- `csp_nonce`: a fresh base64url-encoded nonce for the request.

Header and query arrays start at index 1. Unredirected `getline` reads the
request body as records, and `getbline` reads byte records. `print` and `printf`
write the response body. `RS`, `FS`, `OFS`, and `ORS` retain their ordinary
meaning. Named file and pipe redirections continue to address those streams.

Use `http::setstatus()`, `http::setheader()`, and related helpers to set response
metadata. The default status is 200. See
[hawk-modules(7)](hawk-modules.7.md#http) for the helper interface.

For example, save this as `scripts/handler.hawk`:

```awk
@pragma implicit off

function handle(request)
{
	http::setheader("Content-Type", "text/plain; charset=utf-8")
	print "Hello", request["path"]
}
```

Run it with:

```sh
hawkgo --httpd --listen 127.0.0.1:8080 \
  --script-root ./scripts --route /hello/=handler.hawk
```

The same instance can also serve static content and several addresses:

```sh
hawkgo --httpd --listen 127.0.0.1:8080 --listen '[::1]:8080' \
  --root ./public --script-root ./scripts --route /api/=handler.hawk
```

The document root and script root are independent. `.hawk` files are excluded
from static responses. Use an explicit route for executable scripts.

## Buffering and response maps

Console output is buffered until execution succeeds or `http::flush()` commits
the response. Before commitment, execution and console I/O errors discard
buffered output and produce HTTP 500. After commitment, errors are logged and
execution stops without replacing the already-started response. Request bodies
are read before execution; oversized bodies produce HTTP 413.

A handler may alternatively return a map with `status`, `headers`, and `body`.
Header values can be strings or arrays of strings, and the body can be text or
a byte string. The returned body is appended after any console output. Omitted
metadata fields preserve values already set through `http::` functions.
Other return values are ignored. Set headers and status before flushing.

## Reloading and lifecycle

Parsed scripts are cached. Each request uses a fresh runtime context. Changes
to the script or its resolved `@include` and `@include_once` files trigger
recompilation after the reload interval. Failed reloads retain the previous
working program. Include-search candidates are tracked too, so adding a file
earlier in the search path can change the next compilation.

SIGINT and SIGTERM request graceful shutdown. A shutdown deadline limits how
long the command waits; it does not interrupt arbitrary running Hawk code.
Request cancellation can stop a queued request but does not currently interrupt
an executing script.

Go embedding applications can use `NewHawkHttpd`, `Start`, `Addrs`, `ReqStop`,
`Shutdown`, and `Wait`. Each `HawkHttpd` owns its listeners, script cache, and
request accounting; several server instances can coexist. Its route
`Dependencies` can name additional files, such as templates, whose changes
should also trigger a reload.

# EXIT STATUS

The normal script runner returns zero on success, 99 for command-line usage
errors, and 255 for setup or execution errors. Its successful process status
does not propagate a script's numeric return value. HTTP mode returns zero
after successful shutdown and 1 on an error.

# SEE ALSO

[hawk(1)](hawk.1.md), [hawk-language(7)](hawk-language.7.md),
[hawk-modules(7)](hawk-modules.7.md).
