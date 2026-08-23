/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_std_cmds.h
* @brief      This file contains Winbond flash devices (W25Qxx) definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_STD_CMDS_H__
#define __QLIB_STD_CMDS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                               DEFINITIONS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/* Manufacture Id                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
#define STD_FLASH_MANUFACTURER__WINBOND_SERIAL_FLASH 0xEF

/*---------------------------------------------------------------------------------------------------------*/
/* Flash characteristics                                                                                   */
/*---------------------------------------------------------------------------------------------------------*/
#define FLASH_BLOCK_SIZE  _64KB_
#define FLASH_SECTOR_SIZE _4KB_  // minimum erase size
#define FLASH_PAGE_SIZE   _256B_ // maximum write size

#define SPI_FLASH_MAKE_CMD(cmdBuf, cmd) \
    memset(cmdBuf, 0, sizeof(cmdBuf));  \
    (cmdBuf)[0] = (cmd);

#define SPI_FLASH_MAKE_CMD_ADDR(cmdBuf, cmd, addr) \
    SPI_FLASH_MAKE_CMD((cmdBuf), (cmd))            \
    (cmdBuf)[1] = LSB2(addr);                      \
    (cmdBuf)[2] = LSB1(addr);                      \
    (cmdBuf)[3] = LSB0(addr);

#define SPI_FLASH_MAKE_CMD_ADDR_DATA(cmdBuf, cmd, addr, outData) \
    SPI_FLASH_MAKE_CMD_ADDR((cmdBuf), (cmd), (addr));            \
    memcpy(&(cmdBuf)[4], (outData), sizeof(outData));

/*---------------------------------------------------------------------------------------------------------*/
/* Standard SPI Instructions                                                                               */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__NONE 0x00u

#define SPI_FLASH_CMD__DEVICE_ID                       0xAB
#define SPI_FLASH_CMD__MANUFACTURER_AND_DEVICE_ID      0x90u
#define SPI_FLASH_CMD__MANUFACTURER_AND_DEVICE_ID_DUAL 0x92
#define SPI_FLASH_CMD__MANUFACTURER_AND_DEVICE_ID_QUAD 0x94
#define SPI_FLASH_CMD__READ_JEDEC                      0x9F
#define SPI_FLASH_CMD__READ_UNIQUE_ID                  0x4B
#define SPI_FLASH_CMD__READ_SFDP_TABLE                 0x5A
#define SPI_FLASH_CMD__SET_BURST_WITH_WRAP             0x77

/*---------------------------------------------------------------------------------------------------------*/
/* Status Registers                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__READ_STATUS_REGISTER_1 0x05
#define SPI_FLASH_CMD__READ_STATUS_REGISTER_2 0x35
#define SPI_FLASH_CMD__READ_STATUS_REGISTER_3 0x15

#define SPI_FLASH_CMD__REGISTER_WRITE_ENABLE   0x50
#define SPI_FLASH_CMD__WRITE_STATUS_REGISTER_1 0x01
#define SPI_FLASH_CMD__WRITE_STATUS_REGISTER_2 0x31
#define SPI_FLASH_CMD__WRITE_STATUS_REGISTER_3 0x11

#define SPI_FLASH_CMD__READ_ECC_STATUS_REGISTER           0x25
#define SPI_FLASH_CMD__WRITE_ECC_STATUS_REGISTER          0x56
#define SPI_FLASH_CMD__READ_ADVANCED_ECC_STATUS_REGISTER  0x7B
#define SPI_FLASH_CMD__CLEAR_ADVANCED_ECC_STATUS_REGISTER 0x72

/*---------------------------------------------------------------------------------------------------------*/
/* SOI/QPI/OPI mode                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__ENTER_SPI 0xFFu // This is EXIT_QPI command in W77Q (16Mb to 128Mb) chips
#define SPI_FLASH_CMD__ENTER_QPI 0x38u
#define SPI_FLASH_CMD__ENTER_OPI 0xE8u

#define SPI_FLASH_CMD__ENTER_DOPI_MODE 0x82u
#define SPI_FLASH_CMD__RESET_SPI_MODE  0xFFu

#define SPI_FLASH_CMD__SET_READ_PARAMETERS 0xC0

#define SPI_FLASH_SET_READ_PARAMETERS_DUMMY_CLOCK_MASK 0x30u

/*---------------------------------------------------------------------------------------------------------*/
/* Power Instructions                                                                                      */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__POWER_DOWN         0xB9u
#define SPI_FLASH_CMD__RELEASE_POWER_DOWN 0xABu

