@pragma entry main
@pragma implicit off

function destroy_bridge (&brtab, fd, mx)
{
	@local pfd; 

	pfd = brtab[fd];

	print "Info: Destroying", fd, pfd;

	sys::delfrommux (mx, fd);
	sys::delfrommux (mx, pfd);
	sys::close (fd);
	sys::close (pfd);

	delete brtab[fd];
	delete brtab[fd,"connecting"];
	delete brtab[fd,"pending"];
	delete brtab[fd,"evmask"];
	delete brtab[fd,"eof"];

	delete brtab[pfd];
	delete brtab[pfd,"connecting"];
	delete brtab[pfd,"pending"];
	delete brtab[pfd,"evmask"];
	delete brtab[pfd,"eof"];
}

function prepare_bridge (&brtab, fd, pfd, mx)
{
	if (sys::addtomux(mx, pfd, sys::MUX_EVT_OUT) <= -1) return -1;

	print "Info: Preparing", fd, pfd;

	brtab[fd] = pfd;
	brtab[fd,"evmask"] = 0;

	brtab[pfd] = fd;
	brtab[pfd,"connecting"] = 1;
	brtab[pfd,"evmask"] = sys::MUX_EVT_OUT;
	
	return 0;
}

function establish_bridge (&brtab, fd, mx)
{
	@local pfd;

	pfd = brtab[fd];
	if (sys::addtomux(mx, pfd, sys::MUX_EVT_IN) <= -1 ||
	    sys::modinmux(mx, fd, sys::MUX_EVT_IN) <= -1) return -1;

	brtab[fd,"evmask"] = sys::MUX_EVT_IN;
	brtab[pfd,"evmask"] = sys::MUX_EVT_IN;

	delete brtab[fd,"connecting"];
	return 0;
}

function handle_bridge_eof (&brtab, fd, mx)
{
	@local pfd, evmask;

	pfd = brtab[fd];

	evmask = brtab[fd,"evmask"] & ~~sys::MUX_EVT_IN;
	if (sys::modinmux(mx, fd, evmask) <= -1) return -1;
	brtab[fd,"evmask"] = evmask;
	brtab[fd,"eof"] = 1;

	## EOF on fd, disable transmission on pfd if no pending
	if (length(brtab[pfd,"pending"]) == 0) sys::shutdown (pfd, sys::SHUT_WR);

	# if peer fd reached eof and there are no pending data, arrange to abort the bridge
	if (brtab[pfd,"eof"] != 0 && length(brtab[pfd,"pending"]) == 0 && length(brtab[fd,"pending"]) == 0)	return -1;

	return 0;
}

function bridge_traffic (&brtab, fd, mx)
{
	@local len, buf, pfd, pos, x, evmask;

	pfd = brtab[fd];

	len = sys::read(fd, buf, 8096); 
	if (len <= -1)
	{
		return -1;
	}
	else if (len == 0)
	{
		return handle_bridge_eof (brtab, fd, mx);
	}
	
	pos = 1;
	while (pos <= len)
	{
		x = sys::write(pfd, buf, pos);
		if (x == sys::RC_EAGAIN)
		{
			evmask = brtab[fd,"evmask"] & ~~sys::MUX_EVT_IN;
			if (sys::modinmux(mx, fd, evmask) <= -1) return -1;
			brtab[fd,"evmask"] = evmask;

			evmask = brtab[pfd,"evmask"] | sys::MUX_EVT_OUT;
			if (sys::modinmux(mx, pfd, evmask) <= -1) return -1;
			brtab[pfd,"evmask"] = evmask;

			brtab[pfd,"pending"] %%= substr(buf, pos);
			break;
		}
		else if (x <= -1)
		{
			return -1;
		}

		pos += x;
	}

	return 0;
}

function bridge_pendind_data (&brtab, fd, mx)
{
	@local pending_data, pos, len, x, pfd, evmask;

	pfd = brtab[fd];
	pending_data = brtab[fd,"pending"];

	len = length(pending_data);
	pos = 1;
	while (pos <= len)
	{
		x = sys::write(fd, pending_data, pos);
		if (x == sys::RC_EAGAIN)
		{
			brtab[fd,"pending"] = substr(pending_data, pos);
			return 0;
		}
		pos += x;
	}

	## sent all pending data. 
	delete brtab[fd,"pending"];

	evmask = brtab[fd,"evmask"] & ~~sys::MUX_EVT_OUT;
	if (sys::modinmux(mx, fd, evmask) <= -1) return -1;
	brtab[fd,"evmask"] = evmask;

	if (brtab[pfd,"eof"] == 0)
	{
		evmask = brtab[pfd,"evmask"] | sys::MUX_EVT_IN;
		if (sys::modinmux(mx, pfd, evmask) <= -1) return -1;
		brtab[pfd,"evmask"] = evmask;
	}

	return 0;
}

