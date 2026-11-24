"""Checks for the Markdown inventory to CycloneDX conversion."""

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tool" / "generate_sbom.py"
spec = importlib.util.spec_from_file_location("generate_sbom", SCRIPT)
sbom = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sbom)


class GenerateSbomTest(unittest.TestCase):
    def test_actual_inventory_and_special_licenses(self):
        result = sbom.generate_sbom()
        rows = sbom.parse_inventory(sbom.INVENTORY.read_text(encoding="utf-8"))
        self.assertEqual(len(result["components"]), len(rows))
        by_name = {component["name"]: component for component in result["components"]}
        self.assertEqual(by_name["stb_vorbis"]["licenses"], [{"expression": "MIT OR Unlicense"}])
        self.assertEqual(by_name["STM32 Audio PDM"]["licenses"], [{"license": {"name": "LicenseRef-ST-SLA0047"}}])
        self.assertEqual(by_name["tinydir"]["purl"], "pkg:github/cxong/tinydir@1.2.6")
        self.assertEqual(by_name["lwIP DHCP server"]["purl"], "pkg:github/fetisov/lrndis#lrndis/dhcp-server")
        self.assertNotIn("version", by_name["lwIP DHCP server"])
        self.assertEqual(by_name["stb_vorbis"]["cpe"], "cpe:2.3:a:nothings:stb_vorbis.c:1.25:*:*:*:*:*:*:*")
        self.assertNotIn("cpe", by_name["lwIP"])
        self.assertNotIn("version", by_name["Android SBC Encoder"])
        self.assertEqual(result["metadata"]["component"]["name"], "BTstack")
        self.assertTrue(result["metadata"]["component"]["cpe"].startswith("cpe:2.3:a:bluekitchen-gmbh:btstack:"))
        self.assertEqual(len({c["bom-ref"] for c in result["components"]}), len(rows))

    def test_filter_sources_and_include_directories(self):
        manifest = {"schema_version": 1, "port": "test", "application": "demo", "configuration": [],
                    "sources": [str(ROOT / "3rd-party/md5/md5.c")],
                    "include_directories": [str(ROOT / "3rd-party/tinydir")]}
        result = sbom.filter_for_manifest(sbom.generate_sbom(), manifest)
        self.assertEqual({c["name"] for c in result["components"]}, {"md5", "tinydir"})
        manifest["include_directories"] = [str(ROOT / "3rd-party/bluedroid/encoder/include")]
        result = sbom.filter_for_manifest(sbom.generate_sbom(), manifest)
        self.assertEqual({c["name"] for c in result["components"]}, {"md5", "Android SBC Encoder"})
        manifest["include_directories"] = [str(ROOT / "3rd-party/stb")]
        result = sbom.filter_for_manifest(sbom.generate_sbom(), manifest)
        self.assertIn("stb_vorbis", {c["name"] for c in result["components"]})
        manifest["sources"] = ["relative.c"]
        with self.assertRaisesRegex(ValueError, "absolute"):
            sbom.filter_for_manifest(sbom.generate_sbom(), manifest)

    def test_rejects_stale_identifiers(self):
        rows = sbom.parse_inventory(sbom.INVENTORY.read_text(encoding="utf-8"))
        row = next(row for row in rows if row["Path"] == "`tinydir`")
        identifiers = json.loads(sbom.IDENTIFIERS.read_text(encoding="utf-8"))["tinydir"]
        row["Revision"] = "different"
        with self.assertRaisesRegex(ValueError, "does not match"):
            sbom.component(row, identifiers)

    def test_rejects_incomplete_or_malformed_table(self):
        headers = "| " + " | ".join(sbom.HEADERS) + " |\n"
        separator = "| " + " | ".join(["---"] * len(sbom.HEADERS)) + " |\n"
        with self.assertRaises(ValueError):
            sbom.parse_inventory(headers + separator + "| too | few |\n")
        with self.assertRaises(ValueError):
            sbom.parse_inventory("no inventory table")
        self.assertEqual(sbom.table_cells(r"| `a|b` | c\|d |"), ["`a|b`", r"c\|d"])

    def test_stdout_and_output_file_are_equivalent(self):
        result = subprocess.run([sys.executable, str(SCRIPT)], cwd="/", capture_output=True, text=True, check=True)
        document = json.loads(result.stdout)
        missing = [c["name"] for c in [document["metadata"]["component"], *document["components"]]
                   if not c.get("purl") and not c.get("cpe")]
        if missing:
            self.assertIn(f"Components without a PURL or CPE ({len(missing)}):", result.stderr)
            for name in missing:
                self.assertIn(f"  - {name}\n", result.stderr)
        else:
            self.assertEqual(result.stderr, "")
        self.assertNotIn("  - tinydir\n", result.stderr)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "sbom.json"
            saved = subprocess.run([sys.executable, str(SCRIPT), "--output", str(output)], cwd="/",
                                   capture_output=True, text=True, check=True)
            self.assertEqual(saved.stdout, "")
            self.assertEqual(saved.stderr, result.stderr)
            self.assertEqual(document, json.loads(output.read_text(encoding="utf-8")))


if __name__ == "__main__":
    unittest.main()
