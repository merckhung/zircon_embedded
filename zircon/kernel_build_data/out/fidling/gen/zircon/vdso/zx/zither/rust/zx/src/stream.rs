// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{Immutable, IntoBytes, TryFromBytes};

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum StreamSeekOrigin {
    Start = 0,
    Current = 1,
    End = 2,
}

impl StreamSeekOrigin {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::Start),

            1 => Some(Self::Current),

            2 => Some(Self::End),

            _ => None,
        }
    }
}

impl From<StreamSeekOrigin> for u32 {
    fn from(val: StreamSeekOrigin) -> Self {
        val as Self
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct Iovec {
    pub buffer: *const u8,
    pub capacity: usize,
}