/*---------------------------------------------------------------------------------------------------------*/
/* Reset Instructions                                                                                      */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__RESET_ENABLE 0x66
#define SPI_FLASH_CMD__RESET_DEVICE 0x99

/*---------------------------------------------------------------------------------------------------------*/
/* Erase / Program Suspend / Resume Instructions                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__SUSPEND 0x75
#define SPI_FLASH_CMD__RESUME  0x7A

/*---------------------------------------------------------------------------------------------------------*/
/* Die select Instruction                                                                                  */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__DIE_SELECT 0xC2

/*---------------------------------------------------------------------------------------------------------*/
/* Extended Address Instructions                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__WRITE_EAR 0xC5 // WRITE_EAD command in W77Q (16Mb to 128Mb) chip
#define SPI_FLASH_CMD__READ_EAR  0xC8 // READ_EAD command in W77Q (16Mb to 128Mb) chip

/************************************************************************************************************
 * Read Instructions
************************************************************************************************************/
// *** The Quad Enable (QE) bit in Status Register-2 / CR must be set to 1 before the device will accept the Fast Read Quad/Octal Output Instruction
#define SPI_FLASH_CMD__READ_DATA__1_1_1     0x03
#define SPI_FLASH_CMD__READ_FAST__1_1_1     0x0B
#define SPI_FLASH_CMD__READ_FAST__1_1_2     0x3B
#define SPI_FLASH_CMD__READ_FAST__1_2_2     0xBB
#define SPI_FLASH_CMD__READ_FAST__1_1_4     0x6B
#define SPI_FLASH_CMD__READ_FAST__1_4_4     0xEB
#define SPI_FLASH_CMD__READ_FAST__1_8_8     0xCB
#define SPI_FLASH_CMD__READ_FAST__4_4_4     0xEB
#define SPI_FLASH_CMD__READ_FAST__8_8_8     0xCB
#define SPI_FLASH_CMD__READ_FAST_DTR__1_1_1 0x0D
#define SPI_FLASH_CMD__READ_FAST_DTR__1_2_2 0xBD
#define SPI_FLASH_CMD__READ_FAST_DTR__1_4_4 0xED
#define SPI_FLASH_CMD__READ_FAST_DTR__1_8_8 0xCD
#define SPI_FLASH_CMD__READ_FAST_DTR__4_4_4 0xED
#define SPI_FLASH_CMD__READ_FAST_DTR__8_8_8 0xCD

#define SPI_FLASH_CMD_FAST_READ__MODE_EXIST(qlibContext, cmd)                                                     \
    ((W77Q_READ_BYPASS_MODE_BYTE(qlibContext) != 0u) &&                                                           \
     ((U8)(cmd) == (U8)SPI_FLASH_CMD__READ_FAST__1_2_2 || (U8)(cmd) == (U8)SPI_FLASH_CMD__READ_FAST__1_4_4 ||     \
      (U8)(cmd) == (U8)SPI_FLASH_CMD__READ_FAST__4_4_4 || (U8)(cmd) == (U8)SPI_FLASH_CMD__READ_FAST_DTR__4_4_4 || \
      (U8)(cmd) == (U8)SPI_FLASH_CMD__READ_FAST_DTR__1_2_2 || (U8)(cmd) == (U8)SPI_FLASH_CMD__READ_FAST_DTR__1_4_4))
#define SPI_FLASH_CMD_FAST_READ__MODE_BYTE 0xFF
/*---------------------------------------------------------------------------------------------------------*/
/* Write Instructions                                                                                      */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__WRITE_ENABLE  0x06u
#define SPI_FLASH_CMD__WRITE_DISABLE 0x04u

#define SPI_FLASH_CMD__PAGE_PROGRAM       0x02u
#define SPI_FLASH_CMD__PAGE_PROGRAM_1_1_4 0x32u
#define SPI_FLASH_CMD__PAGE_PROGRAM_1_4_4 0x32u
#define SPI_FLASH_CMD__PAGE_PROGRAM_1_8_8 0x82u
#define SPI_FLASH_CMD__PAGE_PROGRAM_4_4_4 0x32u
#define SPI_FLASH_CMD__PAGE_PROGRAM_8_8_8 0x82u

