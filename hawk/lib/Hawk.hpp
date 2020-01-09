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

#define HAWK_USE_HTB_FOR_FUNCTION_MAP 1
//#define HAWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW 1
//#define HAWK_NO_LOCATION_IN_EXCEPTION 1

// TOOD: redefine these NAMESPACE stuffs or move them somewhere else
#define HAWK_BEGIN_NAMESPACE(x) namespace x {
#define HAWK_END_NAMESPACE(x) }

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
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

#if (__cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1900) // C++11 or later
	#define HAWK_LANG_CPP11 1
	#define HAWK_CPP_NOEXCEPT noexcept(true)

	/// The HAWK_CPP_ENABLE_CPP11_MOVE macro enables C++11 move semantics in various classes.
	#define HAWK_CPP_ENABLE_CPP11_MOVE 1

	// The HAWK_CPP_CALL_DESTRUCTOR() macro calls a destructor explicitly.
	#define HAWK_CPP_CALL_DESTRUCTOR(ptr, class_name) ((ptr)->~class_name())

	// The HAWK_CPP_CALL_PLACEMENT_DELETE1() macro calls the global operator delete
	// with 1 extra argument given.
	#define HAWK_CPP_CALL_PLACEMENT_DELETE1(ptr, arg1) (::operator delete((ptr), (arg1)))

#elif (__cplusplus >= 199711L) // C++98
	#undef HAWK_LANG_CPP11 
	#define HAWK_CPP_NOEXCEPT throw()

	#define HAWK_CPP_CALL_DESTRUCTOR(ptr, class_name) ((ptr)->~class_name())
	#define HAWK_CPP_CALL_PLACEMENT_DELETE1(ptr, arg1) (::operator delete((ptr), (arg1)))
#else
	#undef HAWK_LANG_CPP11 
	#define HAWK_CPP_NOEXCEPT 

	#if defined(__BORLANDC__)

		// Explicit destructor call requires a class name depending on the
		// C++ standard/compiler.  
		// 
		//   Node* x;
		//   x->~Node (); 
		//
		// While x->~Node() is ok with modern compilers, some old compilers
		// like BCC55 required the class name in the call as shown below.
		//
		//   x->Node::~Node ();

		#define HAWK_CPP_CALL_DESTRUCTOR(ptr, class_name) ((ptr)->class_name::~class_name())
		#define HAWK_CPP_CALL_PLACEMENT_DELETE1(ptr, arg1) (::operator delete((ptr), (arg1)))


	#elif defined(__WATCOMC__)
		// WATCOM has a problem with this syntax.
		//    Node* x; x->Node::~Node(). 
		// But it doesn't support operator delete overloading.

		#define HAWK_CPP_CALL_DESTRUCTOR(ptr, class_name) ((ptr)->~class_name())
		#define HAWK_CPP_CALL_PLACEMENT_DELETE1(ptr, arg1) (::hawk_operator_delete((ptr), (arg1)))

		// When  the name of a member template specialization appears after .  or
		// -> in a postfix-expression, or after :: in a qualified-id that explic-
		// itly  depends on a template-argument (_temp.dep_), the member template
		// name must be prefixed by the keyword template.  Otherwise the name  is
		// assumed to name a non-template.  [Example:
		// 		class X {
		// 		public:
		// 			   template<size_t> X* alloc();
		// 		};
		// 		void f(X* p)
		// 		{
		// 			   X* p1 = p->alloc<200>();
		// 					 // ill-formed: < means less than
		// 			   X* p2 = p->template alloc<200>();
		// 					 // fine: < starts explicit qualification
		// 		}
		// --end example]
		//
		// WATCOM doesn't support this qualifier.

		#define HAWK_CPP_NO_OPERATOR_DELETE_OVERLOADING 1
	#else

		#define HAWK_CPP_CALL_DESTRUCTOR(ptr, class_name) ((ptr)->~class_name())
		#define HAWK_CPP_CALL_PLACEMENT_DELETE1(ptr, arg1) (::operator delete((ptr), (arg1)))

	#endif

#endif

#if defined(HAWK_CPP_ENABLE_CPP11_MOVE)

	template<typename T> struct HAWK_CPP_RMREF      { typedef T Type; };
	template<typename T> struct HAWK_CPP_RMREF<T&>  { typedef T Type; };
	template<typename T> struct HAWK_CPP_RMREF<T&&> { typedef T Type; };

	template<typename T> inline
	typename HAWK_CPP_RMREF<T>::Type&& HAWK_CPP_RVREF(T&& v)
	{
		return (typename HAWK_CPP_RMREF<T>::Type&&)v;
	}
