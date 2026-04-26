/************************************************************************************************************
* \internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* \endinternal
*
* @file       qlib_platform.c
* @brief      This file contains platform specific implementations
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_platform.h"
#include <stdio.h>
#include <kernel/thread.h>
#include <kernel/tee_misc.h>
#include <mm/core_memprot.h>
// #include <common/debug.h>
// #include <drivers/spi_mem.h>
// #include <lib/utils.h>
// #include <endian.h>
#define CIX_SIP_NOR_STORAGE 0xC200000B
#define SPI_MEM_BUSWIDTH_1_LINE         1U
#define SPI_MEM_BUSWIDTH_2_LINE         2U
#define SPI_MEM_BUSWIDTH_4_LINE         4U

/*
 * enum spi_mem_data_dir - Describes the direction of a SPI memory data
 *                         transfer from the controller perspective.
 * @SPI_MEM_DATA_IN: data coming from the SPI memory.
 * @SPI_MEM_DATA_OUT: data sent to the SPI memory.
 */
enum spi_mem_data_dir {
        SPI_MEM_NO_DATA,
        SPI_MEM_DATA_IN,
        SPI_MEM_DATA_OUT,
};

/*
 * struct spi_mem_op - Describes a SPI memory operation.
 *
 * @cmd.buswidth: Number of IO lines used to transmit the command.
 * @cmd.opcode: Operation opcode.
 * @addr.nbytes: Number of address bytes to send. Can be zero if the operation
 *               does not need to send an address.
 * @addr.buswidth: Number of IO lines used to transmit the address.
 * @addr.val: Address value. This value is always sent MSB first on the bus.
 *            Note that only @addr.nbytes are taken into account in this
 *            address value, so users should make sure the value fits in the
 *            assigned number of bytes.
 * @dummy.nbytes: Number of dummy bytes to send after an opcode or address. Can
 *                be zero if the operation does not require dummy bytes.
 * @dummy.buswidth: Number of IO lines used to transmit the dummy bytes.
 * @data.buswidth: Number of IO lines used to send/receive the data.
 * @data.dir: Direction of the transfer.
 * @data.nbytes: Number of data bytes to transfer.
 * @data.buf: Input or output data buffer depending on data::dir.
 */
struct spi_mem_op {
        struct {
                uint8_t buswidth;
                uint8_t opcode;
        } cmd;

        struct {
                uint8_t nbytes;
                uint8_t buswidth;
                uint64_t val;
        } addr;

        struct {
                uint8_t nbytes;
                uint8_t buswidth;
        } dummy;

        struct {
                uint8_t buswidth;
                enum spi_mem_data_dir dir;
                unsigned int nbytes;
                void *buf;
        } data;
};

static uint32_t cix_sip_call(uint32_t sip_svc_id, 
                            uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7)
{
    struct thread_smc_args args = {
        .a0 = sip_svc_id,
        .a1 = reg_pair_to_64(arg1, arg0),
		.a2 = reg_pair_to_64(arg3, arg2),
		.a3 = reg_pair_to_64(arg5, arg4),
		.a4 = reg_pair_to_64(arg7, arg6),
    };
    thread_smccc(&args);
    return args.a0;
}
/*                                                Test data                                                */
uint8_t input[] =   
{   
    0x01,0x02,0x03,0x04,  0x05,0x06,0x07,0x08,  0x09,0x0a,0x0b,0x0c,  0x0d,0x0e,0x0f,0x00
};
uint8_t expected_result[32] = 
{  
    0x5e, 0x97, 0xe9, 0xd2, 0x3b, 0xb2, 0x3f, 0xa1, 
    0x51, 0xec, 0x07, 0x05, 0xd6, 0x29, 0xd7, 0x75, 
    0x14, 0x11, 0x59, 0x74, 0xe9, 0x72, 0xce, 0x2d, 
    0x8a, 0xb0, 0x89, 0x90, 0xde, 0x79, 0xb2, 0x64
};

