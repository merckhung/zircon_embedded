// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library
//   zbi (//sdk/fidl/zbi/driver-config.fidl)
// by zither, a Fuchsia platform tool.

#ifndef ZBI_ASM_DRIVER_CONFIG_H_
#define ZBI_ASM_DRIVER_CONFIG_H_

// ZBI_TYPE_KERNEL_DRIVER item types (for zbi_header_t.extra)

// 'PSCI'
#define ZBI_KERNEL_DRIVER_ARM_PSCI (0x49435350)

// 'PSCS'
#define ZBI_KERNEL_DRIVER_ARM_PSCI_CPU_SUSPEND (0x53435350)

// 'GIC2'
#define ZBI_KERNEL_DRIVER_ARM_GIC_V2 (0x32434947)

// 'GIC3'
#define ZBI_KERNEL_DRIVER_ARM_GIC_V3 (0x33434947)

// 'ATIM'
#define ZBI_KERNEL_DRIVER_ARM_GENERIC_TIMER (0x4d495441)

// 'ATMM'
#define ZBI_KERNEL_DRIVER_ARM_GENERIC_TIMER_MMIO (0x4d4d5441)

// 'PL0U'
#define ZBI_KERNEL_DRIVER_PL011_UART (0x55304c50)

// 'AMLU'
#define ZBI_KERNEL_DRIVER_AMLOGIC_UART (0x554c4d41)

// 'AMLH'
#define ZBI_KERNEL_DRIVER_AMLOGIC_HDCP (0x484c4d41)

// 'DW8U'
#define ZBI_KERNEL_DRIVER_DW8250_UART (0x44573855)

// 'RMLH' (typoed, originally intended to by 'AMLR')
#define ZBI_KERNEL_DRIVER_AMLOGIC_RNG (0x484c4d52)

// 'WD32'
#define ZBI_KERNEL_DRIVER_GENERIC32_WATCHDOG (0x32334457)

// 'GENI'
#define ZBI_KERNEL_DRIVER_GENI_UART (0x494E4547)

// '8250'
#define ZBI_KERNEL_DRIVER_I8250_PIO_UART (0x30353238)

// '825M'
#define ZBI_KERNEL_DRIVER_I8250_MMIO32_UART (0x4d353238)

// '825B'
#define ZBI_KERNEL_DRIVER_I8250_MMIO8_UART (0x42353238)

// 'MMTP'
#define ZBI_KERNEL_DRIVER_MOTMOT_POWER (0x4d4d5450)

// '370P'
#define ZBI_KERNEL_DRIVER_AS370_POWER (0x50303733)

// 'MNFP'
#define ZBI_KERNEL_DRIVER_MOONFLOWER_POWER (0x4d4e4650)

// 'IRSP'
#define ZBI_KERNEL_DRIVER_IRIS_POWER (0x49525350)

// 'IMXU'
#define ZBI_KERNEL_DRIVER_IMX_UART (0x55584d49)

// 'PLIC'
#define ZBI_KERNEL_DRIVER_RISCV_PLIC (0x43494C50)

// 'RTIM'
#define ZBI_KERNEL_DRIVER_RISCV_GENERIC_TIMER (0x4D495452)

// 'PXAU'
#define ZBI_KERNEL_DRIVER_PXA_UART (0x50584155)

// 'EXYU'
#define ZBI_KERNEL_DRIVER_EXYNOS_USI_UART (0x45585955)

// `QRNG`
#define ZBI_KERNEL_DRIVER_QCOM_RNG (0x474E5251)

// 'SMMU'
#define ZBI_KERNEL_DRIVER_ARM_SMMU (0x554D4D51)

// 'CPEM'
#define ZBI_KERNEL_DRIVER_CPU_ENERGY_MODEL (0x4d455043)

// Kernel driver struct that can be used for simple drivers.
// Used by ZBI_KERNEL_DRIVER_PL011_UART, ZBI_KERNEL_DRIVER_AMLOGIC_UART, and
// ZBI_KERNEL_DRIVER_GENI_UART, ZBI_KERNEL_DRIVER_I8250_MMIO_UART.
#define ZBI_DCFG_SIMPLE_SIZEOF (16)

