// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use bitflags::bitflags;
use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

pub const ALIGNMENT: u32 = 8;

/// Numeric prefix for kernel types.
///
/// 'KRN\0'
pub const TYPE_KERNEL_PREFIX: u32 = 0x004e524b;

/// Mask to compare against TYPE_KERNEL_PREFIX.
pub const TYPE_KERNEL_MASK: u32 = 0x00ffffff;

/// Numeric prefix for driver metadata types.
///
/// 'm\0\0\0'
pub const TYPE_DRIVER_METADATA_PREFIX: u32 = 0x0000006d;

/// Mask to compare against TYPE_DRIVER_METADATA_PREFIX.
pub const TYPE_DRIVER_METADATA_MASK: u32 = 0x000000ff;

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum Type {
    /// 'BOOT'
    ///
    /// Each ZBI starts with a container header.
    ///     length:          Total size of the image after this header.
    ///                      This includes all item headers, payloads, and padding.
    ///                      It does not include the container header itself.
    ///                      Must be a multiple of ZBI_ALIGNMENT.
    ///     extra:           Must be ZBI_CONTAINER_MAGIC.
    ///     flags:           Must be ZBI_FLAGS_VERSION and no other flags.
    Container = 0x544f4f42,

    /// x86-64 kernel. See zbi_kernel_t for a payload description.
    ///
    /// 'KRNL'
    KernelX64 = 1280201291, // 0x4c000000 | TYPE_KERNEL_PREFIX

    /// ARM64 kernel. See zbi_kernel_t for a payload description.
    ///
    /// KRN8
    KernelArm64 = 944656971, // 0x38000000 | TYPE_KERNEL_PREFIX

    /// RISC-V kernel. See zbi_kernel_t for a payload description.
    ///
    /// 'KRNV'
    KernelRiscv64 = 1447973451, // 0x56000000 | TYPE_KERNEL_PREFIX

    /// A discarded item that should just be ignored.  This is used for an
    /// item that was already processed and should be ignored by whatever
    /// stage is now looking at the ZBI.  An earlier stage already "consumed"
    /// this information, but avoided copying data around to remove it from
    /// the ZBI item stream.
    ///
    /// 'SKIP'
    Discard = 0x50494b53,

    /// A virtual disk image.  This is meant to be treated as if it were a
    /// storage device.  The payload (after decompression) is the contents of
    /// the storage device, in whatever format that might be.
    ///
    /// 'RDSK'
    StorageRamdisk = 0x4b534452,

    /// The /boot filesystem in BOOTFS format, specified in
    /// <lib/zbi-format/internal/bootfs.h>.  This represents an internal
    /// contract between Zircon userboot (//docs/userboot.md), which handles
    /// the contents of this filesystem, and platform tooling, which prepares
    /// them.
    ///
    /// 'BFSB'
    StorageBootfs = 0x42534642,

    /// Storage used by the kernel (such as a compressed image containing the
    /// actual kernel).  The meaning and format of the data is specific to the
    /// kernel, though it always uses the standard (private) storage
    /// compression protocol. Each particular KERNEL_{ARCH} item image and its
    /// STORAGE_KERNEL item image are intimately tied and one cannot work
    /// without the exact correct corresponding other.
    ///
    /// 'KSTR'
    StorageKernel = 0x5254534b,

    /// Device-specific factory data, stored in BOOTFS format.
    ///
    /// TODO(https://fxbug.dev/42109921): This should not use the "STORAGE" infix.
    ///
    /// 'BFSF'
    StorageBootfsFactory = 0x46534642,

    /// A kernel command line fragment, a UTF-8 string that need not be
    /// NUL-terminated.  The kernel's own option parsing accepts only printable
    /// 'ASCI'I and treats all other characters as equivalent to whitespace. Multiple
    /// ZBI_TYPE_CMDLINE items can appear.  They are treated as if concatenated with
    /// ' ' between each item, in the order they appear: first items in the bootable
    /// ZBI containing the kernel; then items in the ZBI synthesized by the boot
    /// loader.  The kernel interprets the [whole command line](../../../../docs/kernel_cmdline.md).
    ///
    /// 'CMDL'
    Cmdline = 0x4c444d43,

    /// The crash log from the previous boot, a UTF-8 string.
    ///
    /// 'BOOM'
    Crashlog = 0x4d4f4f42,

    /// Physical memory region that will persist across warm boots. See zbi_nvram_t
    /// for payload description.
    ///
    /// 'NVLL'
    Nvram = 0x4c4c564e,

    /// Platform ID Information.
    ///
    /// 'PLID'
    PlatformId = 0x44494c50,

    /// Board-specific information.
    ///
    /// mBSI
    DrvBoardInfo = 1230193261, // 0x49534200 | TYPE_DRIVER_METADATA_PREFIX

    /// CPU configuration. See zbi_topology_node_t for a description of the payload.
    CpuTopology = 0x43505533,

    /// Device memory configuration. See zbi_mem_range_t for a description of the
    /// payload.
    ///
    /// 'MEMC'
    MemConfig = 0x434d454d,

    /// Kernel driver configuration.  The zbi_header_t.extra field gives a
    /// ZBI_KERNEL_DRIVER_* type that determines the payload format.
    /// See <lib/zbi-format/driver-config.h> for details.
    ///
    /// 'KDRV'
    KernelDriver = 0x5652444b,

    /// 'ACPI' Root Table Pointer, a uint64_t physical address.
    ///
    /// 'RSDP'
    AcpiRsdp = 0x50445352,

    /// 'SMBI'
    ///
    /// 'SMBI'OS entry point, a uint64_t physical address.
    Smbios = 0x49424d53,

    /// EFI system table, a uint64_t physical address.
    ///
    /// 'EFIS'
    EfiSystemTable = 0x53494645,

    /// EFI memory attributes table. An example of this format can be found in UEFI 2.10 section 4.6.4,
    /// but the consumer of this item is responsible for interpreting whatever the bootloader supplies
    /// (in particular the "version" field may differ as the format evolves).
    ///
    /// 'EMAT'
    EfiMemoryAttributesTable = 0x54414d45,

    /// Framebuffer parameters, a zbi_swfb_t entry.
    ///
    /// 'SWFB'
    Framebuffer = 0x42465753,

    /// The image arguments, data is a trivial text format of one "key=value" per line
    /// with leading whitespace stripped and "#" comment lines and blank lines ignored.
    /// It is processed by bootsvc and parsed args are shared to others via Arguments service.
    /// TODO: the format can be streamlined after the /config/additional_boot_args compat support is
    /// removed.
    ///
    /// 'IARG'
    ImageArgs = 0x47524149,

    /// A copy of the boot version stored within the sysconfig
    /// partition
    ///
    /// 'BVRS'
    BootVersion = 0x53525642,

    /// MAC address for Ethernet, Wifi, Bluetooth, etc.  zbi_header_t.extra
    /// is a board-specific index to specify which device the MAC address
    /// applies to.  zbi_header_t.length gives the size in bytes, which
    /// varies depending on the type of address appropriate for the device.
    ///
    /// mMAC
    DrvMacAddress = 1128353133, // 0x43414d00 | TYPE_DRIVER_METADATA_PREFIX

    /// A partition map for a storage device, a zbi_partition_map_t header
    /// followed by one or more zbi_partition_t entries.  zbi_header_t.extra
    /// is a board-specific index to specify which device this applies to.
    ///
    /// mPRT
    DrvPartitionMap = 1414680685, // 0x54525000 | TYPE_DRIVER_METADATA_PREFIX

    /// Private information for the board driver.
    ///
    /// mBOR
    DrvBoardPrivate = 1380926061, // 0x524f4200 | TYPE_DRIVER_METADATA_PREFIX

    /// 'HWRB'
    HwRebootReason = 0x42525748,

    /// The serial number, an unterminated ASCII string of printable non-whitespace
    /// characters with length zbi_header_t.length.
    ///
    /// 'SRLN'
    SerialNumber = 0x4e4c5253,

    /// This type specifies a binary file passed in by the bootloader.
    /// The first byte specifies the length of the filename without a NUL terminator.
    /// The filename starts on the second byte.
    /// The file contents are located immediately after the filename.
    ///
    /// Layout: | name_len |        name       |   payload
    ///           ^(1 byte)  ^(name_len bytes)     ^(length of file)
    ///
    /// 'BTFL'
    BootloaderFile = 0x4c465442,

    /// The devicetree blob from the legacy boot loader, if any.  This is used only
    /// for diagnostic and development purposes.  Zircon kernel and driver
    /// configuration is entirely driven by specific ZBI items from the boot
    /// loader.  The boot shims for legacy boot loaders pass the raw devicetree
    /// along for development purposes, but extract information from it to populate
    /// specific ZBI items such as ZBI_TYPE_KERNEL_DRIVER et al.
    Devicetree = 0xd00dfeed,

    /// An arbitrary number of random bytes attested to have high entropy.  Any
    /// number of items of any size can be provided, but no data should be provided
    /// that is not true entropy of cryptographic quality.  This is used to seed
    /// secure cryptographic pseudo-random number generators.
    ///
    /// 'RAND'
    SecureEntropy = 0x444e4152,

    /// This provides a data dump and associated logging from a boot loader,
    /// shim, or earlier incarnation that wants its data percolated up by the
    /// booting Zircon kernel. See zbi_debugdata_t for a description of the
    /// payload.
    ///
    /// 'DBGD'
    Debugdata = 0x44474244,

    /// Each RISC-V hart has an associated "ISA string" encoding its supported
    /// extensions: see Chapter 27 (ISA Extension Naming Conventions) in
    /// https://riscv.org/wp-content/uploads/2019/12/riscv-spec-20191213.pdf.
    /// This item gives the NUL-separated (and -terminated) concatenation of
    /// all such strings in the system.
    ///
    /// The first character of the table must be NUL; that is, the first entry
    /// in the table must be the empty and invalid, which is pointed to by a
    /// default-constructructed index. Beyond that, there are no guarantees to
    /// order the strings in the table or the extent of their (de)duplication.
    ///
    /// A given hart's index into the table is encoded within its associated
    /// node structure in the ZBI_TYPE_CPU_TOPOLOGY item.
    ///
    /// 'VISA'
    Riscv64IsaStrtab = 0x41534956,

    /// The payload corresponds to the unmodified contents of the BOOTCONFIG
    /// that was found on the tail of the ramdisk, without the trailer.
    ///
    /// 'BCFG'
    LinuxBootconfig = 0x47464342,
}

