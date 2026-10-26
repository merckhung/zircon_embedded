// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes};

use crate::Signals;

pub const WAIT_MANY_MAX_ITEMS: usize = 64;

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct WaitItem {
    pub handle: Handle,
    pub waitfor: Signals,
    pub pending: Signals,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PowerDomainInfo {
    /// Mask of CPUs part of this power domain.
    pub cpus: [u64; 8],

    /// Id used to register this domain.
    pub domain_id: u32,

    /// Number of registered idle power levels.
    pub idle_power_levels: u8,

    /// Number of registered active power levels.
    pub active_power_levels: u8,
    pub padding1: [u8; 2],
}
