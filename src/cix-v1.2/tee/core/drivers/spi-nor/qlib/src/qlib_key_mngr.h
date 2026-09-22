/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_key_mngr.h
* @brief      This file contains QLIB Key Manager interface
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_KEY_MNGR_H__
#define __QLIB_KEY_MNGR_H__

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
/*                                                 MACROS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define QLIB_KEY_MNGR__GET_KEY_TYPE(kid)              ((((U8)(kid) < 0x40u) ? ((U8)(kid)&0xF0u) : ((U8)(kid))))
#define QLIB_KEY_MNGR__GET_KEY_SECTION(kid)           ((kid)&0x0Fu)
#define QLIB_KEY_MNGR__KID_WITH_SECTION(kid, section) (((U8)(kid) | (U8)(section)))
#define QLIB_KEY_MNGR__INVALIDATE_KEY(key)            memset((key), 0, sizeof(KEY_T))
#define QLIB_KEY_MNGR__SESSION_IS_OPEN(qlibContext)   (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid != (U8)QLIB_KID__INVALID)
#define QLIB_KEY_MNGR__IS_KEY_VALID(key) \
    (((key) != NULL) && (((key)[0] != 0u) || ((key)[1] != 0u) || ((key)[2] != 0u) || ((key)[3] != 0u)))
#define QLIB_KEY_MNGR__IS_LMS_KEY_VALID(key) \
    (((key) != NULL) &&                      \
     (((key)[4] != 0u) || ((key)[5] != 0u) || ((key)[6] != 0u) || ((key)[7] != 0u) || ((key)[8] != 0u) || ((key)[9] != 0u)))
#define QLIB_KEY_MNGR__GET_SECTION_KEY_FULL_ACCESS(qlibContext, section) \
    (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.fullAccessKeys[section])
#define QLIB_KEY_MNGR__GET_SECTION_KEY_RESTRICTED(qlibContext, section) \
    (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.restrictedKeys[section])

#define QLIB_KEY_ID_IS_PROVISIONING(kid)                                         \
    ((QLIB_KEY_MNGR__GET_KEY_TYPE(kid) == (U8)QLIB_KID__SECTION_PROVISIONING) || \
     (QLIB_KEY_MNGR__GET_KEY_TYPE(kid) == (U8)QLIB_KID__DEVICE_KEY_PROVISIONING))

#define QLIB_KEYMNGR_IS_SECTION_FULL_ACCESS(qlibContext, sectionId) \
    (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid ==              \
     QLIB_KEY_MNGR__KID_WITH_SECTION((U32)QLIB_KID__FULL_ACCESS_SECTION, sectionId))

#define QLIB_KEY_MNGR_IS_SECTION_RESTRICTED_ACCESS(qlibContext, sectionId) \
    (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid ==                     \
     QLIB_KEY_MNGR__KID_WITH_SECTION((U32)QLIB_KID__RESTRICTED_ACCESS_SECTION, sectionId))

/*---------------------------------------------------------------------------------------------------------*/
/* Stores new key to Key Manager                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_KEYMNGR_SetKey(keyMngr, key, sectionID, fullAccess)                                                  \
    {                                                                                                             \
        CONST_KEY_P_T* __keys = (((fullAccess) == TRUE) ? (keyMngr)->fullAccessKeys : (keyMngr)->restrictedKeys); \
        __keys[(sectionID)]   = (key);                                                                            \
    }

/*---------------------------------------------------------------------------------------------------------*/
/* Retrieves key from Key Manager                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_SEC_KEYMNGR_GetKey(keyMngr, key, sectionID, fullAccess)                                                         \
    {                                                                                                                        \
        *(key) = (((fullAccess) == TRUE) ? (keyMngr)->fullAccessKeys[(sectionID)] : (keyMngr)->restrictedKeys[(sectionID)]); \
    }

/*---------------------------------------------------------------------------------------------------------*/
/* Removes key from Key Manager                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_KEYMNGR_RemoveKey(keyMngr, sectionID, fullAccess) QLIB_KEYMNGR_SetKey((keyMngr), NULL, (sectionID), (fullAccess))

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief           This routine performs Key Manager init
 *
 * @param[in,out]   keyMngr   Key manager context
 *
 * @return
 * QLIB_STATUS__OK = 0                      - no error occurred\n
 * QLIB_STATUS__(ERROR)                     - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB_KEYMNGR_Init(QLIB_KEY_MNGR_T* keyMngr);

#ifdef __cplusplus
}
#endif

#endif // __QLIB_KEY_MNGR_H__
