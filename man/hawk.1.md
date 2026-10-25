# NAME

hawk - run AWK programs and embedded text processors

# SYNOPSIS

**hawk** [*options*] **-f** *sourcefile* [**--**] [*datafile* ...]

**hawk** [*options*] [**--**] *program* [*datafile* ...]

**hawk** **--cut** [*options*] *selector* [*file* ...]

**hawk** **--sed** [*options*] *script* [*file* ...]

**hawk** **--m4** [*options*] [*file* ...]

**hawk** **--sh** [**-i**] [**--registered** | **--path**] [**-c** *command* | *file*]

**hawk** **--usage** | **--version**

# DESCRIPTION

Hawk is an AWK interpreter with explicit local and global declarations,
first-class functions, arrays, maps, byte strings, and loadable modules. The
same engines are available through an embeddable C library and C++ classes.

Without a mode switch, Hawk parses the supplied program and runs its `BEGIN`
blocks, record-processing rules, and `END` blocks. Without data files, input
comes from standard input. A data filename of `-` also selects standard input.
Unredirected output goes to standard output; diagnostics go to standard error.

Use [hawk-language(7)](hawk-language.7.md) for the language and
[hawk-modules(7)](hawk-modules.7.md) for module interfaces.

# INTERPRETER OPTIONS

Options precede the source string and data operands. `--` ends option parsing.
Options described here apply to interpreter mode; each other mode has its own
options.

- **-h**, **--help**
  Print interpreter options, including the defaults of the feature switches.

- **-f** *file*, **--file** *file*
  Read program source from a file instead of a source-string operand. Repeat
  this option to provide several source files. `-` selects standard input.

- **-c** *name*, **--call** *name*
  Call the named function directly instead of running the record-processing
  loop. Remaining operands become string arguments to the function.

- **-v** *name=value*, **--assign** *name=value*
  Add or initialize a global variable before execution. Repeat for several
  variables. Values are interpreted as numbers or strings as appropriate.

- **-F** *separator*, **--field-separator** *separator*
  Initialize the `FS` field separator.

- **-I** *directories*, **--incdirs** *directories*, **--includedirs** *directories*
  Set the directory list used to resolve include files. Use the platform's
  path-list separator: colon on Unix and semicolon on Windows. Pass a combined
  list rather than relying on repeated options to append directories.

- **--modlibdirs** *directories*
  Set the search path for dynamically loaded modules, using the platform's
  path-list separator.

- **-d** *file*, **--deparsed-file** *file*
  Write the parsed program back as Hawk source to the given file. Execution
  still follows parsing. The result is a deparsed program, not a copy preserving
  the original comments and layout.

- **-t** *file*, **--console-output** *file*
  Set a console output filename. This option may be repeated to configure
  several output files. `nextofile` advances to the next output console.

- **-m** *bytes*, **--memory-limit** *bytes*
  Limit the interpreter's managed allocation heap to the given size. This is
  not a limit on the entire process's address space.

- **-w**
  Expand wildcard patterns in data-file operands within Hawk.

- **-D**
  Print additional execution and allocation information for debugging.

- **--script-encoding** *name*
  Select the source-file encoding in a wide-character build.

- **--conin-encoding** *name*, **--conout-encoding** *name*
  Select console input and output encodings in a wide-character build.
  Available encodings depend on the build.

- **--modern**
  Select modern language traits. This is the default language mode.

- **--classic**
  Select the traits intended for traditional AWK programs. This changes a
  group of features; it does not promise compatibility with every AWK extension.

# FEATURE SWITCHES

Each switch below takes `on` or `off`, for example `--xcall=on` or
`--implicit off`. The defaults listed are for modern mode on Unix.
`hawk --help` marks enabled defaults as `on*/off` and disabled defaults as
`on/off*`; use `hawk --classic --help` to inspect classic-mode defaults.

- **--blankconcat**: adjacency-based string concatenation; default on.
- **--crlf**: CRLF newline output; default off on Unix.
- **--defermodsym**: resolve module symbols at runtime; default off.
- **--flexmap**: assign and return map values; default on.
- **--implicit**: accept undeclared variables; default on.
- **--multilinestr**: allow unescaped newlines inside string and regular
  expression literals; default off.
- **--ncmponstr**: compare numeric strings numerically; default off.
- **--newline**: recognize newlines as statement terminators; default on.
- **--nextofile**: enable `nextofile` and `OFILENAME`; default on.
- **--numstrdetect**: automatically detect numeric strings; default on.
- **--pablock**: enable pattern-action record processing; default on.
- **--pedantic**: enable stricter parser checks; default off.
- **--pipecloexec**: set close-on-exec on descriptors when executing a pipe
  command; default off. The `PIPECLOEXEC` variable controls this at runtime.
- **--rexbound**: enable bounded regular-expression repetition such as `{2,4}`;
  default on.
- **--rio**: enable built-in runtime I/O such as `print` and `getline`;
  default on.
- **--rwpipe**: enable two-way pipes with `|&`; default on.
- **--strictnaming**: enable strict naming rules; default on.
- **--striprecspc**: strip leading and trailing blank fields when splitting a
  record with a whitespace-matching regular expression; default off.
- **--stripstrspc**: trim spaces during string-to-number conversion; default on.
- **--tolerant**: enable more tolerant runtime behavior; default on.
- **--xcall**: enable extended calls through indexed values and function
  literals; default off.

