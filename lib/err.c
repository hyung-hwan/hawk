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

#include "hawk-prv.h"

static const hawk_loc_t _nullloc = { 0, 0, HAWK_NULL };

const hawk_ooch_t* hawk_dfl_errstr (hawk_errnum_t errnum)
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
		HAWK_T("system busy"),
		HAWK_T("operation not allowed"),
		HAWK_T("not supported"),
		HAWK_T("not found"),
		HAWK_T("already exists"),
		HAWK_T("I/O error"),
		HAWK_T("buffer full"),
		HAWK_T("encoding conversion error"),
		HAWK_T("is a directory"),
		HAWK_T("not a directory"),
		HAWK_T("resource temporarily unavailable"),
		HAWK_T("interrupted"),
		HAWK_T("broken pipe"),
		HAWK_T("in progress"),
		HAWK_T("no handle"),
		HAWK_T("no child process"),
		HAWK_T("timed out"),
		HAWK_T("in bad state"),

		HAWK_T("cannot open"),
		HAWK_T("cannot read"),
		HAWK_T("cannot write"),
		HAWK_T("cannot close"),

		HAWK_T("block nested too deeply"),
		HAWK_T("expression nested too deeply"),

		HAWK_T("invalid character"),
		HAWK_T("invalid digit"),

		HAWK_T("unexpected end of input"),
		HAWK_T("comment not closed properly"),
		HAWK_T("string or regular expression not closed"),
		HAWK_T("invalid mbs character"),
		HAWK_T("left brace expected"),
		HAWK_T("left parenthesis expected"),
		HAWK_T("right parenthesis expected"),
		HAWK_T("right brace expected"),
		HAWK_T("right bracket expected"),
		HAWK_T("comma expected"),
		HAWK_T("semicolon expected"),
		HAWK_T("colon expected"),
		HAWK_T("integer literal expected"),
		HAWK_T("statement not ending with a semicolon"),
		HAWK_T("keyword 'in' expected"),
		HAWK_T("right-hand side of 'in' not a variable"),
		HAWK_T("expression not recognized"),

		HAWK_T("keyword 'function' expected"),
		HAWK_T("keyword 'while' expected"),
		HAWK_T("keyword 'case' expected"),
		HAWK_T("multiple 'default' labels"),
		HAWK_T("invalid assignment statement"),
		HAWK_T("identifier expected"),
		HAWK_T("not a valid function name"),
		HAWK_T("BEGIN not followed by left bracket on the same line"),
		HAWK_T("END not followed by left bracket on the same line"),
		HAWK_T("keyword redefined"),
		HAWK_T("intrinsic function redefined"),
		HAWK_T("function redefined"),
		HAWK_T("global variable redefined"),
		HAWK_T("parameter redefined"),
		HAWK_T("variable redefined"),
		HAWK_T("duplicate parameter name"),
		HAWK_T("duplicate global variable"),
		HAWK_T("duplicate local variable"),
		HAWK_T("not a valid parameter name"),
		HAWK_T("not a valid variable name"),
		HAWK_T("variable name missing"),
		HAWK_T("undefined identifier"),
		HAWK_T("l-value required"),
		HAWK_T("too many global variables"),
		HAWK_T("too many local variables"),
		HAWK_T("too many parameters"),
		HAWK_T("too many segments"),
		HAWK_T("segment too long"),
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
		HAWK_T("word after @ not recognized"),
		HAWK_T("@ not followed by a valid word"),

		HAWK_T("stack full"),
		HAWK_T("divide by zero"),
		HAWK_T("invalid operand"),
		HAWK_T("wrong position index"),
		HAWK_T("too few arguments"),
		HAWK_T("too many arguments"),
		HAWK_T("function not found"),
		HAWK_T("non-function value"),
		HAWK_T("not deletable"),
		HAWK_T("value not a map"),
		HAWK_T("value not an array"),
		HAWK_T("value not accessible with index"),
		HAWK_T("value not referenceable"),
		HAWK_T("wrong operand in right-hand side of 'in'"), /* EINROP */
		HAWK_T("cannot return a nonscalar value"),         /* ENONSCARET */
		HAWK_T("cannot assign a nonscalar value to a positional"), /* ENONSCATOPOS */
		HAWK_T("cannot assign a nonscalar value to an indexed variable"),/* ENONSCATOIDX */
		HAWK_T("cannot assign a nonscalar value to a variable"),         /* ENONSCATOVAR */
		HAWK_T("cannot change a nonscalar value to a scalar value"),     /* ENONSCATOSCALAR */
		HAWK_T("cannot change a nonscalar value to another nonscalar value"), /* ENONSCATONONSCA */
		HAWK_T("cannot change a scalar value to a nonscalar value"),           /* ESCALARTONONSCA */
		HAWK_T("invalid value to convert to a string"),
		HAWK_T("invalid value to convert to a number"),
		HAWK_T("invalid value to a character"),
		HAWK_T("invalid value for hashing"),
		HAWK_T("array index out of allowed range"),
		HAWK_T("single-bracketed multidimensional array indices not allowed"),
		HAWK_T("'next' called from BEGIN block"),
		HAWK_T("'next' called from END block"),
		HAWK_T("'nextfile' called from BEGIN block"),
		HAWK_T("'nextfile' called from END block"),
		HAWK_T("wrong implementation of user-defined I/O handler"),
		HAWK_T("I/O handler returned an error"),
		HAWK_T("no such I/O name found"),
		HAWK_T("I/O name empty"),
		HAWK_T("I/O name containing '\\0'"),
		HAWK_T("not sufficient arguments to formatting sequence"),
		HAWK_T("invalid character in CONVFMT"),
		HAWK_T("invalid character in OFMT"),

		HAWK_T("failed to match regular expression"),
		HAWK_T("invalid regular expression in regular expression"),
		HAWK_T("unknown collating element in regular expression"),
		HAWK_T("unknown character clas name in regular expression"),
		HAWK_T("trailing backslash in regular expression"),
		HAWK_T("invalid backreference in regular expression"),
		HAWK_T("imbalanced bracket in regular expression"),
		HAWK_T("imbalanced parenthesis in regular expression"),
		HAWK_T("imbalanced brace in regular expression"),
		HAWK_T("invalid content inside braces in regular expression"),
		HAWK_T("invalid use of range operator in regular expression"),
		HAWK_T("invalid use of repetition operator in regular expression"),

		/* sed error messages */
		HAWK_T("command '${0}' not recognized"),
		HAWK_T("command code missing"),
		HAWK_T("command '${0}' incomplete"),
		HAWK_T("regular expression '${0}' incomplete"),
		HAWK_T("failed to compile regular expression '${0}'"),
		HAWK_T("failed to match regular expression"),
		HAWK_T("address 1 prohibited for '${0}'"),
		HAWK_T("address 1 missing or invalid"),
		HAWK_T("address 2 prohibited for '${0}'"),
		HAWK_T("address 2 missing or invalid"),
		HAWK_T("newline expected"),
		HAWK_T("backslash expected"),
		HAWK_T("backslash used as delimiter"),
		HAWK_T("garbage after backslash"),
		HAWK_T("semicolon expected"),
		HAWK_T("empty label name"),
		HAWK_T("duplicate label name '${0}'"),
		HAWK_T("label '${0}' not found"),
		HAWK_T("empty file name"),
		HAWK_T("illegal file name"),
		HAWK_T("strings in translation set not the same length"),
		HAWK_T("group brackets not balanced"),
		HAWK_T("group nesting too deep"),
		HAWK_T("multiple occurrence specifiers"),
		HAWK_T("occurrence specifier zero"),
		HAWK_T("occurrence specifier too large"),
		HAWK_T("no previous regular expression"),
		HAWK_T("cut selector not valid"),
		HAWK_T("I/O error with file '${0}'"),

		/* cut error messages */
		HAWK_T("selector not valid"),
		HAWK_T("I/O error with file '${0}'")
	};

	static const hawk_ooch_t* unknown_error = HAWK_T("unknown error");

	return (errnum >= 0 && errnum < HAWK_COUNTOF(errstr))? errstr[errnum]: unknown_error;
}