/*---------------------------------------------------------------------------------------------------------*/
/* Erase Instructions                                                                                      */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__ERASE_SECTOR          0x20u
#define SPI_FLASH_CMD__ERASE_BLOCK_32        0x52u
#define SPI_FLASH_CMD__ERASE_BLOCK_64        0xD8u
#define SPI_FLASH_CMD__ERASE_CHIP            0xC7u
#define SPI_FLASH_CMD__ERASE_CHIP_DEPRECATED 0x60u

/*---------------------------------------------------------------------------------------------------------*/
/* 3 x 256B Security registers Instructions                                                                */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__SEC_REG_READ    0x48u
#define SPI_FLASH_CMD__SEC_REG_PROGRAM 0x42u
#define SPI_FLASH_CMD__SEC_REG_ERASE   0x44u

/*---------------------------------------------------------------------------------------------------------*/
/* Block / Sector locking Instructions                                                                     */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__INDIVIDUAL_BLOCK_SECTOR_LOCK   0x36u
#define SPI_FLASH_CMD__INDIVIDUAL_BLOCK_SECTOR_UNLOCK 0x39u
#define SPI_FLASH_CMD__READ_BLOCK_SECTOR_LOCK_STATE   0x3Du
#define SPI_FLASH_CMD__GLOBAL_BLOCK_SECTOR_LOCK       0x7Eu
#define SPI_FLASH_CMD__GLOBAL_BLOCK_SECTOR_UNLOCK     0x98u

/*---------------------------------------------------------------------------------------------------------*/
/* Address mode Instructions                                                                               */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__4_BYTE_ADDRESS_MODE_ENTER 0xB7u
#define SPI_FLASH_CMD__4_BYTE_ADDRESS_MODE_EXIT  0xE9u

/*---------------------------------------------------------------------------------------------------------*/
/* Extended Configuration Register Instructions                                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__READ_CR       0x85
#define SPI_FLASH_CMD__WRITE_CR      0x81
#define SPI_FLASH_CMD__READ_CR_DFLT  0xB5
#define SPI_FLASH_CMD__WRITE_CR_DFLT 0xB1


/*---------------------------------------------------------------------------------------------------------*/
/* Flag Register Instructions                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_CMD__READ_FR  0x70
#define SPI_FLASH_CMD__CLEAR_FR 0x50
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/* Status registers fields                                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH__STATUS_1_FIELD__BUSY 0u, 1u // ERASE/WRITE IN PROGRESS
#define SPI_FLASH__STATUS_1_FIELD__WEL  1u, 1u // WRITE ENABLE LATCH
#define SPI_FLASH__STATUS_1_FIELD__BP   2u, 3u // BLOCK PROTECT BITS
#define SPI_FLASH__STATUS_1_FIELD__TB   5u, 1u // TOP/BOTTOM PROTECT
#define SPI_FLASH__STATUS_1_FIELD__SEC  6u, 1u // SECTOR PROTECT
#define SPI_FLASH__STATUS_1_FIELD__SRP  7u, 1u // STATUS REGISTER PROTECT

#define SPI_FLASH__STATUS_2_FIELD__SRL       0u, 1u // Status Register Lock
#define SPI_FLASH__STATUS_2_FIELD__QE        1u, 1u // Quad Enable
#define SPI_FLASH__STATUS_2_FIELD__RESERVED1 2u, 1u // Reserved
#define SPI_FLASH__STATUS_2_FIELD__LB        3u, 3u // Security Register Lock Bits
#define SPI_FLASH__STATUS_2_FIELD__CMP       6u, 1u // Complement Protect
#define SPI_FLASH__STATUS_2_FIELD__SUS       7u, 1u // Suspend Status (read only)

#define SPI_FLASH__STATUS_3_FIELD__ADS 0u, 1u // Current Address Mode
#define SPI_FLASH__STATUS_3_FIELD__ADP 1u, 1u // Power-Up Address Mode

// extended address bit is used when W77Q_EXTENDED_ADDR_REG is not supported by device:
#define SPI_FLASH__STATUS_3_FIELD__A24 0u, 1u // Extended Address Bit 24

#define SPI_FLASH__STATUS_3_FIELD__WPS      2u, 1u // Write Protect Selection
#define SPI_FLASH__STATUS_3_FIELD__DRV      5u, 2u // Output Driver Strength
#define SPI_FLASH__STATUS_3_FIELD__HOLD_RST 7u, 1u // Set hold/reset pin function

/************************************************************************************************************
 * Status registers type
************************************************************************************************************/
typedef struct
{
    // Status register 1
    union
    {
        U8 asUint;
        struct
        {
            U8 busy : 1;
            U8 writeEnable : 1;
            U8 BP : 3;
            U8 TB : 1;
            U8 SEC : 1;
            U8 SRP : 1;
        } asStruct;
    } SR1;

    // Status register 2
    union
    {
        U8 asUint;
        struct
        {
            U8 SRL : 1;        ///< Status Register Lock
            U8 quadEnable : 1; ///< Quad Enable
            U8 R : 1;          ///< Reserved
            U8 LB : 3;         ///< Security Register Lock Bits
            U8 CMP : 1;        ///< Complement Protect
            U8 suspend : 1;    ///< Suspend Status (read only)
        } asStruct;
    } SR2;

    union
    {
        U8 asUint;

        struct
        {
            U8 ADS_or_A24 : 1; ///< Current Address Mode in HCD (Q2_4_BYTES_ADDRESS_MODE supported), or Extended Address Bit 24 in MCD device
            U8 ADP_or_RESERVED1 : 1; ///< Power-Up Address Mode in HCD (Q2_4_BYTES_ADDRESS_MODE supported), or reserved bit in MCD device
            U8 WPS : 1;              ///< Write Protect Selection
            U8 RESERVED2 : 1;        ///< Reserved
            U8 RESERVED3 : 1;        ///< Reserved
            U8 DRV0 : 2;             ///< Output Driver Strength
            U8 HOLD_RESET : 1; ///< Hold/Reset pin configuration
        } asStruct;

    } SR3;

} STD_FLASH_STATUS_T;

