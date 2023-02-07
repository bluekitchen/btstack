# GAP Central/Observer Test

## IXIT
- Set TSPX_advertising_data to 02010008094254737461636B (Flags = 0, Name "BTstack")- 
- Set TSPX_gap_iut_role to Central
- Set TSPX_psm_2 to 1001

## IUT
- Set PTS address

## Notes
Two tests requires old BR-only dongle:
- GAP/DM/LEP/BV-02-C:
- GAP/DM/LEP/BV-02-C:
With the white dongle, test hangs: [CASE0071230](https://bluetooth.service-now.com/ess/case_detail.do?sysparm_document_key=x_bsig_case_manage_case,331db3171bb7a810bb65db1dcd4bcbc0)

1 tests require BR/EDR dongle without SC:
- GAP/SEC/SEM/BV-16-C
 
Some tests require IUT dongle with support for SC/EDR

Some tests require: 
GAP/SEC/SEM/BI-05-C

## Sequences
- GAP/BROB/BCST/*: -> Peripheral

- GAP/BROB/OBSV/BV-01-C: s
- GAP/BROB/OBSV/BV-02-C: s
- GAP/BROB/OBSV/BV-03-C: S
- GAP/BROB/OBSV/BV-03-C: p, b, s

- GAP/DISC/NONM/* -> Peripheral
- GAP/DISC/LIMM/* -> Peripheral
- GAP/DISC/GENM/* -> Peripheral
 
- GAP/DISC/LIMP/BV-01-C: s, "OK" if "LE Limited Discoverable Mode" is     shown
- GAP/DISC/LIMP/BV-02-C: s, "OK" if "LE Limited Discoverable Mode" is NOT shown (General discoverable)
- GAP/DISC/LIMP/BV-03-C: s, "OK" if "LE Limited Discoverable Mode" is NOT shown (Non-connectable)
- GAP/DISC/LIMP/BV-04-C: s, "OK" if "LE Limited Discoverable Mode" is NOT shown (Non-discoverable)
- GAP/DISC/LIMP/BV-05-C: s, "OK" if "LE Limited Discoverable Mode" is NOT shown (Connectable Directed Adv)

- GAP/DISC/GENP/BV-01-C: s, OK" if "LE General Discoverable Mode" is shown
- GAP/DISC/GENP/BV-02-C: s, OK" if "LE Limited Discoverable Mode" is shown
- GAP/DISC/GENP/BV-03-C: s, OK" if "LE ??????? Discoverable Mode" is NOT shown
- GAP/DISC/GENP/BV-04-C: s, OK" if "LE ??????? Discoverable Mode" is NOT shown
- GAP/DISC/GENP/BV-05-C: s, OK" if "LE ??????? Discoverable Mode" is NOT shown

- GAP/DISC/RPA/BV-01-C: p, b, s

- GAP/IDLE/NAMP/BV-01-C: p, n, t
- GAP/IDLE/NAMP/BV-02-C -> GAP Peripheral 

- GAP/IDLE/GIN/BV-01-C: i
- GAP/IDLE/LIN/BV-01-C: I

- GAP/IDLE/DED/BV-02-C: i

- GAP/IDLE/BON/BV-02-C: N
- GAP/IDLE/BON/BV-03-C: l
- GAP/IDLE/BON/BV-04-C: M, l
- GAP/IDLE/BON/BV-05-C: N, N
- GAP/IDLE/BON/BV-06-C: 3, N

- GAP/CONN/NCON/BV-01-C -> GAP Peripheral
- GAP/CONN/NCON/BV-02-C -> GAP Peripheral
- GAP/CONN/NCON/BV-03-C -> GAP Peripheral

- GAP/CONN/DCON/BV-01-C -> GAP Peripheral

- GAP/CONN/UCON/BV-01-C -> GAP Peripheral
- GAP/CONN/UCON/BV-02-C -> GAP Peripheral
- GAP/CONN/UCON/BV-03-C -> GAP Peripheral
- GAP/CONN/UCON/BV-04-C -> GAP Peripheral

- GAP/CONN/ACEP/BV-01-C: p, t
- GAP/CONN/ACEP/BV-03-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device" 
- GAP/CONN/ACEP/BV-04-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device"

- GAP/CONN/GCEP/BV-01-C: p, t
- GAP/CONN/GCEP/BV-02-C: p, t
- GAP/CONN/GCEP/BV-05-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device"
- GAP/CONN/GCEP/BV-06-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device"

- GAP/CONN/SCEP/BV-01-C: p, t
- GAP/CONN/SCEP/BV-03-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device"

- GAP/CONN/DCEP/BV-01-C: p, t (if it fails, retry)
- GAP/CONN/DCEP/BV-03-C: p, t (if it fails, retry)
- GAP/CONN/DCEP/BV-05-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device"
- GAP/CONN/DCEP/BV-06-C: TODO - PTS "Selected device doesn't support EXTENDED features (EXT ADV, EXT SCAN). Selected test case can not execute with selected device"

# BV-1 to BV-3 are NOT 21_9 - Connection Parameter Update Procedure
- GAP/CONN/CPUP/BV-01-C: C, a, z
- GAP/CONN/CPUP/BV-02-C: C, a, z - wait for timeout
- GAP/CONN/CPUP/BV-03-C: C, a
- GAP/CONN/CPUP/BV-04-C: p, t (restart if connect fails when adv are still on)
- GAP/CONN/CPUP/BV-05-C: p, t
 
# The selected test case requires to have LE only device to turn off the Connection Parameters Request Procedure LL feature bit to 0
- GAP/CONN/CPUP/BV-06-C: p, Z, t
- 
- GAP/CONN/CPUP/BV-08-C: C, a
- GAP/CONN/CPUP/BV-10-C: C, a

- GAP/CONN/TERM/BV-01-C: p, t

- GAP/CONN/PRDA/BV-01-C: C, a, t, t, t
# Requires TSPX_security_enabled = true for resolvable address
- GAP/CONN/PRDA/BV-02-C: f, BD_ADDR, p, b, P, b, P, b

- GAP/EST/LIE/BV-02-C: 9

- GAP/BOND/NBON/BV-01-C: p, d, b, p, b
- GAP/BOND/NBON/BV-02-C: p, d, b, p, b
- GAP/BOND/NBON/BV-03-C: C, a, d

- GAP/BOND/BON/BV-01-C: C, D, a, b
- GAP/BOND/BON/BV-02-C: C, D, p, b, p,  
- GAP/BOND/BON/BV-03-C: C, a, 
# TODO LE
- GAP/BOND/BON/BV-04-C: C, p, t, p, b (to start encryption)

- GAP/SEC/SEM/BV-02-C -> GAP Peripheral
- GAP/SEC/SEM/BV-04-C -> GAP Peripheral
- GAP/SEC/SEM/BV-05-C: 2, d, N, t
- GAP/SEC/SEM/BV-50-C: 2, C, N, t, N, t
- GAP/SEC/SEM/BV-06-C: 2, N, t
- GAP/SEC/SEM/BV-51-C: 2, C, N, t, N, t
- GAP/SEC/SEM/BV-07-C: 3, C, N, 
- GAP/SEC/SEM/BV-52-C: 3, N, t, N, t
- GAP/SEC/SEM/BV-08-C: Fragile PTS test! OK, N, YES, now PTS waits but just continue, t, OK, N, YES
- GAP/SEC/SEM/BV-09-C: F - restart tester - 2, N, 3, N, t 
- GAP/SEC/SEM/BV-53-C: F - restart tester - 2, C, N, t, N, 3, N
- GAP/SEC/SEM/BV-10-C -> GAP Peripheral
- GAP/SEC/SEM/BI-24-C -> GAP Peripheral
- GAP/SEC/SEM/BV-46-C -> GAP Peripheral
- GAP/SEC/SEM/BV-11-C -> GAP Peripheral
- GAP/SEC/SEM/BV-12-C -> GAP Peripheral
- GAP/SEC/SEM/BV-13-C -> GAP Peripheral
- GAP/SEC/SEM/BV-47-C -> GAP Peripheral
- GAP/SEC/SEM/BV-14-C -> GAP Peripheral
- GAP/SEC/SEM/BV-48-C -> GAP Peripheral
- GAP/SEC/SEM/BV-15-C -> GAP Peripheral
- GAP/SEC/SEM/BV-49-C -> GAP Peripheral
- GAP/SEC/SEM/BV-16-C: K, N, wait, confirm reject - requires dongle without support for SC
- GAP/SEC/SEM/BV-17-C: F - restart tester - K, N, wait, confirm reject 
- GAP/SEC/SEM/BV-18-C: F - restart tester - K, N - requires dongle with support for SC 
- GAP/SEC/SEM/BV-54-C  F - restart tester - K, N, N - requires dongle with support for SC
- GAP/SEC/SEM/BV-19-C: F - restart tester - k, N - requires dongle with support for SC
- GAP/SEC/SEM/BV-55-C  k, N, N
- GAP/SEC/SEM/BV-20-C: F - restart tester - 4, N, wait, confirm reject
- GAP/SEC/SEM/BV-21-C: 5, M, J, C, a, g0009, b (5,M,J - Level 4)
- GAP/SEC/SEM/BV-37-C: m, j, C, a, g0009, b    (m,J - Level 2)
- GAP/SEC/SEM/BV-38-C: 5, M, J, C, a, g0009, b (5,M,J - Level 4)
- GAP/SEC/SEM/BV-22-C: 5, M, J, C, a, 000F, Confirm Passkey (5,M,J - Level 4)
- GAP/SEC/SEM/BV-39-C: m, J, C, a, 0011        (5,J,M - Level 2)
- GAP/SEC/SEM/BV-40-C: m, J, C, a, 0011        (5,J,M - Level 2)
- GAP/SEC/SEM/BV-23-C: M, J, K, C, a, G0009, 00, b
- GAP/SEC/SEM/BV-24-C: C, a, 000f
- GAP/SEC/SEM/BV-25-C: J, K, M, C, a, G0009, 00, b, (PTS waits for Write request, ignore Read request), t, confirm
- GAP/SEC/SEM/BV-26-C: F, 5, M, J, p, g0009, b
- GAP/SEC/SEM/BV-41-C: F, 5, m, J, p, g0009, b
- GAP/SEC/SEM/BV-42-C: F, 5, M, J, p, g0009, b
- GAP/SEC/SEM/BV-27-C: F, 5, M, J, p, 000f
- GAP/SEC/SEM/BV-43-C: F, 5, m, J, p, 0011
- GAP/SEC/SEM/BV-44-C: F, 5, M, J, p, 000f
- GAP/SEC/SEM/BV-28-C: K, p, G0009, b
- GAP/SEC/SEM/BV-29-C: K, p, 000f
- GAP/SEC/SEM/BV-30-C: p, G0009, 00, b, N
- GAP/SEC/SEM/BV-31-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BV-32-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BV-34-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BV-35-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BI-01-C -> GAP Peripheral
- GAP/SEC/SEM/BI-02-C -> GAP Peripheral
- GAP/SEC/SEM/BI-03-C -> GAP Peripheral
- GAP/SEC/SEM/BI-04-C -> GAP Peripheral
- GAP/SEC/SEM/BI-05-C: delete link keys, 0, N, N, N, N, N, N
- GAP/SEC/SEM/BI-06-C: delete link keys, N, N, N, N, N, N
- GAP/SEC/SEM/BI-07-C: delete link keys, N, N, N, N, N, N
- GAP/SEC/SEM/BI-08-C: delete link keys, N, N, N, N, N, N, ... N - requires LEVEL_4
- GAP/SEC/SEM/BI-09-C: C, K, a
- GAP/SEC/SEM/BI-10-C: C, K,  { p, b, t }
  GAP/SEC/SEM/BI-11-C -> GAP Peripheral
- GAP/SEC/SEM/BI-12-C: delete link keys, N, N, N, N, N, N
- GAP/SEC/SEM/BI-13-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BV-45-C: NEW LE
- GAP/SEC/SEM/BI-14-C -> GAP Peripheral
- GAP/SEC/SEM/BI-14-C -> GAP Peripheral
- GAP/SEC/SEM/BI-15-C -> GAP Peripheral
- GAP/SEC/SEM/BI-16-C -> GAP Peripheral
- GAP/SEC/SEM/BI-17-C: delete link keys, N, N, N, N, N, N, ... N - requires LEVEL_4
- GAP/SEC/SEM/BI-18-C: delete link keys, N, N, N, N, N, N, ... N - requires LEVEL_4
- GAP/SEC/SEM/BI-19-C: delete link keys, N, N, N, N, N, N, ... N - requires LEVEL_4
- GAP/SEC/SEM/BI-20-C: C, K, a
- GAP/SEC/SEM/BI-21-C: C, K, a
- GAP/SEC/SEM/BI-22-C: C, K,  { p, b, t }
- GAP/SEC/SEM/BI-23-C: C, K,  { p, b, t }
 
- GAP/SEC/AUT/BV-02-C -> GAP Peripheral
- GAP/SEC/AUT/BV-11-C -> GAP Peripheral
- GAP/SEC/AUT/BV-12-C: M, 5, p, 000b, "enter secure id"
- GAP/SEC/AUT/BV-13-C: M, 5, p, 000b, "enter secure id"
- GAP/SEC/AUT/BV-14-C -> GAP Peripheral
- GAP/SEC/AUT/BV-17-C: p, HANDLE, p, g, HANDLE
- GAP/SEC/AUT/BV-18-C  a, g, HANDLE, b, g, HANDLE
- GAP/SEC/AUT/BV-19-C: p, b, p, g, HANDLE, b, g, HANDLE, t
- GAP/SEC/AUT/BV-20-C: C, a, g, HANDLE, b, g, HANDLE
- GAP/SEC/AUT/BV-21-C: p, b, p, b, t
- GAP/SEC/AUT/BV-22-C: C, a, b
- GAP/SEC/AUT/BV-23-C -> GAP Peripheral
- GAP/SEC/AUT/BV-24-C: 5, M, p, t, p, 0009, 

- GAP/SEC/CSIGN/BV-01-C: p, b, "enter secure id", p, w
- GAP/SEC/CSIGN/BV-02-C: p, b, p, 1
- GAP/SEC/CSIGN/BI-01-C: p, b, p, 1, t    (signature is invalid)
- GAP/SEC/CSIGN/BI-02-C: p, b, p, 1, t    (sign counter resets)
- GAP/SEC/CSIGN/BI-03-C: p, b, F, p, 1, t (bonding info deleted -> CSRK not available)
- GAP/SEC/CSIGN/BI-04-C: H, p, b, p,1, t  (characteristic requires authentication write permissions)

- GAP/PRIV/CONN/BV-10-C -> GAP Peripheral
- GAP/PRIV/CONN/BV-11-C: p, b, P

- GAP/ADV/XXX -> GAP Peripheral

- GAP/GAT/BV-04-C -> GAP Peripheral
- GAP/GAT/BV-05-C -> GAP Peripheral
- GAP/GAT/BV-06-C -> GAP Peripheral

- GAP/DM/NCON/BV-01-C: c, a (we need to enable non-connectable advertisements)
- GAP/DM/CON/BV-01-C: C, a
- GAP/DM/NBON/BV-01-C: F - restart tester - C, d, p, p, p
- GAP/DM/BON/BV-01-C: C, D, N, t, p, b, p, b
- GAP/DM/GIN/BV-01-C: i, s
- GAP/DM/LIN/BV-01-C: I, wait for PTS to show up, s, "OK" if "LE Limited Discoverable Mode"
- GAP/DM/NAD/BV-01-C: i
- GAP/DM/NAD/BV-02-C: p, n, t
- GAP/DM/LEP/BV-01-C: C, a,
- GAP/DM/LEP/BV-02-C: C (Test requires an old dongle, but it passes with 5.0 dongle) 
- GAP/DM/LEP/BV-04-C: 9, t
- GAP/DM/LEP/BV-05-C: 9, t (Test requires an old dongle, but it passes with 5.0 dongle) 
- GAP/DM/LEP/BV-06-C: p - Test requires LE-only dongle on PTS side
- GAP/DM/LEP/BV-07-C: C, a,
- GAP/DM/LEP/BV-08-C: C, a,
- GAP/DM/LEP/BV-11:C: C, wait quite a while, p, t
- GAP/DM/LEP/BV-12:C: NEW CTKD LE -> BR/EDR
- GAP/DM/LEP/BV-13:C: NEW CTKD Upgrade
- GAP/DM/LEP/BV-14:C: NEW CTKD LE -> BR/EDR
- GAP/DM/LEP/BV-15:C: NEW CTKD LE -> BR/EDR
- GAP/DM/LEP/BV-16:C: NEW CTKD LE -> BR/EDR
- GAP/DM/LEP/BV-17:C: NEW CTKD BR/EDR -> LE
- GAP/DM/LEP/BV-18:C: NEW CTKD BR/EDR -> LE
- GAP/DM/LEP/BV-19:C: NEW CTKD BR/EDR -> LE
- GAP/DM/LEP/BV-20:C: NEW CTKD BR/EDR -> LE - don't overwrite stronger key
- GAP/DM/LEP/BV-21:C: NEW CTKD BR/EDR -> LE - don't overwrite stronger key
- GAP/DM/LEP/BV-22:C: NEW CTKD LE -> BR/EDR - don't overwrite stronger key
- GAP/DM/LEP/BV-23:C: NEW CTKD LE -> BR/EDR - don't overwrite stronger key
- GAP/DM/LEP/BI-01:C: NEW CTKD BR/EDR -> LE - P192 not sufficient
- GAP/DM/LEP/BI-02:C: NEW CTKD BR/EDR -> LE - P192 not sufficient

- GAP/MOD/* -> GAP Peripheral

## TODO
 
