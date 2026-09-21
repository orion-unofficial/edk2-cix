/** @file

  Copyright 2026 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

Device (CI70) {
  Name (_HID, "CIXHA030")
  Name (_UID, 0x0)
  Name (_STA, 0xF)

  Name (_DSD, Package () {
    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
    Package () {
        Package () {"compatible", "cix,bus-ci700"},
        Package () {"power-domains", Package () {\_SB.SCMI.DVFS, 10}},
        Package () {"power-domain-names", Package () {"perf"}},
    }
  })
}

Device (MMHB) {
  Name (_HID, "CIXHA031")
  Name (_UID, 0x0)
  Name (_STA, 0xF)

  Name (_DSD, Package () {
    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
    Package () {
        Package () {"compatible", "cix,bus-ni700"},
        Package () {"power-domains", Package () {\_SB.SCMI.DVFS, 11}},
        Package () {"power-domain-names", Package () {"perf"}},
    }
  })
}
