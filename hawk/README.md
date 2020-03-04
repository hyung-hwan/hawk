# Hawk

# Table of Contents
- [Language](#language)
- [Embedding Guide](#embedding-guide)

#### TODO: unary bitwise not
#### TODO: fix -F or --field-separator to hawk.


## Language <a name="language"></a>

Hawk implements most of the AWK programming language elements with extensions.


### Entry Point

You may change the entry point of your script by setting a function name with @pragma entry.

<pre>
@pragma entry main

function main ()
{
	print "hello, world";
}
</pre>

### Value

- unitialized value
- integer
- floating-point number
- string
- byte string
- array

### Module



### Incompatibility with AWK

#### Parameter passing

In AWK, the caller can pass an uninitialized variable as a function parameter and get a changed value if the callled function sets it to an array.

<pre>
function q(a) {a[1]=20; a[2]=30;}
BEGIN { q(x); for (i in x) print i, x[i]; }
</pre>

In Hawk, you can prefix the pramater name with & to indicate call-by-reference for the same effect.
<pre>
function q(&a) {a[1]=20; a[2]=30;}
BEGIN { q(x); for (i in x) print i, x[i]; }
</pre>

Alternatively, you may form an array before passing it to a function.
<pre>
function q(a) {a[1]=20; a[2]=30;}
BEGIN { x[3]=99; q(x); for (i in x) print i, x[i]; }'
</pre>


## Embedding Guide <a name="embedding-guide"></a>