#else

	/*
	template<typename T> inline
	T& HAWK_CPP_RVREF(T& v) { return (T&)v; }

	template<typename T> inline
	const T& HAWK_CPP_RVREF(const T& v) { return (const T&)v; }
	*/
	#define HAWK_CPP_RVREF(x) x
#endif

/// The Exception class implements the exception object.
class HAWK_EXPORT Exception
{
public:
	Exception (
		const hawk_ooch_t* name, const hawk_ooch_t* msg, 
		const hawk_ooch_t* file, hawk_oow_t line) HAWK_CPP_NOEXCEPT: 
		name(name), msg(msg)
#if !defined(HAWK_NO_LOCATION_IN_EXCEPTION)
		, file(file), line(line) 
#endif
	{
	}

	const hawk_ooch_t* name;
	const hawk_ooch_t* msg;
#if !defined(HAWK_NO_LOCATION_IN_EXCEPTION)
	const hawk_ooch_t* file;
	hawk_oow_t        line;
#endif
};

#define HAWK_THROW(ex_name) \
	throw ex_name(HAWK_Q(ex_name),HAWK_Q(ex_name), HAWK_T(__FILE__), (hawk_oow_t)__LINE__)
#define HAWK_THROW_WITH_MSG(ex_name,msg) \
	throw ex_name(HAWK_Q(ex_name),msg, HAWK_T(__FILE__), (hawk_oow_t)__LINE__)

#define HAWK_EXCEPTION(ex_name) \
	class ex_name: public HAWK::Exception \
	{ \
	public: \
		ex_name (const hawk_ooch_t* name, const hawk_ooch_t* msg, \
		         const hawk_ooch_t* file, hawk_oow_t line) HAWK_CPP_NOEXCEPT: \
			HAWK::Exception (name, msg, file, line) {} \
	}

#define HAWK_EXCEPTION_NAME(exception_object) ((exception_object).name)
#define HAWK_EXCEPTION_MSG(exception_object)  ((exception_object).msg)

#if !defined(HAWK_NO_LOCATION_IN_EXCEPTION)
#	define HAWK_EXCEPTION_FILE(exception_object) ((exception_object).file)
#	define HAWK_EXCEPTION_LINE(exception_object) ((exception_object).line)
#else
#	define HAWK_EXCEPTION_FILE(exception_object) (HAWK_T(""))
#	define HAWK_EXCEPTION_LINE(exception_object) (0)
#endif

class HAWK_EXPORT Uncopyable
{
public:
	Uncopyable () HAWK_CPP_NOEXCEPT {}
	//virtual ~Uncopyable () {}

private:
	Uncopyable (const Uncopyable&);
	const Uncopyable& operator= (const Uncopyable&);
};

///
/// The Mmgr class defines a memory manager interface that can be inherited
/// by a class in need of a memory manager as defined in the primitive 
/// #hawk_mmgr_t type. Using the class over the primitive type enables you to
/// write code in more object-oriented fashion. An inheriting class should 
/// implement three pure virtual functions.
///
/// You are free to call allocMem(), reallocMem(), and freeMem() in C++ context
/// where no exception raising is desired. If you want an exception to be 
/// raised upon memory allocation errors, you can call allocate(), reallocate(),
/// dispose() instead.
/// 
/// 
class HAWK_EXPORT Mmgr: public hawk_mmgr_t
{
public:
	/// defines an alias type to #hawk_mmgr_t 
	typedef hawk_mmgr_t mmgr_t;

	HAWK_EXCEPTION (MemoryError);

public:
	///
	/// The Mmgr() function builds a memory manager composed of bridge
	/// functions connecting itself with it.
	///
	Mmgr () HAWK_CPP_NOEXCEPT
	{
		// NOTE:
		//  the #hawk_mmgr_t interface is not affected by raise_exception
		//  because direct calls to the virtual functions that don't raise
		//  exceptions are made via bridge functions below.
		this->alloc = alloc_mem;
		this->realloc = realloc_mem;
		this->free = free_mem;
		this->ctx = this;
	}

	///
	/// The ~Mmgr() function finalizes a memory manager.
	///
	virtual ~Mmgr () HAWK_CPP_NOEXCEPT {}

	///
	/// The allocate() function calls allocMem() for memory
	/// allocation. if it fails, it raise an exception if it's
	/// configured to do so.
	///
	void* allocate (hawk_oow_t n, bool raise_exception = true)
	{
		void* xptr = this->allocMem (n);
		if (!xptr && raise_exception) HAWK_THROW (MemoryError);
		return xptr;
	}

	///
	/// The callocate() function allocates memory like allocate() and 
	/// clears the memory before returning.
	///
	void* callocate (hawk_oow_t n, bool raise_exception = true);

