// Copyright (c) 2017-2020 Linaro LTD
// Copyright (c) 2017-2020 JUUL Labs
//
// SPDX-License-Identifier: Apache-2.0

//! TLV Support
//!
//! mcuboot images are followed immediately by a list of TLV items that contain integrity
//! information about the image.  Their generation is made a little complicated because the size of
//! the TLV block is in the image header, which is included in the hash.  Since some signatures can
//! vary in size, we just make them the largest size possible.
//!
//! Because of this header, we have to make two passes.  The first pass will compute the size of
//! the TLV, and the second pass will build the data for the TLV.

use byteorder::{
    LittleEndian, WriteBytesExt,
};
use crate::image::ImageVersion;
use log::info;
use ring::{digest, rand, agreement, hkdf, hmac};
use ring::rand::SecureRandom;
use ring::signature::{
    RsaKeyPair,
    RSA_PSS_SHA256,
    EcdsaKeyPair,
    ECDSA_P256_SHA256_ASN1_SIGNING,
    Ed25519KeyPair,
};
use aes_ctr::{
    Aes128Ctr,
    stream_cipher::{
        generic_array::GenericArray,
        NewStreamCipher,
        SyncStreamCipher,
    },
};
use mcuboot_sys::c;

#[repr(u16)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(dead_code)] // TODO: For now
pub enum TlvKinds {
    KEYHASH = 0x01,
    SHA256 = 0x10,
    RSA2048 = 0x20,
    ECDSA224 = 0x21,
    ECDSA256 = 0x22,
    RSA3072 = 0x23,
    ED25519 = 0x24,
    ENCRSA2048 = 0x30,
    ENCKW128 = 0x31,
    ENCEC256 = 0x32,
    ENCX25519 = 0x33,
    DEPENDENCY = 0x40,
}

#[allow(dead_code, non_camel_case_types)]
pub enum TlvFlags {
    PIC = 0x01,
    NON_BOOTABLE = 0x02,
    ENCRYPTED = 0x04,
    RAM_LOAD = 0x20,
}

/// A generator for manifests.  The format of the manifest can be either a
/// traditional "TLV" or a SUIT-style manifest.
pub trait ManifestGen {
    /// Retrieve the header magic value for this manifest type.
    fn get_magic(&self) -> u32;

    /// Retrieve the flags value for this particular manifest type.
    fn get_flags(&self) -> u32;

    /// Retrieve the number of bytes of this manifest that is "protected".
    /// This field is stored in the outside image header instead of the
    /// manifest header.
    fn protect_size(&self) -> u16;

    /// Add a dependency on another image.
    fn add_dependency(&mut self, id: u8, version: &ImageVersion);

    /// Add a sequence of bytes to the payload that the manifest is
    /// protecting.
    fn add_bytes(&mut self, bytes: &[u8]);

    /// Set an internal flag indicating that the next `make_tlv` should
    /// corrupt the signature.
    fn corrupt_sig(&mut self);

    /// Construct the manifest for this payload.
    fn make_tlv(self: Box<Self>) -> Vec<u8>;

    /// Generate a new encryption random key
    fn generate_enc_key(&mut self);

    /// Return the current encryption key
    fn get_enc_key(&self) -> Vec<u8>;
}

#[derive(Debug, Default)]
pub struct TlvGen {
    flags: u32,
    kinds: Vec<TlvKinds>,
    payload: Vec<u8>,
    dependencies: Vec<Dependency>,
    enc_key: Vec<u8>,
    /// Should this signature be corrupted.
    gen_corrupted: bool,
}

#[derive(Debug)]
struct Dependency {
    id: u8,
    version: ImageVersion,
}

const AES_KEY_LEN: usize = 16;

