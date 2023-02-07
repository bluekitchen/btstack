# PBAP PCE Test Notes

## Setup
- tool: `pbap_pce_test`
- assert TSPX_bd_addr_iut matches our addrewss
- required config flags: 
    - `ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE`
    - `ENABLE_GOEP_L2CAP`

## ICS
- PCE 1.2.3: enable 2a_3 and 2b_3
- Not implemented:
  - image download
  - missed calls handling
  - BT-UCI
  - BT-UID

## Test Cases
- PBAP/PCE/GOEP/BC/BV-04-I: a, L, t, confirm match
- PBAP/PCE/GOEP/CON/BV-01-C: (wait)
- PBAP/PCE/GOEP/SRM/BV-05: a, g (wait)
  PBAP/PCE/GOEP/SRM/BV-07: a, g (wait)
- TSPC_PBAP_25_5 "Send OBEX SRMP header" disabled in last listing
  - PBAP/PCE/GOEP/SRMP/BV-04: a, g (wait)
  - PBAP/PCE/GOEP/SRMP/BV-05: a, g
- PBAP/PCE/GOEP/SRMP/BV-06: a, g
- PBAP/PCE/GOEP/SRMP/BI-01: a, g
 
- PBAP/PCE/PBB/BV-01-C: a, r
- PBAP/PCE/PBB/BV-02-C: a, r, h
- PBAP/PCE/PBB/BV-03-C: a, r, h
- PBAP/PCE/PBB/BV-05-C: a, v, L
- PBAP/PCE/PBB/BV-41-C: a  - a, r, v, h,   i, h,  o, h,  m, h,  c, h,  s, h,  f, h, 
                                R,  e, h,  i, h,  o, h,  m, h,  c, h
- PBAP/PCE/PBB/BV-42-C: a  - a, r,  e, u, h,  i, u, h,  o, u, h,  m, u, h,  c, u, h,  s, u, h,  f, u, h, 
                                R,  e, u, h,  i, u, h,  o, u, h,  m, u, h,  c, u, h

- PBAP/PCE/PBD/BV-01: a, g
- PBAP/PCE/PBD/BV-04: a, d
- PBAP/PCE/PBD/BV-44: a, v,    g,  i, g,  o, g,  m, g,  c, g,  s, g,  f, g,  b, e, g,  i, g,  o, g,  m, g,  c, g
- PBAP/PCE/PBD/BV-45: a, v, V, g,  i, g,  o, g,  m, g,  c, g,  s, g,  f, g,  b, e, g,  i, g,  o, g,  m, g,  c, g
- PBAP/PCE/PBD/BV-46: a, g, i, g, o, g, m, g, c,g, s, g, f, g, b, g, i, g, o, g, m, g, c, g

- PBAP/PCE/PBF/BV-01-I: a, r, j
- PBAP/PCE/PBF/BV-02-I: a, L
- PBAP/PCE/PBF/BV-03-I: a, r, hx (press x right after starting the list download)
  
- PBAP/PCE/PDF/BV-01-I: a, g (just wait)
- PBAP/PCE/PDF/BV-02-I: a, g, (wait), t
- PBAP/PCE/PDF/BV-04-I: a, g, t
- PBAP/PCE/PDF/BV-06-I: a, g, x

- PBAP/PCE/SSM/BV-01-C: a
- PBAP/PCE/SSM/BV-02-C: a, t
- PBAP/PCE/SSM/BV-06-C: a, p
- PBAP/PCE/SSM/BV-09-C: a
- PBAP/PCE/SSM/BV-10-C: a

- IOPT/CL/PBAP-PCE/SFC/BV-22-I: remove pairing (rm /tmp/btstack*), a

## Tests for UCI + UID
- PBAP/PCE/PBB/BV-33-C: a, - ???
- PBAP/PCE/PBB/BV-34-C: a  - ???
- PBAP/PCE/PBB/BV-35-C: a  - ???
- PBAP/PCE/PBB/BV-36-C: a  - ???
- PBAP/PCE/PBB/BV-43-C: a  - a, ??? -- MTC: IUT did not use the appropriate type headers during the GET operation
- PBAP/PCE/PBD/BV-38: a, g -- MTC: no app parameter.
- PBAP/PCE/PBD/BV-39: a    - MTC FAIL: X-BT-UCI vCard Property bit not set
- PBAP/PCE/PBD/BV-40: a    - MTC FAIL: X-BT-UCI vCard Property bit not set
- PBAP/PCE/PBD/BV-41: a    - MTC FAIL: X-BT-UCI vCard Property bit not set
