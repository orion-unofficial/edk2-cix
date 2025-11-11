#include <platform_def.h>
#include <common/debug.h>
#include <sky1_plat.h>

typedef enum {
	TZC400_NSAID_DEFAULT = 0,
	TZC400_NASID_VPU0,
	TZC400_NASID_VPU1,
	TZC400_NASID_VPU2,
	TZC400_NASID_GPU,
	TZC400_NASID_DPU,
	TZC400_NASID_ISP,
	TZC400_NASID_NPU,
	TZC400_NASID_TBD1,
	TZC400_NASID_TBD2,
	TZC400_NASID_TBD3,
	TZC400_NASID_TBD4,
	TZC400_NASID_TBD5,
	TZC400_NASID_FCH,
	TZC400_NASID_HIFI,
	TZC400_NASID_SFU
} tzc400_nsaid_e;

#define NASID_MASK (0xFUL)

#define RCSU_GPU_NSAID_REG   (GPU_RCSU_BASE_REG + 0x304UL)
#define RCSU_NPU_NSAID_1_REG (NPU_RCSU_BASE_REG + 0x300UL)
#define RCSU_NPU_NSAID_2_REG (NPU_RCSU_BASE_REG + 0x308UL)


static void nsaid_set_value(uint32_t addr, uint32_t id)
{
	uint32_t val;

	if (addr == 0) {
		INFO("nsaid addr error\n");
		return;
	}

	val = mmio_read_32(addr);

	val &= ~(NASID_MASK);

	val |= id;

	mmio_write_32(addr, val);

	return;
}

void cix_sky1_nsaid_setting_init(void)
{
	//nsaid_set_value(RCSU_GPU_NSAID_REG, TZC400_NASID_GPU);
	nsaid_set_value(RCSU_NPU_NSAID_1_REG, TZC400_NASID_NPU);
	nsaid_set_value(RCSU_NPU_NSAID_2_REG, TZC400_NASID_NPU);

	return;
}
