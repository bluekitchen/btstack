#!/usr/bin/env python3
"""Generate a CycloneDX 1.6 source-distribution SBOM from 3rd-party/README.md.

Usage: python3 tool/generate_sbom.py [-o sbom.json]

The inventory is authoritative for the listed components, but does not claim
complete coverage of third-party code outside it. Paths in the output are
repository-relative; unknown versions and revisions are not invented.
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INVENTORY = ROOT / "3rd-party" / "README.md"
IDENTIFIERS = ROOT / "3rd-party" / "sbom-identifiers.json"
HEADERS = ("Library", "Path", "Version", "Revision", "License", "License file", "Used", "Notes")
LINK = re.compile(r"^\[([^\]]+)\]\(([^)]+)\)$")
SEPARATOR = re.compile(r"^:?-{3,}:?$")


def table_cells(line):
    """Split a Markdown table row, respecting escaped pipes and code spans."""
    text = line.strip()
    if not text.startswith("|") or not text.endswith("|"):
        raise ValueError("expected a table row with leading and trailing pipes")
    cells, buf = [], []
    in_code = False
    escaped = False
    for char in text[1:-1]:
        if escaped:
            buf.append(char)
            escaped = False
        elif char == "\\":
            escaped = True
            buf.append(char)
        elif char == "`":
            in_code = not in_code
            buf.append(char)
        elif char == "|" and not in_code:
            cells.append("".join(buf).strip())
            buf = []
        else:
            buf.append(char)
    if escaped or in_code:
        raise ValueError("unterminated escape or code span in table row")
    cells.append("".join(buf).strip())
    return cells


def parse_inventory(text):
    lines = text.splitlines()
    matches = []
    for index, line in enumerate(lines):
        if line.strip().startswith("|") and tuple(table_cells(line)) == HEADERS:
            matches.append(index)
    if len(matches) != 1:
        raise ValueError("expected exactly one third-party inventory table")
    index = matches[0]
    if index + 1 >= len(lines):
        raise ValueError("missing table separator")
    separator = table_cells(lines[index + 1])
    if len(separator) != len(HEADERS) or not all(SEPARATOR.fullmatch(x) for x in separator):
        raise ValueError("invalid inventory table separator")
    entries = []
    for line_no, line in enumerate(lines[index + 2:], index + 3):
        if not line.strip().startswith("|"):
            break
        cells = table_cells(line)
        if len(cells) != len(HEADERS) or not all(cells):
            raise ValueError(f"line {line_no}: expected eight nonempty cells")
        entries.append(dict(zip(HEADERS, cells)))
    if not entries:
        raise ValueError("inventory table is empty")
    return entries


def markdown_link(value, field):
    match = LINK.fullmatch(value)
    if not match:
        raise ValueError(f"invalid {field} link: {value!r}")
    return match.groups()


def component(entry, identifiers=None):
    name, url = markdown_link(entry["Library"], "library")
    _, license_path = markdown_link(entry["License file"], "license file")
    path = entry["Path"]
    if len(path) < 3 or not (path.startswith("`") and path.endswith("`")):
        raise ValueError(f"{name}: invalid inventory path {path!r}")
    path = path[1:-1]
    repo_path = (INVENTORY.parent / path).resolve()
    license_file = (INVENTORY.parent / license_path).resolve()
    try:
        relative_path = repo_path.relative_to(ROOT).as_posix()
        relative_license = license_file.relative_to(ROOT).as_posix()
    except ValueError as exc:
        raise ValueError(f"{name}: path outside the repository") from exc
    if not repo_path.exists() or not license_file.is_file():
        raise ValueError(f"{name}: component or license file does not exist")
    version = entry["Version"]
    revision = entry["Revision"]
    license_value = entry["License"]
    if license_value.startswith("LicenseRef-"):
        licenses = [{"license": {"name": license_value}}]
    elif re.search(r"\s(?:AND|OR|WITH)\s|[()]", license_value):
        licenses = [{"expression": license_value}]
    else:
        licenses = [{"license": {"id": license_value}}]
    props = [
        {"name": "btstack:source:path", "value": relative_path},
        {"name": "btstack:inventory:version", "value": version},
        {"name": "btstack:inventory:revision", "value": revision},
        {"name": "btstack:license:file", "value": relative_license},
        {"name": "btstack:inventory:used", "value": entry["Used"]},
        {"name": "btstack:inventory:notes", "value": entry["Notes"]},
    ]
    result = {
        "type": "library",
        "bom-ref": "btstack:third-party:" + relative_path,
        "name": name,
        "licenses": licenses,
        "externalReferences": [{"type": "website", "url": url}],
        "properties": props,
    }
    if version.lower() not in ("git snapshot", "unversioned", "unknown", "n/a"):
        result["version"] = version
    if identifiers is not None:
        if identifiers.get("version") != version or identifiers.get("revision") != revision:
            raise ValueError(f"{name}: identifier version or revision does not match the inventory")
        if set(identifiers) - {"version", "revision", "purl", "cpe"}:
            raise ValueError(f"{name}: unrecognized identifier field")
        for field, prefix in (("purl", "pkg:"), ("cpe", "cpe:2.3:")):
            if field in identifiers:
                value = identifiers[field]
                if not isinstance(value, str) or not value.startswith(prefix) or any(c.isspace() for c in value):
                    raise ValueError(f"{name}: invalid {field}")
                result[field] = value
    return result


def generate_sbom():
    entries = parse_inventory(INVENTORY.read_text(encoding="utf-8"))
    identifiers = json.loads(IDENTIFIERS.read_text(encoding="utf-8"))
    if not isinstance(identifiers, dict) or not all(isinstance(value, dict) for value in identifiers.values()):
        raise ValueError("identifier inventory must map paths to objects")
    inventory_paths = {entry["Path"].strip("`") for entry in entries}
    extra = set(identifiers) - inventory_paths
    if extra:
        raise ValueError("identifiers without inventory entries: " + ", ".join(sorted(extra)))
    components = [component(entry, identifiers.get(entry["Path"].strip("`"))) for entry in entries]
    references = [item["bom-ref"] for item in components]
    if len(references) != len(set(references)):
        raise ValueError("duplicate component path in inventory")
    version_text = (ROOT / "src" / "btstack_version.h").read_text(encoding="utf-8")
    parts = []
    for name in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(r"^#define\s+BTSTACK_VERSION_" + name + r"\s+(\d+)\s*$", version_text, re.MULTILINE)
        if not match:
            raise ValueError(f"BTstack version {name} missing")
        parts.append(match.group(1))
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "version": 1,
        "metadata": {
            "component": {
                "type": "library",
                "bom-ref": "btstack:root",
                "name": "BTstack",
                "version": ".".join(parts),
                "cpe": "cpe:2.3:a:bluekitchen-gmbh:btstack:" + ".".join(parts) + ":*:*:*:*:*:*:*",
                "licenses": [{"license": {"name": "BTstack non-commercial license"}}],
                "externalReferences": [{"type": "website", "url": "https://github.com/bluekitchen/btstack"}],
                "properties": [{"name": "btstack:license:file", "value": "LICENSE"}],
            },
            "properties": [{"name": "btstack:sbom:scope", "value": "Source distribution: BTstack and the components listed in 3rd-party/README.md; other bundled third-party code is not comprehensively reviewed."}],
        },
        "components": components,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", type=Path, help="write JSON to this file instead of stdout")
    args = parser.parse_args()
    try:
        bom = generate_sbom()
        content = json.dumps(bom, indent=2, ensure_ascii=False) + "\n"
        if args.output:
            args.output.write_text(content, encoding="utf-8")
        else:
            sys.stdout.write(content)
        components = [bom["metadata"]["component"], *bom["components"]]
        missing = [item["name"] for item in components if not item.get("purl") and not item.get("cpe")]
        if missing:
            print(f"Components without a PURL or CPE ({len(missing)}):", file=sys.stderr)
            for name in missing:
                print(f"  - {name}", file=sys.stderr)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
