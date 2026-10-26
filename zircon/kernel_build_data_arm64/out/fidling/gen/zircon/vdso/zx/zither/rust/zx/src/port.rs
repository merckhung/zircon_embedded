// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

use crate::{Gpaddr, Signals, Status, Time};

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PacketSignal {
    pub trigger: Signals,
    pub observed: Signals,
    pub count: u64,
    pub timestamp: Time,
    pub reserved1: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PacketException {
    pub pid: u64,
    pub tid: u64,
    pub reserved0: u64,
    pub reserved1: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PacketGuestBell {
    pub addr: Gpaddr,
    pub reserved0: u64,
    pub reserved1: u64,
    pub reserved2: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PacketGuestMem {
    pub addr: Gpaddr,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct PacketGuestIo {
    pub port: u16,
    pub access_size: u8,
    pub input: bool,
    pub reserved0: u64,
    pub reserved1: u64,
    pub reserved2: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct PacketGuestVcpu {
    pub r#type: u8,
    pub reserved: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PacketInterrupt {
    pub timestamp: Time,
    pub reserved0: u64,
    pub reserved1: u64,
    pub reserved2: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct PacketPageRequest {
    pub command: u16,
    pub flags: u16,
    pub reserved0: u32,
    pub offset: u64,
    pub length: u64,
    pub reserved1: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct PortPacket {
    pub key: u64,
    pub r#type: u32,
    pub status: Status,
    pub signal: PacketSignal,
    pub exception: PacketException,
    pub guest_bell: PacketGuestBell,
    pub guest_mem: PacketGuestMem,
    pub guest_io: PacketGuestIo,
    pub guest_vcpu: PacketGuestVcpu,
    pub interrupt: PacketInterrupt,
    pub page_request: PacketPageRequest,
}
