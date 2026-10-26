// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

use bitflags::bitflags;
use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

use crate::{InstantBoot, Koid};

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct SystemPowerctlArg {}

/// Options which can be passed to `zx_system_suspend_enter` syscalls via the
/// `options` parameter.
#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct SystemSuspendOption(u64);

bitflags! {
    impl SystemSuspendOption : u64 {

        /// `ZX_SYSTEM_SUSPEND_OPTION_DISCARD` is an option which may be passed to
        /// `zx_system_suspend_enter` which will cause the syscall to discard any wake
        /// sources report entries for wake sources which have been both triggered and
        /// acknowledged, and are only waiting to be reported.  This operation takes
        /// place before any attempt to suspend is made, and before any report is
        /// generated.
        const DISCARD = 1 << 0;

        /// `ZX_SYSTEM_SUSPEND_OPTION_REPORT_ONLY` is an option which may be passed to
        /// `zx_system_suspend_enter` which will cause the syscall to skip any attempts to
        /// suspend.  Instead, it will only generate a report as it unwinds.
        const REPORT_ONLY = 1 << 1;
  }
}

/// Flags which can be reported via the `flags` member of the
/// `zx_wake_support_entry_t` structure.
#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct SystemWakeReportEntryFlag(u32);

bitflags! {
    impl SystemWakeReportEntryFlag : u32 {

        /// A `zx_wake_source_report_entry_t` flag indicating that the entry's wake
        /// source was still in the signaled (not-yet-acknowledged) state at the
        /// time of report generation.
        const SIGNALED = 1 << 0;

        /// A `zx_wake_source_report_entry_t` flag indicating that this entry has
        /// been reported before, but is being reported again because the wake
        /// source is still in the signaled state, and has not been acknowledged
        /// since being previously reported.
        const PREVIOUSLY_REPORTED = 1 << 1;
  }
}

/// A structure returned from calls to `zx_system_suspend_enter` containing
/// details about an individual wake source which may have been involved in
/// coming out of suspend.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct WakeSourceReportEntry {
    /// The KOID uniquely identifying the kernel wake-source object being
    /// reported.
    pub koid: Koid,

    /// The name of the object to assist in debugging.  This field is for
    /// debugging only, it is not durable and should not be used in production
    /// logic.
    pub name: [u8; 32],

    /// The time (on the boot timeline) at which this wake source first became
    /// signaled.
    pub initial_signal_time: InstantBoot,

    /// The time (on the boot timeline) at which this wake source most recently
    /// became signaled.  Will be equal to the initial_signal_time if the wake
    /// source has only been signaled once.
    pub last_signal_time: InstantBoot,

    /// The time (on the boot timeline) at which this wake source most recently
    /// became acknowledged, if ever.  If not, this will be ZX_TIME_INFINITE.
    pub last_ack_time: InstantBoot,

    /// The total number of times that this wakes source has been signaled
    /// before being reported.
    pub signal_count: u32,

    /// Any valid combination of the `ZX_SYSTEM_WAKE_REPORT_ENTRY_FLAG`s.
    pub flags: SystemWakeReportEntryFlag,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct WakeSourceReportHeader {
    /// The timestamp of when the wake report was generated.
    pub report_time: InstantBoot,

    /// The timestamp of when the suspend operation was committed to and
    /// started, or ZX_TIME_INFINITE if no attempt to suspend was made.
    pub suspend_start_time: InstantBoot,

    /// The total number of wake sources present in the system at the time of
    /// report generation.
    pub total_wake_sources: u32,

    /// The total number of report entries which were waiting to be reported at
    /// the start of report generation, but were skipped due to insufficient
    /// space in the user's buffer.
    pub unreported_wake_report_entries: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct StringView {
    pub c_str: *const u8,
    pub length: usize,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum CpuPerfLimitType {
    /// Represents a limit on the processing rate of the CPU. Values for min and
    /// max are interpreted to be in the range [0, MAX_RATE], where MAX_RATE is
    /// the value of the processing_rate field of the fastest power level in the
    /// same power domain. Values above MAX_RATE are clamped to MAX_RATE.
    Rate = 0,

    /// Represents a limit on the effective power of the CPU. Values for min and
    /// max are in units of nanowatts.
    Power = 1,
}

impl CpuPerfLimitType {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::Rate),

            1 => Some(Self::Power),

            _ => None,
        }
    }
}

impl From<CpuPerfLimitType> for u32 {
    fn from(val: CpuPerfLimitType) -> Self {
        val as Self
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub struct CpuPerfLimit {
    pub logical_cpu_number: u32,
    pub limit_type: CpuPerfLimitType,
    pub min: u64,
    pub max: u64,
}
