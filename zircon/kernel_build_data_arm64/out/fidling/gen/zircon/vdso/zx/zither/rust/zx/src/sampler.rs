// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes};

use crate::Duration;

/// The act of taking a sample takes on the order of single digit microseconds.
/// A period close to or shorter than that doesn't make sense.
pub const SAMPLER_MIN_PERIOD: i64 = 10000;

pub const SAMPLER_MAX_BUFFER_SIZE: usize = 1073741824;

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct SamplerConfig {
    pub period: Duration,
    pub buffer_size: usize,
    pub discipline: u64,
}
