// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zbi` by zither, a Fuchsia platform tool.

use bitflags::bitflags;
use zerocopy::{FromBytes, Immutable, IntoBytes, TryFromBytes};

/// ZBI_TYPE_KERNEL_DRIVER item types (for zbi_header_t.extra)
#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Immutable, IntoBytes, PartialEq, TryFromBytes)]
pub enum KernelDriver {
    /// 'PSCI'
    ArmPsci = 0x49435350,

    /// 'PSCS'
    ArmPsciCpuSuspend = 0x53435350,

    /// 'GIC2'
    ArmGicV2 = 0x32434947,

    /// 'GIC3'
    ArmGicV3 = 0x33434947,

    /// 'ATIM'
    ArmGenericTimer = 0x4d495441,

    /// 'ATMM'
    ArmGenericTimerMmio = 0x4d4d5441,

    /// 'PL0U'
    Pl011Uart = 0x55304c50,

    /// 'AMLU'
    AmlogicUart = 0x554c4d41,

    /// 'AMLH'
    AmlogicHdcp = 0x484c4d41,

    /// 'DW8U'
    Dw8250Uart = 0x44573855,

    /// 'RMLH' (typoed, originally intended to by 'AMLR')
    AmlogicRng = 0x484c4d52,

    /// 'WD32'
    Generic32Watchdog = 0x32334457,

    /// 'GENI'
    GeniUart = 0x494E4547,

    /// '8250'
    I8250PioUart = 0x30353238,

    /// '825M'
    I8250Mmio32Uart = 0x4d353238,

    /// '825B'
    I8250Mmio8Uart = 0x42353238,

    /// 'MMTP'
    MotmotPower = 0x4d4d5450,

    /// '370P'
    As370Power = 0x50303733,

    /// 'MNFP'
    MoonflowerPower = 0x4d4e4650,

    /// 'IRSP'
    IrisPower = 0x49525350,

    /// 'IMXU'
    ImxUart = 0x55584d49,

    /// 'PLIC'
    RiscvPlic = 0x43494C50,

    /// 'RTIM'
    RiscvGenericTimer = 0x4D495452,

    /// 'PXAU'
    PxaUart = 0x50584155,

    /// 'EXYU'
    ExynosUsiUart = 0x45585955,

    /// `QRNG`
    QcomRng = 0x474E5251,

    /// 'SMMU'
    ArmSmmu = 0x554D4D51,

    /// 'CPEM'
    CpuEnergyModel = 0x4d455043,
}

impl KernelDriver {
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0x49435350 => Some(Self::ArmPsci),

            0x53435350 => Some(Self::ArmPsciCpuSuspend),

            0x32434947 => Some(Self::ArmGicV2),

            0x33434947 => Some(Self::ArmGicV3),

            0x4d495441 => Some(Self::ArmGenericTimer),

            0x4d4d5441 => Some(Self::ArmGenericTimerMmio),

            0x55304c50 => Some(Self::Pl011Uart),

            0x554c4d41 => Some(Self::AmlogicUart),

            0x484c4d41 => Some(Self::AmlogicHdcp),

            0x44573855 => Some(Self::Dw8250Uart),

            0x484c4d52 => Some(Self::AmlogicRng),

            0x32334457 => Some(Self::Generic32Watchdog),

            0x494E4547 => Some(Self::GeniUart),

            0x30353238 => Some(Self::I8250PioUart),

            0x4d353238 => Some(Self::I8250Mmio32Uart),

            0x42353238 => Some(Self::I8250Mmio8Uart),

            0x4d4d5450 => Some(Self::MotmotPower),

            0x50303733 => Some(Self::As370Power),

            0x4d4e4650 => Some(Self::MoonflowerPower),

            0x49525350 => Some(Self::IrisPower),

            0x55584d49 => Some(Self::ImxUart),

            0x43494C50 => Some(Self::RiscvPlic),

