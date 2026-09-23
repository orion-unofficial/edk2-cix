#!/usr/bin/env python3
"""Compile the real PPTT cache initializer with the selected ACPI structures."""
import unittest

from test_firmware_update_safety import source, function, run_c, PRELUDE
from test_firmware_acpi_runtime import preprocess

PATH = 'src/edk2-platforms/Platform/CIX/Sky1/Library/Acpi/CIX/AcpiPpttLibCIX/PpttGenerator.c'


class PpttCachePolicyTests(unittest.TestCase):
    def test_instruction_cache_does_not_claim_a_data_write_policy(self):
        text = source(PATH)
        version = '6_4' if 'EFI_ACPI_6_4_PPTT_STRUCTURE_CACHE' in text else '6_3'
        header = source('src/edk2/MdePkg/Include/IndustryStandard/Acpi' + version.replace('_', '') + '.h')
        header = header.replace('EFI_ACPI_' + version, 'EFI_ACPI_6_4')
        initializer = function(text, 'InitCacheNode').replace('EFI_ACPI_' + version, 'EFI_ACPI_6_4')
        start = header.index('#define EFI_ACPI_6_4_PPTT_CACHE_SIZE_INVALID')
        end = header.index('} EFI_ACPI_6_4_PPTT_STRUCTURE_CACHE;') + len('} EFI_ACPI_6_4_PPTT_STRUCTURE_CACHE;')
        harness = PRELUDE + r'''
#define ASSERT assert
#define ZeroMem(p,n) memset(p,0,n)
#define CIX_PPTT_CACHE_LINE_SIZE 64
#define EFI_ACPI_6_4_PPTT_TYPE_CACHE 1
''' + header[start:end] + initializer + r'''
int main(void) {
 void *p=allocate(1);release(p);
 EFI_ACPI_6_4_PPTT_STRUCTURE_CACHE node;
 for(UINT8 type=0;type<=3;type++) {
  memset(&node,0xff,sizeof(node));InitCacheNode(&node,0x124,32768,128,4,0,type);
  assert(node.Type==1 && node.Length==sizeof(node));
  assert(node.Flags.WritePolicyValid==(type!=EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION));
  assert(node.Flags.SizePropertyValid && node.Flags.NumberOfSetsValid && node.Flags.AssociativityValid);
  assert(node.Flags.AllocationTypeValid && node.Flags.CacheTypeValid && node.Flags.LineSizeValid);
  assert(!node.Flags.CacheIdValid && !node.Flags.Reserved);
  assert(node.NextLevelOfCache==0x124 && node.Size==32768 && node.NumberOfSets==128 && node.Associativity==4);
  assert(node.Attributes.CacheType==type && !node.Attributes.AllocationType && !node.Attributes.WritePolicy);
  assert(node.LineSize==64 && node.CacheId==0);
 }
 return 0;
}
'''
        if version == '6_3':
            harness = harness.replace('!node.Flags.CacheIdValid && ', '').replace(' && node.CacheId==0', '')
        run_c(self, harness)
        self.assertNotIn('InitCacheNode (', preprocess(text, False))
        self.assertIn('InitCacheNode (', preprocess(text, True))


if __name__ == '__main__':
    unittest.main()
