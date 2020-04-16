#ifndef _HAWK_T_T_H_
#define _HAWK_T_T_H_

#include <stdio.h>

#if defined(__STDC_VERSION__) && (__STDC_VERSION__>=199901L)
#	define T_ASSERT_FAIL0() printf("FAILURE in %s:%s[%d]\n", __FILE__, __func__, (int)__LINE__)
#	define T_ASSERT_FAIL1(msg1) printf("FAILURE in %s:%s[%d] - %s\n", __FILE__, __func__, (int)__LINE__, msg1)
#	define T_ASSERT_FAIL2(msg1,msg2) printf("FAILURE in %s:%s[%d] - %s - %s\n", __FILE__, __func__, (int)__LINE__, msg1, msg2)
#else
#	define T_ASSERT_FAIL0() printf("FAILURE in %s[%d]\n", __FILE__, (int)__LINE__)
#	define T_ASSERT_FAIL1(msg1) printf("FAILURE in %s[%d] - %s\n", __FILE__, (int)__LINE__, msg1)
#	define T_ASSERT_FAIL2(msg1,msg2) printf("FAILURE in %s[%d] - %s - %s\n", __FILE__, (int)__LINE__, msg1, msg2)
#endif

#define T_ASSERT0(test) do { if (!(test)) { T_ASSERT_FAIL0(); goto oops; } } while(0)
#define T_ASSERT1(test,msg1) do { if (!(test)) { T_ASSERT_FAIL1(msg1); goto oops; } } while(0)
#define T_ASSERT2(test,msg1,msg2) do { if (!(test)) { T_ASSERT_FAIL2(msg1,msg2); goto oops; } } while(0)


#endif
