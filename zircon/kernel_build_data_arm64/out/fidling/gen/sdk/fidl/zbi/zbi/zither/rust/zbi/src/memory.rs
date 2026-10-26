// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

/// Unknown values should be treated as RESERVED to allow forwards compatibility.
#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum MemType {
    /// Standard RAM.
    Ram = 1,

    /// Device memory.
    Peripheral = 2,

    /// Represents memory that should not be used by the system. Reserved ranges may
    /// overlap other RAM or PERIPHERAL regions, in which case the reserved range
    /// should take precedence.
    Reserved = 3,
}

impl MemType {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            1 => Some(Self::Ram),

            2 => Some(Self::Peripheral),

            3 => Some(Self::Reserved),

            _ => None,
        }
    }
}

impl From<MemType> for u32 {
    fn from(val: MemType) -> Self {
        val as Self
    }
}

/// The ZBI_TYPE_MEM_CONFIG payload consist of one or more `zbi_mem_range_t`
/// entries.
///
/// The length of the item is `sizeof(zbi_mem_range_t)` times the number of
/// entries. Each entry describes a contiguous range of memory
///
/// Entries in the table may be in any order, and only a single item of type
/// ZBI_TYPE_MEM_CONFIG should be present in the ZBI.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub struct MemRange {
    pub paddr: u64,
    pub length: u64,
    pub r#type: MemType,
    pub reserved: u32,
}

/// ZBI_TYPE_NVRAM payload.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct Nvram {
    pub base: u64,
    pub length: u64,
}
