#!/usr/bin/env python3
"""Exercise the maintained CIX OP-TEE C code with mocked hardware, in Linux.

Run in the buildbox: python3 scripts/test_optee_component.py --source OPTEE_DIR.
These tests compile the actual HOB implementation and extract complete service
functions from the selected tree; they do not simulate a hardware boot.
"""

import argparse
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


SOURCE = None


def function(path, name):
    text = (SOURCE / path).read_text()
    # Definitions in these files start at column zero and end with a bare }.
    import re
    match = re.search(r"^[^\n;{}]*\b" + name + r"\([^;{}]*?\)\n\{\n.*?^\}",
                      text, re.M | re.S)
    if not match:
        raise AssertionError(f"function missing: {path}:{name}")
    # Restrict the initial line rather than retaining preceding file contents.
    value = match.group()
    start = value.rfind('\n', 0, value.index(name)) + 1
    return value[start:]


@unittest.skipUnless(SOURCE, 'pass --source to test the selected OP-TEE component')
class ComponentTests(unittest.TestCase):
    def test_nor_transport_failure_does_not_resume_vendor_driver(self):
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
struct thread_svc_regs { unsigned long x0,x1,x2,x3,x4,x5,x6,x7; };
struct thread_smc_args { unsigned long a0,a1,a2,a3,a4; };
#define SVC_REGS_A1(r) ((r)->x1)
#define SVC_REGS_A2(r) ((r)->x2)
#define SVC_REGS_A3(r) ((r)->x3)
#define SVC_REGS_A4(r) ((r)->x4)
#define SVC_REGS_A5(r) ((r)->x5)
#define GENMASK_64(h,l) ((UINT64_MAX >> (63-(h))) & (UINT64_MAX << (l)))
#define STMM_RET_SUCCESS 0
#define STMM_RET_INVALID_PARAM (-2)
#define TEE_ERROR_STORAGE_NOT_AVAILABLE 0xf0100003U
#define CIX_SIP_NOR_STORAGE 0xc200000bU
#define DMSG_RAW(...) ((void)0)
#define EMSG(...) ((void)0)
static unsigned long smc_status;
static unsigned responses, panics;
static uintptr_t virt_to_phys(void *p) { return (uintptr_t)p; }
static void thread_smccc(struct thread_smc_args *a) {
    assert(a->a0==CIX_SIP_NOR_STORAGE);
    assert(a->a4==0x10012345000UL);
    a->a0=smc_status;
}
static void return_from_sp_helper(bool panic,uint32_t code,struct thread_svc_regs *r) {
    (void)r; assert(panic && code==TEE_ERROR_STORAGE_NOT_AVAILABLE); panics++;
}
static void service_compose_direct_resp(struct thread_svc_regs *r,uint32_t result) {
    (void)r; assert(!result); responses++;
}
static const unsigned mem_mgr_id=3,ffa_storage_id=4,ffa_debug_message_id=100,ffa_nor_flash_id=101;
static void stmm_handle_mem_mgr_service(struct thread_svc_regs *r) {(void)r;}
static void stmm_handle_storage_service(struct thread_svc_regs *r) {(void)r;}
static void stmm_handle_debug_message_service(struct thread_svc_regs *r) {(void)r;}
static void spm_eret_error(int n,struct thread_svc_regs *r) {(void)n;(void)r;}
'''
        code += function('core/arch/arm/kernel/stmm_sp.c', 'stmm_handle_nor_flash_service')
        code += function('core/arch/arm/kernel/stmm_sp.c', 'spm_handle_direct_req')
        code += r'''
