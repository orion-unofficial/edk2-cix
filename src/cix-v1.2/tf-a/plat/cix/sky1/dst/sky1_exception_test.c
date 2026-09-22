#include "common/debug.h"
#include <plat_cix.h>

#define SKY1_INVALID_ADDR_ACCESS_TEST '0'

static int sky1_invalid_addr_access_test(void)
{
	char *ptr = NULL;

	/*NULL POINT EXCEPTION*/
	*ptr = 0;
	return 0;
}

int sky1_exception_test(uint64_t key, uint64_t arg1, uint64_t arg2)
{
	int ret = 0;

	switch ((char)key) {
	case SKY1_INVALID_ADDR_ACCESS_TEST:
		ret = sky1_invalid_addr_access_test();
		break;

	default:
		INFO("unknown test key\n");
	}
	return ret;
}