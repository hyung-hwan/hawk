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

enum io_mask_t
{
	IO_MASK_READ  = 0x0100,
	IO_MASK_WRITE = 0x0200,
	IO_MASK_RDWR  = 0x0400,
	IO_MASK_CLEAR = 0x00FF
};

static hawk_rio_type_t in_type_map[] =
{
	/* the order should match the order of the
	 * HAWK_IN_XXX values in tree.h */
	HAWK_RIO_PIPE,
	HAWK_RIO_PIPE,
	HAWK_RIO_FILE,
	HAWK_RIO_CONSOLE
};

static int in_mode_map[] =
{
	/* the order should match the order of the
	 * HAWK_IN_XXX values in tree.h */
	HAWK_RIO_PIPE_READ,
	HAWK_RIO_PIPE_RW,
	HAWK_RIO_FILE_READ,
	HAWK_RIO_CONSOLE_READ
};

static int in_mask_map[] =
{
	IO_MASK_READ,
	IO_MASK_RDWR,
	IO_MASK_READ,
	IO_MASK_READ
};

static hawk_rio_type_t out_type_map[] =
{
	/* the order should match the order of the
	 * HAWK_OUT_XXX values in tree.h */
	HAWK_RIO_PIPE,
	HAWK_RIO_PIPE,
	HAWK_RIO_FILE,
	HAWK_RIO_FILE,
	HAWK_RIO_CONSOLE
};

static int out_mode_map[] =
{
	/* the order should match the order of the
	 * HAWK_OUT_XXX values in tree.h */
	HAWK_RIO_PIPE_WRITE,
	HAWK_RIO_PIPE_RW,
	HAWK_RIO_FILE_WRITE,
	HAWK_RIO_FILE_APPEND,
	HAWK_RIO_CONSOLE_WRITE
};

static int out_mask_map[] =
{
	IO_MASK_WRITE,
	IO_MASK_RDWR,
	IO_MASK_WRITE,
	IO_MASK_WRITE,
	IO_MASK_WRITE
};

hawk_rio_type_t hawk_rtx_intoriotype (hawk_rtx_t* rtx, hawk_in_type_t in_type)
{
	return in_type_map[in_type];
}

hawk_rio_type_t hawk_rtx_outtoriotype (hawk_rtx_t* rtx, hawk_out_type_t out_type)
{
	return out_type_map[out_type];
}

static int find_rio_in (
	hawk_rtx_t* rtx, hawk_in_type_t in_type, const hawk_ooch_t* name,
	int mbs_if_new, hawk_rio_arg_t** rio, hawk_rio_impl_t* fun)
{
	hawk_rio_arg_t* p = rtx->rio.chain;
	hawk_rio_impl_t handler;
	int io_type, io_mode, io_mask;

	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_type_map));
	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_mode_map));
	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_mask_map));

	/* translate the in_type into the relevant io type and mode */
	io_type = in_type_map[in_type];
	io_mode = in_mode_map[in_type];
	io_mask = in_mask_map[in_type];

	/* get the I/O handler provided by a user */
	handler = rtx->rio.handler[io_type];
	if (handler == HAWK_NULL)
	{
		/* no I/O handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	/* search the chain for exiting an existing io name */
	while (p)
	{
		if (p->type == (io_type | io_mask) &&
		    hawk_comp_oocstr(p->name, name, 0) == 0) break;
		p = p->next;
	}

	if (p == HAWK_NULL)
	{
		hawk_ooi_t x;

		/* if the name doesn't exist in the chain, create an entry
		 * to the chain */
		p = (hawk_rio_arg_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_rio_arg_t));
		if (p == HAWK_NULL) return -1;

		p->name = hawk_rtx_dupoocstr(rtx, name, HAWK_NULL);
		if (p->name == HAWK_NULL)
		{
			hawk_rtx_freemem (rtx, p);
			return -1;
		}

		p->type = (io_type | io_mask);
		p->mode = io_mode;
		p->rwcmode = HAWK_RIO_CMD_CLOSE_FULL;
		/*
		p->handle = HAWK_NULL;
		p->next = HAWK_NULL;
		p->rwcstate = 0;

		p->in.buf[0] = HAWK_T('\0');
		p->in.pos = 0;
		p->in.len = 0;
		p->in.eof = 0;
		p->in.eos = 0;
		*/
		p->in.mbs = mbs_if_new;

		/* request to open the stream */
		x = handler(rtx, HAWK_RIO_CMD_OPEN, p, HAWK_NULL, 0);
		if (x <= -1)
		{
			hawk_rtx_freemem (rtx, p->name);
			hawk_rtx_freemem (rtx, p);
			return -1;
		}

		/* chain it */
		p->next = rtx->rio.chain;
		rtx->rio.chain = p;
	}

	*rio = p;
	*fun = handler;

	return 0;
}

static HAWK_INLINE int resolve_rs (hawk_rtx_t* rtx, hawk_val_t* rs, hawk_oocs_t* rrs)
{
	int ret = 0;
	hawk_val_type_t rs_vtype;


	rs_vtype = HAWK_RTX_GETVALTYPE(rtx, rs);

	switch (rs_vtype)
	{
		case HAWK_VAL_NIL:
			rrs->ptr = HAWK_NULL;
			rrs->len = 0;
			break;

		case HAWK_VAL_STR:
			rrs->ptr = ((hawk_val_str_t*)rs)->val.ptr;
			rrs->len = ((hawk_val_str_t*)rs)->val.len;
			break;

		default:
			rrs->ptr = hawk_rtx_valtooocstrdup(rtx, rs, &rrs->len);
			if (rrs->ptr == HAWK_NULL) ret = -1;
			break;
	}

	return ret;
}

