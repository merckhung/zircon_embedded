// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/partition.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_PARTITION_H_
#define ZBI_ASM_PARTITION_H_

#define ZBI_PARTITION_NAME_LEN (32)

#define ZBI_PARTITION_GUID_LEN (16)

#define ZBI_PARTITION_SIZEOF (88)

// GUID specifying the format and use of data stored in the partition.
#define ZBI_PARTITION_TYPE_GUID (0)

// GUID unique to this partition.
#define ZBI_PARTITION_UNIQ_GUID (16)

// First and last block occupied by this partition.
#define ZBI_PARTITION_FIRST_BLOCK (32)
#define ZBI_PARTITION_LAST_BLOCK (40)

// Reserved for future use.  Set to 0.
#define ZBI_PARTITION_FLAGS (48)
#define ZBI_PARTITION_NAME (56)

// ZBI_TYPE_DRV_PARTITION_MAP payload. This header is immediately followed by
// an array of the corresponding zbi_partition_t.
#define ZBI_PARTITION_MAP_SIZEOF (40)

// Total blocks used on the device.
#define ZBI_PARTITION_MAP_BLOCK_COUNT (0)

// Size of each block in bytes.
#define ZBI_PARTITION_MAP_BLOCK_SIZE (8)

// Number of partitions in the map.
#define ZBI_PARTITION_MAP_PARTITION_COUNT (16)

// Reserved for future use.
#define ZBI_PARTITION_MAP_RESERVED (20)

// Device GUID.
#define ZBI_PARTITION_MAP_GUID (24)

#endif  // ZBI_ASM_PARTITION_H_
