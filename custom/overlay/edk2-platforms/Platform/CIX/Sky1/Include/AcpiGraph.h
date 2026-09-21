/** @file
  ACPI graph endpoints use the complete namespace path of the remote package.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef CIX_ACPI_GRAPH_H_
#define CIX_ACPI_GRAPH_H_
#define CIX_ASL_STRING_RAW(Value) #Value
#define CIX_ASL_STRING(Value) CIX_ASL_STRING_RAW(Value)
#ifdef ENABLE_FIRMWARE_FIXES
#define CIX_GRAPH_REMOTE4(Dev, Pipeline, Port, Endpoint, AslEndpoint) CIX_ASL_STRING(Dev.AslEndpoint)
#define CIX_GRAPH_REMOTE3(Dev, Port, Endpoint, AslEndpoint) CIX_ASL_STRING(Dev.AslEndpoint)
#else
#define CIX_GRAPH_REMOTE4(Dev, Pipeline, Port, Endpoint, AslEndpoint) Package () { Dev, Pipeline, Port, Endpoint }
#define CIX_GRAPH_REMOTE3(Dev, Port, Endpoint, AslEndpoint) Package () { Dev, Port, Endpoint }
#endif
#endif
