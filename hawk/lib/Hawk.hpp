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

#ifndef _HAWK_HAWK_HPP_
#define _HAWK_HAWK_HPP_

#include <hawk.h>
#include <stdarg.h>

//#define HAWK_AWK_USE_HTB_FOR_FUNCTION_MAP 1
//#define HAWK_AWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW 1

#if defined(HAWK_AWK_USE_HTB_FOR_FUNCTION_MAP)
#	include <hawk-htb.h>
#else
#	include <hawk/HashTable.hpp>
#	include <hawk/Cstr.hpp>
#endif

/// \file
/// AWK Interpreter

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

/// 
/// The Hawk class implements an AWK interpreter by wrapping around 
/// #hawk_t and #hawk_rtx_t.
///
class HAWK_EXPORT Hawk: public Uncopyable, public Types, public Mmged
{
public:

	// define a primitive handle 
	typedef hawk_t awk_t;

	// redefine flt_t. To access Types::flt_t, use the fully qualified 
	// name as it's overriding Types::flt_t. 
	typedef hawk_flt_t  flt_t; 
	typedef hawk_int_t  int_t; 
	typedef hawk_uint_t uint_t; 

	typedef hawk_loc_t loc_t;
	typedef hawk_errnum_t errnum_t;
	typedef hawk_errstr_t errstr_t;
	typedef hawk_errinf_t errinf_t;

	enum depth_t
	{
		DEPTH_INCLUDE     = HAWK_AWK_DEPTH_INCLUDE,
		DEPTH_BLOCK_PARSE = HAWK_AWK_DEPTH_BLOCK_PARSE,
		DEPTH_BLOCK_RUN   = HAWK_AWK_DEPTH_BLOCK_RUN,
		DEPTH_EXPR_PARSE  = HAWK_AWK_DEPTH_EXPR_PARSE,
		DEPTH_EXPR_RUN    = HAWK_AWK_DEPTH_EXPR_RUN,
		DEPTH_REX_BUILD   = HAWK_AWK_DEPTH_REX_BUILD,
		DEPTH_REX_MATCH   = HAWK_AWK_DEPTH_REX_MATCH
	};

	/// The gbl_id_t type redefines #hawk_gbl_id_t.
	typedef hawk_gbl_id_t gbl_id_t;

	/** Represents an internal awk value */
	typedef hawk_val_t val_t;

	/** Represents a runtime context */
	typedef hawk_rtx_t rtx_t;

	/** Represents an runtime I/O data */
	typedef hawk_rio_arg_t rio_arg_t;

	typedef hawk_rio_cmd_t rio_cmd_t;

	typedef hawk_sio_arg_t sio_arg_t;

	typedef hawk_sio_cmd_t sio_cmd_t;

	typedef hawk_fnc_spec_t fnc_spec_t;

	typedef hawk_fnc_info_t fnc_info_t;

	typedef hawk_mod_spec_t mod_spec_t;

	class Run;
	friend class Run;


protected:
	///
	/// \name Error Handling
	///
	/// \{

	///
	/// The getErrorString() function returns a formatting string
	/// for an error code \a num. You can override this function
	/// to customize an error message. You must include the same numbers
	/// of ${X}'s as the orginal formatting string. Their order may be
	/// different. The example below changes the formatting string for
	/// #HAWK_AWK_ENOENT.
	/// \code
	/// const MyHawk::char_t* MyHawk::getErrorString (errnum_t num) const 
	/// {
	///    if (num == HAWK_AWK_ENOENT) return HAWK_T("cannot find '${0}'");
	///    return Hawk::getErrorString (num);
	/// }
	/// \endcode
	///
	virtual const char_t* getErrorString (
		errnum_t num
	) const;

public:
	///
	/// The getErrorNumber() function returns the number of the last
	/// error occurred.
	///
	errnum_t getErrorNumber () const;

	///
	/// The getErrorLocation() function returns the location of the 
	/// last error occurred.
	///
	loc_t getErrorLocation () const;

	///
	/// The Hawk::getErrorMessage() function returns a message describing
	/// the last error occurred.
	///
	const char_t* getErrorMessage () const;

	///
	/// The setError() function sets error information.
	///
	void setError (
		errnum_t      code, ///< error code
		const oocs_t* args  = HAWK_NULL, ///< message formatting 
		                                ///  argument array
		const loc_t*  loc   = HAWK_NULL  ///< error location
	);

	///
	/// The setErrorWithMessage() functions sets error information
	/// with a customized error message.
	///
	void setErrorWithMessage (
		errnum_t      code, ///< error code
		const char_t* msg,  ///< error message
		const loc_t*  loc   ///< error location
	);

