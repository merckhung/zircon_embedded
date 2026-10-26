// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use bitflags::bitflags;
use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

pub const MAX_SMT: u64 = 4;

#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct TopologyProcessorFlags(u16);

bitflags! {
    impl TopologyProcessorFlags : u16 {

        /// The associated processor boots the system and is the last to be shutdown.
        const PRIMARY = 1 << 0;

        /// The associated processor handles all interrupts. Some architectures
        /// will not have such a processor.
        const INTERRUPT = 1 << 1;
  }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyArm64Info {
    /// Cluster ids for each level, one being closest to the cpu.
    /// These map to aff1, aff2, and aff3 values in the ARM registers.
    pub cluster_1_id: u8,
    pub cluster_2_id: u8,
    pub cluster_3_id: u8,

    /// Id of the cpu inside of the bottom-most cluster, aff0 value.
    pub cpu_id: u8,

    /// The GIC interface number for this processor.
    /// In GIC v3+ this is not necessary as the processors are addressed by their
    /// affinity routing (all cluster ids followed by cpu_id).
    pub gic_id: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyX64Info {
    pub apic_ids: [u32; 4],
    pub apic_id_count: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyRiscv64Info {
    /// ID that represents this logical CPU (i.e., hart) in SBI.
    pub hart_id: u64,

    /// Index into the ZBI_TYPE_RISCV64_ISA_STRTAB string table payload giving
    /// the start of the associated ISA string.
    pub isa_strtab_index: u32,
    pub reserved: u32,
}

#[repr(u64)]
#[derive(Clone, Copy, Immutable, TryFromBytes)]
pub enum TopologyArchitectureInfo {
    Arm64(TopologyArm64Info) = 1,
    X64(TopologyX64Info) = 2,
    Riscv64(TopologyRiscv64Info) = 3,
}

#[repr(C)]
#[derive(Clone, Copy, Immutable, TryFromBytes)]
pub struct TopologyProcessor {
    pub architecture_info: TopologyArchitectureInfo,
    pub flags: TopologyProcessorFlags,
    pub logical_ids: [u16; 4],
    pub logical_id_count: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyCluster {
    /// Relative performance level of this processor in the system. The value is
    /// interpreted as the performance of this processor relative to the maximum
    /// performance processor in the system. No specific values are required for
    /// the performance level, only that the following relationship holds:
    ///
    ///   Pmax is the value of performance_class for the maximum performance
    ///   processor in the system, operating at its maximum operating point.
    ///
    ///   P is the value of performance_class for this processor, operating at
    ///   its maximum operating point.
    ///
    ///   R is the performance ratio of this processor to the maximum performance
    ///   processor in the system in the range (0.0, 1.0].
    ///
    ///   R = (P + 1) / (Pmax + 1)
    ///
    /// If accuracy is limited, choose a conservative value that slightly under-
    /// estimates the performance of lower-performance processors.
    pub performance_class: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyCache {
    /// Unique id of this cache node. No other semantics are assumed.
    pub cache_id: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyDie {
    pub reserved: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologySocket {
    pub reserved: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct TopologyNumaRegion {
    /// Starting memory addresses of the numa region.
    pub start: u64,

    /// Size in bytes of the numa region.
    pub size: u64,
}

#[repr(u64)]
#[derive(Clone, Copy, Immutable, TryFromBytes)]
pub enum TopologyEntity {
    Processor(TopologyProcessor) = 1,
    Cluster(TopologyCluster) = 2,
    Cache(TopologyCache) = 3,
    Die(TopologyDie) = 4,
    Socket(TopologySocket) = 5,
    NumaRegion(TopologyNumaRegion) = 6,
}

pub const TOPOLOGY_NO_PARENT: u16 = 0xffff;

/// The ZBI_TYPE_CPU_TOPOLOGY payload consists of an array of
/// zbi_topology_node_t, giving a flattened tree-like description of the CPU
/// configuration according to the entity hierarchy.
#[repr(C)]
#[derive(Clone, Copy, Immutable, TryFromBytes)]
pub struct TopologyNode {
    pub entity: TopologyEntity,
    pub parent_index: u16,
}
