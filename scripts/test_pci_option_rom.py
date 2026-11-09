#!/usr/bin/env python3
"""Compile production ProcessOptionRom with synthetic PCI lists and a ROM loader.

Select an unpublished descendant with SOURCE_TEST_REPO and SOURCE_TEST_REF, or
SOURCE_TEST_ROOT for a checkout. No firmware algorithm is reimplemented here.
"""
import json
import os
from pathlib import Path
import posixpath
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = 'edk2-platforms/Platform/CIX/Sky1/Drivers/Pci/PciBusDxe/'


def source(path):
    if os.environ.get('SOURCE_TEST_ROOT'):
        return (Path(os.environ['SOURCE_TEST_ROOT']) / path).read_text()
    repo = os.environ.get('SOURCE_TEST_REPO', str(ROOT))
    ref = os.environ.get('SOURCE_TEST_REF')
    if not ref:
        records = json.loads((ROOT / 'config/refs-unofficial.json').read_text())
        records = records['refs'] if isinstance(records, dict) else records
        ref = next(row['object_id'] for row in records
                   if row['ref'] == 'source/unofficial/1.3.2/edk2-stable202608')
    for _ in range(8):
        entry = subprocess.check_output(['git', '-C', repo, 'ls-tree', ref, '--', path], text=True)
        text = subprocess.check_output(['git', '-C', repo, 'show', ref + ':' + path], text=True)
        if not entry.startswith('120000 '):
            return text
        path = posixpath.normpath(posixpath.join(posixpath.dirname(path), text))
    raise AssertionError('overlay symlink cycle')


def production(text):
    """Extract verbatim the actual blacklist declaration and complete function."""
    table = re.search(r'typedef struct _PCI_BAD_DEVICE \{.*?\n\};', text, re.S)
    function = re.search(r'\nVOID\nProcessOptionRom \(.*?\n\}', text, re.S)
    if not table or not function:
        raise AssertionError('production blacklist/function not found')
    return table.group() + '\n' + function.group() + '\n'