	///
	/// The clearError() function clears error information 
	///
	void clearError ();

//protected: can't make it protected for borland 
	void retrieveError ();
	void retrieveError (Run* run);
	/// \}

protected:
	class NoSource;

public:
	/// 
	/// The Source class is an abstract class to encapsulate
	/// source script I/O. The Hawk::parse function requires a concrete
	/// object instantiated from its child class.
	///
	class HAWK_EXPORT Source
	{
	public:
		///
		/// The Mode type defines opening mode.
		///
		enum Mode
		{	
			READ,   ///< open for read
			WRITE   ///< open for write
		};

		///
		/// The Data class encapsulates information passed in and out
		/// for source script I/O. 
		///
		class HAWK_EXPORT Data
		{
		public:
			friend class Hawk;

		protected:
			Data (Hawk* awk, Mode mode, sio_arg_t* arg): 
				awk (awk), mode (mode), arg (arg)
			{
			}

		public:
			Mode getMode() const
			{
				return this->mode;
			}

			bool isMaster() const
			{
				return this->arg->prev == HAWK_NULL;
			}

			const char_t* getName() const
			{
				return this->arg->name;
			}

			// since it doesn't copy the contents,
			// it should point to something that outlives this object.
			void setName (const char_t* name)
			{
				this->arg->name = name;
			}

			const char_t* getPrevName() const
			{
				return this->arg->prev->name;
			}

			const void* getPrevHandle() const
			{
				return this->arg->prev->handle;
			}

			void* getHandle () const
			{
				return this->arg->handle;
			}

			void setHandle (void* handle)
			{
				this->arg->handle = handle;
			}

			operator Hawk* () const
			{
				return this->awk;
			}

			operator awk_t* () const
			{
				return this->awk->getHandle();
			}

		protected:
			Hawk* awk;
			Mode  mode;
			sio_arg_t* arg;
		};

		Source () {}
		virtual ~Source () {}

		virtual int open (Data& io) = 0;
		virtual int close (Data& io) = 0;
		virtual ssize_t read (Data& io, char_t* buf, size_t len) = 0;
		virtual ssize_t write (Data& io, const char_t* buf, size_t len) = 0;

		///
		/// The NONE object indicates no source.
		///
		static NoSource NONE;

	private:
		Source (const Source&);
		Source& operator= (const Source&);
	};

protected:
	class HAWK_EXPORT NoSource: public Source
	{
	public:
		int open (Data& io) { return -1; }
		int close (Data& io) { return 0; }
		ssize_t read (Data& io, char_t* buf, size_t len) { return 0; }
		ssize_t write (Data& io, const char_t* buf, size_t len) { return 0; }
	};

public:
	///
	/// The RIOBase class is a base class to represent runtime I/O 
	/// operations. The Console, File, Pipe classes implement more specific
	/// I/O operations by inheriting this class.
	///
	class HAWK_EXPORT RIOBase
	{
	protected:
		RIOBase (Run* run, rio_arg_t* riod);

	public:
		const char_t* getName() const;

		const void* getHandle () const;
		void setHandle (void* handle);

		int getUflags () const;
		void setUflags (int uflags);

		operator Hawk* () const;
		operator awk_t* () const;
		operator rio_arg_t* () const;
		operator Run* () const;
		operator rtx_t* () const;

	protected:
		Run* run;
		rio_arg_t* riod;

	private:
		RIOBase (const RIOBase&);
		RIOBase& operator= (const RIOBase&);
	};

	///
	/// The Pipe class encapsulates the pipe operations indicated by
	/// the | and || operators.
	///
	class HAWK_EXPORT Pipe: public RIOBase
	{
	public:
		friend class Hawk;

		/// The Mode type defines the opening mode.
		enum Mode
		{
			/// open for read-only access
			READ = HAWK_AWK_RIO_PIPE_READ,
			/// open for write-only access
			WRITE = HAWK_AWK_RIO_PIPE_WRITE,
			/// open for read and write
			RW = HAWK_AWK_RIO_PIPE_RW
		};

		/// The CloseMode type defines the closing mode for a pipe
		/// opened in the #RW mode.
		enum CloseMode
		{
			/// close both read and write ends
			CLOSE_FULL = HAWK_AWK_RIO_CLOSE_FULL, 
			/// close the read end only
			CLOSE_READ = HAWK_AWK_RIO_CLOSE_READ,
			/// close the write end only
			CLOSE_WRITE = HAWK_AWK_RIO_CLOSE_WRITE
		};

		class HAWK_EXPORT Handler 
		{
		public:
			virtual ~Handler () {}

			virtual int     open  (Pipe& io) = 0;
			virtual int     close (Pipe& io) = 0;
			virtual ssize_t read  (Pipe& io, char_t* buf, size_t len) = 0;
			virtual ssize_t write (Pipe& io, const char_t* buf, size_t len) = 0;
			virtual ssize_t writeBytes (Pipe& io, const hawk_bch_t* buf, size_t len) = 0;
			virtual int     flush (Pipe& io) = 0;
		};

