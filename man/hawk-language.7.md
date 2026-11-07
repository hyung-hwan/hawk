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

Identifiers begin with a letter or underscore, followed by letters, digits,
or underscores. They are case-sensitive; character classification depends on
the build's character support. Keywords cannot ordinarily be identifiers.
`#` starts a comment
extending to the end of the line. Modern mode also supports `/* ... */` comments.
Semicolons separate statements; newlines normally terminate statements too.

Strings use double quotes, and regular-expression literals use slashes.
Backslash escapes include `\n`, `\r`, `\t`, `\0`, and hexadecimal escapes.
Single-quoted literals denote characters rather than shell-style strings.

Function argument lists, array literals, and map literals can span lines,
including a newline after the opening delimiter and before the closing one.
Keep a direct function name or indexed callable adjacent to its opening `(`
to avoid ambiguity with implicit concatenation.

Integers support decimal, leading-zero octal, `0o` octal, `0x` hexadecimal,
and `0b` binary notation. Floating-point forms include `1.5`, `.5`, and `1e3`.
A leading sign is a unary operator, not part of a numeric token. Raw literals
such as `@r"C:\path"` and `@rb"\n"` leave backslashes unchanged.

The reserved words are:

```text
@argc @argv @const @exit @false @global @include @include_once
@local @nil @pragma @reset @true
BEGIN END break case continue default delete do else exit for
function func getbline getline if in next nextfile nextofile
print printf return switch while
```

Some ordinary keywords depend on enabled features and can occur as module
symbol names after `::`. Built-in variable and function names also have
definition restrictions.

# GRAMMAR

The following EBNF describes the parser's main syntax in modern mode. Quoted
text is literal syntax; `|` separates alternatives, `[ ... ]` is optional,
and `{ ... }` means zero or more repetitions. `identifier`, `integer`,
`float`, `string`, `character`, `byte-string`, `byte-character`, and `regex`
are lexical tokens. `end` means a semicolon or significant newline.
Unquoted parentheses group alternatives; semicolons ending productions are
grammar notation. Backslashes inside quoted grammar tokens are literal.

Spacing and newline rules are described separately below; the productions
omit optional blank lines between program units and statements. The grammar
does not replace checks for valid assignment targets, declarations, argument
counts, execution context, or enabled features.

## Program units and declarations

```text
program         = { unit } ;
unit            = global-decl | const-decl | function-def | directive
                | "BEGIN" block | "END" block | rule ;
rule            = block | pattern block | pattern end ;
pattern         = expression [ "," expression ] ;
directive       = ("@include" | "@include_once") string [ ";" ]
                | "@pragma" identifier pragma-value [ ";" ] ;
pragma-value    = identifier | integer ;
global-decl     = "@global" declarator { "," declarator } end ;
local-decl      = "@local" declarator { "," declarator } end ;
const-decl      = "@const" const-declarator { "," const-declarator } end ;
declarator      = identifier [ "=" conditional ] ;
const-declarator = identifier "=" conditional ;
function-def    = function-keyword identifier "(" [ parameters ] ")" block ;
function-keyword = "function" | "func" ;
parameters      = "..." | parameter { "," parameter } [ "," "..." ] ;
parameter       = [ "&" ] identifier ;
block           = "{" { block-decl } { statement } "}" ;
block-decl      = local-decl | const-decl | include-directive ;
include-directive = ("@include" | "@include_once") string [ ";" ] ;
```

A rule with two patterns is an inclusive range: matching the first starts
the range and matching the second ends it. A pattern without an action has
the implicit action `print $0`; the last such pattern may end at end of file.
`BEGIN` and `END` require explicit blocks. Multiple blocks run in source order.

`@global` is a top-level declaration. `@local` and block-scoped `@const`
belong at the beginning of their block, before executable statements. Use a
nested block to introduce declarations later. Includes are also accepted in
this declaration region; included text must be valid at its insertion point.
Pragma values depend on the named pragma.

