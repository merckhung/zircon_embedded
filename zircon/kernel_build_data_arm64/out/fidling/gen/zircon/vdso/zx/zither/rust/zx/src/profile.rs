// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{Immutable, IntoBytes, TryFromBytes};

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum ProfileInfoType {
    ZxProfileInfoScheduler = 1,
}

impl ProfileInfoType {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            1 => Some(Self::ZxProfileInfoScheduler),

            _ => None,
        }
    }
}

impl From<ProfileInfoType> for u32 {
    fn from(val: ProfileInfoType) -> Self {
        val as Self
    }
}

pub const ZX_PRIORITY_LOWEST: i32 = 0;

pub const ZX_PRIORITY_LOW: i32 = 8;

pub const ZX_PRIORITY_DEFAULT: i32 = 16;

pub const ZX_PRIORITY_HIGH: i32 = 24;

pub const ZX_PRIORITY_HIGHEST: i32 = 31;

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct ProfileInfo {
    pub r#type: ProfileInfoType,
}
