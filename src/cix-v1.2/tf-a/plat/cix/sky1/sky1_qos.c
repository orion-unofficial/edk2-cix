#include "include/platform_def.h"
#include <common/debug.h>
#include <lib/smccc.h>
#include <drivers/delay_timer.h>
#include <sky1_qos.h>

#define MMHUB_RESET_MASK BIT(4)
#define DISPLAY_RESET_MASK (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25))

#define DISPLAY_RESET_REG 0x16000400

typedef enum {
	MMHUB_CSI_SLV0,
	MMHUB_CSI_SLV1,
	MMHUB_CSI_SLV2,
	MMHUB_CSI_SLV3,

	MMHUB_DPU0_AFBC_SLV,
	MMHUB_DPU0_SLV0,
	MMHUB_DPU0_SLV1,
	MMHUB_DPU1_AFBC_SLV,
	MMHUB_DPU1_SLV0,
	MMHUB_DPU1_SLV1,
	MMHUB_DPU2_AFBC_SLV,
	MMHUB_DPU2_SLV0,
	MMHUB_DPU2_SLV1,
	MMHUB_DPU3_AFBC_SLV,
	MMHUB_DPU3_SLV0,
	MMHUB_DPU3_SLV1,
	MMHUB_DPU4_AFBC_SLV,
	MMHUB_DPU4_SLV0,
	MMHUB_DPU4_SLV1,

	MMHUB_ISP_AFBC_SLV,
	MMHUB_ISP_SLV0,
	MMHUB_ISP_SLV1,

	MMHUB_NPU_SLV0,
	MMHUB_NPU_SLV1,

	MMHUB_VPU_SLV0,
	MMHUB_VPU_SLV1,

	SYSHUB_AUIDO_SLV,
	SYSHUB_ETH0_SLV,
	SYSHUB_ETH1_SLV,
	SYSHUB_FCH_SLV,
	SYSHUB_USB2_0_SLV,
	SYSHUB_USB2_1_SLV,
	SYSHUB_USB2_2_SLV,
	SYSHUB_USB2_3_SLV,
	SYSHUB_USB3_0_SLV,
	SYSHUB_USB3_1_SLV,
	SYSHUB_USBC_0_SLV,
	SYSHUB_USBC_1_SLV,
	SYSHUB_USBC_2_SLV,
	SYSHUB_USBC_DRD_SLV,

	PCIEHUB_PCIE_X1_SLV0,
	PCIEHUB_PCIE_X1_SLV1,
	PCIEHUB_PCIE_X2_SLV,
	PCIEHUB_PCIE_X4_SLV,
	PCIEHUB_PCIE_X8_SLV,

	NI700_INVALID_QOS_ID = 0xFF,

}ni700_qos_id_t;

typedef enum {
	CI700_RNF0,
	CI700_RNF1,
	CI700_RNF2,
	CI700_RNF3,
	CI700_GPU_RNI64_S0,
	CI700_GPU_RNI64_S1,
	CI700_GPU_RNI72_S0,
	CI700_GPU_RNI72_S1,
	CI700_PCIE,
	CI700_SYSHUB_S0,
	CI700_SYSHUB_S1,
	CI700_SYSHUB_SMMU,
	CI700_DFD_TMC,
	CI700_MMHUB_SMMU,
	CI700_PCIEHUB_SMMU,

	CI700_INVALID_QOS_ID = 0xFF,
}ci700_qos_id_t;

typedef enum {
	PLAT_QOS_DIS,
	PLAT_QOS_ENA,
}ni700_qos_ena_t;

typedef enum {
	RNI_RND_NODE,
	RNF_NODE
}ci700_node_type_t;

typedef struct {
	const char *name;            /*element name*/
	uint8_t  bena;               /*ena override flag*/
	uint8_t  ci700_node_type;    /*rni-rnd or rnf*/
	uint8_t  qos_id;             /*qos id for current node*/
	uint8_t  rd_qos;             /*qos for read*/
	uint8_t  wr_qos;             /*qos for write*/
	uint8_t  rev[3];
	int  reg_addr;               /*reg-addr for qos override*/
}ci700_qos_setting_t;

typedef struct {
	char *name;                  /*element name*/
	uint8_t  bena;               /*ena override flag*/
	uint8_t  qos_id;             /*qos id for current node*/
	uint8_t  rd_qos;             /*qos for read*/
	uint8_t  wr_qos;             /*qos for write*/
	uint8_t  rev[4];
	int  reg_ena;                /*reg-addr for enable qos override*/
	int  reg_ar_qos;             /*reg-addr for config read qos value*/
	int  reg_aw_qos;             /*reg-addr for config write qos value*/
}ni700_qos_setting_t;

