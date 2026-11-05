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
    ("platform/posix/btstack_audio_portaudio.c", "platform-posix-audio-portaudio"),
    ("platform/posix/btstack_audio_sdl2.c", "platform-posix-audio-sdl2"),
    ("platform/posix/hal_led_posix.c", "platform-posix-hal-led"),
    ("platform/posix/btstack_link_key_db_fs.c", "platform-posix-link-key-db-fs"),
    ("platform/posix/btstack_main_config.c", "platform-posix-main-config"),
    ("platform/posix/btstack_network_posix.c", "platform-posix-network"),
    ("platform/posix/btstack_run_loop_posix.c", "platform-posix-run-loop"),
    ("platform/posix/btstack_sco_transport_posix_i2s_test_bridge.c", "platform-posix-sco-bridge"),
    ("platform/posix/btstack_signal.c", "platform-posix-signal"),
    ("platform/posix/btstack_stdin_posix.c", "platform-posix-stdin"),
    ("platform/posix/btstack_tlv_posix.c", "platform-posix-tlv"),
    ("platform/posix/btstack_uart_posix.c", "platform-posix-uart"),
    ("platform/posix/hci_dump_posix_*.c", "platform-posix-hci-dump"),
    ("platform/posix/wav_util.c", "platform-posix-wav"),
    ("platform/posix/*.c", "platform-posix"),
    # DA145xx firmware images define the same symbols. Keep each selectable
    # image in its own leaf group instead of compiling all of them together.
    ("chipset/da145xx/btstack_chipset_da145xx.c", "chipset-da145xx"),
    ("chipset/da145xx/hci_531_active_uart_460800.c", "chipset-da145xx-531"),
    ("chipset/da145xx/hci_581_active_uart.c", "chipset-da145xx-581"),
    ("chipset/da145xx/hci_585.c", "chipset-da145xx-585"),
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
    # Keep GATT implementation layers separate from the common LE foundation.
    # The gatt aggregate below preserves the all-in source selection used by
    # existing ports while scoped targets can select only their required role.
    ("src/ble/att_db.c", "att-db"),
    ("src/ble/att_db_util.c", "att-db-util"),
    ("src/ble/att_dispatch.c", "att-dispatch"),
    ("src/ble/att_server.c", "gatt-server"),
    ("src/ble/gatt_client.c", "gatt-client"),
    ("src/ble/gatt_service_client.c", "gatt-service-client"),
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

PLATFORM_POSIX_GROUPS = [
    "platform-posix-audio-portaudio",
    "platform-posix-audio-sdl2",
    "platform-posix-hal-led",
    "platform-posix-link-key-db-fs",
    "platform-posix-main-config",
    "platform-posix-network",
    "platform-posix-run-loop",
    "platform-posix-sco-bridge",
    "platform-posix-signal",
    "platform-posix-stdin",
    "platform-posix-tlv",
    "platform-posix-uart",
    "platform-posix-hci-dump",
    "platform-posix-wav",
]

PLATFORM_POSIX_AUDIO_GROUPS = [
    "platform-posix-audio-portaudio",
    "platform-posix-audio-sdl2",
    "platform-posix-wav",
]

# Include directories are curated metadata, not inferred from #include lines.
# The source catalog can derive the simple in-tree cases below; dependencies
# whose public headers live elsewhere are listed explicitly.
GROUP_INCLUDE_DIRECTORIES = {
    # btstack_crypto.c uses these optional crypto provider headers directly.
    "core": ["3rd-party/micro-ecc", "3rd-party/rijndael"],
    # btstack_link_key_db_fs.c in this group uses tinydir's header-only API.
    "platform-posix": ["3rd-party/tinydir"],
    "platform-posix-hal-led": ["platform/embedded"],
    # The runtime-selectable BCM chipset loader uses tinydir to locate firmware.
    "chipset-bcm": ["3rd-party/tinydir"],
    # pbap_client.c uses the Digest-MD5 helper directly.
    "classic-pbap-client": ["3rd-party/md5"],
    "lwip-core": ["3rd-party/lwip/core/src/include"],
    "lwip-ipv4": ["3rd-party/lwip/core/src/include"],
    "lwip-netif": ["3rd-party/lwip/core/src/include"],
    "lwip-httpd": ["3rd-party/lwip/core/src/include"],
    "lwip-dhcp-server": [
        "3rd-party/lwip/core/src/include",
        "3rd-party/lwip/dhcp-server",
    ],
    "lwip-platform": [
        "src",
        "3rd-party/lwip/core/src/include",
        "platform/lwip",
        "platform/lwip/port",
    ],
    "lwip": [
        "src",
        "3rd-party/lwip/core/src/include",
        "3rd-party/lwip/dhcp-server",
        "platform/lwip",
        "platform/lwip/port",
    ],
    "third-party-bluedroid-sbc-decoder": [
        "3rd-party/bluedroid/decoder/include",
    ],
    "third-party-bluedroid-sbc-encoder": [
        "3rd-party/bluedroid/encoder/include",
    ],
    "third-party-bluedroid-sbc": [
        "3rd-party/bluedroid/decoder/include",
        "3rd-party/bluedroid/encoder/include",
    ],
    "third-party-lc3-google": ["3rd-party/lc3-google/include"],
    "third-party-micro-ecc": ["3rd-party/micro-ecc"],
    "third-party-hxcmod": [
        "3rd-party/hxcmod-player",
        "3rd-party/hxcmod-player/mod",
    ],
    "third-party-md5": ["3rd-party/md5"],
    "third-party-rijndael": ["3rd-party/rijndael"],
    "third-party-yxml": ["3rd-party/yxml"],
    "third-party-qr-code-generator": ["3rd-party/qr-code-generator"],
}

