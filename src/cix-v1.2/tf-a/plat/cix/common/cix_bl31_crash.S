#include <asm_macros.S>
#include <platform_def.h>
#include <lib/el3_runtime/cpu_data.h>

.globl	plat_panic_handler

func plat_panic_handler
	msr	spsel, #MODE_SP_EL0
	/*restore tpdir_el3 */
	mrs x0, tpidr_el3
	add	x0, x0, #-CPU_DATA_CRASH_BUF_OFFSET
	msr	tpidr_el3, x0
	no_ret cix_panic_handler
endfunc plat_panic_handler