#define QOS_STR(x)  (#x)
#define QOS_NI700_NODE_INIT(_node_id,_override_ena,_rd_qos,_wr_qos,_reg_ena,_reg_ar_qos,_reg_aw_qos) \
{ \
	.name = #_node_id, \
	.bena = _override_ena, \
	.qos_id = _node_id, \
	.rd_qos = _rd_qos, \
	.wr_qos = _wr_qos, \
	.reg_ena = _reg_ena, \
	.reg_ar_qos = _reg_ar_qos, \
	.reg_aw_qos = _reg_aw_qos, \
}

#define QOS_CI700_NODE_INIT(_node_id,node_type,_override_ena,_rd_qos,_wr_qos,_reg_addr) \
{ \
	.name = #_node_id, \
	.bena = _override_ena, \
	.ci700_node_type = node_type, \
	.qos_id = _node_id, \
	.rd_qos = _rd_qos, \
	.wr_qos = _wr_qos, \
	.reg_addr = _reg_addr, \
}
static ni700_qos_setting_t g_sky1_ni700_qos_setting [ ] = {

	/*mmhub-ni700-ansi qos setting*/
	QOS_NI700_NODE_INIT(MMHUB_CSI_SLV0,     PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd044000+0x84,0xd044000+0x8c,0xd044000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_CSI_SLV1,     PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd045000+0x84,0xd045000+0x8c,0xd045000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_CSI_SLV0,     PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd046000+0x84,0xd046000+0x8c,0xd046000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_CSI_SLV0,     PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd047000+0x84,0xd047000+0x8c,0xd047000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_DPU0_AFBC_SLV,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd048000+0x84,0xd048000+0x8c,0xd048000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU0_SLV0,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd049000+0x84,0xd049000+0x8c,0xd049000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU0_SLV1,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd04a000+0x84,0xd04a000+0x8c,0xd04a000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_DPU1_AFBC_SLV,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd04b000+0x84,0xd04b000+0x8c,0xd04b000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU1_SLV0,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd04c000+0x84,0xd04c000+0x8c,0xd04c000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU1_SLV1,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd04d000+0x84,0xd04d000+0x8c,0xd04d000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_DPU2_AFBC_SLV,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd04e000+0x84,0xd04e000+0x8c,0xd04e000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU2_SLV0,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd04f000+0x84,0xd04f000+0x8c,0xd04f000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU2_SLV1,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd050000+0x84,0xd050000+0x8c,0xd050000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_DPU3_AFBC_SLV,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd051000+0x84,0xd051000+0x8c,0xd051000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU3_SLV0,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd052000+0x84,0xd052000+0x8c,0xd052000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU3_SLV1,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd053000+0x84,0xd053000+0x8c,0xd053000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_DPU4_AFBC_SLV,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd054000+0x84,0xd054000+0x8c,0xd054000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU4_SLV0,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd055000+0x84,0xd055000+0x8c,0xd055000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_DPU4_SLV1,    PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd056000+0x84,0xd056000+0x8c,0xd056000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_ISP_AFBC_SLV, PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd057000+0x84,0xd057000+0x8c,0xd057000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_ISP_SLV0,     PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd058000+0x84,0xd058000+0x8c,0xd058000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_ISP_SLV1,     PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd059000+0x84,0xd059000+0x8c,0xd059000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_NPU_SLV0,     PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0xd05a000+0x84,0xd05a000+0x8c,0xd05a000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_NPU_SLV1,     PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0xd05b000+0x84,0xd05b000+0x8c,0xd05b000+0x90),

	QOS_NI700_NODE_INIT(MMHUB_VPU_SLV0,     PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0xd05d000+0x84,0xd05d000+0x8c,0xd05d000+0x90),
	QOS_NI700_NODE_INIT(MMHUB_VPU_SLV1,     PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0xd05e000+0x84,0xd05e000+0x8c,0xd05e000+0x90),

	/*syshub-ni700-ansi qos setting*/
	QOS_NI700_NODE_INIT(SYSHUB_AUIDO_SLV,   PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd094000+0x84,0xd094000+0x8c,0xd094000+0x90),

	QOS_NI700_NODE_INIT(SYSHUB_USB2_0_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd09e000+0x84,0xd09e000+0x8c,0xd09e000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USB2_1_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd09f000+0x84,0xd09f000+0x8c,0xd09f000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USB2_2_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a0000+0x84,0xd0a0000+0x8c,0xd0a0000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USB2_3_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a1000+0x84,0xd0a1000+0x8c,0xd0a1000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USB3_0_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a2000+0x84,0xd0a2000+0x8c,0xd0a2000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USB3_1_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a3000+0x84,0xd0a3000+0x8c,0xd0a3000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USBC_0_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a4000+0x84,0xd0a4000+0x8c,0xd0a4000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USBC_1_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a5000+0x84,0xd0a5000+0x8c,0xd0a5000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USBC_2_SLV,  PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a6000+0x84,0xd0a6000+0x8c,0xd0a6000+0x90),
	QOS_NI700_NODE_INIT(SYSHUB_USBC_DRD_SLV,PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd0a7000+0x84,0xd0a7000+0x8c,0xd0a7000+0x90),

	/*pciepub-ni700-ansi qos setting*/
//	QOS_NI700_NODE_INIT(PCIEHUB_PCIE_X1_SLV0,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd016000+0x84,0xd016000+0x8c,0xd016000+0x90),
//	QOS_NI700_NODE_INIT(PCIEHUB_PCIE_X1_SLV1,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd017000+0x84,0xd017000+0x8c,0xd017000+0x90),
//	QOS_NI700_NODE_INIT(PCIEHUB_PCIE_X2_SLV, PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0xd018000+0x84,0xd018000+0x8c,0xd018000+0x90),
//	QOS_NI700_NODE_INIT(PCIEHUB_PCIE_X4_SLV, PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd019000+0x84,0xd019000+0x8c,0xd019000+0x90),
//	QOS_NI700_NODE_INIT(PCIEHUB_PCIE_X8_SLV, PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0xd01a000+0x84,0xd01a000+0x8c,0xd01a000+0x90),

	/*last node is a  non-valid node*/
	QOS_NI700_NODE_INIT(NI700_INVALID_QOS_ID,0,0/*ar_qos*/,0/*aw_qos*/,0,0,0),
};

