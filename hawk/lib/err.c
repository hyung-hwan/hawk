/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#include "hawk-prv.h"

static hawk_loc_t _nullloc = { 0, 0, HAWK_NULL };

const hawk_ooch_t* hawk_dflerrstr (hawk_t* awk, hawk_errnum_t errnum)
{
	static const hawk_ooch_t* errstr[] =
 	{
		HAWK_T("no error"),
		HAWK_T("other error"),
		HAWK_T("not implemented"),
		HAWK_T("subsystem error"),
		HAWK_T("internal error that should never have happened"),

		HAWK_T("insufficient memory"),
		HAWK_T("invalid parameter or data"),
		HAWK_T("access denied"),
		HAWK_T("operation not allowed"),
		HAWK_T("not supported"),
		HAWK_T("'${0}' not found"),
		HAWK_T("'${0}' already exists"),
		HAWK_T("I/O error"),
		HAWK_T("buffer full"),
		HAWK_T("encoding conversion error"),

		HAWK_T("cannot open '${0}'"),
		HAWK_T("cannot read '${0}'"),
		HAWK_T("cannot write '${0}'"),
		HAWK_T("cannot close '${0}'"),
		
		HAWK_T("block nested too deeply"),
		HAWK_T("expression nested too deeply"),

		HAWK_T("invalid character '${0}'"),
		HAWK_T("invalid digit '${0}'"),

		HAWK_T("unexpected end of input"),
		HAWK_T("comment not closed properly"),
		HAWK_T("string or regular expression not closed"),
		HAWK_T("invalid mbs character '${0}'"),
		HAWK_T("left brace expected in place of '${0}'"),
		HAWK_T("left parenthesis expected in place of '${0}'"),
		HAWK_T("right parenthesis expected in place of '${0}'"),
		HAWK_T("right bracket expected in place of '${0}'"),
		HAWK_T("comma expected in place of '${0}'"),
		HAWK_T("semicolon expected in place of '${0}'"),
		HAWK_T("colon expected in place of '${0}'"),
		HAWK_T("integer literal expected in place of '${0}'"),
		HAWK_T("statement not ending with a semicolon"),
		HAWK_T("keyword 'in' expected in place of '${0}'"),
		HAWK_T("right-hand side of 'in' not a variable"),
		HAWK_T("expression not recognized around '${0}'"),

		HAWK_T("keyword 'function' expected in place of '${0}'"),
		HAWK_T("keyword 'while' expected in place of '${0}'"),
		HAWK_T("invalid assignment statement"),
		HAWK_T("identifier expected in place of '${0}'"),
		HAWK_T("'${0}' not a valid function name"),
		HAWK_T("BEGIN not followed by left bracket on the same line"),
		HAWK_T("END not followed by left bracket on the same line"),
		HAWK_T("keyword '${0}' redefined"),
		HAWK_T("intrinsic function '${0}' redefined"),
		HAWK_T("function '${0}' redefined"),
		HAWK_T("global variable '${0}' redefined"),
		HAWK_T("parameter '${0}' redefined"),
		HAWK_T("variable '${0}' redefined"),
		HAWK_T("duplicate parameter name '${0}'"),
		HAWK_T("duplicate global variable '${0}'"),
		HAWK_T("duplicate local variable '${0}'"),
		HAWK_T("'${0}' not a valid parameter name"),
		HAWK_T("'${0}' not a valid variable name"),
		HAWK_T("variable name missing"),
		HAWK_T("undefined identifier '${0}'"),
		HAWK_T("l-value required"),
		HAWK_T("too many global variables"),
		HAWK_T("too many local variables"),
		HAWK_T("too many parameters"),
		HAWK_T("too many segments"),
		HAWK_T("segment '${0}' too long"),
		HAWK_T("bad argument"),
		HAWK_T("no argument provided"),
		HAWK_T("'break' outside a loop"),
		HAWK_T("'continue' outside a loop"),
		HAWK_T("'next' illegal in the BEGIN block"),
		HAWK_T("'next' illegal in the END block"),
		HAWK_T("'nextfile' illegal in the BEGIN block"),
		HAWK_T("'nextfile' illegal in the END block"),
		HAWK_T("both prefix and postfix increment/decrement operator present"),
		HAWK_T("illegal operand for increment/decrement operator"),
		HAWK_T("'@include' not followed by a string"),
		HAWK_T("include level too deep"),
		HAWK_T("'${0}' not recognized"),
		HAWK_T("@ not followed by a valid word"),

		HAWK_T("stack error"),
		HAWK_T("divide by zero"),
		HAWK_T("invalid operand"),
		HAWK_T("wrong position index"),
		HAWK_T("too few arguments"),
		HAWK_T("too many arguments"),
		HAWK_T("function '${0}' not found"),
		HAWK_T("non-function value in '${0}'"),
		HAWK_T("'${0}' not deletable"),
		HAWK_T("value not a map"),
		HAWK_T("right-hand side of the 'in' operator not a map"),
		HAWK_T("right-hand side of the 'in' operator not a map nor nil"),
		HAWK_T("value not referenceable"),
		HAWK_T("cannot return a map"),                      /* EMAPRET */
		HAWK_T("cannot assign a map to a positional"),      /* EMAPTOPOS */
		HAWK_T("cannot assign a map to an indexed variable"),/* EMAPTOIDX */
		HAWK_T("cannot assign a map to a variable '${0}'"), /* EMAPTONVAR */
		HAWK_T("cannot change a map to a scalar"),          /* EMAPTOSCALAR */
		HAWK_T("cannot change a scalar to a map"),          /* ESCALARTOMAP */
		HAWK_T("cannot change a map '${0}' to another map"),/* ENMAPTOMAP */
		HAWK_T("cannot change a map '${0}' to a scalar"),   /* ENMAPTOSCALAR */
		HAWK_T("cannot change a scalar '${0}' to a map"),   /* ENSCALARTOMAP */
		HAWK_T("invalid value to convert to a string"),
		HAWK_T("invalid value to convert to a number"),
		HAWK_T("invalid value to a character"),
		HAWK_T("invalid value for hashing"),
		HAWK_T("'next' called from BEGIN block"),
		HAWK_T("'next' called from END block"),
		HAWK_T("'nextfile' called from BEGIN block"),
		HAWK_T("'nextfile' called from END block"),
		HAWK_T("intrinsic function handler for '${0}' failed"),
		HAWK_T("wrong implementation of user-defined I/O handler"),
		HAWK_T("I/O handler returned an error"),
		HAWK_T("no such I/O name found"),
		HAWK_T("I/O name empty"),
		HAWK_T("I/O name '${0}' containing '\\0'"),
		HAWK_T("not sufficient arguments to formatting sequence"),
		HAWK_T("recursion detected in format conversion"),
		HAWK_T("invalid character in CONVFMT"),
		HAWK_T("invalid character in OFMT"),

		HAWK_T("failed to build regular expression"),
		HAWK_T("failed to match regular expression"),
		HAWK_T("recursion too deep in regular expression"),
		HAWK_T("right parenthesis expected in regular expression"),
		HAWK_T("right bracket expected in regular expression"),
		HAWK_T("right brace expected in regular expression"),
		HAWK_T("colon expected in regular expression"),
		HAWK_T("invalid character range in regular expression"),
		HAWK_T("invalid character class in regular expression"),
		HAWK_T("invalid occurrence bound in regular expression"),
		HAWK_T("special character at wrong position"),
		HAWK_T("premature end of regular expression")
	};

	return (errnum >= 0 && errnum < HAWK_COUNTOF(errstr))? errstr[errnum]: HAWK_T("unknown error");
}

