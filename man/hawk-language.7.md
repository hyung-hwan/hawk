# NAME

hawk-language - Hawk language and execution reference

# DESCRIPTION

Hawk extends AWK with explicit declarations, arrays distinct from maps,
first-class functions, byte values, modules, and embeddable execution contexts.
This page describes common language facilities in modern mode. The command-line
interface and feature switches are documented in [hawk(1)](hawk.1.md).

# PROGRAM EXECUTION

A program consists of declarations, function definitions, and pattern-action
rules:

```awk
BEGIN { FS = "," }
$3 ~ /ERROR/ { print FNR, $1, $3 }
END { print "records:", NR }
```

`BEGIN` blocks run before input. Each input record becomes `$0`, and its fields
are available as `$1`, `$2`, and so on. `FS` selects field splitting, and `RS`
selects record separation. Matching rules execute their actions in source
order. A missing pattern matches every record; a missing action prints the
record. `END` blocks run after record processing.

The host normally enters this execution flow. `@pragma entry name` instead
selects a user function, with command-line operands supplied as arguments by
the standard runner:

```awk
@pragma entry main

function main(name)
{
	print "Hello", name
}
```

`hawk -c name` also invokes a function directly. Direct calls do not first run
the program's `BEGIN` blocks.

# LEXICAL CONVENTIONS

Identifiers name variables, parameters, and functions. `#` starts a comment
extending to the end of the line. Modern mode also supports `/* ... */` comments.
Semicolons separate statements; newlines normally terminate statements too.

Strings use double quotes, and regular-expression literals use slashes.
Backslash escapes include `\n`, `\r`, `\t`, `\0`, and hexadecimal escapes.
Single-quoted literals denote characters rather than shell-style strings.

Function argument lists, array literals, and map literals can span lines,
including a newline after the opening delimiter and before the closing one.
Keep a direct function name or indexed callable adjacent to its opening `(`
to avoid ambiguity with implicit concatenation.

# VALUES AND TYPES

The runtime distinguishes these values:

- Nil: `@nil`, also used for missing function arguments.
- Boolean: `@true` and `@false`.
- Integer and floating-point numbers: `42`, `3.5`.
- Text strings: `"Hawk"`.
- Text characters: `'H'` or `@c"H"`.
- Byte strings: `@b"\x00\xff"`.
- Byte characters: `@b'H'` or `@bc"H"`.
- Arrays, maps, regular expressions, functions, and module-specific BOB values.

BOB values hold opaque module data, such as a streaming digest context.

Values convert according to their context. Arithmetic can convert strings to
numbers; booleans convert to 1 and 0. String conversion of booleans produces
`"1"` and `"0"`. Use `hawk::type(value)` for a numeric type code and
`hawk::typename(value)` for its name. Compare codes with constants such as
`hawk::VAL_BOOL`, `hawk::VAL_ARRAY`, and `hawk::VAL_MAP`.

`==` and `!=` permit the language's conversions. `===` and `!==` also compare
the value types:

```awk
BEGIN {
	print @true + 2             # 3
	print @false %% "ABC"       # 0ABC
	print @true === 1           # false: bool and int differ
	print hawk::typename(@true)  # bool
}
```

Integer and floating-point widths are build-dependent. Do not assume that
every build has the same numeric range.

# VARIABLES AND DECLARATIONS

Undeclared names are accepted by default and refer to named runtime variables.
Use `@pragma implicit off` to require declarations. Function parameters are
declared by their parameter lists.

`@global` declares program-wide variables. `@local` declares block-local
variables. Both accept initializers and comma-separated declarators.
`@const` declares a constant evaluated during parsing; its value cannot be
reassigned. Constant expressions must be resolvable at parse time.

```awk
@pragma implicit off

@const FACTOR = 2
@global total = 0

function add(value)
{
	@local scaled = value * FACTOR
	total += scaled
	return total
}

BEGIN { print add(5), add(3) }
```

Declaration initializers have their own parsing restrictions. In particular,
nested assignment expressions such as `@global x = (y = 10)` are not allowed.

# ARRAYS AND MAPS

Arrays use nonnegative integer indexes; maps use associative keys. Construct
them explicitly with `@[]` and `@{}` or with the `hawk::` constructors:

