// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes};

pub const BOARD_NAME_LEN: u64 = 32;

/// ZBI_TYPE_PLATFORM_ID payload.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PlatformId {
    pub vid: u32,
    pub pid: u32,
    pub board_name: [u8; 32],
}

/// ZBI_TYPE_DRV_BOARD_INFO payload.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct BoardInfo {
    pub revision: u32,
}
