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

#ifndef _HAWK_STDAWK_HPP_
#define _HAWK_STDAWK_HPP_

#include <Hawk.hpp>
#include <StdMmgr.hpp>

/// \file
/// Standard AWK Interpreter

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
////////////////////////////////

///
/// The StdHawk class provides an easier-to-use interface by overriding 
/// primitive methods, and implementing the file handler, the pipe handler, 
/// and common intrinsic functions.
///
class HAWK_EXPORT StdHawk: public Hawk
{
public:
	///
	/// The SourceFile class implements script I/O from and to a file.
	///
	class HAWK_EXPORT SourceFile: public Source 
	{
	public:
		SourceFile (const char_t* name, hawk_cmgr_t* cmgr = HAWK_NULL): 
			name (name), cmgr (cmgr)
		{
			dir.ptr = HAWK_NULL; dir.len = 0; 
		}

		int open (Data& io);
		int close (Data& io);
		ssize_t read (Data& io, char_t* buf, size_t len);
		ssize_t write (Data& io, const char_t* buf, size_t len);

	protected:
		const char_t* name;
		hawk_oocs_t dir;
		hawk_cmgr_t* cmgr;
	};

	///
	/// The SourceString class implements script input from a string. 
	/// Deparsing is not supported.
	///
	class HAWK_EXPORT SourceString: public Source
	{
	public:
		SourceString (const char_t* str): str (str) {}

		int open (Data& io);
		int close (Data& io);
		ssize_t read (Data& io, char_t* buf, size_t len);
		ssize_t write (Data& io, const char_t* buf, size_t len);

	protected:
		const char_t* str;
		const char_t* ptr;
	};

	StdHawk (Mmgr* mmgr = HAWK_NULL): Hawk(mmgr), stdmod_up(false), console_cmgr(HAWK_NULL) 
	{
	}

	int open ();
	void close ();

	void uponClosing ();

	Run* parse (Source& in, Source& out);

	/// The setConsoleCmgr() function sets the encoding type of 
	/// the console streams. They include both the input and the output
	/// streams. It provides no way to specify a different encoding
	/// type for the input and the output stream.
	void setConsoleCmgr (const hawk_cmgr_t* cmgr);

	/// The getConsoleCmgr() function returns the current encoding
	/// type set for the console streams.
	const hawk_cmgr_t* getConsoleCmgr () const;

	/// The addConsoleOutput() function adds a file to form an
	/// output console stream.
	int addConsoleOutput (const char_t* arg, size_t len);
	int addConsoleOutput (const char_t* arg);

	void clearConsoleOutputs ();

protected:
	int make_additional_globals (Run* run);
	int build_argcv (Run* run);
	int build_environ (Run* run);
	int __build_environ (Run* run, void* envptr);

	// intrinsic functions 
	hawk_cmgr_t* getiocmgr (const char_t* ioname);

	int setioattr (Run& run, Value& ret, Value* args, size_t nargs, const char_t* name, size_t len);
	int getioattr (Run& run, Value& ret, Value* args, size_t nargs, const char_t* name, size_t len);

	// pipe io handlers 
	int openPipe (Pipe& io);
	int closePipe (Pipe& io);
	ssize_t readPipe  (Pipe& io, char_t* buf, size_t len);
	ssize_t writePipe (Pipe& io, const char_t* buf, size_t len);
	ssize_t writePipeBytes (Pipe& io, const hawk_bch_t* buf, size_t len);
	int flushPipe (Pipe& io);

	// file io handlers 
	int openFile (File& io);
	int closeFile (File& io);
	ssize_t readFile (File& io, char_t* buf, size_t len);
	ssize_t writeFile (File& io, const char_t* buf, size_t len);
	ssize_t writeFileBytes (File& io, const hawk_bch_t* buf, size_t len);
	int flushFile (File& io);

	// console io handlers 
	int openConsole (Console& io);
	int closeConsole (Console& io);
	ssize_t readConsole (Console& io, char_t* buf, size_t len);
	ssize_t writeConsole (Console& io, const char_t* buf, size_t len);
	ssize_t writeConsoleBytes (Console& io, const hawk_bch_t* buf, size_t len);
	int flushConsole (Console& io);
	int nextConsole (Console& io);

	// primitive handlers 
	void* allocMem   (size_t n);
	void* reallocMem (void* ptr, size_t n);
	void  freeMem    (void* ptr);

	flt_t pow (flt_t x, flt_t y);
	flt_t mod (flt_t x, flt_t y);

	void* modopen (const mod_spec_t* spec);
	void  modclose (void* handle);
	void* modsym (void* handle, const char_t* name);

protected:
	hawk_htb_t cmgrtab;
	bool cmgrtab_inited;
	bool stdmod_up;

	hawk_cmgr_t* console_cmgr;

	// global variables 
	int gbl_argc;
	int gbl_argv;
	int gbl_environ;

	// standard input console - reuse runarg 
	size_t runarg_index;
	size_t runarg_count;

	// standard output console 
	xstrs_t ofile;
	size_t ofile_index;
	size_t ofile_count;

public:
	struct ioattr_t
	{
		hawk_cmgr_t* cmgr;
		char_t cmgr_name[64]; // i assume that the cmgr name never exceeds this length.
		hawk_ntime_t tmout[4];

		ioattr_t (): cmgr (HAWK_NULL)
		{
			this->cmgr_name[0] = HAWK_T('\0');
			for (size_t i = 0; i < HAWK_COUNTOF(this->tmout); i++)
			{
				this->tmout[i].sec = -999;
				this->tmout[i].nsec = 0;
			}
		}
	};

	static ioattr_t default_ioattr;

protected:
	ioattr_t* get_ioattr (const char_t* ptr, size_t len);
	ioattr_t* find_or_make_ioattr (const char_t* ptr, size_t len);


private:
	int open_console_in (Console& io);
	int open_console_out (Console& io);

	int open_pio (Pipe& io);
	int open_nwio (Pipe& io, int flags, void* nwad);
};

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////

#endif


