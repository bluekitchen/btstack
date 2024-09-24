# MAP Message Client Equipment (MCE) Test

tooL: example/map_client_demo.c

MAP/MCE/SGSIT/SERR/BV-02-I: a
MAP/MCE/SGSIT/ATTR/BV-08-I: a
MAP/MCE/SGSIT/ATTR/BV-09-I: a
MAP/MCE/SGSIT/ATTR/BV-10-I: a
MAP/MCE/SGSIT/ATTR/BV-11-I: a
MAP/MCE/SGSIT/ATTR/BV-12-I: a
MAP/MCE/SGSIT/OFFS/BV-02-I: a

MAP/MCE/CGSIT/SFC/BV-01-I: a

MAP/MCE/MSM-BV-01-I: a
MAP/MCE/MSM-BV-02-I: a, n,
MAP/MCE/MSM-BV-03-I: a, n, N, A
MAP/MCE/MSM-BV-04-I: a, A
MAP/MCE/MSM-BV-13-I: a, b, n, wait
- note: test spec allows any order, but PTS requires all connects first
MAP/MCE/MSM-BV-14-I: a, b, n, N, wait
- note: test spec allows any order, but PTS requires all connects first

MAP/MCE/MNR-BV-01-I: a, n, N
MAP/MCE/MNR-BV-02-I: a, n

MAP/MCE/MMB/BV-01-I: a, F, f
MAP/MCE/MMB/BV-02-I: a, p
MAP/MCE/MMB/BV-03-I: a, F
MAP/MCE/MMB/BV-04-I: a, 1, g
MAP/MCE/MMB/BV-17-I: a, 4, g
MAP/MCE/MMB/BV-06-I: a, 2, g
MAP/MCE/MMB/BV-08-I: a, U, F, A, ok
MAP/MCE/MMB/BV-08-I: a, C

MAP/MCE/MMB/BV-01-I: PASS a, f, 
MAP/MCE/MMB/BV-02-I: PASS a, p, 
MAP/MCE/MMB/BV-03-I: PASS a, F, 
MAP/MCE/MMB/BV-04-I: PASS a, p, F, 1, g, 
MAP/MCE/MMB/BV-17-I: PASS a, p, F, 4, g, 
MAP/MCE/MMB/BV-06-I: PASS a, p, F, 2, g, 
MAP/MCE/MMB/BV-07-I: PASS a, p, F, 1, r, 2, r, 4, r, 5, r, A, b, 3, r, B, 
MAP/MCE/MMB/BV-08-I: PASS a, P, I, F, A, 

MAP/MCE/MMD/BV-01-C: PASS a, p, F, 1, d, 2, d, 4, d, 5, d, A, b, 3, d, B
MAP/MCE/MMD/BV-03-C: PASS a, n, p, F, 5, d


WIP MAP/MCE/MMU/BV-01-C: a, p, 
MAP/MCE/MMU/BV-05-C: PASS a, p, F, 5, c
MAP/MCE/MMU/BV-06-C: PASS a, o

MAP/MCE/MMN/BV-05-C: a, n, m
MAP/MCE/MMN/BV-01-C: a, n
MAP/MCE/MMN/BV-05-C: a, n, m

MAP/MCE/MMI/BV-01-C: a, i

MAP/MCE/MFB/BV-01: 
MAP/MCE/MFB/BV-03: a
MAP/MCE/MFB/BV-04: a, n
MAP/MCE/MFB/BV-06: a

MAP/MCE/MMU/BV-01-C: TIMEOUT a, p, u - PTS receives our PUSH message (log) but ignores it and waits endless

MAP/MCE/GOEP/BC/BV-02-C: a, n
MAP/MCE/GOEP/BC/BV-04-C: a, f

MAP/MCE/GOEP/ROB/BV-01-C: a, n
MAP/MCE/GOEP/ROB/BV-02-C: a, n
MAP/MCE/GOEP/ROB/BV-01-C: a, n

MAP/MCE/GOEP/BC/BV-01-C: a

MAP/MCE/GOEP/SRM/BV-03-C: a, n
MAP/MCE/GOEP/SRM/BV-07-C: a, f

MAP/MCE/GOEP/SRMP/BV-01-C: a, n
MAP/MCE/GOEP/SRMP/BV-06-C: a, f
MAP/MCE/GOEP/SRMP/BI-01-C: a, f


Status:
TODO:
//ENABLE_GOEP_L2CAP: OBX connection fails