            0x4D495452 => Some(Self::RiscvGenericTimer),

            0x50584155 => Some(Self::PxaUart),

            0x45585955 => Some(Self::ExynosUsiUart),

            0x474E5251 => Some(Self::QcomRng),

            0x554D4D51 => Some(Self::ArmSmmu),

            0x4d455043 => Some(Self::CpuEnergyModel),

            _ => None,
        }
    }
}

impl From<KernelDriver> for u32 {
    fn from(val: KernelDriver) -> Self {
        val as Self
    }
}

/// Kernel driver struct that can be used for simple drivers.
/// Used by ZBI_KERNEL_DRIVER_PL011_UART, ZBI_KERNEL_DRIVER_AMLOGIC_UART, and
/// ZBI_KERNEL_DRIVER_GENI_UART, ZBI_KERNEL_DRIVER_I8250_MMIO_UART.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgSimple {
    pub mmio_phys: u64,
    pub irq: u32,
    pub flags: u32,
}

#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct KernelDriverIrqFlags(u32);

bitflags! {
    impl KernelDriverIrqFlags : u32 {

        /// When no flag is set, implies no information was obtained, and the
        /// kernel will apply default configuration as it sees fit.
        const EDGE_TRIGGERED = 1 << 0;
        const LEVEL_TRIGGERED = 1 << 1;

        /// Interpretation depends on whether is edge or level triggered.
        /// When `LEVEL_TRIGGERED` refers to `ACTIVE_LOW`.
        /// When `EDGE_TRIGGERED` refers to `HIGH_TO_LOW`.
        const POLARITY_LOW = 1 << 2;

        /// Interpretation depends on whether is edge or level triggered.
        /// When `LEVEL_TRIGGERED` refers to `ACTIVE_HIGH`.
        /// When `EDGE_TRIGGERED` refers to `LOW_TO_HIGH`.
        const POLARITY_HIGH = 1 << 3;
  }
}

/// Used by ZBI_KERNEL_DRIVER_I8250_PIO_UART.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgSimplePio {
    pub base: u16,
    pub reserved: u16,
    pub irq: u32,
}

/// for ZBI_KERNEL_DRIVER_ARM_PSCI
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmPsciDriver {
    pub use_hvc: u8,
    pub reserved: [u8; 7],
    pub shutdown_args: [u64; 3],
    pub reboot_args: [u64; 3],
    pub reboot_bootloader_args: [u64; 3],
    pub reboot_recovery_args: [u64; 3],
}

/// for ZBI_KERNEL_DRIVER_ARM_GIC_V2
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmGicV2Driver {
    pub mmio_phys: u64,
    pub msi_frame_phys: u64,
    pub gicd_offset: u64,
    pub gicc_offset: u64,
    pub gich_offset: u64,
    pub gicv_offset: u64,
    pub ipi_base: u32,
    pub optional: u8,
    pub use_msi: u8,
    pub reserved: u16,
}

/// for ZBI_KERNEL_DRIVER_ARM_GIC_V3
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmGicV3Driver {
    pub mmio_phys: u64,
    pub gicd_offset: u64,
    pub gicr_offset: u64,
    pub gicr_stride: u64,
    pub reserved0: u64,
    pub ipi_base: u32,
    pub optional: u8,
    pub reserved1: [u8; 3],
}