static HAWK_INLINE int resolve_brs (hawk_rtx_t* rtx, hawk_val_t* brs, hawk_bcs_t* rrs)
{
	/* record separator for bytes reading */

	int ret = 0;
	hawk_val_type_t brs_vtype;

	brs_vtype = HAWK_RTX_GETVALTYPE(rtx, brs);

	switch (brs_vtype)
	{
		case HAWK_VAL_NIL:
			rrs->ptr = HAWK_NULL;
			rrs->len = 0;
			break;

		case HAWK_VAL_MBS:
			rrs->ptr = ((hawk_val_mbs_t*)brs)->val.ptr;
			rrs->len = ((hawk_val_mbs_t*)brs)->val.len;
			break;

		case HAWK_VAL_BOB:
			rrs->ptr = (hawk_bch_t*)((hawk_val_bob_t*)brs)->val.ptr;
			rrs->len = ((hawk_val_bob_t*)brs)->val.len;
			break;

		default:
			rrs->ptr = hawk_rtx_valtobcstrdup(rtx, brs, &rrs->len);
			if (rrs->ptr == HAWK_NULL) ret = -1;
			break;
	}

	return ret;
}

static HAWK_INLINE int match_long_rs (hawk_rtx_t* rtx, hawk_ooecs_t* buf, hawk_rio_arg_t* p)
{
	hawk_oocs_t match;
	int ret;

	HAWK_ASSERT (rtx->gbl.rs[0] != HAWK_NULL);
	HAWK_ASSERT (rtx->gbl.rs[1] != HAWK_NULL);

	ret = hawk_rtx_matchrexwithoocs(rtx, rtx->gbl.rs[rtx->gbl.ignorecase], HAWK_OOECS_OOCS(buf), HAWK_OOECS_OOCS(buf), &match, HAWK_NULL);
	if (ret >= 1)
	{
		if (p->in.eof)
		{
			/* when EOF is reached, the record buffer
			 * is not added with a new character. It's
			 * just called again with the same record buffer
			 * as the previous call to this function.
			 * A match in this case must end at the end of
			 * the current record buffer */
			HAWK_ASSERT (HAWK_OOECS_PTR(buf) + HAWK_OOECS_LEN(buf) == match.ptr + match.len);

			/* drop the RS part. no extra character after RS to drop
			 * because we're at EOF and the EOF condition didn't
			 * add a new character to the buffer before the call
			 * to this function.
			 */
			HAWK_OOECS_LEN(buf) -= match.len;
		}
		else
		{
			/* If the match is found before the end of the current buffer,
			 * I see it as the longest match. A match ending at the end
			 * of the buffer is not indeterministic as we don't have the
			 * full input yet.
			 */
			const hawk_ooch_t* be = HAWK_OOECS_PTR(buf) + HAWK_OOECS_LEN(buf);
			const hawk_ooch_t* me = match.ptr + match.len;

			if (me < be)
			{
				/* the match ends before the ending boundary.
				 * it must be the longest match. drop the RS part
				 * and the characters after RS. */
				HAWK_OOECS_LEN(buf) -= match.len + (be - me);
				p->in.pos -= (be - me);
			}
			else
			{
				/* the match is at the ending boundary. switch to no match */
				ret = 0;
			}
		}
	}

	return ret;
}

static HAWK_INLINE int match_long_brs(hawk_rtx_t* rtx, hawk_becs_t* buf, hawk_rio_arg_t* p)
{
	hawk_bcs_t match;
	int ret;

	HAWK_ASSERT (rtx->gbl.rs[0] != HAWK_NULL);
	HAWK_ASSERT (rtx->gbl.rs[1] != HAWK_NULL);


	ret = hawk_rtx_matchrexwithbcs(rtx, rtx->gbl.rs[rtx->gbl.ignorecase], HAWK_BECS_BCS(buf), HAWK_BECS_BCS(buf), &match, HAWK_NULL);
	if (ret >= 1)
	{
		if (p->in.eof)
		{
			/* when EOF is reached, the record buffer
			 * is not added with a new character. It's
			 * just called again with the same record buffer
			 * as the previous call to this function.
			 * A match in this case must end at the end of
			 * the current record buffer */
			HAWK_ASSERT (HAWK_BECS_PTR(buf) + HAWK_BECS_LEN(buf) == match.ptr + match.len);

			/* drop the RS part. no extra character after RS to drop
			 * because we're at EOF and the EOF condition didn't
			 * add a new character to the buffer before the call
			 * to this function.
			 */
			HAWK_BECS_LEN(buf) -= match.len;
		}
		else
		{
			/* If the match is found before the end of the current buffer,
			 * I see it as the longest match. A match ending at the end
			 * of the buffer is not indeterministic as we don't have the
			 * full input yet.
			 */
			const hawk_bch_t* be = HAWK_BECS_PTR(buf) + HAWK_BECS_LEN(buf);
			const hawk_bch_t* me = match.ptr + match.len;

			if (me < be)
			{
				/* the match ends before the ending boundary.
				 * it must be the longest match. drop the BRS part
				 * and the characters after BRS. */
				HAWK_BECS_LEN(buf) -= match.len + (be - me);
				p->in.pos -= (be - me);
			}
			else
			{
				/* the match is at the ending boundary. switch to no match */
				ret = 0;
			}
		}
	}

	return ret;
}

