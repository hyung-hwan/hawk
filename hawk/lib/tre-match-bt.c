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
  tre-match-backtrack.c - TRE backtracking regex matching engine

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

/*
  This matcher is for regexps that use back referencing.  Regexp matching
  with back referencing is an NP-complete problem on the number of back
  references.  The easiest way to match them is to use a backtracking
  routine which basically goes through all possible paths in the TNFA
  and chooses the one which results in the best (leftmost and longest)
  match.  This can be spectacularly expensive and may run out of stack
  space, but there really is no better known generic algorithm.	 Quoting
  Henry Spencer from comp.compilers:
  <URL: http://compilers.iecc.com/comparch/article/93-03-102>

    POSIX.2 REs require longest match, which is really exciting to
    implement since the obsolete ("basic") variant also includes
    \<digit>.  I haven't found a better way of tackling this than doing
    a preliminary match using a DFA (or simulation) on a modified RE
    that just replicates subREs for \<digit>, and then doing a
    backtracking match to determine whether the subRE matches were
    right.  This can be rather slow, but I console myself with the
    thought that people who use \<digit> deserve very slow execution.
    (Pun unintentional but very appropriate.)

*/

#include "tre-prv.h"
#include "tre-match-ut.h"
#include "tre-mem.h"

typedef struct
{
	int pos;
	const char *str_byte;
#ifdef TRE_WCHAR
	const hawk_uch_t *str_wide;
#endif /* TRE_WCHAR */
	tre_tnfa_transition_t *state;
	int state_id;
	int next_c;
	int *tags;
#ifdef TRE_MBSTATE
	hawk_mbstate_t mbstate;
#endif /* TRE_MBSTATE */
} tre_backtrack_item_t;

typedef struct tre_backtrack_struct
{
	tre_backtrack_item_t item;
	struct tre_backtrack_struct *prev;
	struct tre_backtrack_struct *next;
} *tre_backtrack_t;

#ifdef TRE_WCHAR
#define BT_STACK_WIDE_IN(_str_wide)	stack->item.str_wide = (_str_wide)
#define BT_STACK_WIDE_OUT		(str_wide) = stack->item.str_wide
#else /* !TRE_WCHAR */
#define BT_STACK_WIDE_IN(_str_wide)
#define BT_STACK_WIDE_OUT
#endif /* !TRE_WCHAR */

#ifdef TRE_MBSTATE
#define BT_STACK_MBSTATE_IN  stack->item.mbstate = (mbstate)
#define BT_STACK_MBSTATE_OUT (mbstate) = stack->item.mbstate
#else /* !TRE_MBSTATE */
#define BT_STACK_MBSTATE_IN
#define BT_STACK_MBSTATE_OUT
#endif /* !TRE_MBSTATE */

#define tre_bt_mem_new		  tre_mem_new
#define tre_bt_mem_alloc	  tre_mem_alloc
#define tre_bt_mem_destroy	  tre_mem_destroy


#define BT_STACK_PUSH(_hawk, _pos, _str_byte, _str_wide, _state, _state_id, _next_c, _tags, _mbstate) \
  do									      \
    {									      \
      int i;								      \
      if (!stack->next)							      \
	{								      \
	  tre_backtrack_t s;						      \
	  s = tre_bt_mem_alloc(mem, sizeof(*s));			      \
	  if (!s)							      \
	    {								      \
	      tre_bt_mem_destroy(mem);				 \
	      if (tags) xfree(_hawk,tags);				 \
	      if (pmatch)  xfree(_hawk,pmatch);			 \
	      if (states_seen) xfree(_hawk,states_seen);	 \
	      return REG_ESPACE;					      \
	    }								      \
	  s->prev = stack;						      \
	  s->next = NULL;						      \
	  s->item.tags = tre_bt_mem_alloc(mem,		\
		  sizeof(*tags) * tnfa->num_tags);    \
	  if (!s->item.tags)						\
	    {								     \
	      tre_bt_mem_destroy(mem);				\
	      if (tags) xfree(_hawk,tags);				\
	      if (pmatch) xfree(_hawk,pmatch);			\
	      if (states_seen) xfree(_hawk,states_seen);	\
	      return REG_ESPACE;					      \
	    }								      \
	  stack->next = s;						      \
	  stack = s;							      \
	}								      \
      else								      \
	stack = stack->next;						      \
      stack->item.pos = (_pos);						      \
      stack->item.str_byte = (_str_byte);				      \
      BT_STACK_WIDE_IN(_str_wide);					      \
      stack->item.state = (_state);					      \
      stack->item.state_id = (_state_id);				      \
      stack->item.next_c = (_next_c);					      \
      for (i = 0; i < tnfa->num_tags; i++)				      \
	stack->item.tags[i] = (_tags)[i];				      \
      BT_STACK_MBSTATE_IN;						      \
    }									      \
  while (/*CONSTCOND*/0)