/*---------------------------------------------------------------------------------------------------------*/
/* Flag register fields                                                                                    */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH__FLAG_FIELD__AMF  0u, 1u // Address Mode Flag
#define SPI_FLASH__FLAG_FIELD__PMAF 1u, 1u // Protected Memory Access Violation Flag
#define SPI_FLASH__FLAG_FIELD__PSF  2u, 1u // Program Suspend Flag
#define SPI_FLASH__FLAG_FIELD__SPIF 3u, 1u // SPI Integrity Error Flag
#define SPI_FLASH__FLAG_FIELD__PF   4u, 1u // Program/CRC Error Flag
#define SPI_FLASH__FLAG_FIELD__EF   5u, 1u // Erase Error Flag
#define SPI_FLASH__FLAG_FIELD__ESF  6u, 1u // Erase Suspend Flag
#define SPI_FLASH__FLAG_FIELD__RF   7u, 1u // Ready Flag

/************************************************************************************************************
 * Flag register type
************************************************************************************************************/
typedef union STD_FLASH_FLAGS_T
{
    U8 asUint;
    struct
    {
        U8 AMF : 1;
        U8 PMAF : 1;
        U8 PSF : 1;
        U8 SPIF : 1;
        U8 PF : 1;
        U8 EF : 1;
        U8 ESF : 1;
        U8 RF : 1;
    } asStruct;
} STD_FLASH_FLAGS_T;