Initializers use conditional expressions at their outermost level, so
`@local x = y = 10` is not accepted. Parenthesized subexpressions use the
general expression grammar: `@local x = (y = 10)` is accepted when `y` is
available under the current declaration rules. Initializers may call functions
and use runtime values; `@const` requires an initializer and prohibits later
assignment to the declared constant.

## Statements

```text
statement       = block | ";" | expression end
                | "if" "(" expression ")" statement [ "else" statement ]
                | "while" "(" expression ")" statement
                | "do" statement "while" "(" expression ")" end
                | "for" "(" [ expression ] ";" [ expression ] ";"
                            [ expression ] ")" statement
                | "for" "(" variable "in" container ")" statement
                | "switch" "(" expression ")" "{" { switch-arm } "}"
                | "break" end | "continue" end
                | "return" [ expression ] end
                | ("exit" | "@exit") [ expression ] end
                | ("next" | "nextfile" | "nextofile") end
                | "delete" delete-target end
                | "@reset" reset-target end
                | output end ;
switch-arm      = ("case" case-literal | "default") ":" { statement } ;
case-literal    = integer | float | string | character
                | byte-string | byte-character ;
delete-target   = variable | "(" variable ")" ;
reset-target    = identifier | "(" identifier ")" ;
```

The final simple statement or block declaration before `}` may omit its
terminator. `else` binds to the nearest unmatched `if`.
`break` requires a loop or switch; `continue`
requires a loop. Switch arms fall through unless control leaves them, and
only one `default` is allowed. A case label is a literal token, not a general
constant expression; for example, `case 1:` is valid but `case 1 + 1:` is not.

Here `variable` is a named variable, parameter, or indexed variable;
`container` is a variable holding a map or array, or `@argv`. `delete` accepts
an element or an entire container; `@reset` requires an unindexed variable.
Neither operation accepts an arbitrary expression or field reference.

## Expressions and precedence

Each binary level below associates left to right. Assignment associates
right to left, and the conditional operator nests in its expression arms.
`concat-op` is `%%` or implicit adjacency when blank concatenation is enabled.

```text
expression      = conditional [ assignment-op expression ] ;
conditional     = logical-or [ "?" expression ":" expression ] ;
logical-or      = logical-and { "||" logical-and } ;
logical-and     = membership { "&&" membership } ;
membership      = matching { "in" matching } ;
matching        = bitwise-or { ("~" | "!~") bitwise-or } ;
bitwise-or      = bitwise-xor { "|" bitwise-xor } ;
bitwise-xor     = bitwise-and { "^^" bitwise-and } ;
bitwise-and     = equality { "&" equality } ;
equality        = relational { ("==" | "!=" | "===" | "!==") relational } ;
relational      = shift { ("<" | "<=" | ">" | ">=") shift } ;
shift           = concatenation { ("<<" | ">>") concatenation } ;
concatenation   = additive { concat-op additive } ;
additive        = multiplicative { ("+" | "-") multiplicative } ;
multiplicative  = unary { ("*" | "/" | "\" | "%") unary } ;
unary           = unary-op unary | power ;
power           = power-operand { ("^" | "**") power-operand } ;
power-operand   = unary-op unary | increment ;
unary-op        = "+" | "-" | "!" | "~" ;
increment       = [ "++" | "--" ] primary [ "++" | "--" ] ;
assignment-op   = "=" | "+=" | "-=" | "*=" | "/=" | "\=" | "%="
                | "^=" | "**=" | "%%=" | "<<=" | ">>="
                | "&=" | "^^=" | "|=" ;
```

Read these levels from lowest binding strength (`expression`) to highest
(`primary`). Assignment and increment/decrement require writable variables
or fields. A single increment expression cannot have both a prefix and a
postfix operator. The right operand of `in` must designate a container.

Hawk's exponentiation currently associates left to right: `2 ^ 3 ^ 2`
means `(2 ^ 3) ^ 2`. A leading unary sign binds less tightly than a power,
so `-2 ^ 2` means `-(2 ^ 2)`. A unary operand on the right can consume
another power; use parentheses for an unambiguous exponent chain.

## Primary expressions, indexing, and calls

