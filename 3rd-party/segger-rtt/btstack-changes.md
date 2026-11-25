# BTstack changes to SEGGER RTT

The files in this directory are based on [SEGGER's RTT repository](https://github.com/SEGGERMicro/RTT) at commit `9106eaa39babe53e00994d7828c1afb29390cdc1`. This is V8.58 plus SEGGER's subsequent `SEGGER_RTT_GetBytesInBuffer` fix.

The five files in `RTT/`, `Config/SEGGER_RTT_Conf.h`, and the four files in `Syscalls/` are flattened into this directory. `License.txt` is the upstream `LICENSE.md`. The older local `README.txt` is retained for historical context.

[`btstack-changes.patch`](btstack-changes.patch) records the local changes against that commit:

- The GCC syscall implementation is enabled only when `ENABLE_SEGGER_RTT` is defined, and `_write` and `_write_r` are weak so applications can override them. A typedef avoids an empty translation unit when disabled.
- The GCC syscall and printf implementations include `SEGGER_RTT.h` through the include path, as in BTstack's prior integration.

To verify, copy the scoped upstream files into an empty directory and apply the patch:

```sh
cp /path/to/RTT/RTT/* .
cp /path/to/RTT/Config/SEGGER_RTT_Conf.h .
cp /path/to/RTT/Syscalls/* .
git apply --check /path/to/btstack/3rd-party/segger-rtt/btstack-changes.patch
git apply /path/to/btstack/3rd-party/segger-rtt/btstack-changes.patch
```

Compare the ten resulting files against this directory. When updating, review and port the local changes, then update the pinned revision, patch, license, and inventory together.
