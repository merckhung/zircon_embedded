// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{Immutable, IntoBytes, TryFromBytes};

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum Clock {
    Monotonic = 0,
    Utc = 1,
    Thread = 2,
}

impl Clock {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::Monotonic),

            1 => Some(Self::Utc),

            2 => Some(Self::Thread),

            _ => None,
        }
    }
}

impl From<Clock> for u32 {
    fn from(val: Clock) -> Self {
        val as Self
    }
}
