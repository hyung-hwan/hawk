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
#include "hawk-mus.h"
#include <hawk-ecs.h>
#include <hawk-sio.h>

typedef struct std_context_t std_context_t;
struct std_context_t
{
	const hawk_mus_iostd_t* input;
	hawk_mus_iostd_t* output;
	hawk_ooecs_t buffer;
};

typedef struct stream_t stream_t;
struct stream_t
{
	hawk_sio_t* sio;
	hawk_ooch_t* path;
};

static int is_stdio (const hawk_ooch_t* path)
{
	return !path || (path[0] == '-' && path[1] == 0);
}

static int is_absolute (const hawk_ooch_t* path)
{
	if (HAWK_IS_PATH_SEP(path[0])) return 1;
#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
	if (HAWK_IS_PATH_DRIVE(path)) return 1;
#endif
	return 0;
}

static int try_partial (hawk_mus_t* mus, stream_t* stream, const hawk_ooch_t* dir, hawk_oow_t dlen, const hawk_ooch_t* name)
{
	hawk_ooecs_t path;
	int pass;
	int result = -1;

	if (hawk_ooecs_init(&path, hawk_mus_getgem(mus), 0) <= -1) return -1;
	if (hawk_ooecs_ncat(&path, dir, dlen) == (hawk_oow_t)-1) goto done;
	if (dlen && !HAWK_IS_PATH_SEP(dir[dlen - 1]) && hawk_ooecs_cat(&path, HAWK_T("/")) == (hawk_oow_t)-1) goto done;
	if (hawk_ooecs_cat(&path, name) == (hawk_oow_t)-1) goto done;
	for (pass = 0; pass < 2; pass++)
	{
		stream->sio = hawk_sio_open(hawk_mus_getgem(mus), 0, HAWK_OOECS_PTR(&path), HAWK_SIO_READ);
		if (stream->sio)
		{
			stream->path = hawk_gem_dupoocstr(hawk_mus_getgem(mus), HAWK_OOECS_PTR(&path), HAWK_NULL);
			if (!stream->path)
			{
				hawk_sio_close(stream->sio);
				stream->sio = HAWK_NULL;
				goto done;
			}
			result = 1;
			goto done;
		}
		/* An absent partial is empty, but access/encoding/resource failures
		 * must not silently select another file or become empty output. */
		if (hawk_mus_geterrnum(mus) != HAWK_ENOENT) goto done;
		if (pass == 0 && hawk_ooecs_cat(&path, HAWK_T(".mustache")) == (hawk_oow_t)-1) goto done;
	}
	hawk_mus_seterrnum(mus, HAWK_NULL, HAWK_ENOERR);
	result = 0;
done:
	hawk_ooecs_fini(&path);
	return result;
}

static int open_partial (hawk_mus_t* mus, stream_t* stream, hawk_mus_io_arg_t* arg)
{
	const hawk_ooch_t* dirs;
	const hawk_ooch_t* sep;
	hawk_oow_t len, i;
	int n;

	if (is_absolute(arg->name)) return try_partial(mus, stream, HAWK_T(""), 0, arg->name);
	len = arg->parent? hawk_count_oocstr(arg->parent): 0;
	for (i = len; i > 0 && !HAWK_IS_PATH_SEP(arg->parent[i - 1]); i--);
	n = try_partial(mus, stream, i? arg->parent: HAWK_T(""), i, arg->name);
	if (n != 0) return n;
	if (hawk_mus_getopt(mus, HAWK_MUS_OPT_PARTIALDIRS, &dirs) <= -1) return -1;
	if (!dirs || !dirs[0]) return 0;
	while (1)
	{
		sep = hawk_find_oochar_in_oocstr(dirs, HAWK_DFL_PATH_LIST_SEP);
		len = sep? (hawk_oow_t)(sep - dirs): hawk_count_oocstr(dirs);
		n = try_partial(mus, stream, dirs, len, arg->name);
		if (n != 0 || !sep) return n;
		dirs = sep + 1;
	}
}