Source pragmas can control selected traits as described in
[hawk-language(7)](hawk-language.7.md#pragmas).

# OTHER COMMAND MODES

The mode selector must be the first argument. All following options are parsed
by the selected processor. `--awk` and `--hawk` explicitly select interpreter
mode. `--usage` lists modes, and `--version` prints the version.

## Cut mode

`hawk --cut` extracts fields and character ranges. Selectors use one-based
positions: `f3` selects field 3, `f3-5` selects fields 3 through 5, and `c2-4`
selects character positions 2 through 4. `d,` sets both delimiters to a comma;
`D:,` sets the input delimiter to colon and the output delimiter to comma.

Pass a selector as an operand, with `-e selector`, or in a file with
`-f selector-file`. `-o file` selects an output file. `-m` limits managed
memory, `-w` expands input wildcards, and `-D` prints debugging information.
Wide-character builds also accept `--script-encoding`, `--infile-encoding`, and
`--outfile-encoding`. `--help` prints the mode's options.

```sh
printf 'alpha,beta,gamma\n' | hawk --cut 'd, f1 f3'
printf 'abcdef\n' | hawk --cut 'c2-4'
```

## Sed mode

`hawk --sed` runs a stream-editing script. Supply the script as an operand,
with `-e script`, or with `-f script-file`.

- `-n`: suppress automatic printing.
- `-r`: use extended regular expressions.
- `-R`: enable additional regular-expression extensions.
- `-o file`: select the output file.
- `-i`: edit input files in place; implies `-s`.
- `-s`: process input files separately.
- `-a`: perform strict address and label checks.
- `-b`: enable extended address forms, including `start~step` and `0,/regex/`.
- `-x`: allow text on the same line as `a`, `c`, or `i`.
- `-y`: ensure that script text for text commands ends in a newline.
- `-m bytes`, `-w`, `-D`: managed-memory limit, wildcard expansion, and
  debugging information.
- `-t`: trace execution when the build includes sed tracing.
- `--script-encoding`, `--infile-encoding`, `--outfile-encoding`: encoding
  selection in wide-character builds.

Input line endings default to `auto`, recognizing LF and CRLF.
`--input-line-ending=lf` recognizes only LF and leaves a preceding CR in
editable text. `--input-line-ending=crlf` recognizes only CRLF boundaries.

Output defaults to `--output-line-ending=preserve`. Select `native`, `lf`, or
`crlf` to convert emitted newlines. This does not remove bare CR characters or
add a missing final newline. To remove CR from CRLF input explicitly:

```sh
hawk --sed --input-line-ending=lf --output-line-ending=lf 's/\r//g' input.txt
```

## M4 mode

`hawk --m4` processes macro definitions and text from files, or standard input
when no files are supplied. A filename of `-` selects standard input.

- `-D name[=value]`, `--define name[=value]`: define a macro; an omitted value
  gives an empty definition.
- `-U name`, `--undefine name`: remove a macro definition.
- `-I directory`, `--incdirs directory`: append an include directory.
- `-h`, `--help`: show mode-specific help.

Definition, undefinition, and include-directory options may be repeated. The
engine supports facilities such as `define`, `undefine`, `pushdef`, `popdef`,
`changequote`, `include`, `sinclude`, and diversions. It is not a complete
implementation of GNU m4.

```sh
printf 'PROJECT\n' | hawk --m4 -DPROJECT=hawk
hawk --m4 -I ./macros document.m4
```

## Shell mode

`hawk --sh` runs the embedded simple-command shell. `-c command` executes a
command string; a file operand reads a script. Otherwise commands come from
standard input. Terminal input enables interactive behavior; `-i` requests it
explicitly.

The shell supports pipelines, input/output redirection, here-documents,
background jobs, and terminal job control on supported POSIX platforms.
It does not implement shell loops, `if` statements, or `case` statements.

`--path` selects external command lookup through `PATH`, the default.
`--registered` selects registered commands. The `mode`, `addcmd`, and `delcmd`
builtins change command lookup and registration. Other builtins include
`echo`, `cd`, `pwd`, `export`, `unset`, `jobs`, `wait`, `fg`, `bg`, `kill`, and
`exit`.

```sh
hawk --sh -c 'echo hello | tr a-z A-Z'
hawk --sh -c 'addcmd copy /bin/cat; mode registered; echo hello | copy'
```

# ENVIRONMENT

- **LANG**, **HOME**, **XDG_DATA_HOME**
  Used by the C executable to locate translated diagnostic catalogs.

- **PATH**
  Used for external commands by the embedded shell in PATH mode and by command
  execution facilities according to the selected host implementation.

The interpreter exposes the process environment through `ENVIRON`. Include
and module search paths are set with the options above; do not assume a
`HAWK_INCDIRS` environment variable is read by the executable.

# EXIT STATUS

Successful execution normally returns zero. The interpreter converts the
program's returned value, including a value supplied by `exit`, to an integer
process status when possible. Parsing, runtime, and setup failures return a
nonzero status and print a diagnostic. The operating system may restrict the
representable process-status range.

Other command modes have their own exit-status conventions. Shell mode returns
the command's status, or 2 for shell processing errors.

# EXAMPLES

```sh
# Print the second field of comma-separated input.
printf 'a,b,c\n' | hawk -F, '{ print $2 }'

# Execute a source file with an initialized global.
hawk -v limit=10 -f report.hawk data.txt

# Call a function without entering the record-processing loop.
hawk -c greet 'function greet(name) { print "Hello", name }' Hawk

# Enable extended function calls in a source string.
hawk --xcall=on 'BEGIN { m = @{"f": func(x) { return x * 2 }}; print m.f(21) }'
```

# SEE ALSO

[hawk-language(7)](hawk-language.7.md),
[hawk-modules(7)](hawk-modules.7.md), [hawkgo(1)](hawkgo.1.md).