	protected:
		Pipe (Run* run, rio_arg_t* riod);

	public:
		/// The getMode() function returns the opening mode requested.
		/// You can inspect the opening mode, typically in the 
		/// openPipe() function, to create a pipe with proper 
		/// access mode. It is harmless to call this function from
		/// other pipe handling functions.
		Mode getMode () const;

		/// The getCloseMode() function returns the closing mode 
		/// requested. The returned value is valid if getMode() 
		/// returns #RW.
		CloseMode getCloseMode () const;
	};

	///
	/// The File class encapsulates file operations by inheriting RIOBase.
	///
	class HAWK_EXPORT File: public RIOBase
	{
	public:
		friend class Hawk;

		enum Mode
		{
			READ = HAWK_AWK_RIO_FILE_READ,
			WRITE = HAWK_AWK_RIO_FILE_WRITE,
			APPEND = HAWK_AWK_RIO_FILE_APPEND
		};

		class HAWK_EXPORT Handler 
		{
		public:
			virtual ~Handler () {}

			virtual int     open  (File& io) = 0;
			virtual int     close (File& io) = 0;
			virtual ssize_t read  (File& io, char_t* buf, size_t len) = 0;
			virtual ssize_t write (File& io, const char_t* buf, size_t len) = 0;
			virtual ssize_t writeBytes (File& io, const hawk_bch_t* buf, size_t len) = 0;
			virtual int     flush (File& io) = 0;
		};

	protected:
		File (Run* run, rio_arg_t* riod);

	public:
		Mode getMode () const;
	};

	///
	/// The Console class encapsulates the console operations by 
	/// inheriting RIOBase.
	///
	class HAWK_EXPORT Console: public RIOBase
	{
	public:
		friend class Hawk;

		/// Console mode enumerators
		enum Mode
		{
			READ = HAWK_AWK_RIO_CONSOLE_READ,  ///< open for input
			WRITE = HAWK_AWK_RIO_CONSOLE_WRITE ///< open for output
		};

		/// 
		/// The Handler class is an abstract class that can be
		/// implemented for customized I/O handling.
		class HAWK_EXPORT Handler 
		{
		public:
			virtual ~Handler () {}

			/// The open() function is called before the initial
			/// access to the console for input and output.
			/// It must return 0 for success and -1 for failure.
			/// Upon successful opening, it can store information
			/// required using setHandle() and setUflags().
			/// The information set here is available in subsequent
			/// calls to other methods and are accessible with
			/// getHandle() and getUflags().
			virtual int open  (Console& io) = 0;

			/// The close() function is called when the console
			/// is not needed any more. It must return 0 for success 
			/// and -1 for failure. 
			virtual int close (Console& io) = 0;

			/// The read() function is called when the console
			/// is read for input. It must fill the buffer \a buf with
			/// data not more than \a len characters and return the
			/// number of characters filled into the buufer. It can
			/// return 0 to indicate EOF and -1 for failure.
			virtual ssize_t read  (Console& io, char_t* buf, size_t len) = 0;

			/// The write() function is called when the console
			/// is written for output. It can write upto \a len characters
			/// available in the buffer \a buf and return the number of
			/// characters written. It can return 0 to indicate EOF and -1 
			/// for failure.
			virtual ssize_t write (Console& io, const char_t* buf, size_t len) = 0;

			virtual ssize_t writeBytes (Console& io, const hawk_bch_t* buf, size_t len) = 0;

			/// You may choose to buffer the data passed to the write() 
			/// function and perform actual writing when flush() is called. 
			/// It must return 0 for success and -1 for failure.
			virtual int flush (Console& io) = 0;

			/// The next() function is called when \b nextfile or 
			/// \b nextofile is executed. It must return 0 for success 
			/// and -1 for failure.
			virtual int next  (Console& io) = 0;
		};

	protected:
		Console (Run* run, rio_arg_t* riod);
		~Console ();

	public:
		/// The getMode() function returns if the console is
		/// opened for reading or writing.
		Mode getMode () const;

		int setFileName (const char_t* name);
		int setFNR (int_t fnr);

	protected:
		char_t* filename;
	};


	///
	/// The Value class wraps around #hawk_val_t to provide a more 
	/// comprehensive interface.
	///
	class HAWK_EXPORT Value
	{
	public:
		friend class Hawk;

	#if defined(HAWK_AWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW)
		// initialization
		void* operator new (size_t n, Run* run) throw ();
		void* operator new[] (size_t n, Run* run) throw ();

	#if !defined(__BORLANDC__) && !defined(__WATCOMC__)
		// deletion when initialization fails
		void operator delete (void* p, Run* run);
		void operator delete[] (void* p, Run* run);
	#endif

		// normal deletion
		void operator delete (void* p);
		void operator delete[] (void* p);
	#endif

		///
		/// The Index class encapsulates an index of an arrayed value.
		///
		class HAWK_EXPORT Index: protected hawk_oocs_t
		{
		public:
			friend class Value;

