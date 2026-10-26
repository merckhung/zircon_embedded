// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use zerocopy::{Immutable, IntoBytes, TryFromBytes};

/// ZBI_TYPE_HW_REBOOT_REASON payload.
#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum HwRebootReason {
    Undefined = 0,
    Cold = 1,
    Warm = 2,
    Brownout = 3,
    Watchdog = 4,
    UserHardReset = 5,
}

impl HwRebootReason {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::Undefined),

            1 => Some(Self::Cold),

            2 => Some(Self::Warm),

            3 => Some(Self::Brownout),

            4 => Some(Self::Watchdog),

            5 => Some(Self::UserHardReset),

            _ => None,
        }
    }
}

impl From<HwRebootReason> for u32 {
    fn from(val: HwRebootReason) -> Self {
        val as Self
    }
}
