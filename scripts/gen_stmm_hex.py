#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019, Linaro Limited
#

import argparse
import hashlib
import sys
import zlib


# Exact CIX legacy payloads recovered from Radxa 1.2.4 and 1.3.1.
# The firmware packaging guard independently verifies the selected checkpoint.
CIX_LEGACY_IMAGES = {
    '813a4b29954c11a0ad8a151b81f67852eccd03e7273e26ad7ecf25abad2f60dd',
    'bcf541629e956efe8e8f06738314b5b43fcca32340ab3afc62d8723f39e380a1',
}


def validate_boot_abi(payload, ffa_version):
    if ffa_version == 12 and hashlib.sha256(payload).hexdigest() in CIX_LEGACY_IMAGES:
        raise ValueError('CIX vendor StandaloneMM requires CFG_STMM_FFA_VERSION=10; '
                         'FF-A 1.2 requires a matching modern StandaloneMM image')


def get_args():
    parser = argparse.ArgumentParser()

    parser.add_argument('--input',
                        required=True, type=argparse.FileType('rb'),
                        help='The input StMM binary (BL32_AP_MM.fd)')

    parser.add_argument('--output',
                        required=True, type=argparse.FileType('w'),
                        help='The output stmm_hex.c')

    parser.add_argument('--ffa-version', type=int, choices=(10, 12), default=10)

    return parser.parse_args()


def main():
    args = get_args()
    inf = args.input
    outf = args.output

    bytes = inf.read()
    validate_boot_abi(bytes, args.ffa_version)
    uncompressed_size = len(bytes)
    bytes = zlib.compress(bytes)
    size = len(bytes)

    outf.write('/* Automatically generated, do no edit */\n')
    outf.write('const unsigned char stmm_image[] = {\n')
    i = 0
    while i < size:
        if i % 8 == 0:
            outf.write('\t')
        outf.write('0x{:02x},'.format(bytes[i]))
        i = i + 1
        if i % 8 == 0 or i == size:
            outf.write('\n')
        else:
            outf.write(' ')
    outf.write('};\n')

    outf.write('const unsigned int stmm_image_size = sizeof(stmm_image);\n')
    outf.write('const unsigned int stmm_image_uncompressed_size = '
               '{:d};\n'.format(uncompressed_size))

    inf.close()
    outf.close()


if __name__ == "__main__":
    main()
