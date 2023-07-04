# Hawk

	█░█ ▄▀█ █░█░█ █▄▀
	█▀█ █▀█ ▀▄▀▄▀ █░█

 - [Language](#language)
 - [Modules](#modules)
 - [Embedding Guide](#embedding-guide)

## Language

`Hawk` is an `AWK` interpreter with many extended features implemented by its creator, with 'H' representing the initial of the creator's name. It aims to be an easy-to-embed implementation as well as a standalone tool.

At its core, `Hawk` largely supports all the fundamental features of `AWK`, ensuring compatibility with existing AWK programs and scripts,  while introducing subtle differences in behavior, which will be explained in the [incompatibility](#incompatibility-with-awk) section. The following is an overview of the basic AWK features supported by `Hawk`:

1. Pattern-Action Statements: Hawk operates on a series of pattern-action statements. Each statement consists of a pattern that matches input records and an associated action that is executed when the pattern matches.
1. Record Processing: Hawk processes input data by splitting it into records and fields. Records are typically lines in a file, while fields are segments of each record separated by a field separator (by default, whitespace). This enables powerful text processing capabilities.
1. Built-in Variables: Hawk provides a set of built-in variables that facilitate data manipulation. Commonly used variables include NF (number of fields in the current record), NR (current record number), and $n (the value of the nth field in the current record).
1. Built-in Functions: Hawk offers a rich set of built-in functions to perform various operations on data. These functions include string manipulation, numeric calculations, regular expression matching, and more. You can harness their power to transform and analyze your input data effectively.
1. Output Formatting: Hawk provides flexible control over the formatting and presentation of output. You can customize the field and record separators, control the output field width, and apply formatting rules to align columns.

With these foundational features, Hawk ensures compatibility with existing AWK scripts and enables you to utilize the vast range of AWK resources available.


### Entry Point

The typical execution begins with the `BEGIN` block, proceeds through pattern-action blocks, and concludes with the `END` block. If you would like to use a function as the entry point, you can specify a function name using `@pragma entry`.

```
@pragma entry main

function main ()
{
	print "hello, world";
}
```

### Pragmas

Besides the `entry` pragma, there are other prgrmas available.
 
A pragma item of the file scope can be placed in any source files.
A pragma item of the global scope can appear only once thoughout the all source files.

| Name          | Scope  | Values        | Description                                            |
|---------------|--------|---------------|--------------------------------------------------------|
| implicit      | file   | on, off       | allow undeclared variables                             |
| multilinestr  | file   | on, off       | allow a multiline string literal without continuation  |
| entry         | global | function name | change the program entry point                         |
| striprecspc   | global | on, off       | trim leading and trailing spaces when convering a string to a number |


```
@pragma implicit off
BEGIN {	a = 10; } ## syntax error - undefined identifier 'a'
```

```
@pragma implicit off
BEGIN {	@local a; a = 10; } # syntax ok - a is declared before use.
```

### @include and @include_once

The @include directive inserts the contents of the file specified in the following string as if they appeared in the source stream being processed.

Assuming the `hello.inc` file contains the print_hello() function as shown below,

```
function print_hello() { print "hello\n"; }
```

You may include the the file and use the function.

```
@include "hello.inc";
BEGIN { print_hello(); }
```

The semicolon after the included file name is optional. You could write @include "hello.inc" without the ending semicolon.

@include_once is similar to @include except it doesn't include the same file multiple times.

```
@include_once "hello.inc";
@include_once "hello.inc";
BEGIN { print_hello(); }
```

In this example, `print_hello()` is not included twice.

You may use @include and @include_once inside a block as well as at the top level.

```
BEGIN {
	@include "init.inc";
	...
}
```

### Comments

`Hawk` supports a single-line commnt that begins with a hash sign # and the C-style multi-line comment.

```
x = y; # assign y to x.
/*
this line is ignored.
this line is ignored too.
*/
```

### Reserved Words

The following words are reserved and cannot be used as a variable name, a parameter name, or a function name.

 - @abort
 - @global
 - @include
 - @include_once
 - @local
 - @pragma
 - @reset
 - BEGIN
 - END
 - break
 - continue
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

However, these words can be used as normal names in the context of a module call. For example, mymod::break. In practice, the predefined names used for built-in commands, functions, and variables are treated as if they are reserved since you can't create another denifition with the same name.

### Values

- unitialized value
- integer
- floating-point number
- string
- byte string
- array - light-weight array with numeric index only
- map - conventional AWK array
- function
- regular expression

To know the current type of a value, call `hawk::typename()`.

```
function f() { return 10; }
BEGIN { 
	a="hello";
	b=12345;
	print hawk::typename(a), hawk::typename(b), hawk::typename(c), hawk::typename(f), hawk::typename(1.23), hawk::typename(B"world");
}
```

A regular expression literal is special in that it never appears as an indendent value and still entails a match operation against $0 without an match operator.	

```
BEGIN { $0="ab"; print /ab/, hawk::typename(/ab/); }
```

For this reason, there is no way to get the type name of a regular expressin literal.

### Numbers ###

An integer begins with a numeric digit between 0 and 9 inclusive and can be
followed by more numeric digits. If an integer is immediately followed by a
floating point, and optionally a series of numeric digits without whitespaces,
it becomes a floting-point number. An integer or a simple floating-point number
can be followed by e or E, and optionally a series of numeric digits with a
optional single sign letter. A floating-point number may begin with a floting
point with a preceeding number.

- 369   # integer
- 3.69  # floating-pointe number
- 13.   # 13.0
- .369  # 0.369
- 34e-2 # 34 * (10 ** -2)
- 34e+2 # 34 * (10 ** 2)
- 34.56e # 34.56
- 34.56E3

An integer can be prefixed with 0x, 0, 0b for a hexa-decimal number, an octal
number, and a binary number respectively. For a hexa-decimal number, letters
from A to F can form a number case-insenstively in addition to numeric digits.

- 0xA1   # 161
- 0xB0b0 # 45232
- 020    # 16
- 0b101  # 5

If the prefix is not followed by any numeric digits, it is still a valid token
and represents the value of 0.

- 0x # 0x0 but not desirable.
- 0b # 0b0 but not desirable.


### Modules

Hawk supports various modules.


#### String
The *str* module provides an extensive set of string manipulation functions.

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
- str::match - equivalent to match
- str::normspace
- str::printf - equivalent to sprintf
- str::rindex
- str::rtrim
- str::split - equivalent to split
- str::sub - equivalent to sub
- str::substr - equivalent to substr
- str::tocharcode - get the numeric value of the first character
- str::tolower - equivalent to tolower
- str::tonum - convert a string to a number. a numeric value passed as a parameter is returned as it is. the leading prefix of 0b, 0, and 0x specifies the radix of 2, 8, 16 repectively. conversion stops when the end of the string is reached or the first invalid character for conversion is encountered.
- str::toupper - equivalent to toupper
- str::trim


#### System

The *sys* module provides various functions concerning the underlying operation system.

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

```
BEGIN {
	f = sys::open("/etc/sysctl.conf", sys::O_RDONLY);
	while (sys::read(f, x, 10) > 0) printf (B"%s", x);
	sys::close (f);
}
```

You can map a raw file descriptor to a handle created by this module and use it.

```
BEGIN {
	a = sys::openfd(1);
	sys::write (a, B"let me write something here\n");
	sys::close (a, sys::C_KEEPFD); ## set C_KEEPFD to release 1 without closing it.
	##sys::close (a);
	print "done\n";
}
```


Creating pipes and sharing them with a child process is not big an issue.

```
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

```
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

```
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

```
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

```
BEGIN { 
	if (sys::getnwifcfg("lo", sys::NWIFCFG_IN6, x) <= -1)
		print sys::errmsg();
	else
		for (i in x) print i, x[i]; 
}
```

Socket functions are available.

```
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

```
BEGIN {
	ffi = ffi::open();
	if (ffi::call(ffi, r, @B"getenv", @B"s>s", "PATH") <= -1) print ffi::errmsg();
	else print r;
	ffi::close (ffi);
}
```

### mysql

```
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

### Incompatibility with AWK

#### Parameter passing

In AWK, it is possible for the caller to pass an uninitialized variable as a function parameter and obtain a modified value if the called function sets it to an array.

```
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

```
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

```
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


| Expression   | HAWK          | AWK             |
|--------------|---------------|-----------------|
| `$++$++i`    | syntax error  | OK              |
| `$(++$(++i))`| OK            | syntax error    |


## Embedding Guide

To use hawk in your program, do the followings:

- create a hawk instance
- parse a source script
- create a runtime context
- trigger execution on the runtime context
- destroy the runtime context
- destroy the hawk instance

The following sample illustrates the basic steps highlighted above.

```
#include <hawk-std.h>
#include <stdio.h>
#include <string.h>

static const hawk_bch_t* src =
	"BEGIN {"
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
		fprintf (stderr, "ERROR: cannot open hawk\n");
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
		hawk_logbfmt (hawk, HAWK_LOG_STDERR, "ERROR(parse): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	/* create a runtime context needed for execution */
	rtx = hawk_rtx_openstd(
		hawk, 
		0,
		HAWK_T("hawk02"),
		HAWK_NULL,  /* stdin */
		HAWK_NULL,  /* stdout */
		HAWK_NULL   /* default cmgr */
	);
	if (!rtx)
	{
		hawk_logbfmt (hawk, HAWK_LOG_STDERR, "ERROR(rtx_open): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	/* execute the BEGIN/pattern-action/END blocks */
	retv = hawk_rtx_loop(rtx);
	if (!retv)
	{
		hawk_logbfmt (hawk, HAWK_LOG_STDERR, "ERROR(rtx_loop): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	/* lowered the reference count of the returned value */
	hawk_rtx_refdownval (rtx, retv);
	ret = 0;

oops:
	if (rtx) hawk_rtx_close (rtx); /* destroy the runtime context */
	if (hawk) hawk_close (hawk); /* destroy the hawk instance */
	return -1;
}
```

If you prefer C++, you may use the Hawk/HawkStd wrapper classes to simplify the task. The C++ classes are inferior to the C equivalents in that they don't allow creation of multiple runtime contexts over a single hawk instance. Here is the sample code that prints "hello, world".

```
#include <Hawk.hpp>
#include <stdio.h>

int main ()
{
	HAWK::HawkStd hawk;

	if (hawk.open() <= -1)
	{
		fprintf (stderr, "unable to open hawk - %s\n", hawk.getErrorMessageB());
		return -1;
	}

	HAWK::HawkStd::SourceString s("BEGIN { print \"hello, world\"; }");
	if (hawk.parse(s, HAWK::HawkStd::Source::NONE) == HAWK_NULL)
	{
		fprintf (stderr, "unable to parse - %s\n", hawk.getErrorMessageB());
		hawk.close ();
		return -1;
	}

	HAWK::Hawk::Value vr;
	hawk.exec (&vr, HAWK_NULL, 0);

	hawk.close ();
	return 0;
}
```
