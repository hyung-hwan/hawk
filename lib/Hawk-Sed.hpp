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

#ifndef _HAWK_SED_HPP_
#define _HAWK_SED_HPP_

#include <hawk-sed.h>
#include <Hawk.hpp>

/// \file
/// This file defines C++ classes that you can use when you create a stream
/// editor. The C++ classes encapsulates the C data types and functions in
/// a more object-oriented manner.
///
/// \todo support sed tracer
///

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

///
/// The Sed class implements a stream editor by wrapping around #hawk_hawk_sed_t.
///
class HAWK_EXPORT Sed: public Uncopyable, public Mmged
{
public:
	///
	/// The Stream class is a abstract class for I/O operation during
	/// execution.
	///
	class HAWK_EXPORT Stream: public Uncopyable
	{
	public:
		/// The Mode type defines I/O modes.
		enum Mode
		{
			READ,  ///< open for read
			WRITE  ///< open for write
		};

		/// The Data class conveys information need for I/O operations.
		class HAWK_EXPORT Data
		{
		public:
			friend class Sed;

		protected:
			Data (Sed* sed, Mode mode, hawk_sed_io_arg_t* arg):
				sed (sed), mode (mode), arg (arg) {}

		public:
			/// The getMode() function returns the I/O mode
			/// requested.
			Mode getMode() const { return this->mode; }

			/// The getHandle() function returns the I/O handle
			/// saved by setHandle().
			void* getHandle () const { return this->arg->handle; }

			/// The setHandle() function sets an I/O handle
			/// typically in the Stream::open() function.
			void setHandle (void* handle) { this->arg->handle = handle; }

			/// The getName() function returns an I/O name.
			/// \return #HAWK_NULL for the main data stream,
			///         file path for explicit file stream
			const hawk_ooch_t* getName () const { return this->arg->path; }

			/// The Sed* operator returns the associated Sed class.
			operator Sed* () const { return this->sed; }

			/// The hawk_sed_t* operator returns a pointer to the
			/// underlying stream editor.
			operator hawk_sed_t* () const { return this->sed->getHandle(); }

			operator hawk_gem_t* () const { return hawk_sed_getgem(this->sed->getHandle()); }

		protected:
			Sed* sed;
			Mode mode;
			hawk_sed_io_arg_t* arg;
		};

		/// The Stream() function constructs a stream.
		Stream () {}

		/// The Stream() function destructs a stream.
		virtual ~Stream () {}

		/// The open() function should be implemented by a subclass
		/// to open a stream. It can get the mode requested by calling
		/// the Data::getMode() function over the I/O parameter \a io.
		///
		/// The return value of 0 may look a bit tricky. Easygoers
		/// can just return 1 on success and never return 0 from open().
		/// - If 0 is returned for the #READ mode, Sed::execute()
		///   returns success after having called close() as it has
		///   opened a console but has reached EOF.
		/// - If 0 is returned for the #WRITE mode and there are
		///   any write() calls, the Sed::execute() function returns
		///   failure after having called close() as it cannot write
		///   further on EOF.
		///
		/// \return -1 on failure, 1 on success,
		///         0 on success but reached EOF.
		virtual int open (Data& io) = 0;

		/// The close() function should be implemented by a subclass
		/// to open a stream.
		virtual int close (Data& io) = 0;

		virtual hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len) = 0;
		virtual hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len) = 0;

	private:
		Stream (const Stream&);
		Stream& operator= (const Stream&);
	};

	///
	/// The Sed() function creates an uninitialized stream editor.
	///
	Sed (Mmgr* mmgr);

	///
	/// The ~Sed() function destroys a stream editor.
	/// \note The close() function is not called by this destructor.
	///       To avoid resource leaks, You should call close() before
	///       a stream editor is destroyed if it has been initialized
	///       with open().
	///
	virtual ~Sed () {}


	hawk_cmgr_t* getCmgr () const;
	void setCmgr (hawk_cmgr_t* cmgr);

	///
	/// The open() function initializes a stream editor and makes it
	/// ready for subsequent use.
	/// \return 0 on success, -1 on failure.
	///
	int open ();

	///
	/// The close() function finalizes a stream editor.
	///
	void close ();

	///
	/// The compile() function compiles a script from a stream
	/// \a iostream.
	/// \return 0 on success, -1 on failure
	///
	int compile (Stream& sstream);

	///
	/// The execute() function executes compiled commands over the I/O
	/// streams defined through I/O handlers
	/// \return 0 on success, -1 on failure
	///
	int execute (Stream& istream, Stream& ostream);

	///
	/// The halt() function makes a request to break a running loop
	/// inside execute(). Note that this does not affect blocking
	/// operations in user-defined stream handlers.
	///
	void halt ();

	///
	/// The isHalt() function returns true if halt() has been called
	/// since the last call to execute(), false otherwise.
	///
	bool isHalt () const;

	///
	/// The getTrait() function gets the current traits.
	/// \return 0 or current options ORed of #trait_t enumerators.
	///
	int getTrait () const;

	///
	/// The setTrait() function sets traits for a stream editor.
	/// The option code \a opt is 0 or OR'ed of #trait_t enumerators.
	///
	void setTrait (
		int trait ///< option code
	);

	///
	/// The getErrorMessage() function gets the description of the last
	/// error occurred. It returns an empty string if the stream editor
	/// has not been initialized with the open() function.
	///
	const hawk_ooch_t* getErrorMessage() const;
	const hawk_uch_t* getErrorMessageU () const;
	const hawk_bch_t* getErrorMessageB () const;

	///
	/// The getErrorLocation() function gets the location where
	/// the last error occurred. The line and the column of the #hawk_loc_t
	/// structure retruend are 0 if the stream editor has not been
	/// initialized with the open() function.
	///
	hawk_loc_t getErrorLocation () const;

	///
	/// The getErrorNumber() function gets the number of the last
	/// error occurred. It returns HAWK_ENOERR if the stream editor
	/// has not been initialized with the open() function.
	///
	hawk_errnum_t getErrorNumber () const;

	///
	/// The setError() function sets information on an error occurred.
	///
	void setError (
		hawk_errnum_t      num,             ///< error number
		const hawk_oocs_t* args = HAWK_NULL, ///< string array for formatting
		                               ///  an error message
		const hawk_loc_t*  loc = HAWK_NULL   ///< error location
	);

	void formatError (
		hawk_errnum_t       code,
		const hawk_loc_t*   loc,
		const hawk_bch_t*   fmt,
		...
	);

	void formatError (
		hawk_errnum_t       code,
		const hawk_loc_t*   loc,
		const hawk_uch_t*   fmt,
		...
	);

	const hawk_ooch_t* getCompileId () const;

	const hawk_ooch_t* setCompileId (
		const hawk_ooch_t* id
	);

	///
	/// The getConsoleLine() function returns the current line
	/// number from an input console.
	/// \return current line number
	///
	hawk_oow_t getConsoleLine ();

	///
	/// The setConsoleLine() function changes the current line
	/// number from an input console.
	///
	void setConsoleLine (
		hawk_oow_t num ///< a line number
	);