hawk_errstr_t hawk_geterrstr (hawk_t* hawk)
{
	return hawk->errstr;
}

void hawk_seterrstr (hawk_t* hawk, hawk_errstr_t errstr)
{
	hawk->errstr = errstr;
}

/* ------------------------------------------------------------------------- */

const hawk_loc_t* hawk_geterrloc (hawk_t* hawk)
{
	return &hawk->_gem.errloc;
}

const hawk_bch_t* hawk_geterrbmsg (hawk_t* hawk)
{
#if defined(MOO_OOCH_IS_BCH)
	return (hawk->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk)(hawk, hawk->_gem.errnum): hawk->_gem.errmsg;
#else
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	msg = (hawk->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk)(hawk, hawk->_gem.errnum): hawk->_gem.errmsg;

	mbslen = HAWK_COUNTOF(hawk->berrmsg);
	hawk_conv_ucstr_to_bcstr_with_cmgr (msg, &wcslen, hawk->berrmsg, &mbslen, hawk_getcmgr(hawk));

	return hawk->berrmsg;
#endif
}

const hawk_uch_t* hawk_geterrumsg (hawk_t* hawk)
{
#if defined(MOO_OOCH_IS_BCH)
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	msg = (hawk->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk)(hawk, hawk->_gem.errnum): hawk->_gem.errmsg;

	wcslen = HAWK_COUNTOF(hawk->uerrmsg);
	hawk_conv_bcstr_to_ucstr_with_cmgr (msg, &mbslen, hawk->uerrmsg, &wcslen, hawk_getcmgr(hawk), 1);

	return hawk->uerrmsg;
#else
	return (hawk->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk)(hawk, hawk->_gem.errnum): hawk->_gem.errmsg;
#endif
}

