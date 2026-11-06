/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_version.h
* @brief      This file contains the version number of QLib
*
* ### project qlib
*
************************************************************************************************************/
#ifndef _QLIB_VERSION_H_
#define _QLIB_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define QLIB_VERSION_MAJOR     0
#define QLIB_VERSION_MINOR     19
#define QLIB_VERSION_PATCH     1
#define QLIB_VERSION_INTERNAL  0

#define QLIB_VERSION         MAKE_32_BIT(QLIB_VERSION_INTERNAL, QLIB_VERSION_PATCH, QLIB_VERSION_MINOR, QLIB_VERSION_MAJOR)
#define QLIB_VERSION_STR     STRINGX(QLIB_VERSION_MAJOR) "." STRINGX(QLIB_VERSION_MINOR) "." STRINGX(QLIB_VERSION_PATCH) "." STRINGX(QLIB_VERSION_INTERNAL)
// clang-format on

#ifdef __cplusplus
}
#endif

#endif //QLIB_VERSION_STR
