# BTstack changes to SEGGER RTT V7.86a

[`btstack-changes.patch`](btstack-changes.patch) records the differences between
the V7.86a RTT implementation pack and the files used by BTstack.

- Source: [J-Link Software Pack](https://www.segger.com/downloads/jlink/), V7.86a,
  `Samples/RTT/SEGGER_RTT_V786a.tgz`
- Archive SHA-256: `712c86f18e360441fbb019dac3906730e18b246ad74fbb85952f185ec63d826b`
- Scope: the four files in `RTT/`, `Config/SEGGER_RTT_Conf.h`, and the four
  files in `Syscalls/`, flattened into this directory.

The patch excludes the archive's examples, README and license, and the local
`README.txt`, `License.txt`, and tracking files. The local README is an older
document. `License.txt` is copied from the V7.86a archive and matches the
one-condition notice in the source headers; it replaced the older
three-condition license file previously bundled with BTstack.

## Changes

- `SEGGER_RTT.h` includes the configuration header from the same directory,
  matching BTstack's flattened layout.
- `SEGGER_RTT_Syscalls_GCC.c` includes `btstack_config.h` and enables the
  syscall implementations only with `ENABLE_SEGGER_RTT`. The write functions
  are weak so an application can override them, and the disabled configuration
  avoids an empty translation unit.
- `SEGGER_RTT_ASM_ARMv7M.S` differs only in line endings (LF instead of CRLF).

The other six files in scope match the archive byte for byte. SEGGER recommends
against modifying the implementation to preserve RTT and J-Link compatibility.

## Verification and maintenance

Extract the archive, copy the scoped files into an empty directory, and apply
the patch there. For example, from that empty directory:

```sh
tar -xzf /path/to/SEGGER_RTT_V786a.tgz
cp SEGGER_RTT_V786a/RTT/* .
cp SEGGER_RTT_V786a/Config/SEGGER_RTT_Conf.h .
cp SEGGER_RTT_V786a/Syscalls/* .
git apply --check --whitespace=nowarn /path/to/btstack/3rd-party/segger-rtt/btstack-changes.patch
git apply --whitespace=nowarn /path/to/btstack/3rd-party/segger-rtt/btstack-changes.patch
```

Compare the nine patched files byte for byte with `3rd-party/segger-rtt/`.
This comparison passed on 2026-11-18. When changing them, update this summary,
regenerate the patch against the same archive, and repeat the comparison.
When updating the pack, review and port local changes, then update the version,
archive checksum, patch, and inventory together. Preserve line endings when
generating the patch.
The local `.gitattributes` keeps Git from normalizing the patch's line endings.