impl TlvGen {
    /// Construct a new tlv generator that will only contain a hash of the data.
    #[allow(dead_code)]
    pub fn new_hash_only() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa_pss() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA2048],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa3072_pss() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA3072],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecdsa() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::ECDSA256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ed25519() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::ED25519],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_enc_rsa() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCRSA2048],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_sig_enc_rsa() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA2048, TlvKinds::ENCRSA2048],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_enc_kw() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCKW128],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa_kw() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA2048, TlvKinds::ENCKW128],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecdsa_kw() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ECDSA256, TlvKinds::ENCKW128],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecies_p256() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCEC256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecdsa_ecies_p256() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ECDSA256, TlvKinds::ENCEC256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecies_x25519() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCX25519],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ed25519_ecies_x25519() -> TlvGen {
        TlvGen {
            flags: TlvFlags::ENCRYPTED as u32,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ED25519, TlvKinds::ENCX25519],
            ..Default::default()
        }
    }
}

impl ManifestGen for TlvGen {
    fn get_magic(&self) -> u32 {
        0x96f3b83d
    }

    /// Retrieve the header flags for this configuration.  This can be called at any time.
    fn get_flags(&self) -> u32 {
        self.flags
    }

    /// Add bytes to the covered hash.
    fn add_bytes(&mut self, bytes: &[u8]) {
        self.payload.extend_from_slice(bytes);
    }

    fn protect_size(&self) -> u16 {
        if self.dependencies.is_empty() {
            0
        } else {
            // Include the header and space for each dependency.
            4 + (self.dependencies.len() as u16) * (4 + 4 + 8)
        }
    }

    fn add_dependency(&mut self, id: u8, version: &ImageVersion) {
        self.dependencies.push(Dependency {
            id,
            version: version.clone(),
        });
    }

    fn corrupt_sig(&mut self) {
        self.gen_corrupted = true;
    }