#define ZBI_DCFG_SIMPLE_MMIO_PHYS (0)
#define ZBI_DCFG_SIMPLE_IRQ (8)
#define ZBI_DCFG_SIMPLE_FLAGS (12)

// When no flag is set, implies no information was obtained, and the
// kernel will apply default configuration as it sees fit.

#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_EDGE_TRIGGERED_SHIFT (0)
#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_EDGE_TRIGGERED \
  (1 << ZBI_KERNEL_DRIVER_IRQ_FLAGS_EDGE_TRIGGERED_SHIFT)

#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_LEVEL_TRIGGERED_SHIFT (1)
#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_LEVEL_TRIGGERED \
  (1 << ZBI_KERNEL_DRIVER_IRQ_FLAGS_LEVEL_TRIGGERED_SHIFT)

// Interpretation depends on whether is edge or level triggered.
// When `LEVEL_TRIGGERED` refers to `ACTIVE_LOW`.
// When `EDGE_TRIGGERED` refers to `HIGH_TO_LOW`.

#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_POLARITY_LOW_SHIFT (2)
#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_POLARITY_LOW \
  (1 << ZBI_KERNEL_DRIVER_IRQ_FLAGS_POLARITY_LOW_SHIFT)

// Interpretation depends on whether is edge or level triggered.
// When `LEVEL_TRIGGERED` refers to `ACTIVE_HIGH`.
// When `EDGE_TRIGGERED` refers to `LOW_TO_HIGH`.

#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_POLARITY_HIGH_SHIFT (3)
#define ZBI_KERNEL_DRIVER_IRQ_FLAGS_POLARITY_HIGH \
  (1 << ZBI_KERNEL_DRIVER_IRQ_FLAGS_POLARITY_HIGH_SHIFT)

// Used by ZBI_KERNEL_DRIVER_I8250_PIO_UART.
#define ZBI_DCFG_SIMPLE_PIO_SIZEOF (8)

#define ZBI_DCFG_SIMPLE_PIO_BASE (0)
#define ZBI_DCFG_SIMPLE_PIO_RESERVED (2)
#define ZBI_DCFG_SIMPLE_PIO_IRQ (4)

// for ZBI_KERNEL_DRIVER_ARM_PSCI
#define ZBI_DCFG_ARM_PSCI_DRIVER_SIZEOF (104)

#define ZBI_DCFG_ARM_PSCI_DRIVER_USE_HVC (0)
#define ZBI_DCFG_ARM_PSCI_DRIVER_RESERVED (1)
#define ZBI_DCFG_ARM_PSCI_DRIVER_SHUTDOWN_ARGS (8)
#define ZBI_DCFG_ARM_PSCI_DRIVER_REBOOT_ARGS (32)
#define ZBI_DCFG_ARM_PSCI_DRIVER_REBOOT_BOOTLOADER_ARGS (56)
#define ZBI_DCFG_ARM_PSCI_DRIVER_REBOOT_RECOVERY_ARGS (80)

// for ZBI_KERNEL_DRIVER_ARM_GIC_V2
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_SIZEOF (56)

#define ZBI_DCFG_ARM_GIC_V2_DRIVER_MMIO_PHYS (0)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_MSI_FRAME_PHYS (8)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_GICD_OFFSET (16)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_GICC_OFFSET (24)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_GICH_OFFSET (32)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_GICV_OFFSET (40)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_IPI_BASE (48)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_OPTIONAL (52)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_USE_MSI (53)
#define ZBI_DCFG_ARM_GIC_V2_DRIVER_RESERVED (54)

// for ZBI_KERNEL_DRIVER_ARM_GIC_V3
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_SIZEOF (48)

#define ZBI_DCFG_ARM_GIC_V3_DRIVER_MMIO_PHYS (0)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_GICD_OFFSET (8)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_GICR_OFFSET (16)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_GICR_STRIDE (24)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_RESERVED0 (32)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_IPI_BASE (40)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_OPTIONAL (44)
#define ZBI_DCFG_ARM_GIC_V3_DRIVER_RESERVED1 (45)

// for ZBI_KERNEL_DRIVER_ARM_GENERIC_TIMER
#define ZBI_DCFG_ARM_GENERIC_TIMER_DRIVER_SIZEOF (16)