PREAMBLE = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define VOID void
#define IN
#define TRUE 1
#define FALSE 0
#define DEBUG(x) do {} while (0)
#define FixedPcdGetBool(token) _PCD_VALUE_##token
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint64_t UINT64;
typedef uint8_t BOOLEAN;
typedef struct list { struct list *ForwardLink, *BackLink; } LIST_ENTRY;
typedef struct {
  LIST_ENTRY Link, ChildList;
  UINT64 RomSize;
  struct { struct { UINT16 VendorId, DeviceId; } Hdr; } Pci;
  unsigned Id;
} PCI_IO_DEVICE;
#define PCI_IO_DEVICE_FROM_LINK(p) ((PCI_IO_DEVICE *)((char *)(p) - offsetof(PCI_IO_DEVICE, Link)))
static int IsListEmpty(LIST_ENTRY *p) { return p->ForwardLink == p; }
static unsigned loaded[32], count;
static void LoadOpRomImage(PCI_IO_DEVICE *p, UINT64 base) {
  assert(base == 0x80000000);
  assert(count < 32);
  loaded[count++] = p->Id;
}
'''

# Only topology construction and expected loader calls live in the test.
HARNESS = r'''
static void init(PCI_IO_DEVICE *p, unsigned id, UINT16 vendor, UINT16 device, UINT64 size) {
  memset(p, 0, sizeof(*p));
  p->Id=id; p->Pci.Hdr.VendorId=vendor; p->Pci.Hdr.DeviceId=device; p->RomSize=size;
  p->ChildList.ForwardLink=p->ChildList.BackLink=&p->ChildList;
}
static void child(PCI_IO_DEVICE *parent, PCI_IO_DEVICE *p) {
  LIST_ENTRY *head=&parent->ChildList;
  p->Link.ForwardLink=head; p->Link.BackLink=head->BackLink;
  head->BackLink->ForwardLink=&p->Link; head->BackLink=&p->Link;
}
static void check(PCI_IO_DEVICE *root, const unsigned *expected, unsigned n) {
  count=0;
  ProcessOptionRom(root, 0x80000000, 4096);
  assert(count == n);
  assert(!memcmp(loaded, expected, n*sizeof(*expected)));
}
int main(void) {
  PCI_IO_DEVICE root, second, p[8];
  const unsigned none[]={0}, one[]={1}, two[]={0,1}, tail[]={1,3};
  const unsigned reverse[]={0}, nested_fixed[]={2,4,5};
  const unsigned nested_first[]={0,2,4,5};
  const UINT16 ids[][2]={{0x1f2e,0x00c1},{0x1d97,0x5220},{0x20c3,0x3118},{0x1000,0x005d}};
  assert(sizeof(mPciBadDeviceList)/sizeof(mPciBadDeviceList[0]) == 4);
  assert(!memcmp(ids, mPciBadDeviceList, sizeof(ids)));
  for (unsigned bad=0;bad<4;bad++) {
    // All requested sibling orders; exact ROM-size boundary qualifies.
    const char *orders[]={"BA", "AB", "BABA", "AA"};
    for (unsigned t=0;t<4;t++) {
      init(&root,99,0,0,0);
      for (unsigned i=0;orders[t][i];i++) {
        int blocked=orders[t][i]=='B';
        init(&p[i],i,blocked?ids[bad][0]:0x1234,blocked?ids[bad][1]:0x5678,4096);
        child(&root,&p[i]);
      }
      if (t==0) check(&root, EXPECT_FIXED?one:none, EXPECT_FIXED?1:0);
      if (t==1) check(&root, reverse, 1);
      if (t==2) check(&root, EXPECT_FIXED?tail:none, EXPECT_FIXED?2:0);
      if (t==3) check(&root, two, 2);
    }
    // A nonqualifying blacklisted ROM must not poison following siblings.
    for (unsigned size=0;size<=8192;size+=8192) {
      init(&root,99,0,0,0);
      init(&p[0],0,ids[bad][0],ids[bad][1],size);
      init(&p[1],1,0x1234,0x5678,1);
      child(&root,&p[0]); child(&root,&p[1]); check(&root,one,1);
    }
    // Matching requires both vendor and device, preserving vendor criteria.
    init(&root,99,0,0,0);
    init(&p[0],0,ids[bad][0],ids[bad][1]^1,1);
    init(&p[1],1,ids[bad][0]^1,ids[bad][1],1);
    child(&root,&p[0]); child(&root,&p[1]); check(&root,two,2);
  }
  // A switch with another bridge: recursion has its own local blacklist state.
  // root: B0, bridge1(B1,A2,bridge2(B3,A4)), A5; visit order is depth-first.
  for (unsigned first=0;first<2;first++) {
    init(&root,99,0,0,0); init(&second,98,0,0,0);
    for (unsigned i=0;i<6;i++) init(&p[i],i,0x1234,0x5678,1);
    p[0].Pci.Hdr.VendorId=first?0x1234:0x1000;
    p[0].Pci.Hdr.DeviceId=first?0x5678:0x005d;
    p[1].Pci.Hdr.VendorId=p[3].Pci.Hdr.VendorId=0x1000;
    p[1].Pci.Hdr.DeviceId=p[3].Pci.Hdr.DeviceId=0x005d;
    PCI_IO_DEVICE bridge1, bridge2;
    init(&bridge1,90,0,0,0); init(&bridge2,91,0,0,0);
    child(&root,&p[0]); child(&root,&bridge1); child(&root,&p[5]);
    child(&bridge1,&p[1]); child(&bridge1,&p[2]); child(&bridge1,&bridge2);
    child(&bridge2,&p[3]); child(&bridge2,&p[4]);
    const unsigned *expected=EXPECT_FIXED?(first?nested_first:nested_fixed):none;
    // Without fixes, an allowed first root endpoint and trailing sibling load.
    const unsigned first_vendor[]={0,5};
    if (!EXPECT_FIXED && first) expected=first_vendor;
    check(&root,expected,EXPECT_FIXED?(first?4:3):(first?2:0));
    // Independent root after a poisoned traversal must still load its endpoint.
    PCI_IO_DEVICE allowed; init(&allowed,1,0x1234,0x5678,1);
    child(&second,&allowed); check(&second,one,1);
  }
  return 0;
}
'''


def run_logic(text, fixed, expected, should_pass=True):
    cc = shutil.which(os.environ.get('CC', 'cc'))
    if not cc:
        raise AssertionError('C compiler required')
    unit = (PREAMBLE + f'\n#define _PCD_VALUE_PcdCustomFirmwareFixesEnable {fixed}\n'
            + production(text) + f'\n#define EXPECT_FIXED {expected}\n' + HARNESS)
    with tempfile.TemporaryDirectory(prefix='pci-option-rom-') as directory:
        path = Path(directory)
        (path / 'probe.c').write_text(unit)
        # Compile exactly the preprocessed production function/table plus mocks.
        result = subprocess.run([cc, '-E', '-P', str(path / 'probe.c')], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)
        (path / 'probe.i').write_text(result.stdout)
        result = subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                                 str(path / 'probe.i'), '-o', str(path / 'probe')],
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)
        result = subprocess.run([str(path / 'probe')], capture_output=True)
        if (result.returncode == 0) != should_pass:
            raise AssertionError(f'production regression exit={result.returncode}: {result.stderr.decode()}')


class PciOptionRomTests(unittest.TestCase):
    def test_vendor_negative_control(self):
        run_logic(source('src/' + MODULE + 'PciEnumerator.c'), 0, 0)

    def test_opt_in_production_path(self):
        custom = source('custom/overlay/' + MODULE + 'PciEnumerator.c')
        vendor = source('src/' + MODULE + 'PciEnumerator.c')
        reset = ('      if (FixedPcdGetBool (PcdCustomFirmwareFixesEnable)) {\n'
                 '        IsBadDev = FALSE;\n      }\n\n')
        self.assertEqual(custom.replace(reset, '', 1), vendor,
                         'only the gated per-device reset should differ')
        run_logic(custom, 1, 1)
        run_logic(custom, 0, 0)
        # Prove that this harness catches removal of the reset and of its gate.
        run_logic(vendor, 1, 1, should_pass=False)
        run_logic(custom.replace(reset, '      IsBadDev = FALSE;\n'), 0, 0, should_pass=False)

    def test_module_declarations(self):
        inf = source('custom/overlay/' + MODULE + 'PciBusDxe.inf')
        self.assertIn('Silicon/CIX/Sky1/CixPkg.dec', inf)
        self.assertRegex(inf, r'\[FixedPcd\]\s+gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable')
        self.assertIn('  PcdLib\n', inf)
        dsc = source('custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc')
        self.assertRegex(dsc, r'!if \$\(ENABLE_FIRMWARE_FIXES\) == TRUE\s+'
                         r'gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable\|TRUE\s+'
                         r'!else\s+gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable\|FALSE')
        dec = source('src/edk2-platforms/Silicon/CIX/Sky1/CixPkg.dec')
        self.assertIn('gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable|FALSE|BOOLEAN|', dec)
        # EDK2 selects the entire module directory through PACKAGES_PATH.
        sources = inf.split('[Sources]', 1)[1].split('[', 1)[0]
        for name in sources.splitlines():
            name = name.strip()
            if name and not name.startswith('#'):
                self.assertTrue(source('custom/overlay/' + MODULE + name))


if __name__ == '__main__':
    unittest.main()