			/// The Index() function creates an empty array index.
			Index ()
			{
				this->ptr = (char_t*)Value::getEmptyStr();
				this->len = 0;
			}

			/// The Index() function creates a string array index.
			Index (const char_t* ptr, size_t len)
			{
				this->ptr = (char_t*)ptr;
				this->len = len;
			}

			void set (const hawk_oocs_t* x)
			{
				this->ptr = x->ptr;
				this->len = x->len;
			}

			Index& operator= (const hawk_oocs_t* x)
			{
				this->set (x);
				return *this;
			}

			const char_t* pointer () const
			{
				return this->ptr;
			}

			size_t length () const
			{
				return this->len;
			}
		};

		///
		/// Represents a numeric index of an arrayed value
		///
		class HAWK_EXPORT IntIndex: public Index
		{
		public:
			IntIndex (int_t num);

		protected:
			// 2^32: 4294967296
			// 2^64: 18446744073709551616
			// 2^128: 340282366920938463463374607431768211456 
			// -(2^32/2): -2147483648
			// -(2^64/2): -9223372036854775808
			// -(2^128/2): -170141183460469231731687303715884105728
		#if HAWK_SIZEOF_LONG_T > 16
		#	error SIZEOF(int_t) TOO LARGE. 
		#	error INCREASE THE BUFFER SIZE TO SUPPORT IT.
		#elif HAWK_SIZEOF_LONG_T == 16
			char_t buf[41];
		#elif HAWK_SIZEOF_LONG_T == 8
			char_t buf[21];
		#else
			char_t buf[12];
		#endif
		};

		///
		/// The IndexIterator class is a helper class to make simple
		/// iteration over array elements.
		///
		class HAWK_EXPORT IndexIterator: public hawk_val_map_itr_t
		{
		public:
			friend class Value;

			///
			/// The END variable is a special variable to 
			/// represent the end of iteration.
			///
			static IndexIterator END;

			///
			/// The IndexIterator() function creates an iterator 
			/// for an arrayed value.
			///
			IndexIterator ()
			{
				this->pair = HAWK_NULL;
				this->buckno = 0;
			}

		protected:
			IndexIterator (hawk_htb_pair_t* pair, size_t buckno)
			{
				this->pair = pair;
				this->buckno = buckno;
			}

		public:
			bool operator==  (const IndexIterator& ii) const
			{
				return this->pair == ii.pair && this->buckno == ii.buckno;
			}

			bool operator!=  (const IndexIterator& ii) const
			{
				return !operator== (ii);
			}
		};

		///
		/// The Value() function creates an empty value associated
		/// with no runtime context. To set an actual inner value, 
		/// you must specify a context when calling setXXX() functions.
		/// i.e., use setInt(run,10) instead of setInt(10).
		/// 
		Value ();

		///
		/// The Value() function creates an empty value associated
		/// with a runtime context.
		///
		Value (Run& run);

		///
		/// The Value() function creates an empty value associated
		/// with a runtime context.
		///
		Value (Run* run);

		Value (const Value& v);
		~Value ();

		Value& operator= (const Value& v);

		void clear ();

		operator val_t* () const { return val; }
		operator int_t () const;
		operator flt_t () const;
		operator const char_t* () const;
	#if defined(HAWK_OOCH_IS_UCH)
		operator const hawk_bch_t* () const;
	#endif

		val_t* toVal () const
		{
			return operator val_t* ();
		}

		int_t toInt () const
		{
			return operator int_t ();
		}

		flt_t toFlt () const
		{
			return operator flt_t ();
		}

		const char_t* toStr (size_t* len) const
		{
			const char_t* p;
			size_t l;

			if (this->getStr(&p, &l) <= -1) 
			{
				p = this->getEmptyStr();
				l = 0;
			}
			
			if (len) *len = l;
			return p;
		}

		const hawk_bch_t* toMbs (size_t* len) const
		{
			const hawk_bch_t* p;
			size_t l;

			if (this->getMbs(&p, &l) <= -1) 
			{
				p = this->getEmptyMbs();
				l = 0;
			}
			
			if (len) *len = l;
			return p;
		}

		int getInt (int_t* v) const;
		int getFlt (flt_t* v) const;
		int getNum (int_t* lv, flt_t* fv) const;
		int getStr (const char_t** str, size_t* len) const;
		int getMbs (const hawk_bch_t** str, size_t* len) const;

		int setVal (val_t* v);
		int setVal (Run* r, val_t* v);

		int setInt (int_t v);
		int setInt (Run* r, int_t v);
		int setFlt (flt_t v);
		int setFlt (Run* r, flt_t v);

		int setStr (const char_t* str, size_t len, bool numeric = false);
		int setStr (Run* r, const char_t* str, size_t len, bool numeric = false);
		int setStr (const char_t* str, bool numeric = false);
		int setStr (Run* r, const char_t* str, bool numeric = false);