void hawk_geterrinf (hawk_t* hawk, hawk_errinf_t* errinf)
{
	errinf->num = hawk->_gem.errnum;
	errinf->loc = hawk->_gem.errloc;
	hawk_copy_oocstr (errinf->msg, HAWK_COUNTOF(errinf->msg), (hawk->_gem.errmsg[0] == '\0'? hawk_geterrstr(hawk)(hawk, hawk->_gem.errnum): hawk->_gem.errmsg));
}

void hawk_geterror (hawk_t* hawk, hawk_errnum_t* errnum, const hawk_ooch_t** errmsg, hawk_loc_t* errloc)
{
	if (errnum) *errnum = hawk->_gem.errnum;
	if (errmsg) *errmsg = (hawk->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk)(hawk, hawk->_gem.errnum): hawk->_gem.errmsg;
	if (errloc) *errloc = hawk->_gem.errloc;
}

const hawk_ooch_t* hawk_backuperrmsg (hawk_t* hawk)
{
	hawk_copy_oocstr (hawk->errmsg_backup, HAWK_COUNTOF(hawk->errmsg_backup), hawk_geterrmsg(hawk));
	return hawk->errmsg_backup;
}

void hawk_seterrnum (hawk_t* hawk, hawk_errnum_t errnum, const hawk_oocs_t* errarg)
{
	hawk_seterror (hawk, errnum, errarg, HAWK_NULL);
}

void hawk_seterrinf (hawk_t* hawk, const hawk_errinf_t* errinf)
{
	hawk->_gem.errnum = errinf->num;
	hawk_copy_oocstr(hawk->_gem.errmsg, HAWK_COUNTOF(hawk->_gem.errmsg), errinf->msg);
	hawk->_gem.errloc = errinf->loc;
}

static int err_bchars (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	hawk_t* hawk = (hawk_t*)fmtout->ctx;
	hawk_oow_t max;

	max = HAWK_COUNTOF(hawk->_gem.errmsg) - hawk->errmsg_len - 1;

#if defined(HAWK_OOCH_IS_UCH)
	if (max <= 0) return 1;
	hawk_conv_bchars_to_uchars_with_cmgr (ptr, &len, &hawk->_gem.errmsg[hawk->errmsg_len], &max, hawk_getcmgr(hawk), 1);
	hawk->errmsg_len += max;
#else
	if (len > max) len = max;
	if (len <= 0) return 1;
	HAWK_MEMCPY (&hawk->_gem.errmsg[hawk->errmsg_len], ptr, len * HAWK_SIZEOF(*ptr));
	hawk->errmsg_len += len;
#endif

	hawk->_gem.errmsg[hawk->errmsg_len] = '\0';

	return 1; /* success */
}

