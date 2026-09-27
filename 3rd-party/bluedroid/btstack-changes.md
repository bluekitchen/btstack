# BTstack changes to Android SBC

[`btstack-changes.patch`](btstack-changes.patch) records the changes
from the pinned Android SBC snapshot to the buildable `bluedroid/` directory. It
covers both the encoder and decoder; their existing paths are unchanged.

- Upstream revision: `e8c3d75b75493911ebf0f99c83676359657178f7`
- Upstream directory: `embdrv/sbc/`
- Source archive: https://android.googlesource.com/platform/external/bluetooth/bluedroid/+archive/e8c3d75b75493911ebf0f99c83676359657178f7/embdrv/sbc.tar.gz
- Archive SHA-256: `842875ff3c8ceed8eb4b1a4fa1fece41ff3bcba7abe5df96bf2e56c1199cdf6f`
- Patch generation: BTstack checkout HEAD `a2cef938640d498de335dbe2202e16daec28d62a`
- Target: `3rd-party/bluedroid/` working-tree contents

Includes source changes, added license files and BTstack makefile fragments, and
removed upstream files. Patch paths are relative to the SBC directory. These two
tracking files, file mode changes, and macOS `.DS_Store` files are excluded.
Whitespace and formatting changes are retained.

Verified on 2026-11-18 by applying the patch to the pinned Git revision and
comparing every codec file path and byte of content with the local copy. The
tracking files, `.DS_Store`, and file modes are excluded from this comparison.

To reproduce, extract the original archive into an empty directory, then run there:

```sh
git apply --check --whitespace=nowarn /path/to/btstack/3rd-party/bluedroid/btstack-changes.patch
git apply --whitespace=nowarn /path/to/btstack/3rd-party/bluedroid/btstack-changes.patch
```

Replace `/path/to/btstack` with the project location. The whitespace option preserves
existing source whitespace without warnings.

To verify the result, compare the patched directory with `3rd-party/bluedroid/`,
excluding the tracking files and `.DS_Store`, for example:

```sh
diff -r -q -x .DS_Store -x btstack-changes.md -x btstack-changes.patch . /path/to/btstack/3rd-party/bluedroid
```

When changing the codec, edit the buildable files, update this summary, and
regenerate the patch against the same upstream base. Check that it applies to a
fresh extraction and that the resulting paths and contents match `bluedroid/`,
excluding the tracking files and `.DS_Store`.
When updating the upstream revision, first review and port the local changes,
then update the revision, archive checksum, patch, and inventory together.

This comparison documents differences against the recorded upstream base; it does
not prove that every difference was authored by BTstack or that the base was the
exact original import. Review when porting changes to newer Android SBC sources;
the patch is not expected to apply unchanged to the latest upstream version.

## Change summary

The encoder and decoder add mSBC support for wideband speech. The encoder also
has BTstack-specific state and interface changes; both codecs include warning
and portability fixes. BTstack makefile fragments and license files were added,
and the upstream Android build files and other unused files were removed. The
patch is the authoritative file-level record of these and other differences.

```text
 Android.mk                             |    7 -
 decoder/Android.mk                     |   28 -
 decoder/LICENSE                        |  202 +++++++
 decoder/Makefile.inc                   |   17 +
 decoder/include/oi_assert.h            |    6 +-
 decoder/include/oi_bitstream.h         |    2 +-
 decoder/include/oi_codec_sbc.h         |   17 +-
 decoder/include/oi_codec_sbc_private.h |   10 +-
 decoder/include/oi_cpu_dep.h           |   53 +-
 decoder/srce/alloc.c                   |   10 +-
 decoder/srce/bitalloc-sbc.c            |    6 +-
 decoder/srce/bitalloc.c                |    8 +-
 decoder/srce/decoder-private.c         |   57 +-
 decoder/srce/decoder-sbc.c             |   56 +-
 decoder/srce/dequant.c                 |    5 +-
 decoder/srce/framing-sbc.c             |    1 +
 decoder/srce/framing.c                 |   37 +-
 decoder/srce/oi_codec_version.c        |    4 +-
 decoder/srce/synthesis-8-generated.c   |   17 +-
 decoder/srce/synthesis-dct8.c          |    8 +-
 decoder/srce/synthesis-sbc.c           |   25 +-
 encoder/LICENSE                        |  202 +++++++
 encoder/Makefile.inc                   |   11 +
 encoder/include/sbc_enc_func_declare.h |    2 +-
 encoder/include/sbc_encoder.h          |   22 +-
 encoder/include/sbc_if.h               |   47 --
 encoder/include/sbc_types.h            |   20 +-
 encoder/srce/sbc_analysis.c            | 1043 ++++++++++++++++----------------
 encoder/srce/sbc_dct.c                 |    4 -
 encoder/srce/sbc_enc_bit_alloc_mono.c  |   36 +-
 encoder/srce/sbc_enc_bit_alloc_ste.c   |   14 +-
 encoder/srce/sbc_encoder.c             |  208 ++-----
 encoder/srce/sbc_packing.c             |   23 +-
 33 files changed, 1344 insertions(+), 864 deletions(-)
```