/*---------------------------------------------------------------------------------------------------------*/
/*  Extended configuration register fields                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH__EXTENDED_CONFIGURATION_0_FIELD__BUSMOD 2u, 3u // Bus mode
#define SPI_FLASH__EXTENDED_CONFIGURATION_0_FIELD__DQS_EN 5u, 1u // DQS enable

#define SPI_FLASH__EXTENDED_CONFIGURATION_1_FIELD__DUMMY 0u, 5u // Number of dummy cycles

#define SPI_FLASH__EXTENDED_CONFIGURATION_DEFAULT_DUMMY_CONFIG 8u
#define SPI_FLASH__EXTENDED_CONFIGURATION_MAX_DUMMY_CONFIG     30u

#define SPI_FLASH__EXTENDED_CONFIGURATION_3_FIELD__DRV 0u, 8u // Output drive strength

#define SPI_FLASH__EXTENDED_CONFIGURATION_5_FIELD__AM 0u, 1u // Address mode (24b/32b)

#define SPI_FLASH__EXTENDED_CONFIGURATION_6_FIELD__XIP      0u, 1u // XIP disable
#define SPI_FLASH__EXTENDED_CONFIGURATION_6_FIELD__XIP_MODE 1u, 3u // XIP mode

#define SPI_FLASH__EXTENDED_CONFIGURATION_7_FIELD__WRAP 0u, 3u // Wrap around length

#define SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__QE         2u, 1u // Quad enable
#define SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__SFDP_DUMMY 3u, 1u // Read SFDP dummy cycles
#define SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__WPU_IO0    4u, 1u // Weak pull up on IO0
#define SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__WPU_IO123  5u, 1u // Weak pull up on IO1-3
#define SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__WPU_IO4567 6u, 1u // Weak pull up on IO4-7
#define SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__AUTO_RCVR  7u, 1u // Erase auto recovery enable

#define SPI_FLASH__EXTENDED_CONFIGURATION_11_FIELD__ADDR_ALIGN 0u, 2u // Address alignment

#define SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SECURE_IMASK   0u, 1u // Security interrupt
#define SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__AWDT_IMASK     1u, 1u // WD timer
#define SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SPI_INTG_IMASK 3u, 1u // SPI integrity interrupt
#define SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SEC_IMASK      4u, 1u // ECC SEC interrupt
#define SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__DED_IMASK      5u, 1u // ECC DED interrupt
#define SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__MULTP_IMASK    6u, 1u // ECC multi-programming interrupt

#define SPI_FLASH__EXTENDED_CONFIGURATION_15_FIELD__LATENCY    0u, 4u // Array latency override value
#define SPI_FLASH__EXTENDED_CONFIGURATION_15_FIELD__LATENCY_EN 4u, 1u // Array latency override enable

#define SPI_FLASH__EXTENDED_CONFIGURATION_SIZE 256u

#define CONFIG_REG_ADDR__BUSMOD                 0x00u
#define CONFIG_REG_ADDR__DUMMY                  0x01u
#define CONFIG_REG_ADDR__OUTPUT_DRIVER_STRENGTH 0x03u
#define CONFIG_REG_ADDR__DATA_EXTENSION         0x04u
#define CONFIG_REG_ADDR__ADDRESS_MODE           0x05u
#define CONFIG_REG_ADDR__QUAD_ENABLE            0x10u
#define CONFIG_REG_ADDR__INTERRUPT_MASK         0x12u
#define CONFIG_REG_ADDR__INTERRUPT_EVENTS       0x13u
/************************************************************************************************************
 * Extended configuration register type
************************************************************************************************************/

typedef struct
{
    // Extended configuration register, address 00h
    union
    {
        U8 asUint;
        struct
        {
            U8 RESERVED1 : 2;
            U8 busmod : 3;
            U8 dqs_en : 1;
        } asStruct;
    } CR0;

    // Extended configuration register, address 01h
    union
    {
        U8 asUint;
        struct
        {
            U8 dummy : 5;
        } asStruct;
    } CR1;

    // Extended configuration register, address 03h
    union
    {
        U8 asUint;
        struct
        {
            U8 drv : 8;
        } asStruct;
    } CR3;

    // Extended configuration register, address 04h
    union
    {
        U8 asUint;
        struct
        {
            U8 RESERVED1 : 8;
        } asStruct;
    } CR4;

    // Extended configuration register, address 05h
    union
    {
        U8 asUint;
        struct
        {
            U8 am : 1;
        } asStruct;
    } CR5;

    // Extended configuration register, address 06h
    union
    {
        U8 asUint;
        struct
        {
            U8 xip : 1;
            U8 xip_mode : 3;
        } asStruct;
    } CR6;

    // Extended configuration register, address 07h
    union
    {
        U8 asUint;
        struct
        {
            U8 wrap : 3;
        } asStruct;
    } CR7;

    // Extended configuration register, address 10h
    union
    {
        U8 asUint;
        struct
        {
            U8 inv_addr : 1;
            U8 mpgm_inten : 1;
            U8 qe : 1;
            U8 sfdp_dummy : 1;
        } asStruct;
    } CR10;

    // Extended configuration register, address 11h
    union
    {
        U8 asUint;
        struct
        {
            U8 addr_align : 2;
        } asStruct;
    } CR11;

    // Extended configuration register, address 12h
    union
    {
        U8 asUint;
        struct
        {
            U8 secure_imask : 1;
            U8 awdt_imask : 1;
            U8 reserved : 1;
            U8 spi_intg_imask : 1;
            U8 sec_imask : 1;
            U8 ded_imask : 1;
            U8 multp_imask : 1;
        } asStruct;
    } CR12;

    // Extended configuration register, address 13h
    union
    {
        U8 asUint;
        struct
        {
            U8 secure_int : 1;
            U8 awdt_int : 1;
            U8 reserved : 1;
            U8 spi_int : 1;
            U8 sec_int : 1;
            U8 ded_int : 1;
            U8 multp_int : 1;
        } asStruct;
    } CR13;

} STD_FLASH_EXTENDED_CONFIGURATION_T;

