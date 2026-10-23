"""Synthetic checks for the descriptor-VA versus ordinal boundary."""
import struct
import unittest

from xex_runtime_inventory import headers, parse_imports


class ImportInventoryTests(unittest.TestCase):
    def fixture(self):
        strings = b'xboxkrnl.exe\0'.ljust(16, b'\0')
        library = bytearray(52)
        struct.pack_into('>I', library, 0, len(library))
        struct.pack_into('>HH', library, 36, 0, 3)
        # Function pair is deliberately separated by a variable descriptor.
        struct.pack_into('>3I', library, 40, 0x82000100, 0x82000108, 0x82000104)
        block = struct.pack('>3I', 12 + len(strings) + len(library), len(strings), 1) + strings + library
        image = bytearray(0x200)
        struct.pack_into('>3I', image, 0x100, 0x0003004e, 0x0103004e, 0x00000123)
        names = {'xboxkrnl.exe': {78: {'name': 'SyntheticFunction', 'export_kind': 'kFunction'},
                                    0x123: {'name': 'SyntheticVariable', 'export_kind': 'kVariable'}}}
        return bytearray(block), image, names

    def parse(self, block, image, names):
        return parse_imports(block, 0, image, 0x82000000,
                             [{'rva': 0, 'virtual_size': len(image)}], names)

    def test_dereferences_guest_vas_and_pairs_by_ordinal(self):
        block, image, names = self.fixture()
        library = self.parse(block, image, names)[0]
        self.assertEqual(library['record_count'], 3)
        function, variable = library['imports']
        self.assertEqual(function['ordinal'], 78)
        self.assertEqual(function['name'], 'SyntheticFunction')
        self.assertEqual(function['value_slots'], ['0x82000100'])
        self.assertEqual(function['thunk_slots'], ['0x82000104'])
        self.assertEqual(variable['value_slots'], ['0x82000108'])
        self.assertEqual(variable['thunk_slots'], [])
        self.assertEqual(library['records'][0]['hint'], 3)

    def test_unmapped_and_misaligned_guest_descriptors_rejected(self):
        for address in [0x82000200, 0x82000101, 0x100]:
            with self.subTest(address=address):
                block, image, names = self.fixture()
                struct.pack_into('>I', block, 28 + 40, address)
                with self.assertRaises(ValueError):
                    self.parse(block, image, names)

    def test_truncation_unknown_type_and_count_mismatch_rejected(self):
        block, image, names = self.fixture()
        with self.assertRaises(ValueError):
            self.parse(block[:-1], image, names)
        struct.pack_into('>I', image, 0x100, 0x0200004e)
        with self.assertRaises(ValueError):
            self.parse(block, image, names)
        struct.pack_into('>I', image, 0x100, 0x0000004e)
        struct.pack_into('>H', block, 28 + 38, 4)
        with self.assertRaises(ValueError):
            self.parse(block, image, names)

    def test_header_inline_entry_and_duplicate_detection(self):
        data = struct.pack('>6I4I', 0x58455832, 1, 40, 0, 0, 2,
                           0x10100, 0x82b91f88, 0x20200, 0x100000)
        options, size = headers(data)
        self.assertEqual(size, 40)
        self.assertEqual(options[0x10100], 0x82b91f88)
        self.assertEqual(options[0x20200], 0x100000)
        damaged = bytearray(data)
        struct.pack_into('>I', damaged, 32, 0x10100)
        with self.assertRaises(ValueError):
            headers(damaged)
        with self.assertRaises(ValueError):
            headers(data[:-1])


if __name__ == '__main__':
    unittest.main()