```text
primary         = atom { ("|" | "|&") input-op [ lvalue ] } ;
atom            = scalar | variable | module-symbol | call | field
                | "(" expression { "," expression } ")"
                | array-literal | map-literal | function-literal
                | "@argc" | "@argv" [ "[" expression "]" ]
                | input-op [ lvalue ] [ "<" atom ] ;
scalar          = integer | float | string | character
                | byte-string | byte-character | regex
                | "@nil" | "@true" | "@false" ;
variable        = identifier { index } ;
index           = "[" expression { "," expression } "]" | "." identifier ;
module-symbol   = identifier "::" identifier ;
call            = (identifier | module-symbol) "(" [ arguments ] ")"
                | indexed-variable "(" [ arguments ] ")" ;
indexed-variable = identifier index { index } ;
arguments       = expression { "," expression } ;
field           = "$" primary ;
lvalue          = variable | field ;
function-literal = function-keyword "(" [ parameters ] ")" block ;
array-literal   = "@[" [ arguments | entries ] "]" ;
map-literal     = "@{" [ entries ] "}" ;
entries         = expression ":" expression { "," expression ":" expression } ;
input-op        = "getline" | "getbline" ;
```

The pipe form above applies only when `|` or `|&` is followed by an input
keyword; otherwise `|` is bitwise OR. Compound filenames after `<` should be
parenthesized, for example `getline line < (prefix %% suffix)`.

`a[x, y]` is an AWK composite map key joined using `SUBSEP`; it differs from
the nested access `a[x][y]`. Dot access requires an identifier and means a
string key. Thus `a.name` means `a["name"]`, while `a.1` is not dot access.
In non-tolerant mode, a comma group is generally restricted to membership,
as in `(x, y) in a`, and parenthesized output arguments. Tolerant mode also
permits general comma groups, whose result is the last expression.

`xcall` enables calls through indexed variables and function literals.
There is no general postfix-call or postfix-index operator for arbitrary
expressions: `a.b(1)` is supported, but `a.b(1)(2)` is not a chained call,
and `(a.b)(1)` is not an indirect call. With blank concatenation enabled,
such adjacent parenthesized expressions can instead mean concatenation.

Named functions may have whitespace before `(` when the parser can identify
them as functions. For variable and indexed calls, keep `(` adjacent to the
callable name or closing `]`. Some intrinsic functions, notably `length`,
also accept the AWK form without parentheses; this is not general call syntax.

## Output syntax

```text
output          = "print" [ output-arguments ] [ redirection ]
                | "printf" output-arguments [ redirection ] ;
output-arguments = arguments | "(" arguments ")" ;
redirection     = (">" | ">>" | "|" | "|&") expression ;
```

`print` without arguments prints `$0`; `printf` requires a format argument.
Output has context-sensitive parsing: `print x > y` redirects, whereas
`print (x > y)` prints a comparison. Parenthesize bitwise OR and right shifts
similarly. Tolerant mode additionally permits `print` and `printf` within
expressions. Runtime I/O must be enabled, and `|&` also requires `rwpipe`.

## Newlines and feature switches

With newline recognition enabled, these rules supplement the grammar:

- `BEGIN` or `END` and the opening `{` must share a line. A pattern's action
  must start on that line too; ending the pattern line selects its implicit
  action. A function body may start on a later line after its parameter list.
- Call arguments and container literals allow any number of newlines after
  their opening delimiter, after a comma (or an entry colon), and before
  their closing delimiter. Trailing commas are not accepted. Put a separating
  comma or colon before a line break, not at the start of the next line.
- The same relaxation does not generally apply to grouping parentheses,
  subscripts, control-condition parentheses, or function parameter lists.
  Parameter lists do permit newlines after a comma.
- Newlines may follow `&&` and `||`. For other expression continuations,
  use a backslash immediately followed by a newline. A CRLF continuation is
  accepted too. With newline recognition disabled, use explicit semicolons.