	///
	/// The reallocate() function calls reallocMem() for memory
	/// reallocation. if it fails, it raise an exception if it's
	/// configured to do so.
	///
	void* reallocate (void* ptr, hawk_oow_t n, bool raise_exception = true)
	{
		void* xptr = this->reallocMem (ptr, n);
		if (!xptr && raise_exception) HAWK_THROW (MemoryError);
		return xptr;
	}

	///
	/// The dispose() function calls freeMem() for memory disposal.
	///
	void dispose (void* ptr) HAWK_CPP_NOEXCEPT
	{
		this->freeMem (ptr);
	}

//protected:
	/// 
	/// The allocMem() function allocates a chunk of memory of the 
	/// size \a n and return the pointer to the beginning of the chunk.
	/// If it fails to allocate memory, it should return #HAWK_NULL.
	///
	virtual void* allocMem (
		hawk_oow_t n ///< size of memory chunk to allocate in bytes 
	) HAWK_CPP_NOEXCEPT = 0;

	///
	/// The reallocMem() function resizes a chunk of memory previously
	/// allocated with the allocMem() function. When resized, the contents
	/// of the surviving part of a memory chunk is preserved. If it fails to
	/// resize memory, it should return HAWK_NULL.
	///
	virtual void* reallocMem (
		void* ptr, ///< pointer to memory chunk to resize
		hawk_oow_t n   ///< new size in bytes
	) HAWK_CPP_NOEXCEPT = 0;

	///
	/// The freeMem() function frees a chunk of memory allocated with
	/// the allocMem() function or resized with the reallocMem() function.
	///
	virtual void freeMem (
		void* ptr ///< pointer to memory chunk to free 
	) HAWK_CPP_NOEXCEPT = 0;

protected:
	///
	/// bridge function from the #hawk_mmgr_t type the allocMem() function.
	///
	static void* alloc_mem (mmgr_t* mmgr, hawk_oow_t n) HAWK_CPP_NOEXCEPT;

	///
	/// bridge function from the #hawk_mmgr_t type the reallocMem() function.
	///
	static void* realloc_mem (mmgr_t* mmgr, void* ptr, hawk_oow_t n) HAWK_CPP_NOEXCEPT;

	///
	/// bridge function from the #hawk_mmgr_t type the freeMem() function.
	///
	static void  free_mem (mmgr_t* mmgr, void* ptr) HAWK_CPP_NOEXCEPT;

public:
	static Mmgr* getDFL () HAWK_CPP_NOEXCEPT;
	static void setDFL (Mmgr* mmgr) HAWK_CPP_NOEXCEPT;

protected:
	static Mmgr* dfl_mmgr;
};


class HAWK_EXPORT Mmged
{
public:
	Mmged (Mmgr* mmgr = HAWK_NULL);

	///
	/// The getMmgr() function returns the memory manager associated.
	///
	Mmgr* getMmgr () const { return this->_mmgr; }

protected:
	/// 
	/// The setMmgr() function changes the memory manager.
	/// Changing memory manager requires extra care to be taken 
	/// especially when you have some data allocated with the previous 
	/// manager. for this reason, i put this as a protected function.
	///
	void setMmgr(Mmgr* mmgr);

private:
	Mmgr* _mmgr;
};

/// 
/// The Hawk class implements an AWK interpreter by wrapping around 
/// #hhawk_t and #hawk_rtx_t.
///
class HAWK_EXPORT Hawk: public Uncopyable, public Mmged
{
public:

	enum depth_t
	{
		DEPTH_INCLUDE     = HAWK_DEPTH_INCLUDE,
		DEPTH_BLOCK_PARSE = HAWK_DEPTH_BLOCK_PARSE,
		DEPTH_BLOCK_RUN   = HAWK_DEPTH_BLOCK_RUN,
		DEPTH_EXPR_PARSE  = HAWK_DEPTH_EXPR_PARSE,
		DEPTH_EXPR_RUN    = HAWK_DEPTH_EXPR_RUN,
		DEPTH_REX_BUILD   = HAWK_DEPTH_REX_BUILD,
		DEPTH_REX_MATCH   = HAWK_DEPTH_REX_MATCH
	};

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
	/// #HAWK_ENOENT.
	/// \code
	/// const hawk_ooch_t* MyHawk::getErrorString (hawk_errnum_t num) const 
	/// {
	///    if (num == HAWK_ENOENT) return HAWK_T("cannot find '${0}'");
	///    return Hawk::getErrorString (num);
	/// }
	/// \endcode
	///
	virtual const hawk_ooch_t* getErrorString (
		hawk_errnum_t num
	) const;

public:
	///
	/// The getErrorNumber() function returns the number of the last
	/// error occurred.
	///
	hawk_errnum_t getErrorNumber () const;

