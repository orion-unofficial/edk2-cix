#ifndef __RAM_LOG_PRINTF__
#define __RAM_LOG_PRINTF__

#include <stdarg.h>
#include <lib/utils_def.h>

#define RLOG_PUTC_ENABLED BIT(0)

typedef void (*LOCKFN)(void);
typedef unsigned long long (*GETTICK)(void);
typedef void (*FLUSHFN)(unsigned long addr, unsigned long size);

typedef struct {
	LOCKFN lock;
	LOCKFN unlock;
	GETTICK get_tick;
	FLUSHFN flush;
} ramlog_ops;

enum RLOG_LEVEL
{
    RLOGLEVEL_MIN = 1,
    RLOGLEVEL_ERR = RLOGLEVEL_MIN,	/* error conditions */
    RLOGLEVEL_WARNING,	/* warning conditions */
    RLOGLEVEL_INFO,	/* informational */
    RLOGLEVEL_DEBUG,	/* debug-level messages */
    RLOGLEVEL_MAX
};

void rlog_init_printf(char *buf, unsigned int size, ramlog_ops *ops);
void rlog_set_log_level(enum RLOG_LEVEL level);
void rlog_printf(enum RLOG_LEVEL level, const char* fmt, ...);
void rlog_printf_va(enum RLOG_LEVEL level, const char* fmt, va_list args);
void rlog_flush_data(void);
void rlog_enable_putc(unsigned char level);
unsigned char rlog_putc(unsigned char c);
void rlog_disable_putc(void);
#endif
