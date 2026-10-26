// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/memory.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_MEMORY_H_
#define ZBI_ASM_MEMORY_H_

// Unknown values should be treated as RESERVED to allow forwards compatibility.

// Standard RAM.
#define ZBI_MEM_TYPE_RAM (1)

// Device memory.
#define ZBI_MEM_TYPE_PERIPHERAL (2)

// Represents memory that should not be used by the system. Reserved ranges may
// overlap other RAM or PERIPHERAL regions, in which case the reserved range
// should take precedence.
#define ZBI_MEM_TYPE_RESERVED (3)

// The ZBI_TYPE_MEM_CONFIG payload consist of one or more `zbi_mem_range_t`
// entries.
//
// The length of the item is `sizeof(zbi_mem_range_t)` times the number of
// entries. Each entry describes a contiguous range of memory
//
// Entries in the table may be in any order, and only a single item of type
// ZBI_TYPE_MEM_CONFIG should be present in the ZBI.
#define ZBI_MEM_RANGE_SIZEOF (24)

#define ZBI_MEM_RANGE_PADDR (0)
#define ZBI_MEM_RANGE_LENGTH (8)
#define ZBI_MEM_RANGE_TYPE (16)
#define ZBI_MEM_RANGE_RESERVED (20)

// ZBI_TYPE_NVRAM payload.
#define ZBI_NVRAM_SIZEOF (16)

#define ZBI_NVRAM_BASE (0)
#define ZBI_NVRAM_LENGTH (8)

#endif  // ZBI_ASM_MEMORY_H_