	///
	/// The getErrorLocation() function returns the location of the 
	/// last error occurred.
	///
	hawk_loc_t getErrorLocation () const;
	const hawk_ooch_t* getErrorLocationFile () const;
	const hawk_uch_t* getErrorLocationFileU () const;
	const hawk_bch_t* getErrorLocationFileB () const;

	///
	/// The Hawk::getErrorMessage() function returns a message describing
	/// the last error occurred.
	///
	const hawk_ooch_t* getErrorMessage () const;
	const hawk_uch_t* getErrorMessageU () const;
	const hawk_bch_t* getErrorMessageB () const;

	void setError (
		hawk_errnum_t       code, ///< error code
		const hawk_loc_t*   loc   = HAWK_NULL  ///< error location
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
			Data (Hawk* awk, Mode mode, hawk_sio_arg_t* arg): 
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

			const hawk_ooch_t* getName() const
			{
				return this->arg->name;
			}

			// since it doesn't copy the contents,
			// it should point to something that outlives this object.
			void setName (const hawk_ooch_t* name)
			{
				this->arg->name = name;
			}

			const hawk_ooch_t* getPath() const
			{
				return this->arg->path;
			}

			void setPath (const hawk_ooch_t* path)
			{
				this->arg->path = (hawk_ooch_t*)path;
			}

			const hawk_ooch_t* getPrevName() const
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

			operator hawk_t* () const
			{
				return this->awk->getHandle();
			}

		protected:
			Hawk* awk;
			Mode  mode;
			hawk_sio_arg_t* arg;
		};

		Source () {}
		virtual ~Source () {}