		int setMbs (const hawk_bch_t* str, size_t len);
		int setMbs (Run* r, const hawk_bch_t* str, size_t len);
		int setMbs (const hawk_bch_t* str);
		int setMbs (Run* r, const hawk_bch_t* str);

		int setIndexedVal (const Index& idx, val_t* v);
		int setIndexedVal (Run* r, const Index& idx, val_t* v);
		int setIndexedInt (const Index& idx, int_t v);
		int setIndexedInt (Run* r, const Index& idx, int_t v);
		int setIndexedFlt (const Index& idx, flt_t v);
		int setIndexedFlt (Run* r, const Index&  idx, flt_t v);

		int setIndexedStr (const Index& idx, const char_t* str, size_t len, bool numeric = false);
		int setIndexedStr (Run* r, const Index& idx, const char_t* str, size_t len, bool numeric = false);
		int setIndexedStr (const Index& idx, const char_t* str, bool  numeric = false);
		int setIndexedStr (Run* r, const Index&  idx, const char_t* str, bool  numeric = false);

		int setIndexedMbs (const Index& idx, const hawk_bch_t* str, size_t len);
		int setIndexedMbs (Run* r, const Index& idx, const hawk_bch_t* str, size_t len);
		int setIndexedMbs (const Index& idx, const hawk_bch_t* str);
		int setIndexedMbs (Run* r, const Index&  idx, const hawk_bch_t* str);

		///
		/// The isIndexed() function determines if a value is arrayed.
		/// \return true if indexed, false if not.
		///
		bool isIndexed () const;

		/// 
		/// The getIndexed() function gets a value at the given 
		/// index \a idx and sets it to \a val.
		/// \return 0 on success, -1 on failure
		///
		int getIndexed (
			const Index&  idx, ///< array index
			Value*        val  ///< value holder
		) const;

		///
		/// The getFirstIndex() function stores the first index of
		/// an arrayed value into \a idx. 
		/// \return IndexIterator::END if the arrayed value is empty,
		///         iterator that can be passed to getNextIndex() if not
		///
		IndexIterator getFirstIndex (
			Index* idx ///< index holder
		) const;

		///
		/// The getNextIndex() function stores into \a idx the next 
		/// index of an array value from the position indicated by 
		/// \a iter.
		/// \return IndexIterator::END if the arrayed value is empty,
		///         iterator that can be passed to getNextIndex() if not
		///
		IndexIterator getNextIndex (
			Index* idx,                 ///< index holder
			const IndexIterator& curitr ///< current position
		) const;

	protected:
		Run* run;
		val_t* val;

		mutable struct
		{
			hawk_oocs_t str;
			hawk_bcs_t mbs;
		} cached;

	public:
		static const char_t* getEmptyStr();
		static const hawk_bch_t* getEmptyMbs();
	};

public:
	///
	/// The Run class wraps around #hawk_rtx_t to represent the
	/// runtime context.
	///
	class HAWK_EXPORT Run
	{
	protected:
		friend class Hawk;
		friend class Value; 
		friend class RIOBase;
		friend class Console;

		Run (Hawk* awk);
		Run (Hawk* awk, rtx_t* run);
		~Run ();

	public:
		operator Hawk* () const;
		operator rtx_t* () const;

		void halt () const;
		bool isHalt () const;

		errnum_t getErrorNumber () const;
		loc_t getErrorLocation () const;
		const char_t* getErrorMessage () const;

		void setError (
			errnum_t      code, 
			const oocs_t* args = HAWK_NULL,
			const loc_t*  loc  = HAWK_NULL
		);

		void setErrorWithMessage (
			errnum_t      code, 
			const char_t* msg,
			const loc_t*  loc
		);

		/// 
		/// The setGlobal() function sets the value of a global 
		/// variable identified by \a id
		/// to \a v.
		/// \return 0 on success, -1 on failure
		///
		int setGlobal (int id, int_t v);

		/// 
		/// The setGlobal() function sets the value of a global 
		/// variable identified by \a id
		/// to \a v.
		/// \return 0 on success, -1 on failure
		///
		int setGlobal (int id, flt_t v); 

		/// 
		/// The setGlobal() function sets the value of a global 
		/// variable identified by \a id
		/// to a string as long as \a len characters pointed to by 
		/// \a ptr.
		/// \return 0 on success, -1 on failure
		///
		int setGlobal (int id, const char_t* ptr, size_t len);

		/// 
		/// The setGlobal() function sets a global variable 
		/// identified by \a id to a value \a v.
		/// \return 0 on success, -1 on failure
		///	
		int setGlobal (int id, const Value& v);

		///
		/// The getGlobal() function gets the value of a global 
		/// variable identified by \a id and stores it in \a v.
		/// \return 0 on success, -1 on failure
		///	
		int getGlobal (int id, Value& v) const;