/// for ZBI_KERNEL_DRIVER_ARM_GENERIC_TIMER
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmGenericTimerDriver {
    pub irq_phys: u32,
    pub irq_virt: u32,
    pub irq_sphys: u32,
    pub freq_override: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmGenericTimerMmioFrame {
    /// Base address for the frame's EL1 view.
    pub mmio_phys_el1: u64,

    /// Base address for the frame's EL0 view.
    /// This is optional.
    pub mmio_phys_el0: u64,

    /// IRQ information for physical timer. This is mandatory.
    pub irq_phys: u32,

    /// Same scheme as `DcfgSimple::irq`. This is mandatory.
    pub irq_phys_flags: u32,

    /// IRQ information for virtual timer.
    /// This is optional.
    /// When is not present both `irq_virt` and `irq_virt_flags` will be zero.
    pub irq_virt: u32,

    /// Same scheme as `DcfgSimple::irq`.
    pub irq_virt_flags: u32,
}

/// for ZBI_KERNEL_DRIVER_ARM_GENERIC_TIMER_MMIO
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmGenericTimerMmioDriver {
    /// Base address of `CNTCTLBase` frame.
    pub mmio_phys: u64,

    /// The frequency of the main counter for the timer.
    pub frequency: u32,

    /// Bitmask containing the set of active frames.
    /// The `i-th` frame is considered active iff the `i`-th bit is set.
    /// Note: While there may be up to 8 frames, both missing and disabled frames are treated
    /// as inactive. Disabled frame information will be present, while missing frames will be zeroed.
    pub active_frames_mask: u8,
    pub reserved0: [u8; 3],

    /// Information for each individual frame.
    /// Inactive frames must be zero-filled.
    pub frames: [DcfgArmGenericTimerMmioFrame; 8],
}

/// for ZBI_KERNEL_DRIVER_AMLOGIC_HDCP
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgAmlogicHdcpDriver {
    pub preset_phys: u64,
    pub hiu_phys: u64,
    pub hdmitx_phys: u64,
}

/// for ZBI_KERNEL_DRIVER_AMLOGIC_RNG
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgAmlogicRngDriver {
    pub rng_data_phys: u64,
    pub rng_status_phys: u64,
    pub rng_refresh_interval_usec: u64,
}

/// Defines a register write action for a generic kernel watchdog driver.  An
/// action consists of the following steps.
///
/// 1) Read from the register located a physical address |addr|
/// 2) Clear all of the bits in the value which was read using the |clr_mask|
/// 3) Set all of the bits in the value using the |set_mask|
/// 4) Write this value back to the address located at addr
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgGeneric32WatchdogAction {
    pub addr: u64,
    pub clr_mask: u32,
    pub set_mask: u32,
}

#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct KernelDriverGeneric32WatchdogFlags(u32);

bitflags! {
    impl KernelDriverGeneric32WatchdogFlags : u32 {
        const ENABLED = 1 << 0;
  }
}

/// 1ms
pub const KERNEL_DRIVER_GENERIC32_WATCHDOG_MIN_PERIOD: i64 = 1000000;

/// Definitions of actions which may be taken by a generic 32 bit watchdog timer
/// kernel driver which may be passed by a bootloader.  Field definitions are as
/// follows.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgGeneric32Watchdog {
    /// The address and masks needed to "pet" (aka, dismiss) a hardware watchdog timer.
    pub pet_action: DcfgGeneric32WatchdogAction,

    /// The address and masks needed to enable a hardware watchdog timer.  If enable
    /// is an unsupported operation, the addr of the |enable_action| shall be zero.
    pub enable_action: DcfgGeneric32WatchdogAction,

    /// The address and masks needed to disable a hardware watchdog timer.  If
    /// disable is an unsupported operation, the addr of the |disable_action| shall
    /// be zero.
    pub disable_action: DcfgGeneric32WatchdogAction,

    /// The period of the watchdog timer given in nanoseconds.  When enabled, the
    /// watchdog timer driver must pet the watch dog at least this often.  The value
    /// must be at least 1 mSec, typically much larger (on the order of a second or
    /// two).
    pub watchdog_period_nsec: i64,

    /// Storage for additional flags.  Currently, only one flag is defined,
    /// "FLAG_ENABLED".  When this flag is set, it indicates that the watchdog timer
    /// was left enabled by the bootloader at startup.
    pub flags: KernelDriverGeneric32WatchdogFlags,
    pub reserved: u32,
}

