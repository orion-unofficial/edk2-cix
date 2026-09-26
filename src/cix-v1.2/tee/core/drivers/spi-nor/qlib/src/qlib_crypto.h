/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_crypto.h
* @brief      This file contains QLIB cryptographic functions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_CRYPTO_H__
#define __QLIB_CRYPTO_H__

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
/*                                                DEFINES                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 MACROS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/* Handling CTAG                                                                                           */
/* CTAG is build as it will be put on W77Q (16Mb to 128Mb) input buffer                                    */
/*                                                                                                         */
/* ADDR (24b)        =      0x ADD2 ADD1 ADD0                                                              */
/* CTAG_ADDR   (32b) = 0x ADD2 ADD1 ADD0 CMD                                                               */
/* CTAG_PARAMS (32b) = 0x P3   P2   P1   CMD                                                               */
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/* Swap address byte order for SPI                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#define Q2_SWAP_24BIT_ADDR_FOR_SPI(addr) MAKE_32_BIT(BYTE(addr, 2), BYTE(addr, 1), BYTE(addr, 0), 0)

/*---------------------------------------------------------------------------------------------------------*/
/* Make CTAG from cmd and bytes                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_CMD_PROC__MAKE_CTAG_PARAMS(cmd, b1, b2, b3) MAKE_32_BIT(cmd, b1, b2, b3)

/*---------------------------------------------------------------------------------------------------------*/
/* Make CTAG from cmd and address                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_CMD_PROC__MAKE_CTAG_ADDR(cmd, addr) MAKE_32_BIT(cmd, BYTE(addr, 0), BYTE(addr, 1), BYTE(addr, 2))

/************************************************************************************************************
 * Make CTAG from cmd and mode
************************************************************************************************************/
#define QLIB_CMD_PROC__MAKE_CTAG_MODE(cmd, mode) MAKE_32_BIT(cmd, mode, 0, 0)

/*---------------------------------------------------------------------------------------------------------*/
/* Make CTAG from cmd                                                                                      */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_CMD_PROC__MAKE_CTAG(cmd) ((U32)(cmd))

/*---------------------------------------------------------------------------------------------------------*/
/* Get CMD from CTAG                                                                                       */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_CMD_PROC__CTAG_GET_CMD(ctag_U32) BYTE(ctag_U32, 0)

/*---------------------------------------------------------------------------------------------------------*/
/* Salt SSK with TC                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_CRYPTO_put_salt_on_session_key(TC_counter_U32, in_out_ssk_128BIT, session_key) \
    {                                                                                       \
        (in_out_ssk_128BIT)[3] = (session_key)[3] ^ (TC_counter_U32);                       \
    }

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  TYPES                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * or 0xA3 for decryption of input data (e.g.: payload of SAWR, SERASE commands).
 * DIR is the direction code: 0x5C for encryption of output data (e.g.: response to SRD, CALC_SIG commands)
************************************************************************************************************/
typedef enum
{
    DECRYPTION_OF_INPUT_DIR_CODE  = 0xA3U, ///<SAWR, SERASE
    ENCRYPTION_OF_OUTPUT_DIR_CODE = 0x5CU  ///< response to SRD , CALC_SIG
} QLIB_DIRECTION_E;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                            INTERFACE MACROS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This macro returns address encryption cipher
 *
 * @param[in]   cipher_key   Cipher key
 *
 * @return      Address encryption cipher
************************************************************************************************************/
#define QLIB_CRYPTO_CreateAddressKey(cipher_key) ((cipher_key)[0] ^ (cipher_key)[1] ^ (cipher_key)[2] ^ (cipher_key)[3])

/************************************************************************************************************
 * @brief This macro returns random bits
 * @param[in]   prng    PRNG state
 * @param[in]   bits   Number of random bits
 * @return      Random value with required number of random bits
************************************************************************************************************/
#define QLIB_CRYPTO_GetRandBits(prng, bits) (U32)(QLIB_CRYPTO_GetRand32(prng) & _BUILD_FIELD_MASK(bits, 0))

#define __QLIB_CRYPTO_EncryptData_ENTRY(i, dst, src, cipher_key) ((dst)[(i)] = (src)[(i)] ^ (cipher_key)[(i)])

