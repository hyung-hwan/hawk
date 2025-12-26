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
	- [Control Flow](#control-flow)
		- [if / else](#if--else)
		- [while](#while)
		- [do ... while](#do--while)
		- [for](#for)
		- [for (i in array)](#for-i-in-array)
		- [in operator (key existence)](#in-operator-key-existence)
		- [switch](#switch)
		- [break / continue / return / exit](#break--continue--return--exit)
		- [nextfile / nextofile](#nextfile--nextofile)
	- [Input, Output, and Pipes](#input-output-and-pipes)
	- [Built-in Variables](#built-in-variables)
	- [Built-in Functions](#built-in-functions)
	- [Pragmas](#pragmas)
		- [@pragma entry](#pragma-entry-1)
		- [@pragma implicit](#pragma-implicit)
		- [@pragma striprecspc](#pragma-striprecspc)
	- [@include and @include\_once](#include-and-include_once)
	- [Comments](#comments)
	- [Reserved Words](#reserved-words)
	- [More Examples](#more-examples)
	- [Garbage Collection](#garbage-collection)
	- [Modules](#modules)
		- [Hawk](#hawk)
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
- Embedded sed functionality - includes a sed engine that can be used from C/C++ or invoked via the CLI using --sed <options>

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

Hawk is an embeddable awk interpreter with extensions. It can run awk scripts from the CLI or from C/C++ and provides modules like `str::`, `sys::`, `ffi::`, `mysql::`, and `sqlite::`.


## Running Hawk

Run a script file:

```sh
$ hawk -f script.hawk input.txt
```

Run an inline program:

```sh
$ echo "a,b,c" | hawk 'BEGIN{FS=","} {print $2}'
```

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
- Characters can be written with single quotes (e.g., `'A'`) and are Unicode.
- Byte strings: raw bytes (`@b"..."`).
- Byte characters use `@b'X'` and must fit in a single byte.
- Containers: array, map.
- `@nil` represents null.

Examples:

```awk
BEGIN {
	a = 10
	b = 3.14
	s = "hello"
	c = 'X'
	bc = @b'x'
	bs = @b"\x00\x01"
	m = @{"k": 1}
	arr = @["x", "y"]
}
```

## Expressions and Operators

### Arithmetic and Comparison

- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**` (exponentiation), `++`, `--`, `<<`, `>>`.
- Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`.
- Type-precise compare: `===` and `!==`.

Example:

```awk
BEGIN {
	x = 10 + 5 * 2
	if (x >= 20) print x
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

- Bitwise AND/OR: `&`, `|`.
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

Example:

```awk
@global g
BEGIN {
	@local x
	x = 1
	g = 2
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

Define functions with `function name(...) { ... }`.

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

### break / continue / return / exit

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

Example:

```awk
{ print NR, NF, $0 }
```

## Built-in Functions

Hawk includes awk built-ins (e.g., `length`, `substr`, `split`, `index`) plus extensions in modules (see below).

Example:

```awk
BEGIN { print length("hawk"), substr("hawk", 2, 2) }
```

## Pragmas

`@pragma` controls parser/runtime behavior. File-scope pragmas apply per file; global-scope pragmas appear once across all files.

| Name          | Scope  | Values        | Default | Description                                            |
|---------------|--------|---------------|---------|--------------------------------------------------------|
| entry         | global | function name |         | change the program entry point                         |
| implicit      | file   | on, off       | on      | allow undeclared variables                             |
| multilinestr  | file   | on, off       | off     | allow a multiline string literal without continuation  |
| rwpipe        | file   | on, off       | on      | allow the two-way pipe operator `\|&`                  |
| striprecspc   | global | on, off       | off     | removes leading and trailing blank fields in splitting a record if FS is a regular expression mathcing all spaces |
| stripstrspc   | global | on, off       | on      | trim leading and trailing spaces when converting a string to a number |
| numstrdetect  | global | on, off       | on      | trim leading and trailing spaces when converting a string to a number |
| stack_limit   | global | number        | 5120    | specify the runtime stack size measured in the number of values |

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
 - @global
 - @include
 - @include_once
 - @local
 - @nil
 - @pragma
 - @reset
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

### Hawk

- hawk::array
- hawk::call
- hawk::cmgr_exists
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

### String
The `str` module provides an extensive set of string manipulation functions.

- str::frombase64 - decode a base64-encoded byte string
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
- str::tobase64 - encode data to a base64 byte string
- str::tocharcode - get the numeric value of the first character
- str::tohex
- str::tolower - equivalent to tolower
- str::tonum - convert a string to a number. a numeric value passed as a parameter is returned as it is. the leading prefix of 0b, 0, and 0x specifies the radix of 2, 8, 16 respectively. conversion stops when the end of the string is reached or the first invalid character for conversion is encountered.
- str::toupper - equivalent to toupper
- str::trim


### System

The `sys` module provides various functions concerning the underlying operation system.

- sys::basename
- sys::chmod
- sys::close
- sys::closedir
- sys::dirname
- sys::dup
- sys::errmsg
- sys::fork
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
- sys::pipe
- sys::read
- sys::readdir
- sys::setttime
- sys::sleep
- sys::strftime
- sys::system
- sys::unlink
- sys::wait
- sys::write


You may read the file in raw bytes.

```awk
BEGIN {
	f = sys::open("/etc/sysctl.conf", sys::O_RDONLY);
	if (f >= 0) {
		while (sys::read(f, x, 10) > 0) printf (B"%s", x);
		sys::close(f);
	}
}
```

You can map a raw file descriptor to a handle created by this module and use it.

```awk
BEGIN {
	a = sys::openfd(1);
	sys::write(a, B"let me write something here\n");
	sys::close(a, sys::C_KEEPFD); ## set C_KEEPFD to release 1 without closing it.
	##sys::close(a);
	print "done\n";
}
```

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
		sys::close (p0);
		sys::close (p1);
	}
	else if (a == 0)
	{
		## child
		printf ("child.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close (p1);
		while (1)
		{
			n = sys::read (p0, k, 3);
			if (n <= 0) 
			{
				if (n == sys::RC_EAGAIN) continue; ## nonblock but data not available
				if (n != 0) print "ERROR: " sys::errmsg();
				break;
			}
			print k;
		}
		sys::close (p0);
		return 123;
	}
	else
	{
		## parent
		printf ("parent.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close (p0);
		sys::write (p1, B"hello");
		sys::write (p1, B"world");
		sys::close (p1);

		##sys::wait(a, status, sys::WNOHANG);
		while (sys::wait(a, status) != a);
		if (sys::WIFEXITED(status)) print "Exit code: " sys::WEXITSTATUS(status);
		else print "Child terminated abnormally"
	}
}
```

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

		print B"hello world";
		print B"testing sys::dup()";
		print B"writing to standard output..";

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

You can duplicate file handles as necessary.

```awk
BEGIN {
	a = sys::open("/etc/inittab", sys::O_RDONLY);
	x = sys::open("/etc/fstab", sys::O_RDONLY);

	b = sys::dup(a);
	sys::close(a);

	while (sys::read(b, abc, 100) > 0) printf (B"%s", abc);

	print "-------------------------------";

	c = sys::dup(x, b, sys::O_CLOEXEC);
	## assertion: b == c
	sys::close (x);

	while (sys::read(c, abc, 100) > 0) printf (B"%s", abc);
	sys::close (c);
}
```

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

You can get information of a network interface.

```awk
BEGIN { 
	if (sys::getnwifcfg("lo", sys::NWIFCFG_IN6, x) <= -1)
		print sys::errmsg();
	else
		for (i in x) print i, x[i]; 
}
```

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
	if (ffi::call(ffi, r, @B"getenv", @B"s>s", "PATH") <= -1) print ffi::errmsg();
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
```
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

### Return value of getline

### Others
- `return` is allowed in `BEGIN` blocks, `END` blocks, and pattern-action blocks.
