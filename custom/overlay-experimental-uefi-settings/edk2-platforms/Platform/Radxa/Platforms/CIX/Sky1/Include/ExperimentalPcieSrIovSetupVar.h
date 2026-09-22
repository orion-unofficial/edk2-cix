#ifndef _EXPERIMENTAL_PCIE_SRIOV_SETUP_VAR_H_
#define _EXPERIMENTAL_PCIE_SRIOV_SETUP_VAR_H_

#define EXPERIMENTAL_PCIE_SRIOV_SETUP_VAR_NAME  L"ExperimentalPcieSrIovSetupVar"

#define EXPERIMENTAL_PCIE_SRIOV_SETUP_VARIABLE_GUID \
  { 0x7f2657cc, 0xad2f, 0x4ca8, { 0xa7, 0x11, 0x8f, 0xb1, 0x35, 0x6d, 0x0d, 0xe0 } }

#pragma pack(1)
typedef struct {
  UINT8    SrIovSupport;
  UINT8    Reserved[3];
} EXPERIMENTAL_PCIE_SRIOV_SETUP_DATA;
#pragma pack()

#endif
