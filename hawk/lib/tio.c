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

#include <hawk-tio.h>
#include "hawk-prv.h"

#define STATUS_OUTPUT_DYNBUF (1 << 0)
#define STATUS_INPUT_DYNBUF  (1 << 1)
#define STATUS_INPUT_ILLSEQ  (1 << 2)
#define STATUS_INPUT_EOF     (1 << 3)

static int detach_in (hawk_tio_t* tio, int fini);
static int detach_out (hawk_tio_t* tio, int fini);

hawk_tio_t* hawk_tio_open (hawk_gem_t* gem, hawk_oow_t xtnsize, int flags)
{
	hawk_tio_t* tio;

	tio = (hawk_tio_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_tio_t) + xtnsize);
	if (tio)
	{
		if (hawk_tio_init(tio, gem, flags) <= -1)
		{
			hawk_gem_freemem (gem, tio);
			return HAWK_NULL;
		}
		else HAWK_MEMSET (tio + 1, 0, xtnsize);
	}
	return tio;
}

int hawk_tio_close (hawk_tio_t* tio)
{
	int n = hawk_tio_fini(tio);
	hawk_gem_freemem (tio->gem, tio);
	return n;
}

int hawk_tio_init (hawk_tio_t* tio, hawk_gem_t* gem, int flags)
{
	HAWK_MEMSET (tio, 0, HAWK_SIZEOF(*tio));

	tio->gem = gem;
	tio->cmgr = gem->cmgr;

	tio->flags = flags;

	/*
	tio->input_func = HAWK_NULL;
	tio->input_arg = HAWK_NULL;
	tio->output_func = HAWK_NULL;
	tio->output_arg = HAWK_NULL;

	tio->status = 0;
	tio->inbuf_cur = 0;
	tio->inbuf_len = 0;
	tio->outbuf_len = 0;
	*/
	return 0;
}

int hawk_tio_fini (hawk_tio_t* tio)
{
	int ret = 0;

	hawk_tio_flush (tio); /* don't care about the result */
	if (detach_in (tio, 1) <= -1) ret = -1;
	if (detach_out (tio, 1) <= -1) ret = -1;

	return ret;
}

hawk_cmgr_t* hawk_tio_getcmgr (hawk_tio_t* tio)
{
	return tio->cmgr;
}

void hawk_tio_setcmgr (hawk_tio_t* tio, hawk_cmgr_t* cmgr)
{
	tio->cmgr = cmgr;
}

