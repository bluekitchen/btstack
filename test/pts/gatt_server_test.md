# GATT Sever Test

CASE0071203:
- PTS 8.0.2 requests to connect via BR/EDR although GATT_1a_2 "GATT Client over BR/DER" is not selected ->
- Temp. workaround: enable GATT_2_1 "Attribute Protocol Support over BR/EDR" for GATT Server tests.

CASE0071217: TSE ID: 16914
- GATT/SR/GAS/BV-04-C, GATT/SR/GAS/BV-05-C,  GATT/SR/GAS/BV-06-C,  GATT/SR/GAS/BV-07-C
- Test Case Waiver can be requested by BQC

## TODO:
- Check if we should return an empty read blob response when reading at offset = len (value) - instead of returning an invalid offset error
  - "If the attribute value has a fixed length that is less than or equal to (ATT_MTU - 1) octets in length, then an ATT_ERROR_RSP PDU may be sent with the error code Attribute Not Long."
  - On "Read Blob returned an error", clock on 'cancel test' to let it continue

## IXIT:

## Test cases
- GATT/SR/GAC/BV-01: A (test repeats twice with MTU 23 and 512)
- GATT/SR/GAD/BV-01-C: A (PTS asks if we as server received the response, just OK)
- GATT/SR/GAD/BV-02-C: A (PTS asks if we as server received the response, just OK)
- GATT/SR/GAD/BV-03-C: A
- GATT/SR/GAD/BV-04-C: A (PTS asks if we as server received the response, just OK)
- GATT/SR/GAD/BV-05-C: A
- GATT/SR/GAD/BV-06-C: A
- GATT/SR/GAD/BV-07-C: (ok)
- GATT/SR/GAD/BV-08-C: (ok)

- GATT/SR/GAR/BV-01-C: (ok)
- GATT/SR/GAR/BI-01-C: (ok)
- GATT/SR/GAR/BI-02-C: "1111" (invalid handle)
- GATT/SR/GAR/BI-03-C: A (ok)
- GATT/SR/GAR/BI-04-C: A (press cancel test in pts to let it continue)
- GATT/SR/GAR/BI-05-C: A
- GATT/SR/GAR/BV-03-C: A
- GATT/SR/GAR/BI-06-C: A, "FFF2", "0013", "FFF2", "0013"
- GATT/SR/GAR/BI-07-C: A, "FFFF"
- GATT/SR/GAR/BI-08-C: A, 
- GATT/SR/GAR/BI-09-C: A, "FFF3", "0013"
- GATT/SR/GAR/BI-10-C: A, "FFF4", "0017"
- GATT/SR/GAR/BI-11-C: A, "FFF5", "0019"
- GATT/SR/GAR/BV-04-C: A
- GATT/SR/GAR/BI-12-C: A, "0013", " 0013"
- GATT/SR/GAR/BI-13-C: A
- GATT/SR/GAR/BI-14-C: A, "1111", "1111" (invalid handle)
- GATT/SR/GAR/BI-15-C: A
- GATT/SR/GAR/BI-16-C: A
- GATT/SR/GAR/BI-17-C: A
- GATT/SR/GAR/BV-05-C: A
- GATT/SR/GAR/BI-18-C: A, "0013","0013"
- GATT/SR/GAR/BI-19-C: A, "1111", "1111"
- GATT/SR/GAR/BI-20-C: A
- GATT/SR/GAR/BI-21-C: A
- GATT/SR/GAR/BI-22-C: A
- GATT/SR/GAR/BV-06-C: A
- GATT/SR/GAR/BV-07-C: A
- GATT/SR/GAR/BV-08-C: A
- GATT/SR/GAR/BI-34-C: A, "0021"
- GATT/SR/GAR/BI-35-C: A, "0023"

- GATT/SR/GAW/BV-01: A
- GATT/SR/GAW/BV-02: A, B
- GATT/SR/GAW/BI-01: A, B
- GATT/SR/GAW/BV-03: A 
- GATT/SR/GAW/BI-02: A, "1111", "1111"
- GATT/SR/GAW/BI-03: A
- GATT/SR/GAW/BI-04: A
- GATT/SR/GAW/BI-05: A
- GATT/SR/GAW/BI-06: A
- GATT/SR/GAW/BV-05: A, L
- GATT/SR/GAW/BI-07: A, "1111", "1111"
- GATT/SR/GAW/BI-08: A, "0011", "0011"
- GATT/SR/GAW/BI-09: A
- GATT/SR/GAW/BI-11: A
- GATT/SR/GAW/BI-12: A
- GATT/SR/GAW/BI-13: A
- GATT/SR/GAW/BV-06: A
- GATT/SR/GAW/BV-10: A, L
- GATT/SR/GAW/BV-11: A, L
- GATT/SR/GAW/BV-07: A, L
- GATT/SR/GAW/BV-08: A, L
- GATT/SR/GAW/BV-09: A
- GATT/SR/GAW/BI-32-C: A, "0009", "0009" (ATT_CHARACTERISTIC_GAP_APPEARANCE_01_VALUE_HANDLE)
- GATT/SR/GAW/BI-33-C: A, "0009", "0009" (ATT_CHARACTERISTIC_GAP_APPEARANCE_01_VALUE_HANDLE)

- GATT/SR/GAN/BV-01-C: A

- GATT/SR/GAI/BV-01-C: A

- GATT/SR/GAS/BV-02-C: n, N, A, N
- GATT/SR/GAS/BV-04-C: -- invalid: TSE ID: 16914
  GATT/SR/GAS/BV-05-C: -- invalid: TSE ID: 16914
- GATT/SR/GAS/BV-06-C: -- invalid: TSE ID: 16914
- GATT/SR/GAS/BV-07-C: -- invalid: TSE ID: 16914
  
- GATT/SR/GAT/BV-01-C: A (wait 30 seconds)

- GATT/SR/GPA/BV-11-C: A
  GATT/SR/GPA/BV-12-C: A

- GATT/SR/UNS/BI-01-C: AA
- GATT/SR/UNS/BI-02-C: A (PST waits rather long to check that no response is sent)