static int err_uchars (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	hawk_t* hawk = (hawk_t*)fmtout->ctx;
	hawk_oow_t max;

	max = HAWK_COUNTOF(hawk->_gem.errmsg) - hawk->errmsg_len - 1;

#if defined(HAWK_OOCH_IS_UCH)
	if (len > max) len = max;
	if (len <= 0) return 1;
	HAWK_MEMCPY (&hawk->_gem.errmsg[hawk->errmsg_len], ptr, len * HAWK_SIZEOF(*ptr));
	hawk->errmsg_len += len;
#else
	if (max <= 0) return 1;
	hawk_conv_uchars_to_bchars_with_cmgr (ptr, &len, &hawk->_gem.errmsg[hawk->errmsg_len], &max, hawk_getcmgr(hawk));
	hawk->errmsg_len += max;
#endif
	hawk->_gem.errmsg[hawk->errmsg_len] = '\0';
	return 1; /* success */
}


void hawk_seterrbfmt (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, ...)
{
	va_list ap;
	hawk_fmtout_t fo;

	/*if (hawk->shuterr) return;*/
	hawk->errmsg_len = 0;
	hawk->_gem.errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = hawk_getmmgr(hawk);
	fo.putbchars = err_bchars;
	fo.putuchars = err_uchars;
	fo.ctx = hawk;

	va_start (ap, errfmt);
	hawk_bfmt_outv (&fo, errfmt, ap);
	va_end (ap);

	hawk->_gem.errnum = errnum;
	hawk->_gem.errloc = (errloc? *errloc: _nullloc);
}

void hawk_seterrufmt (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, ...)
{
	va_list ap;
	hawk_fmtout_t fo;

	/*if (hawk->shuterr) return;*/
	hawk->errmsg_len = 0;
	hawk->_gem.errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = hawk_getmmgr(hawk);
	fo.putbchars = err_bchars;
	fo.putuchars = err_uchars;
	fo.ctx = hawk;

	va_start (ap, errfmt);
	hawk_ufmt_outv (&fo, errfmt, ap);
	va_end (ap);

	hawk->_gem.errnum = errnum;
	hawk->_gem.errloc = (errloc? *errloc: _nullloc);
}

void hawk_seterror (hawk_t* hawk, hawk_errnum_t errnum, const hawk_oocs_t* errarg, const hawk_loc_t* errloc)
{
/* TODO: remove awk_rtx_seterror() and substitute hawk_rtx_seterrfmt()/seterrbfmt()/seterrufmt() */
	const hawk_ooch_t* errfmt;

	hawk->_gem.errnum = errnum;

	errfmt = hawk_geterrstr(hawk)(hawk, errnum);
	HAWK_ASSERT (awk, errfmt != HAWK_NULL);
/* TODO: this change is buggy... copying won't process arguments...
	qse_strxfncpy (hawk->_gem.errmsg, HAWK_COUNTOF(hawk->_gem.errmsg), errfmt, errarg);
*/
	hawk_copy_oocstr(hawk->_gem.errmsg, HAWK_COUNTOF(hawk->_gem.errmsg), errfmt);
/* TODO: remove awk_rtx_seterror() and substitute hawk_rtx_seterrfmt()/seterrbfmt()/seterrufmt() */
	hawk->_gem.errloc =  (errloc? *errloc: _nullloc);
}

/* ------------------------------------------------------------------------- */

const hawk_loc_t* hawk_rtx_geterrloc (hawk_rtx_t* rtx)
{
	return &rtx->_gem.errloc;
}

const hawk_bch_t* hawk_rtx_geterrbmsg (hawk_rtx_t* rtx)
{
#if defined(MOO_OOCH_IS_BCH)
	return (rtx->_gem.errmsg[0] == '\0')? hawk_geterrstr(awk)(hawk_rtx_gethawk(rtx), rtx->_gem.errnum): rtx->_gem.errmsg;
#else
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	msg = (rtx->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk_rtx_gethawk(rtx))(hawk_rtx_gethawk(rtx), rtx->_gem.errnum): rtx->_gem.errmsg;

	mbslen = HAWK_COUNTOF(rtx->berrmsg);
	hawk_conv_ucstr_to_bcstr_with_cmgr (msg, &wcslen, rtx->berrmsg, &mbslen, hawk_rtx_getcmgr(rtx));

	return rtx->berrmsg;
#endif
}

