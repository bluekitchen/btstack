#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2019

# pip3 install pycryptodomex

from Cryptodome.Cipher import AES
from Cryptodome.Hash   import CMAC

def aes_cmac(key, n):
    cobj = CMAC.new(key, ciphermod=AES)
    cobj.update(n)
    return cobj.digest()

db_message = bytes.fromhex('010000280018020003280a0300002a04000328020500012a06000028011807000328200800052a090002290a0003280a0b00292b0c000328020d002a2b0e00002808180f000228140016000f1810000328a21100182a12000229130000290000140001280f1815000328021600192a');
db_hash_expected = bytes.fromhex('F1CA2D48ECF58BAC8A8830BBB9FBA990')
db_hash_actual = aes_cmac(bytes(16), db_message);
if db_hash_actual != db_hash_expected:
    print("Expected: " + db_hash_actual.hex())
    print("Actual:   " + db_hash_actual.hex())
