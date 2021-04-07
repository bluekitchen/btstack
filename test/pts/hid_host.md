Tool: hid_host_test with #define REPORT_ID_DECLARED

HID11/HOS/HCE/BV-01-I: c
HID11/HOS/HCE/BV-02-I: c
HID11/HOS/HCE/BV-05-I: c
HID11/HOS/HCE/BV-08-I: OK, c, (Confirmation)

HID11/HOS/HRE/BV-01-I: rm /tmp/btstack*.tlv, c, CTRL+c, wait 
HID11/HOS/HRE/BV-02-I: rm /tmp/btstack*.tlv, c, CTRL+c, c, c
HID11/HOS/HRE/BV-05-I: rm /tmp/btstack*.tlv, c, CTRL+c, "OK", "OK"
HID11/HOS/HRE/BV-06-I: rm /tmp/btstack*.tlv, c, CTRL+c, c, c
HID11/HOS/HRE/BV-09-I: c, s, S
HID11/DEV/DCE/BV-10-I: l, (OK), L, (OK)
HID11/HOS/HRE/BI-01-I: rm /tmp/btstack*.tlv, c
HID11/HOS/HRE/BI-02-I: rm /tmp/btstack*.tlv, c, c

HID11/HOS/HCR/BV-01-I: c, C, c, (Confirmation), C, (Confirmation)
HID11/HOS/HCR/BV-02-I: c, c, (Confirmation), (Confirmation)
HID11/HOS/HCR/BV-03-I: c, U, c, "OK", u
HID11/HOS/HCR/BV-04-I: c, c, (ok)

HID11/HOS/HGR/BV-02-C: rm /tmp/btstack*.tlv, c, y, w
HID11/HOS/HGR/BV-03-C: rm /tmp/btstack*.tlv, c, y

HID11/HOS/HCT/BV-01-C: c, 1, 2, 3
HID11/HOS/HCT/BV-02-C: c, 4
HID11/HOS/HCT/BV-03-C: c, 5
HID11/HOS/HCT/BV-04-C: c, p
HID11/HOS/HCT/BV-05-C: c, r
HID11/HOS/HCT/BV-06-C: c, U, c, u
HID11/HOS/HCT/BV-07-C: c, s
HID11/HOS/HCT/BV-08-C: c, s, S
HID11/HOS/HCT/BI-01-C: c, (wait)
HID11/HOS/HCT/BI-02-C: c, 1, (Confirmation), 3, (Confirmation), 3, (Confirmation)

HID11/HOS/BHCT/BV-02-C: a, 3
HID11/HOS/BHCT/BV-03-C: a, 7
HID11/HOS/BHCT/BV-03-C: a, o // PTS test fails

HID11/HOS/BHCT/BI-01-C: a, 3, (Confirmation) , 3, (Confirmation), 3, (Confirmation)

HID11/HOS/HIT/BV-01-C: c
HID11/HOS/HIT/BV-02-C: c, 7
HID11/HOS/HIT/BI-01-C: c, 3, (Confirmation), 3, (Confirmation), 3, (Confirmation)

HID11/HOS/BHIT/BV-01-C: a
HID11/HOS/BHIT/BV-02-C: a, o (ID 1, size 9)
    Channel ID: 0x0041  Length: 0x000A (10) [ A2 01 00 00 00 00 00 00 00 00 ]
    Channel ID: 0x0040  Length: 0x000A (10) [ 52 01 00 00 00 00 00 00 00 00 ]

HID11/HOS/BHIT/BI-01-C: a

HID11/HOS/CDD/BV-01-I: c, C, c, CRTL+c