/************************************************************************************************************
 * @brief This macro performs inline data encryption
 * @param[out]  dst         Destination buffer
 * @param[in]   src         Source buffer
 * @param[in]   cipher_key  Cipher key buffer
 * @param[in]   count       Number of iterations
************************************************************************************************************/
#define QLIB_CRYPTO_EncryptData_INLINE(dst, src, cipher_key, count) \
    REPEAT_##count(EVAL(__QLIB_CRYPTO_EncryptData_ENTRY), (dst), (src), (cipher_key))

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief         This routine generates cipher key, use a pre filed buffer
 *
 * @param[in,out] hashBuf      buffer for the HASH input data with the SSK in its first 128Bits,
 *                             Note: this parameter will be modified by this function
 * @param[in]     key_id       KID
 * @param[in]     dir          Encryption direction
 * @param[out]    cipher_key   Cipher key
************************************************************************************************************/
void QLIB_CRYPTO_BuildCipherKey(QLIB_HASH_BUF_T hashBuf, U8 key_id, QLIB_DIRECTION_E dir, _256BIT cipher_key);

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED
/************************************************************************************************************
 * @brief         This routine generates starts asynchronous cipher key generation
 *
 * @param[in,out] qlibContext  QLIB internal state
 * @param[in,out] hashBuf      buffer for the HASH input data with the SSK in its first 128Bits,
 *                             Note: this parameter will be modified by this function
 * @param[in]     key_id       KID
 * @param[in]     dir          Encryption direction
 * @param[out]    cipher_key   Cipher key
 ************************************************************************************************************/
void QLIB_CRYPTO_BuildCipherKey_Async(QLIB_CONTEXT_T*  qlibContext,
                                      QLIB_HASH_BUF_T  hashBuf,
                                      U8               key_id,
                                      QLIB_DIRECTION_E dir,
                                      _256BIT          cipher_key);
#endif

/************************************************************************************************************
 * @brief       This routine performs data encryption
 *
 * @param[out]  dst           Buffer to store the encrypted data
 * @param[in]   src           Data to encrypt
 * @param[in]   cipher_key    Cipher key
 * @param[in]   data_size     Input data size
************************************************************************************************************/
void QLIB_CRYPTO_EncryptData(U32* dst, const U32* src, const U32* cipher_key, U32 data_size);

/************************************************************************************************************
 * @brief         This routine calculates command signature, use a pre filed buffer
 *
 * @param[in,out] hashBuf     buffer for the HASH input data,
 *                            SSK, CTAG and data to sign should be set as part of this parameter.
 *                            Note: this parameter will be modified by this function
 * @param[in]     key_id      KID
 * @param[out]    signature   Signature
************************************************************************************************************/
void QLIB_CRYPTO_CalcAuthSignature(QLIB_HASH_BUF_T hashBuf, U8 key_id, _64BIT signature);

/************************************************************************************************************
 * @brief       This routine calculates provisioning key
 *
 * @param[in]   key_id              KID
 * @param[in]   Kd_128bit           Device Key
 * @param[in]   includeWID          if TRUE, Winbond ID is used
 * @param[in]   ignoreScrValidity   if TRUE, SCR validity is ignored
 * @param[out]  prov_key_128_bit    Provisioning Key
************************************************************************************************************/
void QLIB_CRYPTO_GetProvisionKey(U8          key_id,
                                 const KEY_T Kd_128bit,
                                 BOOL        includeWID,
                                 BOOL        ignoreScrValidity,
                                 KEY_T       prov_key_128_bit);

/************************************************************************************************************
 * @brief       This routine calculates session key and signature
 *
 * @param[in]   key           Master key
 * @param[in]   ctag          CTAG
 * @param[in]   MC            Monotonic counter
 * @param[in]   NONCE         Nonce
 * @param[in]   WID           WID
 * @param[out]  session_key   Session Key
 * @param[out]  cmd_sig       Command signature
 * @param[out]  random_seed   Random seed
************************************************************************************************************/
void QLIB_CRYPTO_SessionKeyAndSignature(const _256BIT   key,
                                        U32             ctag,
                                        const QLIB_MC_T MC,
                                        const _64BIT    NONCE,
                                        const _64BIT    WID,
                                        _128BIT         session_key,
                                        _64BIT          cmd_sig,
                                        _64BIT          random_seed);

/************************************************************************************************************
 * @brief           This function reseeds prng
 *
 * @param[in,out]   prng   PRNG state
************************************************************************************************************/
void QLIB_CRYPTO_Reseed(QLIB_PRNG_STATE_T* prng);

/************************************************************************************************************
 * @brief           This function returns pseudo-random 32bit value
 *
 * @param[in,out]   prng   PRNG state
 *
 * @return          32bit pseudo-random value
************************************************************************************************************/
U32 QLIB_CRYPTO_GetRand32(QLIB_PRNG_STATE_T* prng);


#ifdef __cplusplus
}
#endif

#endif // __QLIB_CRYPTO_H__
