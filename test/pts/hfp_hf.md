Tool: hfp_hf_test

HFP/HF/OOR/BV-01-I: f, B, A, (wait 30sec)
HFP/HF/OOR/BV-02-I: f, B, A, (wait 30sec), a, b

HFP/HF/TRS/BV-01-C: f

HFP/HF/PSI/BV-01-C: (confirm)
HFP/HF/PSI/BV-02-C: (confirm)
HFP/HF/PSI/BV-03-C: (confirm)
HFP/HF/PSI/BV-04-C: g, (confirm)

HFP/HF/ACS/BV-01-I: b 
HFP/HF/ACS/BV-03-I: (wait)
HFP/HF/ACS/BV-05-I: b
HFP/HF/ACS/BV-07-I: f
HFP/HF/ACS/BV-09-I: b
HFP/HF/ACS/BV-12-I: f
HFP/HF/ACS/BI-13-I: f (Secure Connections not enabled, BTstack will reject connection when Link Key with SC available)
HFP/HF/ACS/BV-15-I: (confirm)
? HFP/HF/ACS/BV-17-I: b (requires Controller with Secure Connection)

HFP/HF/ACR/BV-01-I: B
HFP/HF/ACR/BV-02-I: (wait)

HFP/HF/CLI/BV-01-I: L, f

HFP/HF/ICA/BV-01-I: f
HFP/HF/ICA/BV-02-I: f, F, f
HFP/HF/ICA/BV-03-I: o, f (ok if you see RING message)
HFP/HF/ICA/BV-04-I: f
HFP/HF/ICA/BV-05-I: f
HFP/HF/ICA/BV-06-I: (confirm)
HFP/HF/ICA/BV-17-I: a, f

HFP/HF/ICR/BV-01-I: G
HFP/HF/ICR/BV-02-I: (confirm)

HFP/HF/TCA/BV-01-I: f, F
HFP/HF/TCA/BV-02-I: f
HFP/HF/TCA/BV-03-I: f
HFP/HF/TCA/BV-04-I: i, F

HFP/HF/TDS/BV-01-I: (confirm)

HFP/HF/ATH/BV-03-I: a, b
HFP/HF/ATH/BV-04-I: f, b
HFP/HF/ATH/BV-05-I: (confirm)
HFP/HF/ATH/BV-06-I: f
HFP/HF/ATH/BV-09-I: (confirm) (the test is about HF power off/one)

HFP/HF/ATA/BV-01-I: f
HFP/HF/ATA/BV-02-I: f, A
HFP/HF/ATA/BV-03-I: f

HFP/HF/OCN/BV-01-I: i

HFP/HF/OCM/BV-01-I: j
HFP/HF/OCM/BV-02-I: J

HFP/HF/OCL/BV-01-I: W
HFP/HF/OCL/BV-02-I: W

HFP/HF/TWC/BV-01-I: f, u
HFP/HF/TWC/BV-02-I: f, U
HFP/HF/TWC/BV-03-I: f, v, v, U
HFP/HF/TWC/BV-04-I: f, v, V
HFP/HF/TWC/BV-05-I: f, W, U, I, U, j, U
HFP/HF/TWC/BV-06-I: f, v, w

HFP/HF/CIT/BV-01-I: (wait)

HFP/HF/ENO/BV-01-I: m, f

HFP/HF/VRA/BV-01-I: N, n    
HFP/HF/VRA/BV-02-I: (confirm)
HFP/HF/VRA/BV-03-I: N, n

HFP/HF/VRD/BV-01-I: N, n

HFP/HF/VTG/BV-01-I: x

HFP/HF/TDC/BV-01-I: f, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, *, #

HFP/HF/RSV/BV-01-I: f, o
HFP/HF/RSV/BV-02-I: f
HFP/HF/RSV/BV-03-I: f, f

HFP/HF/RMV/BV-01-I: f, q
HFP/HF/RMV/BV-02-I: f, 
HFP/HF/RMV/BV-03-I: f, f

HFP/HF/ECS/BV-01-I: a, X (PTS does not send phone number via CLCC, 1st on hold, 2nd active)
HFP/HF/ECS/BV-02-I: a, X (PTS does not send phone number via CLCC, 1st on hold, 2nd active)
HFP/HF/ECS/BV-03-I: f, X 

HFP/HF/ECC/BV-01-I: f, v, y
HFP/HF/ECC/BV-02-I: f, v, Y, U

HFP/HF/RHH/BV-01-I: a, X
HFP/HF/RHH/BV-02-I: ], X
HFP/HF/RHH/BV-03-I: X
HFP/HF/RHH/BV-04-I: {, X
HFP/HF/RHH/BV-05-I: X, X
HFP/HF/RHH/BV-06-I: X, }, X
HFP/HF/RHH/BV-07-I: X, X
HFP/HF/RHH/BV-08-I: X, X 

HFP/HF/NUM/BV-01-I: ? (use the `?` command to query caller subscriber number)
HFP/HF/NUM/BI-01-I: ? (use the `?` command to query caller subscriber number)

HFP/HF/SLC/BV-01-I: a
HFP/HF/SLC/BV-02-I: (wait)
HFP/HF/SLC/BV-03-I: a
HFP/HF/SLC/BV-04-I: (wait)
HFP/HF/SLC/BV-05-I: a
HFP/HF/SLC/BV-06-I: (wait)
HFP/HF/SLC/BV-08-I: (wait)
HFP/HF/SLC/BV-09-I: (wait)
HFP/HF/SLC/BV-10-I: (wait)

HFP/HF/ACC/BV-01-I: b
HFP/HF/ACC/BV-02-I: b
HFP/HF/ACC/BV-03-I: b
HFP/HF/ACC/BV-04-I: (wait)
HFP/HF/ACC/BV-05-I: (wait)
HFP/HF/ACC/BV-06-I: (wait)
HFP/HF/ACC/BV-07-I: (wait)

HFP/HF/WBS/BV-02-I: (wait)
HFP/HF/WBS/BV-03-I: (wait)

HFP/HF/DIS/BV-01-I: rm /tmp/btstack*.tlv, (wait)
HFP/HF/DIS/BV-02-I: rm /tmp/btstack*.tlv, (wait)

HFP/HF/SDP/BV-01-I: rm /tmp/btstack*.tlv, (wait 30sec), f
HFP/HF/SDP/BV-02-C: (wait)
HFP/HF/SDP/BV-03-C: rm /tmp/btstack*.tlv, (wait), a

HFP/HF/IIA/BV-04-I: D

HFP/HF/HFI/BV-01-I: !, !, !

HFP/HF/VRR/BV-01-I: R, r

HFP/HF/VTA/BV-01-I: R, R

HFP/HF/ATAH/BV-01-I: f, b, B

HFP/HF/OCA/BV-01-I: (wait)

IOPT/CL/HFP-HF/SFC/BV-14-I: rm /tmp/btstack*.tlv, a
