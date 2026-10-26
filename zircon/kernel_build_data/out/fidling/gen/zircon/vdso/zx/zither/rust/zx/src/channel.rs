// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use bitflags::bitflags;
use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

use crate::{ObjType, Rights, Status};

pub type HandleOp = u32;

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub struct HandleInfo {
    pub handle: Handle,
    pub r#type: ObjType,
    pub rights: Rights,
    pub unused: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct ChannelCallArgs {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub struct HandleDisposition {
    pub operation: HandleOp,
    pub handle: Handle,
    pub r#type: ObjType,
    pub rights: Rights,
    pub result: Status,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct ChannelCallEtcArgs {}
