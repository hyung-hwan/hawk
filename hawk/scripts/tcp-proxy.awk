@pragma entry main
@pragma implicit off

function break_bridge_by_local_fd (&localtab, &remotetab, fd, mx)
{
	@local r;
	r = localtab[fd];
	sys::delfrommux (mx, fd);
	sys::delfrommux (mx, r);
	sys::close (fd);
	sys::close (r);
	delete localtab[fd];
	delete localtab[fd,"pending"];
	delete remotetab[r];
	delete remotetab[r,"connecting"];
	delete remotetab[r,"pending"];
}

function break_bridge_by_remote_fd (localtab, remotetab, fd, mx)
{
	@local l;
	l = remotetab[fd];
	sys::delfrommux (mx, l);
	sys::delfrommux (mx, fd);
	sys::close (l);
	sys::close (fd);
	delete localtab[l];
	delete localtab[l,"pending"];
	delete remotetab[fd];
	delete remotetab[fd,"connecting"];
	delete remotetab[fd,"pending"];
}

function bridge_remote_to_local (localtab, remotetab, fd, mx)
{
	@local len, buf;

	len = sys::read(fd, buf, 8096);
	if (len <= 0)
	{
		print "closing read error ", fd;
		break_bridge_by_remote_fd (localtab, remotetab, fd, mx);
	}
	else
	{
		do
		{
			@local x;

			x = sys::write(remotetab[fd], buf);
			if (x == sys::RC_EAGAIN)
			{
				remotetab[fd,"pending"] = remotetab[fd,"pending"] %% buf;
				break;
			}
			else if (x <= -1)
			{
				break_bridge_by_remote_fd (localtab, remotetab, fd, mx);
				break;
			}

			len -= x;
			if (len <= 0) break;

			buf = substr(buf, x + 1);
		}
		while (1);
	}
}

function bridge_local_to_remote (localtab, remotetab, fd, mx)
{
	@local len, buf;

	len = sys::read(fd, buf, 8096);
	if (len <= 0)
	{
		print "closing read error ", fd;
		break_bridge_by_local_fd (localtab, remotetab, fd, mx);
	}
	else
	{
printf ("ABOUT TO WRITE TO REMOTE........\n");
		do
		{
			@local x;

			x = sys::write(localtab[fd], buf);
			if (x == sys::RC_EAGAIN)
			{
				localtab[fd,"pending"] = localtab[fd,"pending"] %% buf;
				## TODO: MOD mux for writing..
				break;
			}
			else if (x <= -1)
			{
				break_bridge_by_local_fd (localtab, remotetab, fd, mx);
				break;
			}

			len -= x;
			if (len <= 0) break;

			buf = substr(buf, x + 1);
		}
		while (1);
	}
}

function serve_connections (mx, ss, remoteaddr)
{
	@local localtab, remotetab

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
					r = sys::socket(sys::AF_INET6, sys::SOCK_STREAM | sys::SOCK_CLOEXEC | sys::SOCK_NONBLOCK, 0);
					if (r <= -1)
					{
						print "Error: unable to create remote socket for local socket", l, "-", sys::errmsg();
						sys::close(l);
					}
					else 
					{
						@local x;
						x = sys::connect(r, remoteaddr);
						if ((x <= -1 && x != sys::RC_EINPROG) || sys::addtomux(mx, r, sys::MUX_EVT_OUT) <= -1)
						{
print "Unable to conneect to remote...", sys::errmsg();
							sys::close (r);
							sys::close (l);
						}
						else
						{
							localtab[l] = r;
							remotetab[r] = l;
							remotetab[r,"connecting"] = 1;
printf ("NEW SESSION %d %d\n", l, r);
						}
					}
				}
			}
			else
			{
				## event on a client socket
				if (fd in remotetab)
				{
					if (evmask & (sys::MUX_EVT_HUP | sys::MUX_EVT_ERR))
					{
						print "closing HUP ERR", fd;
						break_bridge_by_remote_fd (localtab, remotetab, fd, mx);
					}
					else
					{
						if (evmask & sys::MUX_EVT_OUT)
						{
							if ((fd,"connecting") in remotetab)
							{
								## remote connected
								sys::addtomux (mx, remotetab[fd], sys::MUX_EVT_IN);
								sys::modinmux (mx, fd, sys::MUX_EVT_IN);
								delete remotetab[fd,"connecting"];
							}
						}

						if (evmask & sys::MUX_EVT_IN)
						{
							bridge_remote_to_local (localtab, remotetab, fd, mx);
						}
					}
				}

				if (fd in localtab)
				{
					if (evmask & (sys::MUX_EVT_HUP | sys::MUX_EVT_ERR))
					{
						print "closing HUP ERR local", fd;
						break_bridge_by_local_fd (localtab, remotetab, fd, mx);
					}
					else if (evmask & sys::MUX_EVT_IN)
					{
						print "event on " fd;
						bridge_local_to_remote (localtab, remotetab, fd, mx);
					}
				}
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
		printf ("Usage: %s local-address remote-address\n", ARGV[0]); ## TODO: add SCRIPTNAME to hawk interpreter
		return -1;
	}

	mx = sys::openmux();
	if (mx <= -1)
	{
		print "Error: unable to create multiplexer -", sys::errmsg();
		return -1;
	}

	ss = sys::socket(sys::AF_INET6, sys::SOCK_STREAM | sys::SOCK_CLOEXEC | sys::SOCK_NONBLOCK, 0);
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
