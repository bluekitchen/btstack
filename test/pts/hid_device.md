Tool: hid_device_test with #define REPORT_ID_DECLARED

HID11/DEV/DCE/BV-10-I: l, (OK), L, (OK)

HID11/DEV/DRE/BV-09-I: (wait), c

HID11/DEV/DCR/BV-01-I: (wait), c, M, M
HID11/DEV/DCR/BV-02-I: I, C, c, M, I, C, (wait), M

NOTE: if you want to redo the next two tests, remove link key on PTS side
HID11/DEV/DCR/BV-03-I: I, C, c, (OK) 
HID11/DEV/DCR/BV-04-I: u, c, (OK)

HID11/DEV/DCR/BV-02-I: c, q, w, z, D
HID11/DEV/DCR/BV-03-I: c, q, w, z, D
HID11/DEV/DCR/BV-04-I: c, q, w, z, D

HID11/DEV/DCT/BV-01-C: (wait)
HID11/DEV/DCT/BV-02-C: (wait)
HID11/DEV/DCT/BV-03-C: (wait)
HID11/DEV/DCT/BV-04-C: (wait)
HID11/DEV/DCT/BV-07-C: I, C

HID11/DEV/DCT/BV-08-C: (wait)
HID11/DEV/DCT/BV-09-C: m
HID11/DEV/DCT/BV-10-C: u
HID11/DEV/DCT/BI-01-C: (wait)
HID11/DEV/DCT/BI-03-C: (wait)
HID11/DEV/DCT/BI-04-C: (wait)

HID11/DEV/BDCT/BV-01-C: (wait)
HID11/DEV/BDCT/BV-02-C: (wait)
HID11/DEV/BDCT/BV-03-C: (wait)
HID11/DEV/BDCT/BI-01-C: (wait)

HID11/DEV/DIT/BV-01-C: M
HID11/DEV/DIT/BV-02-C: (OK)
HID11/DEV/DIT/BI-01-C: (OK)x3

HID11/DEV/BDIT/BV-01-C: m, M
HID11/DEV/BDIT/BV-02-C: (OK)x2
HID11/DEV/BDIT/BI-01-C: (OK)x2

HID11/DEV/SDD/BV-01-C: (wait)
HID11/DEV/SDD/BV-02-C: (wait)
HID11/DEV/SDD/BV-03-C: (wait)
HID11/DEV/SDD/BV-04-C: (long wait)

IOPT/CL/HID-HOS/SFC/BV-15-I: 
    rm /tmp/btstack_*.tlv
    ./hid_device_test
    (OK), c