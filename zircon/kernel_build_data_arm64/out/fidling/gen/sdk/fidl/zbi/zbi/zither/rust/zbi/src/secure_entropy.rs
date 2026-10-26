// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use zerocopy::{Immutable, IntoBytes, TryFromBytes};

/// ZBI_TYPE_SECURE_ENTROPY item subtypes (for zbi_header_t.extra)
#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum SecureEntropy {
    /// Contents are used to seed the kernel's PRNG.
    General = 0,

    /// Contents are used by early boot, before the kernel is fully
    /// operational.
    EarlyBoot = 1,
}

impl SecureEntropy {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::General),

            1 => Some(Self::EarlyBoot),

            _ => None,
        }
    }
}

impl From<SecureEntropy> for u32 {
    fn from(val: SecureEntropy) -> Self {
        val as Self
    }
}
