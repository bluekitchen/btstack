#!/usr/bin/env python3
"""Exercise the group discovery command-line wrapper."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


WRAPPER = Path(__file__).resolve().parents[2] / "tool" / "btstack_groups.py"


class GroupsWrapperTest(unittest.TestCase):
    def run_wrapper(self, *arguments):
        with tempfile.TemporaryDirectory() as directory:
            return subprocess.run([sys.executable, str(WRAPPER), *arguments],
                                  cwd=directory, text=True, capture_output=True)

    def test_help(self):
        result = self.run_wrapper("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("list", result.stdout)
        self.assertIn("info", result.stdout)

    def test_list_and_info(self):
        result = self.run_wrapper("list", "--filter", "run-loop")
        self.assertEqual(result.returncode, 0)
        self.assertIn("platform-posix-run-loop", result.stderr + result.stdout)
        self.assertNotIn("transport-h4", result.stderr + result.stdout)
        result = self.run_wrapper("info", "gatt-server")
        self.assertEqual(result.returncode, 0)
        self.assertIn("ENABLE_BLE", result.stderr + result.stdout)

    def test_errors(self):
        result = self.run_wrapper("info", "missing-group")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unknown BTstack source group", result.stderr)
        result = self.run_wrapper("info")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("group", result.stderr)


if __name__ == "__main__":
    unittest.main()