/// for ZBI_KERNEL_DRIVER_RISCV_PLIC
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgRiscvPlicDriver {
    /// Physical address of the PLIC MMIO region.
    pub mmio_phys: u64,

    /// Size in bytes of the PLIC MMIO region.
    pub size_bytes: u32,

    /// Number of supported IRQs.
    pub num_irqs: u32,
}

/// for ZBI_KERNEL_DRIVER_RISCV_GENERIC_TIMER
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgRiscvGenericTimerDriver {
    pub freq_hz: u32,
    pub reserved: u32,
}

#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct ArmPsciCpuSuspendStateFlags(u32);

bitflags! {
    impl ArmPsciCpuSuspendStateFlags : u32 {

        /// If set, when entering the associated low power state the CPU's architectural timer will be
        /// turned off, making it an unsuitable source for exiting the low power state.
        /// A different source must be programmed.
        const LOCAL_TIMER_STOPS = 1 << 0;

        /// If set, the PSCI CPU Suspend operation will affect the entire power domain, implying all other
        /// CPUs of the power domain must be in a low power mode. That is, the last CPU in the power
        /// domain is the one to enter this power state.
        const TARGETS_POWER_DOMAIN = 1 << 1;
  }
}

/// The ZBI_KERNEL_DRIVER_ARM_PSCI_CPU_SUSPEND's payload consists on any number of
/// `DcfgArmPsciCpuSuspendState` entries.
///
/// The length of the item is `sizeof(zbi_dcfg_arm_psci_cou_suspend_state_t)` times the number of
/// entries. Each entry describes an 'idle state' that can be entered through PSCI CPU Suspend call.
///
/// Entries in the table may be in any order, and only a single item of type
/// ZBI_KERNEL_DRIVER_ARM_PSCI_CPU_SUSPEND should be present in the ZBI.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmPsciCpuSuspendState {
    /// Unique identifier representing this suspend state.
    pub id: u32,

    /// PSCI power_state as described in "Section 5.4.2. of Arm Power State Coordination Interface"
    /// v1.3.
    pub power_state: u32,
    pub flags: ArmPsciCpuSuspendStateFlags,

    /// Latency in microseconds to enter the low power state.
    pub entry_latency_us: u32,

    /// Latency in microseconds to exit the low power state.
    pub exit_latency_us: u32,

    /// Minimum time in microseconds, including `entry_latency`, to stay in this low power state.
    /// Spending less time would be inefficient energy-wise.
    pub min_residency_us: u32,
}

#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct QcomRngFlags(u32);