int hawk_rtx_readio (hawk_rtx_t* rtx, hawk_in_type_t in_type, const hawk_ooch_t* name, hawk_ooecs_t* buf)
{
	hawk_rio_arg_t* p;
	hawk_rio_impl_t handler;
	int ret;

	hawk_val_t* rs;
	hawk_oocs_t rrs;

	hawk_oow_t line_len = 0;
	hawk_ooch_t c = HAWK_T('\0'), pc;

	if (find_rio_in(rtx, in_type, name, 0, &p, &handler) <= -1) return -1;
	if (p->in.eos) return 0; /* no more streams left */
	if (p->in.mbs)
	{
		if (name[0] == '\0')
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EPERM, HAWK_T("disallowed mixed mode input"));
		else
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EPERM, HAWK_T("disallowed mixed mode input on %js"), name);
		return -1;
	}

	/* ready to read a record(typically a line). clear the buffer. */
	hawk_ooecs_clear (buf);

	/* get the record separator */
	rs = hawk_rtx_getgbl(rtx, HAWK_GBL_RS);
	hawk_rtx_refupval (rtx, rs);

	if (resolve_rs(rtx, rs, &rrs) <= -1)
	{
		hawk_rtx_refdownval (rtx, rs);
		return -1;
	}

	ret = 1;

	/* call the I/O handler */
	while (1)
	{
		if (p->in.pos >= p->in.len)
		{
			hawk_ooi_t x;

			/* no more data in the read buffer.
			 * let the I/O handler read more */

			if (p->in.eof)
			{
				/* it has reached EOF at the previous call. */
				if (HAWK_OOECS_LEN(buf) == 0)
				{
					/* we return EOF if the record buffer is empty */
					ret = 0;
				}
				break;
			}

			x = handler(rtx, HAWK_RIO_CMD_READ, p, p->in.u.buf, HAWK_COUNTOF(p->in.u.buf));
			if (x <= -1)
			{
				ret = -1;
				break;
			}

			if (x == 0)
			{
				/* EOF reached */
				p->in.eof = 1;

				if (HAWK_OOECS_LEN(buf) == 0)
				{
					/* We can return EOF now if the record buffer is empty */
					ret = 0;
				}
				else if (rrs.ptr && rrs.len == 0)
				{
					/* TODO: handle different line terminator */
					/* drop the line terminator from the record
					 * if RS is a blank line and EOF is reached. */
					if (HAWK_OOECS_LASTCHAR(buf) == HAWK_T('\n'))
					{
						HAWK_OOECS_LEN(buf) -= 1;
						if (rtx->hawk->opt.trait & HAWK_CRLF)
						{
							/* drop preceding CR */
							if (HAWK_OOECS_LEN(buf) > 0 && HAWK_OOECS_LASTCHAR(buf) == HAWK_T('\r')) HAWK_OOECS_LEN(buf) -= 1;
						}
					}
				}
				else if (rrs.len >= 2)
				{
					/* When RS is multiple characters, it should
					 * check for the match at the end of the
					 * input stream also because the previous
					 * match could fail as it didn't end at the
					 * desired position to be the longest match.
					 * At EOF, the match at the end is considered
					 * the longest as there are no more characters
					 * left */
					int n = match_long_rs(rtx, buf, p);
					if (n != 0)
					{
						if (n <= -1) ret = -1;
						break;
					}
				}

				break;
			}

			p->in.len = x;
			p->in.pos = 0;
		}

		if (rrs.ptr == HAWK_NULL)
		{
			hawk_oow_t start_pos = p->in.pos;
			hawk_oow_t end_pos, tmp;

			do
			{
				pc = c;
				c = p->in.u.buf[p->in.pos++];
				end_pos = p->in.pos;

				/* TODO: handle different line terminator */
				/* separate by a new line */
				if (c == HAWK_T('\n'))
				{
					end_pos--;
					if (pc == HAWK_T('\r'))
					{
						if (end_pos > start_pos)
						{
							/* CR is the part of the read buffer.
							 * decrementing the end_pos variable can
							 * simply drop it */
							end_pos--;
						}
						else
						{
							/* CR must have come from the previous
							 * read. drop CR that must be found  at
							 * the end of the record buffer. */
							HAWK_ASSERT (end_pos == start_pos);
							HAWK_ASSERT (HAWK_OOECS_LEN(buf) > 0);
							HAWK_ASSERT (HAWK_OOECS_LASTCHAR(buf) == HAWK_T('\r'));
							HAWK_OOECS_LEN(buf)--;
						}
					}
					break;
				}
			}
			while (p->in.pos < p->in.len);

			tmp = hawk_ooecs_ncat(buf, &p->in.u.buf[start_pos], end_pos - start_pos);
			if (tmp == (hawk_oow_t)-1)
			{
				ret = -1;
				break;
			}

			if (end_pos < p->in.len) break; /* RS found */
		}
		else if (rrs.len == 0)
		{
			int done = 0;

			do
			{
				pc = c;
				c = p->in.u.buf[p->in.pos++];

				/* TODO: handle different line terminator */
				/* separate by a blank line */
				if (c == HAWK_T('\n'))
				{
					if (pc == HAWK_T('\r') && HAWK_OOECS_LEN(buf) > 0)
					{
						/* shrink the line length and the record
						 * by dropping of CR before NL */
						HAWK_ASSERT (line_len > 0);
						line_len--;

						/* we don't drop CR from the record buffer
						 * if we're in CRLF mode. POINT-X */
						if (!(rtx->hawk->opt.trait & HAWK_CRLF))
							HAWK_OOECS_LEN(buf) -= 1;
					}

					if (line_len == 0)
					{
						/* we got a blank line */

						if (rtx->hawk->opt.trait & HAWK_CRLF)
						{
							if (HAWK_OOECS_LEN(buf) > 0 && HAWK_OOECS_LASTCHAR(buf) == HAWK_T('\r'))
							{
								/* drop CR not dropped in POINT-X above */
								HAWK_OOECS_LEN(buf) -= 1;
							}

							if (HAWK_OOECS_LEN(buf) <= 0)
							{
								/* if the record is empty when a blank
								 * line is encountered, the line
								 * terminator should not be added to
								 * the record */
								continue;
							}

							/* drop NL */
							HAWK_OOECS_LEN(buf) -= 1;

							/* drop preceding CR */
							if (HAWK_OOECS_LEN(buf) > 0 && HAWK_OOECS_LASTCHAR(buf) == HAWK_T('\r')) HAWK_OOECS_LEN(buf) -= 1;
						}
						else
						{
							if (HAWK_OOECS_LEN(buf) <= 0)
							{
								/* if the record is empty when a blank
								 * line is encountered, the line
								 * terminator should not be added to
								 * the record */
								continue;
							}

							/* drop NL of the previous line */
							HAWK_OOECS_LEN(buf) -= 1; /* simply drop NL */
						}

						done = 1;
						break;
					}

					line_len = 0;
				}
				else line_len++;

				if (hawk_ooecs_ccat(buf, c) == (hawk_oow_t)-1)
				{

					ret = -1;
					done = 1;
					break;
				}
			}
			while (p->in.pos < p->in.len);

			if (done) break;
		}
		else if (rrs.len == 1)
		{
			hawk_oow_t start_pos = p->in.pos;
			hawk_oow_t end_pos, tmp;

			do
			{
				c = p->in.u.buf[p->in.pos++];
				end_pos = p->in.pos;
				if (c == rrs.ptr[0])
				{
					end_pos--;
					break;
				}
			}
			while (p->in.pos < p->in.len);

			tmp = hawk_ooecs_ncat(buf, &p->in.u.buf[start_pos], end_pos - start_pos);
			if (tmp == (hawk_oow_t)-1)
			{
				ret = -1;
				break;
			}

			if (end_pos < p->in.len) break; /* RS found */
		}
		else
		{
			hawk_oow_t tmp;
			int n;

			/* if RS is composed of multiple characters,
			 * I perform the matching after having added the
			 * current character 'c' to the record buffer 'buf'
			 * to find the longest match. If a match found ends
			 * one character before this character just added
			 * to the buffer, it is the longest match.
			 */

			tmp = hawk_ooecs_ncat(buf, &p->in.u.buf[p->in.pos], p->in.len - p->in.pos);
			if (tmp == (hawk_oow_t)-1)
			{
				ret = -1;
				break;
			}

			p->in.pos = p->in.len;

			n = match_long_rs(rtx, buf, p);
			if (n != 0)
			{
				if (n <= -1) ret = -1;
				break;
			}
		}
	}

	if (rrs.ptr && HAWK_RTX_GETVALTYPE(rtx, rs) != HAWK_VAL_STR) hawk_rtx_freemem (rtx, rrs.ptr);
	hawk_rtx_refdownval (rtx, rs);

	return ret;
}