protected:
	///
	/// The getErrorString() function returns an error formatting string
	/// for the error number \a num. A subclass wishing to customize
	/// an error formatting string may override this function.
	///
	virtual const hawk_ooch_t* getErrorString (
		hawk_errnum_t num ///< an error number
	) const;

public:
	// use this with care
	hawk_sed_t* getHandle() const { return this->sed; }

protected:
	hawk_cmgr_t* _cmgr;
	/// handle to a primitive sed object
	hawk_sed_t* sed;
	/// default error formatting string getter
	hawk_errstr_t dflerrstr;
	/// Stream to read script from
	Stream* sstream;
	/// I/O stream to read data from
	Stream* istream;
	/// I/O stream to write output to
	Stream* ostream;

private:
	static hawk_ooi_t sin (hawk_sed_t* s, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len);
	static hawk_ooi_t xin (hawk_sed_t* s, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len);
	static hawk_ooi_t xout (hawk_sed_t* s, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* dat, hawk_oow_t len);
	static const hawk_ooch_t* xerrstr (hawk_sed_t* s, hawk_errnum_t num);
};


///
/// The StdSed class inherits the Sed class, implements a standard
/// I/O stream class, and sets the default memory manager.
///
class HAWK_EXPORT SedStd: public Sed
{
public:
	SedStd (Mmgr* mmgr = MmgrStd::getDFL()): Sed(mmgr) {}

	///
	/// The FileStream class implements a stream over input
	/// and output files.
	///
	class HAWK_EXPORT FileStream: public Stream
	{
	public:
		FileStream (const hawk_uch_t* file = HAWK_NULL,
		            hawk_cmgr_t* cmgr = HAWK_NULL);

		FileStream (const hawk_bch_t* file = HAWK_NULL,
		            hawk_cmgr_t* cmgr = HAWK_NULL);

		~FileStream ();

		int open (Data& io);
		int close (Data& io);
		hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len);
		hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len);

	protected:
		enum
		{
			NAME_UCH,
			NAME_BCH
		} _type;
		const void* _file;
		hawk_ooch_t* file;
		hawk_cmgr_t* cmgr;
	};

	///
	/// The StringStream class implements a stream over a string
	///
	class HAWK_EXPORT StringStream: public Stream
	{
	public:
		StringStream (hawk_cmgr_t* cmr = HAWK_NULL);
		StringStream (const hawk_uch_t* in, hawk_cmgr_t* cmgr = HAWK_NULL);
		StringStream (const hawk_uch_t* in, hawk_oow_t len, hawk_cmgr_t* cmgr = HAWK_NULL);
		StringStream (const hawk_bch_t* in, hawk_cmgr_t* cmgr = HAWK_NULL);
		StringStream (const hawk_bch_t* in, hawk_oow_t len, hawk_cmgr_t* cmgr = HAWK_NULL);
		~StringStream ();

		int open (Data& io);
		int close (Data& io);
		hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len);
		hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len);

		const hawk_ooch_t* getOutput (hawk_oow_t* len = HAWK_NULL) const;
		const hawk_bch_t* getOutputB (hawk_oow_t* len = HAWK_NULL);
		const hawk_uch_t* getOutputU (hawk_oow_t* len = HAWK_NULL);

	protected:
		enum
		{
			STR_UCH,
			STR_BCH
		} _type;

		struct
		{
			hawk_sed_t* _sed;
			const void* _str;
			const void* _end;

			hawk_ooch_t* str;
			hawk_ooch_t* end;
			const hawk_ooch_t* ptr;
		} in;

		struct
		{
			bool inited;
			hawk_ooecs_t buf;
			hawk_sed_t* _sed;
			hawk_sed_ecb_t sed_ecb;

			void* alt_buf;
			hawk_sed_t* alt_sed;
		} out;

		hawk_cmgr_t* cmgr;


		static void on_sed_close (hawk_sed_t* sed, void* ctx);
		void clearInputData ();
		void clearOutputData (bool kill_ecb);
	};
};

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////

#endif
