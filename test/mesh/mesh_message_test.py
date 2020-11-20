#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2019

# pip3 install pycryptodomex

from mesh_crypto import *

try:
    from Cryptodome.Cipher import AES
    from Cryptodome.Hash import CMAC
except ImportError:
    # fallback: try to import PyCryptodome as (an almost drop-in) replacement for the PyCrypto library
    try:
        from Crypto.Cipher import AES
        from Crypto.Hash import CMAC
    except ImportError:
        print("\n[!] PyCryptodome required but not installed (using random value instead)")
        print("[!] Please install PyCryptodome, e.g. 'pip3 install pycryptodomex' or 'pip3 install pycryptodome'\n")
        
# S1('test') = b73cefbd641ef2ea598c2b6efb62f79c
s1_input  = b'test'
s1_actual = s1(s1_input)

s1_expected = bytes.fromhex('b73cefbd641ef2ea598c2b6efb62f79c')
if s1_actual != s1_expected:
    print("s1: expected " + s1_expected.hex + ", but got " + s1_actual.hex())

# K1(3216d1509884b533248541792b877f98, 2ba14ffa0df84a2831938d57d276cab4, 5a09d60797eeb4478aada59db3352a0d) = f6ed15a8934afbe7d83e8dcb57fcf5d7
k1_n        = bytes.fromhex("3216d1509884b533248541792b877f98")
k1_salt     = bytes.fromhex("2ba14ffa0df84a2831938d57d276cab4")
k1_p        = bytes.fromhex("5a09d60797eeb4478aada59db3352a0d")
k1_actual = k1(k1_n, k1_salt, k1_p)

k1_expected = bytes.fromhex("f6ed15a8934afbe7d83e8dcb57fcf5d7")
if k1_actual != k1_expected:
    print("k1: expected " + k1_expected.hex() + ", but got " + k1_actual.hex())

# k2 test
k2_n                       = bytes.fromhex('7dd7364cd842ad18c17c2b820c84c3d6')
k2_p                       = bytes.fromhex('00')
(k2_nid, k2_encryption_key, k2_privacy_key) = k2(k2_n, k2_p)

k2_nid_expected            = 0x68
k2_encryption_key_expected = bytes.fromhex('0953fa93e7caac9638f58820220a398e')
k2_privacy_key_expected    = bytes.fromhex('8b84eedec100067d670971dd2aa700cf')
if k2_nid_expected != k2_nid:
    print("k2: nid expected " + hex(k2_nid_expected) + ", but got " + hex(k2_nid))
if k2_encryption_key_expected != k2_encryption_key:
    print("k2: encryption key expected " + k2_encryption_key_expected.hex() + ", but got " + k2_encryption_key.hex())
if k2_privacy_key_expected != k2_privacy_key:
    print("k2: privacy key expected " + k2_privacy_key_expected.hex() + ", but got " + k2_privacy_key.hex())

# k3 test
k3_n = bytes.fromhex('f7a2a44f8e8a8029064f173ddc1e2b00')
k3_actual = k3(k3_n)

k3_expected = bytes.fromhex('ff046958233db014')
if k3_actual != k3_expected:
    print("k3: expected " + k3_expected.hex() + ", but got " + k3_actual.hex())

# k4 test
k4_n = bytes.fromhex('3216d1509884b533248541792b877f98')
k4_actual = k4(k4_n)

k4_expected = 0x38
if k4_actual != k4_expected:
    print("k4: expected " + hex(k4_expected) + ", but got " + hex(k4_actual))

# aes-ccm-encrypt test
message_1_network_nonce     = bytes.fromhex('00800000011201000012345678')
message_1_network_plaintext = bytes.fromhex('fffd' + '034b50057e400000010000')
message_1_encryption_key    = bytes.fromhex('0953fa93e7caac9638f58820220a398e')
(message_1_ciphertext, message_1_mic) = aes_ccm_encrypt(message_1_encryption_key, message_1_network_nonce, message_1_network_plaintext, b'', 8)

