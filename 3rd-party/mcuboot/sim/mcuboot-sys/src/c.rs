// Copyright (c) 2017-2019 Linaro LTD
// Copyright (c) 2017-2019 JUUL Labs
// Copyright (c) 2019 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

//! Interface wrappers to C API entering to the bootloader

use crate::area::AreaDesc;
use simflash::SimMultiFlash;
use crate::api;

/// Invoke the bootloader on this flash device.
pub fn boot_go(multiflash: &mut SimMultiFlash, areadesc: &AreaDesc,
               counter: Option<&mut i32>, catch_asserts: bool) -> (i32, u8) {
    for (&dev_id, flash) in multiflash.iter_mut() {
        api::set_flash(dev_id, flash);
    }
    let mut sim_ctx = api::CSimContext {
        flash_counter: match counter {
            None => 0,
            Some(ref c) => **c as libc::c_int
        },
        jumped: 0,
        c_asserts: 0,
        c_catch_asserts: if catch_asserts { 1 } else { 0 },
        boot_jmpbuf: [0; 16],
    };
    let result = unsafe {
        raw::invoke_boot_go(&mut sim_ctx as *mut _, &areadesc.get_c() as *const _) as i32
    };
    let asserts = sim_ctx.c_asserts;
    if let Some(c) = counter {
        *c = sim_ctx.flash_counter;
    }
    for &dev_id in multiflash.keys() {
        api::clear_flash(dev_id);
    }
    (result, asserts)
}

pub fn boot_trailer_sz(align: u32) -> u32 {
    unsafe { raw::boot_trailer_sz(align) }
}

pub fn boot_status_sz(align: u32) -> u32 {
    unsafe { raw::boot_status_sz(align) }
}

pub fn boot_magic_sz() -> usize {
    unsafe { raw::boot_magic_sz() as usize }
}

pub fn boot_max_align() -> usize {
    unsafe { raw::boot_max_align() as usize }
}

pub fn rsa_oaep_encrypt(pubkey: &[u8], seckey: &[u8]) -> Result<[u8; 256], &'static str> {
    unsafe {
        let mut encbuf: [u8; 256] = [0; 256];
        if raw::rsa_oaep_encrypt_(pubkey.as_ptr(), pubkey.len() as u32,
                                  seckey.as_ptr(), seckey.len() as u32,
                                  encbuf.as_mut_ptr()) == 0 {
            return Ok(encbuf);
        }
        Err("Failed to encrypt buffer")
    }
}

pub fn kw_encrypt(kek: &[u8], seckey: &[u8]) -> Result<[u8; 24], &'static str> {
    unsafe {
        let mut encbuf = [0u8; 24];
        if raw::kw_encrypt_(kek.as_ptr(), seckey.as_ptr(), encbuf.as_mut_ptr()) == 0 {
            return Ok(encbuf);
        }
        Err("Failed to encrypt buffer")
    }
}

mod raw {
    use crate::area::CAreaDesc;
    use crate::api::CSimContext;

    extern "C" {
        // This generates a warning about `CAreaDesc` not being foreign safe.  There doesn't appear to
        // be any way to get rid of this warning.  See https://github.com/rust-lang/rust/issues/34798
        // for information and tracking.
        pub fn invoke_boot_go(sim_ctx: *mut CSimContext, areadesc: *const CAreaDesc) -> libc::c_int;

        pub fn boot_trailer_sz(min_write_sz: u32) -> u32;
        pub fn boot_status_sz(min_write_sz: u32) -> u32;

        pub fn boot_magic_sz() -> u32;
        pub fn boot_max_align() -> u32;

        pub fn rsa_oaep_encrypt_(pubkey: *const u8, pubkey_len: libc::c_uint,
                                 seckey: *const u8, seckey_len: libc::c_uint,
                                 encbuf: *mut u8) -> libc::c_int;

        pub fn kw_encrypt_(kek: *const u8, seckey: *const u8,
                           encbuf: *mut u8) -> libc::c_int;
    }
}
