import importlib.util
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('acv_switch_audit', ROOT / 'tools/acv_switch_audit.py')
audit_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit_module)


class SwitchAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = (ROOT / 'tmp/scan/text.bin').read_bytes()
        cls.pdata = (ROOT / 'tmp/scan/pdata.bin').read_bytes()
        cls.table = (ROOT / 'logs/xenonrecomp-followup-20261020/switch-table-82152100.bin').read_bytes()

    def test_signed_addi_borrow(self):
        self.assertEqual(audit_module.lis_addi(0x3d8082f0, 0x398c87b4), 0x82ef87b4)
        self.assertEqual(audit_module.signed16(0x8000), -32768)
        self.assertEqual(audit_module.signed16(0x7fff), 32767)

    def test_all_real_targets(self):
        result = audit_module.audit(self.text, self.pdata, self.table)
        self.assertEqual(len(result['targets']), 29)
        self.assertEqual(result['targets'][-1]['target'], '0x82EF9C50')

    def test_truncated_table_rejected(self):
        with self.assertRaises(ValueError):
            audit_module.audit(self.text, self.pdata, self.table[:-2])

    def test_unsigned_halfword_outside_owner_rejected(self):
        with self.assertRaises(ValueError):
            audit_module.audit(self.text, self.pdata, struct.pack('>H', 0xfffc) + self.table[2:])

    def test_unaligned_target_rejected(self):
        with self.assertRaises(ValueError):
            audit_module.audit(self.text, self.pdata, b'\x00\x01' + self.table[2:])

    def test_changed_dispatch_instruction_rejected(self):
        text = bytearray(self.text)
        text[0x82ef87a0 - 0x822b0000 + 3] ^= 1
        with self.assertRaises(ValueError):
            audit_module.audit(text, self.pdata, self.table)


if __name__ == '__main__':
    unittest.main()