```awk
BEGIN {
	@local values = @["one", "two", "three"]
	@local record = @{"name": "Hawk", "count": 3}
	print values[1], record["name"], record.count
}
```

Ordinary array literals start at index 1. Indexed array literals can specify
gaps and index zero:

```awk
BEGIN {
	@local sparse = @[
		0: "zero",
		3: "three",
		8: @[1, 2]
	]
	print sparse[0], sparse[8][2]
	# Equivalent constructor:
	sparse = hawk::indexed_array(0, "zero", 3, "three", 8, @[1, 2])
}
```

All entries in an indexed literal must use `index: value`; they cannot be mixed
with positional entries. Index expressions are evaluated left to right, and a
later assignment to the same index replaces the earlier value. Array indexes
use integer conversion: `"20"` becomes 20 and fractional numbers truncate
toward zero. The result must be nonnegative and within the supported range;
non-finite and out-of-range floating-point values are rejected.

For identifier-like map keys, `record.name` is equivalent to `record["name"]`.
Accesses can be chained, for example `record.address.city` and
`record["items"][1]`.

Use `key in container` to test membership. `delete container[key]` removes an
element. `for (key in container)` visits elements; do not rely on map iteration
order. Sparse arrays retain their explicit indexes.

# FUNCTIONS

Define a named function with `function name(parameters) { statements }`.
`func` is an alternative spelling of `function`. Use `return expression` to
return a value. Missing arguments have the value `@nil`.

Parameters normally receive values. Containers are shared values: modifying
their elements can affect a caller. Prefix a parameter name with `&` to bind it
to the caller's variable, including replacement of that variable's value:

```awk
function increment(&value)
{
	value += 1
}

BEGIN {
	@local count = 0
	increment(count)
	print count
}
```

An uninitialized actual argument does not automatically become an output map
merely because a normal parameter receives indexed assignments. Use a reference
parameter or initialize the container before passing it.

Variadic functions use `...`. `@argc` gives the argument count and `@argv`
provides arguments starting at index 0:

```awk
function show(...)
{
	@local i
	for (i = 0; i < @argc; i++) print @argv[i]
}
```

Functions are values and can be stored in variables and containers. Enable
`@pragma xcall on` for function literals and calls through indexed values:

```awk
@pragma implicit off
@pragma xcall on

BEGIN {
	@local twice = func(value) { return value * 2 }
	@local operations = @{}
	operations.square = func(value) { return value * value }
	print twice(10), operations.square(4), operations["square"](5)
}
```

Function literals do not capture outer parameters or local variables. They are
ordinary function values with independent execution contexts, not closures.
The indexed-call extension supports a call through a dot- or bracket-indexed
value; do not assume arbitrary JavaScript-style postfix call chains are valid.

# OPERATORS

Use parentheses to make grouping explicit, especially around expressions that
could also be parsed as I/O redirection.