static ci700_qos_setting_t g_sky1_ci700_qos_setting[] = {

	QOS_CI700_NODE_INIT(CI700_RNF0,        RNF_NODE,    PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0x10030A80),
	QOS_CI700_NODE_INIT(CI700_RNF1,        RNF_NODE,    PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0x10430A80),
	QOS_CI700_NODE_INIT(CI700_RNF2,        RNF_NODE,    PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0x11030A80),
	QOS_CI700_NODE_INIT(CI700_RNF3,        RNF_NODE,    PLAT_QOS_ENA,0xd/*ar_qos*/,0xd/*aw_qos*/,0x11430A80),

	QOS_CI700_NODE_INIT(CI700_GPU_RNI64_S0,RNI_RND_NODE,PLAT_QOS_ENA,0xb/*ar_qos*/,0xb/*aw_qos*/,0x12050A80),
	QOS_CI700_NODE_INIT(CI700_GPU_RNI64_S1,RNI_RND_NODE,PLAT_QOS_ENA,0xb/*ar_qos*/,0xb/*aw_qos*/,0x12050AA0),
	QOS_CI700_NODE_INIT(CI700_GPU_RNI72_S0,RNI_RND_NODE,PLAT_QOS_ENA,0xb/*ar_qos*/,0xb/*aw_qos*/,0x12450A80),
	QOS_CI700_NODE_INIT(CI700_GPU_RNI72_S1,RNI_RND_NODE,PLAT_QOS_ENA,0xb/*ar_qos*/,0xb/*aw_qos*/,0x12450AA0),

	QOS_CI700_NODE_INIT(CI700_PCIE,        RNI_RND_NODE,PLAT_QOS_ENA,0x8/*ar_qos*/,0x8/*aw_qos*/,0x13590A80),

	QOS_CI700_NODE_INIT(CI700_SYSHUB_SMMU, RNI_RND_NODE,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0x13110AC0),

	QOS_CI700_NODE_INIT(CI700_DFD_TMC,     RNI_RND_NODE,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0x13190AA0),

	QOS_CI700_NODE_INIT(CI700_MMHUB_SMMU,  RNI_RND_NODE,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0x13190AC0),

	QOS_CI700_NODE_INIT(CI700_PCIEHUB_SMMU,RNI_RND_NODE,PLAT_QOS_ENA,0xf/*ar_qos*/,0xf/*aw_qos*/,0x13520A80),

	/*last node is a  non-valid node*/
	QOS_CI700_NODE_INIT(CI700_INVALID_QOS_ID,0,0,0/*ar_qos*/,0/*aw_qos*/,0),
};