	protected:
		Hawk*   awk;
		rtx_t* rtx;
	};

	///
	/// Returns the primitive handle 
	///
	operator awk_t* () const;

	///
	/// \name Basic Functions
	/// \{
	///

	/// The Hawk() function creates an interpreter without fully 
	/// initializing it. You must call open() for full initialization
	/// before calling other functions. 
	Hawk (Mmgr* mmgr = HAWK_NULL);

	/// The ~Hawk() function destroys an interpreter. Make sure to have
	/// called close() for finalization before the destructor is executed.
	virtual ~Hawk () {}

	hawk_cmgr_t* getCmgr () const;

	///
	/// The open() function initializes an interpreter. 
	/// You must call this function before doing anything meaningful.
	/// \return 0 on success, -1 on failure
	///
	int open ();

	///
	/// The close() function closes the interpreter. 
	///
	void close ();

	///
	/// The uponClosing() function is called back after Hawk::close() 
	/// has cleared most of the internal data but before destroying 
	/// the underlying awk object. This maps to the close callback
	/// of #hawk_ecb_t.
	/// 
	virtual void uponClosing ();

	///
	/// The uponClearing() function is called back when Hawk::close()
	/// begins clearing internal data. This maps to the clear callback
	/// of #hawk_ecb_t.
	///
	virtual void uponClearing ();

	///
	/// The parse() function parses the source code read from the input
	/// stream \a in and writes the parse tree to the output stream \a out.
	/// To disable deparsing, you may set \a out to Hawk::Source::NONE. 
	/// However, it is not allowed to specify Hawk::Source::NONE for \a in.
	///
	/// \return Run object on success, #HAWK_NULL on failure
	///
	Hawk::Run* parse (
		Source& in,  ///< script to parse 
		Source& out  ///< deparsing target 
	);

	///
	/// The getRunContext() funciton returns the execution context 
	/// returned by the parse() function. The returned context
	/// is valid if parse() has been called. You may call this 
	/// function to get the context if you forgot to store it
	/// in a call to parse().
	///
	const Hawk::Run* getRunContext () const
	{
		return &runctx;
	}

	///
	/// The getRunContext() funciton returns the execution context 
	/// returned by the parse() function. The returned context
	/// is valid if parse() has been called. You may call this 
	/// function to get the context if you forgot to store it
	/// in a call to parse().
	///
	Hawk::Run* getRunContext () 
	{
		return &runctx;
	}

	///
	/// The resetRunContext() function closes an existing 
	/// execution context and creates a new execution context.
	/// You may want to call this function if you want to
	/// reset it without calling the parse() function again
	/// after the first call to it. 
	Hawk::Run* resetRunContext ();
	
	///
	/// The loop() function executes the BEGIN block, pattern-action blocks,
	/// and the END block. The return value is stored into \a ret.
	/// \return 0 on succes, -1 on failure
	///
	int loop (
		Value* ret  ///< return value holder
	);

	///
	/// The call() function invokes a function named \a name.
	///
	int call (
		const hawk_bch_t* name,  ///< function name
		Value*             ret,   ///< return value holder
		const Value*       args,  ///< argument array
		size_t             nargs  ///< number of arguments
	);

	///
	/// The call() function invokes a function named \a name.
	///
	int call (
		const hawk_uch_t* name,  ///< function name
		Value*             ret,   ///< return value holder
		const Value*       args,  ///< argument array
		size_t             nargs  ///< number of arguments
	);

	///
	/// The halt() function makes request to abort execution
	///
	void halt ();
	/// \}

	///
	/// \name Configuration
	/// \{
	///

	///
	/// The getTrait() function gets the current options.
	/// \return current traits
	///
	int getTrait () const;

	///
	/// The setTrait() function changes the current traits.
	///
	void setTrait (
		int trait 
	);

	/// 
	/// The setMaxDepth() function sets the maximum processing depth
	/// for operations identified by \a ids.
	///
	void setMaxDepth (
		depth_t id,  ///< depth identifier
		size_t depth ///< new depth
	);

	///
	/// The getMaxDepth() function gets the maximum depth for an operation
	/// type identified by \a id.
	///
	size_t getMaxDepth (
		depth_t id   ///< depth identifier
	) const;

	///
	/// The addArgument() function adds an ARGV string as long as \a len 
	/// characters pointed to 
	/// by \a arg. loop() and call() make a string added available 
	/// to a script through ARGV. 
	/// \return 0 on success, -1 on failure
	///
	int addArgument (
		const char_t* arg,  ///< string pointer
		size_t        len   ///< string length
	);

	///
	/// The addArgument() function adds a null-terminated string \a arg. 
	/// loop() and call() make a string added available to a script 
	/// through ARGV. 
	/// \return 0 on success, -1 on failure
	///
	int addArgument (
		const char_t* arg ///< string pointer
	);