    /// Compute the TLV given the specified block of data.
    fn make_tlv(self: Box<Self>) -> Vec<u8> {
        let mut protected_tlv: Vec<u8> = vec![];

        if self.protect_size() > 0 {
            protected_tlv.push(0x08);
            protected_tlv.push(0x69);
            let size = self.protect_size();
            protected_tlv.write_u16::<LittleEndian>(size).unwrap();
            for dep in &self.dependencies {
                protected_tlv.write_u16::<LittleEndian>(TlvKinds::DEPENDENCY as u16).unwrap();
                protected_tlv.write_u16::<LittleEndian>(12).unwrap();

                // The dependency.
                protected_tlv.push(dep.id);
                protected_tlv.push(0);
                protected_tlv.write_u16::<LittleEndian>(0).unwrap();
                protected_tlv.push(dep.version.major);
                protected_tlv.push(dep.version.minor);
                protected_tlv.write_u16::<LittleEndian>(dep.version.revision).unwrap();
                protected_tlv.write_u32::<LittleEndian>(dep.version.build_num).unwrap();
            }

            assert_eq!(size, protected_tlv.len() as u16, "protected TLV length incorrect");
        }

        // Ring does the signature itself, which means that it must be
        // given a full, contiguous payload.  Although this does help from
        // a correct usage perspective, it is fairly stupid from an
        // efficiency view.  If this is shown to be a performance issue
        // with the tests, the protected data could be appended to the
        // payload, and then removed after the signature is done.  For now,
        // just make a signed payload.
        let mut sig_payload = self.payload.clone();
        sig_payload.extend_from_slice(&protected_tlv);

        let mut result: Vec<u8> = vec![];

        // add back signed payload
        result.extend_from_slice(&protected_tlv);

        // add non-protected payload
        let npro_pos = result.len();
        result.push(0x07);
        result.push(0x69);
        // Placeholder for the size.
        result.write_u16::<LittleEndian>(0).unwrap();

        if self.kinds.contains(&TlvKinds::SHA256) {
            // If a signature is not requested, corrupt the hash we are
            // generating.  But, if there is a signature, output the
            // correct hash.  We want the hash test to pass so that the
            // signature verification can be validated.
            let mut corrupt_hash = self.gen_corrupted;
            for k in &[TlvKinds::RSA2048, TlvKinds::RSA3072,
                TlvKinds::ECDSA224, TlvKinds::ECDSA256,
                TlvKinds::ED25519]
            {
                if self.kinds.contains(k) {
                    corrupt_hash = false;
                    break;
                }
            }

            if corrupt_hash {
                sig_payload[0] ^= 1;
            }

            let hash = digest::digest(&digest::SHA256, &sig_payload);
            let hash = hash.as_ref();

            assert!(hash.len() == 32);
            result.write_u16::<LittleEndian>(TlvKinds::SHA256 as u16).unwrap();
            result.write_u16::<LittleEndian>(32).unwrap();
            result.extend_from_slice(hash);

            // Undo the corruption.
            if corrupt_hash {
                sig_payload[0] ^= 1;
            }

        }

        if self.gen_corrupted {
            // Corrupt what is signed by modifying the input to the
            // signature code.
            sig_payload[0] ^= 1;
        }

        if self.kinds.contains(&TlvKinds::RSA2048) ||
            self.kinds.contains(&TlvKinds::RSA3072) {

            let is_rsa2048 = self.kinds.contains(&TlvKinds::RSA2048);

            // Output the hash of the public key.
            let hash = if is_rsa2048 {
                digest::digest(&digest::SHA256, RSA_PUB_KEY)
            } else {
                digest::digest(&digest::SHA256, RSA3072_PUB_KEY)
            };
            let hash = hash.as_ref();

            assert!(hash.len() == 32);
            result.write_u16::<LittleEndian>(TlvKinds::KEYHASH as u16).unwrap();
            result.write_u16::<LittleEndian>(32).unwrap();
            result.extend_from_slice(hash);

            // For now assume PSS.
            let key_bytes = if is_rsa2048 {
                pem::parse(include_bytes!("../../root-rsa-2048.pem").as_ref()).unwrap()
            } else {
                pem::parse(include_bytes!("../../root-rsa-3072.pem").as_ref()).unwrap()
            };
            assert_eq!(key_bytes.tag, "RSA PRIVATE KEY");
            let key_pair = RsaKeyPair::from_der(&key_bytes.contents).unwrap();
            let rng = rand::SystemRandom::new();
            let mut signature = vec![0; key_pair.public_modulus_len()];
            if is_rsa2048 {
                assert_eq!(signature.len(), 256);
            } else {
                assert_eq!(signature.len(), 384);
            }
            key_pair.sign(&RSA_PSS_SHA256, &rng, &sig_payload, &mut signature).unwrap();

            if is_rsa2048 {
                result.write_u16::<LittleEndian>(TlvKinds::RSA2048 as u16).unwrap();
            } else {
                result.write_u16::<LittleEndian>(TlvKinds::RSA3072 as u16).unwrap();
            }
            result.write_u16::<LittleEndian>(signature.len() as u16).unwrap();
            result.extend_from_slice(&signature);
        }

        if self.kinds.contains(&TlvKinds::ECDSA256) {
            let keyhash = digest::digest(&digest::SHA256, ECDSA256_PUB_KEY);
            let keyhash = keyhash.as_ref();

            assert!(keyhash.len() == 32);
            result.write_u16::<LittleEndian>(TlvKinds::KEYHASH as u16).unwrap();
            result.write_u16::<LittleEndian>(32).unwrap();
            result.extend_from_slice(keyhash);

            let key_bytes = pem::parse(include_bytes!("../../root-ec-p256-pkcs8.pem").as_ref()).unwrap();
            assert_eq!(key_bytes.tag, "PRIVATE KEY");

            let key_pair = EcdsaKeyPair::from_pkcs8(&ECDSA_P256_SHA256_ASN1_SIGNING,
                                                    &key_bytes.contents).unwrap();
            let rng = rand::SystemRandom::new();
            let signature = key_pair.sign(&rng, &sig_payload).unwrap();

            result.write_u16::<LittleEndian>(TlvKinds::ECDSA256 as u16).unwrap();

            let signature = signature.as_ref().to_vec();

            result.write_u16::<LittleEndian>(signature.len() as u16).unwrap();
            result.extend_from_slice(signature.as_ref());
        }

        if self.kinds.contains(&TlvKinds::ED25519) {
            let keyhash = digest::digest(&digest::SHA256, ED25519_PUB_KEY);
            let keyhash = keyhash.as_ref();

            assert!(keyhash.len() == 32);
            result.write_u16::<LittleEndian>(TlvKinds::KEYHASH as u16).unwrap();
            result.write_u16::<LittleEndian>(32).unwrap();
            result.extend_from_slice(keyhash);

            let hash = digest::digest(&digest::SHA256, &sig_payload);
            let hash = hash.as_ref();
            assert!(hash.len() == 32);

            let key_bytes = pem::parse(include_bytes!("../../root-ed25519.pem").as_ref()).unwrap();
            assert_eq!(key_bytes.tag, "PRIVATE KEY");

            let key_pair = Ed25519KeyPair::from_seed_and_public_key(
                &key_bytes.contents[16..48], &ED25519_PUB_KEY[12..44]).unwrap();
            let signature = key_pair.sign(&hash);

            result.write_u16::<LittleEndian>(TlvKinds::ED25519 as u16).unwrap();

            let signature = signature.as_ref().to_vec();
            result.write_u16::<LittleEndian>(signature.len() as u16).unwrap();
            result.extend_from_slice(signature.as_ref());
        }

        if self.kinds.contains(&TlvKinds::ENCRSA2048) {
            let key_bytes = pem::parse(include_bytes!("../../enc-rsa2048-pub.pem")
                                       .as_ref()).unwrap();
            assert_eq!(key_bytes.tag, "PUBLIC KEY");

            let cipherkey = self.get_enc_key();
            let cipherkey = cipherkey.as_slice();
            let encbuf = match c::rsa_oaep_encrypt(&key_bytes.contents, cipherkey) {
                Ok(v) => v,
                Err(_) => panic!("Failed to encrypt secret key"),
            };

            assert!(encbuf.len() == 256);
            result.write_u16::<LittleEndian>(TlvKinds::ENCRSA2048 as u16).unwrap();
            result.write_u16::<LittleEndian>(256).unwrap();
            result.extend_from_slice(&encbuf);
        }

        if self.kinds.contains(&TlvKinds::ENCKW128) {
            let key_bytes = base64::decode(
                include_str!("../../enc-aes128kw.b64").trim()).unwrap();

            let cipherkey = self.get_enc_key();
            let cipherkey = cipherkey.as_slice();
            let encbuf = match c::kw_encrypt(&key_bytes, cipherkey) {
                Ok(v) => v,
                Err(_) => panic!("Failed to encrypt secret key"),
            };

            assert!(encbuf.len() == 24);
            result.write_u16::<LittleEndian>(TlvKinds::ENCKW128 as u16).unwrap();
            result.write_u16::<LittleEndian>(24).unwrap();
            result.extend_from_slice(&encbuf);
        }

        if self.kinds.contains(&TlvKinds::ENCEC256) || self.kinds.contains(&TlvKinds::ENCX25519) {
            let key_bytes = if self.kinds.contains(&TlvKinds::ENCEC256) {
                pem::parse(include_bytes!("../../enc-ec256-pub.pem").as_ref()).unwrap()
            } else {
                pem::parse(include_bytes!("../../enc-x25519-pub.pem").as_ref()).unwrap()
            };
            assert_eq!(key_bytes.tag, "PUBLIC KEY");

            let rng = rand::SystemRandom::new();
            let alg = if self.kinds.contains(&TlvKinds::ENCEC256) {
                &agreement::ECDH_P256
            } else {
                &agreement::X25519
            };
            let pk = match agreement::EphemeralPrivateKey::generate(alg, &rng) {
                Ok(v) => v,
                Err(_) => panic!("Failed to generate ephemeral keypair"),
            };

            let pubk = match pk.compute_public_key() {
                Ok(pubk) => pubk,
                Err(_) => panic!("Failed computing ephemeral public key"),
            };

            let peer_pubk = if self.kinds.contains(&TlvKinds::ENCEC256) {
                agreement::UnparsedPublicKey::new(&agreement::ECDH_P256, &key_bytes.contents[26..])
            } else {
                agreement::UnparsedPublicKey::new(&agreement::X25519, &key_bytes.contents[12..])
            };

            #[derive(Debug, PartialEq)]
            struct OkmLen<T: core::fmt::Debug + PartialEq>(T);

            impl hkdf::KeyType for OkmLen<usize> {
                fn len(&self) -> usize {
                    self.0
                }
            }

            let derived_key = match agreement::agree_ephemeral(
                pk, &peer_pubk, ring::error::Unspecified, |shared| {
                    let salt = hkdf::Salt::new(hkdf::HKDF_SHA256, &[]);
                    let prk = salt.extract(&shared);
                    let okm = match prk.expand(&[b"MCUBoot_ECIES_v1"], OkmLen(48)) {
                        Ok(okm) => okm,
                        Err(_) => panic!("Failed building HKDF OKM"),
                    };
                    let mut buf = [0u8; 48];
                    match okm.fill(&mut buf) {
                        Ok(_) => Ok(buf),
                        Err(_) => panic!("Failed generating HKDF output"),
                    }
                },
            ) {
                Ok(v) => v,
                Err(_) => panic!("Failed building HKDF"),
            };

            let key = GenericArray::from_slice(&derived_key[..16]);
            let nonce = GenericArray::from_slice(&[0; 16]);
            let mut cipher = Aes128Ctr::new(&key, &nonce);
            let mut cipherkey = self.get_enc_key();
            cipher.apply_keystream(&mut cipherkey);

            let key = hmac::Key::new(hmac::HMAC_SHA256, &derived_key[16..]);
            let tag = hmac::sign(&key, &cipherkey);

            let mut buf = vec![];
            buf.append(&mut pubk.as_ref().to_vec());
            buf.append(&mut tag.as_ref().to_vec());
            buf.append(&mut cipherkey);

            if self.kinds.contains(&TlvKinds::ENCEC256) {
                assert!(buf.len() == 113);
                result.write_u16::<LittleEndian>(TlvKinds::ENCEC256 as u16).unwrap();
                result.write_u16::<LittleEndian>(113).unwrap();
            } else {
                assert!(buf.len() == 80);
                result.write_u16::<LittleEndian>(TlvKinds::ENCX25519 as u16).unwrap();
                result.write_u16::<LittleEndian>(80).unwrap();
            }
            result.extend_from_slice(&buf);
        }

        // Patch the size back into the TLV header.
        let size = (result.len() - npro_pos) as u16;
        let mut size_buf = &mut result[npro_pos + 2 .. npro_pos + 4];
        size_buf.write_u16::<LittleEndian>(size).unwrap();

        result
    }

    fn generate_enc_key(&mut self) {
        let rng = rand::SystemRandom::new();
        let mut buf = vec![0u8; AES_KEY_LEN];
        if rng.fill(&mut buf).is_err() {
            panic!("Error generating encrypted key");
        }
        info!("New encryption key: {:02x?}", buf);
        self.enc_key = buf;
    }

    fn get_enc_key(&self) -> Vec<u8> {
        if self.enc_key.len() != AES_KEY_LEN {
            panic!("No random key was generated");
        }
        self.enc_key.clone()
    }
}

include!("rsa_pub_key-rs.txt");
include!("rsa3072_pub_key-rs.txt");
include!("ecdsa_pub_key-rs.txt");
include!("ed25519_pub_key-rs.txt");