bitflags! {
    impl QcomRngFlags : u32 {

        /// The driver has been handed off, configured and initialized.
        const ENABLED = 1 << 0;
  }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgQcomRng {
    /// Base address of the register banks for the device.
    pub mmio_phys: u64,
    pub flags: QcomRngFlags,
    pub reserved: [u8; 4],
}

pub const KERNEL_DRIVER_SMMU_MAX_IRQS: u32 = 128;

pub const KERNEL_DRIVER_SMMU_MAX_HANDOFF_SMRS: u32 = 16;

/// The IRQ flags as they would be reported by a device tree description.
#[repr(C)]
#[derive(
    Immutable, IntoBytes, FromBytes, Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub struct ArmSmmuIrqFlags(u32);

bitflags! {
    impl ArmSmmuIrqFlags : u32 {

        /// A rising edge triggered interrupt.
        const RISING_EDGE = 1 << 0;

        /// A falling edge triggered interrupt.
        const FALLING_EDGE = 1 << 1;

        /// An active high level triggered interrupt.
        const ACTIVE_HIGH = 1 << 2;

        /// An active low level triggered interrupt.
        const ACTIVE_LOW = 1 << 3;
  }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct DcfgArmSmmuIrq {
    /// The IRQ number reported as an "absolute" GIC interrupt index.  SMMU
    /// interrupts are expected to be SPIs, and the SPI range of a GIC's
    /// interrupt indices starts at 32.  Because of this, it can be assumed that
    /// any interrupt number which is < 32 indicates an invalid interrupt.
    pub num: u32,

    /// The IRQ flags as they would be reported by a device tree description.
    pub flags: ArmSmmuIrqFlags,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Immutable, PartialEq, TryFromBytes)]
pub struct DcfgArmSmmuDriver {
    /// Base physical address of the SMMU address space.  This is also the start of
    /// Global Register Space 0 in the SMMU.  The effective size of these registers
    /// will be determined at runtime from the ID registers present in this first
    /// page, specifically the number of context banks in IDR1.NUMCB.
    ///
    /// See:
    ///
    /// ARM System Memory Management Unit Architecture Specification
    /// SMMU architecture version 2.0
    /// ARM IHI 0062D.c
    ///
    /// for more details.
    pub mmio_phys: u64,

    /// The number of context banks that a driver should limit itself to using,
    /// or 0 if all discovered context banks are available for use.
    pub num_context_banks_override: u32,

    /// The number of stream match registers that a driver should limit itself
    /// to using, or 0 if all discovered SMRs are available for use.
    pub num_smr_override: u32,

    /// An array of context bank interrupt definitions discovered during device
    /// enumeration.  |irq_cnt| reports the total number of valid members while
    /// |irqs|, while |global_irq_cnt| reports the total number of global
    /// interrupts in |irqs|, with the rest of the interrupts being context bank
    /// interrupts.  So:
    ///
    /// + IRQs [0, global_irq_cnt) are global interrupts
    /// + IRQs [global_irq_cnt, irq_cnt) are context bank interrupts
    ///   corresponding to context banks [0, irq_cnt - global_irq_cnt)
    pub irq_cnt: u32,
    pub global_irq_cnt: u32,
    pub irqs: [DcfgArmSmmuIrq; 128],

    /// An array of stream IDs (defined using SMR value/mask encoding) which are
    /// being "handed off" from the bootloader to the HLOS.  Downstream drivers
    /// should take care to preserve the existing configuration for these stream
    /// ids.  |handoff_smr_cnt| reports the number of members of |handoff_smrs|
    /// which are valid.
    ///
    /// Note:  Stream ID ranges in SMRs are encoded using two 16 bit fields, a
    /// value field and a mask field.  These two fields are packed into a single
    /// 32-bit integer, with the mask packed into the upper 16 bits, and the
    /// value packed into the lower 16 bits.
    pub handoff_smr_cnt: u32,
    pub handoff_smrs: [u32; 16],
}

pub const KERNEL_DRIVER_CPU_ENERGY_MODEL_MAX_OPPS: u32 = 32;

/// Operating Performance Point (OPP) definition for CPU Energy Model.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct CpuEnergyModelOpp {
    /// CPU frequency in kilohertz (kHz).
    pub frequency_khz: u32,

    /// Relative compute capacity scale (unit-less, normalized relative to max capacity across power domains, e.g. 1024 or 1000 scale).
    pub capacity: u32,

    /// Power consumption in microwatts (uW).
    pub power_uw: u32,

    /// Operating voltage in millivolts (mV).
    pub voltage_mv: u32,
}

/// Performance / Power Domain definition for CPU Energy Model.
///
/// The ZBI_KERNEL_DRIVER_IRIS_POWER / ZBI_KERNEL_DRIVER_CPU_ENERGY_MODEL payload
/// consists of one or more `CpuEnergyModelDomain` entries. The length of the
/// item is `sizeof(zbi_cpu_energy_model_domain_t)` times the number of domains.
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, FromBytes, Immutable, IntoBytes, PartialEq)]
pub struct CpuEnergyModelDomain {
    pub cpu_mask: u64,
    pub max_rate: u64,
    pub domain_id: u32,
    pub opp_count: u32,
    pub opps: [CpuEnergyModelOpp; 32],
}