int hawk_rtx_readiobytes (hawk_rtx_t* rtx, hawk_in_type_t in_type, const hawk_ooch_t* name, hawk_becs_t* buf)
{
	hawk_rio_arg_t* p;
	hawk_rio_impl_t handler;
	int ret;

	hawk_val_t* brs;
	hawk_bcs_t rrs;

	hawk_oow_t line_len = 0;
	hawk_bch_t c = '\0', pc;

	if (find_rio_in(rtx, in_type, name, 1, &p, &handler) <= -1) return -1;
	if (p->in.eos) return 0; /* no more streams left */
	if (!p->in.mbs)
	{
		if (name[0] == '\0')
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EPERM, HAWK_T("disallowed mixed mode input"));
		else
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EPERM, HAWK_T("disallowed mixed mode input on %js"), name);
		return -1;
	}

	/* ready to read a record(typically a line). clear the buffer. */
	hawk_becs_clear (buf);

	/* get the record separator */
	brs = hawk_rtx_getgbl(rtx, HAWK_GBL_RS);
	hawk_rtx_refupval (rtx, brs);

	if (resolve_brs(rtx, brs, &rrs) <= -1)
	{
		hawk_rtx_refdownval (rtx, brs);
		return -1;
	}

	ret = 1;

	/* call the I/O handler */
	while (1)
	{
		if (p->in.pos >= p->in.len)
		{
			hawk_ooi_t x;

			/* no more data in the read buffer.
			 * let the I/O handler read more */

			if (p->in.eof)
			{
				/* it has reached EOF at the previous call. */
				if (HAWK_BECS_LEN(buf) == 0)
				{
					/* we return EOF if the record buffer is empty */
					ret = 0;
				}
				break;
			}

			x = handler(rtx, HAWK_RIO_CMD_READ_BYTES, p, p->in.u.bbuf, HAWK_COUNTOF(p->in.u.bbuf));
			if (x <= -1)
			{
				ret = -1;
				break;
			}

			if (x == 0)
			{
				/* EOF reached */
				p->in.eof = 1;

				if (HAWK_BECS_LEN(buf) == 0)
				{
					/* We can return EOF now if the record buffer is empty */
					ret = 0;
				}
				else if (rrs.ptr && rrs.len == 0)
				{
					/* TODO: handle different line terminator */
					/* drop the line terminator from the record
					 * if RS is a blank line and EOF is reached. */
					if (HAWK_BECS_LASTCHAR(buf) == '\n')
					{
						HAWK_BECS_LEN(buf) -= 1;
						if (rtx->hawk->opt.trait & HAWK_CRLF)
						{
							/* drop preceding CR */
							if (HAWK_BECS_LEN(buf) > 0 && HAWK_BECS_LASTCHAR(buf) == '\r') HAWK_BECS_LEN(buf) -= 1;
						}
					}
				}
				else if (rrs.len >= 2)
				{
					/* When RS is multiple characters, it should
					 * check for the match at the end of the
					 * input stream also because the previous
					 * match could fail as it didn't end at the
					 * desired position to be the longest match.
					 * At EOF, the match at the end is considered
					 * the longest as there are no more characters
					 * left */
					int n = match_long_brs(rtx, buf, p);
					if (n != 0)
					{
						if (n <= -1) ret = -1;
						break;
					}
				}

				break;
			}

			p->in.len = x;
			p->in.pos = 0;
		}

		if (rrs.ptr == HAWK_NULL)
		{
			hawk_oow_t start_pos = p->in.pos;
			hawk_oow_t end_pos, tmp;

			do
			{
				pc = c;
				c = p->in.u.bbuf[p->in.pos++];
				end_pos = p->in.pos;

				/* TODO: handle different line terminator */
				/* separate by a new line */
				if (c == '\n')
				{
					end_pos--;
					if (pc == '\r')
					{
						if (end_pos > start_pos)
						{
							/* CR is the part of the read buffer.
							 * decrementing the end_pos variable can
							 * simply drop it */
							end_pos--;
						}
						else
						{
							/* CR must have come from the previous
							 * read. drop CR that must be found  at
							 * the end of the record buffer. */
							HAWK_ASSERT (end_pos == start_pos);
							HAWK_ASSERT (HAWK_BECS_LEN(buf) > 0);
							HAWK_ASSERT (HAWK_BECS_LASTCHAR(buf) == '\r');
							HAWK_BECS_LEN(buf)--;
						}
					}
					break;
				}
			}
			while (p->in.pos < p->in.len);

			tmp = hawk_becs_ncat(buf, &p->in.u.bbuf[start_pos], end_pos - start_pos);
			if (tmp == (hawk_oow_t)-1)
			{
				ret = -1;
				break;
			}

			if (end_pos < p->in.len) break; /* RS found */
		}
		else if (rrs.len == 0)
		{
			int done = 0;

			do
			{
				pc = c;
				c = p->in.u.bbuf[p->in.pos++];

				/* TODO: handle different line terminator */
				/* separate by a blank line */
				if (c == '\n')
				{
					if (pc == '\r' && HAWK_BECS_LEN(buf) > 0)
					{
						/* shrink the line length and the record
						 * by dropping of CR before NL */
						HAWK_ASSERT (line_len > 0);
						line_len--;

						/* we don't drop CR from the record buffer
						 * if we're in CRLF mode. POINT-X */
						if (!(rtx->hawk->opt.trait & HAWK_CRLF))
							HAWK_BECS_LEN(buf) -= 1;
					}

					if (line_len == 0)
					{
						/* we got a blank line */

						if (rtx->hawk->opt.trait & HAWK_CRLF)
						{
							if (HAWK_BECS_LEN(buf) > 0 && HAWK_BECS_LASTCHAR(buf) == '\r')
							{
								/* drop CR not dropped in POINT-X above */
								HAWK_BECS_LEN(buf) -= 1;
							}

							if (HAWK_BECS_LEN(buf) <= 0)
							{
								/* if the record is empty when a blank
								 * line is encountered, the line
								 * terminator should not be added to
								 * the record */
								continue;
							}

							/* drop NL */
							HAWK_BECS_LEN(buf) -= 1;

							/* drop preceding CR */
							if (HAWK_BECS_LEN(buf) > 0 && HAWK_BECS_LASTCHAR(buf) == '\r') HAWK_BECS_LEN(buf) -= 1;
						}
						else
						{
							if (HAWK_BECS_LEN(buf) <= 0)
							{
								/* if the record is empty when a blank
								 * line is encountered, the line
								 * terminator should not be added to
								 * the record */
								continue;
							}

							/* drop NL of the previous line */
							HAWK_BECS_LEN(buf) -= 1; /* simply drop NL */
						}

						done = 1;
						break;
					}

					line_len = 0;
				}
				else line_len++;

				if (hawk_becs_ccat(buf, c) == (hawk_oow_t)-1)
				{

					ret = -1;
					done = 1;
					break;
				}
			}
			while (p->in.pos < p->in.len);

			if (done) break;
		}
		else if (rrs.len == 1)
		{
			hawk_oow_t start_pos = p->in.pos;
			hawk_oow_t end_pos, tmp;

			do
			{
				c = p->in.u.bbuf[p->in.pos++];
				end_pos = p->in.pos;
				if (c == rrs.ptr[0])
				{
					end_pos--;
					break;
				}
			}
			while (p->in.pos < p->in.len);

			tmp = hawk_becs_ncat(buf, &p->in.u.bbuf[start_pos], end_pos - start_pos);
			if (tmp == (hawk_oow_t)-1)
			{
				ret = -1;
				break;
			}

			if (end_pos < p->in.len) break; /* RS found */
		}
		else
		{
			hawk_oow_t tmp;
			int n;

			/* if RS is composed of multiple characters,
			 * I perform the matching after having added the
			 * current character 'c' to the record buffer 'buf'
			 * to find the longest match. If a match found ends
			 * one character before this character just added
			 * to the buffer, it is the longest match.
			 */

			tmp = hawk_becs_ncat(buf, &p->in.u.bbuf[p->in.pos], p->in.len - p->in.pos);
			if (tmp == (hawk_oow_t)-1)
			{
				ret = -1;
				break;
			}

			p->in.pos = p->in.len;

			n = match_long_brs(rtx, buf, p);
			if (n != 0)
			{
				if (n <= -1) ret = -1;
				break;
			}
		}
	}

	if (rrs.ptr && HAWK_RTX_GETVALTYPE(rtx, brs) != HAWK_VAL_MBS) hawk_rtx_freemem (rtx, rrs.ptr);
	hawk_rtx_refdownval (rtx, brs);

	return ret;
}


