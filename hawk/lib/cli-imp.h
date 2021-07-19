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


/* this file is supposed to be included by opt.c multiple times */

/* 
 * hawk_getopt is based on BSD getopt.
 * --------------------------------------------------------------------------
 *
 * Copyright (c) 1987-2002 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * A. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * B. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * C. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * --------------------------------------------------------------------------
 */



xci_t xgetcli (int argc, xch_t* const* argv, xcli_t* opt)
{
	xch_t* oli; /* option letter list index */
	int dbldash = 0;

	opt->arg = HAWK_NULL;
	opt->lngopt = HAWK_NULL;

	if (opt->cur == HAWK_NULL) 
	{
		opt->cur = XEMSG;
		opt->ind = 1;
	}

	if (*opt->cur == '\0') 
	{
		/* update scanning pointer */
		if (opt->ind >= argc || *(opt->cur = argv[opt->ind]) != '-') 
		{
			/* All arguments have been processed or the current 
			 * argument doesn't start with a dash */
			opt->cur = XEMSG;
			return XCI_EOF;
		}

		opt->cur++;

	#if 0
		if (*opt->cur == '\0')
		{
			/* - */
			opt->ind++;
			opt->cur = XEMSG;
			return XCI_EOF;
		}
	#endif

		if (*opt->cur == '-')
		{
			if (*++opt->cur == '\0')
			{
				/* -- */
				opt->ind++;
				opt->cur = XEMSG;
				return XCI_EOF;
			}
			else
			{
				dbldash = 1;
			}
		}
	}

	if (dbldash && opt->lng != HAWK_NULL)
	{
		const xcli_lng_t* o;
		xch_t* end = opt->cur;

		while (*end != '\0' && *end != '=') end++;

		for (o = opt->lng; o->str; o++) 
		{
			const xch_t* str = o->str;

			if (*str == ':') str++;

			if (xcompcharscstr(opt->cur, end - opt->cur, str, 0) != 0) continue;

			/* match */
			opt->cur = XEMSG;
			opt->lngopt = o->str;

			/* for a long matching option, remove the leading colon */
			if (opt->lngopt[0] == ':') opt->lngopt++;

			if (*end == '=') opt->arg = end + 1;

			if (*o->str != ':')
			{
				/* should not have an option argument */
				if (opt->arg != HAWK_NULL) return BADARG;
			}
			else if (opt->arg == HAWK_NULL)
			{
				/* check if it has a remaining argument 
				 * available */
				if (argc <= ++opt->ind) return BADARG; 
				/* If so, the next available argument is 
				 * taken to be an option argument */
				opt->arg = argv[opt->ind];
			}

			opt->ind++;
			return o->val;
		}

		/*if (*end == HAWK_T('=')) *end = HAWK_T('\0');*/
		opt->lngopt = opt->cur; 
		return BADCH;
	}

	if ((opt->opt = *opt->cur++) == ':' ||
	    (oli = xfindcharincstr(opt->str, opt->opt)) == HAWK_NULL) 
	{
		/*
		 * if the user didn't specify '-' as an option,
		 * assume it means EOF.
		 */
		if (opt->opt == (int)'-') return XCI_EOF;
		if (*opt->cur == '\0') ++opt->ind;
		return BADCH;
	}

	if (*++oli != ':') 
	{
		/* don't need argument */
		if (*opt->cur == '\0') opt->ind++;
	}
	else 
	{
		/* need an argument */

		if (*opt->cur != '\0') 
		{
			/* no white space */
			opt->arg = opt->cur;
		}
		else if (argc <= ++opt->ind) 
		{
			/* no arg */
			opt->cur = XEMSG;
			/*if (*opt->str == ':')*/ return BADARG;
			/*return BADCH;*/
		}
		else
		{
			/* white space */
			opt->arg = argv[opt->ind];
		}

		opt->cur = XEMSG;
		opt->ind++;
	}

	return opt->opt;  /* dump back option letter */
}
