/*
 * $Id$
 *
    Copyright (c) 2006-2020 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
  tre-stack.c - Simple stack implementation

This is the license, copyright notice, and disclaimer for TRE, a regex
matching package (library and tools) with support for approximate
matching.

Copyright (c) 2001-2009 Ville Laurikari <vl@iki.fi>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "tre-prv.h"
#include "tre-stack.h"

union tre_stack_item
{
	void *voidptr_value;
	int int_value;
};

struct tre_stack_rec
{
	hawk_gem_t* gem;
	int size;
	int max_size;
	int increment;
	int ptr;
	union tre_stack_item *stack;
};


tre_stack_t* tre_stack_new(hawk_gem_t* gem, int size, int max_size, int increment)
{
	tre_stack_t *s;

	s = xmalloc(gem, sizeof(*s));
	if (s != NULL)
	{
		s->stack = xmalloc(gem, sizeof(*s->stack) * size);
		if (s->stack == NULL)
		{
			xfree(gem, s);
			return NULL;
		}
		s->size = size;
		s->max_size = max_size;
		s->increment = increment;
		s->ptr = 0;
		s->gem = gem;
	}
	return s;
}

void
tre_stack_destroy(tre_stack_t *s)
{
	xfree(s->gem,s->stack);
	xfree(s->gem,s);
}

int
tre_stack_num_objects(tre_stack_t *s)
{
	return s->ptr;
}

static reg_errcode_t
tre_stack_push(tre_stack_t *s, union tre_stack_item value)
{
	if (s->ptr < s->size)
	{
		s->stack[s->ptr] = value;
		s->ptr++;
	}
	else
	{
/* HAWK  added check for s->max_size > 0 
		if (s->size >= s->max_size)*/
		if (s->max_size > 0 && s->size >= s->max_size)
		{
			DPRINT(("tre_stack_push: stack full\n"));
			return REG_ESPACE;
		}
		else
		{
			union tre_stack_item *new_buffer;
			int new_size;
			DPRINT(("tre_stack_push: trying to realloc more space\n"));
			new_size = s->size + s->increment;
/* HAWK  added check for s->max_size > 0 
			if (new_size > s->max_size) */
			if (s->max_size > 0 && new_size > s->max_size) 
				new_size = s->max_size;
			new_buffer = xrealloc(s->gem, s->stack, sizeof(*new_buffer) * new_size);
			if (new_buffer == NULL)
			{
				DPRINT(("tre_stack_push: realloc failed.\n"));
				return REG_ESPACE;
			}
			DPRINT(("tre_stack_push: realloc succeeded.\n"));
			assert(new_size > s->size);
			s->size = new_size;
			s->stack = new_buffer;
			tre_stack_push(s, value);
		}
	}
	return REG_OK;
}

#define define_pushf(typetag, type)  \
  declare_pushf(typetag, type) {     \
    union tre_stack_item item;	     \
    item.typetag ## _value = value;  \
    return tre_stack_push(s, item);  \
}

define_pushf(int, int)
define_pushf(voidptr, void *)

#define define_popf(typetag, type)		    \
  declare_popf(typetag, type) {			    \
    return s->stack[--s->ptr].typetag ## _value;    \
  }

define_popf(int, int)
define_popf(voidptr, void *)

/* EOF */
