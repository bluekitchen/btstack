# GAP Test Sequences

## Setup
Use BL654 dongles on PTS and IUT
Set TSPX_bd_addr_iut for GAP Profile in IXIT

## Tests
- GAP/PADV/PASM/BV-01-C: 1
- GAP/PADV/PAM/BV-01-C: 1
- GAP/PADV/PASE/BV-01-C: 2
- GAP/PADV/PASE/BV-02-C: 3
- GAP/PADV/PASE/BV-03-C: a, 2
- GAP/PADV/PASE/BV-04-C: a, 3
- GAP/PADV/PASE/BV-05-C: c, 2
- GAP/PADV/PASE/BV-06-C: c, 3
- GAP/PADV/PAST/BV-01-C: a, 1, 4
- GAP/PADV/PASE/BV-02-C: c, 1, 4

- GAP/CONN/DCON/BV-01-C: X, a
- GAP/CONN/DCON/BV-02-C: a, a, X, a
- GAP/CONN/DCON/BV-03-C: a, R, a, wait
- 
- GAP/CONN/UCON/BV-01-C: c, ok, X
- GAP/CONN/UCON/BV-02-C: d, a
- GAP/CONN/UCON/BV-03-C: a
- GAP/CONN/UCON/BV-06-C: a, X, R, a - wait for uRPA update, X

- GAP/CONN/ACEP/* -> PTS/gap_central_test

- GAP/BIS/BSE/BV-01-C: 3, 
- GAP/BIS/BBM/BV-01-C: 1, b

- GAP/SEC/SEM/BV-32-C: 2,
- GAP/SEC/SEM/BV-33-C: B, 2 
- GAP/SEC/SEM/BV-34-C: 1, b
- GAP/SEC/SEM/BV-35-C: 1, B, b
- GAP/SEC/SEM/BI-13-C: B, 2 -- expect 'reject BIGInfo' on stdout