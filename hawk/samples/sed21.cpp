#include <Hawk-Sed.hpp>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>

static void print_error (const hawk_bch_t* fmt, ...)
{
	va_list va;
	fprintf (stderr, "ERROR: ");
	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);
}

static void print_error (HAWK::SedStd& sed)
{
	hawk_errnum_t code = sed.getErrorNumber();
	hawk_loc_t loc = sed.getErrorLocation();
	print_error ("code %d line %lu column %lu - %s\n", (int)code, (unsigned long int)loc.line, (unsigned long int)loc.colm, sed.getErrorMessageB());
}

int execute_sed (int argc, hawk_bch_t* argv[])
{
	if (argc <  2 || argc > 4)
	{
		print_error ("USAGE: %s command-string [input-file [output-file]]\n", argv[0]);
		return -1;
	}

	HAWK::SedStd sed;

	if (sed.open() <= -1)
	{
		print_error (sed);
		return -1;
	}

	HAWK::SedStd::StringStream sstream (argv[1]);
	if (sed.compile(sstream) <= -1)
	{
		print_error (sed);
		sed.close ();
		return -1;
	}

	hawk_bch_t* infile = (argc >= 3)? argv[2]: HAWK_NULL;
	hawk_bch_t* outfile = (argc >= 4)? argv[3]: HAWK_NULL;
	HAWK::SedStd::FileStream ifstream (infile);
	HAWK::SedStd::FileStream ofstream (outfile);

	if (sed.execute(ifstream, ofstream) <= -1)
	{
		print_error (sed);
		sed.close ();
		return -1;
	}

#if 0
	HAWK::SedStd::StringStream istream2 ("xxxxxxxxxCCCQQQQ ZZZZ hello");
	HAWK::SedStd::StringStream ostream2;
	if (sed.execute(istream2, ostream2) <= -1)
	{
		print_error (sed);
		sed.close ();
		return -1;
	}
	hawk_oow_t len;
	const hawk_bch_t* tmp = ostream2.getOutputB(&len);
	printf ("[%.*s]\n", (int)len, tmp);

	{
		HAWK::SedStd::StringStream sstream ("s|CCC|BBBBBBBBBBB|g");
		if (sed.compile(sstream) <= -1)
		{
			print_error (sed);
			sed.close ();
			return -1;
		}

		HAWK::SedStd::FileStream of("/dev/stdout");
		if (sed.execute(ostream2, of) <= -1)
		{
			print_error (sed);
			sed.close ();
			return -1;
		}
	}
#endif


	sed.close ();
	return 0;
}

int main (int argc, hawk_bch_t* argv[])
{
	int ret;

#if defined(_WIN32)
	char locale[100];
	UINT codepage;
	/* nothing special */
#endif

#if defined(_WIN32)
	codepage = GetConsoleOutputCP();
	if (codepage == CP_UTF8)
	{
		/*SetConsoleOUtputCP (CP_UTF8);*/
		/*hawk_setdflcmgrbyid (HAWK_CMGR_UTF8);*/
	}
	else
	{
		/* .codepage */
		hawk_fmt_uintmax_to_bcstr (locale, HAWK_COUNTOF(locale), codepage, 10, -1, '\0', ".");
		setlocale (LC_ALL, locale);
		/* hawk_setdflcmgrbyid (HAWK_CMGR_SLMB); */
	}

#else
	setlocale (LC_ALL, "");
	/* hawk_setdflcmgrbyid (HAWK_CMGR_SLMB); */
#endif

	ret = execute_sed(argc, argv);

	return ret;
}
