/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string_ext.h>
#include <trace.h>
#include <util.h>
#include <compiler.h>
#include <assert.h>
#include <mbedtls/bignum.h>
#include <kernel/panic.h>
#include <mempool.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

extern void mbedtls_platform_custom(void);

#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
#define MAX_PRINT_SIZE      256
__printf(1, 2) static int mbed_plat_printf( const char *fmt, ...)
{
	char to_format[MAX_PRINT_SIZE];
	va_list ap;
	int s;

	va_start(ap, fmt);
	s = vsnprintf(to_format, sizeof(to_format), fmt, ap);
	va_end(ap);

	if (s < 0)
		return s;

	trace_ext_puts(to_format);

	return s;
}
#endif

#if defined(MBEDTLS_CHECK_PARAMS)
void mbedtls_param_failed( const char *failure_condition,
                           const char *file,
                           int line )
{
	MSG("%s+%d: '%s' failed\n", file, line, failure_condition);
}

#endif

void mbedtls_platform_custom(void)
{
#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
	mbedtls_platform_set_printf(mbed_plat_printf);
	#pragma GCC diagnostic pop
#endif
}
#endif /* MBEDTLS_PLATFORM_C */

/* Size needed for xtest to pass reliably on both ARM32 and ARM64 */
#define MPI_MEMPOOL_SIZE	(46 * 1024)

#if defined(_CFG_CORE_LTC_PAGER)
/* allocate pageable_zi vmem for mp scratch memory pool */
static struct mempool *get_mp_scratch_memory_pool(void)
{
	size_t size;
	void *data;

	size = ROUNDUP(MPI_MEMPOOL_SIZE, SMALL_PAGE_SIZE);
	data = tee_pager_alloc(size);
	if (!data)
		panic();

	return mempool_alloc_pool(data, size, tee_pager_release_phys);
}
#else /* _CFG_CORE_LTC_PAGER */
static struct mempool *get_mp_scratch_memory_pool(void)
{
	static uint8_t data[MPI_MEMPOOL_SIZE] __aligned(MEMPOOL_ALIGN);
	return mempool_alloc_pool(data, sizeof(data), NULL);
}
#endif

void init_mp_mbedtls(void)
{
	struct mempool *p = get_mp_scratch_memory_pool();

	if (!p)
		panic();
	mbedtls_mpi_mempool = p;
	assert(!mempool_default);
	mempool_default = p;
}

