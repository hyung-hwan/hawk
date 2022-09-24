/*
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

#include "tre-prv.h"
#include "tre-compile.h"

hawk_tre_t* hawk_tre_open (hawk_gem_t* gem, hawk_oow_t xtnsize)
{
	hawk_tre_t* tre;

	tre = (hawk_tre_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_tre_t) + xtnsize);
	if (!tre) return HAWK_NULL;

	if (hawk_tre_init(tre, gem) <= -1)
	{
		hawk_gem_freemem (gem, tre);
		return HAWK_NULL;
	}

	HAWK_MEMSET (tre + 1, 0, xtnsize);
	return tre;
}

void hawk_tre_close (hawk_tre_t* tre)
{
	hawk_tre_fini (tre);
	hawk_gem_freemem (tre->gem, tre);
}

int hawk_tre_init (hawk_tre_t* tre, hawk_gem_t* gem)
{
	HAWK_MEMSET (tre, 0, HAWK_SIZEOF(*tre));
	tre->gem = gem;
	return 0;
}

void hawk_tre_fini (hawk_tre_t* tre)
{
	if (tre->TRE_REGEX_T_FIELD) 
	{
		tre_free (tre);
		tre->TRE_REGEX_T_FIELD = HAWK_NULL;
	}
}

int hawk_tre_compx (hawk_tre_t* tre, const hawk_ooch_t* regex, hawk_oow_t n, unsigned int* nsubmat, int cflags)
{
	int ret;

	if (tre->TRE_REGEX_T_FIELD) 
	{
		tre_free (tre);
		tre->TRE_REGEX_T_FIELD = HAWK_NULL;
	}

	ret = tre_compile(tre, regex, n, cflags);
	if (ret > 0) 
	{
		tre->TRE_REGEX_T_FIELD = HAWK_NULL; /* just to make sure */
		hawk_gem_seterrnum (tre->gem, HAWK_NULL, ret);
		return -1;
	}
	
	if (nsubmat) 
	{
		*nsubmat = ((struct tnfa*)tre->TRE_REGEX_T_FIELD)->num_submatches;
	}
	return 0;
}

int hawk_tre_comp (hawk_tre_t* tre, const hawk_ooch_t* regex, unsigned int* nsubmat, int cflags)
{
	return hawk_tre_compx(tre, regex, (regex? hawk_count_oocstr(regex):0), nsubmat, cflags);
}

/* Fills the POSIX.2 regmatch_t array according to the TNFA tag and match
   endpoint values. */
void tre_fill_pmatch (size_t nmatch, regmatch_t pmatch[], int cflags,
                      const tre_tnfa_t *tnfa, int *tags, int match_eo)
{
	tre_submatch_data_t *submatch_data;
	unsigned int i, j;
	int *parents;

	i = 0;
	if (match_eo >= 0 && !(cflags & REG_NOSUB))
	{
		/* Construct submatch offsets from the tags. */
		DPRINT(("end tag = t%d = %d\n", tnfa->end_tag, match_eo));
		submatch_data = tnfa->submatch_data;
		while (i < tnfa->num_submatches && i < nmatch)
		{
			if (submatch_data[i].so_tag == tnfa->end_tag)
				pmatch[i].rm_so = match_eo;
			else
				pmatch[i].rm_so = tags[submatch_data[i].so_tag];

			if (submatch_data[i].eo_tag == tnfa->end_tag)
				pmatch[i].rm_eo = match_eo;
			else
				pmatch[i].rm_eo = tags[submatch_data[i].eo_tag];

			/* If either of the endpoints were not used, this submatch
			   was not part of the match. */
			if (pmatch[i].rm_so == -1 || pmatch[i].rm_eo == -1)
				pmatch[i].rm_so = pmatch[i].rm_eo = -1;

			DPRINT(("pmatch[%d] = {t%d = %d, t%d = %d}\n", i,
			        submatch_data[i].so_tag, pmatch[i].rm_so,
			        submatch_data[i].eo_tag, pmatch[i].rm_eo));
			i++;
		}
		/* Reset all submatches that are not within all of their parent
			 submatches. */
		i = 0;
		while (i < tnfa->num_submatches && i < nmatch)
		{
			if (pmatch[i].rm_eo == -1)
				assert(pmatch[i].rm_so == -1);
			assert(pmatch[i].rm_so <= pmatch[i].rm_eo);

			parents = submatch_data[i].parents;
			if (parents != HAWK_NULL)
				for (j = 0; parents[j] >= 0; j++)
				{
					DPRINT(("pmatch[%d] parent %d\n", i, parents[j]));
					if (pmatch[i].rm_so < pmatch[parents[j]].rm_so
					        || pmatch[i].rm_eo > pmatch[parents[j]].rm_eo)
						pmatch[i].rm_so = pmatch[i].rm_eo = -1;
				}
			i++;
		}
	}

	while (i < nmatch)
	{
		pmatch[i].rm_so = -1;
		pmatch[i].rm_eo = -1;
		i++;
	}
}