function handle_bridge_event (&brtab, fd, mx, evmask)
{
	if (evmask & sys::MUX_EVT_ERR)
	{
		destroy_bridge (brtab, fd, mx);
	}
	else if (evmask & sys::MUX_EVT_HUP)
	{
		print "Info: HUP detected on", fd;
		if (handle_bridge_eof(brtab, fd, mx) <= -1)
		{
			destroy_bridge (brtab, fd, mx);
		}
	}
	else
	{
		if (evmask & sys::MUX_EVT_OUT)
		{
			if ((fd,"connecting") in brtab)
			{
				## remote peer connection
				if (establish_bridge(brtab, fd, mx) <= -1) 
				{
					destroy_bridge (brtab, fd, mx);
					return;
				}
			}
			else if ((fd,"pending") in brtab) 
			{
				if (bridge_pending_data(brtab, fd, mx) <= -1) 
				{
					destroy_bridge (brtab, fd, mx);
					return;
				}
			}
		}

		if (evmask & sys::MUX_EVT_IN)
		{
			if (bridge_traffic(brtab, fd, mx) <= -1) 
			{
				destroy_bridge (brtab, fd, mx);
				return;
			}
		}
	}
}

function serve_connections (mx, ss, remoteaddr)
{
	@local brtab, remotetab

	while (1)
	{
		@local x, i;

		if ((x = sys::waitonmux(mx, 3.10)) <= -1)
		{
			print "Error: problem while waiting on multiplexer -", sys::errmsg();
			break;
		}

		if (x == 0) continue; ## timed out


		for (i = 0; i < x; i++)
		{
			@local fd, evmask;

			if (sys::getmuxevt(mx, i, fd, evmask) <= -1) continue;

			if (fd == ss)
			{
				## server socket event
				@local l, r;

				l = sys::accept(ss, sys::SOCK_CLOEXEC | sys::SOCK_NONBLOCK);
				if (l <= -1)
				{
					print "Error: failed to accept connection -", sys::errmsg();
				}
				else
				{
					r = sys::socket(sys::sockaddrdom(remoteaddr), sys::SOCK_STREAM | sys::SOCK_CLOEXEC | sys::SOCK_NONBLOCK, 0);
					if (r <= -1)
					{
						print "Error: unable to create remote socket for local socket", l, "-", sys::errmsg();
						sys::close(l);
					}
					else 
					{
						@local rc;
						rc = sys::connect(r, remoteaddr);
						if ((rc <= -1 && rc != sys::RC_EINPROG) || prepare_bridge(brtab, l, r, mx) <= -1)
						{
							print "Error: unable to conneect to", remoteaddr, "-", sys::errmsg();
							sys::close (r);
							sys::close (l);
						}
					}
				}
			}
			else 
			{
				if (fd in brtab) handle_bridge_event (brtab, fd, mx, evmask);
			}
			
		}
	}

	return 0;
}

function main (localaddr, remoteaddr, c)
{
	@local ss, mx;

	if (ARGC != 3)
	{
		###printf ("Usage: %s -f %s local-address remote-address\n", ARGV[0], SCRIPTNAME);
		printf ("Usage: %s local-address remote-address\n", SCRIPTNAME);
		return -1;
	}

	mx = sys::openmux();
	if (mx <= -1)
	{
		print "Error: unable to create multiplexer -", sys::errmsg();
		return -1;
	}

	ss = sys::socket(sys::sockaddrdom(localaddr), sys::SOCK_STREAM | sys::SOCK_CLOEXEC | sys::SOCK_NONBLOCK, 0);
	if (ss <= -1)
	{
		print "Error: unable to create socket -", sys::errmsg();
		return -1;
	}

	sys::setsockopt(ss, sys::SOL_SOCKET, sys::SO_REUSEADDR, 1);
	sys::setsockopt(ss, sys::SOL_SOCKET, sys::SO_REUSEPORT, 1);

	if (sys::bind(ss, localaddr) <= -1 || sys::listen(ss, 0) <= -1)
	{
		print "Error: socket error -", sys::errmsg();
		sys::close (ss);
		return -1;
	}

	if (sys::addtomux(mx, ss, sys::MUX_EVT_IN) <= -1)
	{
		print "Error: unable to add server socket to multiplexer -", sys::errmsg();
		sys::close (ss);
		return -1;
	}

	serve_connections (mx, ss, remoteaddr);

	sys::delfrommux (mx, ss);
	sys::close (ss);
	sys::close (mx);
}
