# L2CAP Test

Tools:
- l2cap_test.c
- l2cap_cbm_ecbm.c

# Sequences
- L2CAP/COS/ECH/BV-02-C: l2cap_test - c, p, t
- L2CAP/COS/IEX/BV-01-C: l2cap_test - c, t
- L2CAP/CLS/CLR/BV-01-C: l2cap_test - c, u, t
- L2CAP/COS/CFD/BV-14-C: l2cap_test - (wait)
  
- L2CAP/FIX/BV-01-C:
- L2CAP/EXF/BV-05-C:

- L2CAP/LE/CID/BV-01-C: l2cap_cbm_ecbm - y, b, z, s
- L2CAP/LE/CID/BV-02-C: l2cap_cbm_ecbm - z, s

- L2CAP/COS/ECFC/BV-01: l2cap_cbm_ecbm - S
- L2CAP/COS/ECFC/BV-02: l2cap_cbm_ecbm - confirm receive of data
- L2CAP/COS/ECFC/BV-03: l2cap_cbm_ecbm - confirm receive of data
- L2CAP/ECFC/BV-01: l2cap_cbm_ecbm - d, status 0x65 = Refused PSM - PTS: Command not understood
- L2CAP/ECFC/BV-02: l2cap_cbm_ecbm - d
- L2CAP/ECFC/BV-03: l2cap_cbm_ecbm - s, s,
- L2CAP/ECFC/BV-04: l2cap_cbm_ecbm - D, status 0x65 = Refused PSM - PTS: 0x0002
- L2CAP/ECFC/BV-06: l2cap_cbm_ecbm - s, s
- L2CAP/ECFC/BV-07: l2cap_cbm_ecbm - m, PTC connects, c
- L2CAP/ECFC/BI-01: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BI-02: l2cap_cbm_ecbm - m, PTS connects, c, t
- L2CAP/ECFC/BV-08: l2cap_cbm_ecbm - t
- L2CAP/ECFC/BV-09: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BV-10: l2cap_cbm_ecbm - d, status 0x66 = Security issue - PTS: 0x0005
- L2CAP/ECFC/BV-11: l2cap_cbm_ecbm - E (for LEVEL_3)
- L2CAP/ECFC/BV-12: l2cap_cbm_ecbm - d, status 0x66 = Security issue - PTS: 0x0006
- L2CAP/ECFC/BV-13: l2cap_cbm_ecbm - f (for LEVEL_3 + Authorization)
- L2CAP/ECFC/BV-14: l2cap_cbm_ecbm - d, status 0x66 = Security issue - PTS: 0x0007
- L2CAP/ECFC/BV-15: l2cap_cbm_ecbm - e (for LEVEL_2)
- L2CAP/ECFC/BV-16: l2cap_cbm_ecbm - d, s, status 0x67 = Resource issue - PTS: 0x0004
- L2CAP/ECFC/BV-17: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BV-18: l2cap_cbm_ecbm - d, status 0x0f = Unknown error - PTS: 0x0009
- L2CAP/ECFC/BV-19: l2cap_cbm_ecbm - d, status 0x0f = Unknown error - PTS: 0x000A 
- L2CAP/ECFC/BV-20: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BV-21: l2cap_cbm_ecbm - d, status 0x0f = Unknown error - PTS: 0x000B
- L2CAP/ECFC/BV-22: l2cap_cbm_ecbm - r
- L2CAP/ECFC/BV-23: l2cap_cbm_ecbm - S, S, (reconfigure), S, S
- L2CAP/ECFC/BI-03: l2cap_cbm_ecbm - t
- L2CAP/ECFC/BV-24: l2cap_cbm_ecbm - r, r
- L2CAP/ECFC/BV-25: l2cap_cbm_ecbm - S, S, S, S, S, S, (reconfigure), S, S, S, S, S, (reconfigure), S, S, S, S, S
- L2CAP/ECFC/BI-04: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BV-26: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BV-27: l2cap_cbm_ecbm - s
- L2CAP/ECFC/BI-05: l2cap_cbm_ecbm - (wait)
- L2CAP/ECFC/BI-06: l2cap_cbm_ecbm - (wait)