#define ZBI_DCFG_ARM_GENERIC_TIMER_DRIVER_IRQ_PHYS (0)
#define ZBI_DCFG_ARM_GENERIC_TIMER_DRIVER_IRQ_VIRT (4)
#define ZBI_DCFG_ARM_GENERIC_TIMER_DRIVER_IRQ_SPHYS (8)
#define ZBI_DCFG_ARM_GENERIC_TIMER_DRIVER_FREQ_OVERRIDE (12)

#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_SIZEOF (32)

// Base address for the frame's EL1 view.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_MMIO_PHYS_EL1 (0)

// Base address for the frame's EL0 view.
// This is optional.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_MMIO_PHYS_EL0 (8)

// IRQ information for physical timer. This is mandatory.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_IRQ_PHYS (16)

// Same scheme as `DcfgSimple::irq`. This is mandatory.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_IRQ_PHYS_FLAGS (20)

// IRQ information for virtual timer.
// This is optional.
// When is not present both `irq_virt` and `irq_virt_flags` will be zero.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_IRQ_VIRT (24)

// Same scheme as `DcfgSimple::irq`.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_FRAME_IRQ_VIRT_FLAGS (28)

// for ZBI_KERNEL_DRIVER_ARM_GENERIC_TIMER_MMIO
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_DRIVER_SIZEOF (272)

// Base address of `CNTCTLBase` frame.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_DRIVER_MMIO_PHYS (0)

// The frequency of the main counter for the timer.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_DRIVER_FREQUENCY (8)

// Bitmask containing the set of active frames.
// The `i-th` frame is considered active iff the `i`-th bit is set.
// Note: While there may be up to 8 frames, both missing and disabled frames are treated
// as inactive. Disabled frame information will be present, while missing frames will be zeroed.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_DRIVER_ACTIVE_FRAMES_MASK (12)
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_DRIVER_RESERVED0 (13)

// Information for each individual frame.
// Inactive frames must be zero-filled.
#define ZBI_DCFG_ARM_GENERIC_TIMER_MMIO_DRIVER_FRAMES (16)

// for ZBI_KERNEL_DRIVER_AMLOGIC_HDCP
#define ZBI_DCFG_AMLOGIC_HDCP_DRIVER_SIZEOF (24)

#define ZBI_DCFG_AMLOGIC_HDCP_DRIVER_PRESET_PHYS (0)
#define ZBI_DCFG_AMLOGIC_HDCP_DRIVER_HIU_PHYS (8)
#define ZBI_DCFG_AMLOGIC_HDCP_DRIVER_HDMITX_PHYS (16)

// for ZBI_KERNEL_DRIVER_AMLOGIC_RNG
#define ZBI_DCFG_AMLOGIC_RNG_DRIVER_SIZEOF (24)

#define ZBI_DCFG_AMLOGIC_RNG_DRIVER_RNG_DATA_PHYS (0)
#define ZBI_DCFG_AMLOGIC_RNG_DRIVER_RNG_STATUS_PHYS (8)
#define ZBI_DCFG_AMLOGIC_RNG_DRIVER_RNG_REFRESH_INTERVAL_USEC (16)

// Defines a register write action for a generic kernel watchdog driver.  An
// action consists of the following steps.
//
// 1) Read from the register located a physical address |addr|
// 2) Clear all of the bits in the value which was read using the |clr_mask|
// 3) Set all of the bits in the value using the |set_mask|
// 4) Write this value back to the address located at addr
#define ZBI_DCFG_GENERIC32_WATCHDOG_ACTION_SIZEOF (16)

#define ZBI_DCFG_GENERIC32_WATCHDOG_ACTION_ADDR (0)
#define ZBI_DCFG_GENERIC32_WATCHDOG_ACTION_CLR_MASK (8)
#define ZBI_DCFG_GENERIC32_WATCHDOG_ACTION_SET_MASK (12)

#define ZBI_KERNEL_DRIVER_GENERIC32_WATCHDOG_FLAGS_ENABLED_SHIFT (0)
#define ZBI_KERNEL_DRIVER_GENERIC32_WATCHDOG_FLAGS_ENABLED \
  (1 << ZBI_KERNEL_DRIVER_GENERIC32_WATCHDOG_FLAGS_ENABLED_SHIFT)