		virtual int open (Data& io) = 0;
		virtual int close (Data& io) = 0;
		virtual hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len) = 0;
		virtual hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len) = 0;

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
		hawk_ooi_t read (Data& io, hawk_ooch_t* buf, hawk_oow_t len) { return 0; }
		hawk_ooi_t write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len) { return 0; }
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
		RIOBase (Run* run, hawk_rio_arg_t* riod): run(run), riod(riod) {}

	public:
		const hawk_ooch_t* getName() const { return this->riod->name; }

		const void* getHandle () const { return this->riod->handle; }
		void setHandle (void* handle) { this->riod->handle = handle; }

		int getUflags () const { return this->riod->uflags; }
		void setUflags (int uflags) { this->riod->uflags = uflags; }

		operator Hawk* () const { return this->run->awk; }
		operator hawk_t* () const 
		{
			HAWK_ASSERT (hawk_rtx_getawk(this->run->rtx) == this->run->awk->getHandle());
			return this->run->awk->getHandle(); 
		}
		operator hawk_rio_arg_t* () const { return this->riod; }
		operator Run* () const { return this->run; }
		operator hawk_rtx_t* () const { return this->run->rtx; }
		operator hawk_gem_t* () const { return hawk_rtx_getgem(this->run->rtx); }

	protected:
		Run* run;
		hawk_rio_arg_t* riod;

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
			READ = HAWK_RIO_PIPE_READ,
			/// open for write-only access
			WRITE = HAWK_RIO_PIPE_WRITE,
			/// open for read and write
			RW = HAWK_RIO_PIPE_RW
		};

		/// The CloseMode type defines the closing mode for a pipe
		/// opened in the #RW mode.
		enum CloseMode
		{
			/// close both read and write ends
			CLOSE_FULL = HAWK_RIO_CMD_CLOSE_FULL, 
			/// close the read end only
			CLOSE_READ = HAWK_RIO_CMD_CLOSE_READ,
			/// close the write end only
			CLOSE_WRITE = HAWK_RIO_CMD_CLOSE_WRITE
		};

		class HAWK_EXPORT Handler 
		{
		public:
			virtual ~Handler () {}

			virtual int     open  (Pipe& io) = 0;
			virtual int     close (Pipe& io) = 0;
			virtual hawk_ooi_t read  (Pipe& io, hawk_ooch_t* buf, hawk_oow_t len) = 0;
			virtual hawk_ooi_t write (Pipe& io, const hawk_ooch_t* buf, hawk_oow_t len) = 0;
			virtual hawk_ooi_t writeBytes (Pipe& io, const hawk_bch_t* buf, hawk_oow_t len) = 0;
			virtual int     flush (Pipe& io) = 0;
		};

	protected:
		Pipe (Run* run, hawk_rio_arg_t* riod);

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
			READ = HAWK_RIO_FILE_READ,
			WRITE = HAWK_RIO_FILE_WRITE,
			APPEND = HAWK_RIO_FILE_APPEND
		};

		class HAWK_EXPORT Handler 
		{
		public:
			virtual ~Handler () {}

			virtual int     open  (File& io) = 0;
			virtual int     close (File& io) = 0;
			virtual hawk_ooi_t read  (File& io, hawk_ooch_t* buf, hawk_oow_t len) = 0;
			virtual hawk_ooi_t write (File& io, const hawk_ooch_t* buf, hawk_oow_t len) = 0;
			virtual hawk_ooi_t writeBytes (File& io, const hawk_bch_t* buf, hawk_oow_t len) = 0;
			virtual int     flush (File& io) = 0;
		};

	protected:
		File (Run* run, hawk_rio_arg_t* riod);

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
			READ = HAWK_RIO_CONSOLE_READ,  ///< open for input
			WRITE = HAWK_RIO_CONSOLE_WRITE ///< open for output
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
			virtual hawk_ooi_t read  (Console& io, hawk_ooch_t* buf, hawk_oow_t len) = 0;

			/// The write() function is called when the console
			/// is written for output. It can write upto \a len characters
			/// available in the buffer \a buf and return the number of
			/// characters written. It can return 0 to indicate EOF and -1 
			/// for failure.
			virtual hawk_ooi_t write (Console& io, const hawk_ooch_t* buf, hawk_oow_t len) = 0;

			virtual hawk_ooi_t writeBytes (Console& io, const hawk_bch_t* buf, hawk_oow_t len) = 0;

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
		Console (Run* run, hawk_rio_arg_t* riod);
		~Console ();

	public:
		/// The getMode() function returns if the console is
		/// opened for reading or writing.
		Mode getMode () const;

		int setFileName (const hawk_ooch_t* name);
		int setFNR (hawk_int_t fnr);

	protected:
		hawk_ooch_t* filename;
	};


	///
	/// The Value class wraps around #hawk_val_t to provide a more 
	/// comprehensive interface.
	///
	class HAWK_EXPORT Value
	{
	public:
		friend class Hawk;

	#if defined(HAWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW)
		// initialization
		void* operator new (hawk_oow_t n, Run* run) throw ();
		void* operator new[] (hawk_oow_t n, Run* run) throw ();

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
				this->ptr = (hawk_ooch_t*)Value::getEmptyStr();
				this->len = 0;
			}

			/// The Index() function creates a string array index.
			Index (const hawk_ooch_t* ptr, hawk_oow_t len)
			{
				this->ptr = (hawk_ooch_t*)ptr;
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

			const hawk_ooch_t* pointer () const
			{
				return this->ptr;
			}

			hawk_oow_t length () const
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
			IntIndex (hawk_int_t num);

		protected:
			// 2^32: 4294967296
			// 2^64: 18446744073709551616
			// 2^128: 340282366920938463463374607431768211456 
			// -(2^32/2): -2147483648
			// -(2^64/2): -9223372036854775808
			// -(2^128/2): -170141183460469231731687303715884105728
		#if HAWK_SIZEOF_LONG_T > 16
		#	error SIZEOF(hawk_int_t) TOO LARGE. 
		#	error INCREASE THE BUFFER SIZE TO SUPPORT IT.
		#elif HAWK_SIZEOF_LONG_T == 16
			hawk_ooch_t buf[41];
		#elif HAWK_SIZEOF_LONG_T == 8
			hawk_ooch_t buf[21];
		#else
			hawk_ooch_t buf[12];
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
			IndexIterator (hawk_htb_pair_t* pair, hawk_oow_t buckno)
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

		operator hawk_val_t* () const { return val; }
		operator hawk_int_t () const;
		operator hawk_flt_t () const;
		operator const hawk_ooch_t* () const;
	#if defined(HAWK_OOCH_IS_UCH)
		operator const hawk_bch_t* () const;
	#endif

		hawk_val_t* toVal () const
		{
			return operator hawk_val_t* ();
		}

		hawk_int_t toInt () const
		{
			return operator hawk_int_t ();
		}

		hawk_flt_t toFlt () const
		{
			return operator hawk_flt_t ();
		}

		const hawk_ooch_t* toStr (hawk_oow_t* len) const
		{
			const hawk_ooch_t* p;
			hawk_oow_t l;

			if (this->getStr(&p, &l) <= -1) 
			{
				p = this->getEmptyStr();
				l = 0;
			}
			
			if (len) *len = l;
			return p;
		}

		const hawk_bch_t* toMbs (hawk_oow_t* len) const
		{
			const hawk_bch_t* p;
			hawk_oow_t l;

			if (this->getMbs(&p, &l) <= -1) 
			{
				p = this->getEmptyMbs();
				l = 0;
			}
			
			if (len) *len = l;
			return p;
		}

		int getInt (hawk_int_t* v) const;
		int getFlt (hawk_flt_t* v) const;
		int getNum (hawk_int_t* lv, hawk_flt_t* fv) const;
		int getStr (const hawk_ooch_t** str, hawk_oow_t* len) const;
		int getMbs (const hawk_bch_t** str, hawk_oow_t* len) const;

		int setVal (hawk_val_t* v);
		int setVal (Run* r, hawk_val_t* v);

		int setInt (hawk_int_t v);
		int setInt (Run* r, hawk_int_t v);
		int setFlt (hawk_flt_t v);
		int setFlt (Run* r, hawk_flt_t v);

		int setStr (const hawk_uch_t* str, hawk_oow_t len, bool numeric = false);
		int setStr (Run* r, const hawk_uch_t* str, hawk_oow_t len, bool numeric = false);
		int setStr (const hawk_uch_t* str, bool numeric = false);
		int setStr (Run* r, const hawk_uch_t* str, bool numeric = false);
		int setStr (const hawk_bch_t* str, hawk_oow_t len, bool numeric = false);
		int setStr (Run* r, const hawk_bch_t* str, hawk_oow_t len, bool numeric = false);
		int setStr (const hawk_bch_t* str, bool numeric = false);
		int setStr (Run* r, const hawk_bch_t* str, bool numeric = false);

		int setMbs (const hawk_bch_t* str, hawk_oow_t len);
		int setMbs (Run* r, const hawk_bch_t* str, hawk_oow_t len);
		int setMbs (const hawk_bch_t* str);
		int setMbs (Run* r, const hawk_bch_t* str);

		int setIndexedVal (const Index& idx, hawk_val_t* v);
		int setIndexedVal (Run* r, const Index& idx, hawk_val_t* v);
		int setIndexedInt (const Index& idx, hawk_int_t v);
		int setIndexedInt (Run* r, const Index& idx, hawk_int_t v);
		int setIndexedFlt (const Index& idx, hawk_flt_t v);
		int setIndexedFlt (Run* r, const Index&  idx, hawk_flt_t v);

		int setIndexedStr (const Index& idx, const hawk_uch_t* str, hawk_oow_t len, bool numeric = false);
		int setIndexedStr (Run* r, const Index& idx, const hawk_uch_t* str, hawk_oow_t len, bool numeric = false);
		int setIndexedStr (const Index& idx, const hawk_uch_t* str, bool  numeric = false);
		int setIndexedStr (Run* r, const Index&  idx, const hawk_uch_t* str, bool  numeric = false);

		int setIndexedStr (const Index& idx, const hawk_bch_t* str, hawk_oow_t len, bool numeric = false);
		int setIndexedStr (Run* r, const Index& idx, const hawk_bch_t* str, hawk_oow_t len, bool numeric = false);
		int setIndexedStr (const Index& idx, const hawk_bch_t* str, bool  numeric = false);
		int setIndexedStr (Run* r, const Index&  idx, const hawk_bch_t* str, bool  numeric = false);

		int setIndexedMbs (const Index& idx, const hawk_bch_t* str, hawk_oow_t len);
		int setIndexedMbs (Run* r, const Index& idx, const hawk_bch_t* str, hawk_oow_t len);
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
		hawk_val_t* val;

		mutable struct
		{
			hawk_oocs_t str;
			hawk_bcs_t mbs;
		} cached;

	public:
		static const hawk_ooch_t* getEmptyStr();
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
		Run (Hawk* awk, hawk_rtx_t* run);
		~Run ();

	public:
		operator Hawk* () const
		{
			return this->awk;
		}

		operator hawk_rtx_t* () const
		{
			return this->rtx;
		}

		operator hawk_gem_t* () const
		{
			return this->rtx? hawk_rtx_getgem(this->rtx): HAWK_NULL;
		}

		void halt () const;
		bool isHalt () const;

		hawk_errnum_t getErrorNumber () const;
		hawk_loc_t getErrorLocation () const;
		const hawk_ooch_t* getErrorMessage () const;
		const hawk_uch_t* getErrorMessageU () const;
		const hawk_bch_t* getErrorMessageB () const;

		///
		/// The setError() function sets error information.
		///
		void setError (
			hawk_errnum_t      code, ///< error code
			const hawk_loc_t*  loc   = HAWK_NULL  ///< error location
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

		/// 
		/// The setGlobal() function sets the value of a global 
		/// variable identified by \a id
		/// to \a v.
		/// \return 0 on success, -1 on failure
		///
		int setGlobal (int id, hawk_int_t v);

		/// 
		/// The setGlobal() function sets the value of a global 
		/// variable identified by \a id
		/// to \a v.
		/// \return 0 on success, -1 on failure
		///
		int setGlobal (int id, hawk_flt_t v); 

		/// 
		/// The setGlobal() function sets the value of a global 
		/// variable identified by \a id
		/// to a string as long as \a len characters pointed to by 
		/// \a ptr.
		/// \return 0 on success, -1 on failure
		///
		int setGlobal (int id, const hawk_ooch_t* ptr, hawk_oow_t len);

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
		Hawk*  awk;
		hawk_rtx_t* rtx;
	};

	///
	/// Returns the primitive handle 
	///
	operator hawk_t* () const
	{
		return this->hawk;
	}

	operator hawk_gem_t* () const
	{
		return this->hawk? hawk_getgem(this->hawk): HAWK_NULL;
	}

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
		const hawk_bch_t*  name,  ///< function name
		Value*             ret,   ///< return value holder
		const Value*       args,  ///< argument array
		hawk_oow_t         nargs  ///< number of arguments
	);

	///
	/// The call() function invokes a function named \a name.
	///
	int call (
		const hawk_uch_t*  name,  ///< function name
		Value*             ret,   ///< return value holder
		const Value*       args,  ///< argument array
		hawk_oow_t         nargs  ///< number of arguments
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
		hawk_oow_t depth ///< new depth
	);

	///
	/// The getMaxDepth() function gets the maximum depth for an operation
	/// type identified by \a id.
	///
	hawk_oow_t getMaxDepth (
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
		const hawk_uch_t* arg,  ///< string pointer
		hawk_oow_t        len   ///< string length
	);

	int addArgument (
		const hawk_bch_t* arg,  ///< string pointer
		hawk_oow_t        len   ///< string length
	);

	///
	/// The addArgument() function adds a null-terminated string \a arg. 
	/// loop() and call() make a string added available to a script 
	/// through ARGV. 
	/// \return 0 on success, -1 on failure
	///
	int addArgument (
		const hawk_uch_t* arg ///< string pointer
	);

	int addArgument (
		const hawk_bch_t* arg ///< string pointer
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
	/// addGlobal() or one of the #hawk_gbl_id_t enumerators. It is not allowed
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
	/// addGlobal() or one of the #hawk_gbl_id_t enumerators. It is not allowed
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
		hawk_oow_t        nargs, 
		const hawk_fnc_info_t* fi
	);

	/// 
	/// The addFunction() function adds a new user-defined intrinsic 
	/// function.
	///
	int addFunction (
		const hawk_bch_t* name,      ///< function name
		hawk_oow_t minArgs,               ///< minimum numbers of arguments
		hawk_oow_t maxArgs,               ///< maximum numbers of arguments
		const hawk_bch_t* argSpec,   ///< argument specification
		FunctionHandler handler,      ///< function handler
		int    validOpts = 0          ///< valid if these options are set
	);

	int addFunction (
		const hawk_uch_t* name,      ///< function name
		hawk_oow_t minArgs,               ///< minimum numbers of arguments
		hawk_oow_t maxArgs,               ///< maximum numbers of arguments
		const hawk_uch_t* argSpec,   ///< argument specification
		FunctionHandler handler,      ///< function handler
		int    validOpts = 0          ///< valid if these options are set
	);

	///
	/// The deleteFunction() function deletes a user-defined intrinsic 
	/// function by name.
	///
	int deleteFunction (
		const hawk_ooch_t* name ///< function name
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

	virtual hawk_ooi_t readPipe  (Pipe& io, hawk_ooch_t* buf, hawk_oow_t len);
	virtual hawk_ooi_t writePipe (Pipe& io, const hawk_ooch_t* buf, hawk_oow_t len);
	virtual hawk_ooi_t writePipeBytes (Pipe& io, const hawk_bch_t* buf, hawk_oow_t len);
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
	virtual hawk_ooi_t readFile  (File& io, hawk_ooch_t* buf, hawk_oow_t len);
	virtual hawk_ooi_t writeFile (File& io, const hawk_ooch_t* buf, hawk_oow_t len);
	virtual hawk_ooi_t writeFileBytes (File& io, const hawk_bch_t* buf, hawk_oow_t len);
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
	virtual hawk_ooi_t readConsole  (Console& io, hawk_ooch_t* buf, hawk_oow_t len);
	virtual hawk_ooi_t writeConsole (Console& io, const hawk_ooch_t* buf, hawk_oow_t len);
	virtual hawk_ooi_t writeConsoleBytes (Console& io, const hawk_bch_t* buf, hawk_oow_t len);
	virtual int     flushConsole (Console& io);
	virtual int     nextConsole  (Console& io);
	/// \}

	// primitive handlers 
	virtual hawk_flt_t pow (hawk_flt_t x, hawk_flt_t y) = 0;
	virtual hawk_flt_t mod (hawk_flt_t x, hawk_flt_t y) = 0;

	virtual void* modopen (const hawk_mod_spec_t* spec) = 0;
	virtual void  modclose (void* handle) = 0;
	virtual void* modgetsym (void* handle, const hawk_ooch_t* name) = 0;

	// static glue members for various handlers
	static hawk_ooi_t readSource (
		hawk_t* awk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg,
		hawk_ooch_t* data, hawk_oow_t count);
	static hawk_ooi_t writeSource (
		hawk_t* awk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg,
		hawk_ooch_t* data, hawk_oow_t count);

	static hawk_ooi_t pipeHandler (
		hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod,
		void* data, hawk_oow_t count);
	static hawk_ooi_t fileHandler (
		hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod,
		void* data, hawk_oow_t count);
	static hawk_ooi_t consoleHandler (
		hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod,
		void* data, hawk_oow_t count);

	static int functionHandler (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);


	static hawk_flt_t pow (hawk_t* awk, hawk_flt_t x, hawk_flt_t y);
	static hawk_flt_t mod (hawk_t* awk, hawk_flt_t x, hawk_flt_t y);

	static void* modopen (hawk_t* awk, const hawk_mod_spec_t* spec);
	static void  modclose (hawk_t* awk, void* handle);
	static void* modgetsym (hawk_t* awk, void* handle, const hawk_ooch_t* name);

public:
	// use this with care
	hawk_t* getHandle() const { return this->hawk; }

protected:
	hawk_t* hawk;

	hawk_errstr_t dflerrstr;
	hawk_errinf_t errinf;
#if defined(HAWK_OOCH_IS_BCH)
	mutable hawk_uch_t xerrmsg[HAWK_ERRMSG_CAPA];
	mutable hawk_uch_t xerrlocfile[128];
#else
	mutable hawk_bch_t xerrmsg[HAWK_ERRMSG_CAPA * 2];
	mutable hawk_bch_t xerrlocfile[256];
#endif

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	hawk_htb_t* functionMap;
#else

	class FunctionMap: public HashTable<Cstr,FunctionHandler>
	{
	public:
		FunctionMap (Hawk* awk): hawk(hawk) {}

	protected:
		Hawk* hawk;
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

		int add (hawk_t* awk, const hawk_uch_t* arg, hawk_oow_t len);
		int add (hawk_t* awk, const hawk_bch_t* arg, hawk_oow_t len);

		void clear (hawk_t* awk);

		hawk_oocs_t* ptr;
		hawk_oow_t   len;
		hawk_oow_t   capa;
	};

	xstrs_t runarg;

private:
	Run runctx;

	int init_runctx ();
	void fini_runctx ();
	int dispatch_function (Run* run, const hawk_fnc_info_t* fi);

	static const hawk_ooch_t* xerrstr (hawk_t* a, hawk_errnum_t num);
};

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////


