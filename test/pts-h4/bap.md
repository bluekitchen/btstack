
Tool: pts-h4/bap_service_client_test

## BASS Client:
- BAP/BA/BASS/BV-01-C    : c, 
- BAP/BA/BASS/BV-02-C    : c, s
- BAP/BA/BASS/BV-03-C    : c, S
- BAP/BA/BASS/BV-04-C    : c, C
- BAP/BA/BASS/BV-05-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-06-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-07-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-08-C    : see test/le_audio/le_broadcast_assistant
- BAP/BA/BASS/BV-09-C    : c, p

## Unicast Client: pts-h4/bap_service_client_test
### PACS Client:
- BAP/CL/CGGIT/SER/BV-01-C:  c, p
- BAP/CL/CGGIT/CHA/BV-01-C:  c, p
- BAP/CL/CGGIT/CHA/BV-02-C:  c, p, e, E
- BAP/CL/CGGIT/CHA/BV-03-C:  c, p, 
- BAP/CL/CGGIT/CHA/BV-04-C:  c, p, f, F
- BAP/CL/CGGIT/CHA/BV-05-C:  c, p, g
- BAP/CL/CGGIT/CHA/BV-06-C:  c, p, h
- BAP/UCL/CGGIT/SER/BV-01-C: c, p 
### ASCS Client
- BAP/UCL/CGGIT/CHA/BV-01-C: c, d, k, k
- BAP/UCL/CGGIT/CHA/BV-02-C: c, d, k, k, k, k
- BAP/UCL/CGGIT/CHA/BV-03-C: c, d, No, Yes, K


### PACS Client:
- BAP/UCL/DISC/BV-01-C: c, p, i, e, C
- BAP/UCL/DISC/BV-02-C: c, p, j, f, C
### ASCS Client:
- BAP/UCL/DISC/BV-03-C: c, d, k, ctrl+c
- BAP/UCL/DISC/BV-04-C: c, d, k, k, k, ctrl+c
### PACS Client:
- BAP/UCL/DISC/BV-05-C: c, p, h, C
- BAP/UCL/DISC/BV-06-C: c, p, g, C

### ASCS Client:
- BAP/UCL/SCC/BV-001-C: c, d, k, K
- BAP/UCL/SCC/BV-035-C: c, d, k, K, l
- BAP/UCL/SCC/BV-101-C: c, d, k, L, m, M, n

### BASS Client:
- BAP/BA/CGGIT/SER/BV-01-C  : c, b
- BAP/BA/CGGIT/CHA/BV-01-C  : c, b
- BAP/BA/CGGIT/CHA/BV-02-C  : c, b

## Unicast Server: pts/gatt_profiles_bap
- BAP/USR/DISC/BV-01-C: // PTS tool log: Wait for Ext ADV event
                        // check why Ext ADV Event does not come

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