// 1ms
#define ZBI_KERNEL_DRIVER_GENERIC32_WATCHDOG_MIN_PERIOD (1000000)

// Definitions of actions which may be taken by a generic 32 bit watchdog timer
// kernel driver which may be passed by a bootloader.  Field definitions are as
// follows.
#define ZBI_DCFG_GENERIC32_WATCHDOG_SIZEOF (64)

// The address and masks needed to "pet" (aka, dismiss) a hardware watchdog timer.
#define ZBI_DCFG_GENERIC32_WATCHDOG_PET_ACTION (0)

// The address and masks needed to enable a hardware watchdog timer.  If enable
// is an unsupported operation, the addr of the |enable_action| shall be zero.
#define ZBI_DCFG_GENERIC32_WATCHDOG_ENABLE_ACTION (16)

// The address and masks needed to disable a hardware watchdog timer.  If
// disable is an unsupported operation, the addr of the |disable_action| shall
// be zero.
#define ZBI_DCFG_GENERIC32_WATCHDOG_DISABLE_ACTION (32)

// The period of the watchdog timer given in nanoseconds.  When enabled, the
// watchdog timer driver must pet the watch dog at least this often.  The value
// must be at least 1 mSec, typically much larger (on the order of a second or
// two).
#define ZBI_DCFG_GENERIC32_WATCHDOG_WATCHDOG_PERIOD_NSEC (48)

// Storage for additional flags.  Currently, only one flag is defined,
// "FLAG_ENABLED".  When this flag is set, it indicates that the watchdog timer
// was left enabled by the bootloader at startup.
#define ZBI_DCFG_GENERIC32_WATCHDOG_FLAGS (56)
#define ZBI_DCFG_GENERIC32_WATCHDOG_RESERVED (60)

// for ZBI_KERNEL_DRIVER_RISCV_PLIC
#define ZBI_DCFG_RISCV_PLIC_DRIVER_SIZEOF (16)

// Physical address of the PLIC MMIO region.
#define ZBI_DCFG_RISCV_PLIC_DRIVER_MMIO_PHYS (0)

// Size in bytes of the PLIC MMIO region.
#define ZBI_DCFG_RISCV_PLIC_DRIVER_SIZE_BYTES (8)

// Number of supported IRQs.
#define ZBI_DCFG_RISCV_PLIC_DRIVER_NUM_IRQS (12)

// for ZBI_KERNEL_DRIVER_RISCV_GENERIC_TIMER
#define ZBI_DCFG_RISCV_GENERIC_TIMER_DRIVER_SIZEOF (8)

#define ZBI_DCFG_RISCV_GENERIC_TIMER_DRIVER_FREQ_HZ (0)
#define ZBI_DCFG_RISCV_GENERIC_TIMER_DRIVER_RESERVED (4)

// If set, when entering the associated low power state the CPU's architectural timer will be
// turned off, making it an unsuitable source for exiting the low power state.
// A different source must be programmed.

#define ZBI_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS_LOCAL_TIMER_STOPS_SHIFT (0)
#define ZBI_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS_LOCAL_TIMER_STOPS \
  (1 << ZBI_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS_LOCAL_TIMER_STOPS_SHIFT)

// If set, the PSCI CPU Suspend operation will affect the entire power domain, implying all other
// CPUs of the power domain must be in a low power mode. That is, the last CPU in the power
// domain is the one to enter this power state.

#define ZBI_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS_TARGETS_POWER_DOMAIN_SHIFT (1)
#define ZBI_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS_TARGETS_POWER_DOMAIN \
  (1 << ZBI_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS_TARGETS_POWER_DOMAIN_SHIFT)

// The ZBI_KERNEL_DRIVER_ARM_PSCI_CPU_SUSPEND's payload consists on any number of
// `DcfgArmPsciCpuSuspendState` entries.
//
// The length of the item is `sizeof(zbi_dcfg_arm_psci_cou_suspend_state_t)` times the number of
// entries. Each entry describes an 'idle state' that can be entered through PSCI CPU Suspend call.
//
// Entries in the table may be in any order, and only a single item of type
// ZBI_KERNEL_DRIVER_ARM_PSCI_CPU_SUSPEND should be present in the ZBI.
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_SIZEOF (24)

