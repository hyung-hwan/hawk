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

	vi.setInt(1010);
	OK_X(vi.getType() == HAWK_VAL_INT);

	v.setMbs("hello", 5);
	OK_X(v.getType() == HAWK_VAL_MBS);

	v.setArrayedVal(4, vi);
	OK_X(v.getType() == HAWK_VAL_ARR);
	OK_X(v.getArrayedSize() == 5);
	OK_X(v.getArrayedLength() == 1);

	// i must not call hawk.close() to ensure that v is destroyed first.
	//hawk.close();
}

int main()
{
	no_plan ();

	test1();

	return exit_status();
}

