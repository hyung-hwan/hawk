# Hawk - Embeddable AWK Interpreter in C/C++

`Hawk` is a stable and embeddable **AWK interpreter written in C**.
It can run AWK scripts inside your own applications or as a standalone AWK engine.
The library is stable, portable, and designed for projects that need a scripting engine with a small footprint.

![Hawk](hawk.png)

## Table of Contents
- [Features](#features)
- [Building Hawk From Source Code](#building-hawk-from-source-code)
- [Embedding Hawk in C Applications](#embedding-hawk-in-c-applications)
- [Embedding Hawk in C++ Applications](#embedding-hawk-in-c-applications-1)
- [Language](#language)
	- [What Hawk Is](#what-hawk-is)
	- [Running Hawk](#running-hawk)
		- [Cut, Sed, and M4 command modes](#cut-sed-and-m4-command-modes)
	- [Execution Model](#execution-model)
		- [@pragma entry](#pragma-entry)
	- [Values and Types](#values-and-types)
	- [Expressions and Operators](#expressions-and-operators)
		- [Arithmetic and Comparison](#arithmetic-and-comparison)
		- [Strings and Regex](#strings-and-regex)
		- [Logical Operators](#logical-operators)
		- [Bitwise Operators](#bitwise-operators)
	- [Variables and Scope](#variables-and-scope)
		- [Arrays and Maps](#arrays-and-maps)
		- [Functions](#functions)
			- [Function Literals](#function-literals)
		- [Control Flow](#control-flow)
			- [if / else](#if--else)
			- [while](#while)
		- [do ... while](#do--while)
		- [for](#for)
		- [for (i in array)](#for-i-in-array)
		- [in operator (key existence)](#in-operator-key-existence)
		- [switch](#switch)
		- [break / continue / return / exit / @abort](#break--continue--return--exit--abort)
		- [nextfile / nextofile](#nextfile--nextofile)
	- [Input, Output, and Pipes](#input-output-and-pipes)
	- [Built-in Variables](#built-in-variables)
	- [Built-in Functions](#built-in-functions)
	- [Pragmas](#pragmas)
		- [@pragma entry](#pragma-entry-1)
		- [@pragma implicit](#pragma-implicit)
		- [@pragma defermodsym](#pragma-defermodsym)
		- [@pragma striprecspc](#pragma-striprecspc)
	- [@include and @include\_once](#include-and-include_once)
	- [Comments](#comments)
	- [Reserved Words](#reserved-words)
	- [Some Examples](#some-examples)
	- [Garbage Collection](#garbage-collection)
	- [Modules](#modules)
		- [Message Digest](#message-digest)
		- [Hawk](#hawk)
		- [JSON](#json)
		- [M4](#m4)
		- [Sed](#sed)
		- [String](#string)
		- [System](#system)
		- [ffi](#ffi)
		- [mysql](#mysql)
		- [sqlite](#sqlite)
	- [Incompatibility with AWK](#incompatibility-with-awk)
		- [Parameter passing](#parameter-passing)
		- [Positional variable expression](#positional-variable-expression)
		- [Return value of getline](#return-value-of-getline)
		- [Others](#others)

## Features

- Full AWK interpreter - mostly POSIX AWK compatible, with additional extensions.
- Embeddable library - integrate AWK scripting into C or C++ projects as an execution engine.
- C and C++ APIs - core functions exposed in C, with convenient C++ wrapper classes available.
- Flexible usage - usable as both a standalone command-line interpreter and a library.
- Portable core - the base library depends only on the standard C library.
- Optional extensions - loadable modules (e.g. MySQL access, FFI) can be built in or used via shared objects.
- Mature and stable - developed and maintained for many years with proven reliability.
- Embedded text processors - includes cut, sed, and m4 engines usable from C/C++ or through the CLI with `--cut`, `--sed`, and `--m4`.

# Building Hawk From Source Code

Hawk uses `autoconf` and `automake` for building. Run the following commands to configure and compile Hawk:

```sh
$ ./configure ## This step offers various build options
$ make
$ make install
```

# Embedding Hawk in C Applications

Here's an example of how Hawk can be embedded within a C application:

```c
#include <hawk.h>
#include <stdio.h>
#include <string.h>

static const hawk_bch_t* src =
	"BEGIN { print ARGV[0];"
	"   for (i=2;i<=9;i++)"
	"   {"
	"       for (j=1;j<=9;j++)"
	"           print i \"*\" j \"=\" i * j;"
	"       print \"---------------------\";"
	"   }"
	"}";

int main ()
{
	hawk_t* hawk = HAWK_NULL;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_val_t* retv;
	hawk_parsestd_t psin[2];
	int ret;

	hawk = hawk_openstd(0, HAWK_NULL); /* create a hawk instance */
	if (!hawk)
	{
		fprintf(stderr, "ERROR: cannot open hawk\n");
		ret = -1; goto oops;
	}

	/* set up source script file to read in */
	memset(&psin, 0, HAWK_SIZEOF(psin));
	psin[0].type = HAWK_PARSESTD_BCS;  /* specify the first script path */
	psin[0].u.bcs.ptr = (hawk_bch_t*)src;
	psin[0].u.bcs.len = hawk_count_bcstr(src);
	psin[1].type = HAWK_PARSESTD_NULL; /* indicate the no more script to read */

	ret = hawk_parsestd(hawk, psin, HAWK_NULL); /* parse the script */
	if (ret <= -1)
	{
		hawk_logbfmt(hawk, HAWK_LOG_STDERR, "ERROR(parse): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	/* create a runtime context needed for execution */
	rtx = hawk_rtx_openstd(
		hawk,
		0,
		HAWK_T("hawk02"), /* ARGV[0] */
		HAWK_NULL,  /* stdin */
		HAWK_NULL,  /* stdout */
		HAWK_NULL   /* default cmgr */
	);
	if (!rtx)
	{
		hawk_logbfmt(hawk, HAWK_LOG_STDERR, "ERROR(rtx_open): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	/* execute the BEGIN/pattern-action/END blocks */
	retv = hawk_rtx_loop(rtx); /* alternatively, hawk_rtx_exec(rtx, HAWK_NULL, 0) */
	if (!retv)
	{
		hawk_logbfmt(hawk, HAWK_LOG_STDERR, "ERROR(rtx_loop): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	/* lowered the reference count of the returned value */
	hawk_rtx_refdownval(rtx, retv);
	ret = 0;

oops:
	if (rtx) hawk_rtx_close(rtx); /* destroy the runtime context */
	if (hawk) hawk_close(hawk); /* destroy the hawk instance */
	return -1;
}
```

Embedding Hawk within an application involves a few key steps:

- Creating a Hawk Instance: The `hawk_openstd()` function is used to create a new instance of the Hawk interpreter, which serves as the entry point for interacting with Hawk from within the application.
- Parsing Scripts: The application can provide Hawk scripts as string literals or read them from files using the `hawk_parsestd()` function. This associates the scripts with the Hawk instance for execution.
- Creating a Runtime Context: A runtime context is created using `hawk_rtx_openstd()`, encapsulating the state and configuration required for script execution, such as input/output streams and other settings.
- Executing the Script: The `hawk_rtx_loop()` or `hawk_rtx_exec()` functions are used to execute the Hawk script within the created runtime context, returning a value representing the result of the execution.
- Handling the Result: The application can check the returned value for successful execution and handle any errors or results as needed.
- Cleaning Up: Finally, the application cleans up by closing the runtime context and destroying the Hawk instance using `hawk_rtx_close()` and `hawk_close()`, respectively.

By following this pattern, applications can seamlessly embed the Hawk interpreter, leveraging its scripting capabilities and data manipulation functionality while benefiting from its portability, efficiency, and extensibility.

Assuming the above sample code is stored in `hawk02.c` and the built Hawk library has been installed properly, you may compile the sample code by running the following commands:

```sh
$ gcc -Wall -O2 -o hawk02 hawk02.c -lhawk
```

The actual command may vary depending on the compiler used and the `configure` options used.

## Embedding Signal Handling

Signal handling is provided via a callback hook. The core library does not install OS-level signal handlers; the embedding application must do that and notify the runtime.

- Register a runtime callback with `hawk_rtx_pushecb()` and fill `hawk_rtx_ecb_t.sigset` (type `hawk_rtx_ecb_sigset_t`).
- Your callback is invoked when `sys::signal()` sets or clears a handler; use it to install or reset the OS signal handler.
- When the OS handler runs, call `hawk_rtx_raisesig()` on the target runtime.

Example:

```c
static hawk_rtx_t* g_rtx = HAWK_NULL;

static void on_os_signal (int sig)
{
	if (g_rtx) hawk_rtx_raisesig(g_rtx, sig);
}

static void on_sigset (hawk_rtx_t* rtx, int sig, hawk_fun_t* fun)
{
	if (fun)
	{
		g_rtx = rtx;
		signal(sig, on_os_signal);
	}
	else
	{
		if (g_rtx == rtx) g_rtx = HAWK_NULL;
		signal(sig, SIG_DFL);
	}
}

hawk_rtx_ecb_t ecb;
memset(&ecb, 0, HAWK_SIZEOF(ecb));
ecb.sigset = on_sigset;
hawk_rtx_pushecb(rtx, &ecb);
```

# Embedding Hawk in C++ Applications

Hawk can also be embedded in C++ applications. Here's an example:

```c++
#include <Hawk.hpp>
#include <stdio.h>

int main ()
{
	HAWK::HawkStd hawk;

	if (hawk.open() <= -1)
	{
		fprintf(stderr, "unable to open hawk - %s\n", hawk.getErrorMessageB());
		return -1;
	}

	HAWK::HawkStd::SourceString s("BEGIN { print \"hello, world\"; }");
	if (hawk.parse(s, HAWK::HawkStd::Source::NONE) == HAWK_NULL)
	{
		fprintf(stderr, "unable to parse - %s\n", hawk.getErrorMessageB());
		hawk.close();
		return -1;
	}

	HAWK::Hawk::Value vr;
	hawk.loop(&vr);  // alternatively, hawk.exec(&vr, HAWK_NULL, 0);

	hawk.close();
	return 0;
}
```

Embedding Hawk within a C++ application involves the following key steps:

- Creating a Hawk Instance: Create a new instance of the Hawk interpreter using the `HAWK::HawkStd` class.
- Parsing Scripts: Provide Hawk scripts as strings using the `HAWK::HawkStd::SourceString` class, and parse them using the `hawk.parse()` method.
- Executing the Script: Use the `hawk.loop()` or `hawk.exec()` methods to execute the Hawk script, returning a value representing the result of the execution.
- Handling the Result: Handle the returned value or any errors that occurred during execution.
- Cleaning Up: Clean up by calling `hawk.close()` to destroy the Hawk instance.

The C++ classes are inferior to the C equivalents in that they don't allow creation of multiple runtime contexts over a single hawk instance.

# Language

## What Hawk Is

Hawk is an embeddable awk interpreter with extensions. It can run hawk/awk scripts from the CLI or from C/C++ and provides modules like `str::`, `sys::`, `digest::`, `json::`, `sed::`, `ffi::`, `mysql::`, and `sqlite::`.


## Running Hawk

Run a script file:

```sh
$ hawk -f script.hawk input.txt
```

Run an inline program:

```sh
$ echo "a,b,c" | hawk 'BEGIN{FS=","} {print $2}'
```

### Cut, Sed, and M4 command modes

The `hawk` executable also provides embedded cut, sed, and m4 processors. The
mode switch must be the first option so that subsequent arguments are
interpreted by the selected processor.

Use `--cut` with a selector and optional input files. A selector beginning with
`f` chooses fields, `c` chooses character positions, `d` sets the input and
output delimiter, and `D` sets separate input and output delimiters. Positions
start at 1 and may be written as single numbers or ranges.

```sh
$ printf 'alpha,beta,gamma\n' | hawk --cut 'd, f1 f3'
alpha,gamma
$ printf 'abcdef\n' | hawk --cut 'c2-4'
bcd
```

Pass a selector directly, with `-e selector`, or load one from a file with
`-f selector-file`. Use `-o file` to redirect the result to a file.

Use `--sed` with a script argument, `-e script`, or `-f script-file`:

```sh
$ hawk --sed 's/foo/bar/g' input.txt
$ hawk --sed -n -e '/ERROR/p' application.log
```

Sed preserves input line endings by default. Use `--input-line-ending=auto`
(the default), `lf`, or `crlf` to select input terminators. `auto` recognizes
LF and CRLF; `lf` excludes only LF from editable text, making a preceding CR
available to substitutions, addresses, and translations. `crlf` recognizes
only CRLF record boundaries and keeps lone LF characters in the record.

Use `--output-line-ending=preserve` (the default), `native`, `lf`, or `crlf`
to select output formatting. Explicit conversion applies to emitted text,
including embedded newlines, generated text, and `w` output files. A missing
final newline stays missing, and bare CR characters are retained. With LF-only
input, CR is content and is retained even before a converted newline; remove
it explicitly if needed:

```sh
$ hawk --sed --output-line-ending=native '' input.txt
$ hawk --sed --input-line-ending=lf --output-line-ending=lf 's/\r//g' input.txt
```

The C API exposes the same policies through `hawk_sed_setopt()` using
`HAWK_SED_INPUT_LINE_ENDING` and `HAWK_SED_OUTPUT_LINE_ENDING`, with values of
`hawk_sed_input_line_ending_t` and `hawk_sed_output_line_ending_t`, respectively.

Use `--m4` with files or standard input. `-D`/`--define` defines a macro,
`-U`/`--undefine` undefines one, and `-I`/`--incdirs` adds an include directory;
these options may be repeated.

```sh
$ printf 'PROJECT\n' | hawk --m4 -DPROJECT=hawk
$ hawk --m4 -I ./macros document.m4
```

Run `hawk --cut --help`, `hawk --sed --help`, or `hawk --m4 --help` for all
mode-specific options.

### Embeddable shell and `--sh`

`hawk --sh -c 'command'` runs the Hawk simple-command shell. Use
`hawk --sh script.sh` or feed commands on standard input. Terminal input
automatically enables interactive mode; `-i` enables it explicitly.
The standard process backend currently requires POSIX (Linux, BSD, macOS,
Haiku, etc.); other platforms report `HAWK_ENOIMPL`.

```sh
hawk --sh -c 'echo hello | tr a-z A-Z > result.txt; cat < result.txt'
hawk --sh -c 'addcmd copy /bin/cat; mode registered; echo hello | copy'
```

`mode path` uses normal executable/PATH lookup. `mode registered` requires
external commands to have been registered, with `addcmd name /absolute/path`
or `hawk_sh_addcmd()`. `delcmd name` removes a registration. Core builtins and
registered callbacks work in either mode. This is **not a security sandbox**:
`mode` can switch back, and registered programs retain their OS privileges.
Select the initial mode with `--path` or `--registered`.

The grammar supports concurrent pipelines (`|`), command separators,
`&&`/`||`, background execution (`&`), quotes, escapes and comments.
Redirections include `<`, `>`, `>>`, `<>`, descriptor duplication/closure
(`2>&1`, `0<&3`, `1>&-`), and ordered multiple redirections. Descriptor numbers
range from 0 to 255. Heredocs support `<<`, quoted delimiters (no expansion),
and `<<-` (strip leading tabs); their size is not limited by a pipe buffer.
Variables support `$NAME`, `${NAME}`, `$?`, `$$`, and `$!`.
`export`, `unset`, assignment-only commands and command-prefix assignments
provide environment values; `cd` and environment changes are per instance,
not changes to the embedding application's working directory/environment.

Interactive job control uses process groups and terminal ownership:
Ctrl-Z stops a foreground job; `jobs`, `fg %1`, `bg %1`, `wait %1` (or a PID),
and `kill -TERM %1` manage jobs. `exit`/close terminates and reaps outstanding
jobs; background jobs do not detach from an embedded instance.
`echo`, `pwd`, `true`, `false`, and `:` are also builtins.

The core (`lib/sh.c`, `hawk-sh.h`) uses Hawk memory/error/character management
and configurable input/execution/byte-stream/environment callbacks. Standard
callbacks live in `lib/std-sh.c`. `hawk_sh_open()` accepts an allocator,
character manager, and extension size; `hawk_sh_openstd()` installs the POSIX
backend. Registered C callbacks use `hawk_sh_read()`/`hawk_sh_write()` for
redirectable I/O. Foreground standalone callbacks run in the parent; pipeline
and background callbacks run in fork children, so their memory changes do not
propagate back. Interactive mode owns process signal dispositions and the
terminal and is intended for standalone main-thread use. The host must not
independently reap the backend's children. Custom input callbacks can provide
completion or other UI features without coupling them to the core.
The standard backend forks; allocators and callbacks used in fork children
must be suitable for the application's threading/fork policy. An embedder
needing another process model can supply its own execution callbacks.

For `hawk_pio_open()`, opt into `HAWK_PIO_HAWKSH` to execute commands through
this engine without invoking `/bin/sh -c`. Existing `HAWK_PIO_SHELL` behavior
is unchanged. `HAWK_PIO_HAWKSH` cannot be combined with `HAWK_PIO_FNCCMD`.

This is not yet a complete POSIX shell: loops, `if`/`case`, shell functions,
subshell groups, globbing, field splitting, positional parameters, and command
substitution are not implemented. Parameter expansion currently preserves each
input word as one argument.

### Go HTTP server mode

`hawkgo --httpd` runs an HTTP application server using Go's standard library.
Repeat `--listen` for multiple HTTP endpoints. For HTTPS, use `--tls-listen`
with `--tls-cert` and `--tls-key`. Static files come from `--root`; configured
script routes take precedence, with the longest matching URL prefix selected.
Use `--script-root` (`HawkHttpdConfig.ScriptRoot`) as the base for relative
route scripts and dependency paths. A relative script root is resolved against
the working directory once when the server is created. An empty script root
preserves working-directory resolution; absolute script and dependency paths
are used directly. The static document root remains separate.

```sh
hawkgo --httpd --listen 127.0.0.1:8080 --listen '[::1]:8080' \
  --root ./public --script-root ./scripts --route /api/=handler.hawk
```

For example, `scripts/handler.hawk` can contain:

```awk
function handle(request)
{
	print "Hello", request["path"];
}
```

The request map contains `method`, `path`, `raw_query`, `host`,
`remote_address`, `scheme`, `headers`, `query`, and a binary `body`.
Header names follow Go's HTTP canonicalization. Header and query values are
arrays starting at index 1, preserving repeated values. Unredirected `getline`
reads the request body as records, `getbline` reads byte records, and `print` /
`printf` produce the response body. `RS`, `FS`, and `ORS` retain their normal
meaning, so record processing can change delimiters. Explicit file and pipe
redirections continue using their named destinations.

The entry function does not need to return a response map. Returning a map is
still supported for response `status`, `headers`, and an optional `body`, which
is appended after console output. Response headers accept strings or arrays of
strings; the returned body accepts text or a byte string. Other return values
are ignored. The default status is 200, and the content type is detected from
the body when not supplied.
`--entry` selects a function other than `handle`; this calls the function
directly rather than executing `BEGIN`, pattern-action, or `END` blocks.

Request bodies are read and size-checked before execution. Output is buffered
until execution succeeds or the script calls `http::flush()`. Before commitment,
console errors, including those tolerated by Hawk's I/O operations, discard
output and produce HTTP 500. After commitment, errors are logged and execution
stops without replacing the response. An oversized request produces HTTP 413.
Automatic interpreter flush requests do not commit the HTTP response.
`--max-response-bytes` counts all output, including bytes already flushed.

Parsed scripts are cached, while each request receives a fresh runtime.
`--reload-interval` controls change checks; failed reloads retain the previous
working script. Embedding applications can list included files in each route's
`Dependencies` for reload detection; includes are not discovered automatically.
`--max-concurrent`, `--max-body-bytes`, and `--max-response-bytes` bound script
requests and buffered bodies. Keep scripts outside the static document root;
`.hawk` files are excluded from static responses.

Embedding applications can construct independent `HawkHttpd` instances with
`NewHawkHttpd(context, config)`. Each instance owns multiple listeners, its
script cache, and runtime accounting. `Start()` binds every listener before
serving; `Addrs()` reports actual bound addresses. `ReqStop()` requests graceful
shutdown, `Shutdown(context)` waits with a deadline, and `Wait()` waits for all
tasks and interpreter cleanup. A shutdown deadline does not interrupt running
Hawk code or free its runtime. Request cancellation currently stops queued
requests but does not interrupt an executing script. Instances have one
start/stop lifecycle. Use `hawkgo --httpd --help` for command-line options.

### HTTP request and response helpers

The optional C `http::` module works with HawkHttpd and other hosts that attach
an HTTP context to the runtime. It is enabled by default; configure with
`--enable-mod-http=no` to omit it or `--enable-mod-http=yes` to build it dynamically.

| Function | Behavior |
| --- | --- |
| `http::getmethod()` | Request method, such as `GET` or `POST`. |
| `http::getpath()` | Request URL path. |
| `http::getheader(name [, index])` | Request header occurrence, indexed from 1; default 1. Missing values return an empty string. |
| `http::getheaders()` | Snapshot of request headers as a map of arrays indexed from 1. |
| `http::setstatus(code)` / `http::getstatus()` | Set/read response status; integer codes 200–599 are accepted. |
| `http::setheader(name, value)` | Replace a response header's values. |
| `http::addheader(name, value)` | Append a response header value, including repeated `Set-Cookie` fields. |
| `http::delheader(name)` | Remove a response header. |
| `http::flush()` | Commit status and headers and flush current console output. |

Setters and `flush()` return 0 on success. Invalid arguments, missing host
contexts, host failures, and status/header changes after commitment cause a
runtime error. Header names must be nonempty ASCII tokens; values must be
strings without NUL or control characters except tab. In HawkHttpd, names are
case-insensitive, `Host` is included in request getters, and the default status
is 200. Returned response-map fields can still supply metadata before
commitment and append a body; an omitted field preserves the context's value.

```awk
function handle(request)
{
	http::setstatus(200);
	http::setheader("Content-Type", "text/plain; charset=utf-8");
	printf "%s %s\n", http::getmethod(), http::getpath();
	http::flush();
	while (getline > 0) print $0;
}
```

C hosts include the HTTP module's installed `hawk-http.h` (source:
`mod/hawk-http.h`), resolve `HAWK_HTTP_AUX_NAME` with `hawk_rtx_getauxid()`, and
register a `hawk_aux_cbs_t` with `hawk_rtx_setauxctx()` before execution. The
operation callback receives an HTTP command and a borrowed `hawk_http_arg_t`;
getters set the intrinsic return value with `hawk_rtx_setretval()`. Hosts supply
console I/O for bodies and keep response commitment state in their context,
rejecting status/header mutations after headers have been sent, including a
partially failing flush. Cleanup runs once on replacement, detachment, or
runtime close. This protocol requires no HTTP-specific core functions and
works with static or dynamic modules and C CGI/FastCGI hosts.

`NewHawkHttpd()` returns an unavailable error when configured with
`--enable-mod-http=no`; the ordinary Go Hawk binding remains usable.

### Generic aux services

`hawk-aux.h` provides a per-runtime bridge for embedding applications to register
services under numeric IDs. Each runtime has its own slots; multiple services
can coexist, and the same ID can have different contexts in different runtimes.
The default slot count is 64, accepting IDs 0 through 63. Resolve a service
name with `hawk_rtx_getauxid()` to avoid collisions. Names are case-sensitive
and copied on first use; reservations survive detachment until runtime close.
Live unnamed bindings are skipped. IDs can differ between runtimes, so resolve
names separately for each runtime and do not cache IDs in shared module state.
A full registry produces `HAWK_EBUFFULL`; out-of-range IDs produce `HAWK_EINVAL`.

```c
#include <hawk-aux.h>

hawk_uint16_t id;
hawk_aux_cbs_t callbacks = { application_call, application_close };

/* Check the return value of each operation for errors. */
hawk_rtx_getauxid(rtx, HAWK_T("application"), &id);
hawk_rtx_setauxctx(rtx, id, &callbacks, context);
hawk_rtx_auxcall(rtx, id, command, data);
hawk_rtx_setauxctx(rtx, id, HAWK_NULL, HAWK_NULL);
```

The operation callback receives `(rtx, command, data, context)`. Commands and
opaque data pointers are passed unchanged; the service defines their protocol,
and data may be NULL. A callback returns a nonnegative service result or -1
with a runtime diagnostic. The bridge supplies a diagnostic if a failing
callback omits one. It does not set Hawk function return values automatically.

`hawk_rtx_setauxctx()` copies the callbacks and borrows the context, which may
be NULL. Registration requires no allocation because slots are stored in the
runtime. It returns 0 on success or -1 on error. Optional cleanup runs once on
replacement, detachment, or runtime close. Failed registration preserves the
old service and leaves ownership of the proposed context with the caller.
Detach with NULL callbacks; detaching an empty slot is harmless.

`hawk_rtx_getauxctx(rtx, id, &callbacks, &context)` retrieves a binding snapshot,
returning 1 when found, 0 when absent, and -1 on error. Output pointers are
optional. An absent slot leaves outputs and the existing diagnostic unchanged;
a registered NULL context still returns 1. The borrowed context remains valid
until replacement, detachment, or runtime close.

Do not access one runtime concurrently. An active operation callback cannot
replace or detach its own slot. While any aux cleanup callback runs, lookups,
dispatch, and registration changes for every ID on that runtime are rejected
with `HAWK_EPERM`. During runtime teardown this protection remains active through
the rest of runtime destruction. Cleanup callbacks must not execute scripts
or close the runtime.

The HTTP module and its hosts resolve the same `HAWK_HTTP_AUX_NAME`
(`"http"`), declared in `hawk-http.h`. Its command and argument definitions and
response commitment rules belong to the module's protocol, while registration
and dispatch use the generic aux API.

### Go console I/O callbacks

Before executing a Go runtime, use `rtx.SetConsoleIO(reader, writer)` to connect
its console input/output to any standard Go `io.Reader` and `io.Writer`.
Unredirected `getline` reads from that reader; `print` and `printf` write to that
writer. Explicit file and pipe redirections retain their normal handlers.
Text passes through Hawk's character manager, including multibyte characters
split across reads, while byte strings are transferred unchanged.

The streams belong to the caller: closing the runtime neither closes them nor
flushes the caller's buffers. A nil reader supplies EOF, and a nil writer
discards output. An optional third argument supplies a `func() error` flush
callback, for example `rtx.SetConsoleIO(reader, writer, writer.Flush)` for a
buffered writer. Hawk also requests flushing after `printf` and at execution
completion, so omit that callback when the host needs to decide whether to
commit or discard buffered output.

Check `rtx.ConsoleError()` for console failures, including errors handled by
`getline` or tolerated by `print`/`printf`. Configure console I/O before the
first `Exec`, `Loop`, or `Call`; each runtime owns its callback state and must
not be used concurrently. This interface is independent of HTTP and can also
support other embedding applications.

## Execution Model

Hawk follows the awk pipeline:

- Input is read as records (usually lines). `RS` controls record separation.
- Each record (`$0`) is split into fields `$1`, `$2`, ... by `FS`.
- A script is a sequence of `pattern { action }` blocks.
- `BEGIN` runs before input; `END` runs after input.

Example:

```awk
BEGIN { FS=","; print "start" }
$3 ~ /ERR/ { print NR, $1, $3 }
END { print "done", NR }
```

### @pragma entry

Hawk can override the default `BEGIN`/pattern/`END` flow with a custom entry point:

```awk
@pragma entry main
function main(a, b) {
	print "entry:", a, b
}
```

Run:

```sh
$ hawk -f script.hawk one two
entry: one two
```

## Values and Types

Hawk is dynamically typed:

- Numbers: integer and floating-point.
- Strings: Unicode text.
- Characters can be written with single quotes (e.g., `'A'`) or with double quotes prefixed with `@c` (e.g. `@c"A"`).
- Byte strings: raw bytes (`@b"..."`).
- Byte characters use `@b'X'` or `@bc"X"` and must fit in a single byte.
- Containers: array, map.
- `@nil` represents null.
- `@true` represents true.
- `@false` represents false.

Examples:

```awk
BEGIN {
	a = 10
	b = 3.14
	s = "hello"
	c = 'X'
	c2 = @C"X"
	bc = @b'x'
	bc2 = @bc"x"
	bs = @b"\x00\x01"
	m = @{"k": 1, "j": "hello"}
	arr = @["x", "y"]
	print hawk::typename(a), hawk::typename(b), hawk::typename(s), hawk::typename(c), hawk::typename(c2), hawk::typename(bc), hawk::typename(bc2), hawk::typename(bs), hawk::typename(m), hawk::typename(arr)
}
```

## Expressions and Operators

### Arithmetic and Comparison

- Arithmetic: `+`, `-`, `*`, `/`, `\`(integer division), `%`(modulo), `**` or '^'(exponentiation), `++`, `--`, `<<`, `>>`.
- Compound assignment: `=`, `+=`, `-=`, `*=`, `/=`, `\=`, `%=`, `^=` or `**=`, `%%=`.
- Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`.
- Type-precise compare: `===` and `!==`.

Example:

```awk
BEGIN {
	x = 10 + 5 * 2
	x += 3
	s = "ha"
	s %%= "wk"
	if (x >= 20) print x
	print s
	if ("10" === 10) print "no"
}
```

### Strings and Regex

- Concatenation by adjacency: `"a" "b"`.
- Explicit concatenation: `"a" %% "b"`.
- Regex match: `~` and `!~`.

Example:

```awk
BEGIN {
	print "hi" %% "!"
	if ("A" ~ /^[A-Z]$/) print "regex ok"
}
```

### Logical Operators

- Logical AND/OR: `&&`, `||`.
- Boolean results are numeric (`0` or `1`).

Example:

```awk
BEGIN {
	if (1 && 0) print "no"; else print "ok"
}
```

### Bitwise Operators

- Bitwise AND/OR/XOR: `&`, `|`, ^^
- Unary bitwise negation: ~
- Compound bitwise assignment: `&=`, `|=`, `^^=`, `<<=`, `>>=`
- `|` also denotes pipes, so use parentheses when you mean bitwise OR.
- `>>` is also used for append redirection; use parentheses when you mean right shift.

Bitwise OR vs pipe example:

```awk
BEGIN {
	print (1 | 2)  # bitwise OR => 3
	print 1 | 2    # pipe to external command "2"
}
```

## Variables and Scope

- Variables are created on assignment.
- `@local` and `@global` declare scope explicitly.
- `@const` defines a compile-time constant evaluated at parse time.

You can initialize `@global` and `@local` at declaration time. Initializers are expressions and can reference earlier constants and globals. `@const` values are fixed and cannot be reassigned.

Example:

```awk
@const C = 5
@global g = 10 + C, h
BEGIN {
	@local x = g + 1, y
	print x, g
	x = 1
	g = 2
	print x, g
}
```

## Arrays and Maps

Hawk supports arrays and maps.

- Arrays are indexed by numbers.
- Maps accept string and numeric keys.
- Constructors: `@[]`, `@{}`, `hawk::array()`, `hawk::map()`.
- All constructors accept initial values.

Example:

```awk
BEGIN {
	arr = @["a", "b", "c"]
	m = @{"k": "v", 10: "ten"}
	arr[4] = "d"
	m["x"] = 99
	print arr[1], m["k"], m[10]
}
```

## Functions

Define functions with `function name(...) { ... }`. You may use the short keyword `func` instead of `function`.

- Missing args are `@nil`.
- Use `&` for call-by-reference.
- Use `...` for varargs and access them via `@argc` and `@argv`.
- Functions are first-class values and can be passed as parameters (e.g., a comparator for `asort`).

Example:

```awk
function inc(&x) { x += 1 }
function greet(name) { if (name == "") name = "world"; print "hi", name }
BEGIN { n = 1; inc(n); greet(); greet("hawk"); print n }
```

Varargs example:

```awk
function dump(...) {
	@local i
	for (i = 0; i < @argc; i++) print @argv[i]
}
BEGIN { dump("a", 10, "b") }
```

Function-parameter example:

```awk
function desc(a, b) { return b - a }
BEGIN {
	@local a, b, i
	a = @[3, 1, 2]
	asort(a, b, desc)
	for (i in b) print i, b[i]
}
```

### Function Literals

Hawk supports unnamed function literals with `function(...) {...}` or `func(...) {...}`.
You can assign them to `@global`, `@local`, implicit/plain variables, and indexed elements.

- Function literals do not capture outer local state (no closure).
- They behave like normal user-defined functions used as values.

Example:

```awk
@pragma xcall on

@global gf = func(a, b) { return a + b }

BEGIN {
	@local lf = func(x) { return x * 2 }
	pf = func(z) { return z - 1 }      # plain/implicit variable
	m = @{}
	m["call"] = func(v) { return v * v }  # indexed assignment
	m.call2 = func(v) { return v + 100 }  # dot-notation indexed assignment

	print gf(10, 20), lf(7), pf(9), m["call"](5), m.call2(5)
}
```


## Control Flow

Hawk supports standard awk control flow.

### if / else

```awk
{ if ($1 > 0) print $1; else print "skip" }
```

### while

```awk
BEGIN {
	i = 1
	while (i <= 3) { print i; i++ }
}
```

### do ... while

```awk
BEGIN {
	i = 0
	do { print i; i++ } while (i < 3)
}
```

### for

```awk
BEGIN {
	for (i = 1; i <= 3; i++) print i
}
```

### for (i in array)

```awk
BEGIN {
	arr = @["x", "y"]
	for (i in arr) print i, arr[i]
}
```

### in operator (key existence)

Use `x in b` to test if a key/index exists in a map or array.

```awk
BEGIN {
	b = @{"k": 1}
	if ("k" in b) print "yes"
}
```

### switch

```awk
BEGIN {
	x = 2
	switch (x) {
	case 1: print "one"; break;
	case 2: print "two"; break;
	default: print "other";
	}
}
```

### break / continue / return / exit / @abort

```awk
BEGIN {
	for (i = 1; i <= 5; i++) {
		if (i == 3) continue
		if (i == 5) break
		print i
	}
	exit 0
}
```

`exit` terminates a script, but `END` blocks are still executed if it's not called from an `END` block.

```awk
BEGIN {
	print "BEGIN"
	exit 99
}
END {
	print "END" ## this is executed
}
```

`@abort` terminates a script immediately, skipping `END` blocks if it's not called from an `END` block.

```awk
BEGIN {
	print "BEGIN"
	@abort 99
}
END {
	print "END" ## this is not executed
}
```

Note: Hawk allows `return` inside `BEGIN` and `END` blocks, in addition to functions.

### nextfile / nextofile

`nextfile` skips the rest of the current input file (standard awk behavior). `nextofile` advances to the next output file specified with `-t`.

Example:

```sh
$ hawk -t /tmp/1 -t /tmp/2 'BEGIN { print 10; nextofile; print 20 }'
```

This writes `10` to `/tmp/1` and `20` to `/tmp/2`.


## Input, Output, and Pipes

- `getline` reads records.
- `getbline` reads records as bytes.
- `getline`/`getbline` return `1` on success, `0` on EOF, and `-1` on error.
- Redirection works with `<`, `>`, and `>>`.
- Pipes: `cmd | getline var` and `print x | "cmd"`.
- Two-way pipes: `|&`
- CSV-style field splitting is supported when `FS` begins with `?` followed by four characters (separator, escaper, left quote, right quote).

Example:

```awk
BEGIN {
	while (("ls -laF" | getline x) > 0) print "\t", x;
	close ("ls -laF");
}
```

Two-way pipe example:


```awk
BEGIN {
	cmd = "sort";
	data = hawk::array("hello", "world", "two-way pipe", "testing");

	for (i = 1; i <= length(data); i++) print data[i] |& cmd;
	close(cmd, "to");

	while ((cmd |& getline line) > 0) print line;
	close(cmd);
}
```

Redirection examples:

```awk
BEGIN {
	while ((getline line < "input.txt") > 0) print line > "out.txt"
	print "more" >> "out.txt"
}
```

Byte-record example:

```awk
BEGIN { getbline b < "bin.dat"; print str::tohex(b) }
```

CSV-style `FS` example:

```awk
BEGIN { FS="?,\"\"\""; }
{ for (i = 0; i <= NF; i++) print i, "[" $i "]"; }
```

This example splits `hawk,can,read,"a ""CSV"" file",.` to 5 fields.
- hawk
- can
- read
- a "CSV" file
- .

## Built-in Variables

Common built-ins:

- `NR`, `FNR`, `NF`
- `FS`, `RS`, `OFS`, `ORS`
- `FILENAME`, `OFILENAME`
- `ARGC`, `ARGV`
- `ENVIRON`
- `PIPECLOEXEC`

Example:

```awk
{ print NR, NF, $0 }
```

Runtime-specific predefined variables:

| Name | Type | Description |
|------|------|-------------|
| `ARGC` | int | number of command-line arguments available via `ARGV` |
| `ARGV` | array | command-line argument vector. `ARGV[0]` is usually the program name |
| `ENVIRON` | map | process environment as a map. Values imported from the host environment may become `int`, `flt`, or `str` depending on content |
| `PIPECLOEXEC` | int/bool-like | controls whether file descriptors are opened with `CLOEXEC` when Hawk runs an external command for piping. `0` keeps the default inheritance behavior, `1` enables close-on-exec. This corresponds to `@pragma pipecloexec` |

`ENVIRON` affects child processes started by piping and `sys::system()`. Modifying it changes the environment passed to those children without changing the process-global environment of the embedding application.

Examples:

```awk
BEGIN {
	print ARGC, ARGV[0]
	print ENVIRON["HOME"]
	PIPECLOEXEC = 1
}
```

## Built-in Functions

Hawk includes awk built-ins (e.g., `length`, `substr`, `split`, `index`) plus extensions in modules (see below).

Example:

```awk
BEGIN { print length("hawk"), substr("hawk", 2, 2) }
```

## Pragmas

`@pragma` controls parser/runtime behavior. File-scope pragmas apply per file; global-scope pragmas appear once across all files.

| Name          | Scope  | Values        | Default | Description                                             |
|---------------|--------|---------------|---------|---------------------------------------------------------|
| entry         | global | function name |         | change the program entry point                          |
| implicit      | file   | on, off       | on      | allow undeclared variables                              |
| defermodsym   | file   | on, off       | off     | defer resolving `mod::symbol` references to runtime     |
| multilinestr  | file   | on, off       | off     | allow a multiline string literal without continuation   |
| pipecloexec   | global | on, off       | off     | set CLOEXEC when running an external command for piping |
| rwpipe        | file   | on, off       | on      | allow the two-way pipe operator `\|&`                   |
| striprecspc   | global | on, off       | off     | removes leading and trailing blank fields in splitting a record if FS is a regular expression mathcing all spaces |
| stripstrspc   | global | on, off       | on      | trim leading and trailing spaces when converting a string to a number |
| numstrdetect  | global | on, off       | off     | convert a numeric string to a value of a numeric type automatically |
| stack_limit   | global | number        | 5120    | specify the runtime stack size measured in the number of values |
| xcall         | file   | on, off       | off     | allow one-level function call after dot or bracket index access (e.g. `m.f(...)`, `m["f"](...)`) |

`stack_limit` sizes the heap-based value stack; it does not size the native C
stack. When the required stack-bound APIs are detected on Linux, macOS, FreeBSD,
NetBSD, OpenBSD, Haiku, or Windows, the runtime also checks native stack bounds
automatically, independently of this pragma and the logical recursion limit.
It leaves 64 KiB of headroom and returns `HAWK_ESTACK` (`native C stack
limit reached`) before recursive evaluation exhausts that space. Bounds are
refreshed at host execution entry, including callback reentry and sequential use
of a runtime context on different threads. Failure to obtain valid bounds after
the feature has been enabled is a runtime error.

These checks protect recursive runtime evaluation, not parsing or arbitrary
stack use inside native extensions, host callbacks, or library routines. Hosts
must enter Hawk with sufficient stack space for entry and error handling and
must not change the active stack limits during execution. Builds without the
required interfaces retain the value-stack and logical recursion limits.

### @pragma entry

Sets a custom entry function instead of the default `BEGIN`/pattern/`END` flow.

```awk
@pragma entry main;
function main () { print "hello, world"; }
```

Arguments passed on the command line are provided to the entry function:

```awk
@pragma entry main
function main(arg1, arg2) {
	print "Arguments:", arg1, arg2
}
```

```sh
$ hawk -f main.hawk arg1_value arg2_value
```

If you don't know the number of arguments in advance, use `...` and `@argv`/`@argc`:

```awk
@pragma entry main
function main(...) {
	@local i
	for (i = 0; i < @argc; i++) printf("%s:", @argv[i])
	print ""
}
```

```sh
$ hawk -f main.hawk 10 20 30 40 50
```

Named arguments can be combined with `...` to require a minimum number of parameters:

```awk
function x(a, b, ...) {
	print "a=", a, "b=", b, "rest=", (@argc - 2)
}
BEGIN { x(1, 2, 3, 4) }
```

### @pragma implicit

Controls implicit variable declaration. `off` requires `@local`/`@global`.

```awk
@pragma implicit off;
BEGIN {
    a = 10; ## syntax error - undefined identifier 'a'
}
```

In the example above, the `@pragma implicit off` directive is used to turn off implicit variable declaration. As a result, attempting to use the undeclared variable a will result in a syntax error.

```awk
@pragma implicit off;
BEGIN {
    @local a;
    a = 10; ## syntax ok - 'a' is declared before use
}
```

### @pragma defermodsym

Defers module symbol resolution to runtime.
This is useful when a script can reference optional modules that may not exist in every deployment.

```awk
@pragma defermodsym on
BEGIN {
	if (0) optionalmod::setup();
	print "startup complete";
}
```

For module functions and non-function symbols, unresolved names like `mymod::fn(...)` and `mymod::CONST` are checked when executed, not when parsed.

Use the CLI option to set the initial mode without editing scripts:

```sh
$ hawk --defermodsym on -f script.hawk
```

### @pragma striprecspc

When `FS` is a space-matching regex, this controls whether leading/trailing blank fields are removed.

- @pragma striprecspc on
```sh
$ echo '  a  b  c  d  ' | hawk '@pragma striprecspc on;
BEGIN { FS="[[:space:]]+"; }
{
    print "NF=" NF;
    for (i = 0; i < NF; i++) print i " [" $(i+1) "]";
}'
NF=4
0 [a]
1 [b]
2 [c]
3 [d]
```

- @pragma striprecspc off

``` sh
$ echo '  a  b  c  d  ' | hawk '@pragma striprecspc off;
BEGIN { FS="[[:space:]]+"; }
{
    print "NF=" NF;
    for (i = 0; i < NF; i++) print i " [" $(i+1) "]";
}'
NF=6
0 []
1 [a]
2 [b]
3 [c]
4 [d]
5 []
```

## @include and @include_once

`@include` inserts another file at parse time; the semicolon is optional. `@include_once` avoids duplicate inclusion.

```awk
function print_hello() { print "hello\n"; }
```

```awk
@include "hello.inc";
BEGIN { print_hello(); }
```

```awk
@include_once "hello.inc";
@include_once "hello.inc";
BEGIN { print_hello(); }
```

You can use them inside a block or at the top level:

```awk
BEGIN {
	@include "init.inc";
	...
}
```

## Comments

`Hawk` supports a single-line comment that begins with a hash sign # and the C-style multi-line comment.

```awk
x = y; # assign y to x.
/*
this line is ignored.
this line is ignored too.
*/
```

## Reserved Words

The following words are reserved and cannot be used as a variable name, a parameter name, or a function name.

 - @abort
 - @argc
 - @argv
 - @const
 - @false
 - @global
 - @include
 - @include_once
 - @local
 - @nil
 - @pragma
 - @reset
 - @true
 - BEGIN
 - END
 - break
 - case
 - continue
 - default
 - delete
 - do
 - else
 - exit
 - for
 - function
 - getbline
 - getline
 - if
 - in
 - next
 - nextfile
 - nextofile
 - print
 - printf
 - return
 - while
 - switch

However, some of these words not beginning with `@` can be used as normal names in the context of a module call. For example, `mymod::break`. In practice, the predefined names used for built-in commands, functions, and variables are treated as if they are reserved since you can't create another definition with the same name.

## Some Examples

- Print the first 10 even numbers
```awk
BEGIN {
	i = 0
	n = 1
	while (i < 10) {
		if (n % 2 == 0) {
			print n
			i++
		}
		n++
	}
}
```

- Prompt the user for a positive number
```awk
BEGIN {
	do {
		printf "Enter a positive number: "
		getline num
	} while (num <= 0)
	print "You entered:", num
}
```

- Print the multiplication table
```awk
BEGIN {
	for (i = 1; i <= 10; i++) {
		for (j = 1; j <= 10; j++) {
			printf "%4d", i * j
		}
		printf "\n"
	}
}
```

- Print only the even numbers from 1 to 16
```awk
BEGIN {
	for (i = 1; i <= 20; i++) {
		if (i % 2 != 0) {
			continue
		}
		print i
		if (i >= 16) {
			break
		}
	}
}
```

- Count the frequency of words in a file
```awk
{
	n = split($0, words, /[^[:alnum:]_]+/)
	for (i = 1; i <= n; i++) {
		freq[words[i]]++
	}
}

END {
	for (w in freq) {
		printf "%s: %d\n", w, freq[w]
	}
}
```

## Garbage Collection

The primary value management is reference counting based but `map` and `array` values are garbage-collected additionally.

## Modules

Hawk supports various modules.

### HTTP

The optional `http::` module provides request metadata, response status/header
control, and flushing for hosts with an attached HTTP context. See
[HTTP request and response helpers](#http-request-and-response-helpers) for
the function table, HawkHttpd example, and reusable C callback interface.

### Message Digest

The optional `digest::` module computes MD5, SHA-1, and SHA-256 message digests.
Digest functions return raw multi-byte strings; use `str::tohex()` when a
printable hexadecimal representation is required.

- `digest::md5(data)`, `digest::sha1(data)`, `digest::sha256(data)` compute a
  digest in one call.
- `digest::init(name)` creates a streaming context for `md5`, `sha1`, or
  `sha256`.
- `digest::update(context, data)` adds a byte-string chunk and returns the
  context.
- `digest::final(context)` finishes the context and returns the raw digest.

```awk
BEGIN {
	print str::tohex(digest::sha256(@b"hello"))
}
```

The streaming context is a private BOB value. It owns the digest state directly,
so it needs no explicit close operation. A context cannot be updated or finalized
again after `digest::final()`. Assigning a context to another variable creates an
alias to the same mutable state; an active context should not be used as a map key.

```awk
ctx = digest::init("sha256")
ctx = digest::update(ctx, first_chunk)
ctx = digest::update(ctx, second_chunk)
hash = str::tohex(digest::final(ctx))
```

### Hawk

- hawk::array
- hawk::call
- hawk::cmgr_exists
- hawk::func_exists
- hawk::function_exists
- hawk::gc
- hawk::gc_get_threshold
- hawk::gc_set_threshold
- hawk::gcrefs
- hawk::hash
- hawk::isarray
- hawk::ismap
- hawk::isnil
- hawk::map
- hawk::modlibdirs
- hawk::type
- hawk::typename
- hawk::GC_NUM_GENS

### JSON

The optional `json::` module converts between JSON text and Hawk values.
`json::parse(text)` returns maps, arrays, strings, numbers, booleans, or `@nil`.
`json::stringify(value)` serializes a supported Hawk value and escapes JSON
strings as needed.

```awk
BEGIN {
	data = json::parse("{\"name\":\"hawk\",\"items\":[1,2,3]}")
	data.items[4] = 4
	print data.name
	print json::stringify(data)
}
```

Invalid or incomplete JSON raises a runtime error. A JSON object becomes a Hawk
map, a JSON array becomes a Hawk array, and JSON `null` becomes `@nil`.

### M4

The optional `m4::` module processes m4 source held in a string or file:

```awk
BEGIN {
	source = "define(`NAME', `hawk')dnl\nNAME"
	rc = m4::process(source, output, m4::STR_TO_STR)
	if (rc == 0) print output
}
```

`m4::process(input, output, mode [, include-directories])` returns zero on
success. The input contains both macro definitions and text to expand. For a
string output mode, `output` must be a variable passed by reference; for a file
output mode, it is the destination path. Select the input/output interpretation
with one of these constants:

- `m4::STR_TO_STR`
- `m4::FILE_TO_STR`
- `m4::STR_TO_FILE`
- `m4::FILE_TO_FILE`

The optional fourth argument is a platform-specific path list searched by
`include()` and `sinclude()`:

```awk
BEGIN {
	rc = m4::process("document.m4", "document.txt",
	                 m4::FILE_TO_FILE, "./macros")
}
```

### Sed

The optional `sed::` module applies a sed script to string or file input:

```awk
BEGIN {
	rc = sed::process("s/foo/bar/g", "foo foo", output, sed::STR_TO_STR)
	if (rc == 0) print output
}
```

`sed::process(script, input, output, mode)` returns zero on success. For a
string output mode, `output` must be a variable passed by reference; for a file
output mode, it is the destination path. Select the input/output interpretation
with one of these constants:

- `sed::STR_TO_STR`
- `sed::FILE_TO_STR`
- `sed::STR_TO_FILE`
- `sed::FILE_TO_FILE`

### String
The `str` module provides an extensive set of string manipulation functions.

- str::BASE64_NOPAD - omit trailing `=` padding when encoding
- str::BASE64_URL - use the URL-safe Base64 alphabet
- str::frombase64 - decode a Base64 byte string; `str::BASE64_URL` selects the URL-safe alphabet, and padding is optional
- str::fromcharcode
- str::fromhex
- str::gsub - equivalent to gsub
- str::index
- str::isalnum
- str::isalpha
- str::isblank
- str::iscntrl
- str::isdigit
- str::isgraph
- str::islower
- str::isprint
- str::ispunct
- str::isspace
- str::isupper
- str::isxdigit
- str::length - equivalent to length
- str::ltrim
- str::match - similar to match. the optional third argument is the search start index. the optional fourth argument is equivalent to the third argument to match().
- str::normspace
- str::printf - equivalent to sprintf
- str::rindex
- str::rtrim
- str::split - equivalent to split
- str::sub - equivalent to sub
- str::substr - equivalent to substr
- str::tobase64 - encode data to a Base64 byte string; the optional argument combines `str::BASE64_URL` and `str::BASE64_NOPAD`
- str::tocharcode - get the numeric value of the first character
- str::tohex
- str::tolower - equivalent to tolower
- str::tonum - convert a string to a number. a numeric value passed as a parameter is returned as it is. the leading prefix of `0b`, `0` or `0o`, and `0x` specifies the radix of 2, 8, 16 respectively. conversion stops when the end of the string is reached or the first invalid character for conversion is encountered.
- str::toupper - equivalent to toupper
- str::trim


### System

The `sys` module provides various functions concerning the underlying operation system.

- sys::basename
- sys::chmod
- sys::close
- sys::closedir
- sys::closelog
- sys::dirname
- sys::dup
- sys::errmsg
- sys::fork
- sys::fseek
- sys::getegid
- sys::getenv
- sys::geteuid
- sys::getgid
- sys::getpid
- sys::getppid
- sys::gettid
- sys::gettime
- sys::getuid
- sys::kill
- sys::mkdir
- sys::mktime
- sys::open
- sys::opendir
- sys::openfd
- sys::openlog
- sys::pack
- sys::pipe
- sys::raise
- sys::read
- sys::readdir
- sys::setttime
- sys::signal
- sys::sleep
- sys::strftime
- sys::system
- sys::tempname
- sys::unlink
- sys::unpack
- sys::wait
- sys::write
- sys::writelog

#### Pseudo-terminals and terminal control

`sys::openpty(master, slave)` creates a pseudo-terminal pair and assigns two
ordinary `sys` file handles. It returns zero on success and leaves both output
arguments unchanged on failure. These handles work with `sys::read()`,
`sys::write()`, `sys::close()`, `sys::fcntl()`, and the mux functions.

For separate master/slave setup, use `sys::openpt(flags)`, followed by
`sys::grantpt(master)`, `sys::unlockpt(master)`, and
`sys::ptsname(master, pathname)`. Open the returned pathname with `sys::open()`:

```awk
BEGIN {
	master = sys::openpt(sys::O_RDWR | sys::O_NOCTTY);
	if (master < 0) { print sys::errmsg(); exit 1; }
	if (sys::grantpt(master) < 0 || sys::unlockpt(master) < 0 ||
	    sys::ptsname(master, pathname) < 0) {
		print sys::errmsg(); sys::close(master); exit 1;
	}
	slave = sys::open(pathname, sys::O_RDWR | sys::O_NOCTTY);
	if (slave < 0) { print sys::errmsg(); sys::close(master); exit 1; }
	sys::tcsetraw(slave);
	sys::write(slave, @b"hello");
	sys::read(master, text, 5);
	print text;
	sys::close(slave);
	sys::close(master);
}
```

`sys::isatty(handle)` returns one for a terminal and zero for another valid
file descriptor. `sys::ttyname(handle, pathname)` retrieves its pathname.
`sys::tcgetwinsize(handle, size)` returns a map with `rows`, `cols`, `xpixels`,
and `ypixels`. `sys::tcsetwinsize(handle, size)` updates the supplied fields,
preserving omitted fields. Unknown fields and values outside 0..65535 are
rejected before changing the terminal. Existing `tcgetattr()`, `tcsetattr()`,
and `tcsetraw()` control terminal modes.

In a forked child, `sys::login_tty(slave)` creates a session, establishes its
controlling terminal, redirects native descriptors 0, 1, and 2, and consumes
the slave handle on success. Close the child's master handle first. It calls
`setsid()` itself; calling `sys::setsid()` first would make the child a session
leader, which cannot create another session. A failed setup can leave session
or standard-descriptor changes in place; it retains the slave handle unless
setup completed and its final close
failed. Existing wrappers for redirected standard descriptors have their
buffered input and mux registrations discarded.

For example, run an interactive shell on the slave and exchange data through
the master:

```awk
BEGIN {
	if (sys::openpty(master, slave) < 0) exit 1;

	fflush();
	pid = sys::fork();
	if (pid < 0)
	{
		sys::close(master);
		sys::close(slave);
		exit 1;
	}

	if (pid == 0)
	{
		sys::close(master);

		if (sys::login_tty(slave) < 0) exit 1;

		rc = sys::system("exec /bin/sh -i");
		exit (rc == 0? 0: 1);
	}

	sys::close(slave);
	sys::write(master, @b"echo hello; exit\n");

	while (1)
	{
		n = sys::read(master, data);
		if (n == sys::RC_EINTR) continue;
		if (n <= 0) break;

		printf "%s", data;
	}

	sys::wait(pid, status);
	sys::close(master);
}
```

`sys::system()` starts another process that inherits the redirected standard
descriptors; the forked Hawk child waits for it. The shell's `exec` replaces
the command shell with `/bin/sh -i`. Both stdout and stderr reach the master
as a single stream. Keep normal slave terminal modes for interactive shells;
`tcsetraw()` disables terminal line editing and control-character handling.

Drain the master while the child runs, before waiting for termination, to
avoid blocking the child on a full output buffer. For ongoing interactive
use, drive input and output with the mux functions and handle partial writes.
A closed slave may produce EOF or `sys::RC_EIOERR`; other read errors should
be reported rather than treated as normal termination.

`sys::tcgetpgrp(handle)` gets the foreground process-group ID;
`sys::tcsetpgrp(handle, pgid)` changes it. `sys::setsid()` returns the new
session ID, `sys::setpgid(pid, pgid)` sets a process group, and
`sys::getpgid([pid])` queries one (zero or omitted means the current process).
Terminal access and foreground-group changes follow the OS job-control rules.
The PTY and terminal-control operations return negative `RC_*` values on
failure; use `sys::errmsg()` for details. Unavailable operations return
`sys::RC_ENOIMPL`. PTY creation is currently supported on POSIX platforms.

`sys::tempname([prefix])` atomically creates an empty file in the system
temporary directory, closes it, and returns its full path. It returns an empty
string on failure; call `sys::errmsg()` for the error text. The optional prefix
must not contain a path separator. Remove the file with `sys::unlink()` when it
is no longer needed.

`sys::opendir(path, flags)` accepts `sys::DIR_SORT` for sorted entry names and
`sys::DIR_SKIPSPCDIR` to exclude `.` and `..`. Combine them with `|`, for example
`sys::opendir(".", sys::DIR_SORT | sys::DIR_SKIPSPCDIR)`. The optional flags
default to zero. Path-encoding flags are rejected; paths and returned entry
names always use Hawk's configured character type.

#### Remote logging timestamps

`sys::openlog("remote://address/identifier", options, facility)` selects remote
datagram logging; `sock://` is also accepted. Add `sys::LOG_OPT_ISO8601` to `options`
to include the year and local UTC offset in each timestamp, including any
daylight-saving adjustment:

```awk
BEGIN {
	if (sys::openlog("remote://127.0.0.1:514/myapp",
		sys::LOG_OPT_ISO8601 | sys::LOG_OPT_PID | sys::LOG_OPT_NDELAY,
		sys::LOG_FAC_USER) < 0) {
		print sys::errmsg(); exit 1;
	}
	if (sys::writelog(sys::LOG_PRI_INFO, "started") < 0) print sys::errmsg();
	sys::closelog();
}
```

Example datagram: `<14>2026-10-10T15:30:00+09:00 myapp[1234]: started`.
UTC is written as `+00:00`. Without this option, timestamps retain the existing
`Oct 10 15:30:00` format. The option changes the timestamp only; it does not
select the complete [RFC 5424](https://www.rfc-editor.org/rfc/rfc5424.html) message
format. Local logging ignores this option and leaves timestamp formatting to
the native logging service. If the local year cannot fit four digits or the
offset cannot be represented as whole minutes within `-23:59` through `+23:59`,
`sys::writelog()` returns `sys::RC_EINVAL` and sets `sys::errmsg()`.

#### Time formatting

`sys::strftime(format, seconds[, flags])` formats a timestamp in local time;
`sys::STRFTIME_UTC` selects UTC. A format containing an embedded NUL character
returns `sys::RC_EINVAL` and sets `sys::errmsg()` instead of silently truncating
the format. An empty format is valid and produces an empty string.

#### Sleep durations

`sys::sleep(seconds)` accepts a nonnegative duration in seconds, including
fractional values. Actual delays depend on platform timer resolution and
scheduling.

On Windows and OS/2, durations are converted to integer milliseconds by
truncating fractional milliseconds. Positive durations below one millisecond,
such as `sys::sleep(0.0005)` (0.5 milliseconds) or `sys::sleep(0.00000005)`
(50 nanoseconds), therefore become a zero-duration platform call.

On Windows, a zero-duration call yields execution to another ready thread while
the calling thread remains ready to run. If no other thread is ready, it returns
immediately. A loop using sub-millisecond durations can therefore consume CPU
continuously. Use a duration of at least `0.001` seconds when requesting a timed
suspension on Windows; this does not guarantee an exact one-millisecond delay.

#### Signals

Use these to register handlers and raise signals from Hawk.

- sys::signal(sig, func) sets a handler function for a signal number. pass nil to clear it.
- sys::raise(sig) raises a signal to the current runtime.
- sys::kill(pid, sig) sends a signal to another process or to self.

Example:

```awk
function on_int(sig) { print "got", sig }

BEGIN {
	sys::signal(sys::SIGINT, on_int);
	sys::raise(sys::SIGINT);
	## sys::kill(sys::getpid(), sys::SIGINT);
	sys::signal(sys::SIGINT, @nil);
}
```


#### Raw byte reads

You may read the file in raw bytes.

```awk
BEGIN {
	f = sys::open("/etc/sysctl.conf", sys::O_RDONLY);
	if (f >= 0) {
		while(sys::read(f, x, 10) > 0) printf(@b"%s", x);
		sys::close(f);
	}
}
```

#### Buffered reads and file positions

`sys::open()` uses binary mode on Windows: reads and writes preserve CR, LF,
NUL, and `0x1A` bytes, and file positions count bytes without CRT translation.
`sys::openfd()` preserves an imported descriptor's CRT mode. On Windows, the
embedding application must configure that descriptor in binary mode before
wrapping it when exact byte I/O or buffered seeking is required.

When a delimiter is supplied to `sys::read(handle, output, limit, delimiter)`,
the function may read beyond the returned delimiter and keep the remaining
bytes in that handle's buffer. Each handle owns its buffer independently,
including handles created by `sys::openfd()` for the same raw descriptor and
handles created by `sys::dup()` that share the underlying file offset.

A successful `sys::fseek()` clears only the specified handle's buffered input.
Other handles retain their buffered bytes even though the shared file offset
has changed. Callers must coordinate access across such handles; use one handle
for buffered reading and seeking whenever possible.

If the OS seek succeeds but the resulting position exceeds Hawk's integer
range, `sys::fseek()` returns `sys::RC_EINVAL` with `seek position out of range`.
The file position has already changed and that handle's buffered input is cleared.

`sys::write()` uses the underlying descriptor's current offset without
adjusting for unread buffered input. On a seekable file, call
`sys::fseek(handle, 0, sys::SEEK_CUR)` on the same handle before switching from
buffered reading to writing. This backs up by the unread byte count and clears
the buffer, positioning the next write immediately after the bytes returned
to the caller, unless the file was opened with `sys::O_APPEND`. Check that the
seek succeeds before writing.

```awk
BEGIN {
	f = sys::open("data.bin", sys::O_RDWR);
	if (f < 0) { print sys::errmsg(); exit 1; }
	if (sys::read(f, line, 8192, "\n") > 0) {
		if (sys::fseek(f, 0, sys::SEEK_CUR) >= 0)
			sys::write(f, @b"replacement");
		else print sys::errmsg();
	}
	sys::close(f);
}
```

#### Wrap a file descriptor

You can map a raw file descriptor to a handle created by this module and use it.

```awk
BEGIN {
	a = sys::openfd(1);
	sys::write(a, @b"let me write something here\n");
	sys::close(a, sys::C_KEEPFD); ## set C_KEEPFD to release 1 without closing it.
	##sys::close(a);
	print "done\n";
}
```

#### I/O multiplexing

`sys::openmux()` selects epoll, kqueue, poll, or select, in that order of
availability. Add handles with `sys::addtomux(m, handle, events)`, wait with
`sys::waitonmux(m, timeout_seconds)`, and retrieve each event using
`sys::getmuxevt(m, index, handle, events)`. A negative timeout waits indefinitely;
zero polls without blocking. `sys::modinmux()` changes a watch and
`sys::delfrommux()` removes it. Close the instance with `sys::closemux()`.

Use `sys::MUX_EVT_IN` and `sys::MUX_EVT_OUT` for readiness. Add
`sys::MUX_EVT_ONESHOT` to disable a registration after its first event; modify
it to rearm. Error and hangup events are reported where the backend supports
them. Select reports EOF as readable input. On Windows, only sockets can be
watched; POSIX select rejects descriptors at or above `FD_SETSIZE`.

The standalone C interface is in `hawk-mux.h`. It accepts Hawk memory and
character managers, an extension size, and an optional explicit backend
selection. Events contain copied 64-bit registration tokens rather than
pointers into the interpreter's handle table.

#### Multiplexer cleanup and fork

`sys::addtomux(mux, handle, events, flags)` accepts an optional fourth argument,
defaulting to zero. `sys::MUX_FLAG_NOAUTODEL` makes Hawk skip automatic
native watch deletion when closing the watched handle, closing the mux, or cleaning
up the runtime. Local membership and cached events are still cleared.
`sys::modinmux()` preserves this setting; `sys::delfrommux()` explicitly deletes
the registration regardless of the flag. Unknown flag bits are rejected.

If explicit deletion fails in the OS, `sys::delfrommux()` returns a negative
error code and sets `sys::errmsg()`. Local membership and cached events are
still cleared. The next add, modify, or wait operation attempts to rebuild
the multiplexer without the deleted registration.

```awk
sys::addtomux(m, fd, sys::MUX_EVT_IN, sys::MUX_FLAG_NOAUTODEL);
```

After `sys::fork()`, inherited epoll instances remain shared. Set this flag
before forking if child cleanup must leave the parent's watches alone.
Without it, child cleanup can delete shared registrations and stop the parent
receiving their events. Explicit deletion and one-shot rearming also affect
the shared instance. Cached events and handle tables are copied per process;
coordinate inherited mux use, or create separate muxes after forking for
independent registrations.

Other backends follow their native inheritance rules: poll/select watch tables
are copied into the child, and kqueue descriptors are not inherited on systems
such as FreeBSD. Create a new mux in the child when using those backends.

The flag does not keep descriptors open: use `sys::C_KEEPFD` with `sys::close()`
for that. Kernel descriptor-lifetime rules still apply, and stale-event repair
may rebuild a mux without registrations that have been locally removed.

#### Pipe to a child process

`sys::system(command)` waits for the command and returns its exit status.
On POSIX, normal statuses are 0 through 255; signal termination returns
256 plus the signal number. Windows statuses are unsigned 32-bit values,
including 256 and 259. If a status exceeds Hawk's signed integer range,
the function returns `sys::RC_EINVAL` and sets `sys::errmsg()` to describe
the range error. Other launch or wait failures return the corresponding
negative module error code.

Creating pipes and sharing them with a child process is not big an issue.

```awk
BEGIN {
	if (sys::pipe(p0, p1, sys::O_CLOEXEC | sys::O_NONBLOCK) <= -1)
	##if (sys::pipe(p0, p1, sys::O_CLOEXEC) <= -1)
	##if (sys::pipe(p0, p1) <= -1)
	{
		print "pipe error";
		return -1;
	}
	a = sys::fork();
	if (a <= -1) 
	{
		print "fork error";
		sys::close(p0);
		sys::close(p1);
	}
	else if (a == 0)
	{
		## child
		printf("child.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close(p1);
		while (1)
		{
			n = sys::read(p0, k, 3);
			if (n <= 0) 
			{
				if (n == sys::RC_EAGAIN) continue; ## nonblock but data not available
				if (n != 0) print "ERROR: " sys::errmsg();
				break;
			}
			print k;
		}
		sys::close(p0);
		return 123;
	}
	else
	{
		## parent
		printf("parent.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close(p0);
		sys::write(p1, @b"hello");
		sys::write(p1, @b"world");
		sys::close(p1);

		##sys::wait(a, status, sys::WNOHANG);
		while (sys::wait(a, status) != a);
		if (sys::WIFEXITED(status)) print "Exit code: " sys::WEXITSTATUS(status);
		else print "Child terminated abnormally"
	}
}
```

#### Read child stdout

You can read standard output of a child process in a parent process.

```awk
BEGIN {
	if (sys::pipe(p0, p1, sys::O_NONBLOCK | sys::O_CLOEXEC) <= -1)
	{
			print "pipe error";
			return -1;
	}
	a = sys::fork();
	if (a <= -1)
	{
		print "fork error";
		sys::close (p0);
		sys::close (p1);
	}
	else if (a == 0)
	{
		## child
		sys::close (p0);

		stdout = sys::openfd(1);
		sys::dup(p1, stdout);

		print @b"hello world";
		print @b"testing sys::dup()";
		print @b"writing to standard output..";

		sys::close (p1);
		sys::close (stdout);
	}
	else
	{
		sys::close (p1);
		while (1)
		{
			n = sys::read(p0, k, 10);
			if (n <= 0)
			{
				if (n == sys::RC_EAGAIN) continue; ## nonblock but data not available
				if (n != 0) print "ERROR: " sys::errmsg();
				break;
			}
			print "[" k "]";
		}
		sys::close (p0);
		sys::wait(a);
	}
}
```

#### Duplicate file handles

You can duplicate file handles as necessary.

```awk
BEGIN {
	a = sys::open("/etc/inittab", sys::O_RDONLY);
	x = sys::open("/etc/fstab", sys::O_RDONLY);

	b = sys::dup(a);
	sys::close(a);

	while (sys::read(b, abc, 100) > 0) printf (@b"%s", abc);

	print "-------------------------------";

	c = sys::dup(x, b, sys::O_CLOEXEC);
	## assertion: b == c
	sys::close (x);

	while (sys::read(c, abc, 100) > 0) printf (@b"%s", abc);
	sys::close (c);
}
```

#### Binary packing and unpacking

`sys::pack(output, format, ...)` writes a byte string to `output`;
`sys::unpack(input, format, ...)` assigns decoded values to its output
arguments. Both return zero on success and a negative error code on failure.
`sys::unpack()` rejects integers outside Hawk's signed integer range with
`sys::RC_EINVAL`, including unsigned values too large to represent. The
rejected output argument is unchanged; earlier outputs may already be assigned.
A decimal repeat count applies to the following format specifier. For example,
`2B` processes two unsigned bytes, and `2x` processes two padding bytes.

An unfinished trailing repeat count is ignored. Thus `"2x3"` behaves like
`"2x"`, and `"B3"` behaves like `"B"`. A format containing only a repeat count
performs no packing or unpacking. Counts that overflow the repeat-count range
are still rejected.

```awk
BEGIN {
	sys::pack(bin, "2x3");         ## Two zero bytes; the trailing 3 is ignored.
	sys::unpack(@b"AB", "B3", v);  ## v becomes 65; the trailing 3 is ignored.
}
```

#### Directory traversal

Directory traversal is easy.

```awk
BEGIN {
	d = sys::opendir("/etc", sys::DIR_SORT);
	if (d >= 0)
	{
		while (sys::readdir(d,a) > 0)
		{
			print a;
			sys::stat("/etc/" %% a, b);
			for (i in b) print "\t", i, b[i];
		}
		sys::closedir(d);
	} 
}
```

#### Network interface info

You can get information of a network interface.

```awk
BEGIN { 
	if (sys::getnwifcfg("lo", sys::NWIFCFG_IN6, x) <= -1)
		print sys::errmsg();
	else
		for (i in x) print i, x[i]; 
}
```

#### Sockets

Socket functions are available.

```awk
BEGIN
{
	s = sys::socket();
	...
	sys::close (s);
}
```

### ffi

- ffi::open
- ffi::close
- ffi::call
- ffi::errmsg

```awk
BEGIN {
	ffi = ffi::open();
	if (ffi::call(ffi, r, @b"getenv", @b"s>s", "PATH") <= -1) print ffi::errmsg();
	else print r;
	ffi::close (ffi);
}
```

### mysql

```awk
BEGIN {
	mysql = mysql::open();

	if (mysql::connect(mysql, "localhost", "username", "password", "mysql") <= -1)
	{
		print "connect error -", mysql::errmsg();
	}

	if (mysql::query(mysql, "select * from user") <= -1)
	{
		print "query error -", mysql::errmsg();
	}

	result = mysql::store_result(mysql);
	if (result <= -1)
	{
		print "store result error - ", mysql::errmsg();
	}

	while (mysql::fetch_row(result, row) > 0)
	{
		ncols = length(row);
		for (i = 0; i < ncols; i++) print row[i];
		print "----";
	}

	mysql::free_result(result);

	mysql::close(mysql);
}
```

### sqlite
Assuming `/tmp/test.db` with the following schema,

```
sqlite> .schema
CREATE TABLE a(x int, y varchar(255));
```

You can retreive all rows as shown below:
```awk
@pragma entry main
@pragma implicit off

function main() {
	@local db, stmt, row, i, ncols;

	db = sqlite::open();
	if (db <= -1) {
		print "open error -", sqlite::errmsg();
		return;
	}

	if (sqlite::connect(db, "/tmp/test.db", sqlite::CONNECT_READWRITE) <= -1) {
		print "connect error -", sqlite::errmsg();
		sqlite::close(db);
		return;
	}

	sqlite::exec(db, "begin transaction");
	sqlite::exec(db, "delete from a");
	for (i = 0; i < 10; i++) {
		@local sql, fld;
		if (sqlite::escape_string(db, ((i % 2)? @b"'STXETX'": "'␂␃'") %% (math::rand() * 100), fld) <= -1) {
			print "escape_string error -", sqlite::errmsg();
			sqlite::exec(db, "rollback");
			sqlite::close(db);
			return;
		}
		sql=sprintf("insert into a(x,y) values(%d,'%s')", math::rand() * 100, fld);
		print sql;
		if (sqlite::exec(db, sql) <= -1) {
			print "exec error -", sqlite::errmsg();
			sqlite::exec(db, "rollback");
			sqlite::close(db);
			return;
		}
	}
	sqlite::exec(db, "commit");

	stmt = sqlite::prepare(db, "select x,y from a where x>?");
	if (stmt <= -1) {
		print "prepare error -", sqlite::errmsg();
		sqlite::close(db);
		return;
	}

	if (sqlite::bind(stmt, 1, 10) <= -1) {
		print "bind error -", sqlite::errmsg();
		sqlite::finalize(stmt);
		sqlite::close(db);
		return;
	}

	ncols = sqlite::column_count(stmt);
	printf ("TOTAL %d COLUMNS:\n", ncols);
	for (i = 1; i <= ncols; i++) {
		print "-", i, sqlite::column_name(stmt, i);
	}
	while (sqlite::fetch_row(stmt, row, sqlite::FETCH_ROW_ARRAY) > 0) {
		print "[id]", row[1], "[name]", row[2];
	}

	sqlite::finalize(stmt);
	sqlite::close(db);
}
```

## Incompatibility with AWK

### Parameter passing

In AWK, it is possible for the caller to pass an uninitialized variable as a function parameter and obtain a modified value if the called function sets it to an array.

```awk
function q(a) {
  a[1] = 20;
  a[2] = 30;
}

BEGIN {
  q(x);
  for (i in x)
    print i, x[i];
}
```

In Hawk, to achieve the same effect, you can indicate call-by-reference by prefixing the parameter name with an ampersand (&).

```awk
function q(&a) {
  a[1] = 20;
  a[2] = 30;
}

BEGIN {
  q(x);
  for (i in x)
    print i, x[i];
}
```

Alternatively, you may create an array or a map before passing it to a function.

```awk
function q(a) {
  a[1] = 20;
  a[2] = 30;
}

BEGIN {
  x[3] = 99; delete (x[3]);  ## x = hawk::array() or x = hawk::map() also will do
  q(x);
  for (i in x)
    print i, x[i];
}
```

### Positional variable expression

There are subtle differences in handling expressions for positional variables. In Hawk, many of the ambiguity issues can be resolved by enclosing the expression in parentheses.

| Expression   | Hawk          | AWK             |
|--------------|---------------|-----------------|
| `$++$++i`    | syntax error  | OK              |
| `$(++$(++i))`| OK            | syntax error    |

### Others
- `return` is allowed in `BEGIN` blocks, `END` blocks, and pattern-action blocks.
