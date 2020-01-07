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

#ifndef _HAWK_HAWK_STD_HPP_
#define _HAWK_HAWK_STD_HPP_

#include <Hawk.hpp>

/// \file
/// Standard AWK Interpreter

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
////////////////////////////////

class HAWK_EXPORT MmgrStd: public Mmgr
{
public:
	MmgrStd () HAWK_CPP_NOEXCEPT: Mmgr () {}

	void* allocMem (hawk_oow_t n) HAWK_CPP_NOEXCEPT;
	void* reallocMem (void* ptr, hawk_oow_t n) HAWK_CPP_NOEXCEPT;
	void freeMem (void* ptr) HAWK_CPP_NOEXCEPT;

#if 0
	/// The getInstance() function returns the stock instance of the MmgrStd class.
	static MmgrStd* getInstance () HAWK_CPP_NOEXCEPT;
#endif
};

///
/// The HawkStd class provides an easier-to-use interface by overriding 
/// primitive methods, and implementing the file handler, the pipe handler, 
/// and common intrinsic functions.
///
class HAWK_EXPORT HawkStd: public Hawk
{
public:
	///
	/// The SourceFile class implements script I/O from and to a file.
	///
	class HAWK_EXPORT SourceFile: public Source 
	{
	public:
		SourceFile (const hawk_ooch_t* name, hawk_cmgr_t* cmgr = HAWK_NULL): 
			name (name), cmgr (cmgr)
		{
			dir.ptr = HAWK_NULL; dir.len = 0; 
		}

		int open (Data& io);
		int close (Data& io);
		hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len);
		hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len);

	protected:
		const hawk_ooch_t* name;
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
		SourceString (const hawk_uch_t* str): _type(STR_UCH), _str(str), _hawk(HAWK_NULL), str(HAWK_NULL), ptr(HAWK_NULL) {}
		SourceString (const hawk_bch_t* str): _type(STR_BCH), _str(str), _hawk(HAWK_NULL), str(HAWK_NULL), ptr(HAWK_NULL) {}
		~SourceString ();

		int open (Data& io);
		int close (Data& io);
		hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len);
		hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len);

	protected:
		enum
		{
			STR_UCH,
			STR_BCH
		} _type;
		const void* _str;
		hawk_t* _hawk;

		hawk_ooch_t* str;
		const hawk_ooch_t* ptr;
	};

	HawkStd (Mmgr* mmgr = HAWK_NULL): Hawk(mmgr), stdmod_up(false), console_cmgr(HAWK_NULL) 
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
	int addConsoleOutput (const hawk_uch_t* arg, hawk_oow_t len);
	int addConsoleOutput (const hawk_uch_t* arg);
	int addConsoleOutput (const hawk_bch_t* arg, hawk_oow_t len);
	int addConsoleOutput (const hawk_bch_t* arg);

	void clearConsoleOutputs ();

protected:
	#define HAWK_STD_ENV_CHAR_IS_BCH
	typedef hawk_bch_t env_char_t;

	int make_additional_globals (Run* run);
	int build_argcv (Run* run);
	int build_environ (Run* run, env_char_t* envarr[]);

	// intrinsic functions 
	hawk_cmgr_t* getiocmgr (const hawk_ooch_t* ioname);

	int setioattr (Run& run, Value& ret, Value* args, hawk_oow_t nargs, const hawk_ooch_t* name, hawk_oow_t len);
	int getioattr (Run& run, Value& ret, Value* args, hawk_oow_t nargs, const hawk_ooch_t* name, hawk_oow_t len);

	// pipe io handlers 
	int openPipe (Pipe& io);
	int closePipe (Pipe& io);
	hawk_ooi_t readPipe  (Pipe& io, hawk_ooch_t* buf, hawk_oow_t len);
	hawk_ooi_t writePipe (Pipe& io, const hawk_ooch_t* buf, hawk_oow_t len);
	hawk_ooi_t writePipeBytes (Pipe& io, const hawk_bch_t* buf, hawk_oow_t len);
	int flushPipe (Pipe& io);

	// file io handlers 
	int openFile (File& io);
	int closeFile (File& io);
	hawk_ooi_t readFile (File& io, hawk_ooch_t* buf, hawk_oow_t len);
	hawk_ooi_t writeFile (File& io, const hawk_ooch_t* buf, hawk_oow_t len);
	hawk_ooi_t writeFileBytes (File& io, const hawk_bch_t* buf, hawk_oow_t len);
	int flushFile (File& io);

	// console io handlers 
	int openConsole (Console& io);
	int closeConsole (Console& io);
	hawk_ooi_t readConsole (Console& io, hawk_ooch_t* buf, hawk_oow_t len);
	hawk_ooi_t writeConsole (Console& io, const hawk_ooch_t* buf, hawk_oow_t len);
	hawk_ooi_t writeConsoleBytes (Console& io, const hawk_bch_t* buf, hawk_oow_t len);
	int flushConsole (Console& io);
	int nextConsole (Console& io);

	// primitive handlers 
	void* allocMem   (hawk_oow_t n);
	void* reallocMem (void* ptr, hawk_oow_t n);
	void  freeMem    (void* ptr);

	flt_t pow (flt_t x, flt_t y);
	flt_t mod (flt_t x, flt_t y);

	void* modopen (const mod_spec_t* spec);
	void  modclose (void* handle);
	void* modgetsym (void* handle, const hawk_ooch_t* name);

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
	hawk_oow_t runarg_index;
	hawk_oow_t runarg_count;

	// standard output console 
	xstrs_t ofile;
	hawk_oow_t ofile_index;
	hawk_oow_t ofile_count;

public:
	struct ioattr_t
	{
		hawk_cmgr_t* cmgr;
		hawk_ooch_t cmgr_name[64]; // i assume that the cmgr name never exceeds this length.
		hawk_ntime_t tmout[4];

		ioattr_t (): cmgr (HAWK_NULL)
		{
			this->cmgr_name[0] = HAWK_T('\0');
			for (hawk_oow_t i = 0; i < HAWK_COUNTOF(this->tmout); i++)
			{
				this->tmout[i].sec = -999;
				this->tmout[i].nsec = 0;
			}
		}
	};

	static ioattr_t default_ioattr;

protected:
	ioattr_t* get_ioattr (const hawk_ooch_t* ptr, hawk_oow_t len);
	ioattr_t* find_or_make_ioattr (const hawk_ooch_t* ptr, hawk_oow_t len);


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