/************************************************************************************************************
 * @brief       This function performs a SPI transaction
 * @param[in,out]   userData	User data which is set using QLIB_SetUserData
 * @param[in]   format	SPI format
 * @param[in]   flags	SPI flags, including DTR flags. For supported flags refer to QLIB_SPI_FLAGS definitions above.
 * @param[in]   dataOutStream	pointer to a buffer with SPI output information: SPI command followed by address and dataOut
 * @param[in]   cmdSize	Number of SPI command bytes in dataOutStream buffer
 * @param[in]   addressSize	Number of address bytes in dataOutStream buffer
 * @param[in]   dataOutSize	Number of dataOut bytes in dataOutStream buffer.
 * @param[in]   dummyCycles	Dummy cycles between write and read phases
 * @param[out]  dataIn	pointer to a buffer which holds the data received
 * @param[in]   dataInSize	data received size in bytes.
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/

uint8_t tmpbuf[1024];
struct spi_mem_op op;

int PLAT_SPI_WriteReadTransaction(const void*     userData, //userData
                                  QLIB_BUS_MODE_T format, //format
                                  uint32_t        flags, //dtr
                                  const uint8_t*  dataOutStream, //dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize, //addressSize
                                  uint32_t        dataOutSize, //dataOutSize
                                  uint32_t        dummyCycles, //dummyCycles
                                  uint8_t*        dataIn, //dataIn
                                  uint32_t        dataInSize){ //dataInSize
    // We assume that the dummy cycles are multiple of 8 bits. This is true for single SPI.
    uint8_t *out = NULL;
    int ret = -1;
    uint32_t address = 0;
    size_t size = 1024;
    uint8_t *tmpbuf = NULL;
    int data_dir = SPI_MEM_NO_DATA;
    uint8_t* payload = NULL;
    uint32_t payload_size = 0;
    tmpbuf = malloc(size);
    if(tmpbuf == NULL){
        DMSG("tmpbuf fail");
    }

    // Set output command
    if(NULL == dataOutStream){
        DMSG("dataOutStream is null... \n");
        ret = -1;
        return ret;
    }
    op.cmd.opcode = dataOutStream[0];
    if(addressSize > 0) {
        for(int i=0; i<addressSize; i++)
        {
            address = address + (dataOutStream[i+cmdSize] << (( addressSize - i - 1) << 3));
        }
    }
    if(dataOutSize > 0) {
        out = (uint8_t *)&dataOutStream[cmdSize + addressSize];
    }
    if ((op.cmd.opcode == 0xA1) ||(op.cmd.opcode == 0xD1)) {
        out = tmpbuf;
        address = TEE_U32_BSWAP(address);
        memcpy(out,(uint8_t*)&address,addressSize);
        if(dataOutSize != 0){
            memcpy(out+addressSize,dataOutStream+cmdSize+addressSize,dataOutSize);
        }
        dataOutSize = addressSize + dataOutSize;
        address = 0;
        addressSize = 0;
        DMSG("A1/D1 swap address to dataout\n");
    }
    DMSG("tee format:%d cmd:%x cmdSize:0x%x address:%x addresssize:%x dataOutStream:%x \
    dataOutSize:%x dummyCycles:%x dataIn:%x dataInSize:%x\n", format, dataOutStream[0], cmdSize,  \
    address, addressSize,dataOutStream==NULL?0:1,dataOutSize,dummyCycles,dataIn==NULL?0:1,dataInSize);

    if (dataIn != NULL) {
        DHEXDUMP(dataIn, dataInSize);
        data_dir = SPI_MEM_DATA_IN;
        payload = (uint8_t *)(virt_to_phys(dataIn));
        DMSG("data-in virt buf :0x%" PRIxVA "\n", (vaddr_t)dataIn);
        DMSG("data-in phys buf :0x%" PRIxPA "\n", virt_to_phys(dataIn));
        payload_size = dataInSize;
    } else if (dataOutSize != 0) {
        data_dir = SPI_MEM_DATA_OUT;
        payload = (uint8_t *)(virt_to_phys(out));
        DMSG("data-out buf :0x%" PRIxPA "\n", virt_to_phys(out));
        payload_size = dataOutSize;
    }

    ret = cix_sip_call(CIX_SIP_NOR_STORAGE,
		                dataOutStream[0], address, addressSize, data_dir, format,
			            dummyCycles, (uint32_t)payload, payload_size);
    
    if (dataIn != NULL){
        DHEXDUMP(dataIn, dataInSize);
    }

    if(tmpbuf){
        free(tmpbuf);
    }

    // Return OK
    ret = 0;
    return ret;
}

void CORE_RESET(void)
{
    
}