int main(void) {
    const unsigned long statuses[]={0,1,(unsigned long)-1,(unsigned long)-5};
    for (unsigned i=0;i<4;i++) {
        struct thread_svc_regs regs={.x1=101,.x5=0x10012345000UL};
        responses=panics=0; smc_status=statuses[i];
        assert(spm_handle_direct_req(&regs)==(smc_status==0));
        assert(responses==(smc_status==0));
        assert(panics==(smc_status!=0));
    }
    return 0;
}
'''
        self.compile_run(code, flags=('-Wno-parentheses',))

    def compile_run(self, code, sources=(), headers=None, flags=()):
        with tempfile.TemporaryDirectory(prefix='optee-component-') as directory:
            root = Path(directory)
            for name, content in (headers or {}).items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            test = root / 'test.c'
            test.write_text(code)
            exe = root / 'test'
            subprocess.run(['cc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=undefined', '-I', str(root), *flags,
                            str(test), *map(str, sources), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_key_results_and_caller_length(self):
        code = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <drivers/ipc.h>
#define TEE_ERROR_GENERIC 0xffff0000U
#define TEE_ERROR_BAD_PARAMETERS 0xffff0006U
#define TEE_SUCCESS 0
#define __maybe_unused __attribute__((unused))
#define EMSG(...) ((void)0)
#define EMSG_RAW(...) ((void)0)
#define DHEXDUMP(...) ((void)0)
#define TEE_KEY_META_SHM_BASE 0
#define MEM_AREA_RAM_SEC 0
static csec_km_meta_t metadata;
#define phys_to_virt(...) (&metadata)
'''
        code += function('core/drivers/ipc.c', 'cix_get_key_info') + '\n'
        code += function('core/arch/arm/plat-cix/main.c', 'tee_otp_get_ta_enc_key')
        code += r'''
int main(void) {
    uint8_t out[64];
    uint32_t len;
    memset(&metadata, 0x73, sizeof(metadata));
    for (unsigned id = 0; id <= KID_MAX; id++) {
        for (unsigned n = 0; n < sizeof(out); n++) {
            memset(out, 0xa5, sizeof(out)); len = 0;
            unsigned result = cix_get_key_info(id, n, out, &len);
            int valid = (id == KEY_ID_DEVICE_KEY && n <= KM_DEVICE_KEY_SIZE) ||
                        (id == KEY_ID_MODEL_KEY && n <= KM_MODEL_KEY_SIZE) ||
                        (id == KEY_ID_TA_CONFIG_ENABLE && n == TA_ENC_CONFIG_SIZE);
            assert((result == TEE_SUCCESS) == valid);
            assert(len == (valid ? n : 0));
            for (unsigned k = 0; k < sizeof(out); k++)
                assert(out[k] == (valid && k < n ? 0x73 : 0xa5));
        }
    }
    for (unsigned n = 0; n < sizeof(out); n++) {
        memset(out, 0xa5, sizeof(out));
        unsigned result = tee_otp_get_ta_enc_key(0, out, n);
        assert((result == TEE_SUCCESS) == (n <= KM_MODEL_KEY_SIZE));
        for (unsigned k = n; k < sizeof(out); k++) assert(out[k] == 0xa5);
    }
    assert(tee_otp_get_ta_enc_key(0, out, (size_t)UINT32_MAX + 1) == TEE_ERROR_BAD_PARAMETERS);
    return 0;
}
'''
        self.compile_run(code, headers={'tee_api_types.h': TYPES},
                         flags=('-I', str(SOURCE / 'core/include')))

    def test_entropy_failure_and_short_read(self):
        code = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <setjmp.h>
static jmp_buf failed;
static size_t produced;
static int status;
static int mbedtls_hardware_poll(void *p, unsigned char *b, size_t n, size_t *out) {
    (void)p; memset(b, 0x23, n); *out = produced; return status;
}
#define panic(...) longjmp(failed, 1)
#define abort() longjmp(failed, 1)
'''
        code += function('lib/libmbedtls/mbedtls/library/entropy.c', 'rand')
        code += r'''