/*
  Wrapper functions for POSIX compatible regexp matching.
*/

int tre_have_backrefs (const regex_t *preg)
{
	tre_tnfa_t *tnfa = (void *)preg->TRE_REGEX_T_FIELD;
	return tnfa->have_backrefs;
}

static int tre_match (
	const regex_t* preg, const void *string, hawk_oow_t len,
	tre_str_type_t type, hawk_oow_t nmatch, regmatch_t pmatch[],
	int eflags)
{
	tre_tnfa_t *tnfa = (void *)preg->TRE_REGEX_T_FIELD;
	reg_errcode_t status;
	int *tags = HAWK_NULL, eo;
	if (tnfa->num_tags > 0 && nmatch > 0)
	{
		tags = xmalloc(preg->gem, sizeof(*tags) * tnfa->num_tags);
		if (tags == HAWK_NULL) return REG_ESPACE;
	}

	/* Dispatch to the appropriate matcher. */
	if (tnfa->have_backrefs || (eflags & REG_BACKTRACKING_MATCHER))
	{
		/* The regex has back references, use the backtracking matcher. */
		status = tre_tnfa_run_backtrack (
			preg->gem, tnfa, string, (int)len, type,
			tags, eflags, &eo);
	}
	else
	{
		/* Exact matching, no back references, use the parallel matcher. */
		status = tre_tnfa_run_parallel (
			preg->gem, tnfa, string, (int)len, type,
			tags, eflags, &eo);
	}

	if (status == REG_OK)
		/* A match was found, so fill the submatch registers. */
		tre_fill_pmatch(nmatch, pmatch, tnfa->cflags, tnfa, tags, eo);
	if (tags) xfree (preg->gem, tags);
	return status;
}

int hawk_tre_execx (
	hawk_tre_t* tre, const hawk_ooch_t *str, hawk_oow_t len,
	regmatch_t* pmatch, hawk_oow_t nmatch, int eflags, hawk_gem_t* errgem)
{
	int ret;

	if (tre->TRE_REGEX_T_FIELD == HAWK_NULL)
	{
		/* regular expression is bad as none is compiled yet */
		hawk_gem_seterrnum ((errgem? errgem: tre->gem), HAWK_NULL, HAWK_EREXBADPAT);
		return -1;
	}
#if defined(HAWK_OOCH_IS_UCH)
	ret = tre_match(tre, str, len, STR_WIDE, nmatch, pmatch, eflags);
#else
	ret = tre_match(tre, str, len, STR_BYTE, nmatch, pmatch, eflags);
#endif
	if (ret > 0) 
	{
		hawk_gem_seterrnum ((errgem? errgem: tre->gem), HAWK_NULL, ret);
		return -1;
	}
	
	return 0;
}


int hawk_tre_exec (
	hawk_tre_t* tre, const hawk_ooch_t* str,
	regmatch_t* pmatch, hawk_oow_t nmatch, int eflags, hawk_gem_t* errgem)
{
	return hawk_tre_execx(tre, str, (hawk_oow_t)-1, pmatch, nmatch, eflags, errgem);
}

int hawk_tre_execuchars (
	hawk_tre_t* tre, const hawk_uch_t* str, hawk_oow_t len,
	regmatch_t* pmatch, hawk_oow_t nmatch, int eflags, hawk_gem_t* errgem)
{
	int ret;

	if (tre->TRE_REGEX_T_FIELD == HAWK_NULL)
	{
		/* regular expression is bad as none is compiled yet */
		hawk_gem_seterrnum ((errgem? errgem: tre->gem), HAWK_NULL, HAWK_EREXBADPAT);
		return -1;
	}
	ret = tre_match(tre, str, len, STR_WIDE, nmatch, pmatch, eflags);
	if (ret > 0) 
	{
		hawk_gem_seterrnum ((errgem? errgem: tre->gem), HAWK_NULL, ret);
		return -1;
	}
	
	return 0;
}

int hawk_tre_execbchars (
	hawk_tre_t* tre, const hawk_bch_t* str, hawk_oow_t len,
	regmatch_t* pmatch, hawk_oow_t nmatch, int eflags, hawk_gem_t* errgem)
{
	int ret;

	if (tre->TRE_REGEX_T_FIELD == HAWK_NULL)
	{
		/* regular expression is bad as none is compiled yet */
		hawk_gem_seterrnum ((errgem? errgem: tre->gem), HAWK_NULL, HAWK_EREXBADPAT);
		return -1;
	}
	ret = tre_match(tre, str, len, STR_BYTE, nmatch, pmatch, eflags);
	if (ret > 0) 
	{
		hawk_gem_seterrnum ((errgem? errgem: tre->gem), HAWK_NULL, ret);
		return -1;
	}
	
	return 0;
}

