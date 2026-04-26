#include <lib/mmio.h>
#include  "hwspinlock.h"
#include <stdbool.h>

#define HW_SPINLOCK_BASE 0x06510000

/* Number of Hardware Spinlocks*/
#define HWSPINLOCK_NUMBER_MAX       100

/* Hardware spinlock register offsets */
#define HWSPINLOCK_OFFSET(x)    (0x900 + 0x4 * (x))

#define HWSPINLOCK_OWNER_ID     0x01



int sky1_hwspinlock_trylock(uint8_t mutex_idx, uint32_t timeout_us)
{
	uint32_t mutex_addr = HW_SPINLOCK_BASE + HWSPINLOCK_OFFSET(mutex_idx);

	if (mutex_idx >= HWSPINLOCK_NUMBER_MAX)
		return false;

	while (1) {
		mmio_write_32(mutex_addr, HWSPINLOCK_OWNER_ID);

		if (HWSPINLOCK_OWNER_ID == (mmio_read_32(mutex_addr) & 0xFF))
			return true;

		if (timeout_us) {
			timeout_us--;
			if (timeout_us == 0x0)
				return false;
		}
	}

	return false;
}

void sky1_hwspinlock_unlock(uint8_t mutex_idx)
{
	uint32_t mutex_addr = HW_SPINLOCK_BASE + HWSPINLOCK_OFFSET(mutex_idx);

	if (mutex_idx >= HWSPINLOCK_NUMBER_MAX)
		return;

	if ((mmio_read_32(mutex_addr) & 0xFF) == HWSPINLOCK_OWNER_ID)
		mmio_write_32(mutex_addr, HWSPINLOCK_OWNER_ID);
}