int hawk_rtx_writeioval (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name, hawk_val_t* v)
{
	hawk_val_type_t vtype;
	vtype = HAWK_RTX_GETVALTYPE (rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_CHAR:
		{
			hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
			return hawk_rtx_writeiostr(rtx, out_type, name, &tmp, 1);
		}

		case HAWK_VAL_STR:
			return hawk_rtx_writeiostr(rtx, out_type, name, ((hawk_val_str_t*)v)->val.ptr, ((hawk_val_str_t*)v)->val.len);

		case HAWK_VAL_BCHR:
		{
			hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
			return hawk_rtx_writeiobytes(rtx, out_type, name, &tmp, 1);
		}

		case HAWK_VAL_MBS:
			return hawk_rtx_writeiobytes(rtx, out_type, name, ((hawk_val_mbs_t*)v)->val.ptr, ((hawk_val_mbs_t*)v)->val.len);

		case HAWK_VAL_BOB:
			return hawk_rtx_writeiobytes(rtx, out_type, name, (hawk_bch_t*)((hawk_val_bob_t*)v)->val.ptr, ((hawk_val_bob_t*)v)->val.len);

		default:
		{
			hawk_rtx_valtostr_out_t out;
			int n;
			out.type = HAWK_RTX_VALTOSTR_CPLDUP | HAWK_RTX_VALTOSTR_PRINT;
			if (hawk_rtx_valtostr(rtx, v, &out) <= -1) return -1;
			n = hawk_rtx_writeiostr(rtx, out_type, name, out.u.cpldup.ptr, out.u.cpldup.len);
			hawk_rtx_freemem (rtx, out.u.cpldup.ptr);
			return n;
		}
	}
}

