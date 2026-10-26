// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes};

pub const PARTITION_NAME_LEN: u64 = 32;

pub const PARTITION_GUID_LEN: u64 = 16;

pub type PartitionGuid = [u8; 16];

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct Partition {
    /// GUID specifying the format and use of data stored in the partition.
    pub type_guid: PartitionGuid,

    /// GUID unique to this partition.
    pub uniq_guid: PartitionGuid,

    /// First and last block occupied by this partition.
    pub first_block: u64,
    pub last_block: u64,

    /// Reserved for future use.  Set to 0.
    pub flags: u64,
    pub name: [u8; 32],
}

/// ZBI_TYPE_DRV_PARTITION_MAP payload. This header is immediately followed by
/// an array of the corresponding zbi_partition_t.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PartitionMap {
    /// Total blocks used on the device.
    pub block_count: u64,

    /// Size of each block in bytes.
    pub block_size: u64,

    /// Number of partitions in the map.
    pub partition_count: u32,

    /// Reserved for future use.
    pub reserved: u32,

    /// Device GUID.
    pub guid: PartitionGuid,
}