const hawk_uch_t* hawk_rtx_geterrumsg (hawk_rtx_t* rtx)
{
#if defined(MOO_OOCH_IS_BCH)
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	msg = (rtx->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk_rtx_gethawk(rtx))(hawk_rtx_gethawk(rtx), rtx->_gem.errnum): rtx->_gem.errmsg;

	wcslen = HAWK_COUNTOF(rtx->uerrmsg);
	hawk_conv_bcstr_to_ucstr_with_cmgr (msg, &mbslen, rtx->uerrmsg, &wcslen, hawk_rtx_getcmgr(rtx), 1);

	return rtx->uerrmsg;
#else
	return (rtx->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk_rtx_gethawk(rtx))(hawk_rtx_gethawk(rtx), rtx->_gem.errnum): rtx->_gem.errmsg;
#endif
}

void hawk_rtx_geterrinf (hawk_rtx_t* rtx, hawk_errinf_t* errinf)
{
	errinf->num = rtx->_gem.errnum;
	errinf->loc = rtx->_gem.errloc;
	hawk_copy_oocstr (errinf->msg, HAWK_COUNTOF(errinf->msg), (rtx->_gem.errmsg[0] == '\0'? hawk_geterrstr(hawk_rtx_gethawk(rtx))(hawk_rtx_gethawk(rtx), rtx->_gem.errnum): rtx->_gem.errmsg));
}

void hawk_rtx_geterror (hawk_rtx_t* rtx, hawk_errnum_t* errnum, const hawk_ooch_t** errmsg, hawk_loc_t* errloc)
{
	if (errnum) *errnum = rtx->_gem.errnum;
	if (errmsg) *errmsg = (rtx->_gem.errmsg[0] == '\0')? hawk_geterrstr(hawk_rtx_gethawk(rtx))(hawk_rtx_gethawk(rtx), rtx->_gem.errnum): rtx->_gem.errmsg;
	if (errloc) *errloc = rtx->_gem.errloc;
}

const hawk_ooch_t* hawk_rtx_backuperrmsg (hawk_rtx_t* rtx)
{
	hawk_copy_oocstr (rtx->errmsg_backup, HAWK_COUNTOF(rtx->errmsg_backup), hawk_rtx_geterrmsg(rtx));
	return rtx->errmsg_backup;
}

void hawk_rtx_seterrnum (hawk_rtx_t* rtx, hawk_errnum_t errnum, const hawk_oocs_t* errarg)
{
	hawk_rtx_seterror (rtx, errnum, errarg, HAWK_NULL);
}

void hawk_rtx_seterrinf (hawk_rtx_t* rtx, const hawk_errinf_t* errinf)
{
	rtx->_gem.errnum = errinf->num;
	hawk_copy_oocstr(rtx->_gem.errmsg, HAWK_COUNTOF(rtx->_gem.errmsg), errinf->msg);
	rtx->_gem.errloc = errinf->loc;
}


static int rtx_err_bchars (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)fmtout->ctx;
	hawk_oow_t max;

	max = HAWK_COUNTOF(rtx->_gem.errmsg) - rtx->errmsg_len - 1;

#if defined(HAWK_OOCH_IS_UCH)
	if (max <= 0) return 1;
	hawk_conv_bchars_to_uchars_with_cmgr (ptr, &len, &rtx->_gem.errmsg[rtx->errmsg_len], &max, hawk_rtx_getcmgr(rtx), 1);
	rtx->errmsg_len += max;
#else
	if (len > max) len = max;
	if (len <= 0) return 1;
	HAWK_MEMCPY (&rtx->errinf.msg[rtx->errmsg_len], ptr, len * HAWK_SIZEOF(*ptr));
	rtx->errmsg_len += len;
#endif

	rtx->_gem.errmsg[rtx->errmsg_len] = '\0';

	return 1; /* success */
}

