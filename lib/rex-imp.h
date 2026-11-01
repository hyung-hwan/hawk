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

/* Included once for each subject character type. Kept outside the native TRE
 * helpers so installing no primitives adds no generic descriptors or scratch
 * arrays to the native matching stack frame. */
int match_xcs (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm, const void* code,
	const xcs_t* str, const xcs_t* substr, int flags,
	xcs_t* match, xcs_t submat[9])
{
	hawk_rex_input_t input;
	hawk_rex_match_t matches[10];
	hawk_oow_t i;
	int n;

	input.type = input_type;
	input.ptr = str->ptr;
	input.len = str->len;
	input.start = substr->ptr - str->ptr;
	input.end = input.start + substr->len;

	n = hawk_rex_match(gem, prm, code, &input, flags, matches, submat? 10: 1);
	if (n != 1) return n;

	if (match)
	{
		match->ptr = str->ptr + matches[0].start;
		match->len = matches[0].end - matches[0].start;
	}

	if (submat)
	{
		for (i = 1; i < HAWK_COUNTOF(matches); i++)
		{
			submat[i - 1].ptr = (matches[i].start == HAWK_REX_NOMATCH)? HAWK_NULL: str->ptr + matches[i].start;
			submat[i - 1].len = (matches[i].start == HAWK_REX_NOMATCH)? 0: matches[i].end - matches[i].start;
		}
	}

	return 1;
}