static void sky1_ni700_qos_init(ni700_qos_setting_t *p_qos_init_table)
{
	uint32_t val;
	ni700_qos_setting_t *pt = p_qos_init_table;

	while(pt->qos_id != NI700_INVALID_QOS_ID) {
		if(pt->bena) {

			/*enable qos override*/
			val = mmio_read_32(pt->reg_ena);
			val &=~ 0x1;
			val |=pt->bena & 0x1;
			mmio_write_32(pt->reg_ena,val);

			/*config read-qos*/
			val = mmio_read_32(pt->reg_ar_qos);
			val &=~ 0xf;
			val |=pt->rd_qos & 0xf;
			mmio_write_32(pt->reg_ar_qos,val);

			/*config write-qos*/
			val = mmio_read_32(pt->reg_aw_qos);
			val &=~ 0xf;
			val |=pt->wr_qos & 0xf;
			mmio_write_32(pt->reg_aw_qos,val);
		}
		pt++;
	}

	return;
}


static void sky1_ci700_qos_init(ci700_qos_setting_t *p_qos_init_table)
{
	uint32_t val;
	ci700_qos_setting_t *pt = p_qos_init_table;
	while(pt->qos_id != CI700_INVALID_QOS_ID) {
		if(pt->bena && pt->ci700_node_type == RNF_NODE) {
			/*enable qos override and config qos(rd_qos=wr_qos=config_qos)*/
			val = mmio_read_32(pt->reg_addr);
			val |=(0x1 << 2);
			val &=~(0xF << 16);
			val |= (pt->rd_qos & 0xF) << 16;
			mmio_write_32(pt->reg_addr,val);
		}else if(pt->bena && pt->ci700_node_type == RNI_RND_NODE) {
			/*enable qos override and config qos(rd_qos=wr_qos=config_qos)*/
			val = mmio_read_32(pt->reg_addr);
			val |=(0x3 << 2);
			val &=~(0xF << 16);
			val |= (pt->rd_qos & 0xF) << 16;
			val &=~(0xF << 20);
			val |= (pt->wr_qos & 0xF) << 20;
			mmio_write_32(pt->reg_addr,val);
		}
		pt++;
	}

	return;
}

void sky1_ni700_qos_setting_dump_all(ni700_qos_setting_t *p_qos_init_table)
{
	uint32_t val;
	ni700_qos_setting_t *pt = p_qos_init_table;
	uint32_t qos_ena,read_qos,write_qos;

	INFO("###########ni700-qos setting###########################\n");
	while(pt->qos_id != NI700_INVALID_QOS_ID) {
		val = mmio_read_32(pt->reg_ena);
		qos_ena = val & 0x1;

		val = mmio_read_32(pt->reg_ar_qos);
		read_qos=val & 0xf;
		mmio_write_32(pt->reg_ar_qos,val);

		val = mmio_read_32(pt->reg_aw_qos);
		write_qos = val &0xf;
		INFO("[%s]:override--%d read_qos--%02x write_qos--%02x \n",pt->name,qos_ena,read_qos,write_qos);
		pt++;
	}
	INFO("######################################################\n");
}

void sky1_ci700_qos_setting_dump_all(ci700_qos_setting_t *p_qos_init_table)
{
	uint32_t val;
	ci700_qos_setting_t *pt = p_qos_init_table;
	uint32_t qos_ena,qos_ena1,read_qos,write_qos;

	INFO("###########ci700-qos setting###########################\n");
	while(pt->qos_id != CI700_INVALID_QOS_ID) {
		if(pt->ci700_node_type == RNF_NODE) {
			val = mmio_read_32(pt->reg_addr);
			qos_ena =(val >> 2) & 0x1;
			read_qos = write_qos= (val >> 16) & 0xF;
			INFO("[%s]:override--%d read_qos--%02x write_qos--%02x \n",pt->name,qos_ena,read_qos,write_qos);
		}else if(pt->ci700_node_type == RNI_RND_NODE) {
			val = mmio_read_32(pt->reg_addr);
			qos_ena =(val >> 2) & 0x1;
			qos_ena1 =(val >> 3) & 0x1;
			read_qos = (val >> 16) & 0xF;
			write_qos = (val >> 20) & 0xF;
			INFO("[%s]:override[r-w]--[%01d-%01d] read_qos--%02x write_qos--%02x \n",pt->name,qos_ena,qos_ena1,read_qos,write_qos);
		}
		pt++;
	}
	INFO("######################################################\n");

	return;
}

