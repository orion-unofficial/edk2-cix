/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_platform.h
* @brief      This file includes platform specific definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_PLATFORM_H__
#define __QLIB_PLATFORM_H__

#define Q2_API 1
#ifdef Q2_API
#define QCONF_TARGET w77q128jw_revA
#define QLIB_TARGET all_targets
#define QLIB_NUM_OF_DIES 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                          QLIB CONFIGURATIONS                                            */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * define QLIB_MAX_SPI_INPUT_SIZE to SPI input buffer limitation. If defined PLAT_SPI_WriteReadTransaction
 * will be invoked in chunks of that limit.
************************************************************************************************************/
//example for 1024 bytes limit
//#define QLIB_MAX_SPI_INPUT_SIZE 1024

/************************************************************************************************************
 * define QLIB_MAX_SPI_OUTPUT_SIZE to SPI output buffer limitation. If defined,  PLAT_SPI_WriteReadTransaction
 * for secure commands will be invoked in chunks of that limit. This value can not be less then 4 bytes
 * since the CTAG is 4 bytes long. This define is relevant only for flash that supports Q2_SPLIT_IBUF_FEATURE
 *
 * example:
 * The SET_GMT command format is [OP1, CTAG (32 bits), GMT (160 bits), SIG (64 bits)]
 * If QLIB_MAX_SPI_OUTPUT_SIZE is 16 bytes,
 *
 * [OP1, CTAG (4 bytes), GMT (first 12 bytes)]
 * [OP1, GMT (next 8 bytes), SIG (8 bytes)]
 ************************************************************************************************************/
//example for 16 bytes limit
//#define QLIB_MAX_SPI_OUTPUT_SIZE 16

/************************************************************************************************************
 * Enable async HASH implementation if available
************************************************************************************************************/
//#define QLIB_HASH_OPTIMIZATION_ENABLED
//#define QLIB_SPI_OPTIMIZATION_ENABLED

/************************************************************************************************************
 * define SPI_INIT_ADDRESS_MODE_4_BYTES if the core operates in 4 bytes address mode on its initialization.
 * by default the flash powers up in 3 bytes address mode. If the user wants the flash to power up
 * in 4 bytes address mode, this definition is required.
 * This configuration is relevant only for flash that supports 4 bytes address mode
 ************************************************************************************************************/
//#define SPI_INIT_ADDRESS_MODE_4_BYTES

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                              QLIB DEFINES                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * SPI bus mode
************************************************************************************************************/
typedef enum QLIB_BUS_MODE_T
{
    QLIB_BUS_MODE_INVALID = 0,
    QLIB_BUS_MODE_1_1_1   = 1,
    QLIB_BUS_MODE_1_1_2   = 2,
    QLIB_BUS_MODE_1_2_2   = 3,
    QLIB_BUS_MODE_1_1_4   = 4,
    QLIB_BUS_MODE_1_4_4   = 5,
    QLIB_BUS_MODE_4_4_4   = 6,
    QLIB_BUS_MODE_1_8_8   = 7,
    QLIB_BUS_MODE_8_8_8   = 8,
    QLIB_BUS_MODE_MAX     = QLIB_BUS_MODE_8_8_8
} QLIB_BUS_MODE_T;

/************************************************************************************************************
 * SPI commands Flags
************************************************************************************************************/
#define QLIB_SPI_FLAGS__DATA_PHASE_DTR (1u << 0u)
#define QLIB_SPI_FLAGS__ADDR_PHASE_DTR (1u << 1u)
#define QLIB_SPI_FLAGS__CMD_PHASE_DTR  (1u << 2u)

/************************************************************************************************************
 * This type contains dual transfer rate for SPI commands
************************************************************************************************************/
#define QLIB_DTR__NO_DTR    0u
#define QLIB_DTR__ADDR_DATA (QLIB_SPI_FLAGS__DATA_PHASE_DTR | QLIB_SPI_FLAGS__ADDR_PHASE_DTR)
#define QLIB_DTR__ALL       (QLIB_SPI_FLAGS__DATA_PHASE_DTR | QLIB_SPI_FLAGS__ADDR_PHASE_DTR | QLIB_SPI_FLAGS__CMD_PHASE_DTR)
#define QLIB_DTR_MASK       QLIB_DTR__ALL

#define noHW_TRNG

/************************************************************************************************************
 * Hash optimization options
 * QLIB can send optimization request for hash operation. this is optional, user may ignore it
************************************************************************************************************/
typedef enum QLIB_HASH_OPT_T
{
    QLIB_HASH_OPT_NONE,            ///< No optimization
    QLIB_HASH_OPT_FIXED_55_ALIGNED ///< following hash operation includes exactly 55 bytes in one update operation and data pointer is 4 bytes aligned (optimize SRD/SARD)
} QLIB_HASH_OPT_T;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                         QLIB DEFINE OVERRIDES                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief   This macro declare variable to store interrupt state
 * @param   ints     variable name that is declared to store the interrupt state