- Arithmetic: `+`, `-`, `*`, `/`, `\` for integer division, `%` for remainder,
  and `^` or `**` for exponentiation.
- Increment and decrement: prefix or postfix `++` and `--`.
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `\=`, `%=`, `^=`, `**=`.
- Concatenation: adjacency, or explicit `%%`; `%%=` appends to a variable.
- Comparison: `<`, `<=`, `>`, `>=`, `==`, `!=`, `===`, `!==`.
- Regex matching: `~` and `!~`.
- Logical operations: `!`, `&&`, `||`. The latter two short-circuit.
- Conditional expression: `condition ? value_if_true : value_if_false`.
- Bitwise operations: `&`, `|`, `^^`, `~`, `<<`, `>>`; compound assignments
  include `&=`, `|=`, `^^=`, `<<=`, and `>>=`.

`|` also introduces a pipe and `>>` also redirects appended output. For
example, `print (1 | 2)` prints a bitwise result, while `print 1 | "command"`
writes to a command.

# CONTROL FLOW

Hawk supports `if`/`else`, `while`, `do`/`while`, C-style `for`, container
iteration, and `switch`/`case`/`default`. Use `break` and `continue` within
loops, and `return` to leave a function.

`next` skips the remaining rules for the current input record. `nextfile`
advances to the next input file. `nextofile` advances to the next configured
output console when that feature is enabled.

`exit` leaves normal input processing and permits `END` processing. A value
can be supplied for the runner's exit status. `@exit` aborts execution without
the normal `END` processing.

# INPUT AND OUTPUT

`print` separates its arguments with `OFS` and appends `ORS`. `printf` formats
output without an automatic record terminator; `sprintf` returns formatted
text. Use `%d`, `%f`, and `%s` for familiar numeric and string formatting.

`%v` displays values, including nested arrays and maps, in a diagnostic form.
Sparse arrays display explicit indexes. Rendering is bounded and can include
`<cycle>`, `<depth-limit>`, or `<truncated>` markers. This is not JSON; use
`json::stringify()` for interchange.

```awk
BEGIN {
	printf "%v\n", @[0: "zero", 3: @{"ready": @true}]
}
```

`getline` returns 1 for a record, 0 for end of input, and -1 for an I/O error.
`getbline` provides the byte-record counterpart. A target variable receives the
record without assigning `$0`:

```awk
BEGIN {
	@local line
	while ((getline line < "input.txt") > 0) print line > "output.txt"
	close("input.txt")
	close("output.txt")
}
```

`>` creates or truncates an output when first opened; `>>` opens for append.
Further writes to the same open destination continue on that stream. Use
`close(name)` to close a named stream.

Pipes use `print value | command` and `command | getline variable`. Two-way
pipes use `|&`; `close(command, "to")` closes the writing direction so the
child can finish before its output is read.

Useful built-in functions include `length`, `index`, `substr`, `split`,
`match`, `sub`, `gsub`, `sprintf`, `tolower`, `toupper`, and `close`.
Additional functions are described in [hawk-modules(7)](hawk-modules.7.md).

# PREDEFINED VARIABLES

- `NR`: total input record count.
- `FNR`: record count in the current input file.
- `NF`: number of fields in the current record.
- `FILENAME`: current input filename.
- `OFILENAME`: current output-console filename when enabled.
- `FS`, `RS`: field and record separators.
- `OFS`, `ORS`: output field and record separators.
- `ARGC`, `ARGV`: command-line argument count and vector; `ARGV[0]` identifies
  the program.
- `ENVIRON`: environment supplied to the runtime and its child processes.
- `IGNORECASE`: case-insensitive matching and comparison control.
- `PIPECLOEXEC`: descriptor inheritance policy for external pipe commands.

# PRAGMAS

Write pragmas as `@pragma name value`. Available pragmas include:

- `entry name`: choose the program's entry function.
- `implicit on|off`: allow or reject undeclared variables.
- `pedantic on|off`: enable stricter parser checks.
- `xcall on|off`: enable extended calls and function literals.
- `defermodsym on|off`: defer module-symbol resolution until runtime.
- `multilinestr on|off`: allow raw multiline string and regex literals.
- `rwpipe on|off`: allow two-way pipes.
- `pipecloexec on|off`: set the initial close-on-exec policy for pipe commands.
- `striprecspc on|off`: control trimming during record splitting.
- `stripstrspc on|off`: control trimming during string-to-number conversion.
- `numstrdetect on|off`: control automatic numeric-string detection.
- `stack_limit number`: set the heap-based runtime value-stack limit.

Some pragmas are file-scoped and others, such as `entry` and `stack_limit`, are
program-wide. Include files have their own file-scoped pragma state. Defaults
depend on the runner's selected traits; command-line feature defaults are
shown by `hawk --help`.

`stack_limit` does not allocate or enlarge the native C stack. On supported
builds, native stack bounds are checked separately before recursive evaluation.
Embedding callbacks and native extensions remain responsible for their own
stack use.

# INCLUDE FILES

`@include "file.hawk"` reads another source file. `@include_once "file.hawk"`
avoids processing the same included source repeatedly. The standard source
handler uses the including file's context and configured include directories;
use `hawk --incdirs directories` to supply the search path.

# SEE ALSO

[hawk(1)](hawk.1.md), [hawkgo(1)](hawkgo.1.md),
[hawk-modules(7)](hawk-modules.7.md).