static int rtx_err_uchars (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)fmtout->ctx;
	hawk_oow_t max;

	max = HAWK_COUNTOF(rtx->_gem.errmsg) - rtx->errmsg_len - 1;

#if defined(HAWK_OOCH_IS_UCH)
	if (len > max) len = max;
	if (len <= 0) return 1;
	HAWK_MEMCPY (&rtx->_gem.errmsg[rtx->errmsg_len], ptr, len * HAWK_SIZEOF(*ptr));
	rtx->errmsg_len += len;
#else
	if (max <= 0) return 1;
	hawk_conv_uchars_to_bchars_with_cmgr (ptr, &len, &rtx->_gem.errmsg[rtx->errmsg_len], &max, hawk_getcmgr(hawk));
	rtx->errmsg_len += max;
#endif
	rtx->_gem.errmsg[rtx->errmsg_len] = '\0';
	return 1; /* success */
}

void hawk_rtx_seterrbfmt (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, ...)
{
	va_list ap;
	hawk_fmtout_t fo;

	/*if (hawk->shuterr) return;*/
	rtx->errmsg_len = 0;
	rtx->_gem.errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = hawk_rtx_getmmgr(rtx);
	fo.putbchars = rtx_err_bchars;
	fo.putuchars = rtx_err_uchars;
	fo.ctx = rtx;

	va_start (ap, errfmt);
	hawk_bfmt_outv (&fo, errfmt, ap);
	va_end (ap);

	rtx->_gem.errnum = errnum;
	rtx->_gem.errloc = (errloc? *errloc: _nullloc);
}

void hawk_rtx_seterrufmt (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, ...)
{
	va_list ap;
	hawk_fmtout_t fo;

	/*if (hawk->shuterr) return;*/
	rtx->errmsg_len = 0;
	rtx->_gem.errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = hawk_rtx_getmmgr(rtx);
	fo.putbchars = rtx_err_bchars;
	fo.putuchars = rtx_err_uchars;
	fo.ctx = rtx;

	va_start (ap, errfmt);
	hawk_ufmt_outv (&fo, errfmt, ap);
	va_end (ap);

	rtx->_gem.errnum = errnum;
	rtx->_gem.errloc =  (errloc? *errloc: _nullloc);
}


void hawk_rtx_seterror (hawk_rtx_t* rtx, hawk_errnum_t errnum, const hawk_oocs_t* errarg, const hawk_loc_t* errloc)
{
	/* TODO: remove awk_rtx_seterror() and substitute hawk_rtx_seterrfmt()/seterrbfmt()/seterrufmt() */
	const hawk_ooch_t* errfmt;

	rtx->_gem.errnum = errnum;

	errfmt = hawk_geterrstr(hawk_rtx_gethawk(rtx))(hawk_rtx_gethawk(rtx), errnum);
	HAWK_ASSERT (awk, errfmt != HAWK_NULL);
/* TODO: this change is buggy... copying won't process arguments...
	qse_strxfncpy (rtx->_gem.errmsg, HAWK_COUNTOF(rtx->_gem.errmsg), errfmt, errarg);
*/
	hawk_copy_oocstr(rtx->_gem.errmsg, HAWK_COUNTOF(rtx->_gem.errmsg), errfmt);
/* TODO: remove awk_rtx_seterror() and substitute hawk_rtx_seterrfmt()/seterrbfmt()/seterrufmt() */
	rtx->_gem.errloc = (errloc? *errloc: _nullloc);
}

void hawk_rtx_errortohawk (hawk_rtx_t* rtx, hawk_t* hawk)
{
	hawk->_gem.errnum = rtx->_gem.errnum;
	hawk->_gem.errloc = rtx->_gem.errloc;
	hawk_copy_oocstr (hawk->_gem.errmsg, HAWK_COUNTOF(hawk->_gem.errmsg), rtx->_gem.errmsg);
}

/* ------------------------------------------------------------------------- */

void hawk_gem_seterrnum (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum)
{
	gem->errnum = errnum;
	gem->errmsg[0] = '\0';
	gem->errloc = (errloc? *errloc: _nullloc);
}
