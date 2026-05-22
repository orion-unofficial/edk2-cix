/** @file

  Copyright 2024-2025 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Include/AcpiScmi.h>

// External CPU device declarations
External (\_SB.CPB0, PkgObj)          //(CPU8, CPU9)
External (\_SB.CPB1, PkgObj)          //(CPU10, CPU11)
External (\_SB.CPM0, PkgObj)          //(CPU4, CPU5)
External (\_SB.CPM1, PkgObj)          //(CPU6, CPU7)

Name (TPDM, 0) /* Thermal power model: 0 = vendor ACPI, 1 = DTB-derived */

// Temperature conversion method (Celsius to Kelvin)
// Celsius to Kelvin conversion formula: K = 10 * C + 2732
Method(C2DK, 1, Serialized) {
  Multiply(Arg0, 10, Local0)
  Add(Local0, 2732, Local0)
  Return(Local0)
}

// Thermal Zone for CPU-B0
ThermalZone(TZB0) {
  Method(_PSV) { Return(3582) }       // Passive trip point: 85°C
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TC1) { Return(4) }          // Thermal Constant1
  Method(_TC2) { Return(3) }          // Thermal Constant2
  Method(_TSP) { Return(1) }          // Sampling Period: 100ms
  Method(_PSL) { Return(\_SB.CPB0)}   // Passive cooling list
  Method(SWIT) { Return(3332) }       // Switch-On trip point: 60°C
  Method(SSTP) {                      // sustainable power in mW
    If (LEqual (TPDM, 1)) {
      Return(5500)
    }
    Return(12000)
  }
  // Method(_TZD) {}                     // Thermal Zone Devices
  Method(_TMP, 0, Serialized) {       // Temperature reading
    Store(\_SB.PMMX.SENG(CPU_B0_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_SCP, 1, Serialized) {}      // Set Cooling Policy
  Method(_TZP) { Return(10) }         // Polling Interval: 1000ms
#ifdef ENABLE_FIRMWARE_FIXES
  Name (_STR, Unicode ("CPU Big Cluster 0 (CPU 8-9)"))
#else
  Name (_STR, Unicode ("CPU-B0"))
#endif
}

// Thermal Zone for CPU-B1
ThermalZone(TZB1) {
  Method(_PSV) { Return(3582) }       // Passive trip point: 85°C
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TC1) { Return(4) }          // Thermal Constant1
  Method(_TC2) { Return(3) }          // Thermal Constant2
  Method(_TSP) { Return(1) }          // Sampling Period: 100ms
  Method(_PSL) { Return(\_SB.CPB1) }  // Passive cooling list
  Method(SWIT) { Return(3332) }       // Switch-On trip point: 60°C
  Method(SSTP) {                      // sustainable power in mW
    If (LEqual (TPDM, 1)) {
      Return(6000)
    }
    Return(12000)
  }
  // Method(_TZD) {}                     // Thermal Zone Devices
  Method(_TMP, 0, Serialized) {       // Temperature reading
    Store(\_SB.PMMX.SENG(CPU_B1_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_SCP, 1, Serialized) {}      // Set Cooling Policy
  Method(_TZP) { Return(10) }         // Polling Interval: 1000ms
#ifdef ENABLE_FIRMWARE_FIXES
  Name (_STR, Unicode ("CPU Big Cluster 1 (CPU 10-11)"))
#else
  Name (_STR, Unicode ("CPU-B1"))
#endif
}

// Thermal Zone for CPU-M0
ThermalZone(TZM0) {
  Method(_PSV) { Return(3582) }       // Passive trip point: 85°C
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TC1) { Return(4) }          // Thermal Constant1
  Method(_TC2) { Return(3) }          // Thermal Constant2
  Method(_TSP) { Return(1) }          // Sampling Period: 100ms
  Method(_PSL) { Return(\_SB.CPM0) }  // Passive cooling list
  Method(SWIT) { Return(3332) }       // Switch-On trip point: 60°C
  Method(SSTP) {                      // sustainable power in mW
    If (LEqual (TPDM, 1)) {
      Return(5000)
    }
    Return(10000)
  }
  // Method(_TZD) {}                     // Thermal Zone Devices
  Method(_TMP, 0, Serialized) {       // Temperature reading
    Store(\_SB.PMMX.SENG(CPU_M0_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_SCP, 1, Serialized) {}      // Set Cooling Policy
  Method(_TZP) { Return(10) }         // Polling Interval: 1000ms
#ifdef ENABLE_FIRMWARE_FIXES
  Name (_STR, Unicode ("CPU Mid Cluster 0 (CPU 4-5)"))
#else
  Name (_STR, Unicode ("CPU-M0"))
#endif
}

// Thermal Zone for CPU-M1
ThermalZone(TZM1) {
  Method(_PSV) { Return(3582) }       // Passive trip point: 85°C
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TC1) { Return(4) }          // Thermal Constant1
  Method(_TC2) { Return(3) }          // Thermal Constant2
  Method(_TSP) { Return(1) }          // Sampling Period: 100ms
  Method(_PSL) { Return(\_SB.CPM1) }  // Passive cooling list
  Method(SWIT) { Return(3332) }       // Switch-On trip point: 60°C
  Method(SSTP) {                      // sustainable power in mW
    If (LEqual (TPDM, 1)) {
      Return(4500)
    }
    Return(9000)
  }
  // Method(_TZD) {}                     // Thermal Zone Devices
  Method(_TMP, 0, Serialized) {       // Temperature reading
    Store(\_SB.PMMX.SENG(CPU_M1_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_SCP, 1, Serialized) {}      // Set Cooling Policy
  Method(_TZP) { Return(10) }         // Polling Interval: 1000ms
#ifdef ENABLE_FIRMWARE_FIXES
  Name (_STR, Unicode ("CPU Mid Cluster 1 (CPU 6-7)"))
#else
  Name (_STR, Unicode ("CPU-M1"))
#endif
}

//
// Name: SPFA [Set PM Fan to Auto Mode]
// Description: Function to set PM fan to auto mode
// Input: None
// Output: None
//
Method(SPFA, 0, Serialized){
  \_SB.PMMX.SFMD(1)
}

//
// Name: SPFM [Set PM Fan to Mute Mode]
// Description: Function to set PM fan to mute mode
// Input: None
// Output: None
//
Method(SPFM, 0, Serialized){
  \_SB.PMMX.SFMD(0)
}

//
// Name: SPFP [Set PM Fan to Performance Mode]
// Description: Function to set PM fan to performance mode
// Input: None
// Output: None
//
Method(SPFP, 0, Serialized){
  \_SB.PMMX.SFMD(2)
}

OperationRegion (IPBF, SystemMemory, 0x83BF0300, 0x400)
Field (IPBF, ByteAcc, NoLock, Preserve)
{
  Offset (0x0),
  BUF_, 8192
}
// static power of CPU
Method (SPRG, 1, Serialized)
{
    // static_power offset = 0x3c + CPU index * 0x40
    Multiply (Arg0, 0x40, Local0)
    Add (Local0, 0x3c, Local1)

    CreateDWordField (BUF_, Local1, SPWR)
    Return (SPWR)
}


// dynamic power of CPU
Method (DPRG, 1, Serialized)
{
    // dynamic_power offset = 0x38 + CPU index * 0x40
    Multiply (Arg0, 0x40, Local0)
    Add (Local0, 0x38, Local1)

    CreateDWordField (BUF_, Local1, DPWR)
    Return (DPWR)
}

// Thermal Zone for GPU
ThermalZone(TZGT) {
  Method(_PSV) { Return (3582) }      // Passive trip point: 85°C
  Method(SWIT) { Return(3432) }       // Switch-On trip point: 70°C
  Method(SSTP) { Return(15000) }      // sustainable power in mW
  Method(_TC1) { Return(4) }          // Thermal Constant1
  Method(_TC2) { Return(3) }          // Thermal Constant2
  Method(_TSP) { Return(1) }          // Sampling Period: 100ms
  Name(_PSL, Package (){ \_SB.GPU })  // Passive cooling list
#ifdef ENABLE_FIRMWARE_FIXES
  Name(_TZD, Package (){ \_SB.GPU })  // Thermal Zone Devices
  Name (_STR, Unicode ("GPU Average"))
#else
  // Method(_TZD) {}                  // Thermal Zone Devices
#endif
  Method(_TMP, 0, Serialized) {       // Temperature reading
    Store(\_SB.PMMX.SENG(GPU_AVERAGE_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_SCP, 1, Serialized) {}      // Set Cooling Policy
  Method(_TZP) { Return(10) }         // Polling Interval: 1000ms
}


#ifdef ENABLE_FIRMWARE_FIXES
// Additional DTB/MemoryMap-backed SoC thermal monitor zones.
ThermalZone(TZVP) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(VPU_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("VPU"))
}

ThermalZone(TZGB) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Name(_TZD, Package (){ \_SB.GPU })
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(GPU_BOTTOM_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("GPU Bottom"))
}

ThermalZone(TZGP) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Name(_TZD, Package (){ \_SB.GPU })
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(GPU_TOP_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("GPU Top"))
}

ThermalZone(TZBR) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(SOC_BRC_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("SoC Bridge"))
}

ThermalZone(TZD0) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(DDR_BOTTOM4_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("DDR Bottom"))
}

ThermalZone(TZD1) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(DDR_TOP_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("DDR Top"))
}

ThermalZone(TZCI) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(CI700_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("CI700 Interconnect"))
}

ThermalZone(TZNP) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(NPU_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("NPU"))
}

ThermalZone(TZTR) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(SOC_TRC_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("SoC Trace"))
}

ThermalZone(TZN0) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(NTC0_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("Board NTC 0"))
}

ThermalZone(TZN1) {
  Method(_CRT) { Return(0x0E80) }       // Critical trip point: 98°C
  Method(_TMP, 0, Serialized) {
    Store(\_SB.PMMX.SENG(NTC1_TEMP_SENSOR_ID, 0), Local0)
    CreateDWordField(Local0, 0x00, STAT)
    If (STAT == SCMI_SUCCESS) {
      CreateQWordField(Local0, 0x04, TEMP)
      TEMP = ToInteger(TEMP)
      Return(C2DK(TEMP))
    } Else {
      Return (0xFFFFFFFFFFFFFFFF)
    }
  }
  Method(_TZP) { Return(10) }
  Name (_STR, Unicode ("Board NTC 1"))
}
#endif
