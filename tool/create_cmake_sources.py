#!/usr/bin/env python3
"""Generate the CMake source catalog for BTstack CMake consumers.

The catalog covers src/, the reviewed shared platform implementations, and
the explicitly selected lwIP source subset. Third-party source trees are not
scanned broadly: each supported component is an explicit, reviewed list.
"""

from __future__ import annotations

import argparse
import difflib
import re
import sys
from pathlib import Path


# Ordered: the first matching rule wins. Group names are the public CMake API:
# lower-case, with hyphens where that reads better.  Generated variable names
# use the corresponding underscore spelling.
#
# Each GATT service is a leaf group. gatt-clients, gatt-servers, and
# gatt-services are convenience aggregates; they intentionally overlap leaf
# groups. The resolver decides which of these groups a port should use.
# Classic sources are leaf groups too; classic is their convenience aggregate.
# BNEP and link-key database providers remain separate because their selection
# semantics differ from normal Classic protocol/profile sources.
RULES = [
    # Shared platform sources. le_device_db_fs.c is explicitly excluded below
    # because it is deprecated in favor of src/ble/le_device_db_tlv.c.
    ("platform/libusb/hci_transport_h2_libusb.c", "platform-libusb-h2"),
    ("platform/windows/hci_transport_h2_winusb.c", "platform-winusb-h2"),
    ("platform/windows/*.c", "platform-windows"),
    ("platform/posix/*.c", "platform-posix"),
    ("src/hci_transport_h4.c", "transport-h4"),
    ("src/hci_transport_h5.c", "transport-h5"),
    ("src/hci_transport_em9304_spi.c", "transport-em9304-spi"),
    # The SLIP wrapper is an implementation detail of H5, not a transport.
    ("src/btstack_uart_slip_wrapper.c", "transport-h5"),
    ("src/btstack_tlv_none.c", "tlv-none"),
    ("src/ble/le_device_db_memory.c", "le-device-db-memory"),
    ("src/ble/le_device_db_tlv.c", "le-device-db-tlv"),
    ("src/classic/btstack_link_key_db_memory.c", "link-key-db-memory"),
    ("src/classic/btstack_link_key_db_static.c", "link-key-db-static"),
    ("src/classic/btstack_link_key_db_tlv.c", "link-key-db-tlv"),
    ("src/classic/bnep.c", "bnep"),
    ("src/btstack_audio_generator.c", "audio-demo"),
    ("src/btstack_audio_player.c", "audio-demo"),
    ("src/btstack_audio_song.c", "audio-demo"),
    # LC3 is also used by HFP; the future resolver supplies it to both users
    ("src/btstack_lc3*.c", "lc3"),
    ("src/ble/*.c", "ble"),
    ("src/le-audio/*.c", "le-audio"),
    ("src/mesh/*.c", "mesh"),
    ("src/mesh/gatt-service/*.c", "mesh"),
    ("src/*.c", "core"),
]

# Only these roots are scanned. Other platform and third-party trees can have
# their own SDK-specific ownership and are deliberately outside this catalog.
SCANNED_SOURCE_DIRECTORIES = (
    "src",
    "platform/posix",
    "platform/windows",
    "platform/libusb",
    "chipset",
)

EXCLUDED_SOURCES = {
    "platform/posix/le_device_db_fs.c",
}