int hawk_tio_attachin (
	hawk_tio_t* tio, hawk_tio_io_impl_t input,
	hawk_bch_t* bufptr, hawk_oow_t bufcapa)
{
	hawk_bch_t* xbufptr;

	if (input == HAWK_NULL || bufcapa < HAWK_TIO_MININBUFCAPA) 
	{
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	if (hawk_tio_detachin(tio) <= -1) return -1;

	HAWK_ASSERT (tio->hawk, tio->in.fun == HAWK_NULL);

	xbufptr = bufptr;
	if (xbufptr == HAWK_NULL)
	{
		xbufptr = hawk_gem_allocmem(tio->gem, HAWK_SIZEOF(hawk_bch_t) * bufcapa);
		if (xbufptr == HAWK_NULL) return -1;
	}

	if (input(tio, HAWK_TIO_OPEN, HAWK_NULL, 0) <= -1) 
	{
		if (xbufptr != bufptr) hawk_gem_freemem (tio->gem, xbufptr);
		return -1;
	}

	/* if i defined tio->io[2] instead of tio->in and tio-out, 
	 * i would be able to shorten code amount. but fields to initialize
	 * are not symmetric between input and output.
	 * so it's just a bit clumsy that i repeat almost the same code
	 * in hawk_tio_attachout().
	 */

	tio->in.fun = input;
	tio->in.buf.ptr = xbufptr;
	tio->in.buf.capa = bufcapa;

	tio->status &= ~(STATUS_INPUT_ILLSEQ | STATUS_INPUT_EOF);
	tio->inbuf_cur = 0;
	tio->inbuf_len = 0;

	if (xbufptr != bufptr) tio->status |= STATUS_INPUT_DYNBUF;
	return 0;
}

static int detach_in (hawk_tio_t* tio, int fini)
{
	int ret = 0;

	if (tio->in.fun)
	{
		if (tio->in.fun(tio, HAWK_TIO_CLOSE, HAWK_NULL, 0) <= -1) 
		{
			/* returning with an error here allows you to retry detaching */
			if (!fini) return -1; 

			/* otherwise, you can't retry since the input handler information
			 * is reset below */
			ret = -1; 
		}

		if (tio->status & STATUS_INPUT_DYNBUF) 
		{
			hawk_gem_freemem(tio->gem, tio->in.buf.ptr);
			tio->status &= ~STATUS_INPUT_DYNBUF;
		}

		tio->in.fun = HAWK_NULL;
		tio->in.buf.ptr = HAWK_NULL;
		tio->in.buf.capa = 0;
	}
		
	return ret;
}

int hawk_tio_detachin (hawk_tio_t* tio)
{
	return detach_in (tio, 0);
}

int hawk_tio_attachout (
	hawk_tio_t* tio, hawk_tio_io_impl_t output, 
	hawk_bch_t* bufptr, hawk_oow_t bufcapa)
{
	hawk_bch_t* xbufptr;

	if (output == HAWK_NULL || bufcapa < HAWK_TIO_MINOUTBUFCAPA)  
	{
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	if (hawk_tio_detachout(tio) == -1) return -1;

	/*HAWK_ASSERT (tio->hawk, tio->out.fun == HAWK_NULL);*/

	xbufptr = bufptr;
	if (xbufptr == HAWK_NULL)
	{
		xbufptr = hawk_gem_allocmem(tio->gem, HAWK_SIZEOF(hawk_bch_t) * bufcapa);
		if (xbufptr == HAWK_NULL) return -1;
	}

	if (output(tio, HAWK_TIO_OPEN, HAWK_NULL, 0) <= -1) 
	{
		if (xbufptr != bufptr) hawk_gem_freemem (tio->gem, xbufptr);
		return -1;
	}

	tio->out.fun = output;
	tio->out.buf.ptr = xbufptr;
	tio->out.buf.capa = bufcapa;

	tio->outbuf_len = 0;

	if (xbufptr != bufptr) tio->status |= STATUS_OUTPUT_DYNBUF;
	return 0;
}

static int detach_out (hawk_tio_t* tio, int fini)
{
	int ret = 0;

	if (tio->out.fun)
	{
		hawk_tio_flush (tio); /* don't care about the result */

		if (tio->out.fun (tio, HAWK_TIO_CLOSE, HAWK_NULL, 0) <= -1) 
		{
			/* returning with an error here allows you to retry detaching */
			if (!fini) return -1;

			/* otherwise, you can't retry since the input handler information
			 * is reset below */
			ret = -1;
		}
	
		if (tio->status & STATUS_OUTPUT_DYNBUF) 
		{
			hawk_gem_freemem (tio->gem, tio->out.buf.ptr);
			tio->status &= ~STATUS_OUTPUT_DYNBUF;
		}

		tio->out.fun = HAWK_NULL;
		tio->out.buf.ptr = HAWK_NULL;
		tio->out.buf.capa = 0;
	}

	return ret;
}

int hawk_tio_detachout (hawk_tio_t* tio)
{
	return detach_out(tio, 0);
}

hawk_ooi_t hawk_tio_flush (hawk_tio_t* tio)
{
	hawk_oow_t left, count;
	hawk_ooi_t n;
	hawk_bch_t* cur;

	if (tio->out.fun == HAWK_NULL)
	{
		/* no output function */
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EINVAL);
		return (hawk_ooi_t)-1;
	}

	left = tio->outbuf_len;
	cur = tio->out.buf.ptr;
	while (left > 0) 
	{
		n = tio->out.fun(tio, HAWK_TIO_DATA, cur, left);
		if (n <= -1) 
		{
			if (cur != tio->out.buf.ptr)
			{
				HAWK_MEMCPY (tio->out.buf.ptr, cur, left);
				tio->outbuf_len = left;
			}
			return -1;
		}
		if (n == 0) 
		{
			if (cur != tio->out.buf.ptr)
				HAWK_MEMCPY (tio->out.buf.ptr, cur, left);
			break;
		}
	
		left -= n;
		cur += n;
	}

	count = tio->outbuf_len - left;
	tio->outbuf_len = left;

	return (hawk_ooi_t)count;
}

void hawk_tio_drain (hawk_tio_t* tio)
{
	tio->status &= ~(STATUS_INPUT_ILLSEQ | STATUS_INPUT_EOF);
	tio->inbuf_cur = 0;
	tio->inbuf_len = 0;
	tio->outbuf_len = 0;
}

/* ------------------------------------------------------------- */


hawk_ooi_t hawk_tio_readbchars (hawk_tio_t* tio, hawk_bch_t* buf, hawk_oow_t size)
{
	hawk_oow_t nread;
	hawk_ooi_t n;

	/*HAWK_ASSERT (tio->hawk, tio->in.fun != HAWK_NULL);*/
	if (tio->in.fun == HAWK_NULL) 
	{
		/* no input function */
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	/* note that this function doesn't check if
	 * tio->status is set with STATUS_INPUT_ILLSEQ
	 * since this function can simply return the next
	 * available byte. */

	if (size > HAWK_TYPE_MAX(hawk_ooi_t)) size = HAWK_TYPE_MAX(hawk_ooi_t);

	nread = 0;
	while (nread < size)
	{
		if (tio->inbuf_cur >= tio->inbuf_len) 
		{
			n = tio->in.fun(tio, HAWK_TIO_DATA, tio->in.buf.ptr, tio->in.buf.capa);
			if (n == 0) break;
			if (n <= -1) return -1;

			tio->inbuf_cur = 0;
			tio->inbuf_len = (hawk_oow_t)n;
		}

		do
		{
			buf[nread] = tio->in.buf.ptr[tio->inbuf_cur++];
			/* TODO: support a different line terminator */
			if (buf[nread++] == '\n') goto done;
		}
		while (tio->inbuf_cur < tio->inbuf_len && nread < size);
	}

done:
	return nread;
}

static HAWK_INLINE hawk_ooi_t tio_read_uchars (
	hawk_tio_t* tio, hawk_uch_t* buf, hawk_oow_t bufsize)
{
	hawk_oow_t mlen, wlen;
	hawk_ooi_t n;
	int x;

	if (tio->inbuf_cur >= tio->inbuf_len) 
	{
		tio->inbuf_cur = 0;
		tio->inbuf_len = 0;

	getc_conv:
		if (tio->status & STATUS_INPUT_EOF) n = 0;
		else
		{
			n = tio->in.fun(tio, HAWK_TIO_DATA, &tio->in.buf.ptr[tio->inbuf_len], tio->in.buf.capa - tio->inbuf_len);
		}
		if (n == 0) 
		{
			tio->status |= STATUS_INPUT_EOF;

			if (tio->inbuf_cur < tio->inbuf_len)
			{
				/* no more input from the underlying input handler.
				 * but some incomplete bytes in the buffer. */
				if (tio->flags & HAWK_TIO_IGNOREECERR) 
				{
					goto ignore_illseq;
				}
				else
				{
					/* treat them as illegal sequence */
					hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EECERR);
					return -1;
				}
			}

			return 0;
		}
		if (n <= -1) return -1;

		tio->inbuf_len += n;
	}

	mlen = tio->inbuf_len - tio->inbuf_cur;
	wlen = bufsize;

	x = hawk_conv_bchars_to_uchars_upto_stopper_with_cmgr(
		&tio->in.buf.ptr[tio->inbuf_cur],
		&mlen, buf, &wlen, '\n', tio->cmgr);
	tio->inbuf_cur += mlen;

	if (x == -3)
	{
		/* incomplete sequence */
		if (wlen <= 0)
		{
			/* not even a single character was handled. 
			 * shift bytes in the buffer to the head. */
			HAWK_ASSERT (tio->hawk, mlen <= 0);
			tio->inbuf_len = tio->inbuf_len - tio->inbuf_cur;
			HAWK_MEMCPY (&tio->in.buf.ptr[0], 
			            &tio->in.buf.ptr[tio->inbuf_cur],
			            tio->inbuf_len * HAWK_SIZEOF(tio->in.buf.ptr[0]));
			tio->inbuf_cur = 0;
			goto getc_conv; /* and read more */
		}

		/* get going if some characters are handled */
	}
	else if (x == -2)
	{
		/* buffer not large enough */
		HAWK_ASSERT (tio->hawk, wlen > 0);
		
		/* the wide-character buffer is not just large enough to
		 * hold the entire conversion result. lets's go on so long as 
		 * 1 wide-character is produced though it may be inefficient.
		 */
	}
	else if (x <= -1)
	{
		/* illegal sequence */
		if (tio->flags & HAWK_TIO_IGNOREECERR)
		{
		ignore_illseq:
			tio->inbuf_cur++; /* skip one byte */
			buf[wlen++] = '?';
		}
		else if (wlen <= 0)
		{
			/* really illegal sequence */
			hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EECERR);
			return -1;
		}
		else
		{
			/* some characters are already handled.
			 * mark that an illegal sequence encountered
			 * and carry on. */
			tio->status |= STATUS_INPUT_ILLSEQ;
		}
	}
	
	return wlen;
}

