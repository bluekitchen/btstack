# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Introduced btstack_network.h network interface abstraction
- btstack_network_posix implementation using Linux tun/tap interface
- WICED: support for btstack_stdin (HAVE_STDIN)
- GATT Server: Writes to GATT Client Characteristic Configuration are stored in TLV and restored on reconnect. The db.h file generated from db.gatt needs to be re-created.

### Changed
- panu_demo: uses btstack_network.h now
- WICED: configure printf to replace Linefeed with CRLF