# Explicit third-party selections. Avoid broad scans here: vendored trees also
# contain tests, tools, examples, and alternative implementations.
EXPLICIT_GROUP_SOURCES = {
    "lwip-core": [
        "3rd-party/lwip/core/src/core/def.c",
        "3rd-party/lwip/core/src/core/inet_chksum.c",
        "3rd-party/lwip/core/src/core/init.c",
        "3rd-party/lwip/core/src/core/ip.c",
        "3rd-party/lwip/core/src/core/mem.c",
        "3rd-party/lwip/core/src/core/memp.c",
        "3rd-party/lwip/core/src/core/netif.c",
        "3rd-party/lwip/core/src/core/pbuf.c",
        "3rd-party/lwip/core/src/core/tcp.c",
        "3rd-party/lwip/core/src/core/tcp_in.c",
        "3rd-party/lwip/core/src/core/tcp_out.c",
        "3rd-party/lwip/core/src/core/timeouts.c",
        "3rd-party/lwip/core/src/core/udp.c",
    ],
    "lwip-ipv4": [
        "3rd-party/lwip/core/src/core/ipv4/acd.c",
        "3rd-party/lwip/core/src/core/ipv4/dhcp.c",
        "3rd-party/lwip/core/src/core/ipv4/etharp.c",
        "3rd-party/lwip/core/src/core/ipv4/icmp.c",
        "3rd-party/lwip/core/src/core/ipv4/ip4.c",
        "3rd-party/lwip/core/src/core/ipv4/ip4_addr.c",
        "3rd-party/lwip/core/src/core/ipv4/ip4_frag.c",
    ],
    "lwip-netif": [
        "3rd-party/lwip/core/src/netif/ethernet.c",
    ],
    "lwip-httpd": [
        "3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c",
        "3rd-party/lwip/core/src/apps/http/fs.c",
        "3rd-party/lwip/core/src/apps/http/httpd.c",
    ],
    "lwip-dhcp-server": [
        "3rd-party/lwip/dhcp-server/dhserver.c",
    ],
    "lwip-platform": [
        "platform/lwip/port/sys_arch.c",
        "platform/lwip/bnep_lwip.c",
    ],
    "third-party-bluedroid-sbc-decoder": [
        "3rd-party/bluedroid/decoder/srce/alloc.c",
        "3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c",
        "3rd-party/bluedroid/decoder/srce/bitalloc.c",
        "3rd-party/bluedroid/decoder/srce/bitstream-decode.c",
        "3rd-party/bluedroid/decoder/srce/decoder-oina.c",
        "3rd-party/bluedroid/decoder/srce/decoder-private.c",
        "3rd-party/bluedroid/decoder/srce/decoder-sbc.c",
        "3rd-party/bluedroid/decoder/srce/dequant.c",
        "3rd-party/bluedroid/decoder/srce/framing-sbc.c",
        "3rd-party/bluedroid/decoder/srce/framing.c",
        "3rd-party/bluedroid/decoder/srce/oi_codec_version.c",
        "3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c",
        "3rd-party/bluedroid/decoder/srce/synthesis-dct8.c",
        "3rd-party/bluedroid/decoder/srce/synthesis-sbc.c",
    ],
    "third-party-bluedroid-sbc-encoder": [
        "3rd-party/bluedroid/encoder/srce/sbc_analysis.c",
        "3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c",
        "3rd-party/bluedroid/encoder/srce/sbc_dct.c",
        "3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c",
        "3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c",
        "3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c",
        "3rd-party/bluedroid/encoder/srce/sbc_encoder.c",
        "3rd-party/bluedroid/encoder/srce/sbc_packing.c",
    ],
    "third-party-lc3-google": [
        "3rd-party/lc3-google/src/attdet.c",
        "3rd-party/lc3-google/src/bits.c",
        "3rd-party/lc3-google/src/bwdet.c",
        "3rd-party/lc3-google/src/energy.c",
        "3rd-party/lc3-google/src/lc3.c",
        "3rd-party/lc3-google/src/ltpf.c",
        "3rd-party/lc3-google/src/mdct.c",
        "3rd-party/lc3-google/src/plc.c",
        "3rd-party/lc3-google/src/sns.c",
        "3rd-party/lc3-google/src/spec.c",
        "3rd-party/lc3-google/src/tables.c",
        "3rd-party/lc3-google/src/tns.c",
    ],
    "third-party-micro-ecc": [
        "3rd-party/micro-ecc/uECC.c",
    ],
    "third-party-hxcmod": [
        "3rd-party/hxcmod-player/hxcmod.c",
        "3rd-party/hxcmod-player/mods/mod.c",
    ],
    "third-party-md5": [
        "3rd-party/md5/md5.c",
    ],
    "third-party-rijndael": [
        "3rd-party/rijndael/rijndael.c",
    ],
    "third-party-yxml": [
        "3rd-party/yxml/yxml.c",
    ],
    "third-party-qr-code-generator": [
        "3rd-party/qr-code-generator/qrcodegen.c",
    ],
}

LWIP_GROUPS = [
    "lwip-core",
    "lwip-ipv4",
    "lwip-netif",
    "lwip-httpd",
    "lwip-dhcp-server",
    "lwip-platform",
]

BLUEDROID_SBC_GROUPS = [
    "third-party-bluedroid-sbc-decoder",
    "third-party-bluedroid-sbc-encoder",
]

# Most GATT service roles are clear from their filenames. These two are named
# after the local HID role instead, so record their role explicitly.
GATT_SERVICE_ROLE_OVERRIDES = {
    "gatt-service-hids-device": "server",
    "gatt-service-hids-host": "client",
}

CLASSIC_CONVENIENCE_GROUPS = {
    "classic-btstack-sbc": [
        "classic-btstack-sbc-bluedroid",
        "classic-btstack-sbc-decoder-bluedroid",
        "classic-btstack-sbc-encoder-bluedroid",
        "classic-btstack-sbc-plc",
    ],
}


def matches(path: str, pattern: str) -> bool:
    """Match a path with * limited to one path component."""
    expression = ""
    for part in re.split(r"(\*)", pattern):
        expression += "[^/]*" if part == "*" else re.escape(part)
    return re.fullmatch(expression, path) is not None


def variable_suffix(group: str) -> str:
    return group.replace("-", "_")