hawk_errstr_t hawk_geterrstr (hawk_t* hawk)
{
	return hawk->_gem.errstr;
}

/* ------------------------------------------------------------------------- */
void hawk_seterrbfmt (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, ...)
{
	va_list ap;
	va_start (ap, errfmt);
	hawk_gem_seterrbvfmt (hawk_getgem(hawk), errloc, errnum, errfmt, ap);
	va_end (ap);
}

void hawk_seterrufmt (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, ...)
{
	va_list ap;
	va_start (ap, errfmt);
	hawk_gem_seterruvfmt (hawk_getgem(hawk), errloc, errnum, errfmt, ap);
	va_end (ap);
}

void hawk_seterrbvfmt (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, va_list ap)
{
	hawk_gem_seterrbvfmt (hawk_getgem(hawk), errloc, errnum, errfmt, ap);
}

void hawk_seterruvfmt (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, va_list ap)
{
	hawk_gem_seterruvfmt (hawk_getgem(hawk), errloc, errnum, errfmt, ap);
}

/* ------------------------------------------------------------------------- */

void hawk_rtx_seterrbfmt (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, ...)
{
	va_list ap;
	va_start (ap, errfmt);
	hawk_gem_seterrbvfmt (hawk_rtx_getgem(rtx), errloc, errnum, errfmt, ap);
	va_end (ap);
}