int main(void) {
    for (status = 0; status < 2; status++) {
        for (produced = 0; produced <= sizeof(int) + 1; produced++) {
            if (!setjmp(failed)) {
                int value = rand();
                assert(!status && produced == sizeof(int));
                assert(value == 0x23232323);
            } else assert(status || produced != sizeof(int));
        }
    }
    return 0;
}
'''
        # Both the TEE core panic and user-TA abort paths must fail closed.
        self.compile_run(code)
        self.compile_run(code, flags=('-D__KERNEL__',))

    def test_hob_bounds_and_cix_serialization(self):
        original = (SOURCE / 'core/arch/arm/include/kernel/stmm_sp.h').read_text()
        begin = original.index('struct stmm_flash_nvram_storage_info {')
        flash = original[begin:original.index('\n};', begin) + 3]
        headers = {
            'tee_api_types.h': TYPES,
            'tee_api_defines.h': ERRORS,
            'types_ext.h': '#pragma once\n#include <stdint.h>\ntypedef uintptr_t vaddr_t;\n',
            'trace.h': '#pragma once\n#define EMSG(...) ((void)0)\n',
            'util.h': UTIL,
            'smccc.h': UTIL,
            'kernel/stmm_sp.h': '#pragma once\n#include <tee_api_types.h>\n#include <tee_api_defines.h>\n#include <types_ext.h>\n' + flash,
        }
        code = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <efi/hob.h>
#include <tee_api_defines.h>
#include <ffa.h>
#include "stmm_hob.h"
static _Alignas(8) unsigned char area[65536];
int main(void) {
    uintptr_t base = (uintptr_t)area;
    struct efi_hob_handoff_info_table *h;
    struct stmm_flash_nvram_storage_info flash = {
        0x0123456789abcdef, 0x1020304050607080, 0xfedcba9876543210,
        0x11223344, 0x55667788, 0x99aabbcc
    };
    struct stmm_ffa_mem mem = {
        .sp_addr=base, .sp_size=sizeof(area), .image_addr=base,
        .image_region_size=4096, .heap_addr=base+4096, .heap_size=4096,
        .sec_buf_addr=base+8192, .sec_buf_size=16384,
        .ns_comm_buf_addr=base+32768, .ns_comm_buf_size=4096
    };
    struct stmm_ffa_init_regs regs = {0};
    memset(area, 0xa5, sizeof(area));
    assert(stmm_ffa_init(&mem, &flash, &regs) == TEE_SUCCESS);
    struct ffa_boot_info_header_1_1 *hdr = (void *)regs.a0;
    assert(hdr->signature == 0xffa && hdr->version == 0x10002);
    assert(hdr->desc_count == 1 && hdr->reserved == 0);
    struct ffa_boot_info_1_1 *desc = (void *)((char *)hdr + hdr->desc_offset);
    assert(desc->type == 1 && desc->flags == 1 && desc->reserved == 0);
    h = (void *)(uintptr_t)desc->contents;
    assert(h->efi_free_memory_top == base+8192+16384);
    unsigned char *cursor = (void *)h;
    unsigned count = 0, cix = 0;
    const unsigned char guid[] = {0xfc,0x67,0x81,0x4c,0x9d,0x5a,0x8e,0x46,
                                  0xa9,0x42,0x86,0x1f,0xb5,0x0a,0xb5,0x39};
    const unsigned char expected[] = {
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01,
        0x80,0x70,0x60,0x50,0x40,0x30,0x20,0x10,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xcc,0xbb,0xaa,0x99};
    while (1) {
        struct efi_hob_generic_header *node = (void *)cursor;
        assert(!node->reserved && node->hob_length >= 8 && !(node->hob_length % 8));
        assert(cursor + node->hob_length <= (unsigned char *)h + desc->size);
        if (node->hob_type == EFI_HOB_TYPE_END_OF_HOB_LIST) break;
        if (node->hob_type == EFI_HOB_TYPE_GUID_EXTENSION && !memcmp(cursor+8,guid,16)) {
            assert(node->hob_length == 64);
            assert(!memcmp(cursor+24, expected, sizeof(expected)));
            assert(!memcmp(cursor+60, "\0\0\0\0", 4)); cix++;
        }
        cursor += node->hob_length; count++;
    }
    assert(count == 5 && cix == 1);
    assert(area[8192+16384] == 0xa5);
    h = efi_create_hob_list(base, sizeof(area), base+4096, 64);
    assert(h && h->efi_free_memory_bottom == h->efi_free_memory_top);
    assert(efi_create_fv_hob(h, base, 4096) == TEE_ERROR_OUT_OF_MEMORY);
    assert(!efi_create_hob_list(base, sizeof(area), base+4096, 63));
    assert(!efi_create_hob_list(base, sizeof(area), base+4097, 64));
    assert(!efi_create_hob_list(base, sizeof(area), base+65528, 64));
    assert(!efi_create_hob_list(UINTPTR_MAX-7, 16, UINTPTR_MAX-7, 64));
    assert(!efi_create_hob_list(base, sizeof(area), base+4096, SIZE_MAX));
    h = efi_create_hob_list(base, sizeof(area), base+4096, 128);
    TEE_UUID uuid = {0}; void *data = NULL;
    assert(efi_create_guid_hob(h, &uuid, UINT16_MAX, &data) != TEE_SUCCESS);
    assert(efi_create_guid_hob(h, &uuid, UINT16_MAX-24, &data) != TEE_SUCCESS);
    h->efi_free_memory_top = h->efi_free_memory_bottom-1;
    assert(efi_create_fv_hob(h, base, 4096) == TEE_ERROR_OUT_OF_MEMORY);
    for (unsigned n = 0; n < 128; n++) {
        mem.sec_buf_size = n;
        assert(stmm_ffa_init(&mem, &flash, &regs) != TEE_SUCCESS);
    }
    return 0;
}
'''
        self.compile_run(code, headers=headers,
                         sources=(SOURCE / 'core/lib/libefi/hob.c',
                                  SOURCE / 'core/arch/arm/kernel/stmm_hob.c'),
                         flags=('-I', str(SOURCE / 'core/lib/libefi/include'),
                                '-I', str(SOURCE / 'core/arch/arm/kernel'),
                                '-I', str(SOURCE / 'core/arch/arm/include')))

    def test_known_legacy_images_reject_modern_boot_abi(self):
        spec = importlib.util.spec_from_file_location('stmm_hex', SOURCE / 'scripts/gen_stmm_hex.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        from unittest.mock import patch
        for digest in module.CIX_LEGACY_IMAGES:
            with patch.object(module.hashlib, 'sha256') as sha:
                sha.return_value.hexdigest.return_value = digest
                module.validate_boot_abi(b'payload', 10)
                with self.assertRaisesRegex(ValueError, 'requires CFG_STMM_FFA_VERSION=10'):
                    module.validate_boot_abi(b'payload', 12)

    def test_modern_memory_permission_calls(self):
        code = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <ffa.h>
#include <tee_api_defines.h>
typedef uintptr_t vaddr_t;
typedef uint32_t TEE_Result;
struct user_mode_ctx { int unused; };
struct thread_svc_regs { uint64_t a[8]; };
#define SVC_REGS_A0(r) ((r)->a[0])
#define SVC_REGS_A1(r) ((r)->a[1])
#define SVC_REGS_A2(r) ((r)->a[2])
#define SVC_REGS_A3(r) ((r)->a[3])
#define SVC_REGS_A4(r) ((r)->a[4])
#define SVC_REGS_A5(r) ((r)->a[5])
#define SVC_REGS_A6(r) ((r)->a[6])
#define SVC_REGS_A7(r) ((r)->a[7])
#define SMALL_PAGE_SIZE 4096U
#define SMALL_PAGE_MASK 4095U
'''
        attrs = (SOURCE / 'core/include/mm/tee_mmu_types.h').read_text()
        for line in attrs.splitlines():
            if line.startswith('#define TEE_MATTR_') and line.split()[1] in {
                    'TEE_MATTR_UR', 'TEE_MATTR_UW', 'TEE_MATTR_UX', 'TEE_MATTR_URW'}:
                code += line + '\n'
        code += r'''
static unsigned calls, saved_prot;
static uint16_t queried_prot;
static int fail_vm;
static TEE_Result vm_get_prot(struct user_mode_ctx *c, vaddr_t va, size_t size, uint16_t *p) {
    (void)c; assert(va && size == SMALL_PAGE_SIZE); calls++; *p=queried_prot; return fail_vm;
}
static TEE_Result vm_set_prot(struct user_mode_ctx *c, vaddr_t va, size_t size, uint32_t p) {
    (void)c; assert(va && size); calls++; saved_prot=p; return fail_vm;
}
'''
        for name in ('spm_eret_error', 'spm_handle_get_mem_attr', 'spm_handle_set_mem_attr'):
            code += function('core/arch/arm/kernel/stmm_sp.c', name) + '\n'
        code += r'''
int main(void) {
    struct user_mode_ctx ctx = {0};
    struct thread_svc_regs r;
    for (unsigned perm=0; perm<16; perm++) {
        r=(struct thread_svc_regs){ .a={0,4096,2,perm} }; calls=0;
        spm_handle_set_mem_attr(&ctx,&r);
        int valid=perm<8 && (perm&3)!=2;
        assert(calls == (unsigned)valid);
        assert(r.a[0] == (valid ? FFA_SUCCESS_32 : FFA_ERROR));
        if (!valid) { assert((int32_t)r.a[2] == FFA_INVALID_PARAMETERS); continue; }
        queried_prot=saved_prot;
        r=(struct thread_svc_regs){ .a={0,4096} };
        spm_handle_get_mem_attr(&ctx,&r);
        assert(r.a[0] == FFA_SUCCESS_32 && r.a[2] == perm);
        assert(!r.a[1] && !r.a[3] && !r.a[7]);
    }
    const uint64_t bad[][3] = {
        {0,1,1},{4097,1,1},{4096,0,1},{4096,1ULL<<32,1},
        {4096,(1ULL<<32)+1,1},{4096,1,(1ULL<<32)+1}
    };
    for (unsigned i=0; i<sizeof(bad)/sizeof(*bad); i++) {
        r=(struct thread_svc_regs){.a={0,bad[i][0],bad[i][1],bad[i][2]}}; calls=0;
        spm_handle_set_mem_attr(&ctx,&r);
        assert(!calls && r.a[0] == FFA_ERROR && (int32_t)r.a[2] == FFA_INVALID_PARAMETERS);
    }
    fail_vm=1;
    r=(struct thread_svc_regs){.a={0,4096,1,1}};
    spm_handle_set_mem_attr(&ctx,&r);
    assert(r.a[0] == FFA_ERROR && (int32_t)r.a[2] == FFA_DENIED);
    r=(struct thread_svc_regs){.a={0,4096}};
    spm_handle_get_mem_attr(&ctx,&r);
    assert(r.a[0] == FFA_ERROR && (int32_t)r.a[2] == FFA_INVALID_PARAMETERS);
    return 0;
}
'''
        self.compile_run(code, headers={'smccc.h': UTIL, 'tee_api_defines.h': ERRORS},
                         flags=('-I', str(SOURCE / 'core/arch/arm/include')))


TYPES = '''#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint32_t TEE_Result;
typedef struct { uint32_t a; uint16_t b,c; uint8_t d[8]; } TEE_UUID;
'''
ERRORS = '''#pragma once
#define TEE_SUCCESS 0
#define TEE_ERROR_OUT_OF_MEMORY 0xffff000cU
#define TEE_ERROR_BAD_PARAMETERS 0xffff0006U
#define TEE_ERROR_NO_DATA 0xffff000bU
#define TEE_ERROR_GENERIC 0xffff0000U
'''
UTIL = '''#pragma once
#include <stdint.h>
#define U(x) x##U
#define BIT(n) (1U << (n))
#define ROUNDUP(x,a) (((x)+(a)-1)&~((a)-1))
#define ADD_OVERFLOW(a,b,r) __builtin_add_overflow(a,b,r)
#define MUL_OVERFLOW(a,b,r) __builtin_mul_overflow(a,b,r)
#define GENMASK_32(h,l) ((UINT32_MAX >> (31-(h))) & (UINT32_MAX << (l)))
'''


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    SOURCE = args.source.resolve()
    # Discovery skips this suite; explicit invocation selects its source tree.
    ComponentTests.__unittest_skip__ = False
    unittest.main(argv=[__file__, *remaining])