	///
	/// The clearArguments() function deletes all ARGV strings.
	///
	void clearArguments ();

	///
	/// The addGlobal() function registers an intrinsic global variable. 
	/// \return integer >= 0 on success, -1 on failure.
	///
	int addGlobal (
		const hawk_bch_t* name ///< variable name
	);

	int addGlobal (
		const hawk_uch_t* name ///< variable name
	);

	///
	/// The deleteGlobal() function unregisters an intrinsic global 
	/// variable by name.
	/// \return 0 on success, -1 on failure.
	///
	int deleteGlobal (
		const hawk_bch_t* name ///< variable name
	);

	int deleteGlobal (
		const hawk_uch_t* name ///< variable name
	);

	///
	/// The findGlobal() function returns the numeric ID of an intrinsic 
	//  global variable. 
	/// \return integer >= 0 on success, -1 on failure.
	///
	int findGlobal (
		const hawk_bch_t* name ///> variable name
	);
	int findGlobal (
		const hawk_uch_t* name ///> variable name
	);

	///
	/// The setGlobal() function sets the value of a global variable 
	/// identified by \a id. The \a id is either a value returned by 
	/// addGlobal() or one of the #gbl_id_t enumerators. It is not allowed
	/// to call this function prior to parse().
	/// \return 0 on success, -1 on failure
	///
	int setGlobal (
		int          id,  ///< numeric identifier
		const Value& v    ///< value
	);

	///
	/// The getGlobal() function gets the value of a global variable 
	/// identified by \a id. The \a id is either a value returned by 
	/// addGlobal() or one of the #gbl_id_t enumerators. It is not allowed
	/// to call this function before parse().
	/// \return 0 on success, -1 on failure
	///
	int getGlobal (
		int    id, ///< numeric identifier 
		Value& v   ///< value store 
	);

	///
	/// The FunctionHandler type defines a intrinsic function handler.
	///
	typedef int (Hawk::*FunctionHandler) (
		Run&              run,
		Value&            ret,
		Value*            args,
		size_t            nargs, 
		const fnc_info_t* fi
	);

	/// 
	/// The addFunction() function adds a new user-defined intrinsic 
	/// function.
	///
	int addFunction (
		const hawk_bch_t* name,      ///< function name
		size_t minArgs,               ///< minimum numbers of arguments
		size_t maxArgs,               ///< maximum numbers of arguments
		const hawk_bch_t* argSpec,   ///< argument specification
		FunctionHandler handler,      ///< function handler
		int    validOpts = 0          ///< valid if these options are set
	);

	int addFunction (
		const hawk_uch_t* name,      ///< function name
		size_t minArgs,               ///< minimum numbers of arguments
		size_t maxArgs,               ///< maximum numbers of arguments
		const hawk_uch_t* argSpec,   ///< argument specification
		FunctionHandler handler,      ///< function handler
		int    validOpts = 0          ///< valid if these options are set
	);

	///
	/// The deleteFunction() function deletes a user-defined intrinsic 
	/// function by name.
	///
	int deleteFunction (
		const char_t* name ///< function name
	);
	/// \}

	Pipe::Handler* getPipeHandler ()
	{
		return this->pipe_handler;
	}

	const Pipe::Handler* getPipeHandler () const
	{
		return this->pipe_handler;
	}

	///
	/// The setPipeHandler() function registers an external pipe
	/// handler object. An external pipe handler can be implemented
	/// outside this class without overriding various pipe functions.
	/// Note that an external pipe handler must outlive an outer
	/// awk object.
	/// 
	void setPipeHandler (Pipe::Handler* handler)
	{
		this->pipe_handler = handler;
	}

	File::Handler* getFileHandler ()
	{
		return this->file_handler;
	}

	const File::Handler* getFileHandler () const
	{
		return this->file_handler;
	}

	///
	/// The setFileHandler() function registers an external file
	/// handler object. An external file handler can be implemented
	/// outside this class without overriding various file functions.
	/// Note that an external file handler must outlive an outer
	/// awk object.
	/// 
	void setFileHandler (File::Handler* handler)
	{
		this->file_handler = handler;
	}

	Console::Handler* getConsoleHandler ()
	{
		return this->console_handler;
	}

	const Console::Handler* getConsoleHandler () const
	{
		return this->console_handler;
	}

	///
	/// The setConsoleHandler() function registers an external console
	/// handler object. An external file handler can be implemented
	/// outside this class without overriding various console functions.
	/// Note that an external console handler must outlive an outer
	/// awk object.
	/// 
	void setConsoleHandler (Console::Handler* handler)
	{
		this->console_handler = handler;
	}

protected:
	/// 
	/// \name Pipe I/O handlers
	/// Pipe operations are achieved through the following functions
	/// if no external pipe handler is set with setPipeHandler().
	/// \{

