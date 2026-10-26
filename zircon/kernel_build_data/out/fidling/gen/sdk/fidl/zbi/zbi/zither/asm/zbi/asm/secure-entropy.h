// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/secure-entropy.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_SECURE_ENTROPY_H_
#define ZBI_ASM_SECURE_ENTROPY_H_

// ZBI_TYPE_SECURE_ENTROPY item subtypes (for zbi_header_t.extra)

// Contents are used to seed the kernel's PRNG.
#define ZBI_SECURE_ENTROPY_GENERAL (0)

// Contents are used by early boot, before the kernel is fully
// operational.
#define ZBI_SECURE_ENTROPY_EARLY_BOOT (1)

#endif  // ZBI_ASM_SECURE_ENTROPY_H_
