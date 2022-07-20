---
name: Bug report
about: Create a report to help us improve
title: ''
labels: ''
assignees: ''

---

**Describe the bug**

A clear and concise description of what the bug is.

**To Reproduce**

Steps to reproduce the behavior:
1. Run example '...'
2. Connect to/from remove device '...'
3. Start action '...'

**Expected behavior**

A clear and concise description of what you expected to happen.

**HCI Packet Logs**
For all Bluetooth issues, please provide a full packet log that includes initial pairing. You need to zip the packet log as GitHub does not know how to handle .pklg files. 

How to get packet log depends on the port:
- For desktop ports, full packet logging is enabled by default and the packet log is available as `/tmp/hci_dump.pklg` (Mac/Linux), or `hci_dump.pklg' (Windows).
- For embedded ports, you can enable #define ENABLE_LOG_INFO in btstack_config.h and uncomment the call to 'hci_dump_init(..)' in the main() function to enable logging over UART Console or Segger RTT. Please capture text output and process using `btstack/tool/create_packet_log.py`. The tool will generate the text file into a .pklg file which can be analysed with Wireshark or Apple's PacketLogger tool.
- For ESP32: BTstack's `port/esp32/integrate.py` script creates one project per example. To enable packet logging, you need to edit app_main in main/main.c of the example project.

**Environment:  (please complete the following information):**
 - Current BTstack branch: [e.g. develop]
 - Bluetooth Controller [e.g. CSR 8510, TI CC2564C, ...]
 - Remote device: [e.g. iPhone X with iOS 13.3]

**Additional context**
Add any other context about the problem here.