hawk_ooi_t hawk_tio_readuchars (hawk_tio_t* tio, hawk_uch_t* buf, hawk_oow_t size)
{
	hawk_oow_t nread = 0;
	hawk_ooi_t n;

	/*HAWK_ASSERT (tio->hawk, tio->in.fun != HAWK_NULL);*/
	if (tio->in.fun == HAWK_NULL) 
	{
		/* no input handler function */
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	if (size > HAWK_TYPE_MAX(hawk_ooi_t)) size = HAWK_TYPE_MAX(hawk_ooi_t);

	while (nread < size)
	{
		if (tio->status & STATUS_INPUT_ILLSEQ) 
		{
			tio->status &= ~STATUS_INPUT_ILLSEQ;
			hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EECERR);
			return -1;
		}
		
		n = tio_read_uchars(tio, &buf[nread], size - nread);
		if (n == 0) break;
		if (n <= -1) return -1;

		nread += n;
		if (buf[nread-1] == '\n') break;
	}

	return nread;
}


/* ------------------------------------------------------------- */
hawk_ooi_t hawk_tio_writebchars (hawk_tio_t* tio, const hawk_bch_t* mptr, hawk_oow_t mlen)
{
	if (tio->outbuf_len >= tio->out.buf.capa) 
	{
		/* maybe, previous flush operation has failed a few 
		 * times previously. so the buffer is full.
		 */
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EBUFFULL);
		return -1;
	}

	if (mlen == (hawk_oow_t)-1)
	{
		hawk_oow_t pos = 0;

		if (tio->flags & HAWK_TIO_NOAUTOFLUSH)
		{
			while (mptr[pos] != '\0') 
			{
				tio->out.buf.ptr[tio->outbuf_len++] = mptr[pos++];
				if (tio->outbuf_len >= tio->out.buf.capa &&
				    hawk_tio_flush(tio) <= -1) return -1;
				if (pos >= HAWK_TYPE_MAX(hawk_ooi_t)) break;
			}
		}
		else
		{
			int nl = 0;
			while (mptr[pos] != '\0') 
			{
				tio->out.buf.ptr[tio->outbuf_len++] = mptr[pos];
				if (tio->outbuf_len >= tio->out.buf.capa)
				{
					if (hawk_tio_flush(tio) <= -1) return -1;
					nl = 0;
				}
				else if (mptr[pos] == '\n') nl = 1; 
				/* TODO: different line terminator */
				if (++pos >= HAWK_TYPE_MAX(hawk_ooi_t)) break;
			}
			if (nl && hawk_tio_flush(tio) <= -1) return -1;
		}

		return pos;
	}
	else
	{
		const hawk_bch_t* xptr, * xend;
		hawk_oow_t capa;
		int nl = 0;

		/* adjust mlen for the type difference between the parameter
		 * and the return value */
		if (mlen > HAWK_TYPE_MAX(hawk_ooi_t)) mlen = HAWK_TYPE_MAX(hawk_ooi_t);
		xptr = mptr;

		/* handle the parts that can't fit into the internal buffer */
		while (mlen >= (capa = tio->out.buf.capa - tio->outbuf_len))
		{
			for (xend = xptr + capa; xptr < xend; xptr++)
				tio->out.buf.ptr[tio->outbuf_len++] = *xptr;
			if (hawk_tio_flush(tio) <= -1) return -1;
			mlen -= capa;
		}

		if (tio->flags & HAWK_TIO_NOAUTOFLUSH)
		{
			/* handle the last part that can fit into the internal buffer */
			for (xend = xptr + mlen; xptr < xend; xptr++)
				tio->out.buf.ptr[tio->outbuf_len++] = *xptr;
		}
		else
		{
			/* handle the last part that can fit into the internal buffer */
			for (xend = xptr + mlen; xptr < xend; xptr++)
			{
				/* TODO: support different line terminating characeter */
				if (*xptr == '\n')
				{
					nl = 1; 
					break;
				}

				tio->out.buf.ptr[tio->outbuf_len++] = *xptr;
			}

			/* continue copying without checking for nl */
			while (xptr < xend) tio->out.buf.ptr[tio->outbuf_len++] = *xptr++;
		}

		/* if the last part contains a new line, flush the internal
		 * buffer. note that this flushes characters after nl also.*/
		if (nl && hawk_tio_flush(tio) <= -1) return -1;

		/* returns the number multi-byte characters handled */
		return xptr - mptr;
	}
}

