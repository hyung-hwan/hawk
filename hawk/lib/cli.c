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

#include <hawk-cli.h>
#include <hawk-utl.h>

#define BADCH   '?'
#define BADARG  ':'

static hawk_uch_t EMSG_UCH[] = { '\0' };
static hawk_bch_t EMSG_BCH[] = { '\0' };

/* ------------------------------------------------------------ */

#undef XEMSG
#undef xch_t
#undef xci_t
#undef xcli_t
#undef xcli_lng_t
#undef xgetcli
#undef xcompcharscstr
#undef xfindcharincstr
#undef XCI_EOF

#define XEMSG EMSG_UCH
#define xch_t hawk_uch_t
#define xci_t hawk_uci_t
#define xcli_t hawk_ucli_t
#define xcli_lng_t hawk_ucli_lng_t
#define xgetcli hawk_get_ucli
#define xcompcharscstr hawk_comp_uchars_ucstr
#define xfindcharincstr hawk_find_uchar_in_ucstr
#define XCI_EOF HAWK_BCI_EOF
#include "cli-imp.h"

/* ------------------------------------------------------------ */

#undef XEMSG
#undef xch_t
#undef xci_t
#undef xcli_t
#undef xcli_lng_t
#undef xgetcli
#undef xcompcharscstr
#undef xfindcharincstr
#undef XCI_EOF

#define XEMSG EMSG_BCH
#define xch_t hawk_bch_t
#define xci_t hawk_bci_t
#define xcli_t hawk_bcli_t
#define xcli_lng_t hawk_bcli_lng_t
#define xgetcli hawk_get_bcli
#define xcompcharscstr hawk_comp_bchars_bcstr
#define xfindcharincstr hawk_find_bchar_in_bcstr
#define XCI_EOF HAWK_UCI_EOF
#include "cli-imp.h"

/* ------------------------------------------------------------ */
