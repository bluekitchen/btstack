
Tool: pts-h4/bap_service_client_test

PTS IXIT Configuration: 
- TSPX_Codec_ID set to FF00000000

## Unicast Client: pts-h4/bap_service_client_test

### BAP/UCL/CGGIT
- BAP/CL/CGGIT/SER/BV-01-C:  c, p
- BAP/CL/CGGIT/CHA/BV-01-C:  c, p
- BAP/CL/CGGIT/CHA/BV-02-C:  c, p, e, E
- BAP/CL/CGGIT/CHA/BV-03-C:  c, p, 
- BAP/CL/CGGIT/CHA/BV-04-C:  c, p, f, F
- BAP/CL/CGGIT/CHA/BV-05-C:  c, p, g
- BAP/CL/CGGIT/CHA/BV-06-C:  c, p, h
 
- BAP/UCL/CGGIT/SER/BV-01-C: c, p 
- BAP/UCL/CGGIT/CHA/BV-01-C: c, d, u
- BAP/UCL/CGGIT/CHA/BV-02-C: c, d, u, u, u
- BAP/UCL/CGGIT/CHA/BV-03-C: c, d, No, Yes, K

### BAP/UCL/DISC
- BAP/UCL/DISC/BV-01-C: c, p, i, e, X
- BAP/UCL/DISC/BV-02-C: c, p, j, f, X
- BAP/UCL/DISC/BV-03-C: c, d, X
- BAP/UCL/DISC/BV-04-C: c, d, u, u, X
- BAP/UCL/DISC/BV-05-C: c, p, h, X
- BAP/UCL/DISC/BV-06-C: c, p, g, X

### BAP/UCL/SCC
- BAP/UCL/ADV/BV-01-C: ?

### BAP/UCL/SCC -> auto-pts
- BAP/UCL/SCC/BV-001-C: c, d, K
- BAP/UCL/SCC/BV-033-C: y,y,y,y,y, z,z,z,z, c, d, T 
- BAP/UCL/SCC/BV-035-C: c, d, K, l
- BAP/UCL/SCC/BV-101-C: c, d, z, K, y, l, M
- BAP/UCL/SCC/BV-102-C: c, d, z, K, y, l, M, n
- BAP/UCL/SCC/BV-103-C: c, d, z, K, y, l, M, n, r, N
- BAP/UCL/SCC/BV-104-C: c, d, z, K, y, l, M, r
- BAP/UCL/SCC/BV-105-C: c, d, z, K, y, l, M, n, r, N
- BAP/UCL/SCC/BV-106-C: c, d, z, K, R
- BAP/UCL/SCC/BV-107-C: c, d, z, K, R
- BAP/UCL/SCC/BV-108-C: c, d, z, K, y, l, R
- BAP/UCL/SCC/BV-109-C: c, d, z, K, y, l, R
- BAP/UCL/SCC/BV-110-C: c, d, z, K, y, l, M, R
- BAP/UCL/SCC/BV-111-C: c, d, z, K, y, l, M, R
- BAP/UCL/SCC/BV-112-C: c, d, z, K, y, l, M, n, R
- BAP/UCL/SCC/BV-113-C: c, d, z, K, y, l, M, n, r, R
- BAP/UCL/SCC/BV-115-C: c, d, z, K, y, l, M, Q
- BAP/UCL/SCC/BV-116-C: c, d, z, K, y, l, M, Q
- BAP/UCL/SCC/BV-117-C: c, d, z, K, y, l, M, n, Q

### BAP/UCL/P
- BAP/UCL/PD/BV-01-C: c, d, x, y, K, l, 
- BAP/UCL/PD/BV-02-C: ?
- BAP/UCL/PD/BV-03-C: ?
- BAP/UCL/PD/BV-04-C: ?

### BAP/UCL/STR -> auto-pts

### BAP/USR/STR -> auto-pts

### BAP/USR/ADV
- BAP/USR/DISC/BV-01-C: 9
- BAP/USR/DISC/BV-04-C: ?

### BAP/USR/SCC -> auto-pts

### BAP/USR/SPE
- BAP/USR/SPE/BI-01-C: ?
- BAP/USR/SPE/BI-02-C: ?
- BAP/USR/SPE/BI-03-C: ?
- BAP/USR/SPE/BI-04-C: ?
- BAP/USR/SPE/BI-05-C: ?

### BAP/BSRC/SCC -> auto-pts

### BAP/BSRC/STR -> auto-pts

### BAP/BSRC/SCC -> auto-pts

### BAP/BSNK/SCC -> auto-pts

### BAP/BSNK/ADV -> auto-pts

### BAP/BSNK/STR -> auto-pts

### BAP/SDE
Tool: test/le_audio/le_broadcast_sink
- BAP/SDE/BASS/BV-01-C: s

### BAP/BA/CGGIT -> auto-pts
- BAP/BA/CGGIT/SER/BV-01-C  : c, b
- BAP/BA/CGGIT/CHA/BV-01-C  : c, b
- BAP/BA/CGGIT/CHA/BV-02-C  : c, b
 
#### BAP/BA/ADV -> auto-pts

## BAP/BA/BASS
Tool: pts-h4/bap_service_client_test
TSPX_Public_bd_addr_LT2: enter address of LT2 in LT1 and test tool
ADD Source operation in BV-04 must contain address and adv_sid of LT2

- BAP/BA/BASS/BV-01-C     : c, b
- BAP/BA/BASS/BV-02-C     : c, b, s
- BAP/BA/BASS/BV-03-C     : c, b, S
- BAP/BA/BASS/BV-04-C     : start LT2, c, b, C
- BAP/BA/BASS/BV-04-C_LT2 : ok
- BAP/BA/BASS/BV-05-C     : start LT2, c, b, C, wait until LT1 has synchronized, m, ok
- BAP/BA/BASS/BV-05-C_LT2 : ok
- BAP/BA/BASS/BV-06-C     : c, b, a, L
- BAP/BA/BASS/BV-06-C_LT2 : ok
- BAP/BA/BASS/BV-07-C     : start LT2, c, b, C, o
- BAP/BA/BASS/BV-08-C     : test/le_audio_broadcast_assistant
- BAP/BA/BASS/BV-09-C     : c, p
