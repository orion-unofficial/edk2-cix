// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2016, Linaro Limited
 * Copyright (c) 2014, STMicroelectronics International N.V.
 */

#include <kernel/panic.h>
#include <kernel/thread.h>
#include <kernel/unwind.h>
#include <trace.h>
#include <mm/core_memprot.h>
#include <printk.h>
#include <arm.h>
#include <kernel/linker.h>

#include <unw/unwind.h>

//#include "unwind_private.h"

void dump_stack_arm64(struct unwind_state_arm64 *state,
		       vaddr_t stack, size_t stack_size);
void trace_panic_vprintf(const char *function, int line, int level, bool level_ok,
		   const char *fmt, va_list ap);
void trace_panic_printf(const char *func, int line, int level, bool level_ok,
		  const char *fmt, ...);

#define trace_printf_panic_raw(level, level_ok, ...) \
	trace_panic_printf(NULL, 0, (level), (level_ok), __VA_ARGS__)


#define PANIC_RAW(...)   trace_printf_panic_raw(TRACE_ERROR, true, __VA_ARGS__)


static size_t panic_buf_offset = 0;

static char log_level_to_string(int level, bool level_ok)
{
	/*
	 * U = Unused
	 * E = Error
	 * I = Information
	 * D = Debug
	 * F = Flow
	 */
	static const char lvl_strs[] = { 'U', 'E', 'I', 'D', 'F' };
	int l = 0;

	if (!level_ok)
		return 'M';

	if ((level >= TRACE_MIN) && (level <= TRACE_MAX))
		l = level;

	return lvl_strs[l];
}

static int get_core_id(char *buf, size_t bs)
{
#if CFG_TEE_CORE_NB_CORE > 100
	const int num_digits = 3;
	const char qm[] = "???";
#elif CFG_TEE_CORE_NB_CORE > 10
	const int num_digits = 2;
	const char qm[] = "??";
#else
	const int num_digits = 1;
	const char qm[] = "?";
#endif
	int core_id = trace_ext_get_core_id();

	if (core_id >= 0)
		return snprintk(buf, bs, "%0*u ", num_digits, core_id);
	else
		return snprintk(buf, bs, "%s ", qm);
}

static int get_thread_id(char *buf, size_t bs)
{
#if CFG_NUM_THREADS > 100
	int num_thread_digits = 3;
#elif CFG_NUM_THREADS > 10
	int num_thread_digits = 2;
#else
	int num_thread_digits = 1;
#endif
	int thread_id = trace_ext_get_thread_id();

	if (thread_id >= 0)
		return snprintk(buf, bs, "%0*d ", num_thread_digits, thread_id);
	else
		return snprintk(buf, bs, "%*s ", num_thread_digits, "");
}



void trace_panic_vprintf(const char *function, int line, int level, bool level_ok,
		   const char *fmt, va_list ap)
{
	char* panic_buf = phys_to_virt(TZ_TFA_SHM_BASE, MEM_AREA_RAM_SEC, 1);
	char* buf = &(panic_buf[panic_buf_offset]);
	size_t res_buf_size = TZ_TFA_SHM_SIZE - panic_buf_offset;
	size_t boffs = 0;
	int res;

	if (level_ok && level > trace_level)
		return;

	/* Print the type of message */
	res = snprintk(buf, res_buf_size, "%c/",
		       log_level_to_string(level, level_ok));
	if (res < 0)
		return;
	boffs += res;

	/* Print the location, i.e., TEE core or TA */
	res = snprintk(buf + boffs, res_buf_size - boffs, "%s:",
		       trace_ext_prefix);
	if (res < 0)
		return;
	boffs += res;

	if (level_ok && (BIT(level) & CFG_MSG_LONG_PREFIX_MASK)) {
		/* Print the core ID if in atomic context  */
		res = get_core_id(buf + boffs, res_buf_size - boffs);
		if (res < 0)
			return;
		boffs += res;

		/* Print the Thread ID */
		res = get_thread_id(buf + boffs, res_buf_size - boffs);
		if (res < 0)
			return;
		boffs += res;

		if (function) {
			res = snprintk(buf + boffs, res_buf_size - boffs, "%s:%d ",
				       function, line);
			if (res < 0)
				return;
			boffs += res;
		}
	} else {
		/* Add space after location info */
		if (boffs >= sizeof(buf) - 1)
		    return;
		buf[boffs++] = ' ';
		buf[boffs] = 0;
	}

	res = vsnprintk(buf + boffs, res_buf_size - boffs, fmt, ap);
	if (res > 0)
		boffs += res;

	if (boffs >= res_buf_size - 1)
		boffs = res_buf_size - 2;

	buf[boffs] = '\n';
	boffs++;
	panic_buf_offset = panic_buf_offset + boffs;
}

