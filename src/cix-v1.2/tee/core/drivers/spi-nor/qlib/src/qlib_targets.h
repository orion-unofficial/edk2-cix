/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_targets.h
* @brief      This file contains qlib targets definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_TARGETS_H__
#define __QLIB_TARGETS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                DEFINITIONS                                              */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define w77q128jw_revA (0x1u)
#define w77q64jw_revA  (0x4u)
#define w77q128jv_revA (0x8u)
#define w77q64jv_revA  (0x10u)
#define w77q32jw_revB  (0x20u)
#define w77q25nw_Ind_revB  (0x80u)
#define w77q25nw_Auto_revB (0x100u)
#define w77t25nw_Ind_revB  (0x200u)
#define w77t25nw_Auto_revB (0x400u)

#define all_q2_targets (w77q128jw_revA | w77q64jw_revA | w77q128jv_revA | w77q64jv_revA | w77q32jw_revB)
#define all_q3_targets (w77q25nw_Ind_revB | w77q25nw_Auto_revB | w77t25nw_Ind_revB | w77t25nw_Auto_revB)
#define all_targets (all_q2_targets | all_q3_targets)

#ifndef QLIB_TARGET
#define QLIB_TARGET all_targets
#endif

#if ((QLIB_TARGET & all_targets) == 0)
#error "wrong QLIB_TARGET"
#endif

#define QLIB_TARGET_SUPPORTED(target) ((U32)(target) & (U32)QLIB_TARGET)

#ifdef __cplusplus
}
#endif

#endif // __QLIB_TARGETS_H__
