// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

#![no_std]

// The following `use ... as _;` declarations satisfy any unused-crate-dependencies
// errors for unconditionally depended-on crates.
use bitflags as _;
use zerocopy as _;

mod board;
pub use board::*;

mod cpu;
pub use cpu::*;

mod driver_config;
pub use driver_config::*;

mod graphics;
pub use graphics::*;

mod kernel;
pub use kernel::*;

mod memory;
pub use memory::*;

mod partition;
pub use partition::*;

mod reboot;
pub use reboot::*;

mod secure_entropy;
pub use secure_entropy::*;

mod zbi;
pub use zbi::*;