/*---------------------------------------------------------------------------------------------------------*/
/*  Extended configuration register values                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
#define STD_FLASH_EXTENDED_CONFIGURATION_BUS_MODE_SPI  0x7u
#define STD_FLASH_EXTENDED_CONFIGURATION_BUS_MODE_QPI  0x6u
#define STD_FLASH_EXTENDED_CONFIGURATION_BUS_MODE_SOPI 0x3u
#define STD_FLASH_EXTENDED_CONFIGURATION_BUS_MODE_DOPI 0x1u

#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_18_OHM  0xF8
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_25_OHM  0xF9
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_35_OHM  0xFA
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_50_OHM  0xFB
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_75_OHM  0xFC
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_80_OHM  0xFD
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_150_OHM 0xFE
#define STD_FLASH_EXTENDED_CONFIGURATION_DRV_NA      0xFF

/*---------------------------------------------------------------------------------------------------------*/
/*  ECC status register fields                                                                             */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH__ECC_STATUS_FIELD__ECCO      0u, 1u // ECC on/off status
#define SPI_FLASH__ECC_STATUS_FIELD__ECC_EN    2u, 1u // ECC enable
#define SPI_FLASH__ECC_STATUS_FIELD__ERASED    4u, 1u // Erased line
#define SPI_FLASH__ECC_STATUS_FIELD__MULT_PROG 5u, 1u // Multiple programming
#define SPI_FLASH__ECC_STATUS_FIELD__DED       6u, 1u // Double error detection
#define SPI_FLASH__ECC_STATUS_FIELD__SEC       7u, 1u // Single error correction

/************************************************************************************************************
 * ECC status register type
************************************************************************************************************/
typedef union STD_FLASH_ECC_STATUS_T
{
    U8 asUint;
    struct
    {
        U8 ecco : 1;
        U8 RESERVED1 : 1;
        U8 ecc_en : 1;
        U8 RESERVED2 : 1;
        U8 erased : 1;
        U8 mult_prog : 1;
        U8 ded : 1;
        U8 sec : 1;
    } asStruct;
} STD_FLASH_ECC_STATUS_T;

