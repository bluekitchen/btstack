# PAN NAP Tests

## Tool
pan_nap_test

## Sequences
- PAN/NAP/BNEP/BROADCAST-0/BV-01-C: yes, ok, ok, ok
- PAN/NAP/BNEP/BROADCAST-1/BV-02-C: b
- PAN/NAP/BNEP/MULTICAST-0/BV-03-C: yes, ok, ok, ok
- PAN/NAP/BNEP/MULTICAST-0/BV-04-C: g
- PAN/NAP/BNEP/FORWARD-UNICAST/BV-05-C: yes, ok, ok, ok
- PAN/NAP/BNEP/FORWARD-UNICAST/BV-06-C: e
- PAN/NAP/IP/LLMNR/BV-01-I: 7
- PAN/NAP/IP/LLMNR/BV-02-I: 8
- PAN/NAP/IP/APP/BV-03-I: 1
- PAN/NAP/IP/APP/BV-05-I: wait
- PAN/NAP/MISC/UUID/BV-01-C: wait
- PAN/NAP/MISC/UUID/BV-02-C: wait
- PAN/NAP/SDP/BV-01-C: wait

## For Later
- PAN NAP: tests with more than one PANU
    - PAN/NAP/BNEP/FORWARD/BV-08-C
    - PAN/NAP/BNEP/FORWARD-BROADCAST/BV-09-C
    - PAN/NAP/BNEP/EXTENSION-0/BV-07-C
    - PAN/NAP/BNEP/FILTER/BV-10-C
    - PAN/NAP/BNEP/FILTER/BV-11-C
    - PAN/NAP/BNEP/FILTER/BV-12-C
    - PAN/NAP/BNEP/FILTER/BV-13-C
    - PAN/NAP/BNEP/FILTER/BV-14-C
    - PAN/NAP/BNEP/FILTER/BV-10-C
- PAN NAP: not supported yet, optional features:
    - DNS resolution
        - PAN/NAP/IP/DNS/BV-01-I
    - Basic Data Transmission / HTTP Request -- maybe with lwIP
        - PAN/NAP/IP/APP/BV-01-I
        - PAN/NAP/IP/APP/BV-02-I