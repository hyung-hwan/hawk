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

#include "main.h"

int main(int argc, hawk_bch_t* argv[])
{
	if (argc >= 2 && hawk_comp_bcstr(argv[1], "--sed", 0) == 0)
	{
		/* hawk --sed ... */
		return main_sed(argc - 1, &argv[1], argv[0]);
	}
	else if (argc >= 2 && hawk_comp_bcstr(argv[1], "--awk", 0) == 0)
	{
		/* hawk --awk ... */
		/* in this mode, the value ARGV[0] inside a hawk script is "--awk" */
		return main_hawk(argc - 1, &argv[1], argv[0]);
	}

	return main_hawk(argc, argv, HAWK_NULL);
}

/* ---------------------------------------------------------------------- */

#if defined(FAKE_SOCKET)
socket () {}
listen () {}
accept () {}
recvfrom () {}
connect () {}
getsockopt () {}
recv      () {}
setsockopt () {}
send      () {}
bind     () {}
shutdown  () {}

void* memmove (void* x, void* y, size_t z) {}
#endif