struct write_io_data_t
{
	hawk_rio_arg_t* p;
	hawk_rio_impl_t handler;
};
typedef struct write_io_data_t write_io_data_t;

static int prepare_for_write_io_data (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name, write_io_data_t* wid)
{
	hawk_rio_arg_t* p = rtx->rio.chain;
	hawk_rio_impl_t handler;
	int io_type, io_mode, io_mask, n;

	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_type_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mode_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mask_map));

	/* translate the out_type into the relevant io type and mode */
	io_type = out_type_map[out_type];
	io_mode = out_mode_map[out_type];
	io_mask = out_mask_map[out_type];

	handler = rtx->rio.handler[io_type];
	if (HAWK_UNLIKELY(!handler))
	{
		/* no I/O handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	/* look for the corresponding rio for name */
	while (p)
	{
		/* the file "1.tmp", in the following code snippets,
		 * would be opened by the first print statement, but not by
		 * the second print statement. this is because
		 * both HAWK_OUT_FILE and HAWK_OUT_APFILE are
		 * translated to HAWK_RIO_FILE and it is used to
		 * keep track of file handles..
		 *
		 *    print "1111" >> "1.tmp"
		 *    print "1111" > "1.tmp"
		 */
		if (p->type == (io_type | io_mask) && hawk_comp_oocstr(p->name, name, 0) == 0) break;
		p = p->next;
	}

	/* if there is not corresponding rio for name, create one */
	if (p == HAWK_NULL)
	{
		p = (hawk_rio_arg_t*)hawk_rtx_allocmem(rtx, HAWK_SIZEOF(hawk_rio_arg_t));
		if (HAWK_UNLIKELY(!p)) return -1;

		HAWK_MEMSET (p, 0, HAWK_SIZEOF(*p));

		p->name = hawk_rtx_dupoocstr(rtx, name, HAWK_NULL);
		if (HAWK_UNLIKELY(!p->name))
		{
			hawk_rtx_freemem (rtx, p);
			return -1;
		}

		p->type = (io_type | io_mask);
		p->mode = io_mode;
		p->rwcmode = HAWK_RIO_CMD_CLOSE_FULL;
		/*
		p->handle = HAWK_NULL;
		p->next = HAWK_NULL;
		p->rwcstate = 0;

		p->out.eof = 0;
		p->out.eos = 0;
		*/

		n = handler(rtx, HAWK_RIO_CMD_OPEN, p, HAWK_NULL, 0);
		if (n <= -1)
		{
			hawk_rtx_freemem (rtx, p->name);
			hawk_rtx_freemem (rtx, p);
			return -1;
		}

		/* chain it */
		p->next = rtx->rio.chain;
		rtx->rio.chain = p;
	}

	if (p->out.eos) return 0; /* no more streams */
	if (p->out.eof) return 0; /* it has reached the end of the stream but this function has been recalled */

	wid->handler = handler;
	wid->p = p;
	return 1;
}

int hawk_rtx_writeiostr (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name, hawk_ooch_t* str, hawk_oow_t len)
{
	int x;
	write_io_data_t wid;

	if ((x = prepare_for_write_io_data(rtx, out_type, name, &wid)) <= 0) return x;

	while (len > 0)
	{
		hawk_ooi_t n;

		n = wid.handler(rtx, HAWK_RIO_CMD_WRITE, wid.p, str, len);
		if (n <= -1) return -1;

		if (n == 0)
		{
			wid.p->out.eof = 1;
			return 0;
		}

		len -= n;
		str += n;
	}

	return 1;
}

int hawk_rtx_writeiobytes (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name, hawk_bch_t* str, hawk_oow_t len)
{
	int x;
	write_io_data_t wid;

	if ((x = prepare_for_write_io_data(rtx, out_type, name, &wid)) <= 0) return x;

	while (len > 0)
	{
		hawk_ooi_t n;

		n = wid.handler(rtx, HAWK_RIO_CMD_WRITE_BYTES, wid.p, str, len);
		if (n <= -1) return -1;

		if (n == 0)
		{
			wid.p->out.eof = 1;
			return 0;
		}

		len -= n;
		str += n;
	}

	return 1;
}