	/// The openPipe() function is a pure virtual function that must be
	/// overridden by a child class to open a pipe. It must return 1
	/// on success, 0 on end of a pipe, and -1 on failure.
	virtual int     openPipe  (Pipe& io);

	/// The closePipe() function is a pure virtual function that must be
	/// overridden by a child class to close a pipe. It must return 0
	/// on success and -1 on failure.
	virtual int     closePipe (Pipe& io);

	virtual ssize_t readPipe  (Pipe& io, char_t* buf, size_t len);
	virtual ssize_t writePipe (Pipe& io, const char_t* buf, size_t len);
	virtual ssize_t writePipeBytes (Pipe& io, const hawk_bch_t* buf, size_t len);
	virtual int     flushPipe (Pipe& io);
	/// \}

	/// 
	/// \name File I/O handlers
	/// File operations are achieved through the following functions
	/// if no external file handler is set with setFileHandler().
	/// \{
	///
	virtual int     openFile  (File& io);
	virtual int     closeFile (File& io);
	virtual ssize_t readFile  (File& io, char_t* buf, size_t len);
	virtual ssize_t writeFile (File& io, const char_t* buf, size_t len);
	virtual ssize_t writeFileBytes (File& io, const hawk_bch_t* buf, size_t len);
	virtual int     flushFile (File& io);
	/// \}

	/// 
	/// \name Console I/O handlers
	/// Console operations are achieved through the following functions.
	/// if no external console handler is set with setConsoleHandler().
	/// \{
	///
	virtual int     openConsole  (Console& io);
	virtual int     closeConsole (Console& io);
	virtual ssize_t readConsole  (Console& io, char_t* buf, size_t len);
	virtual ssize_t writeConsole (Console& io, const char_t* buf, size_t len);
	virtual ssize_t writeConsoleBytes (Console& io, const hawk_bch_t* buf, size_t len);
	virtual int     flushConsole (Console& io);
	virtual int     nextConsole  (Console& io);
	/// \}

	// primitive handlers 
	virtual flt_t pow (flt_t x, flt_t y) = 0;
	virtual flt_t mod (flt_t x, flt_t y) = 0;

	virtual void* modopen (const mod_spec_t* spec) = 0;
	virtual void  modclose (void* handle) = 0;
	virtual void* modsym (void* handle, const char_t* name) = 0;

	// static glue members for various handlers
	static ssize_t readSource (
		awk_t* awk, sio_cmd_t cmd, sio_arg_t* arg,
		char_t* data, size_t count);
	static ssize_t writeSource (
		awk_t* awk, sio_cmd_t cmd, sio_arg_t* arg,
		char_t* data, size_t count);

	static ssize_t pipeHandler (
		rtx_t* rtx, rio_cmd_t cmd, rio_arg_t* riod,
		void* data, size_t count);
	static ssize_t fileHandler (
		rtx_t* rtx, rio_cmd_t cmd, rio_arg_t* riod,
		void* data, size_t count);
	static ssize_t consoleHandler (
		rtx_t* rtx, rio_cmd_t cmd, rio_arg_t* riod,
		void* data, size_t count);

	static int functionHandler (rtx_t* rtx, const fnc_info_t* fi);


	static flt_t pow     (awk_t* awk, flt_t x, flt_t y);
	static flt_t mod     (awk_t* awk, flt_t x, flt_t y);

	static void* modopen  (awk_t* awk, const mod_spec_t* spec);
	static void  modclose (awk_t* awk, void* handle);
	static void* modsym   (awk_t* awk, void* handle, const char_t* name);

public:
	// use this with care
	awk_t* getHandle() const { return this->awk; }

protected:
	awk_t* awk;

	errstr_t dflerrstr;
	errinf_t errinf;

#if defined(HAWK_AWK_USE_HTB_FOR_FUNCTION_MAP)
	hawk_htb_t* functionMap;
#else
	
	class FunctionMap: public HashTable<Cstr,FunctionHandler>
	{
	public:
		FunctionMap (Hawk* awk): awk(awk) {}

	protected:
		Hawk* awk;
	};

	FunctionMap functionMap;
#endif

	Source* source_reader;
	Source* source_writer;

	Pipe::Handler* pipe_handler;
	File::Handler* file_handler;
	Console::Handler* console_handler;

	struct xstrs_t
	{
		xstrs_t (): ptr (HAWK_NULL), len (0), capa (0) {}

		int add (awk_t* awk, const char_t* arg, size_t len);
		void clear (awk_t* awk);

		hawk_oocs_t* ptr;
		size_t      len;
		size_t      capa;
	};

	xstrs_t runarg;

private:
	Run runctx;

	int init_runctx ();
	void fini_runctx ();
	int dispatch_function (Run* run, const fnc_info_t* fi);

	static const char_t* xerrstr (awk_t* a, errnum_t num);
};

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////

#endif
