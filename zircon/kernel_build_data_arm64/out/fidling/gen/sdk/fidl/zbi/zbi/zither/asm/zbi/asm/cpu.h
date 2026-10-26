// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/cpu.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_CPU_H_
#define ZBI_ASM_CPU_H_

#define ZBI_MAX_SMT (4)

// The associated processor boots the system and is the last to be shutdown.

#define ZBI_TOPOLOGY_PROCESSOR_FLAGS_PRIMARY_SHIFT (0)
#define ZBI_TOPOLOGY_PROCESSOR_FLAGS_PRIMARY (1 << ZBI_TOPOLOGY_PROCESSOR_FLAGS_PRIMARY_SHIFT)

// The associated processor handles all interrupts. Some architectures
// will not have such a processor.

#define ZBI_TOPOLOGY_PROCESSOR_FLAGS_INTERRUPT_SHIFT (1)
#define ZBI_TOPOLOGY_PROCESSOR_FLAGS_INTERRUPT (1 << ZBI_TOPOLOGY_PROCESSOR_FLAGS_INTERRUPT_SHIFT)

#define ZBI_TOPOLOGY_ARM64_INFO_SIZEOF (5)

// Cluster ids for each level, one being closest to the cpu.
// These map to aff1, aff2, and aff3 values in the ARM registers.
#define ZBI_TOPOLOGY_ARM64_INFO_CLUSTER_1_ID (0)
#define ZBI_TOPOLOGY_ARM64_INFO_CLUSTER_2_ID (1)
#define ZBI_TOPOLOGY_ARM64_INFO_CLUSTER_3_ID (2)

// Id of the cpu inside of the bottom-most cluster, aff0 value.
#define ZBI_TOPOLOGY_ARM64_INFO_CPU_ID (3)

// The GIC interface number for this processor.
// In GIC v3+ this is not necessary as the processors are addressed by their
// affinity routing (all cluster ids followed by cpu_id).
#define ZBI_TOPOLOGY_ARM64_INFO_GIC_ID (4)

#define ZBI_TOPOLOGY_X64_INFO_SIZEOF (20)

#define ZBI_TOPOLOGY_X64_INFO_APIC_IDS (0)
#define ZBI_TOPOLOGY_X64_INFO_APIC_ID_COUNT (16)

#define ZBI_TOPOLOGY_RISCV64_INFO_SIZEOF (16)

// ID that represents this logical CPU (i.e., hart) in SBI.
#define ZBI_TOPOLOGY_RISCV64_INFO_HART_ID (0)

// Index into the ZBI_TYPE_RISCV64_ISA_STRTAB string table payload giving
// the start of the associated ISA string.
#define ZBI_TOPOLOGY_RISCV64_INFO_ISA_STRTAB_INDEX (8)
#define ZBI_TOPOLOGY_RISCV64_INFO_RESERVED (12)

#define ZBI_TOPOLOGY_ARCHITECTURE_INFO_SIZEOF (32)
#define ZBI_TOPOLOGY_ARCHITECTURE_INFO_DISCRIMINANT (0)
#define ZBI_TOPOLOGY_ARCHITECTURE_INFO_VALUE (8)

#define ZBI_TOPOLOGY_ARCHITECTURE_INFO_ARM64 (1)
#define ZBI_TOPOLOGY_ARCHITECTURE_INFO_X64 (2)
#define ZBI_TOPOLOGY_ARCHITECTURE_INFO_RISCV64 (3)

#define ZBI_TOPOLOGY_PROCESSOR_SIZEOF (48)

#define ZBI_TOPOLOGY_PROCESSOR_ARCHITECTURE_INFO (0)
#define ZBI_TOPOLOGY_PROCESSOR_FLAGS (32)
#define ZBI_TOPOLOGY_PROCESSOR_LOGICAL_IDS (34)
#define ZBI_TOPOLOGY_PROCESSOR_LOGICAL_ID_COUNT (42)

#define ZBI_TOPOLOGY_CLUSTER_SIZEOF (1)

// Relative performance level of this processor in the system. The value is
// interpreted as the performance of this processor relative to the maximum
// performance processor in the system. No specific values are required for
// the performance level, only that the following relationship holds:
//
//   Pmax is the value of performance_class for the maximum performance
//   processor in the system, operating at its maximum operating point.
//
//   P is the value of performance_class for this processor, operating at
//   its maximum operating point.
//
//   R is the performance ratio of this processor to the maximum performance
//   processor in the system in the range (0.0, 1.0].
//
//   R = (P + 1) / (Pmax + 1)
//
// If accuracy is limited, choose a conservative value that slightly under-
// estimates the performance of lower-performance processors.
#define ZBI_TOPOLOGY_CLUSTER_PERFORMANCE_CLASS (0)

#define ZBI_TOPOLOGY_CACHE_SIZEOF (4)

// Unique id of this cache node. No other semantics are assumed.
#define ZBI_TOPOLOGY_CACHE_CACHE_ID (0)

#define ZBI_TOPOLOGY_DIE_SIZEOF (8)

#define ZBI_TOPOLOGY_DIE_RESERVED (0)

#define ZBI_TOPOLOGY_SOCKET_SIZEOF (8)

#define ZBI_TOPOLOGY_SOCKET_RESERVED (0)

#define ZBI_TOPOLOGY_NUMA_REGION_SIZEOF (16)

// Starting memory addresses of the numa region.
#define ZBI_TOPOLOGY_NUMA_REGION_START (0)

// Size in bytes of the numa region.
#define ZBI_TOPOLOGY_NUMA_REGION_SIZE (8)

#define ZBI_TOPOLOGY_ENTITY_SIZEOF (56)
#define ZBI_TOPOLOGY_ENTITY_DISCRIMINANT (0)
#define ZBI_TOPOLOGY_ENTITY_VALUE (8)

#define ZBI_TOPOLOGY_ENTITY_PROCESSOR (1)
#define ZBI_TOPOLOGY_ENTITY_CLUSTER (2)
#define ZBI_TOPOLOGY_ENTITY_CACHE (3)
#define ZBI_TOPOLOGY_ENTITY_DIE (4)
#define ZBI_TOPOLOGY_ENTITY_SOCKET (5)
#define ZBI_TOPOLOGY_ENTITY_NUMA_REGION (6)

#define ZBI_TOPOLOGY_NO_PARENT (0xffff)

// The ZBI_TYPE_CPU_TOPOLOGY payload consists of an array of
// zbi_topology_node_t, giving a flattened tree-like description of the CPU
// configuration according to the entity hierarchy.
#define ZBI_TOPOLOGY_NODE_SIZEOF (64)

#define ZBI_TOPOLOGY_NODE_ENTITY (0)
#define ZBI_TOPOLOGY_NODE_PARENT_INDEX (56)

#endif  // ZBI_ASM_CPU_H_