static hawk_ooi_t std_io (hawk_mus_t* mus, hawk_mus_io_cmd_t cmd, hawk_mus_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count, void* ctx)
{
	std_context_t* std = ctx;
	stream_t* stream;
	const hawk_ooch_t* path;
	hawk_cmgr_t* cmgr;
	hawk_ooi_t n;
	int status;

	switch (cmd)
	{
		case HAWK_MUS_IO_OPEN:
			if (arg->kind == HAWK_MUS_IO_OUTPUT && std->output && std->output->type == HAWK_MUS_IOSTD_OOCS)
			{
				arg->handle = std;
				return 1;
			}
			stream = hawk_gem_callocmem(hawk_mus_getgem(mus), HAWK_SIZEOF(*stream));
			if (!stream) return -1;
			cmgr = hawk_mus_getcmgr(mus);
			if (arg->kind == HAWK_MUS_IO_PARTIAL)
			{
				status = open_partial(mus, stream, arg);
				if (status <= 0)
				{
					hawk_mus_freemem(mus, stream);
					return status;
				}
			}
			else
			{
				path = arg->name;
				if (arg->kind == HAWK_MUS_IO_OUTPUT)
				{
					path = std->output? std->output->u.file.path: HAWK_NULL;
					if (std->output && std->output->u.file.cmgr) cmgr = std->output->u.file.cmgr;
				}
				else if (std->input && std->input->u.file.cmgr) cmgr = std->input->u.file.cmgr;
				if (is_stdio(path))
					stream->sio = hawk_sio_openstd(hawk_mus_getgem(mus), 0,
						arg->kind == HAWK_MUS_IO_OUTPUT? HAWK_SIO_STDOUT: HAWK_SIO_STDIN,
						arg->kind == HAWK_MUS_IO_OUTPUT? HAWK_SIO_WRITE: HAWK_SIO_READ);
				else
				{
					stream->sio = hawk_sio_open(hawk_mus_getgem(mus), 0, path,
						arg->kind == HAWK_MUS_IO_OUTPUT? HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE: HAWK_SIO_READ);
					if (stream->sio) stream->path = hawk_gem_dupoocstr(hawk_mus_getgem(mus), path, HAWK_NULL);
					if (stream->sio && !stream->path)
					{
						hawk_sio_close(stream->sio);
						stream->sio = HAWK_NULL;
					}
				}
				if (!stream->sio)
				{
					hawk_mus_freemem(mus, stream);
					return -1;
				}
			}
			hawk_sio_setcmgr(stream->sio, cmgr);
			arg->handle = stream;
			arg->path = stream->path;
			return 1;

		case HAWK_MUS_IO_CLOSE:
			if (arg->handle == std)
			{
				arg->handle = HAWK_NULL;
				return 0;
			}
			stream = arg->handle;
			n = 0;
			if (arg->kind == HAWK_MUS_IO_OUTPUT && hawk_sio_flush(stream->sio) <= -1) n = -1;
			hawk_sio_close(stream->sio);
			if (stream->path) hawk_mus_freemem(mus, stream->path);
			hawk_mus_freemem(mus, stream);
			arg->handle = HAWK_NULL;
			return n;

		case HAWK_MUS_IO_READ:
			stream = arg->handle;
			return hawk_sio_getoochars(stream->sio, data, count);

		case HAWK_MUS_IO_WRITE:
			if (arg->handle == std)
				return hawk_ooecs_ncat(&std->buffer, data, count) == (hawk_oow_t)-1? -1: (hawk_ooi_t)count;
			stream = arg->handle;
			return hawk_sio_putoochars(stream->sio, data, count);
	}
	hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EINVAL, "invalid mustache I/O operation");
	return -1;
}

hawk_mus_t* hawk_mus_openstd (hawk_oow_t xtnsize, hawk_errinf_t* errinf)
{
	return hawk_mus_openstdwithmmgr(HAWK_NULL, xtnsize, HAWK_NULL, errinf);
}

hawk_mus_t* hawk_mus_openstdwithmmgr (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	if (!mmgr) mmgr = hawk_get_sys_mmgr();
	if (!cmgr) cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
	return hawk_mus_open(mmgr, xtnsize, cmgr, errinf);
}

int hawk_mus_compilestd (hawk_mus_t* mus, const hawk_mus_iostd_t* input)
{
	std_context_t ctx;

	if (!input)
	{
		HAWK_MEMSET(&ctx, 0, HAWK_SIZEOF(ctx));
		return hawk_mus_compile(mus, HAWK_NULL, std_io, &ctx);
	}
	if (input->type == HAWK_MUS_IOSTD_OOCS)
		return hawk_mus_compilewithoochars(mus, input->u.oocs.ptr, input->u.oocs.len);
	if (input->type != HAWK_MUS_IOSTD_FILE)
	{
		hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EINVAL, "invalid mustache input type");
		return -1;
	}
	HAWK_MEMSET(&ctx, 0, HAWK_SIZEOF(ctx));
	ctx.input = input;
	return hawk_mus_compile(mus, input->u.file.path, std_io, &ctx);
}

int hawk_mus_renderstd (hawk_mus_t* mus, void* root, const hawk_mus_data_t* data, void* data_ctx, hawk_mus_iostd_t* output)
{
	std_context_t ctx;
	hawk_oocs_t result;
	int n;

	if (output && output->type != HAWK_MUS_IOSTD_FILE && output->type != HAWK_MUS_IOSTD_OOCS)
	{
		hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EINVAL, "invalid mustache output type");
		return -1;
	}
	HAWK_MEMSET(&ctx, 0, HAWK_SIZEOF(ctx));
	ctx.output = output;
	if (hawk_ooecs_init(&ctx.buffer, hawk_mus_getgem(mus),
	    output && output->type == HAWK_MUS_IOSTD_OOCS? 1: 0) <= -1) return -1;
	n = hawk_mus_render(mus, root, data, data_ctx, std_io, &ctx);
	if (n == 0 && output && output->type == HAWK_MUS_IOSTD_OOCS)
	{
		if (hawk_ooecs_yield(&ctx.buffer, &result, 0) <= -1) n = -1;
		else output->u.oocs = result;
	}
	hawk_ooecs_fini(&ctx.buffer);
	return n;
}