# Headers in this mapping are used directly by example/application sources,
# rather than only by the implementation of the source group. They are emitted
# separately so target consumers do not inherit every implementation include
# directory. wav_util.h is used by the LE Audio demo utilities when a desktop
# audio backend provides the audio capability.
GROUP_CONSUMER_INCLUDE_DIRECTORIES = {
    "platform-posix-wav": ["platform/posix"],
}

# Include paths required by BTstack's public headers. These are intentionally
# independent of selected source groups: an application can include btstack.h
# (or another public BTstack header) without compiling the corresponding
# optional third-party implementation. Keep this list explicit and limited to
# in-tree dependencies exposed by public headers.
PUBLIC_INCLUDE_DIRECTORIES = [
    "src",
    "3rd-party/bluedroid/decoder/include",
    "3rd-party/bluedroid/encoder/include",
    "3rd-party/hxcmod-player",
    "3rd-party/lc3-google/include",
    "3rd-party/yxml",
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

GATT_IMPLEMENTATION_GROUPS = (
    "att-db",
    "att-db-util",
    "att-dispatch",
    "gatt-server",
    "gatt-client",
    "gatt-service-client",
)

# Leaf groups added while incrementally splitting the broad core group. Their
# sources intentionally remain in core for existing ports; collection helpers
# remove duplicates when a resolver selects both groups.
CORE_LEAF_SOURCES = {
    "l2cap": [
        "src/l2cap.c",
        "src/l2cap_signaling.c",
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

    for group, sources in CORE_LEAF_SOURCES.items():
        for relative in sources:
            if not (root / relative).is_file():
                raise RuntimeError(f"Core leaf group '{group}' references missing file: {relative}")
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
    groups["gatt"] = sources_for(GATT_IMPLEMENTATION_GROUPS)

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
    groups["platform-posix-audio"] = sources_for(PLATFORM_POSIX_AUDIO_GROUPS)
    groups["platform-posix"] = sources_for(PLATFORM_POSIX_GROUPS)

    return groups, unclassified


def unique(values: list[str]) -> list[str]:
    return list(dict.fromkeys(values))


def include_directories(groups: dict[str, list[str]]) -> dict[str, list[str]]:
    directories: dict[str, list[str]] = {}
    for group, sources in groups.items():
        group_directories = list(GROUP_INCLUDE_DIRECTORIES.get(group, []))
        for source in sources:
            parts = source.split("/")
            if source.startswith("src/"):
                group_directories.append("src")
            elif source.startswith("platform/posix/"):
                group_directories.extend(["src", "platform/posix"])
            elif source.startswith("platform/windows/"):
                group_directories.extend(["src", "platform/windows"])
            elif source.startswith("platform/libusb/"):
                group_directories.extend(["src", "platform/libusb"])
            elif source.startswith("chipset/"):
                group_directories.extend(["src", "/".join(parts[:2])])
        directories[group] = unique(group_directories)
    return directories


def render(groups: dict[str, list[str]], unclassified: list[str]) -> str:
    group_include_directories = include_directories(groups)
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
        "# Include paths required by public BTstack headers. Apply these to consumers",
        "# independently of the source groups selected for a target.",
        "set(btstack_public_include_directories",
    ]
    lines.extend(f"    {directory}" for directory in PUBLIC_INCLUDE_DIRECTORIES)
    lines.extend([
        ")",
        "",
        "set(btstack_source_groups",
    ])
    lines.extend(f"    {group}" for group in sorted(groups))
    lines.extend([")", ""])

    for group in sorted(groups):
        lines.append(f"set(btstack_sources_{variable_suffix(group)}")
        lines.extend(f"    {source}" for source in groups[group])
        lines.extend([")", ""])

        lines.append(f"set(btstack_include_directories_{variable_suffix(group)}")
        lines.extend(f"    {directory}" for directory in group_include_directories[group])
        lines.extend([")", ""])

        consumer_include_directories = GROUP_CONSUMER_INCLUDE_DIRECTORIES.get(group)
        if consumer_include_directories:
            lines.append(f"set(btstack_consumer_include_directories_{variable_suffix(group)}")
            lines.extend(f"    {directory}" for directory in consumer_include_directories)
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
