# SM Test

The tests for Cross-Transport Key-Generation require a Dual-Mode Controller with support for BR/EDR Secure Connections.
The need for BR/EDR SC is not listed in SM.TS.p19. CTKG works well without BR/EDR Secure Connections again iOS.

For unknown reasons, the Laird BT851 00:16:A4:4A:3C:C5 does not pick up advertisements from the Bluetooth 5.0 PTS 00:1B:DC:08:E2:xx dongles.
Current test device: the other Bluetooth 5.0 PTS dongle

# Sequences

- SM/CEN/SCCT/BV-03-C: 9, B, t, p, b, t
- SM/CEN/SCCT/BV-05-C: 9, B, t, p, b, t  
- SM/CEN/SCCT/BV-07-C: S, p, b, t, 9, t
- SM/CEN/SCCT/BV-09-C: S, p, b, t, 9, t
- SM/CEN/KDU/BI-01-C: { p, t }
- 
- SM/PER/SCCT/BV-04-C: C, a
- SM/PER/SCCT/BV-06-C: C, a
- SM/PER/SCCT/BV-08-C: S, C, a
- SM/PER/SCCT/BV-10-C: S, C, a
- SM/PER/KDU/BI-01-C: C, a
