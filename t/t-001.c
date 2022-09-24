/* test endian conversion macros */

#include <hawk-utl.h>
#include <stdio.h>
#include "tap.h"

int main ()
{
	no_plan();

	{
		union {
			hawk_uint16_t u16;
			hawk_uint8_t arr[2];
		} x;

		x.arr[0] = 0x11;
		x.arr[1] = 0x22;

		/*
		printf("x.u16 = 0x%04x\n", x.u16);
		printf("htole16(x.u16) = 0x%04x\n", hawk_htole16(x.u16));
		printf("htobe16(x.u16) = 0x%04x\n", hawk_htobe16(x.u16));
		*/

		OK (x.u16 != hawk_htole16(x.u16) || x.u16 != hawk_htobe16(x.u16), "u16 endian conversion #0");
		OK (x.u16 == hawk_le16toh(hawk_htole16(x.u16)), "u16 endian conversion #1");
		OK (x.u16 == hawk_be16toh(hawk_htobe16(x.u16)), "u16 endian conversion #2");
		OK (x.u16 == hawk_ntoh16(hawk_hton16(x.u16)), "u16 endian conversion #3");

		#define X_CONST (0x1122)
		OK (X_CONST != HAWK_CONST_HTOLE16(X_CONST) || X_CONST != HAWK_CONST_HTOBE16(X_CONST), "u16 constant endian conversion #0");
		OK (X_CONST == HAWK_CONST_LE16TOH(HAWK_CONST_HTOLE16(X_CONST)), "u16 constant endian conversion #1");
		OK (X_CONST == HAWK_CONST_BE16TOH(HAWK_CONST_HTOBE16(X_CONST)), "u16 constant endian conversion #2");
		OK (X_CONST == HAWK_CONST_NTOH16(HAWK_CONST_HTON16(X_CONST)), "u16 constant endian conversion #3");
		#undef X_CONST
	}


	{
		union {
			hawk_uint32_t u32;
			hawk_uint8_t arr[4];
		} x;

		x.arr[0] = 0x11;
		x.arr[1] = 0x22;
		x.arr[2] = 0x33;
		x.arr[3] = 0x44;

		/*
		printf("x.u32 = 0x%08x\n", (unsigned int)x.u32);
		printf("htole32(x.u32) = 0x%08x\n", (unsigned int)hawk_htole32(x.u32));
		printf("htobe32(x.u32) = 0x%08x\n", (unsigned int)hawk_htobe32(x.u32));
		*/

		OK (x.u32 != hawk_htole32(x.u32) || x.u32 != hawk_htobe32(x.u32), "u32 endian conversion #0");
		OK (x.u32 == hawk_le32toh(hawk_htole32(x.u32)), "u32 endian conversion #1");
		OK (x.u32 == hawk_be32toh(hawk_htobe32(x.u32)), "u32 endian conversion #2");
		OK (x.u32 == hawk_ntoh32(hawk_hton32(x.u32)), "u32 endian conversion #3");

		#define X_CONST (0x11223344)
		OK (X_CONST != HAWK_CONST_HTOLE32(X_CONST) || X_CONST != HAWK_CONST_HTOBE32(X_CONST), "u32 constant endian conversion #0");
		OK (X_CONST == HAWK_CONST_LE32TOH(HAWK_CONST_HTOLE32(X_CONST)), "u32 constant endian conversion #1");
		OK (X_CONST == HAWK_CONST_BE32TOH(HAWK_CONST_HTOBE32(X_CONST)), "u32 constant endian conversion #2");
		OK (X_CONST == HAWK_CONST_NTOH32(HAWK_CONST_HTON32(X_CONST)), "u32 constant endian conversion #3");
		#undef X_CONST
	}

#if defined(HAWK_HAVE_UINT64_T)
	{
		union {
			hawk_uint64_t u64;
			hawk_uint8_t arr[8];
		} x;

		x.arr[0] = 0x11;
		x.arr[1] = 0x22;
		x.arr[2] = 0x33;
		x.arr[3] = 0x44;
		x.arr[4] = 0x55;
		x.arr[5] = 0x66;
		x.arr[6] = 0x77;
		x.arr[7] = 0x88;

		/*
		printf("x.u64 = 0x%016llx\n", (unsigned long long)x.u64);
		printf("htole64(x.u64) = 0x%016llx\n", (unsigned long long)hawk_htole64(x.u64));
		printf("htobe64(x.u64) = 0x%016llx\n", (unsigned long long)hawk_htobe64(x.u64));
		*/

		OK (x.u64 != hawk_htole64(x.u64) || x.u64 != hawk_htobe64(x.u64), "u64 endian conversion #0");
		OK (x.u64 == hawk_le64toh(hawk_htole64(x.u64)), "u64 endian conversion #1");
		OK (x.u64 == hawk_be64toh(hawk_htobe64(x.u64)), "u64 endian conversion #2");
		OK (x.u64 == hawk_ntoh64(hawk_hton64(x.u64)), "u64 endian conversion #3");

		#define X_CONST (((hawk_uint64_t)0x11223344 << 32) | (hawk_uint64_t)0x55667788)
		OK (X_CONST != HAWK_CONST_HTOLE64(X_CONST) || X_CONST != HAWK_CONST_HTOBE64(X_CONST), "u64 constant endian conversion #0");
		OK (X_CONST == HAWK_CONST_LE64TOH(HAWK_CONST_HTOLE64(X_CONST)), "u64 constant endian conversion #1");
		OK (X_CONST == HAWK_CONST_BE64TOH(HAWK_CONST_HTOBE64(X_CONST)), "u64 constant endian conversion #2");
		OK (X_CONST == HAWK_CONST_NTOH64(HAWK_CONST_HTON64(X_CONST)), "u64 constant endian conversion #3");
		#undef X_CONST
	}
#endif

#if defined(HAWK_HAVE_UINT128_T)
	{
		union {
			hawk_uint128_t u128;
			hawk_uint8_t arr[16];
		} x;
		hawk_uint128_t tmp;

		x.arr[0] = 0x11;
		x.arr[1] = 0x22;
		x.arr[2] = 0x33;
		x.arr[3] = 0x44;
		x.arr[4] = 0x55;
		x.arr[5] = 0x66;
		x.arr[6] = 0x77;
		x.arr[7] = 0x88;
		x.arr[8] = 0x99;
		x.arr[9] = 0xaa;
		x.arr[10] = 0xbb;
		x.arr[11] = 0xcc;
		x.arr[12] = 0xdd;
		x.arr[13] = 0xee;
		x.arr[14] = 0xff;
		x.arr[15] = 0xfa;

		/*
		printf("x.u128 = 0x%016llx%016llx\n", (unsigned long long)(hawk_uint64_t)(x.u128 >> 64), (unsigned long long)(hawk_uint64_t)(x.u128 >> 0));

		tmp = hawk_htole128(x.u128);
		printf("htole128(tmp) = 0x%016llx%016llx\n", (unsigned long long)(hawk_uint64_t)(tmp >> 64), (unsigned long long)(hawk_uint64_t)(tmp >> 0));

		tmp = hawk_htobe128(x.u128);
		printf("htobe128(tmp) = 0x%016llx%016llx\n", (unsigned long long)(hawk_uint64_t)(tmp >> 64), (unsigned long long)(hawk_uint64_t)(tmp >> 0));
		*/

		OK (x.u128 != hawk_htole128(x.u128) || x.u128 != hawk_htobe128(x.u128), "u128 endian conversion #0");
		OK (x.u128 == hawk_le128toh(hawk_htole128(x.u128)), "u128 endian conversion #1");
		OK (x.u128 == hawk_be128toh(hawk_htobe128(x.u128)), "u128 endian conversion #2");
		OK (x.u128 == hawk_ntoh128(hawk_hton128(x.u128)), "u128 endian conversion #3");

		#define X_CONST (((hawk_uint128_t)0x11223344 << 96) | ((hawk_uint128_t)0x55667788 << 64) | ((hawk_uint128_t)0x99aabbcc << 32)  | ((hawk_uint128_t)0xddeefffa))
		OK (X_CONST != HAWK_CONST_HTOLE128(X_CONST) || X_CONST != HAWK_CONST_HTOBE128(X_CONST), "u128 constant endian conversion #0");
		OK (X_CONST == HAWK_CONST_LE128TOH(HAWK_CONST_HTOLE128(X_CONST)), "u128 constant endian conversion #1");
		OK (X_CONST == HAWK_CONST_BE128TOH(HAWK_CONST_HTOBE128(X_CONST)), "u128 constant endian conversion #2");
		OK (X_CONST == HAWK_CONST_NTOH128(HAWK_CONST_HTON128(X_CONST)), "u128 constant endian conversion #3");
		#undef X_CONST
	}
#endif

	return exit_status();
}