************************************************************************************************************/
#ifndef INTERRUPTS_VAR_DECLARE
#define INTERRUPTS_VAR_DECLARE(ints)
#endif

/************************************************************************************************************
 * @brief   This macro preserves interrupt state and disable interrupts - Used in the beginning of an atomic flow
 * @param   ints   the variable declared in @ref INTERRUPTS_VAR_DECLARE which will store the current enabled interrupts
************************************************************************************************************/
#ifndef INTERRUPTS_SAVE_DISABLE
#define INTERRUPTS_SAVE_DISABLE(ints)
#endif

/************************************************************************************************************
 * @brief   This macro restores the saved interrupt state from @ref INTERRUPTS_SAVE_DISABLE. Used at the end of an atomic flow
 * @param   ints   the variable declared in @ref INTERRUPTS_VAR_DECLARE which used in @ref INTERRUPTS_SAVE_DISABLE and stored the enabled interrupts
************************************************************************************************************/
#ifndef INTERRUPTS_RESTORE
#define INTERRUPTS_RESTORE(ints)
#endif

/************************************************************************************************************
 * @brief This macro disables platform XIP to execute standard or secure command
 ************************************************************************************************************/
#ifndef PLATFORM_XIP_DISABLE
#define PLATFORM_XIP_DISABLE()
#endif

/************************************************************************************************************
 * @brief This macro re-enables platform XIP that was disabled with @ref PLATFORM_XIP_DISABLE
 ************************************************************************************************************/
#ifndef PLATFORM_XIP_ENABLE
#define PLATFORM_XIP_ENABLE()
#endif

/************************************************************************************************************
 * @brief   Define the LMS attestation Merkle Tree height, which determines the maximal number of signatures.
 *          The OST number is limited to (2^h - 1)
 *          Relevant only for flash that supports LMS attestation feature (W77Q/T 256Mb to 1Gb)
************************************************************************************************************/
#ifndef QLIB_LMS_ATTEST_TREE_HEIGHT
#define QLIB_LMS_ATTEST_TREE_HEIGHT 10u
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                       PLATFORM SPECIFIC FUNCTIONS                                       */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This function resets the core CPU
************************************************************************************************************/
void CORE_RESET(void);

#ifndef Q2_API
    #define Q2_API 1
#endif

#ifndef Q2_API
/************************************************************************************************************
 * @brief       This function initialize HASH context\n
 *
 * @param[out]  ctx     Hash context
 * @param[in]   opt     Hash optimization option
 *
 * @return
 * 0                      - no error occurred\n
 * non-zero               - error occurred
 ************************************************************************************************************/
int PLAT_HASH_Init(void** ctx, QLIB_HASH_OPT_T opt);

/************************************************************************************************************
 * @brief       This function adds data to current HASH calculation.\n
 * This function can be called repeatedly with an arbitrary amount of data to be hashed.\n
 * This function is an implementation of the hash function supported by the W77Q defined in the spec.\n
 * For performance reasons, it is recommended to use HW implementation of this function.\n
 * Test vectors can be found in the spec TBD
 *
 * @param[in,out]  ctx        Hash context
 * @param[in]      data       Input data
 * @param[in]      dataSize   Input data size in bytes
 *
 * @return
 * 0                      - no error occurred\n
 * non-zero               - error occurred
 ************************************************************************************************************/
int PLAT_HASH_Update(void* ctx, const void* data, uint32_t dataSize);

/************************************************************************************************************
 * @brief       Finalize hashing and erases the context.
 *
 * @param[in,out]  ctx        Hash context
 * @param[out]     output     digest
 *
* @return
 * 0                      - no error occurred\n
 * non-zero               - error occurred
 ************************************************************************************************************/
int PLAT_HASH_Finish(void* ctx, uint32_t* output);

#else
/************************************************************************************************************
 * @brief       This function is an implementation of the hash function supported by the W77Q defined in the spec.\n
 * For performance reasons, it is recommended to use HW implementation of this function.\n
 * Test vectors can be found in the spec TBD
 *
 * @param[out]  output     digest
 * @param[in]   data       Input data
 * @param[in]   dataSize   Input data size in bytes
 ************************************************************************************************************/
void PLAT_HASH(uint32_t* output, const void* data, uint32_t dataSize);

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED

/************************************************************************************************************
 * @brief The function starts asynchronous HASH calculation
 *
 * @param[out]  output     Pointer to HASH output
 * @param[in]   data       Input data
 * @param[in]   dataSize   Input data size in bytes
************************************************************************************************************/
void PLAT_HASH_Async(uint32_t* output, const void* data, uint32_t dataSize);

