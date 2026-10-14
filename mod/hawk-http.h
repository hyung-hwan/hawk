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

#ifndef _HAWK_HTTP_H_
#define _HAWK_HTTP_H_

#include <hawk-aux.h>

#define HAWK_HTTP_AUX_NAME HAWK_T("http")

typedef enum hawk_http_cmd_t hawk_http_cmd_t;
enum hawk_http_cmd_t
{
	HAWK_HTTP_GETMETHOD,
	HAWK_HTTP_GETPATH,
	HAWK_HTTP_GETHEADER,
	HAWK_HTTP_GETHEADERS,
	HAWK_HTTP_SETSTATUS,
	HAWK_HTTP_GETSTATUS,
	HAWK_HTTP_SETHEADER,
	HAWK_HTTP_ADDHEADER,
	HAWK_HTTP_DELHEADER,
	HAWK_HTTP_FLUSH
};

typedef struct hawk_http_arg_t hawk_http_arg_t;
struct hawk_http_arg_t
{
	hawk_oocs_t name;  /**< Borrowed header name, valid during the callback. */
	hawk_oocs_t value; /**< Borrowed header value, valid during the callback. */
	hawk_oow_t index;  /**< Header occurrence starting at 1. */
	int status;       /**< Response status in 200..599. */
};

/* Bind a hawk_aux_cbs_t with hawk_rtx_getauxid(rtx, HAWK_HTTP_AUX_NAME,
 * &id) and hawk_rtx_setauxctx(rtx, id, &callbacks, context). Its impl receives
 * a hawk_http_cmd_t command and a non-NULL hawk_http_arg_t data pointer,
 * borrowed for the duration of the call. Return 0 on success or -1 with a
 * runtime diagnostic on failure. GET commands set the intrinsic return value
 * with hawk_rtx_setretval(); GETHEADERS returns a map of arrays indexed from 1,
 * and a missing GETHEADER returns an empty string.
 *
 * The host owns response commitment state. A successful FLUSH commits headers;
 * status/header mutations must then fail with HAWK_EPERM. Also mark commitment
 * when headers are sent by console output or a partially failing flush. GET
 * commands and repeated FLUSH remain available after commitment. Bodies use
 * the host's console I/O callbacks, independently of this metadata protocol.
 *
 * This header declares only the HTTP module's aux protocol, with no module
 * helper symbols to link. It can be used by C CGI/FastCGI hosts and Go hosts,
 * whether the module is linked statically or loaded dynamically. IDs are
 * local to a runtime; resolve this name separately for each runtime. */

#endif
