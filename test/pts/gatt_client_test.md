# GATT Client Test

Tool: gatt_client_test

CASE0071203:
- PTS 8.0.2 requests to connect via BR/EDR although GATT_1a_2 "GATT Client over BR/DER" is not selected -> 
- Temp. workaround: disable GATT_2_1 "Attribute Protocol Support over BR/EDR" for GATT Client LE-only tests.

# Sequences

- GATT/CL/GAC/BV-01-C: p, A, E, HANDLE, OFFSET, Fake DATA, t

- GATT/CL/GAC/BV-01-C: { p,  e, t }
- GATT/CL/GAC/BV-02-C: { p,  f, UUID16, t } or { p, F, UUID128, t}
- GATT/CL/GAC/BV-03-C: { p, i, t }
- GATT/CL/GAC/BV-04-C: { p, g, UUID16, t }
- GATT/CL/GAC/BV-05-C: { p, G, FROM_HANDLE, TO_HANDLE, t }
- GATT/CL/GAC/BV-06-C: { p, h, HANDLE, t }

- GATT/CL/GAR/BV-01-C: p, j, HANDLE, t
- GATT/CL/GAR/BI-01-C: p, j, HANDLE, t
- GATT/CL/GAR/BI-02-C: p, j, HANDLE, t
- GATT/CL/GAR/BI-03-C: p, j, HANDLE, t
- GATT/CL/GAR/BI-04-C: p, j, HANDLE, t
- GATT/CL/GAR/BI-04-C: (delete bonding information), p, j, HANDLE, t
- GATT/CL/GAR/BV-03-C: p, k, UUID16, 1, K, UUID128, 1, t
- GATT/CL/GAR/BI-06-C: p, k, UUID16, t
- GATT/CL/GAR/BI-07-C: p, k, UUID16, t
- GATT/CL/GAR/BI-08-C: p, k, UUID16, t
- GATT/CL/GAR/BI-09-C: p, k, UUID16, t
- GATT/CL/GAR/BI-10-C: p, k, UUID16, t
- GATT/CL/GAR/BI-11-C: (delete bonding information), p, k, UUID16, t
- GATT/CL/GAR/BV-04-C: p, { J, HANDLE, 0 }, t
- GATT/CL/GAR/BI-12-C: p, J, HANDLE, 0, t
- GATT/CL/GAR/BI-13-C: p, J, HANDLE, OFFSET (larger than shown), t
- GATT/CL/GAR/BI-14-C: p, J, HANDLE, 0, t
- GATT/CL/GAR/BI-15-C: p, J, HANDLE, 0, t
- GATT/CL/GAR/BI-16-C: p, J, HANDLE, 0, t
- GATT/CL/GAR/BI-17-C: p, J, HANDLE, 0, t
- GATT/CL/GAR/BV-05-C: p, N, HANDLE-1, HANDLE-2, 0, t
- GATT/CL/GAR/BI-18-C: p, N, HANDLE-1, HANDLE-2, 0, t
- GATT/CL/GAR/BI-19-C: p, N, HANDLE-1, HANDLE-2, 0, t
- GATT/CL/GAR/BI-20-C: p, N, HANDLE-1, HANDLE-2, 0, t
- GATT/CL/GAR/BI-21-C: p, N, HANDLE-1, HANDLE-2, 0, t
- GATT/CL/GAR/BI-22-C: (delete bonding information), p, N, HANDLE-1, HANDLE-2, 0, t
- GATT/CL/GAR/BV-06-C: p, j, HANDLE, t
- GATT/CL/GAR/BV-07-C: p, { J, HANDLE, }, t

- GATT/CL/GAW/BV-01-C: p, O, HANDLE, DATA, t
- GATT/CL/GAW/BV-02-C: p, R, HANDLE, DATA, t
- GATT/CL/GAW/BV-03-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BI-02-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BI-03-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BI-04-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BI-05-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BI-06-C: (delete bonding information), p, q, HANDLE, DATA, t
- GATT/CL/GAW/BV-05-C: p, Q, HANDLE, DATA, t
- GATT/CL/GAW/BI-06-C: p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BI-07-C: p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BI-08-C: p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BI-09-C: p, Q, HANDLE, OFFSET (larger than shown), t
- GATT/CL/GAW/BI-10-C: p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BI-11-C: p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BI-12-C: p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BI-13-C: (delete bonding information), p, Q, HANDLE, 0, DATA, t
- GATT/CL/GAW/BV-06-C: p, Q, HANDLE, DATA, t
- GATT/CL/GAW/BV-08-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BV-09-C: p, Q, HANDLE, DATA, t
- GATT/CL/GAW/BI-32-C: p, E, HANDLE, OFFSET, DATA, V, t
- GATT/CL/GAW/BI-33-C: p, q, HANDLE, DATA, t
- GATT/CL/GAW/BI-34-C: p, Q, HANDLE, 11 22 33 44 55 66 77 88 99 00 11 22 33 44 55 66 77 88 99 00 11 22 33 44, t

- GATT/CL/GAN/BV-01-C: p, q, HANDLE, 01 00, t

- GATT/CL/GAI/BV-01-C: p, q, HANDLE, 02 00, t

- GATT/CL/GAS/BV-01-C: p, q, HANDLE, 02 00, t
- GATT/CL/GAS/BV-02-C: p, k, 2B2A, 1, t

- GATT/CL/GAT/BV-01-C: p, j, HANDLE, wait
- GATT/CL/GAT/BV-02-C: p, q, HANDLE, DATA, wait