void cix_sky1_qos_setting_init(void)
{
	sky1_ci700_qos_init((ci700_qos_setting_t*)&g_sky1_ci700_qos_setting);
	sky1_ni700_qos_init((ni700_qos_setting_t*)&g_sky1_ni700_qos_setting);

	return;
}

void cix_sky1_qos_setting_dump(void)
{
	sky1_ni700_qos_setting_dump_all((ni700_qos_setting_t*)&g_sky1_ni700_qos_setting);
	sky1_ci700_qos_setting_dump_all((ci700_qos_setting_t*)&g_sky1_ci700_qos_setting);

	return;
}

#define SMNHUB_ANMI_NODE_NUM (27)
#define SMNHUB_ANMI_NODE_BASE (0x0D040000UL + 0x4000)
uint8_t g_mmhub_port_config[SMNHUB_ANMI_NODE_NUM] = {0};

void cix_sky1_mmhub_config_save(void)
{
	uint32_t i = 0, val = 0;

	/* save amni port config */
	for (i = 0; i < SMNHUB_ANMI_NODE_NUM; i++) {
		/*rcsu node*/
		if ( i == 24 )
			continue;
		g_mmhub_port_config[i] = mmio_read_32(SMNHUB_ANMI_NODE_BASE + i * 0x1000 + 0x48);
	}

	/*reset dpu0-4 & dp0-4 & mmhub*/
	val = mmio_read_32(DISPLAY_RESET_REG);
	val &= ~(DISPLAY_RESET_MASK|MMHUB_RESET_MASK);
	mmio_write_32(DISPLAY_RESET_REG, val);

	/* relese mmhub */
	val = mmio_read_32(DISPLAY_RESET_REG);
	val |= MMHUB_RESET_MASK;
	mmio_write_32(DISPLAY_RESET_REG, val);
}

void cix_sky1_mmhub_config_restore(void)
{
	uint32_t i = 0, val = 0;

	/*release dpu0-4 & dp0-4*/
	val = mmio_read_32(DISPLAY_RESET_REG);
	val |= DISPLAY_RESET_MASK;
	mmio_write_32(DISPLAY_RESET_REG, val);

	/* restore amni port config */
	for (i = 0; i < SMNHUB_ANMI_NODE_NUM; i++) {
		/*rcsu node*/
		if ( i == 24 )
			continue;
		mmio_write_32(SMNHUB_ANMI_NODE_BASE + i * 0x1000 + 0x48,g_mmhub_port_config[i]);
	}

	/*reconfig qos on mmhub*/
	ni700_qos_setting_t *pt =(ni700_qos_setting_t* ) &g_sky1_ni700_qos_setting;

	while(pt->qos_id != NI700_INVALID_QOS_ID) {
		if(pt->bena && (pt->qos_id >= MMHUB_CSI_SLV0 && pt->qos_id <= MMHUB_VPU_SLV1 ) ) {

			/*enable qos override*/
			val = mmio_read_32(pt->reg_ena);
			val &=~ 0x1;
			val |=pt->bena & 0x1;
			mmio_write_32(pt->reg_ena,val);

			/*config read-qos*/
			val = mmio_read_32(pt->reg_ar_qos);
			val &=~ 0xf;
			val |=pt->rd_qos & 0xf;
			mmio_write_32(pt->reg_ar_qos,val);

			/*config write-qos*/
			val = mmio_read_32(pt->reg_aw_qos);
			val &=~ 0xf;
			val |=pt->wr_qos & 0xf;
			mmio_write_32(pt->reg_aw_qos,val);
		}
		pt++;
	}
}

uint32_t sky1_smmu_gop_handler(uint64_t arg0, uint64_t arg1,
			 uint64_t arg2, uint64_t arg3,
			 uint64_t arg4, uint64_t arg5)
{
	switch (arg0) {
	case SMMU_RESET_BEFORE:
		INFO("%s %d smmu_reset_before\n", __func__, __LINE__);
		cix_sky1_mmhub_config_save();
		break;
	case SMMU_RESET_AFTER:
		INFO("%s %d smmu_reset_after\n", __func__, __LINE__);
		cix_sky1_mmhub_config_restore();
		break;
	default:
		INFO("%s %d smmu_reset_not find\n", __func__, __LINE__);
		return SMC_UNK;
	}

	return 0;
}