def collect_sources(root: Path) -> tuple[dict[str, list[str]], list[str]]:
    groups: dict[str, list[str]] = {}
    unclassified: list[str] = []

    scanned_sources = []
    for directory in SCANNED_SOURCE_DIRECTORIES:
        scanned_sources.extend((root / directory).rglob("*.c"))

    for source in sorted(scanned_sources):
        relative = source.relative_to(root).as_posix()
        if relative in EXCLUDED_SOURCES:
            continue
        group = next((name for pattern, name in RULES if matches(relative, pattern)), None)
        if group is not None:
            pass
        elif relative.startswith("chipset/"):
            chipset = relative.split("/", 2)[1]
            group = "chipset-" + chipset
        elif relative.startswith("src/ble/gatt-service/"):
            group = "gatt-service-" + source.stem.replace("_", "-")
        elif relative.startswith("src/classic/"):
            group = "classic-" + source.stem.replace("_", "-")
        else:
            group = None
        if group is None:
            unclassified.append(relative)
        else:
            groups.setdefault(group, []).append(relative)

    for group, sources in EXPLICIT_GROUP_SOURCES.items():
        for relative in sources:
            if not (root / relative).is_file():
                raise RuntimeError(f"Explicit source group '{group}' references missing file: {relative}")
        groups[group] = sources

    gatt_service_groups = sorted(group for group in groups if group.startswith("gatt-service-"))
    gatt_client_groups = []
    gatt_server_groups = []
    for group in gatt_service_groups:
        role = GATT_SERVICE_ROLE_OVERRIDES.get(group)
        if role is None:
            if group.endswith("-client"):
                role = "client"
            elif group.endswith("-server"):
                role = "server"
            else:
                raise RuntimeError(f"GATT service group '{group}' has no client/server role")
        if role == "client":
            gatt_client_groups.append(group)
        else:
            gatt_server_groups.append(group)

    def sources_for(source_groups: list[str]) -> list[str]:
        return [source for group in source_groups for source in groups[group]]

    groups["gatt-clients"] = sources_for(gatt_client_groups)
    groups["gatt-servers"] = sources_for(gatt_server_groups)
    groups["gatt-services"] = sources_for(gatt_service_groups)

    classic_leaf_groups = sorted(group for group in groups if group.startswith("classic-"))
    groups["classic"] = sources_for(classic_leaf_groups)
    for aggregate, leaf_groups in CLASSIC_CONVENIENCE_GROUPS.items():
        missing_groups = [group for group in leaf_groups if group not in groups]
        if missing_groups:
            raise RuntimeError(
                f"Classic convenience group '{aggregate}' references missing groups: "
                + ", ".join(missing_groups)
            )
        groups[aggregate] = sources_for(leaf_groups)

    groups["lwip"] = sources_for(LWIP_GROUPS)
    groups["third-party-bluedroid-sbc"] = sources_for(BLUEDROID_SBC_GROUPS)

    return groups, unclassified


def render(groups: dict[str, list[str]], unclassified: list[str]) -> str:
    lines = [
        "# Source file lists for CMake consumers of BTstack.",
        "#",
        "# GENERATED by tool/create_cmake_sources.py -- do not edit.",
        "#",
        "# This catalog covers src/, reviewed shared platform sources, and selected",
        "# third-party source lists. Paths are relative to BTSTACK_ROOT. Group IDs are",
        "# lower-case and hyphenated; variable names",
        "# use underscores, e.g. group le-audio -> btstack_sources_le_audio.",
        "",
        "set(btstack_source_groups",
    ]
    lines.extend(f"    {group}" for group in sorted(groups))
    lines.extend([")", ""])

    for group in sorted(groups):
        lines.append(f"set(btstack_sources_{variable_suffix(group)}")
        lines.extend(f"    {source}" for source in groups[group])
        lines.extend([")", ""])

    if unclassified:
        lines.extend([
            "# No grouping rule in tool/create_cmake_sources.py matched these files.",
            "set(btstack_unclassified_sources",
        ])
        lines.extend(f"    {source}" for source in unclassified)
        lines.extend([")", ""])

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if the checked-in catalog is missing or stale; do not write it",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    output = root / "tool" / "cmake" / "BTstackSources.cmake"
    groups, unclassified = collect_sources(root)
    content = render(groups, unclassified)

    if args.check:
        existing = output.read_text() if output.exists() else ""
        if existing != content:
            print(f"{output.relative_to(root)} is stale; run {Path(__file__).relative_to(root)}", file=sys.stderr)
            print("".join(difflib.unified_diff(
                existing.splitlines(keepends=True),
                content.splitlines(keepends=True),
                fromfile=str(output.relative_to(root)),
                tofile="generated",
            )), file=sys.stderr)
            return 1
    else:
        output.write_text(content)
        print(f"wrote {output.relative_to(root)}: {len(groups)} groups, {sum(map(len, groups.values()))} files")

    if unclassified:
        print("unclassified source files:", file=sys.stderr)
        print("\n".join(f"  {source}" for source in unclassified), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
