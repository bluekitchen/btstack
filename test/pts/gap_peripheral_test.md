# GAP Peripheral/Broadcaster Test

## Notes
Set TSPX_advertising_data to 02010008094254737461636B (Flags = 0, Name "BTstack")
Set TSPX_iut_private_address_interval to 60000 (default 5 seconds, but GAP/BROB/BCST/BV-05-C is tricky with that)

## Sequences
- GAP/BROB/BCST/BV-01-C: 2, A
- GAP/BROB/BCST/BV-02-C: A
- GAP/BROB/BCST/BV-03-C: C, A, R, Z
- GAP/BROB/BCST/BV-04-C: c, r, A
- GAP/BROB/BCST/BV-05-C: C, A, wait for connect and pairing, L, R, wait for request to add, q, "OK" (fragile!, random update set to 60 seconds)

- GAP/BROB/OBSV/BV-01-C -> GAP Central
- GAP/BROB/OBSV/BV-02-C -> GAP Central
- GAP/BROB/OBSV/BV-05-C -> GAP Central
- GAP/BROB/OBSV/BV-06-C -> GAP Central

- GAP/DISC/NONM/BV-01-C: d, c, A
- GAP/DISC/NONM/BV-02-C: d, C, A

- GAP/DISC/LIMM/BV-01-C: T, A, a (wait 30 sec)
- GAP/DISC/LIMM/BV-02-C: T, C, A, a (wait 30 sec)

- GAP/DISC/GENM/BV-01-C: D, a, d
- GAP/DISC/GENM/BV-02-C: D, A, C

- GAP/DISC/LIMP/BV-01-C -> GAP Central 
- GAP/DISC/LIMP/BV-02-C -> GAP Central 
- GAP/DISC/LIMP/BV-03-C -> GAP Central 
- GAP/DISC/LIMP/BV-04-C -> GAP Central 
- GAP/DISC/LIMP/BV-05-C -> GAP Central

- GAP/DISC/GENP/BV-01-C -> GAP Central 
- GAP/DISC/GENP/BV-02-C -> GAP Central 
- GAP/DISC/GENP/BV-03-C -> GAP Central 
- GAP/DISC/GENP/BV-04-C -> GAP Central 
- GAP/DISC/GENP/BV-05-C -> GAP Central

- GAP/DISC/RPA/BV-01-C -> GAP Central 

- GAP/IDLE/NAMP/BV-01-C -> GAP Central
- GAP/IDLE/NAMP/BV-02-C: C, A

- GAP/IDLE/GIN/BV-01-C -> GAP Central
 
- GAP/IDLE/DNDIS/BV-01-C: D
 
- GAP/IDLE/LIN/BV-01-C: -> GAP Central

