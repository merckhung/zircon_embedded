// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/board.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_BOARD_H_
#define ZBI_ASM_BOARD_H_

#define ZBI_BOARD_NAME_LEN (32)

// ZBI_TYPE_PLATFORM_ID payload.
#define ZBI_PLATFORM_ID_SIZEOF (40)

#define ZBI_PLATFORM_ID_VID (0)
#define ZBI_PLATFORM_ID_PID (4)
#define ZBI_PLATFORM_ID_BOARD_NAME (8)

// ZBI_TYPE_DRV_BOARD_INFO payload.
#define ZBI_BOARD_INFO_SIZEOF (4)

#define ZBI_BOARD_INFO_REVISION (0)

#endif  // ZBI_ASM_BOARD_H_