// Unique identifier representing this suspend state.
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_ID (0)

// PSCI power_state as described in "Section 5.4.2. of Arm Power State Coordination Interface"
// v1.3.
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_POWER_STATE (4)
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_FLAGS (8)

// Latency in microseconds to enter the low power state.
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_ENTRY_LATENCY_US (12)

// Latency in microseconds to exit the low power state.
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_EXIT_LATENCY_US (16)

// Minimum time in microseconds, including `entry_latency`, to stay in this low power state.
// Spending less time would be inefficient energy-wise.
#define ZBI_DCFG_ARM_PSCI_CPU_SUSPEND_STATE_MIN_RESIDENCY_US (20)

// The driver has been handed off, configured and initialized.

#define ZBI_QCOM_RNG_FLAGS_ENABLED_SHIFT (0)
#define ZBI_QCOM_RNG_FLAGS_ENABLED (1 << ZBI_QCOM_RNG_FLAGS_ENABLED_SHIFT)

#define ZBI_DCFG_QCOM_RNG_SIZEOF (16)

// Base address of the register banks for the device.
#define ZBI_DCFG_QCOM_RNG_MMIO_PHYS (0)
#define ZBI_DCFG_QCOM_RNG_FLAGS (8)
#define ZBI_DCFG_QCOM_RNG_RESERVED (12)

#define ZBI_KERNEL_DRIVER_SMMU_MAX_IRQS (128)

#define ZBI_KERNEL_DRIVER_SMMU_MAX_HANDOFF_SMRS (16)

// The IRQ flags as they would be reported by a device tree description.

// A rising edge triggered interrupt.

#define ZBI_ARM_SMMU_IRQ_FLAGS_RISING_EDGE_SHIFT (0)
#define ZBI_ARM_SMMU_IRQ_FLAGS_RISING_EDGE (1 << ZBI_ARM_SMMU_IRQ_FLAGS_RISING_EDGE_SHIFT)

// A falling edge triggered interrupt.

#define ZBI_ARM_SMMU_IRQ_FLAGS_FALLING_EDGE_SHIFT (1)
#define ZBI_ARM_SMMU_IRQ_FLAGS_FALLING_EDGE (1 << ZBI_ARM_SMMU_IRQ_FLAGS_FALLING_EDGE_SHIFT)

// An active high level triggered interrupt.

#define ZBI_ARM_SMMU_IRQ_FLAGS_ACTIVE_HIGH_SHIFT (2)
#define ZBI_ARM_SMMU_IRQ_FLAGS_ACTIVE_HIGH (1 << ZBI_ARM_SMMU_IRQ_FLAGS_ACTIVE_HIGH_SHIFT)

// An active low level triggered interrupt.

#define ZBI_ARM_SMMU_IRQ_FLAGS_ACTIVE_LOW_SHIFT (3)
#define ZBI_ARM_SMMU_IRQ_FLAGS_ACTIVE_LOW (1 << ZBI_ARM_SMMU_IRQ_FLAGS_ACTIVE_LOW_SHIFT)

#define ZBI_DCFG_ARM_SMMU_IRQ_SIZEOF (8)

// The IRQ number reported as an "absolute" GIC interrupt index.  SMMU
// interrupts are expected to be SPIs, and the SPI range of a GIC's
// interrupt indices starts at 32.  Because of this, it can be assumed that
// any interrupt number which is < 32 indicates an invalid interrupt.
#define ZBI_DCFG_ARM_SMMU_IRQ_NUM (0)

// The IRQ flags as they would be reported by a device tree description.
#define ZBI_DCFG_ARM_SMMU_IRQ_FLAGS (4)

#define ZBI_DCFG_ARM_SMMU_DRIVER_SIZEOF (1120)

// Base physical address of the SMMU address space.  This is also the start of
// Global Register Space 0 in the SMMU.  The effective size of these registers
// will be determined at runtime from the ID registers present in this first
// page, specifically the number of context banks in IDR1.NUMCB.
//
// See:
//
// ARM System Memory Management Unit Architecture Specification
// SMMU architecture version 2.0
// ARM IHI 0062D.c
//
// for more details.
#define ZBI_DCFG_ARM_SMMU_DRIVER_MMIO_PHYS (0)