int hawk_rtx_flushio (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name)
{
	hawk_rio_arg_t* p = rtx->rio.chain;
	hawk_rio_impl_t handler;
	int io_type, io_mode, io_mask;
	hawk_ooi_t n;
	int ok = 0;

	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_type_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mode_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mask_map));

	/* translate the out_type into the relevant I/O type and mode */
	io_type = out_type_map[out_type];
	io_mode = out_mode_map[out_type];
	io_mask = out_mask_map[out_type];

	handler = rtx->rio.handler[io_type];
	if (!handler)
	{
		/* no I/O handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	/* look for the corresponding rio for name */
	while (p)
	{
		/* without the check for io_mode and p->mode,
		 * HAWK_OUT_FILE and HAWK_OUT_APFILE matches the
		 * same entry since (io_type | io_mask) has the same value
		 * for both. */
		if (p->type == (io_type | io_mask) && p->mode == io_mode &&
		    (name == HAWK_NULL || hawk_comp_oocstr(p->name, name, 0) == 0))
		{
			n = handler(rtx, HAWK_RIO_CMD_FLUSH, p, HAWK_NULL, 0);
			if (n <= -1) return -1;
			ok = 1;
		}

		p = p->next;
	}

	if (ok) return 0;

	/* there is no corresponding rio for name */
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIONMNF);
	return -1;
}

int hawk_rtx_nextio_read (hawk_rtx_t* rtx, hawk_in_type_t in_type, const hawk_ooch_t* name)
{
	hawk_rio_arg_t* p = rtx->rio.chain;
	hawk_rio_impl_t handler;
	int io_type, /*io_mode,*/ io_mask;
	hawk_ooi_t n;

	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_type_map));
	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_mode_map));
	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_mask_map));

	/* translate the in_type into the relevant I/O type and mode */
	io_type = in_type_map[in_type];
	/*io_mode = in_mode_map[in_type];*/
	io_mask = in_mask_map[in_type];

	handler = rtx->rio.handler[io_type];
	if (!handler)
	{
		/* no I/O handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	while (p)
	{
		if (p->type == (io_type | io_mask) && hawk_comp_oocstr(p->name, name,  0) == 0) break;
		p = p->next;
	}

	if (!p)
	{
		/* something is totally wrong */
		HAWK_ASSERT (!"should never happen - cannot find the relevant rio entry");
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
		return -1;
	}

	if (p->in.eos)
	{
		/* no more streams. */
		return 0;
	}

	n = handler(rtx, HAWK_RIO_CMD_NEXT, p, HAWK_NULL, 0);
	if (n <= -1) return -1;

	if (n == 0)
	{
		/* the next stream cannot be opened.
		 * set the EOS flags so that the next call to nextio_read
		 * will return 0 without executing the handler */
		p->in.eos = 1;
		return 0;
	}
	else
	{
		/* as the next stream has been opened successfully,
		 * the EOF flag should be cleared if set */
		p->in.eof = 0;

		/* also the previous input buffer must be reset */
		p->in.pos = 0;
		p->in.len = 0;

		return 1;
	}
}

int hawk_rtx_nextio_write (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name)
{
	hawk_rio_arg_t* p = rtx->rio.chain;
	hawk_rio_impl_t handler;
	int io_type, /*io_mode,*/ io_mask;
	hawk_ooi_t n;

	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_type_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mode_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mask_map));

	/* translate the out_type into the relevant I/O type and mode */
	io_type = out_type_map[out_type];
	/*io_mode = out_mode_map[out_type];*/
	io_mask = out_mask_map[out_type];

	handler = rtx->rio.handler[io_type];
	if (!handler)
	{
		/* no I/O handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	while (p)
	{
		if (p->type == (io_type | io_mask) && hawk_comp_oocstr(p->name, name, 0) == 0) break;
		p = p->next;
	}

	if (!p)
	{
		/* something is totally wrong */
		HAWK_ASSERT (!"should never happen - cannot find the relevant rio entry");

		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
		return -1;
	}

	if (p->out.eos)
	{
		/* no more streams. */
		return 0;
	}

	n = handler(rtx, HAWK_RIO_CMD_NEXT, p, HAWK_NULL, 0);
	if (n <= -1) return -1;

	if (n == 0)
	{
		/* the next stream cannot be opened.
		 * set the EOS flags so that the next call to nextio_write
		 * will return 0 without executing the handler */
		p->out.eos = 1;
		return 0;
	}
	else
	{
		/* as the next stream has been opened successfully,
		 * the EOF flag should be cleared if set */
		p->out.eof = 0;
		return 1;
	}
}

int hawk_rtx_closio_read (hawk_rtx_t* rtx, hawk_in_type_t in_type, const hawk_ooch_t* name)
{
	hawk_rio_arg_t* p = rtx->rio.chain, * px = HAWK_NULL;
	hawk_rio_impl_t handler;
	int io_type, /*io_mode,*/ io_mask;

	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_type_map));
	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_mode_map));
	HAWK_ASSERT (in_type >= 0 && in_type <= HAWK_COUNTOF(in_mask_map));

	/* translate the in_type into the relevant I/O type and mode */
	io_type = in_type_map[in_type];
	/*io_mode = in_mode_map[in_type];*/
	io_mask = in_mask_map[in_type];

	handler = rtx->rio.handler[io_type];
	if (!handler)
	{
		/* no I/O handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	while (p)
	{
		if (p->type == (io_type | io_mask) && hawk_comp_oocstr(p->name, name, 0) == 0)
		{
			hawk_rio_impl_t handler;

			handler = rtx->rio.handler[p->type & IO_MASK_CLEAR];
			if (handler)
			{
				if (handler (rtx, HAWK_RIO_CMD_CLOSE, p, HAWK_NULL, 0) <= -1)
				{
					/* this is not a rtx-time error.*/
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOIMPL);
					return -1;
				}
			}

			if (px) px->next = p->next;
			else rtx->rio.chain = p->next;

			hawk_rtx_freemem (rtx, p->name);
			hawk_rtx_freemem (rtx, p);
			return 0;
		}

		px = p;
		p = p->next;
	}

	/* the name given is not found */
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIONMNF);
	return -1;
}