void hawk_rtx_seterrufmt (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, ...)
{
	va_list ap;
	va_start (ap, errfmt);
	hawk_gem_seterruvfmt (hawk_rtx_getgem(rtx), errloc, errnum, errfmt, ap);
	va_end (ap);
}

void hawk_rtx_seterrbvfmt (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, va_list ap)
{
	hawk_gem_seterrbvfmt (hawk_rtx_getgem(rtx), errloc, errnum, errfmt, ap);
}

void hawk_rtx_seterruvfmt (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, va_list ap)
{
	hawk_gem_seterruvfmt (hawk_rtx_getgem(rtx), errloc, errnum, errfmt, ap);
}

void hawk_rtx_errortohawk (hawk_rtx_t* rtx, hawk_t* hawk)
{
	/* copy error information in 'rtx' to the 'hawk' object */
	hawk->_gem.errnum = rtx->_gem.errnum;
	hawk->_gem.errloc = rtx->_gem.errloc;
	hawk_copy_oocstr (hawk->_gem.errmsg, HAWK_COUNTOF(hawk->_gem.errmsg), rtx->_gem.errmsg);
}

/* ------------------------------------------------------------------------- */

void hawk_gem_geterrbinf (hawk_gem_t* gem, hawk_errbinf_t* errinf)
{
#if defined(HAWK_OOCH_IS_BCH)
	errinf->num = gem->errnum;
	errinf->loc = gem->errloc;
	hawk_copy_oocstr (errinf->msg, HAWK_COUNTOF(errinf->msg), (gem->errmsg[0] == '\0'? gem->errstr(gem->errnum): gem->errmsg));
#else
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	/*errinf->num = gem->errnum;*/
	errinf->loc.line = gem->errloc.line;
	errinf->loc.colm = gem->errloc.colm;
	if (!gem->errloc.file) errinf->loc.file = HAWK_NULL;
	else
	{
		mbslen = HAWK_COUNTOF(gem->xerrlocfile);
		hawk_conv_ucstr_to_bcstr_with_cmgr(gem->errloc.file, &wcslen, gem->xerrlocfile, &mbslen, gem->cmgr);
		errinf->loc.file = gem->xerrlocfile; /* this can be truncated and is transient */
	}

	msg = (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;
	mbslen = HAWK_COUNTOF(errinf->msg);
	hawk_conv_ucstr_to_bcstr_with_cmgr (msg, &wcslen, errinf->msg, &mbslen, gem->cmgr);
#endif
}

void hawk_gem_geterruinf (hawk_gem_t* gem, hawk_erruinf_t* errinf)
{
#if defined(HAWK_OOCH_IS_BCH)
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	/*errinf->num = gem->errnum;*/
	errinf->loc.line = gem->errloc.line;
	errinf->loc.colm = gem->errloc.colm;
	if (!gem->errloc.file) errinf->loc.file = HAWK_NULL;
	else
	{
		wcslen = HAWK_COUNTOF(gem->xerrlocfile);
		hawk_conv_bcstr_to_ucstr_with_cmgr(gem->errloc.file, &mbslen, gem->xerrlocfile, &wcslen, gem->cmgr, 1);
		errinf->loc.file = gem->xerrlocfile; /* this can be truncated and is transient */
	}

	msg = (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;
	wcslen = HAWK_COUNTOF(errinf->msg);
	hawk_conv_bcstr_to_ucstr_with_cmgr (msg, &mbslen, errinf->msg, &wcslen, gem->cmgr, 1);
#else
	errinf->num = gem->errnum;
	errinf->loc = gem->errloc;
	hawk_copy_oocstr (errinf->msg, HAWK_COUNTOF(errinf->msg), (gem->errmsg[0] == '\0'? gem->errstr(gem->errnum): gem->errmsg));
#endif
}

void hawk_gem_geterror (hawk_gem_t* gem, hawk_errnum_t* errnum, const hawk_ooch_t** errmsg, hawk_loc_t* errloc)
{
	if (errnum) *errnum = gem->errnum;
	if (errmsg) *errmsg = (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;
	if (errloc) *errloc = gem->errloc;
}

const hawk_bch_t* hawk_gem_geterrbmsg (hawk_gem_t* gem)
{
#if defined(HAWK_OOCH_IS_BCH)
	return (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;
#else
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	msg = (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;

	mbslen = HAWK_COUNTOF(gem->xerrmsg);
	hawk_conv_ucstr_to_bcstr_with_cmgr (msg, &wcslen, gem->xerrmsg, &mbslen, gem->cmgr);

	return gem->xerrmsg;
#endif
}

const hawk_uch_t* hawk_gem_geterrumsg (hawk_gem_t* gem)
{
#if defined(HAWK_OOCH_IS_BCH)
	const hawk_ooch_t* msg;
	hawk_oow_t wcslen, mbslen;

	msg = (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;

	wcslen = HAWK_COUNTOF(gem->xerrmsg);
	hawk_conv_bcstr_to_ucstr_with_cmgr (msg, &mbslen, gem->xerrmsg, &wcslen, gem->cmgr, 1);

	return gem->xerrmsg;
#else
	return (gem->errmsg[0] == '\0')? gem->errstr(gem->errnum): gem->errmsg;
#endif
}

void hawk_gem_seterrnum (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum)
{
	gem->errnum = errnum;
	gem->errmsg[0] = '\0';
	gem->errloc = (errloc? *errloc: _nullloc);
}

void hawk_gem_seterrinf (hawk_gem_t* gem, const hawk_errinf_t* errinf)
{
	gem->errnum = errinf->num;
	hawk_copy_oocstr(gem->errmsg, HAWK_COUNTOF(gem->errmsg), errinf->msg);
	gem->errloc = errinf->loc;
}

static int gem_err_bchars (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	hawk_gem_t* gem = (hawk_gem_t*)fmtout->ctx;
	hawk_oow_t max;

	max = HAWK_COUNTOF(gem->errmsg) - gem->errmsg_len - 1;

#if defined(HAWK_OOCH_IS_UCH)
	if (max <= 0) return 1;
	hawk_conv_bchars_to_uchars_with_cmgr (ptr, &len, &gem->errmsg[gem->errmsg_len], &max, gem->cmgr, 1);
	gem->errmsg_len += max;
#else
	if (len > max) len = max;
	if (len <= 0) return 1;
	HAWK_MEMCPY (&gem->errmsg[gem->errmsg_len], ptr, len * HAWK_SIZEOF(*ptr));
	gem->errmsg_len += len;
#endif

	gem->errmsg[gem->errmsg_len] = '\0';

	return 1; /* success */
}

static int gem_err_uchars (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	hawk_gem_t* gem = (hawk_gem_t*)fmtout->ctx;
	hawk_oow_t max;

	max = HAWK_COUNTOF(gem->errmsg) - gem->errmsg_len - 1;

#if defined(HAWK_OOCH_IS_UCH)
	if (len > max) len = max;
	if (len <= 0) return 1;
	HAWK_MEMCPY (&gem->errmsg[gem->errmsg_len], ptr, len * HAWK_SIZEOF(*ptr));
	gem->errmsg_len += len;
#else
	if (max <= 0) return 1;
	hawk_conv_uchars_to_bchars_with_cmgr (ptr, &len, &gem->errmsg[gem->errmsg_len], &max, gem->cmgr);
	gem->errmsg_len += max;
#endif
	gem->errmsg[gem->errmsg_len] = '\0';
	return 1; /* success */
}

void hawk_gem_seterrbfmt (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, ...)
{
	va_list ap;
	hawk_fmtout_t fo;

	gem->errmsg_len = 0;
	gem->errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = gem->mmgr;
	fo.putbchars = gem_err_bchars;
	fo.putuchars = gem_err_uchars;
	fo.ctx = gem;

	va_start (ap, errfmt);
	hawk_bfmt_outv (&fo, errfmt, ap);
	va_end (ap);

	gem->errnum = errnum;
	gem->errloc = (errloc? *errloc: _nullloc);
}

void hawk_gem_seterrufmt (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, ...)
{
	va_list ap;
	hawk_fmtout_t fo;

	gem->errmsg_len = 0;
	gem->errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = gem->mmgr;
	fo.putbchars = gem_err_bchars;
	fo.putuchars = gem_err_uchars;
	fo.ctx = gem;

	va_start (ap, errfmt);
	hawk_ufmt_outv (&fo, errfmt, ap);
	va_end (ap);

	gem->errnum = errnum;
	gem->errloc = (errloc? *errloc: _nullloc);
}

void hawk_gem_seterrbvfmt (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, va_list ap)
{
	hawk_fmtout_t fo;

	gem->errmsg_len = 0;
	gem->errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = gem->mmgr;
	fo.putbchars = gem_err_bchars;
	fo.putuchars = gem_err_uchars;
	fo.ctx = gem;

	hawk_bfmt_outv (&fo, errfmt, ap);

	gem->errnum = errnum;
	gem->errloc = (errloc? *errloc: _nullloc);
}

void hawk_gem_seterruvfmt (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, va_list ap)
{
	hawk_fmtout_t fo;

	gem->errmsg_len = 0;
	gem->errmsg[0] = '\0';

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = gem->mmgr;
	fo.putbchars = gem_err_bchars;
	fo.putuchars = gem_err_uchars;
	fo.ctx = gem;

	hawk_ufmt_outv (&fo, errfmt, ap);

	gem->errnum = errnum;
	gem->errloc =  (errloc? *errloc: _nullloc);
}

void hawk_gem_seterror (hawk_gem_t* gem, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_oocs_t* errarg)
{
	const hawk_ooch_t* errfmt;

	gem->errnum = errnum;

	errfmt = gem->errstr(gem->errnum);
	HAWK_ASSERT (errfmt != HAWK_NULL);

	hawk_copy_fmt_oocses_to_oocstr (gem->errmsg, HAWK_COUNTOF(gem->errmsg), errfmt, errarg);

	if (errloc != HAWK_NULL) gem->errloc = *errloc;
	else HAWK_MEMSET (&gem->errloc, 0, HAWK_SIZEOF(gem->errloc));
}

const hawk_ooch_t* hawk_gem_backuperrmsg (hawk_gem_t* gem)
{
	hawk_copy_oocstr (gem->errmsg_backup, HAWK_COUNTOF(gem->errmsg_backup), hawk_gem_geterrmsg(gem));
	return gem->errmsg_backup;
}
