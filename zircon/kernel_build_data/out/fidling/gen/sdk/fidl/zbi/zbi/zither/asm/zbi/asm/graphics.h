// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/graphics.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_GRAPHICS_H_
#define ZBI_ASM_GRAPHICS_H_

// Gives a pixel format representation.
//
// Bits [23:16] (i.e., the third byte) encode the number of bytes per pixel
// in the representation.

#define ZBI_PIXEL_FORMAT_NONE (0x00000000)
#define ZBI_PIXEL_FORMAT_RGB_565 (0x00020001)
#define ZBI_PIXEL_FORMAT_RGB_332 (0x00010002)
#define ZBI_PIXEL_FORMAT_RGB_2220 (0x00010003)
#define ZBI_PIXEL_FORMAT_ARGB_8888 (0x00040004)
#define ZBI_PIXEL_FORMAT_RGB_X888 (0x00040005)
#define ZBI_PIXEL_FORMAT_MONO_8 (0x00010007)
#define ZBI_PIXEL_FORMAT_NV12 (0x00010008)
#define ZBI_PIXEL_FORMAT_I420 (0x00010009)
#define ZBI_PIXEL_FORMAT_RGB_888 (0x00030009)
#define ZBI_PIXEL_FORMAT_ABGR_8888 (0x0004000a)
#define ZBI_PIXEL_FORMAT_BGR_888_X (0x0004000b)
#define ZBI_PIXEL_FORMAT_ARGB_2_10_10_10 (0x0004000c)
#define ZBI_PIXEL_FORMAT_ABGR_2_10_10_10 (0x0004000d)

// ZBI_TYPE_FRAMEBUFFER payload.
#define ZBI_SWFB_SIZEOF (24)

// Physical memory address.
#define ZBI_SWFB_BASE (0)

// Pixel layout and format.
#define ZBI_SWFB_WIDTH (8)
#define ZBI_SWFB_HEIGHT (12)
#define ZBI_SWFB_STRIDE (16)
#define ZBI_SWFB_FORMAT (20)

#endif  // ZBI_ASM_GRAPHICS_H_