#define BT_STACK_POP()							      \
	do { \
		int i;								      \
		assert(stack->prev);						      \
		pos = stack->item.pos;						      \
		str_byte = stack->item.str_byte;					      \
		BT_STACK_WIDE_OUT;						      \
		state = stack->item.state;					      \
		next_c = stack->item.next_c;					      \
		for (i = 0; i < tnfa->num_tags; i++) tags[i] = stack->item.tags[i]; \
		BT_STACK_MBSTATE_OUT;						      \
		stack = stack->prev;						      \
	} while (/*CONSTCOND*/0)

#undef MIN
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

reg_errcode_t
tre_tnfa_run_backtrack(hawk_gem_t* gem, const tre_tnfa_t *tnfa, const void *string,
                       int len, tre_str_type_t type, int *match_tags,
                       int eflags, int *match_end_ofs)
{
	/* State variables required by GET_NEXT_WCHAR. */
	tre_char_t prev_c = 0, next_c = 0;
	const char *str_byte = string;
	int pos = 0;
	unsigned int pos_add_next = 1;
#ifdef TRE_WCHAR
	const hawk_uch_t *str_wide = string;
#ifdef TRE_MBSTATE
	hawk_mbstate_t mbstate;
#endif /* TRE_MBSTATE */
#endif /* TRE_WCHAR */
	int reg_notbol = eflags & REG_NOTBOL;
	int reg_noteol = eflags & REG_NOTEOL;
	int reg_newline = tnfa->cflags & REG_NEWLINE;

	/* These are used to remember the necessary values of the above
	   variables to return to the position where the current search
	   started from. */
	int next_c_start;
	const char *str_byte_start;
	int pos_start = -1;
#ifdef TRE_WCHAR
	const hawk_uch_t *str_wide_start;
#endif /* TRE_WCHAR */
#ifdef TRE_MBSTATE
	hawk_mbstate_t mbstate_start;
#endif /* TRE_MBSTATE */

	/* End offset of best match so far, or -1 if no match found yet. */
	int match_eo = -1;
	/* Tag arrays. */
	int *next_tags, *tags = NULL;
	/* Current TNFA state. */
	tre_tnfa_transition_t *state;
	int *states_seen = NULL;

	/* Memory allocator to for allocating the backtracking stack. */
	tre_mem_t mem = tre_bt_mem_new(gem);

	/* The backtracking stack. */
	tre_backtrack_t stack;

	tre_tnfa_transition_t *trans_i;
	regmatch_t *pmatch = NULL;
	int ret;

#ifdef TRE_MBSTATE
	HAWK_MEMSET(&mbstate, '\0', sizeof(mbstate));
#endif /* TRE_MBSTATE */

	if (!mem)
		return REG_ESPACE;
	stack = tre_bt_mem_alloc(mem, sizeof(*stack));
	if (!stack)
	{
		ret = REG_ESPACE;
		goto error_exit;
	}
	stack->prev = NULL;
	stack->next = NULL;

	DPRINT(("tnfa_execute_backtrack, input type %d\n", type));
	DPRINT(("len = %d\n", len));

	if (tnfa->num_tags)
	{
		tags = xmalloc(gem, sizeof(*tags) * tnfa->num_tags);
		if (!tags)
		{
			ret = REG_ESPACE;
			goto error_exit;
		}
	}
	if (tnfa->num_submatches)
	{
		pmatch = xmalloc(gem, sizeof(*pmatch) * tnfa->num_submatches);
		if (!pmatch)
		{
			ret = REG_ESPACE;
			goto error_exit;
		}
	}
	if (tnfa->num_states)
	{
		states_seen = xmalloc(gem, sizeof(*states_seen) * tnfa->num_states);
		if (!states_seen)
		{
			ret = REG_ESPACE;
			goto error_exit;
		}
	}

retry:
	{
		int i;
		for (i = 0; i < tnfa->num_tags; i++)
		{
			tags[i] = -1;
			if (match_tags)
				match_tags[i] = -1;
		}
		for (i = 0; i < tnfa->num_states; i++)
			states_seen[i] = 0;
	}

	state = NULL;
	pos = pos_start;
	GET_NEXT_WCHAR();
	pos_start = pos;
	next_c_start = next_c;
	str_byte_start = str_byte;
#ifdef TRE_WCHAR
	str_wide_start = str_wide;
#endif /* TRE_WCHAR */
#ifdef TRE_MBSTATE
	mbstate_start = mbstate;
#endif /* TRE_MBSTATE */

	/* Handle initial states. */
	next_tags = NULL;
	for (trans_i = tnfa->initial; trans_i->state; trans_i++)
	{
		DPRINT(("> init %p, prev_c %lc\n", trans_i->state, (tre_cint_t)prev_c));
		if (trans_i->assertions && CHECK_ASSERTIONS(trans_i->assertions))
		{
			DPRINT(("assert failed\n"));
			continue;
		}
		if (state == NULL)
		{
			/* Start from this state. */
			state = trans_i->state;
			next_tags = trans_i->tags;
		}
		else
		{
			/* Backtrack to this state. */
			DPRINT(("saving state %d for backtracking\n", trans_i->state_id));
			BT_STACK_PUSH(gem, pos, str_byte, str_wide, trans_i->state,
			              trans_i->state_id, next_c, tags, mbstate);
			{
				int *tmp = trans_i->tags;
				if (tmp)
					while (*tmp >= 0)
						stack->item.tags[*tmp++] = pos;
			}
		}
	}

	if (next_tags)
		for (; *next_tags >= 0; next_tags++)
			tags[*next_tags] = pos;


	DPRINT(("entering match loop, pos %d, str_byte %p\n", pos, str_byte));
	DPRINT(("pos:chr/code | state and tags\n"));
	DPRINT(("-------------+------------------------------------------------\n"));

	if (state == NULL)
		goto backtrack;

	while (/*CONSTCOND*/1)
	{
		tre_tnfa_transition_t *next_state;
		int empty_br_match;

		DPRINT(("start loop\n"));
		if (state == tnfa->final)
		{
			DPRINT(("  match found, %d %d\n", match_eo, pos));
			if (match_eo < pos
			        || (match_eo == pos
			            && match_tags
			            && tre_tag_order(tnfa->num_tags, tnfa->tag_directions,
			                             tags, match_tags)))
			{
				int i;
				/* This match wins the previous match. */
				DPRINT(("	 win previous\n"));
				match_eo = pos;
				if (match_tags)
					for (i = 0; i < tnfa->num_tags; i++)
						match_tags[i] = tags[i];
			}
			/* Our TNFAs never have transitions leaving from the final state,
			   so we jump right to backtracking. */
			goto backtrack;
		}

#ifdef TRE_DEBUG
		DPRINT(("%3d:%2lc/%05d | %p ", pos, (tre_cint_t)next_c, (int)next_c,
		        state));
		{
			int i;
			for (i = 0; i < tnfa->num_tags; i++)
				DPRINT(("%d%s", tags[i], i < tnfa->num_tags - 1 ? ", " : ""));
			DPRINT(("\n"));
		}
#endif /* TRE_DEBUG */

		/* Go to the next character in the input string. */
		empty_br_match = 0;
		trans_i = state;
		if (trans_i->state && (trans_i->assertions & ASSERT_BACKREF))
		{
			/* This is a back reference state.  All transitions leaving from
			   this state have the same back reference "assertion".  Instead
			   of reading the next character, we match the back reference. */
			int so, eo, bt = trans_i->u.backref;
			int bt_len;
			int result;

			DPRINT(("  should match back reference %d\n", bt));
			/* Get the substring we need to match against.  Remember to
			   turn off REG_NOSUB temporarily. */
			tre_fill_pmatch(bt + 1, pmatch, tnfa->cflags & /*LINTED*/!REG_NOSUB,
			                tnfa, tags, pos);
			so = pmatch[bt].rm_so;
			eo = pmatch[bt].rm_eo;
			bt_len = eo - so;

#ifdef TRE_DEBUG
			{
				int slen;
				if (len < 0)
					slen = bt_len;
				else
					slen = MIN(bt_len, len - pos);

				if (type == STR_BYTE)
				{
					DPRINT(("  substring (len %d) is [%d, %d[: '%.*s'\n",
					        bt_len, so, eo, bt_len, (char*)string + so));
					DPRINT(("  current string is '%.*s'\n", slen, str_byte - 1));
				}
#ifdef TRE_WCHAR
				else if (type == STR_WIDE)
				{
					DPRINT(("  substring (len %d) is [%d, %d[: '%.*" STRF "'\n",
					        bt_len, so, eo, bt_len, (hawk_uch_t*)string + so));
					DPRINT(("  current string is '%.*" STRF "'\n",
					        slen, str_wide - 1));
				}
#endif /* TRE_WCHAR */
			}
#endif

			if (len < 0)
			{
#ifdef TRE_WCHAR
				if (type == STR_WIDE)
					result = hawk_comp_ucstr_limited((const hawk_uch_t*)string + so, str_wide - 1, (size_t)bt_len, 0);
				else
#endif /* TRE_WCHAR */
					result = hawk_comp_bcstr_limited((const char*)string + so, str_byte - 1, (size_t)bt_len, 0);
			}
			else if (len - pos < bt_len)
				result = 1;
#ifdef TRE_WCHAR
			else if (type == STR_WIDE)
			{
				/*result = wmemcmp((const hawk_uch_t*)string + so, str_wide - 1, (size_t)bt_len);*/
				result = hawk_comp_uchars((const hawk_uch_t*)string + so, (size_t)bt_len, str_wide - 1, (size_t)bt_len, 0);
			}
#endif /* TRE_WCHAR */
			else
			{
				/*result = memcmp((const char*)string + so, str_byte - 1, (size_t)bt_len); */
				result = hawk_comp_bchars((const char*)string + so, (size_t)bt_len, str_byte - 1, (size_t)bt_len, 0);
			}


			if (result == 0)
			{
				/* Back reference matched.  Check for infinite loop. */
				if (bt_len == 0)
					empty_br_match = 1;
				if (empty_br_match && states_seen[trans_i->state_id])
				{
					DPRINT(("  avoid loop\n"));
					goto backtrack;
				}

				states_seen[trans_i->state_id] = empty_br_match;

				/* Advance in input string and resync `prev_c', `next_c'
						 and pos. */
				DPRINT(("	 back reference matched\n"));
				str_byte += bt_len - 1;
#ifdef TRE_WCHAR
				str_wide += bt_len - 1;
#endif /* TRE_WCHAR */
				pos += bt_len - 1;
				GET_NEXT_WCHAR();
				DPRINT(("	 pos now %d\n", pos));
			}
			else
			{
				DPRINT(("	 back reference did not match\n"));
				goto backtrack;
			}
		}
		else
		{
			/* Check for end of string. */
			if (len < 0)
			{
				if (next_c == HAWK_T('\0'))
					goto backtrack;
			}
			else
			{
				if (pos >= len)
					goto backtrack;
			}

			/* Read the next character. */
			GET_NEXT_WCHAR();
		}

		next_state = NULL;
		for (trans_i = state; trans_i->state; trans_i++)
		{
			DPRINT(("  transition %d-%d (%c-%c) %d to %d\n",
			        trans_i->code_min, trans_i->code_max,
			        trans_i->code_min, trans_i->code_max,
			        trans_i->assertions, trans_i->state_id));

			if (trans_i->code_min <= (tre_cint_t)prev_c && trans_i->code_max >= (tre_cint_t)prev_c)
			{
				if (trans_i->assertions
				        && (CHECK_ASSERTIONS(trans_i->assertions)
				            || CHECK_CHAR_CLASSES(trans_i, tnfa, eflags)))
				{
					DPRINT(("  assertion failed\n"));
					continue;
				}

				if (next_state == NULL)
				{
					/* First matching transition. */
					DPRINT(("  Next state is %d\n", trans_i->state_id));
					next_state = trans_i->state;
					next_tags = trans_i->tags;
				}
				else
				{
					/* Second matching transition.  We may need to backtrack here
					   to take this transition instead of the first one, so we
					   push this transition in the backtracking stack so we can
					   jump back here if needed. */
					DPRINT(("  saving state %d for backtracking\n",
					        trans_i->state_id));
					BT_STACK_PUSH(gem, pos, str_byte, str_wide, trans_i->state,
					              trans_i->state_id, next_c, tags, mbstate);
					{
						int *tmp;
						for (tmp = trans_i->tags; tmp && *tmp >= 0; tmp++)
							stack->item.tags[*tmp] = pos;
					}
#if 0 /* XXX - it's important not to look at all transitions here to keep
					the stack small! */
					break;
#endif
				}
			}
		}

		if (next_state != NULL)
		{
			/* Matching transitions were found.  Take the first one. */
			state = next_state;

			/* Update the tag values. */
			if (next_tags)
				while (*next_tags >= 0)
					tags[*next_tags++] = pos;
		}
		else
		{
backtrack:
			/* A matching transition was not found.  Try to backtrack. */
			if (stack->prev)
			{
				DPRINT(("	 backtracking\n"));
				if (stack->item.state->assertions && ASSERT_BACKREF)
				{
					DPRINT(("  states_seen[%d] = 0\n", stack->item.state_id));
					states_seen[stack->item.state_id] = 0;
				}

				BT_STACK_POP();
			}
			else if (match_eo < 0)
			{
				/* Try starting from a later position in the input string. */
				/* Check for end of string. */
				if (len < 0)
				{
					if (next_c == HAWK_T('\0'))
					{
						DPRINT(("end of string.\n"));
						break;
					}
				}
				else
				{
					if (pos >= len)
					{
						DPRINT(("end of string.\n"));
						break;
					}
				}
				DPRINT(("restarting from next start position\n"));
				next_c = next_c_start;
#ifdef TRE_MBSTATE
				mbstate = mbstate_start;
#endif /* TRE_MBSTATE */
				str_byte = str_byte_start;
#ifdef TRE_WCHAR
				str_wide = str_wide_start;
#endif /* TRE_WCHAR */
				goto retry;
			}
			else
			{
				DPRINT(("finished\n"));
				break;
			}
		}
	}

	ret = match_eo >= 0 ? REG_OK : REG_NOMATCH;
	*match_end_ofs = match_eo;

error_exit:
	tre_bt_mem_destroy(mem);
	if (tags) xfree(gem, tags);
	if (pmatch) xfree(gem, pmatch);
	if (states_seen) xfree(gem, states_seen);

	return ret;
}