Grammar extensions depend on the runner's traits and build options. In
particular, blank concatenation, tolerant parsing, pattern-action blocks,
runtime I/O, and extended calls can be disabled. Select explicit parentheses
and separators when writing code intended for different modes.

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
`@const` declares an initialized constant whose value cannot be reassigned.
Initializers execute at runtime: top-level initializers run before `BEGIN`
processing, and block initializers run when their block is entered.

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

Declaration initializers have their own parsing restrictions; see the grammar
above for assignment expressions within initializers.

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

`return` is also accepted in `BEGIN` and `END` blocks. `next` and `nextfile`
are not allowed directly in those blocks.

# INPUT AND OUTPUT

`print` separates its arguments with `OFS` and appends `ORS`. `printf` formats
output without an automatic record terminator; `sprintf` returns formatted
text. Use `%d`, `%f`, and `%s` for familiar numeric and string formatting.

`%v` displays values, including nested arrays and maps, in a diagnostic form.
Sparse arrays display explicit indexes. Rendering is bounded and can include
`<cycle>`, `<depth-limit>`, or `<truncated>` markers. This is not JSON; use
`json::stringify()` for interchange.

Nested strings and map keys are quoted, byte strings retain their `@b`
prefix, and control characters and non-ASCII values are escaped. Map order
follows the underlying container. Rendering uses a heap stack, with default
limits of 32 nested containers, 4,096 output characters, and 16,384 traversal
steps, including scanning empty array slots and map buckets. Truncation closes
container delimiters; format precision can further shorten the result.

C embedders can select `HAWK_RTX_VALTOSTR_REPR` in `hawk_rtx_valtostr()`, or
call `hawk_rtx_valtorepr()` with a `hawk_rtx_valrepr_opt_t` for custom depth,
size, and work limits. Zero limit fields select defaults.

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

For example, send records to a command, close its input, then read its output:

```awk
BEGIN {
	@local command = "sort", line
	print "world" |& command
	print "hello" |& command
	close(command, "to")
	while ((command |& getline line) > 0) print line
	close(command)
}
```

CSV-style splitting is selected by an `FS` starting with `?` followed by
four characters: separator, escaper, opening quote, and closing quote.
For comma-separated fields with doubled double quotes, use:

```awk
BEGIN { FS = "?,\"\"\"" }
{ print NF, $1 }
```

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

`ENVIRON` affects children started through pipes and `sys::system()`.
Changing it does not change the embedding application's process environment.
Imported values can become numeric values depending on conversion settings.
`PIPECLOEXEC = 1` enables close-on-exec for pipe-command descriptors;
`@pragma pipecloexec` sets its initial policy. `nextofile` advances among
output consoles supplied with the runner's `-t` option.

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

`defermodsym on` delays resolving module functions and constants until their
expressions execute, allowing unexecuted branches to reference optional
modules. `striprecspc` applies when the field separator is a regular expression
matching spaces; it controls removal of leading and trailing empty fields.
`stripstrspc` instead controls trimming around string-to-number conversions.

`stack_limit` does not allocate or enlarge the native C stack. On supported
builds, native stack bounds are checked separately before recursive evaluation.
Embedding callbacks and native extensions remain responsible for their own
stack use.

When the necessary APIs are detected on Linux, macOS, FreeBSD, NetBSD,
OpenBSD, Haiku, or Windows, the runtime reserves 64 KiB of native stack
headroom and reports `HAWK_ESTACK` before recursive evaluation exhausts it.
Bounds are refreshed at host execution entry, including callback reentry and
sequential use of a runtime context on different threads. Failure to obtain
valid bounds on such a build is a runtime error. Hosts must enter with enough
stack for initialization and error handling and must not change active stack
limits during execution. Other builds retain the value-stack and logical
recursion limits.

# INCLUDE FILES

`@include "file.hawk"` reads another source file. `@include_once "file.hawk"`
avoids processing the same included source repeatedly. The standard source
handler uses the including file's context and configured include directories;
use `hawk --incdirs directories` to supply the search path.

# SEE ALSO

[hawk(1)](hawk.1.md), [hawkgo(1)](hawkgo.1.md),
[hawk-modules(7)](hawk-modules.7.md).