hawk_ooi_t hawk_tio_writeuchars (hawk_tio_t* tio, const hawk_uch_t* wptr, hawk_oow_t wlen)
{
	hawk_oow_t capa, wcnt, mcnt, xwlen;
	int n, nl = 0;

	if (tio->outbuf_len >= tio->out.buf.capa) 
	{
		/* maybe, previous flush operation has failed a few 
		 * times previously. so the buffer is full. */
		hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EBUFFULL);
		return -1;
	}

	if (wlen == (hawk_oow_t)-1) wlen = hawk_count_ucstr(wptr);
	if (wlen > HAWK_TYPE_MAX(hawk_ooi_t)) wlen = HAWK_TYPE_MAX(hawk_ooi_t);

	xwlen = wlen;
	while (xwlen > 0)
	{
		capa = tio->out.buf.capa - tio->outbuf_len;
		wcnt = xwlen; mcnt = capa;

		n = hawk_conv_uchars_to_bchars_with_cmgr(wptr, &wcnt, &tio->out.buf.ptr[tio->outbuf_len], &mcnt, tio->cmgr);
		tio->outbuf_len += mcnt;

		if (n == -2)
		{
			/* the buffer is not large enough to 
			 * convert more. so flush now and continue.
			 * note that the buffer may not be full though 
			 * it is not large enough in this case */
			if (hawk_tio_flush(tio) <= -1) return -1;
			nl = 0;
		}
		else 
		{
			if (tio->outbuf_len >= tio->out.buf.capa)
			{
				/* flush the full buffer regardless of conversion
				 * result. */
				if (hawk_tio_flush (tio) <= -1) return -1;
				nl = 0;
			}

			if (n <= -1)
			{
				/* an invalid wide-character is encountered. */
				if (tio->flags & HAWK_TIO_IGNOREECERR)
				{
					/* insert a question mark for an illegal 
					 * character. */
					HAWK_ASSERT (tio->hawk, tio->outbuf_len < tio->out.buf.capa);
					tio->out.buf.ptr[tio->outbuf_len++] = '?';
					wcnt++; /* skip this illegal character */
					/* don't need to increment mcnt since
					 * it's not used below */
				}
				else
				{
					/* illegal character */
					hawk_gem_seterrnum (tio->gem, HAWK_NULL, HAWK_EECERR);
					return -1;
				}
			}
			else
			{
				if (!(tio->flags & HAWK_TIO_NOAUTOFLUSH) && !nl)
				{
					/* checking for a newline this way looks damn ugly.
					 * TODO: how can i do this more elegantly? */
					hawk_oow_t i = wcnt;
					while (i > 0)
					{
						/* scan backward assuming a line terminator
						 * is typically at the back */
						if (wptr[--i] == '\n')  
						{
							/* TOOD: handle different line terminator */
							nl = 1; 
							break;
						}
					}
				}
			}
		}
		wptr += wcnt; xwlen -= wcnt;
	}

	if (nl && hawk_tio_flush(tio) <= -1) return -1;
	return wlen;
}
