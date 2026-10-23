"""Regression checks for sparse PPC listings and loss of the RPC connection."""
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "ghidra-rpc-local"))
from ghidra_capture import coverage
from ghidra_rpc import client, daemon


class EvidenceTests(unittest.TestCase):
    def test_skipped_instruction_is_not_replaced_by_next_function(self):
        # The RPC iterator can return its requested count by crossing a gap and
        # continuing into the next function. A successful response is insufficient.
        listing = [{"address": f"{a:x}", "length": 4}
                   for a in [0x822F4ED0, 0x822F4ED4, 0x822F4FB0]]
        selected, missing = coverage(listing, 0x822F4ED0, 12)
        self.assertEqual(len(selected), 2)
        self.assertEqual(missing, ["0x822F4ED8"])

    def test_complete_listing(self):
        listing = [{"address": f"{a:x}", "length": 4}
                   for a in range(0x82AF1304, 0x82AF1310, 4)]
        selected, missing = coverage(listing, 0x82AF1304, 12)
        self.assertEqual(selected, listing)
        self.assertEqual(missing, [])

    def test_disconnect_does_not_start_a_gui(self):
        with patch.dict(os.environ, {"GHIDRA_RPC_NO_AUTOSTART": "1"}), \
             patch.object(client, "send_request", side_effect=client.DaemonNotRunning("closed")), \
             patch.object(client.session_mod, "socket_path_for_project", return_value=Path("unused")), \
             patch.object(daemon, "start_background") as start:
            with self.assertRaises(client.DaemonNotRunning):
                client.send_request_with_auto_restart(Path("unused.gpr"), "metadata")
            start.assert_not_called()


if __name__ == "__main__":
    unittest.main()
