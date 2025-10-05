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
	- [Pragmas](#pragmas)
		- [@pragma entry](#pragma-entry)
		- [@pragma implicit](#pragma-implicit)
		- [@pragma striprecspc](#pragma-striprecspc)
	- [@include and @include\_once](#include-and-include_once)
	- [Comments](#comments)
	- [Reserved Words](#reserved-words)
	- [Values](#values)
		- [Numbers](#numbers)
		- [Map](#map)
		- [Array](#array)
		- [Multidimensional Map/Array](#multidimensional-maparray)
	- [Operators](#operators)
	- [Control Strucutres](#control-strucutres)
	- [Function](#function)
	- [Variable](#variable)
		- [Built-in Variable](#built-in-variable)
	- [Pipes](#pipes)
	- [Garbage Collection](#garbage-collection)
	- [Modules](#modules)
		- [Hawk](#hawk-1)
		- [String](#string)
		- [System](#system)
		- [ffi](#ffi)
		- [mysql](#mysql)
	- [Incompatibility with AWK](#incompatibility-with-awk)
		- [Parameter passing](#parameter-passing)
	- [Positional variable expression](#positional-variable-expression)

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
	memset (&psin, 0, HAWK_SIZEOF(psin));
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

`Hawk` is an AWK interpreter created by an individual whose name starts with H, which inspired the H in the name. It serves a dual purpose: as an embeddable AWK engine for integration into other applications, and as a standalone command-line tool for general use.

At its core, `Hawk` supports the fundamental features of AWK, ensuring compatibility with most existing AWK programs and scripts. However, it introduces some subtle differences in behavior compared to traditional AWK implementations, which are detailed in the [Incompatibility with AWK](#incompatibility-with-awk) section.
 section.

`Hawk` follows the standard AWK execution flow: the BEGIN block is executed first, followed by pattern-action blocks, and finally the END block.

1. `BEGIN` Block: Executed before any input is processed. It is typically used for initializations, such as setting variable values or defining functions to be used later in the script.
1. Pattern-Action Blocks: After the BEGIN block, `Hawk` reads input line by line (or record by record, based on the record separator RS). For each input line, `Hawk` checks if it matches the specified pattern. If it does, the associated action block is executed.
1. `END` Block: Executed after all input has been processed. It is typically used for final operations, such as printing summaries or closing files.

Here's a sample code that demonstrates the basic `BEGIN`, pattern-action, and `END` loop in Hawk:

```awk
BEGIN {
	print "Starting the script..."
	total = 0
}
/^[0-9]+$/ { # Pattern-action block to sum up the numbers
	total += $0  # Add the current line (which is a number) to the total
}
END {
	print "The sum of all numbers is:", total
}
```

In this example:

1. The `BEGIN` block is executed first, printing the message "Starting the script..." and initializing the total variable to 0.
1. For each input line, Hawk checks if it matches the regular expression `/^[0-9]+$/` (which matches lines containing only digits). If a match is found, the action block `{ total += $0 }` is executed, adding the current line (treated as a number) to the total variable.
1. After processing all input lines, the `END` block is executed, printing the final message "The sum of all numbers is: `total`", where `total` is the accumulated sum of all numbers from the input.

You can provide input to this script in various ways, such as piping from another command, reading from a file, or entering input interactively. For example:

```sh
$ echo -e "42\n3.14\n100" | hawk -f sum.hawk
Starting the script...
The sum of all numbers is: 142
```

In this example, the `sum.hawk` file contains the Hawk script that sums up the numbers from the input. The input is provided via the `echo` command, which outputs three lines: 42, 3.14 (ignored because it doesn't match the pattern), and 100. The script sums up the numbers 42 and 100, resulting in a total of 142.

It's important to note that if there is no action-pattern block or `END` block present in the Hawk script, the interpreter will not wait for input records. In this case, the script will execute only the `BEGIN` block (if present) and then immediately terminate.

However, if an action-pattern block or an END block is present in the script, even if there is no action-pattern block, Hawk will wait for input records or lines. This behavior is consistent with the way awk was designed to operate: it expects input data to process unless the script explicitly indicates that no input is required.

For example, consider the following command:

```sh
$ ls -l | hawk 'END { print NR; }'
```

In this case, the Hawk script contains only an `END` block that prints the value of the `NR`(Number of Records) variable, which keeps track of the number of input records processed. Since there is an END block present, Hawk will wait for input records from the `ls -l` command, process them (though no action is taken for each record), and finally execute the END block, printing the total number of records processed.

Additionally, Hawk introduces the `@pragma entry` feature, which allows you to change the entry point of your script to a custom function instead of the default `BEGIN` block. This feature will be covered in the [Pragmas](#pragmas) section.

## Pragmas

The `@pragma` keyword enables you to modify Hawk’s behavior. You can place a pragma item at the file scope within any source files. Additionally, a pragma item at the global scope can appear only once across all source files.

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

In addition to the standard `BEGIN` and `END` blocks found in awk, Hawk introduces the `@pragma entry` feature, which allows you to specify a custom entry point function. This can be useful when you want to bypass the default `BEGIN` block behavior and instead start executing your script from a specific function.

The `@pragma entry` pragma is used to define the entry point function, like this:

```awk
@pragma entry main;
function main () { print "hello, world"; }
```

In this example, the `main` function is set as the entry point for script execution. When the script is run, Hawk will execute the code inside the main function instead of the `BEGIN` block.

You can also pass arguments to the entry point function by defining it with parameters:

```awk
@pragma entry main
function main(arg1, arg2) {
	print "Arguments:", arg1, arg2
}
```

In this example, let's assume the script is saved as `main.hawk`. The `main` function is set as the entry point for script execution, and it accepts two arguments, `arg1` and `arg2`. Then, when executing the `main.hawk` script, you can provide the arguments like this:

```sh
$ hawk -f main.hawk arg1_value arg2_value
```

This will cause Hawk to execute the code inside the main function, passing `arg1_value` and `arg2_value` as the respective values for `arg1` and `arg2`.

This flexibility in specifying the entry point can be useful in various scenarios, such as:

- Modular Script Design: You can organize your script into multiple functions and specify the entry point function, making it easier to manage and maintain your code.
- Command-line Arguments: By defining the entry point function with parameters, you can easily accept and process command-line arguments passed to your script.
- Testing and Debugging: When working on specific parts of your script, you can temporarily set the entry point to a different function, making it easier to test and debug that particular functionality.
- Integration with Other Systems: If you need to embed Hawk scripts within a larger application or system, you can use the `@pragma entry` feature to specify the function that should be executed as the entry point, enabling better integration and control over the script execution flow.

If you don't know the number of arguments in advance, you can use the ellipsis `...` in the parameter list and access the variadic arguments using `@argv()` and `@argc()`.

```awk
@pragma entry main
function main(...) {
	@local i
	for (i = 0; i < @argc; i++) printf("%s:", @argv[i])
	print ""
}
```

In this example, the `main` function can accept variable number of arguments.

```sh
$ hawk -f main.hawk 10 20 30 40 50
```

The expected output of the above command is `10:20:30:40:50:`.

It's important to note that if you don't define an entry point function using `@pragma entry`, Hawk will default to the standard awk behavior and execute the `BEGIN` block first, followed by the pattern-action blocks, and finally the `END` block.

Overall, the @pragma entry feature in Hawk provides you with greater flexibility and control over the execution flow of your scripts, allowing you to tailor the entry point to your specific needs and requirements.

### @pragma implicit

Hawk also introduces the `@pragma implicit` feature, which allows you to enforce variable declarations. Unlike traditional awk, where local variable declarations are not necessary, Hawk can require you to declare variables before using them. This is controlled by the `@pragma implicit` pragma:

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
With the `@local` declaration, the variable `a` is explicitly declared, allowing it to be used without triggering a syntax error.
This feature can be beneficial for catching potential variable misspellings or unintended uses of global variables, promoting better code quality and maintainability.

If you don't want to enforce variable declarations, you can simply omit the `@pragma implicit off` directive or specify `@pragma implicit on`, and Hawk will behave like traditional awk, allowing implicit variable declarations.

### @pragma striprecspc

The `@pragma striprecspc` directive in Hawk controls how the interpreter handles leading and trailing blank fields in input records when using a regular expression as the field separator (FS).

When you set `FS` to a regular expression that matches one or more whitespace characters (e.g., FS="[[:space:]]+"), Hawk will split the input records into fields based on that pattern. By default, Hawk follows the behavior of traditional awk, which means that leading and trailing blank fields are preserved.

However, Hawk introduces the `@pragma striprecspc` directive, which allows you to change this behavior. Here's how it works:

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

When `@pragma striprecspc on` is set, Hawk will automatically remove any leading and trailing blank fields from the input records. In the example above, the input string ' a b c d ' has a leading and trailing space, which would normally result in two additional blank fields. However, with `@pragma striprecspc on`, these blank fields are stripped, and the resulting `NF`(number of fields) is 4, corresponding to the fields "a", "b", "c", and "d".

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

When `@pragma striprecspc off` is set (or the directive is omitted, as this is the default behavior), Hawk preserves any leading and trailing blank fields in the input records. In the example above, the input string ' a b c d ' has a leading and trailing space, resulting in two additional blank fields. The `NF`(number of fields) is now 6, with the first and last fields being empty, and the remaining fields containing "a", "b", "c", and "d".

## @include and @include_once

The `@include` directive inserts the contents of the file specified in the following string as if they appeared in the source stream being processed.

Assuming the `hello.inc` file contains the print_hello() function as shown below,

```awk
function print_hello() { print "hello\n"; }
```

You may include the the file and use the function.

```awk
@include "hello.inc";
BEGIN { print_hello(); }
```

The semicolon after the included file name is optional. You could write `@include "hello.inc"` without the ending semicolon.

`@include_once` is similar to `@include` except it doesn't include the same file multiple times.

```awk
@include_once "hello.inc";
@include_once "hello.inc";
BEGIN { print_hello(); }
```

In this example, `print_hello()` is not included twice.

You may use @include and @include_once inside a block as well as at the top level.

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

## Values

- uninitialized value
- character - 'C'
- byte character - @b'B'
- integer
- floating-point number
- string - "string"
- byte string - @b"byte string"
- array - light-weight array with numeric index only
- map - conventional AWK array
- function
- regular expression
- reference to a value

To know the current type name of a value, call `hawk::typename()`.

```awk
function f() { return 10; }
BEGIN { 
	a="hello";
	b=12345;
	print hawk::typename(a), hawk::typename(b), hawk::typename(c), hawk::typename(f), hawk::typename(1.23), hawk::typename(B"world");
}
```

`hawk::type()` returns a numeric type code:
- hawk::VAL_ARRAY
- hawk::VAL_BCHAR
- hawk::VAL_CHAR
- hawk::VAL_FLT
- hawk::VAL_INT
- hawk::VAL_MAP
- hawk::VAL_MBS
- hawk::VAL_NIL
- hawk::VAL_STR
- hawk::VAL_REF
- hawk::VAL_REX

A regular expression literal is special in that it never appears as an independent value and still entails a match operation against $0 without an match operator.

```awk
BEGIN { $0="ab"; print /ab/, hawk::typename(/ab/); }
```

For this reason, there is no way to get the type name of a regular expression literal.

### Numbers

An integer begins with a numeric digit between 0 and 9 inclusive and can be
followed by more numeric digits. If an integer is immediately followed by a
floating point, and optionally a series of numeric digits without whitespaces,
it becomes a floating-point number. An integer or a simple floating-point number
can be followed by e or E, and optionally a series of numeric digits with a
optional single sign letter. A floating-point number may begin with a floating
point with a preceding number.

- `369`   # integer
- `3.69`  # floating-point number
- `13.`   # 13.0
- `.369`  # 0.369
- `34e-2` # 34 * (10 ** -2)
- `34e+2` # 34 * (10 ** 2)
- `34.56e` # 34.56
- `34.56E3`

An integer can be prefixed with 0x, 0, 0b for a hexa-decimal number, an octal
number, and a binary number respectively. For a hexa-decimal number, letters
from A to F can form a number case-insensitively in addition to numeric digits.

- `0xA1`   # 161
- `0xB0b0` # 45232
- `020`    # 16
- `0b101`  # 5

If the prefix is not followed by any numeric digits, it is still a valid token
and represents the value of 0.

- `0x` # 0x0 but not desirable.
- `0b` # 0b0 but not desirable.


### Map

```awk
BEGIN {
	@local x, i;
	x = hawk::map(); ## you can omit this line
	x["one"] = 1;
	x["two"] = 2;
	x[199] = 3;
	for (i in x) print i, x[i];
}
```


### Array

```awk
BEGIN {
	@local x, i
	x = hawk::array()
	for (i = 0; i < 20; i++) x[i] = i;
	print hawk::isarray(x), hawk::ismap(x)
	print "--------------";
	for (i in x) print i, x[i];
}
```

### Multidimensional Map/Array

```awk
BEGIN {
        @local x, i, j, k;
        k = hawk::array();

        x = hawk::array();
        k[0] = x;
        k[1] = x;

        for (i = 0; i < 20; i++) x[i] = i;
        k[0][0] = 99;
        for (j in k)
                for (i in x) print j, i, x[i];
}
```

## Operators

- ===, ==, !==, !=
- +, -, *, %
- &&, ||, &, |

## Control Structures

Hawk supports various control structures for flow control and iteration, similar to those found in awk.

The `if` statement follows the same syntax as in awk and other programming languages. It allows you to execute a block of code conditionally based on a specified condition.

```awk
if (condition) {
	## statements
} else if (another_condition) {
	## other statements
} else {
	## default statements
}
```

The `switch` statement allows the result of an expression to be tested against a list of values.
```awk
switch (expression) {
case value:
    ## statements
...
default:
    ## statements
}
```

The `while` loop is used to repeatedly execute a block of code as long as a specific condition is true.
```awk
while (condition) {
	# statements
}
```

The `do`-`while` loop is similar to the `while` loop, but it guarantees that the code block will be executed at least once, as the condition is evaluated after the first iteration.
```awk
do {
	# statements
} while (condition)
```

The `for` loop follows the same syntax as in awk and allows you to iterate over a range of values or an array.
```awk
for (initialization; condition; increment/decrement) {
	## statements
}
```
You can also use the `fo`r loop to iterate over the elements of an array:
```awk
for (index in array) {
	## statements using array[index]
}
```

Hawk also supports the `break` and `continue` statements, which work the same way as in awk and other programming languages. The `break` statement is used to exit a loop prematurely, while `continue` skips the remaining statements in the current iteration and moves to the next iteration.

TODO:
`return`
`exit`
`nextfile`
`nextofile`

Here are some examples demonstrating the usage of control structures in Hawk.

- Check if a number is even or odd
```awk
{
	if ($1 % 2 == 0) {
		print $1, "is an even number"
	} else {
		print $1, "is an odd number"
	}
}
```

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

The syntax and behavior of these structures are largely consistent with awk, making it easy for awk users to transition to Hawk and leverage their existing knowledge.

## Function

Hawk supports user-defined functions, enabling developers to break down complex logic into modular component for reuse. Hawk also provides a wide range of built-in functions that extend its capabilities for various tasks, such as string manipulation, array handling, and more.

To define a function in Hawk, you use the function keyword followed by the function name and a set of parentheses to enclose the optional function parameters:

```awk
function function_name(parameter1, parameter2, ...) {
	## function body
	## statements
	return value
}
```

Functions in Hawk can accept parameters, perform operations, and optionally return a value using the `return` statement.

Here's an example of a function that calculates the factorial of 10:

```awk
function factorial(n) {
	if (n <= 1) {
		return 1
	} else {
		return n * factorial(n - 1)
	}
}

BEGIN {
	num = 10
	result = factorial(num)
	print "The factorial of", num, "is", result
}
```

If no `return` statement is encountered, the function returns `@nil`, which is Hawk's equivalent of `nil` or `null` in other programming languages.

```awk
function a() { k=999; }
BEGIN { k=a(); print k === @nil, k === "", k == ""; }
```

The expected output of the above example code is `1 0 1`.
- `k === @nil`: This expression evaluates to 1 (true) because `k` is indeed equal to `@nil` when using the type-precise `===` operator.
- `k === ""`: This expression evaluates to 0 (false) because `k` is not equal to an empty string when using the type-precise `===` operator.
- `k == ""`: This expression evaluates to 1 (true) because `@nil` is considered equal to an empty string when using the double equal sign `==` operator.

Functions can be called from various contexts, including `BEGIN`, pattern-action blocks, and `END` blocks, as well as from other functions. They can be defined before or after they are used, as Hawk resolves function references.

You can pass fewer arguments than the number of declared parameters to a function. In such cases, the missing parameters are treated as having `@nil`.

Here's an example to illustrate this behavior:

```awk
function greet(name, greeting) {
    if (greeting == "") {
        greeting = "Hello"
    }
    print greeting, name
}

BEGIN {
    greet("Alice", "Hi")     ## Output: Hi Alice
    greet("Bob")             ## Output: Hello Bob
    greet()                  ## Output: Hello
}
```

In the above example:

1. The `greet` function is defined with two parameters: `name` and `greeting`.
1. In the first function call `greet("Alice", "Hi")`, both arguments are provided, so name is assigned `"Alice"`, and greeting is assigned `"Hi"`.
1. In the second function call `greet("Bob")`, only one argument is provided. Therefore, name is assigned `"Bob"`, and greeting is assigned `@nil`. The function checks if greeting is empty and assigns the default value `"Hello"`.
1. In the third function call `greet()`, no arguments are provided. Both `name` and `greeting` are assigned `@nil`. The function then assigns the default value `"Hello"` to greeting and prints `"Hello"`.

However, it's important to note that you cannot pass more arguments than the number of declared parameters in a function. If you attempt to do so, Hawk will raise an error.

## Variable

Variables can be used to store and manipulate data. There are two types of variables:

- Built-in Variables: These are predefined variables provided by the awk language itself. They are used for specific purposes and contain information about the input data or the state of the program.
- User-defined Variables: These are variables created and used by the programmer to store and manipulate data as needed within the program.

You can declare variables explicitly using the following syntax:

1. Local Variables:
   - Declared using `@local var_1, var_2, ...`
   - These variables are scoped within the current block or function.
1. Global Variables:
   - Declared using `@global var_1, var_2, ...`
   - These variables are accessible throughout the entire Hawk program.

While explicit variable declaration is supported, Hawk also maintains compatibility with awk by allowing implicit variable creation and usage. See [@pragma implicit](#pragma-implicit) on how to control this behavior.

```awk
@global count, total;  # Global variables

BEGIN {
	@local i, j;  ## Local variables in the BEGIN block
	count = 0;
	total = 0;
}

{
	@local value;  ## Local variable in the main block
	value = $1 + $2;
	count++;
	total += value;
}

END {
	print "Total count:", count;
	print "Sum of values:", total;
}
```

In this example:

- `count` and `total` are global variables declared using `@global`.
- `i` and `j` are local variables declared in the `BEGIN` block using `@local`.
- `value` is a local variable declared in the main block using `@local`.


### Built-in Variable

| Variable     | Description |
|--------------|-------------|
| CONVFMT      |             |
| FILENAME     |             |
| FNR          | File Number of Records, It reset to 1 for each new input file |
| FS           | Field Separator, specifies the character(s) that separate fields (columns) in an input record. The default is whitespace. |
| IGNORECASE   |             |
| NF           | Number of Fields (columns) in the current input record |
| NR           | Number of Records processed so far |
| NUMSTRDETECT |             |
| OFILENAME    |             |
| OFMT         |             |
| OFS          |             |
| ORS          |             |
| RLENGTH      |             |
| RS           | Record Separator, specifies the character(s) that separate input records (lines). The default is a newline `"\n"` |
| RSTART       |             |
| SCRIPTNAME   |             |
| STRIPRECSPC  |             |
| STRIPSTRSPC  |             |
| SUBSPEP      |             |

If `FS` is a string beginning with a question mark(`?`) followed by four characters, those characters define special quoting behavior in this order:
- Separator
- Escaper
- Left quote
- Right quote

When the escaper, left quote, and right quote are all the same (for example, `?,"""`), you must repeat that character twice to represent it literally.

In this specific case - when `FS` is in quoting form and the escaper, left quote, and right quote are identical - if `RS` is unset or set to `@nil`, then records may span multiple lines. This allows fields enclosed in quotes to contain embedded newlines.

```sh
$ echo -e 'the tiger, "pounced on\n""me"""' | hawk -v FS='?,"""' '{ for (i = 0; i <= NF; i++) print i, "[" $i "]"; }'
0 [the tiger, "pounced on
""me"""]
1 [the tiger]
2 [pounced on
"me"]
```

## Pipes

```awk
BEGIN {
	while (("ls -laF" | getline x) > 0) print "\t", x;
	close ("ls -laF");
}
```

```awk
{ print $0 | "cat" }
END { close("cat"); print "ENDED"; }
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

- str::fromcharcode
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
- str::tocharcode - get the numeric value of the first character
- str::tolower - equivalent to tolower
- str::tonum - convert a string to a number. a numeric value passed as a parameter is returned as it is. the leading prefix of 0b, 0, and 0x specifies the radix of 2, 8, 16 respectively. conversion stops when the end of the string is reached or the first invalid character for conversion is encountered.
- str::toupper - equivalent to toupper
- str::trim


### System

The `sys` module provides various functions concerning the underlying operation system.

- sys::chmod
- sys::close
- sys::closedir
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
	while (sys::read(f, x, 10) > 0) printf (B"%s", x);
	sys::close (f);
}
```

You can map a raw file descriptor to a handle created by this module and use it.

```awk
BEGIN {
	a = sys::openfd(1);
	sys::write (a, B"let me write something here\n");
	sys::close (a, sys::C_KEEPFD); ## set C_KEEPFD to release 1 without closing it.
	##sys::close (a);
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