impl Type {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0x544f4f42 => Some(Self::Container),

            1280201291 => Some(Self::KernelX64),

            944656971 => Some(Self::KernelArm64),

            1447973451 => Some(Self::KernelRiscv64),

            0x50494b53 => Some(Self::Discard),

            0x4b534452 => Some(Self::StorageRamdisk),

            0x42534642 => Some(Self::StorageBootfs),

            0x5254534b => Some(Self::StorageKernel),

            0x46534642 => Some(Self::StorageBootfsFactory),

            0x4c444d43 => Some(Self::Cmdline),

            0x4d4f4f42 => Some(Self::Crashlog),

            0x4c4c564e => Some(Self::Nvram),

            0x44494c50 => Some(Self::PlatformId),

            1230193261 => Some(Self::DrvBoardInfo),

            0x43505533 => Some(Self::CpuTopology),

            0x434d454d => Some(Self::MemConfig),

            0x5652444b => Some(Self::KernelDriver),

            0x50445352 => Some(Self::AcpiRsdp),

            0x49424d53 => Some(Self::Smbios),

            0x53494645 => Some(Self::EfiSystemTable),

            0x54414d45 => Some(Self::EfiMemoryAttributesTable),

            0x42465753 => Some(Self::Framebuffer),

            0x47524149 => Some(Self::ImageArgs),