/************************************************************************************************************
 * @brief The function waits for previous asynchronous HASH calculation to finish.
************************************************************************************************************/
void PLAT_HASH_Async_WaitWhileBusy(void);

#endif //QLIB_HASH_OPTIMIZATION_ENABLED
#endif //Q2_API


#ifdef USE_CONSTANT_NONCE
/************************************************************************************************************
 * @brief       This function sets a constant 'nonce' number.
 *
* @return
 * 0                      - no error occurred\n
 * non-zero               - error occurred
************************************************************************************************************/
int PLAT_SetNONCE(uint64_t constNonce);
#endif // USE_CONSTANT_NONCE

/************************************************************************************************************
 * @brief       This function returns non-repeating 'nonce' number.
 * A 'nonce' is a 64bit number that is used in session establishment.\n
 * To prevent replay attacks such nonce must be 'non-repeating' - appear different each function execution.\n
 * Typically implemented as a HW TRNG.
 *
 * @return      64 bit random number
************************************************************************************************************/
uint64_t PLAT_GetNONCE(void);

/************************************************************************************************************
 * @brief       This routine performs SPI write-read transaction.
 * This function should be linked to RAM memory.\n
 * In order to verify that the function works properly, please use the following wave form examples:\n\n
 * PLAT_SPI_WriteReadTransaction(format=1_1_1, flags=0, dataOutStream=[0x90, 0x00, 0x00, 0x00], cmdSize=1, addressSize=3, dataOutSize=0, dummy=0, dataIn=Ptr, dataInSize=2)\n
 * ![wave form example 1](spi_wave_form_1.png)\n\n
 * PLAT_SPI_WriteReadTransaction(format=1_1_1, flags=0, dataOutStream=[0x0B, 0x00, 0x00, 0x00], cmdSize=1, addressSize=3, dataOutSize=0, dummy=8, dataIn=Ptr, dataInSize=32)\n
 * ![wave form example 2](spi_wave_form_2.png)
 * The system integrator needs to allocate sufficient space in the SPI controller on the MCU side to drive the command, address, and dataOut for the SPI transaction.
 * Sufficient space should be allocated in the SPI controller to sample dataIn.
 * The maximal sizes for the fields are specified in the routine's interface below @p dataOutSize and @p dataInSize parameters.
 *
 * @param[in,out]   userData        User data which is set using @ref QLIB_SetUserData
 * @param[in]       format          SPI format
 * @param[in]       flags           SPI flags, including DTR flags. For supported flags refer to QLIB_SPI_FLAGS definitions above.
 * @param[in]       dataOutStream   pointer to a buffer with SPI output information: SPI command followed by address and  dataOut
 * @param[in]       cmdSize         Number of SPI command bytes in dataOutStream buffer
 * @param[in]       addressSize     Number of address bytes in dataOutStream buffer
 * @param[in]       dataOutSize     Number of dataOut bytes in dataOutStream buffer.
 * @param[in]       dummyCycles     Dummy cycles between write and read phases
 * @param[out]      dataIn          pointer to a buffer which holds the data received
 * @param[in]       dataInSize      data received size in bytes.
 *
 * @return
 * QLIB_STATUS__OK = 0                      - no error occurred\n
 * QLIB_STATUS__(ERROR)                     - Other error
************************************************************************************************************/
int PLAT_SPI_WriteReadTransaction(const void*     userData,
                                  QLIB_BUS_MODE_T format,
                                  uint32_t        flags,
                                  const uint8_t*  dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize,
                                  uint32_t        dataOutSize,
                                  uint32_t        dummyCycles,
                                  uint8_t*        dataIn,
                                  uint32_t        dataInSize);

#ifdef QLIB_SPI_OPTIMIZATION_ENABLED

/************************************************************************************************************
 * @brief       This routine performs SPI multi transaction start.
 * When using the same SPI command, `PLAT_SPI_MultiTransactionStart` saves the command in dedicated SPI cache.\n
 * Next time the command will be called it will be taken directly from the SPI cache thus saving calculation time
************************************************************************************************************/
void PLAT_SPI_MultiTransactionStart(void);

/************************************************************************************************************
 * @brief       This routine performs SPI multi transaction stop.
 * This function stops the usage of SPI cache saved while calling `PLAT_SPI_MultiTransactionStart`
************************************************************************************************************/
void PLAT_SPI_MultiTransactionStop(void);

#endif //QLIB_SPI_OPTIMIZATION_ENABLED

#ifdef __cplusplus
}
#endif

#endif // __QLIB_PLATFORM_H__
