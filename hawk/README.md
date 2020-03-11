# Hawk

 - [Language](#language)
 - [Basic Modules](#basic-modules)
 - [Embedding Guide](#embedding-guide)

## Language <a name="language"></a>

Hawk implements most of the AWK programming language elements with extensions.

### Program Structure

A Hawk program is composed of the following elements at the top level.

 - pattern-action block pair
   - BEGIN action block pair
   - END action block pair
   - action block without a pattern
   - pattern without an action block
 - user-defined function
 - @global variable declaration
 - @include directive
 - @pragma directive

However, none of the above is mandatory. Hawk accepts an empty program.

### Pattern-Action Block Pair

A pattern-action pair is composed of a pattern and an action block as shown below:

	pattern {
		statement
		statement
		...
	}

A pattern can be one of the followings when specified:

 - expression
 - first-expression, last-expression
 - *BEGIN*
 - *END*

An action block is a series of statements enclosed in a curly bracket pair. The *BEGIN* and *END* patterns require an action block while normal patterns don't. When no action block is specified for a normal pattern, it is treated
as if `{ print $0; }` is specified.

Hawk executes the action block for the *BEGIN* pattern when it starts executing a program; No start-up action is taken if no *BEGIN* pattern-action pair is specified. If a normal pattern-action pair and/or the *END* 
pattern-action is specified, it reads the standard input stream. For each input line it reads, it checks if a normal pattern expression evaluates to true. For each pattern that evaluates to true, it executes the action block specified for
the pattern. When it reaches the end of the input stream, it executes the action block for the *END* pattern.

Hawk allows zero or more *BEGIN* patterns. When multiple *BEGIN* patterns are specified, it executes their action blocks in their appearance order in the program. The same applies to the *END* patterns and their action blocks. It 
doesn't read the standard input stream for programs composed of BEGIN blocks only whereas it reads the stream as long as there is an action block for the END pattern or a normal pattern. It evaluates an empty pattern to true;
As a result, the action block for an empty pattern is executed for all input lines read.

You can compose a pattern range by putting 2 patterns separated by a comma. The pattern range evaluates to true once the first expression evaluates to true until the last expression evaluates to true.

The following code snippet is a valid Hawk program that prints the string *hello, world* to the console and exits.


	BEGIN {
		print "hello, world";
	}

This program prints "hello, world" followed by "hello, all" to the console.

	BEGIN {
		print "hello, world";
	}
	BEGIN {
		print "hello, all";
	}

For the following text input,

	abcdefgahijklmn
	1234567890
	opqrstuvwxyzabc
	9876543210

this program

	BEGIN { mr=0; my_nr=0; }
	/abc/ { print "[" $0 "]"; mr++; }
	{ my_nr++; }
	END { 
		print "total records: " NR; 
		print "total records selfcounted: " my_nr; 
		print "matching records: " mr; 
	}

produces the output text like this:

	[abcdefgahijklmn]
	[opqrstuvwxyzabc]
	total records: 4
	total records selfcounted: 4
	matching records: 2

See the table for the order of execution indicated by the number and the result 
of pattern evaluation enclosed in parenthesis. The action block is executed if
the evaluation result is true.

|                                     | START-UP | abcdefgahijklmn | 1234567890 | opqrstuvwxyzabc | 9876543210 | SHUTDOWN |
|-------------------------------------|----------|-----------------|------------|-----------------|------------|----------|
| `BEGIN { mr = 0; my_nr=0; }`        | 1(true)  |                 |            |                 |            |          |
| `/abc/ { print "[" $0 "]"; mr++; }` |          | 2(true)         | 4(false)   | 6(true)         | 8(false)   |          |
| `{ my_nr++; }`                      |          | 3(true)         | 5(true)    | 7(true)         | 9(true)    |          |
| `END { print ... }`                 |          |                 |            |                 |            | 10(true) | 

For the same input, this program shows how to use a ranged pattern.

	/abc/,/stu/ { print "[" $0 "]"; }

It produces the output text like this:

	[abcdefgahijklmn]
	[1234567890]
	[opqrstuvwxyzabc]

The regular expression `/abc/` matches the first input line and `/stu/` matches the
third input line. So the range is true between the first input line and the
third input line inclusive.

### Entry Point

The typical execution begins with the BEGIN block, goes through pattern-action blocks, and eaches the END block. If you like to use a function as an entry point, you may set a function name with @pragma entry.

	@pragma entry main

	function main ()
	{
		print "hello, world";
	}


### Values

- unitialized value
- integer
- floating-point number
- string
- byte string
- array
- function
- regular expression

To know the current type of a value, call typename().

	function f() { return 10; }
	BEGIN { 
		a="hello";
		b=12345;
		print typename(a), typename(b), typename(c), typename(f), typename(1.23), typename(B"world");
	}

A regular expression literal is special in that it never appears as an indendent value and still entails a match operation against $0 without an match operator.	

	BEGIN { $0="ab"; print /ab/, typename(/ab/); }

For this reason, there is no way to get the type name of a regular expressin literal.

### Pragmas

A pragma item of the file scope can be placed in any source files.
A pragma item of the global scope can appear only once thoughout the all source files.

|               | Scope  | Values        | Description                                            |
|---------------|--------|---------------|--------------------------------------------------------|
| implicit      | file   | on, off       | allow undeclared variables                             |
| multilinestr  | file   | on, off       | allow a multiline string literal without continuation  |
| entry         | global | function name | change the program entry point                         |
| striprecspc   | global | on, off       |                                                        |
| striprecspc   | global | on, off       | trim leading and trailing spaces when convering a string to a number |


### Module

### Incompatibility with AWK

#### Parameter passing

In AWK, the caller can pass an uninitialized variable as a function parameter and get a changed value if the callled function sets it to an array.


	function q(a) {a[1]=20; a[2]=30;}
	BEGIN { q(x); for (i in x) print i, x[i]; }

In Hawk, you can prefix the pramater name with & to indicate call-by-reference for the same effect.

	function q(&a) {a[1]=20; a[2]=30;}
	BEGIN { q(x); for (i in x) print i, x[i]; }


Alternatively, you may form an array before passing it to a function.

	function q(a) {a[1]=20; a[2]=30;}
	BEGIN { x[3]=99; q(x); for (i in x) print i, x[i]; }'


### Positional variable expression

There are subtle differences when you put an expression for a position variable. In Hawk, most of the ambiguity issues are resolved if you enclose the expression inside parentheses.

|              | HAWK          | AWK             |
|--------------|---------------|-----------------|
| $++$++i      | syntax error  | OK              |
| $(++$(++i))  | OK            | syntax error    |


## Basic Modules <a name="basic-modules"></a>

### sys

### ffi

### mysql

## Embedding Guide <a name="embedding-guide"></a>

To use hawk in your program, do the followings:

- create a hawk instance
- parse a source script
- create a runtime context
- trigger execution on the runtime context
- destroy the runtime context
- destroy the hawk instance

The following sample illustrates the basic steps hightlighed above.

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
		rtx = hawk_rtx_openstd (
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


If you prefer C++, you may use the Hawk/HawkStd wrapper classes to simplify the task. The C++ classes are inferior to the C equivalents in that they don't allow creation of multiple runtime contexts over a single hawk instance.
