#!/usr/bin/env python3
"""Build a standalone consumer and check missing-feature diagnostics."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
STANDALONE = ROOT / "port" / "posix-h4" / "gatt-counter-standalone"


class ExternalProjectTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="btstack-external-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.env = os.environ.copy()
        # Keep compiler cache files out of a user's home directory in CI.
        self.env["CCACHE_DISABLE"] = "1"

    def cmake(self, *args):
        return subprocess.run(["cmake", *map(str, args)], cwd=self.directory,
                              env=self.env, text=True, capture_output=True)

    @unittest.skipIf(os.name == "nt", "the standalone example requires POSIX")
    def test_standalone_project_build(self):
        build = self.directory / "build"
        result = self.cmake("-S", STANDALONE, "-B", build)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = self.cmake("--build", build, "--parallel", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((build / "btstack-gatt" / "gatt_counter" / "gatt_counter.h").is_file())
        self.assertTrue((build / "gatt_counter").is_file())

    def test_missing_required_feature(self):
        source = self.directory / "missing-feature"
        source.mkdir()
        (source / "btstack_config.h").write_text("#pragma once\n", encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.18)\n'
            'project(missing_feature C)\n'
            'include("${BTSTACK_MODULE}")\n'
            'btstack_add_library(test_btstack CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h" '
            'GROUPS gatt-server)\n', encoding="utf-8")
        result = self.cmake("-S", source, "-B", self.directory / "missing-build",
                            f"-DBTSTACK_MODULE={ROOT / 'tool' / 'cmake' / 'BTstack.cmake'}")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ENABLE_BLE", result.stdout + result.stderr)
        self.assertIn("required by the selected groups", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
