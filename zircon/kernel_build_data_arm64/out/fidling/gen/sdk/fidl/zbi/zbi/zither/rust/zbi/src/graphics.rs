// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use zerocopy::{Immutable, IntoBytes, TryFromBytes};

/// Gives a pixel format representation.
///
/// Bits [23:16] (i.e., the third byte) encode the number of bytes per pixel
/// in the representation.
#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum PixelFormat {
    None = 0x00000000,
    Rgb565 = 0x00020001,
    Rgb332 = 0x00010002,
    Rgb2220 = 0x00010003,
    Argb8888 = 0x00040004,
    RgbX888 = 0x00040005,
    Mono8 = 0x00010007,
    Nv12 = 0x00010008,
    I420 = 0x00010009,
    Rgb888 = 0x00030009,
    Abgr8888 = 0x0004000a,
    Bgr888X = 0x0004000b,
    Argb2101010 = 0x0004000c,
    Abgr2101010 = 0x0004000d,
}

impl PixelFormat {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0x00000000 => Some(Self::None),

            0x00020001 => Some(Self::Rgb565),

            0x00010002 => Some(Self::Rgb332),

            0x00010003 => Some(Self::Rgb2220),

            0x00040004 => Some(Self::Argb8888),

            0x00040005 => Some(Self::RgbX888),

            0x00010007 => Some(Self::Mono8),

            0x00010008 => Some(Self::Nv12),

            0x00010009 => Some(Self::I420),

            0x00030009 => Some(Self::Rgb888),

            0x0004000a => Some(Self::Abgr8888),

            0x0004000b => Some(Self::Bgr888X),

            0x0004000c => Some(Self::Argb2101010),

            0x0004000d => Some(Self::Abgr2101010),

            _ => None,
        }
    }
}

impl From<PixelFormat> for u32 {
    fn from(val: PixelFormat) -> Self {
        val as Self
    }
}

/// ZBI_TYPE_FRAMEBUFFER payload.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub struct Swfb {
    /// Physical memory address.
    pub base: u64,

    /// Pixel layout and format.
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub format: PixelFormat,
}
