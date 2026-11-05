# Build-time firmware artifacts

## Status

Accepted

## Decision

Firmware downloads are build dependencies, never CMake configuration side
effects. `BTstackFirmware.cmake` requires an immutable source URL and
SHA-256 digest, retains verified cached artifacts, and atomically replaces
invalid ones. Firmware providers expose explicit CMake targets and output
paths to their ports.

Controller-specific conversion, such as CC256x BTS-to-C generation, is a
separate build step that writes generated sources to the build directory.

## Consequences

Configuration is offline and reproducible. A build requiring a missing
artifact performs the verified download; failed downloads or checksum changes
fail the build without overwriting a valid cached artifact.