/*---------------------------------------------------------------------------------------------------------*/
/*  Advanced ECC register fields                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH__ADVANCED_ECC_0_FIELD__SRA3  0u, 4u // SRA (bits 27:24)
#define SPI_FLASH__ADVANCED_ECC_0_FIELD__SACVF 7u, 1u // SRA valid

#define SPI_FLASH__ADVANCED_ECC_1_FIELD__SRA2 0u, 8u // SRA (bits 23:16)

#define SPI_FLASH__ADVANCED_ECC_2_FIELD__SRA1 0u, 8u // SRA (bits 15:8)

#define SPI_FLASH__ADVANCED_ECC_3_FIELD__SC   0u, 4u // SEC counter
#define SPI_FLASH__ADVANCED_ECC_3_FIELD__SRA0 4u, 4u // SRA (bits 7:4)

#define SPI_FLASH__ADVANCED_ECC_4_FIELD__DRA3  0u, 4u // DRA (bits 27:24)
#define SPI_FLASH__ADVANCED_ECC_4_FIELD__DACVF 7u, 1u // DRA valid

#define SPI_FLASH__ADVANCED_ECC_5_FIELD__DRA2 0u, 8u // DRA (bits 23:16)

#define SPI_FLASH__ADVANCED_ECC_6_FIELD__DRA1 0u, 8u // DRA (bits 15:8)

#define SPI_FLASH__ADVANCED_ECC_7_FIELD__DC   0u, 4u // DED counter
#define SPI_FLASH__ADVANCED_ECC_7_FIELD__DRA0 4u, 4u // DRA (bits 7:4)

/************************************************************************************************************
 * Advanced ECC register type
************************************************************************************************************/
typedef struct
{
    // Advanced ECC register, byte 00h
    union
    {
        U8 asUint;
        struct
        {
            U8 sra3 : 4;
            U8 RESERVED1 : 3;
            U8 sacvf : 1;
        } asStruct;
    } AECCR0;

    // Advanced ECC register, byte 01h
    union
    {
        U8 asUint;
        struct
        {
            U8 sra2 : 8;
        } asStruct;
    } AECCR1;

    // Advanced ECC register, byte 02h
    union
    {
        U8 asUint;
        struct
        {
            U8 sra1 : 8;
        } asStruct;
    } AECCR2;

    // Advanced ECC register, byte 03h
    union
    {
        U8 asUint;
        struct
        {
            U8 sc : 4;
            U8 sra0 : 4;
        } asStruct;
    } AECCR3;

    // Advanced ECC register, byte 04h
    union
    {
        U8 asUint;
        struct
        {
            U8 dra3 : 4;
            U8 RESERVED1 : 3;
            U8 dacvf : 1;
        } asStruct;
    } AECCR4;

    // Advanced ECC register, byte 05h
    union
    {
        U8 asUint;
        struct
        {
            U8 dra2 : 8;
        } asStruct;
    } AECCR5;

    // Advanced ECC register, byte 06h
    union
    {
        U8 asUint;
        struct
        {
            U8 dra1 : 8;
        } asStruct;
    } AECCR6;

    // Advanced ECC register, byte 07h
    union
    {
        U8 asUint;
        struct
        {
            U8 dc : 4;
            U8 dra0 : 4;
        } asStruct;
    } AECCR7;
} STD_FLASH_ADVANCED_ECC_T;