- GAP/IDLE/DED/BV-02-C -> GAP Central
- 
- GAP/IDLE/BON/* -> GAP Central

- GAP/CONN/NCON/BV-01-C: c, A
- GAP/CONN/NCON/BV-02-C: c, D, A
- GAP/CONN/NCON/BV-03-C: T, c, A

- GAP/CONN/DCON/BV-01-C: C, A

- GAP/CONN/UCON/BV-01-C: d, C, A
- GAP/CONN/UCON/BV-02-C: (wait)
- GAP/CONN/UCON/BV-03-C: (wait)
- GAP/CONN/UCON/BV-06-C: C, A, R, Z, wait 60 seconds (see TSPX_iut_private_address_interval)

- GAP/CONN/ACEP/BV-01-C -> GAP Central

- GAP/CONN/GCEP/BV-01-C -> GAP Central
- GAP/CONN/GCEP/BV-02-C -> GAP Central

- GAP/CONN/SCEP/BV-01-C -> GAP Central

- GAP/CONN/DCEP/BV-01-C -> GAP Central 
- GAP/CONN/DCEP/BV-03-C -> GAP Central

- GAP/CONN/CPUP/BV-01-C -> GAP Central 
- GAP/CONN/CPUP/BV-02-C -> GAP Central 
- GAP/CONN/CPUP/BV-03-C -> GAP Central 
- GAP/CONN/CPUP/BV-04-C -> GAP Central 
- GAP/CONN/CPUP/BV-05-C -> GAP Central 
- GAP/CONN/CPUP/BV-06-C -> GAP Central 
- GAP/CONN/CPUP/BV-08-C -> GAP Central 

- GAP/CONN/TERM/BV-01-C -> GAP Central

- GAP/CONN/PRDA/BV-01-C -> GAP Central
- GAP/CONN/PRDA/BV-02-C -> GAP Central

- GAP/EST/LIE/BV-02-C -> GAP Central
 
- BOND -> GAP Central

- GAP/SEC/SEM/BV-02-C: u, (wait)
- GAP/SEC/SEM/BV-04-C: (wait)
- GAP/SEC/SEM/BV-05-C -> GAP Central
- GAP/SEC/SEM/BV-50-C -> GAP Central
- GAP/SEC/SEM/BV-06-C -> GAP Central
- GAP/SEC/SEM/BV-51-C -> GAP Central
- GAP/SEC/SEM/BV-07-C -> GAP Central
- GAP/SEC/SEM/BV-52-C -> GAP Central
- GAP/SEC/SEM/BV-08-C -> GAP Central
- GAP/SEC/SEM/BV-09-C -> GAP Central
- GAP/SEC/SEM/BV-53-C -> GAP Central
- GAP/SEC/SEM/BV-10-C: F, C, D
- GAP/SEC/SEM/BI-24-C: F, C, D, ???
- GAP/SEC/SEM/BV-46-C: NEW LE CBM
- GAP/SEC/SEM/BV-11-C: REQUIRE - PTS v4.0 Dongle without BR/EDR SC. C, D, J
- GAP/SEC/SEM/BV-12-C: F, C, D, J
- GAP/SEC/SEM/BV-13-C: F, C, D, M, B
- GAP/SEC/SEM/BV-47-C: C, D
- GAP/SEC/SEM/BV-14-C: F, C, D
- GAP/SEC/SEM/BV-48-C: C, D
- GAP/SEC/SEM/BV-15-C: Set L2CAP Service Level to 4 in gap_peripheral_test.c, F, C, D
- GAP/SEC/SEM/BV-49-C: Set L2CAP Service Level to 4 in gap_peripheral_test.c, F, C, D
- GAP/SEC/SEM/BV-16-C -> GAP Central
- GAP/SEC/SEM/BV-17-C -> GAP Central
- GAP/SEC/SEM/BV-18-C -> GAP Central
- GAP/SEC/SEM/BV-54-C -> GAP Central
- GAP/SEC/SEM/BV-19-C -> GAP Central
- GAP/SEC/SEM/BV-55-C -> GAP Central
- GAP/SEC/SEM/BV-20-C -> GAP Central
- GAP/SEC/SEM/BV-21-C -> GAP Central
- GAP/SEC/SEM/BV-37-C -> GAP Central
- GAP/SEC/SEM/BV-38-C -> GAP Central
- GAP/SEC/SEM/BV-22-C -> GAP Central
- GAP/SEC/SEM/BV-39-C -> GAP Central
- GAP/SEC/SEM/BV-40-C -> GAP Central
- GAP/SEC/SEM/BV-23-C -> GAP Central
- GAP/SEC/SEM/BV-24-C -> GAP Central
- GAP/SEC/SEM/BV-25-C -> GAP Central
- GAP/SEC/SEM/BV-26-C -> GAP Central
- GAP/SEC/SEM/BV-41-C -> GAP Central
- GAP/SEC/SEM/BV-42-C -> GAP Central
- GAP/SEC/SEM/BV-27-C -> GAP Central
- GAP/SEC/SEM/BV-43-C -> GAP Central
- GAP/SEC/SEM/BV-44-C -> GAP Central
- GAP/SEC/SEM/BV-28-C -> GAP Central
- GAP/SEC/SEM/BV-29-C -> GAP Central
- GAP/SEC/SEM/BV-30-C -> GAP Central
- GAP/SEC/SEM/BV-31-C: not supported yet - LE Security Mode 3`
- GAP/SEC/SEM/BV-32-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BV-34-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BV-35-C: not supported yet - LE Security Mode 3
- GAP/SEC/SEM/BI-01-C: u, C, D (PTS says "send Connection Response" although we did), wait ca. 3 minutes
- GAP/SEC/SEM/BI-11-C: Set L2CAP Service Level to 1 - C, D
- GAP/SEC/SEM/BI-02-C: Set L2CAP Service Level to 2 - C, D
- GAP/SEC/SEM/BI-03-C: Set L2CAP Service Level to 2 - C, D
- GAP/SEC/SEM/BI-04-C: Set L2CAP Service Level to 2 - C, D
- GAP/SEC/SEM/BI-14-C: Set L2CAP Service Level to 2 - C, D
- GAP/SEC/SEM/BI-15-C: Set L2CAP Service Level to 2 - C, D
- GAP/SEC/SEM/BI-16-C: Set L2CAP Service Level to 2 - C, D
- GAP/SEC/SEM/BI-05-C -> GAP Central
- GAP/SEC/SEM/BI-12-C -> GAP Central
- GAP/SEC/SEM/BI-06-C -> GAP Central
- GAP/SEC/SEM/BI-07-C -> GAP Central
- GAP/SEC/SEM/BI-08-C -> GAP Central
- GAP/SEC/SEM/BI-17-C -> GAP Central
- GAP/SEC/SEM/BI-18-C -> GAP Central
- GAP/SEC/SEM/BI-19-C -> GAP Central
- GAP/SEC/SEM/BI-09-C -> GAP Central
- GAP/SEC/SEM/BI-20-C -> GAP Central
- GAP/SEC/SEM/BI-21-C -> GAP Central
- GAP/SEC/SEM/BI-10-C -> GAP Central
- GAP/SEC/SEM/BI-22-C -> GAP Central
- GAP/SEC/SEM/BI-23-C -> GAP Central
- GAP/SEC/SEM/BV-05-C -> GAP Central
- GAP/SEC/SEM/BV-06-C -> GAP Central
- GAP/SEC/SEM/BV-07-C -> GAP Central
- GAP/SEC/SEM/BV-08-C -> GAP Central
- GAP/SEC/SEM/BV-09-C -> GAP Central
- GAP/SEC/SEM/BV-16-C -> GAP Central
- GAP/SEC/SEM/BV-17-C -> GAP Central
- GAP/SEC/SEM/BV-18-C -> GAP Central
- GAP/SEC/SEM/BV-19-C -> GAP Central
- GAP/SEC/SEM/BV-20-C -> GAP Central
- GAP/SEC/SEM/BV-26-C -> GAP Central
- GAP/SEC/SEM/BV-41-C -> GAP Central
- GAP/SEC/SEM/BV-42-C -> GAP Central
- GAP/SEC/SEM/BV-27-C -> GAP Central
- GAP/SEC/SEM/BV-43-C -> GAP Central
- GAP/SEC/SEM/BV-44-C -> GAP Central
- GAP/SEC/SEM/BV-28-C -> GAP Central
- GAP/SEC/SEM/BV-29-C -> GAP Central
- GAP/SEC/SEM/BV-30-C -> GAP Central
- 
- GAP/SEC/AUT/BV-02-C: delete link keys, u, C
- GAP/SEC/AUT/BV-11-C: C, M, e, A, 18
- GAP/SEC/AUT/BV-12-C -> GAP Central
- GAP/SEC/AUT/BV-13-C -> GAP Central
- GAP/SEC/AUT/BV-14-C: C, M, e, A, 18
- GAP/SEC/AUT/BV-17-C -> GAP Central
- GAP/SEC/AUT/BV-18-C -> GAP Central
- GAP/SEC/AUT/BV-19-C -> GAP Central
- GAP/SEC/AUT/BV-20-C -> GAP Central
- GAP/SEC/AUT/BV-21-C -> GAP Central
- GAP/SEC/AUT/BV-22-C -> GAP Central
- GAP/SEC/AUT/BV-23-C: C, M, e, B, A, 18, 
- GAP/SEC/AUT/BV-24-C -> GAP Central

- GAP/SEC/CSIGN/BV-01-C -> GAP Central
- GAP/SEC/CSIGN/BV-02-C -> GAP Central
- GAP/SEC/CSIGN/BI-01-C -> GAP Central
- GAP/SEC/CSIGN/BI-02-C -> GAP Central
- GAP/SEC/CSIGN/BI-03-C -> GAP Central
- GAP/SEC/CSIGN/BI-04-C -> GAP Central

- GAP/PRIV/CONN/BV-10-C: C, A, Z, R, C, A, Z
- GAP/PRIV/CONN/BV-11-C -> GAP Central

- GAP/ADV/BV-01-C: 6, A
- GAP/ADV/BV-02-C: 2, A
- GAP/ADV/BV-03-C: 3, A
- GAP/ADV/BV-04-C: 1, A
- GAP/ADV/BV-05-C: 8, A
- GAP/ADV/BV-08-C: 7, A 
- GAP/ADV/BV-09-C: 5, A
- GAP/ADV/BV-10-C: 4, A
- GAP/ADV/BV-11-C: -, A 
- GAP/ADV/BV-12-C: =, A
- GAP/ADV/BV-13-C: /, A
- GAP/ADV/BV-14-C: #, A
- GAP/ADV/BV-17-C: *, A

- GAP/GAT/BV-04-C: C, A
- GAP/GAT/BV-05-C: C, A
- GAP/GAT/BV-06-C: C, A

- DM -> GAP Central

- GAP/MOD/NDIS/BV-01-C: d

- GAP/MOD/LDIS/BV-01-C: T, D
- GAP/MOD/LDIS/BV-02-C: T, D
- GAP/MOD/LDIS/BV-03-C: T, D, t, d

- GAP/MOD/GDIS/BV-01-C: D, A
- GAP/MOD/GDIS/BV-02-C: t, D, A

- GAP/MOD/NCON/BV-01-C: c

- GAP/MOD/CON/BV-01-C: C
 
- GAP/MOD/NBON/BV-02-C: E, C
- GAP/MOD/NBON/BV-03-C: Set L2CAP Service Level to 2, b, C