message_1_ciphertext_expected = bytes.fromhex('b5e5bfdacbaf6cb7fb6bff871f')
message_1_mic_expected        = bytes.fromhex('035444ce83a670df')
if message_1_ciphertext_expected != message_1_ciphertext:
    print("aes_ccm_encrypt: ciphertext expected " + hex(message_1_ciphertext_expected) + ", but got " + hex(message_1_ciphertext))
if message_1_mic_expected != message_1_mic:
    print("aes_ccm_encrypt: encryption key expected " + message_1_mic_expected.hex() + ", but got " + message_1_mic.hex())

# aes-ccm-decrypt test
message_1_network_nonce       = bytes.fromhex('00800000011201000012345678')
message_1_ciphertext_expected = bytes.fromhex('b5e5bfdacbaf6cb7fb6bff871f')
message_1_mic_expected        = bytes.fromhex('035444ce83a670df')
message_1_plaintext_decrypted = aes_ccm_decrypt(message_1_encryption_key, message_1_network_nonce, message_1_ciphertext_expected, b'', 8, message_1_mic_expected)

message_1_network_plaintext   = bytes.fromhex('fffd' + '034b50057e400000010000')
if message_1_plaintext_decrypted == None:
    print("aes_ccm_decrypt: digest validation failed")
elif message_1_network_plaintext != message_1_plaintext_decrypted:
    print("aes_ccm: plaintext expected " + hex(message_1_network_plaintext) + ", but got " + hex(message_1_plaintext_decrypted))

# aes128 encrypt
aes_key = bytes(16)
aes_n   = bytes(16)
aes_actual = aes128(aes_key, aes_n)

aes_expected = bytes.fromhex('66E94BD4EF8A2C3B884CFA59CA342B2E')
if aes_expected != aes_actual:
    print("aes128: expected " + aes_expected.hex() + ", but got " + aes_actual.hex())

# network PECB
iv_index = bytes.fromhex('12345678')
privacy_key = bytes.fromhex('8b84eedec100067d670971dd2aa700cf')
message_1_network_pdu = bytes.fromhex('68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df')
privacy_random = message_1_network_pdu[7:14]
message_1_pecb = network_pecb(privacy_random, iv_index, privacy_key)

message_1_pecb_expected = bytes.fromhex('6ca487507564')
if message_1_pecb_expected != message_1_pecb:
    print("network_pecb: expected " + message_1_pecb_expected.hex() + ", but got " + message_1_pecb.hex())

# message 1
iv_index                 = bytes.fromhex('12345678')
privacy_key              = bytes.fromhex('8b84eedec100067d670971dd2aa700cf')
message_1_network_pdu    = bytes.fromhex('68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df')
message_1_encryption_key = bytes.fromhex('0953fa93e7caac9638f58820220a398e')
message_1_decrypted      = network_decrypt(message_1_network_pdu, iv_index, message_1_encryption_key, privacy_key)

message_1_decrypted_expected = bytes.fromhex('68800000011201fffd034b50057e400000010000')
if message_1_decrypted_expected != message_1_decrypted:
    print("network_pdu: encrypt expected " + message_1_decrypted_expected.hex() + ", but got " + message_1_decrypted.hex())

# message x decrypt
iv_index                 = bytes.fromhex('00000001')
privacy_key              = bytes.fromhex('035efaafbbd84f898b95190cc3cac36b')
encryption_key           = bytes.fromhex('9ba46b86afbe8b7f0b63db597372babe')
message_1_network_pdu    = bytes.fromhex('B10000520D001C000155555555555555555555555555555555')
message_1_encrypted      = network_encrypt(message_1_network_pdu, iv_index, encryption_key, privacy_key)

message_1_expected       = bytes.fromhex('b12a7492504b03fa604d6d2f298ad31233fe1d568175b2f1104e23380b')
if message_1_expected != message_1_encrypted:
    print("network_pdu: decrypt expected " + message_1_expected.hex() + ", but got " + message_1_encrypted.hex())
