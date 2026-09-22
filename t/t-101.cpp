#include <Hawk.hpp>
#include <stdio.h>
#include "tap.h"

#define OK_X(test) OK(test, #test)

static void test1()
{
	HAWK::HawkStd hawk;
	HAWK::Hawk::Run* rtx;
	int n;

	n = hawk.open();
	OK_X(n == 0);

	HAWK::HawkStd::SourceString in("BEGIN{}");
	rtx = hawk.parse(in, HAWK::Hawk::Source::NONE);
	OK_X(rtx != HAWK_NULL);

	HAWK::Hawk::Value v(rtx);
	HAWK::Hawk::Value vi(rtx);
	HAWK::Hawk::Value vx(rtx);
	hawk_int_t vvx;

	vi.setInt(1010);
	OK_X(vi.getType() == HAWK_VAL_INT);

	v.setMbs("hello", 5);
	OK_X(v.getType() == HAWK_VAL_MBS);

	v.setArrayedVal(4, vi);
	OK_X(v.getType() == HAWK_VAL_ARR);
	OK_X(v.getArrayedSize() == 5);
	OK_X(v.getArrayedLength() == 1);
	// no way to know the capacity as it's internally set
	OK_X(v.getArrayed(4, &vx) == 0);
	OK_X(vx.getType() == HAWK_VAL_INT && vx.getInt(&vvx) == 0);
	OK_X(vvx == 1010);

	OK_X(v.scaleArrayed(10) == 0);
	OK_X(v.getType() == HAWK_VAL_ARR);
	OK_X(v.getArrayedSize() == 5);
	OK_X(v.getArrayedLength() == 1);
	OK_X(v.getArrayedCapa() == 10); // capacity must be known after it's set explicitly set

	HAWK::Hawk::Value x(rtx);
	OK_X(x.scaleArrayed(3) == 0);
	OK_X(x.getArrayedSize() == 0);
	OK_X(x.getArrayedLength() == 0);
	OK_X(x.getArrayedCapa() == 3);

	// i must not call hawk.close() to ensure that v is destroyed first.
	//hawk.close();
}

static int write_text_file (const char* name, const char* text)
{
	FILE* fp = fopen(name, "w");
	if (!fp) return -1;
	if (fputs(text, fp) == EOF)
	{
		fclose(fp);
		return -1;
	}
	return fclose(fp);
}

static void test_source_include_identity ()
{
	static const char inc1[] = "t-101-inc-1.tmp";
	static const char inc2[] = "t-101-inc-2.tmp";
	static const char main_file[] = "t-101-main.tmp";
	static const char script[] =
		"@pragma implicit off\n"
		"@include_once \"t-101-inc-1.tmp\";\n"
		"@include_once \"./t-101-inc-1.tmp\";\n"
		"@include_once \"t-101-inc-2.tmp\";\n"
		"BEGIN { cpp_inc_1(); cpp_inc_2(); }\n";

	OK_X(write_text_file(inc1, "function cpp_inc_1() { return 1; }\n") == 0);
	OK_X(write_text_file(inc2, "function cpp_inc_2() { return 2; }\n") == 0);
	OK_X(write_text_file(main_file, script) == 0);

	{
		HAWK::HawkStd hawk;
		HAWK::Hawk::Run* rtx;
		HAWK::HawkStd::SourceString in(script);

		OK_X(hawk.open() == 0);
		rtx = hawk.parse(in, HAWK::Hawk::Source::NONE);
		OK_X(rtx != HAWK_NULL);
	}

	{
		HAWK::HawkStd hawk;
		HAWK::Hawk::Run* rtx;
		HAWK::HawkStd::SourceFile in(main_file);

		OK_X(hawk.open() == 0);
		rtx = hawk.parse(in, HAWK::Hawk::Source::NONE);
		OK_X(rtx != HAWK_NULL);
	}

	remove(main_file);
	remove(inc2);
	remove(inc1);
}

int main()
{
	no_plan ();

	test1();
	test_source_include_identity();

	return exit_status();
}