// The number of context banks that a driver should limit itself to using,
// or 0 if all discovered context banks are available for use.
#define ZBI_DCFG_ARM_SMMU_DRIVER_NUM_CONTEXT_BANKS_OVERRIDE (8)

// The number of stream match registers that a driver should limit itself
// to using, or 0 if all discovered SMRs are available for use.
#define ZBI_DCFG_ARM_SMMU_DRIVER_NUM_SMR_OVERRIDE (12)

// An array of context bank interrupt definitions discovered during device
// enumeration.  |irq_cnt| reports the total number of valid members while
// |irqs|, while |global_irq_cnt| reports the total number of global
// interrupts in |irqs|, with the rest of the interrupts being context bank
// interrupts.  So:
//
// + IRQs [0, global_irq_cnt) are global interrupts
// + IRQs [global_irq_cnt, irq_cnt) are context bank interrupts
//   corresponding to context banks [0, irq_cnt - global_irq_cnt)
#define ZBI_DCFG_ARM_SMMU_DRIVER_IRQ_CNT (16)
#define ZBI_DCFG_ARM_SMMU_DRIVER_GLOBAL_IRQ_CNT (20)
#define ZBI_DCFG_ARM_SMMU_DRIVER_IRQS (24)

// An array of stream IDs (defined using SMR value/mask encoding) which are
// being "handed off" from the bootloader to the HLOS.  Downstream drivers
// should take care to preserve the existing configuration for these stream
// ids.  |handoff_smr_cnt| reports the number of members of |handoff_smrs|
// which are valid.
//
// Note:  Stream ID ranges in SMRs are encoded using two 16 bit fields, a
// value field and a mask field.  These two fields are packed into a single
// 32-bit integer, with the mask packed into the upper 16 bits, and the
// value packed into the lower 16 bits.
#define ZBI_DCFG_ARM_SMMU_DRIVER_HANDOFF_SMR_CNT (1048)
#define ZBI_DCFG_ARM_SMMU_DRIVER_HANDOFF_SMRS (1052)

#define ZBI_KERNEL_DRIVER_CPU_ENERGY_MODEL_MAX_OPPS (32)

// Operating Performance Point (OPP) definition for CPU Energy Model.
#define ZBI_CPU_ENERGY_MODEL_OPP_SIZEOF (16)

// CPU frequency in kilohertz (kHz).
#define ZBI_CPU_ENERGY_MODEL_OPP_FREQUENCY_KHZ (0)

// Relative compute capacity scale (unit-less, normalized relative to max capacity across power
// domains, e.g. 1024 or 1000 scale).
#define ZBI_CPU_ENERGY_MODEL_OPP_CAPACITY (4)

// Power consumption in microwatts (uW).
#define ZBI_CPU_ENERGY_MODEL_OPP_POWER_UW (8)

// Operating voltage in millivolts (mV).
#define ZBI_CPU_ENERGY_MODEL_OPP_VOLTAGE_MV (12)

// Performance / Power Domain definition for CPU Energy Model.
//
// The ZBI_KERNEL_DRIVER_IRIS_POWER / ZBI_KERNEL_DRIVER_CPU_ENERGY_MODEL payload
// consists of one or more `CpuEnergyModelDomain` entries. The length of the
// item is `sizeof(zbi_cpu_energy_model_domain_t)` times the number of domains.
#define ZBI_CPU_ENERGY_MODEL_DOMAIN_SIZEOF (536)

#define ZBI_CPU_ENERGY_MODEL_DOMAIN_CPU_MASK (0)
#define ZBI_CPU_ENERGY_MODEL_DOMAIN_MAX_RATE (8)
#define ZBI_CPU_ENERGY_MODEL_DOMAIN_DOMAIN_ID (16)
#define ZBI_CPU_ENERGY_MODEL_DOMAIN_OPP_COUNT (20)
#define ZBI_CPU_ENERGY_MODEL_DOMAIN_OPPS (24)

#endif  // ZBI_ASM_DRIVER_CONFIG_H_