HAWK_EXPORT void* operator new (hawk_oow_t size, HAWK::Mmgr* mmgr);

#if defined(HAWK_CPP_NO_OPERATOR_DELETE_OVERLOADING)
HAWK_EXPORT void hawk_operator_delete (void* ptr, HAWK::Mmgr* mmgr);
#else
HAWK_EXPORT void operator delete (void* ptr, HAWK::Mmgr* mmgr);
#endif

HAWK_EXPORT void* operator new (hawk_oow_t size, HAWK::Mmgr* mmgr, void* existing_ptr);

#if 0
// i found no way to delete an array allocated with
// the placement new operator. if the array element is an instance
// of a class, the pointer returned by the new operator call
// may not be the actual pointer allocated. some compilers
// seem to put some information about the array at the 
// beginning of the allocated memory and return the pointer
// off a few bytes from the beginning.
void* operator new[] (hawk_oow_t size, HAWK::Mmgr* mmgr);
void operator delete[] (void* ptr, HAWK::Mmgr* mmgr);
#endif

#define HAWK_CPP_DELETE_WITH_MMGR(ptr, class_name, mmgr) \
	do { \
		HAWK_CPP_CALL_DESTRUCTOR (ptr, class_name); \
		HAWK_CPP_CALL_PLACEMENT_DELETE1(ptr, mmgr); \
	} while(0); 

#endif
