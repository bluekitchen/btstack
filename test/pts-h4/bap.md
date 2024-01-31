
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

### BAP/UCL/ADV
- BAP/USR/DISC/BV-01-C: 9

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

- BAP/UCL/PD/BV-01-C: c, d, x, y, K, l, 

### BASS Client:
- BAP/BA/CGGIT/SER/BV-01-C  : c, b
- BAP/BA/CGGIT/CHA/BV-01-C  : c, b
- BAP/BA/CGGIT/CHA/BV-02-C  : c, b

## Unicast Server: pts/gatt_profiles_bap

Tool: test/le_audio/le_broadcast_sink

## BASS Server:
- BAP/SDE/BASS/BV-01-C:    s

Tool: test/le_audio/le_broadcast_assistant

## BASS Client:
- BAP/BA/BASS/BV-01-C    : see pts-h4/bap_service_client_test
- BAP/BA/BASS/BV-02-C    : see pts-h4/bap_service_client_test
- BAP/BA/BASS/BV-03-C    : see pts-h4/bap_service_client_test
- BAP/BA/BASS/BV-04-C    : c, C
- BAP/BA/BASS/BV-05-C    : c, a
- BAP/BA/BASS/BV-06-C    : c, a, m
- BAP/BA/BASS/BV-07-C    : c, a, m, r
- BAP/BA/BASS/BV-08-C    : c,
- BAP/BA/BASS/BV-09-C    : see pts-h4/bap_service_client_test

## Broadcast Assistant
- BAP/BA/BASS/BV-01-C    : c,
- BAP/BA/BASS/BV-02-C    : c, s
- BAP/BA/BASS/BV-03-C    : c, S
- BAP/BA/BASS/BV-04-C    : c, C
- BAP/BA/BASS/BV-05-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-06-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-07-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-08-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-09-C    : c, p

## CSIS Client -> Documentation moved to pts-h4/csip_client_test.md