/*---------------------------------------------------------------------------------------------------------*/
/* Dummy cycles                                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
// for W77Q (16Mb to 128Mb) flash that supports Set Read Parameters command
#ifndef QLIB_QPI_READ_DUMMY_CYCLES
#define QLIB_QPI_READ_DUMMY_CYCLES (8u) // set to max value to support all frequencies
#else
#if (QLIB_QPI_READ_DUMMY_CYCLES != 4u) && (QLIB_QPI_READ_DUMMY_CYCLES != 6u) && (QLIB_QPI_READ_DUMMY_CYCLES != 8u)
#error Value of QLIB_QPI_READ_DUMMY_CYCLES is not supported
#endif
#endif

#define SPI_FLASH_DUMMY_CYCLES__DEVICE_ID_Q2__1_1_1 (3u * 8u)
#define SPI_FLASH_DUMMY_CYCLES__DEVICE_ID_Q2__4_4_4 (3u * 2u)
#define SPI_FLASH_DUMMY_CYCLES__DEVICE_ID           (8u)
#define SPI_FLASH_DUMMY_CYCLES__RELEASE_POWER_DOWN_GET_ID(context)                                                \
    ((Q2_DEVICE_ID_DUMMY(context) == 0u)                                                                          \
         ? SPI_FLASH_DUMMY_CYCLES__DEVICE_ID                                                                      \
         : ((QLIB_STD_GET_BUS_MODE(context) == QLIB_BUS_MODE_4_4_4) ? SPI_FLASH_DUMMY_CYCLES__DEVICE_ID_Q2__4_4_4 \
                                                                    : SPI_FLASH_DUMMY_CYCLES__DEVICE_ID_Q2__1_1_1))
#define SPI_FLASH_DUMMY_CYCLES__UNIQUE_ID__SPI_ADDRESS_MODE_3B (4u * 8u)
#define SPI_FLASH_DUMMY_CYCLES__UNIQUE_ID__SPI_ADDRESS_MODE_4B (5u * 8u)
#define SPI_FLASH_DUMMY_CYCLES__UNIQUE_ID__QPI_OPI             (20u)
#define SPI_FLASH_DUMMY_CYCLES__READ_CR                        (8u)
#define SPI_FLASH_DUMMY_CYCLES__READ_FR_SPI                    (0u)
#define SPI_FLASH_DUMMY_CYCLES__READ_FR_QPI_OPI                (8u)
#define SPI_FLASH_DUMMY_CYCLES__SECURITY_REGS                  (1u * 8u)
#define SPI_FLASH_DUMMY_CYCLES__SET_BURST_WITH_WRAP            (3u * 2u)

// dummy cycles in flash where Read Extended Address Register command requires dummy cycles
#define SPI_FLASH_DUMMY_CYCLES__READ_EAR_SPI     (0u)
#define SPI_FLASH_DUMMY_CYCLES__READ_EAR_QPI_OPI (8u)

// W77Q (16Mb to 128Mb) fast read dummy cycles.
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_1_1 (1u * 8u) // deprecated, use "1_1_X" instead
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_1_X (8u)
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_2_2 (0u)
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_4_4 (4u)
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ__4_4_4 \
    (QLIB_QPI_READ_DUMMY_CYCLES - 2u) // should be set with 0xC0 cmd. remove 2 mode clocks
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__1_1_1 (6u)
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__1_2_2 (4u)
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__1_4_4 (7u)
#define SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__4_4_4 (7u)

// dummy cycles in flash where the Read register commands requires dummy cycles
#define SPI_FLASH_DUMMY_CYCLES__READ_STATUS_REGISTER_SPI     (0u)
#define SPI_FLASH_DUMMY_CYCLES__READ_STATUS_REGISTER_QPI_OPI (8u)
#define SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_SPI               (0u)
#define SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI           (8u)

#ifdef QLIB_SUPPORT_QPI
#define SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI(deviceId) ((deviceId) <= 0x17u ? 0u : SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI)
#endif

#define SPI_FLASH_DUMMY_CYCLES__READ_ECCR_SPI     (0u)
#define SPI_FLASH_DUMMY_CYCLES__READ_ECCR_QPI_OPI (8u)
#define SPI_FLASH_DUMMY_CYCLES__READ_AECCR        (8u)

// translate dummy cycles to bits 4,5 in the Set Read Parameters command:
// P5 - P4 = (0 - 0 Not Supported) / (0 - 1 for 4 dummy cycles) / (1 - 0 for 6 dummy cycles) / (1 - 1 for 8 dummy cycles)
#define SPI_FLASH_SET_READ_PARAMETERS (((QLIB_QPI_READ_DUMMY_CYCLES - 1u) << 3) & SPI_FLASH_SET_READ_PARAMETERS_DUMMY_CLOCK_MASK)

#define Q2_DEFAULT_QPI_READ_DUMMY_CYCLES (2u) // default value of QPI dummy cycles if not set by 0xC0 command

/*---------------------------------------------------------------------------------------------------------*/
/* Security registers parameters                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_FLASH_SECURITY_REGISTER_1_ADDR 0x1000u
#define SPI_FLASH_SECURITY_REGISTER_2_ADDR 0x2000u
#define SPI_FLASH_SECURITY_REGISTER_3_ADDR 0x3000u

/*---------------------------------------------------------------------------------------------------------*/
/* Time outs                                                                                               */
/*---------------------------------------------------------------------------------------------------------*/
#define STD_FLASH__TIMEOUT_SCALE_ms_G2 67 // At sys_clk=100M: 1 cycle = 15us => 66.6 = 1ms
#define STD_FLASH__TIMEOUT_INIT        100
#define STD_FLASH__TIMEOUT             2010                          // 2ms      (30ms max) (Write Status Register Time)
#define STD_FLASH__TIMEOUT_WRITE       335                           // 0.8ms    (5ms max)
#define STD_FLASH__TIMEOUT_ERASE_4k    26800                         // 45ms     (400ms max)
#define STD_FLASH__TIMEOUT_ERASE_32K   107200                        // 120ms    (1,600ms max)
#define STD_FLASH__TIMEOUT_ERASE_64K   134000                        // 200ms    (2,000ms max)
#define STD_FLASH__TIMEOUT_ERASE_CHIP  3350000                       // 10,000ms (50,000ms max)
#define STD_FLASH__TIMEOUT_ERASE       STD_FLASH__TIMEOUT_ERASE_CHIP // TODO - temporary
#define STD_FLASH__TIMEOUT_MAX         0xFFFFFFFFu

/*---------------------------------------------------------------------------------------------------------*/
/* Other properties                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define STD_FLASH__MAX_READ_DATA_CMD_CLK (50 * _1MHz_)
#define STD_FLASH__MAX_SPI_CLK           (133 * _1MHz_)




#ifdef __cplusplus
}
#endif

#endif // __QLIB_STD_CMDS_H__