int hawk_rtx_closio_write (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name)
{
	hawk_rio_arg_t* p = rtx->rio.chain, * px = HAWK_NULL;
	hawk_rio_impl_t handler;
	int io_type, /*io_mode,*/ io_mask;

	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_type_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mode_map));
	HAWK_ASSERT (out_type >= 0 && out_type <= HAWK_COUNTOF(out_mask_map));

	/* translate the out_type into the relevant io type and mode */
	io_type = out_type_map[out_type];
	/*io_mode = out_mode_map[out_type];*/
	io_mask = out_mask_map[out_type];

	handler = rtx->rio.handler[io_type];
	if (HAWK_UNLIKELY(!handler))
	{
		/* no io handler provided */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIOUSER);
		return -1;
	}

	while (p)
	{
		if (p->type == (io_type | io_mask) && hawk_comp_oocstr(p->name, name, 0) == 0)
		{
			hawk_rio_impl_t handler;

			handler = rtx->rio.handler[p->type & IO_MASK_CLEAR];
			if (handler && handler(rtx, HAWK_RIO_CMD_CLOSE, p, HAWK_NULL, 0) <= -1) return -1;

			if (px) px->next = p->next;
			else rtx->rio.chain = p->next;

			hawk_rtx_freemem (rtx, p->name);
			hawk_rtx_freemem (rtx, p);
			return 0;
		}

		px = p;
		p = p->next;
	}

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIONMNF);
	return -1;
}

int hawk_rtx_closeio (hawk_rtx_t* rtx, const hawk_ooch_t* name, const hawk_ooch_t* opt)
{
	hawk_rio_arg_t* p = rtx->rio.chain, * px = HAWK_NULL;

	while (p)
	{
		 /* it handles the first that matches the given name
		  * regardless of the io type */
		if (hawk_comp_oocstr(p->name, name, 0) == 0)
		{
			hawk_rio_impl_t handler;
			hawk_rio_rwcmode_t rwcmode = HAWK_RIO_CMD_CLOSE_FULL;

			if (opt)
			{
				if (opt[0] == HAWK_T('r'))
				{
					if (p->type & IO_MASK_RDWR)
					{
						if (p->rwcstate != HAWK_RIO_CMD_CLOSE_WRITE)
						{
							/* if the write end is not
							 * closed, let io handler close
							 * the read end only. */
							rwcmode = HAWK_RIO_CMD_CLOSE_READ;
						}
					}
					else if (!(p->type & IO_MASK_READ)) goto skip;
				}
				else
				{
					HAWK_ASSERT (opt[0] == HAWK_T('w'));
					if (p->type & IO_MASK_RDWR)
					{
						if (p->rwcstate != HAWK_RIO_CMD_CLOSE_READ)
						{
							/* if the read end is not
							 * closed, let io handler close
							 * the write end only. */
							rwcmode = HAWK_RIO_CMD_CLOSE_WRITE;
						}
					}
					else if (!(p->type & IO_MASK_WRITE)) goto skip;
				}
			}

			handler = rtx->rio.handler[p->type & IO_MASK_CLEAR];
			if (handler)
			{
				p->rwcmode = rwcmode;
				if (handler(rtx, HAWK_RIO_CMD_CLOSE, p, HAWK_NULL, 0) <= -1)
				{
					/* this is not a run-time error.*/
					return -1;
				}
			}

			if (p->type & IO_MASK_RDWR)
			{
				p->rwcmode = rwcmode;
				if (p->rwcstate == 0 && rwcmode != 0)
				{
					/* if either end has not been closed.
					 * return success without destroying
					 * the internal node. rwcstate keeps
					 * what has been successfully closed */
					p->rwcstate = rwcmode;
					return 0;
				}
			}

			if (px) px->next = p->next;
			else rtx->rio.chain = p->next;

			hawk_rtx_freemem (rtx, p->name);
			hawk_rtx_freemem (rtx, p);

			return 0;
		}

	skip:
		px = p;
		p = p->next;
	}

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EIONMNF);
	return -1;
}

void hawk_rtx_flushallios (hawk_rtx_t* rtx)
{
	hawk_rio_arg_t* rio;
	hawk_rio_impl_t handler;

	for (rio = rtx->rio.chain; rio; rio = rio->next)
	{
		handler = rtx->rio.handler[rio->type & IO_MASK_CLEAR];
		if (handler)
		{
			handler (rtx, HAWK_RIO_CMD_FLUSH, rio, HAWK_NULL, 0);
		}
	}
}

void hawk_rtx_clearallios (hawk_rtx_t* rtx)
{
	hawk_rio_arg_t* next;
	hawk_rio_impl_t handler;
	hawk_ooi_t n;

	while (rtx->rio.chain)
	{
		handler = rtx->rio.handler[rtx->rio.chain->type & IO_MASK_CLEAR];
		next = rtx->rio.chain->next;

		if (handler)
		{
			rtx->rio.chain->rwcmode = 0;
			n = handler(rtx, HAWK_RIO_CMD_CLOSE, rtx->rio.chain, HAWK_NULL, 0);
			if (n <= -1)
			{
				/* TODO: some warnings need to be shown??? */
			}
		}

		hawk_rtx_freemem (rtx, rtx->rio.chain->name);
		hawk_rtx_freemem (rtx, rtx->rio.chain);

		rtx->rio.chain = next;
	}
}
