"""Exercise rejection of corrupt and ambiguous qualification-control inputs."""
import hashlib
import struct
import unittest
import uuid

from stmm_vendor_control import MODULES, extract


def checksum(data):
    return hashlib.sha256(data).hexdigest()


def volume(duplicate=False):
    image = bytearray(b'\xff' * (2 * 1024 * 1024))
    header = bytearray(72)
    struct.pack_into('<Q', header, 32, len(image))
    header[40:44] = b'_FVH'
    struct.pack_into('<H', header, 48, len(header))
    struct.pack_into('<H', header, 50, -sum(struct.unpack('<36H', header)) & 65535)
    image[:72] = header
    position = 72
    modules = list(MODULES.items())
    if duplicate:
        modules.append(modules[0])
    for guid, (name, _) in modules:
        pe = bytearray(128)
        pe[:2] = b'MZ'
        struct.pack_into('<I', pe, 60, 64)
        pe[64:68] = b'PE\0\0'
        struct.pack_into('<H', pe, 68, 0xaa64)
        body = bytearray()
        for kind, payload in ((0x10, pe), (0x1c, b'\x06\x08'),
                              (0x15, (name + '\0').encode('utf-16le'))):
            body.extend((4 + len(payload)).to_bytes(3, 'little') + bytes([kind]) + payload)
            body.extend(b'\0' * (-len(body) % 4))
        ffs = bytearray(24)
        ffs[:16] = uuid.UUID(guid).bytes_le
        ffs[17] = 0xaa
        ffs[18] = 0x0e
        ffs[20:23] = (24 + len(body)).to_bytes(3, 'little')
        ffs[23] = 7
        ffs[16] = -(sum(ffs) - ffs[17] - ffs[23]) & 255
        data = ffs + body
        image[position:position + len(data)] = data
        position = (position + len(data) + 7) & ~7
    return bytes(image)


class VendorControlTests(unittest.TestCase):
    def test_exact_pair_and_source_pin(self):
        data = volume()
        self.assertEqual(len(extract(data, checksum(data))), 4)
        with self.assertRaisesRegex(ValueError, 'source pin'):
            extract(data, '0' * 64)

    def test_truncation_and_duplicate_storage(self):
        for data in (volume()[:100], volume(duplicate=True)):
            with self.assertRaises(ValueError):
                extract(data, checksum(data))

    def test_corruption_is_rejected_even_with_matching_input_hash(self):
        for offset in (50, 72 + 16, 72 + 20, 72 + 24, 72 + 24 + 4 + 68, 0x1fffff):
            data = bytearray(volume())
            data[offset] ^= 0x80
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                extract(bytes(data), checksum(data))


if __name__ == '__main__':
    unittest.main()
