# L2CAP Test

Tools:
- l2cap_test.c
- le_data_channel.c

# Sequences
- L2CAP/COS/ECH/BV-02-C: l2cap_test - c, p, t
- L2CAP/COS/IEX/BV-01-C: l2cap_test - c, t
- L2CAP/CLS/CLR/BV-01-C: l2cap_test - c, u, t
- L2CAP/COS/CFD/BV-14-C: l2cap_test - (wait)
  
- L2CAP/FIX/BV-01-C:
- L2CAP/EXF/BV-05-C:

- L2CAP/LE/CID/BV-01-C: le_data_channel - y, b, z, s
- L2CAP/LE/CID/BV-02-C: le_data_channel - z, s

- L2CAP/COS/ECFC/BV-01: le_data_channel - S
- L2CAP/COS/ECFC/BV-02: le_data_channel - confirm receive of data
- L2CAP/COS/ECFC/BV-03: le_data_channel - confirm receive of data
- L2CAP/ECFC/BV-01: le_data_channel - d, status 0x65 = Refused PSM - PTS: Command not understood
- L2CAP/ECFC/BV-02: le_data_channel - d
- L2CAP/ECFC/BV-03: le_data_channel - s, s,
- L2CAP/ECFC/BV-04: le_data_channel - D, status 0x65 = Refused PSM - PTS: 0x0002
- L2CAP/ECFC/BV-06: le_data_channel - s, s
- L2CAP/ECFC/BV-07: le_data_channel - m, PTC connects, c
- L2CAP/ECFC/BI-01: le_data_channel - (wait)
- L2CAP/ECFC/BI-02: le_data_channel - m, PTS connects, c, t
- L2CAP/ECFC/BV-08: le_data_channel - t
- L2CAP/ECFC/BV-09: le_data_channel - (wait)
- L2CAP/ECFC/BV-10: le_data_channel - d, status 0x66 = Security issue - PTS: 0x0005
- L2CAP/ECFC/BV-11: le_data_channel - E (for LEVEL_3)
- L2CAP/ECFC/BV-12: le_data_channel - d, status 0x66 = Security issue - PTS: 0x0006
- L2CAP/ECFC/BV-13: le_data_channel - f (for LEVEL_3 + Authorization)
- L2CAP/ECFC/BV-14: le_data_channel - d, status 0x66 = Security issue - PTS: 0x0007
- L2CAP/ECFC/BV-15: le_data_channel - e (for LEVEL_2)
- L2CAP/ECFC/BV-16: le_data_channel - d, s, status 0x67 = Resource issue - PTS: 0x0004
- L2CAP/ECFC/BV-17: le_data_channel - (wait)
- L2CAP/ECFC/BV-18: le_data_channel - d, status 0x0f = Unknown error - PTS: 0x0009
- L2CAP/ECFC/BV-19: le_data_channel - d, status 0x0f = Unknown error - PTS: 0x000A 
- L2CAP/ECFC/BV-20: le_data_channel - (wait)
- L2CAP/ECFC/BV-21: le_data_channel - d, status 0x0f = Unknown error - PTS: 0x000B
- L2CAP/ECFC/BV-22: le_data_channel - r
- L2CAP/ECFC/BV-23: le_data_channel - S, S, (reconfigure), S, S
- L2CAP/ECFC/BI-03: le_data_channel - t
- L2CAP/ECFC/BV-24: le_data_channel - r, r
- L2CAP/ECFC/BV-25: le_data_channel - S, S, S, S, S, S, (reconfigure), S, S, S, S, S, (reconfigure), S, S, S, S, S
- L2CAP/ECFC/BI-04: le_data_channel - (wait)
- L2CAP/ECFC/BV-26: le_data_channel - (wait)
- L2CAP/ECFC/BV-27: le_data_channel - s
- L2CAP/ECFC/BI-05: le_data_channel - (wait)
- L2CAP/ECFC/BI-06: le_data_channel - (wait)
