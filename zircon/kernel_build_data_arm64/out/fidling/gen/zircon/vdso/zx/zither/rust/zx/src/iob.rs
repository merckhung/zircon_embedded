// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes};

/// zx_iob_allocate_id() options.
#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct IobAllocateIdOptions(u32);

bitflags! {
    impl IobAllocateIdOptions : u32 {
  }
}

#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct IobWriteOptions(u64);

bitflags! {
    impl IobWriteOptions : u64 {
  }
}