            0x53525642 => Some(Self::BootVersion),

            1128353133 => Some(Self::DrvMacAddress),

            1414680685 => Some(Self::DrvPartitionMap),

            1380926061 => Some(Self::DrvBoardPrivate),

            0x42525748 => Some(Self::HwRebootReason),

            0x4e4c5253 => Some(Self::SerialNumber),

            0x4c465442 => Some(Self::BootloaderFile),

            0xd00dfeed => Some(Self::Devicetree),

            0x444e4152 => Some(Self::SecureEntropy),

            0x44474244 => Some(Self::Debugdata),

            0x41534956 => Some(Self::Riscv64IsaStrtab),

            0x47464342 => Some(Self::LinuxBootconfig),

            _ => None,
        }
    }
}

impl From<Type> for u32 {
    fn from(val: Type) -> Self {
        val as Self
    }
}

/// LSW of sha256("bootdata")
pub const CONTAINER_MAGIC: u32 = 0x868cf7e6;

/// LSW of sha256("bootitem")
pub const ITEM_MAGIC: u32 = 0xb5781729;

/// Flags associated with an item.  A valid flags value must always include
/// ZBI_FLAGS_VERSION. Values should also contain ZBI_FLAGS_CRC32 for any item
/// where it's feasible to compute the CRC32 at build time.  Other flags are
/// specific to each type.
#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct Flags(u32);

bitflags! {
    impl Flags : u32 {

        /// This flag is always required.
        const VERSION = 1 << 16;

        /// ZBI items with the CRC32 flag must have a valid crc32.
        /// Otherwise their crc32 field must contain ZBI_ITEM_NO_CRC32
        const CRC32 = 1 << 17;
  }
}

/// Value for zbi_header_t.crc32 when ZBI_FLAGS_CRC32 is not set.
pub const ITEM_NO_CRC32: u32 = 0x4a87e8d6;

/// Each header must be 8-byte aligned.  The length field specifies the
/// actual payload length and does not include the size of padding.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub struct Header {
    /// ZBI_TYPE_* constant.
    pub r#type: Type,

    /// Size of the payload immediately following this header.  This
    /// does not include the header itself nor any alignment padding
    /// after the payload.
    pub length: u32,

    /// Type-specific extra data.  Each type specifies the use of this
    /// field.  When not explicitly specified, it should be zero.
    pub extra: u32,

    /// Flags for this item.
    pub flags: Flags,

    /// For future expansion.  Set to 0.
    pub reserved0: u32,
    pub reserved1: u32,

    /// Must be ZBI_ITEM_MAGIC.
    pub magic: u32,

    /// Must be the CRC32 of payload if ZBI_FLAGS_CRC32 is set,
    /// otherwise must be ZBI_ITEM_NO_CRC32.
    pub crc32: u32,
}