void trace_panic_printf(const char *function, int line, int level, bool level_ok,
		  const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	trace_panic_vprintf(function, line, level, level_ok, fmt, ap);
	va_end(ap);
}


void dump_stack_arm64(struct unwind_state_arm64 *state,
		       vaddr_t stack, size_t stack_size)
{
	int width = 8;

	PANIC_RAW("Call stack:");

	ftrace_map_lr(&state->pc);
	do {
		PANIC_RAW(" 0x%0*"PRIx64, width, state->pc);
	} while (unwind_stack_arm64(state, stack, stack_size));
}

static void dump_tee_panic(const char *file __maybe_unused,
		const int line __maybe_unused,
		const char *func __maybe_unused,
		const char *msg __maybe_unused)
{
	struct unwind_state_arm64 state = { };
	vaddr_t stack_start = 0;
	vaddr_t stack_end = 0;
	char* panic_buf = phys_to_virt(TZ_TFA_SHM_BASE, MEM_AREA_RAM_SEC, 1);

	/** 1) Dump header info */
	if (!file && !func && !msg)
		PANIC_RAW("Panic");
	else
		PANIC_RAW("Panic %s%s%sat %s:%d %s%s%s",
			 msg ? "'" : "", msg ? msg : "", msg ? "' " : "",
			 file ? file : "?", file ? line : 0,
			 func ? "<" : "", func ? func : "", func ? ">" : "");

	/** 2) Get pc & fp */
	state.pc = read_pc();
	state.fp = read_fp();

	PANIC_RAW("TEE load address @ %#"PRIxVA, VCORE_START_VA);
	get_stack_hard_limits(&stack_start, &stack_end);
	dump_stack_arm64(&state, stack_start, stack_end - stack_start);
	panic_buf[panic_buf_offset] = '\0';
}




void __do_panic(const char *file __maybe_unused,
		const int line __maybe_unused,
		const char *func __maybe_unused,
		const char *msg __maybe_unused)
{
	char* panic_buf = phys_to_virt(TZ_TFA_SHM_BASE, MEM_AREA_RAM_SEC, 1);

	/* disable prehemption */
	(void)thread_mask_exceptions(THREAD_EXCP_ALL);

	/* TODO: notify other cores */
	dump_tee_panic(file, line, func, msg);

	/* trace: Panic ['panic-string-message' ]at FILE:LINE [<FUNCTION>]" */
	if (!file && !func && !msg)
		EMSG_RAW("Panic");
	else
		EMSG_RAW("Panic %s%s%sat %s:%d %s%s%s",
			 msg ? "'" : "", msg ? msg : "", msg ? "' " : "",
			 file ? file : "?", file ? line : 0,
			 func ? "<" : "", func ? func : "", func ? ">" : "");

	print_kernel_stack();
	EMSG_RAW("Start to dump share buffer data: 0x%x", panic_buf_offset);
	/* abort current execution */
	EMSG_RAW("%s", panic_buf);
	tee_panic_smc(panic_buf_offset);
}

void __weak cpu_idle(void)
{
}
