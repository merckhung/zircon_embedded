// Copyright 2026 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library zx by zither, a Fuchsia platform tool.

#[allow(unused_imports)]
use crate::object::HandleValue;
#[allow(unused_imports)]
use crate::user_copy::{UserInOutPtr, UserInPtr, UserOutPtr};
#[allow(unused_imports)]
use zx_status::Status;
#[allow(unused_imports)]
use zx_types::*;

#[allow(non_camel_case_types)]
pub type bti_create = extern "C" fn(HandleValue, u32, u64, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type bti_pin = extern "C" fn(HandleValue, u32, HandleValue, u64, u64, UserOutPtr<zx_paddr_t>,
                                 usize, &mut HandleValue)
                       ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type bti_release_quarantine = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type cache_flush = extern "C" fn(UserInPtr<u8>, usize, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_call =
    extern "C" fn(HandleValue, u32, zx_instant_mono_t, UserInPtr<zx_channel_call_args_t>,
                  UserOutPtr<u32>, UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_call_etc =
    extern "C" fn(HandleValue, u32, zx_instant_mono_t, UserInOutPtr<zx_channel_call_etc_args_t>,
                  UserOutPtr<u32>, UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_call_etc_finish =
    extern "C" fn(zx_instant_mono_t, UserInOutPtr<zx_channel_call_etc_args_t>, UserOutPtr<u32>,
                  UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_call_etc_noretry =
    extern "C" fn(HandleValue, u32, zx_instant_mono_t, UserInOutPtr<zx_channel_call_etc_args_t>,
                  UserOutPtr<u32>, UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_call_finish = extern "C" fn(zx_instant_mono_t, UserInPtr<zx_channel_call_args_t>,
                                             UserOutPtr<u32>, UserOutPtr<u32>)
                                   ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_call_noretry =
    extern "C" fn(HandleValue, u32, zx_instant_mono_t, UserInPtr<zx_channel_call_args_t>,
                  UserOutPtr<u32>, UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_create =
    extern "C" fn(u32, &mut HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_read = extern "C" fn(HandleValue, u32, UserOutPtr<u8>, UserOutPtr<zx_handle_t>,
                                      u32, u32, UserOutPtr<u32>, UserOutPtr<u32>)
                            ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_read_etc =
    extern "C" fn(HandleValue, u32, UserOutPtr<u8>, UserOutPtr<zx_handle_info_t>, u32, u32,
                  UserOutPtr<u32>, UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_write =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, u32, UserInPtr<zx_handle_t>, u32)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type channel_write_etc =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, u32, UserInOutPtr<zx_handle_disposition_t>, u32)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type clock_create = extern "C" fn(u64, UserInPtr<u8>, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type clock_get_boot = extern "C" fn()->zx_instant_boot_t;

#[allow(non_camel_case_types)]
pub type clock_get_boot_via_kernel = extern "C" fn()->zx_instant_boot_t;

#[allow(non_camel_case_types)]
pub type clock_get_details = extern "C" fn(HandleValue, u64, UserOutPtr<u8>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type clock_get_details_mapped =
    extern "C" fn(UserInPtr<u8>, u64, UserOutPtr<u8>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type clock_get_monotonic = extern "C" fn()->zx_instant_mono_t;

#[allow(non_camel_case_types)]
pub type clock_get_monotonic_via_kernel = extern "C" fn()->zx_instant_mono_t;

#[allow(non_camel_case_types)]
pub type clock_read = extern "C" fn(HandleValue, UserOutPtr<zx_time_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type clock_read_mapped =
    extern "C" fn(UserInPtr<u8>, UserOutPtr<zx_time_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type clock_update = extern "C" fn(HandleValue, u64, UserInPtr<u8>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type counter_add = extern "C" fn(HandleValue, i64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type counter_create = extern "C" fn(u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type counter_read = extern "C" fn(HandleValue, UserOutPtr<i64>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type counter_write = extern "C" fn(HandleValue, i64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type cprng_add_entropy = extern "C" fn(UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type cprng_draw = extern "C" fn(UserOutPtr<u8>, usize)->();

#[allow(non_camel_case_types)]
pub type cprng_draw_once = extern "C" fn(UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type deadline_after = extern "C" fn(zx_duration_mono_t)->zx_instant_mono_t;

#[allow(non_camel_case_types)]
pub type debug_read =
    extern "C" fn(HandleValue, UserOutPtr<u8>, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type debug_send_command = extern "C" fn(HandleValue, UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type debug_write = extern "C" fn(UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type debuglog_create = extern "C" fn(HandleValue, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type debuglog_read = extern "C" fn(HandleValue, u32, UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type debuglog_write = extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type event_create = extern "C" fn(u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type eventpair_create =
    extern "C" fn(u32, &mut HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type exception_get_process = extern "C" fn(HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type exception_get_thread = extern "C" fn(HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type fifo_create =
    extern "C" fn(usize, usize, u32, &mut HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type fifo_read =
    extern "C" fn(HandleValue, usize, UserOutPtr<u8>, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type fifo_write =
    extern "C" fn(HandleValue, usize, UserInPtr<u8>, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type futex_get_owner =
    extern "C" fn(UserInPtr<zx_futex_t>, UserOutPtr<zx_koid_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type futex_requeue =
    extern "C" fn(UserInPtr<zx_futex_t>, u32, i32, UserInPtr<zx_futex_t>, u32, HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type futex_requeue_single_owner =
    extern "C" fn(UserInPtr<zx_futex_t>, i32, UserInPtr<zx_futex_t>, u32, HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type futex_wait =
    extern "C" fn(UserInPtr<zx_futex_t>, i32, HandleValue, zx_instant_mono_t)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type futex_wake = extern "C" fn(UserInPtr<zx_futex_t>, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type futex_wake_handle_close_thread_exit =
    extern "C" fn(UserInPtr<zx_futex_t>, u32, i32, HandleValue)->();

#[allow(non_camel_case_types)]
pub type futex_wake_single_owner = extern "C" fn(UserInPtr<zx_futex_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type guest_create =
    extern "C" fn(HandleValue, u32, &mut HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type guest_set_trap =
    extern "C" fn(HandleValue, u32, zx_vaddr_t, usize, HandleValue, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type handle_check_valid = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type handle_close = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type handle_close_many = extern "C" fn(UserInPtr<zx_handle_t>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type handle_duplicate =
    extern "C" fn(HandleValue, zx_rights_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type handle_replace =
    extern "C" fn(HandleValue, zx_rights_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type interrupt_ack = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type interrupt_bind = extern "C" fn(HandleValue, HandleValue, u64, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type interrupt_create =
    extern "C" fn(HandleValue, u32, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type interrupt_destroy = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type interrupt_trigger = extern "C" fn(HandleValue, u32, zx_time_t)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type interrupt_wait = extern "C" fn(HandleValue, UserOutPtr<zx_time_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type iob_allocate_id = extern "C" fn(HandleValue, zx_iob_allocate_id_options_t, u32,
                                         UserInPtr<u8>, usize, UserOutPtr<u32>)
                               ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type iob_create = extern "C" fn(u64, UserInPtr<u8>, usize, &mut HandleValue, &mut HandleValue)
                          ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type iob_create_shared_region = extern "C" fn(u64, u64, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type iob_writev =
    extern "C" fn(HandleValue, zx_iob_write_options_t, u32, UserInPtr<zx_iovec_t>, usize)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type iommu_create =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type ioports_release = extern "C" fn(HandleValue, u16, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type ioports_request = extern "C" fn(HandleValue, u16, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type job_create = extern "C" fn(HandleValue, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type job_set_critical = extern "C" fn(HandleValue, u32, HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type job_set_policy =
    extern "C" fn(HandleValue, u32, u32, UserInPtr<u8>, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type ktrace_control =
    extern "C" fn(HandleValue, u32, u32, UserInOutPtr<u8>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type ktrace_read =
    extern "C" fn(HandleValue, UserOutPtr<u8>, u32, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type membarrier_sync_process_data = extern "C" fn()->();

#[allow(non_camel_case_types)]
pub type membarrier_sync_process_insn = extern "C" fn()->();

#[allow(non_camel_case_types)]
pub type msi_allocate = extern "C" fn(HandleValue, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type msi_create =
    extern "C" fn(HandleValue, u32, u32, HandleValue, usize, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type nanosleep = extern "C" fn(zx_instant_mono_t)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_get_child =
    extern "C" fn(HandleValue, u64, zx_rights_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_get_info =
    extern "C" fn(HandleValue, u32, UserOutPtr<u8>, usize, UserOutPtr<usize>, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_get_property =
    extern "C" fn(HandleValue, u32, UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_set_profile = extern "C" fn(HandleValue, HandleValue, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_set_property =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_signal = extern "C" fn(HandleValue, u32, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_signal_peer = extern "C" fn(HandleValue, u32, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_wait_async =
    extern "C" fn(HandleValue, HandleValue, u64, zx_signals_t, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_wait_many =
    extern "C" fn(UserInOutPtr<zx_wait_item_t>, usize, zx_instant_mono_t)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type object_wait_one =
    extern "C" fn(HandleValue, zx_signals_t, zx_instant_mono_t, UserOutPtr<zx_signals_t>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_create = extern "C" fn(u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_create_vmo =
    extern "C" fn(HandleValue, u32, HandleValue, u64, u64, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_detach_vmo = extern "C" fn(HandleValue, HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_op_range =
    extern "C" fn(HandleValue, u32, HandleValue, u64, u64, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_query_dirty_ranges =
    extern "C" fn(HandleValue, HandleValue, u64, u64, UserOutPtr<u8>, usize, UserOutPtr<usize>,
                  UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_query_vmo_stats =
    extern "C" fn(HandleValue, HandleValue, u32, UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pager_supply_pages =
    extern "C" fn(HandleValue, HandleValue, u64, u64, HandleValue, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type pmt_unpin = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type port_cancel = extern "C" fn(HandleValue, HandleValue, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type port_cancel_key = extern "C" fn(HandleValue, u32, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type port_create = extern "C" fn(u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type port_queue = extern "C" fn(HandleValue, UserInPtr<zx_port_packet_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type port_wait =
    extern "C" fn(HandleValue, zx_instant_mono_t, UserOutPtr<zx_port_packet_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type process_create =
    extern "C" fn(HandleValue, UserInPtr<u8>, usize, u32, &mut HandleValue, &mut HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type process_create_shared =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize, &mut HandleValue, &mut HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type process_exit = extern "C" fn(i64)->();

#[allow(non_camel_case_types)]
pub type process_read_memory =
    extern "C" fn(HandleValue, zx_vaddr_t, UserOutPtr<u8>, usize, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type process_start =
    extern "C" fn(HandleValue, HandleValue, zx_vaddr_t, zx_vaddr_t, HandleValue, usize)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type process_write_memory =
    extern "C" fn(HandleValue, zx_vaddr_t, UserInPtr<u8>, usize, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type profile_create =
    extern "C" fn(HandleValue, u32, UserInPtr<zx_profile_info_t>, &mut HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type resource_create =
    extern "C" fn(HandleValue, u32, u64, usize, UserInPtr<u8>, usize, &mut HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type restricted_bind_state =
    extern "C" fn(u32, &mut HandleValue, UserOutPtr<zx_exception_report_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type restricted_enter = extern "C" fn(u32, usize, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type restricted_kick = extern "C" fn(HandleValue, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type restricted_unbind_state = extern "C" fn(u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type sampler_create =
    extern "C" fn(HandleValue, u64, UserInPtr<zx_sampler_config_t>, &mut HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type sampler_read =
    extern "C" fn(HandleValue, UserOutPtr<u8>, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type sampler_start = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type sampler_stop = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type smc_call =
    extern "C" fn(HandleValue, UserInPtr<zx_smc_parameters_t>, UserOutPtr<zx_smc_result_t>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type socket_create = extern "C" fn(u32, &mut HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type socket_read =
    extern "C" fn(HandleValue, u32, UserOutPtr<u8>, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type socket_set_disposition = extern "C" fn(HandleValue, u32, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type socket_write =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type stream_create =
    extern "C" fn(u32, HandleValue, zx_off_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type stream_readv =
    extern "C" fn(HandleValue, u32, UserOutPtr<zx_iovec_t>, usize, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type stream_readv_at =
    extern "C" fn(HandleValue, u32, zx_off_t, UserOutPtr<zx_iovec_t>, usize, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type stream_seek =
    extern "C" fn(HandleValue, zx_stream_seek_origin_t, i64, UserOutPtr<zx_off_t>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type stream_writev =
    extern "C" fn(HandleValue, u32, UserInPtr<zx_iovec_t>, usize, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type stream_writev_at =
    extern "C" fn(HandleValue, u32, zx_off_t, UserInPtr<zx_iovec_t>, usize, UserOutPtr<usize>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_next_1 = extern "C" fn(i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_handle_create =
    extern "C" fn(zx_status_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_handle = extern "C" fn(HandleValue, UserOutPtr<u32>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_inoutptr = extern "C" fn(UserInOutPtr<i32>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_inptr =
    extern "C" fn(UserInPtr<i32>, UserOutPtr<i32>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_outptr = extern "C" fn(i32, UserOutPtr<i32>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_wrapper = extern "C" fn(i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_0 = extern "C" fn()->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_1 = extern "C" fn(i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_2 = extern "C" fn(i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_3 = extern "C" fn(i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_4 = extern "C" fn(i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_5 = extern "C" fn(i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_6 = extern "C" fn(i32, i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_7 = extern "C" fn(i32, i32, i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_rust_8 =
    extern "C" fn(i32, i32, i32, i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_widening_signed_narrow = extern "C" fn(i64, i32, i16, i8)->i64;

#[allow(non_camel_case_types)]
pub type syscall_test_widening_signed_wide = extern "C" fn(i64, i32, i16, i8)->i64;

#[allow(non_camel_case_types)]
pub type syscall_test_widening_unsigned_narrow = extern "C" fn(u64, u32, u16, u8)->u64;

#[allow(non_camel_case_types)]
pub type syscall_test_widening_unsigned_wide = extern "C" fn(u64, u32, u16, u8)->u64;

#[allow(non_camel_case_types)]
pub type syscall_test_wrapper = extern "C" fn(i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_0 = extern "C" fn()->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_1 = extern "C" fn(i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_2 = extern "C" fn(i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_3 = extern "C" fn(i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_4 = extern "C" fn(i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_5 = extern "C" fn(i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_6 = extern "C" fn(i32, i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_7 = extern "C" fn(i32, i32, i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type syscall_test_8 = extern "C" fn(i32, i32, i32, i32, i32, i32, i32, i32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_get_dcache_line_size = extern "C" fn()->u32;

#[allow(non_camel_case_types)]
pub type system_get_event = extern "C" fn(HandleValue, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_get_features = extern "C" fn(u32, UserOutPtr<u32>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_get_num_cpus = extern "C" fn()->u32;

#[allow(non_camel_case_types)]
pub type system_get_page_size = extern "C" fn()->u32;

#[allow(non_camel_case_types)]
pub type system_get_performance_info =
    extern "C" fn(HandleValue, u32, usize, UserOutPtr<u8>, UserOutPtr<usize>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_get_physmem = extern "C" fn()->u64;

#[allow(non_camel_case_types)]
pub type system_get_version_string = extern "C" fn()->zx_string_view_t;

#[allow(non_camel_case_types)]
pub type system_mexec = extern "C" fn(HandleValue, HandleValue, HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_mexec_payload_get =
    extern "C" fn(HandleValue, UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_powerctl =
    extern "C" fn(HandleValue, u32, UserInPtr<zx_system_powerctl_arg_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_set_performance_info =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_suspend_enter =
    extern "C" fn(HandleValue, zx_instant_boot_t, zx_system_suspend_option_t,
                  UserOutPtr<zx_wake_source_report_header_t>,
                  UserOutPtr<zx_wake_source_report_entry_t>, u32, UserOutPtr<u32>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type system_watch_memory_stall =
    extern "C" fn(HandleValue, u32, zx_duration_t, zx_duration_t, &mut HandleValue)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type task_create_exception_channel =
    extern "C" fn(HandleValue, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type task_kill = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type task_suspend = extern "C" fn(HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type task_suspend_token = extern "C" fn(HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_create =
    extern "C" fn(HandleValue, UserInPtr<u8>, usize, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_exit = extern "C" fn()->();

#[allow(non_camel_case_types)]
pub type thread_legacy_yield = extern "C" fn(u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_raise_exception =
    extern "C" fn(u32, u32, UserInPtr<zx_exception_context_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_read_state =
    extern "C" fn(HandleValue, u32, UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_set_rseq = extern "C" fn(HandleValue, u64, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_start =
    extern "C" fn(HandleValue, zx_vaddr_t, zx_vaddr_t, usize, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_start_regs =
    extern "C" fn(HandleValue, u64, u64, u64, u64, u64, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type thread_write_state =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type ticks_get = extern "C" fn()->zx_instant_mono_ticks_t;

#[allow(non_camel_case_types)]
pub type ticks_get_boot = extern "C" fn()->zx_instant_boot_ticks_t;

#[allow(non_camel_case_types)]
pub type ticks_get_boot_via_kernel = extern "C" fn()->zx_instant_boot_ticks_t;

#[allow(non_camel_case_types)]
pub type ticks_get_via_kernel = extern "C" fn()->zx_instant_mono_ticks_t;

#[allow(non_camel_case_types)]
pub type ticks_per_second = extern "C" fn()->zx_ticks_t;

#[allow(non_camel_case_types)]
pub type timer_cancel = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type timer_create = extern "C" fn(u32, zx_clock_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type timer_set = extern "C" fn(HandleValue, zx_time_t, zx_duration_t)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vcpu_create =
    extern "C" fn(HandleValue, u32, zx_vaddr_t, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vcpu_enter = extern "C" fn(HandleValue, UserOutPtr<zx_port_packet_t>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vcpu_interrupt = extern "C" fn(HandleValue, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vcpu_kick = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vcpu_read_state =
    extern "C" fn(HandleValue, u32, UserOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vcpu_write_state =
    extern "C" fn(HandleValue, u32, UserInPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_allocate = extern "C" fn(HandleValue, zx_vm_option_t, usize, usize, &mut HandleValue,
                                       UserOutPtr<zx_vaddr_t>)
                             ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_destroy = extern "C" fn(HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_map = extern "C" fn(HandleValue, zx_vm_option_t, usize, HandleValue, u64, usize,
                                  UserOutPtr<zx_vaddr_t>)
                        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_map_clock =
    extern "C" fn(HandleValue, zx_vm_option_t, usize, HandleValue, usize, UserOutPtr<zx_vaddr_t>)
        ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_map_iob = extern "C" fn(HandleValue, zx_vm_option_t, usize, HandleValue, u32, u64,
                                      usize, UserOutPtr<zx_vaddr_t>)
                            ->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_op_range =
    extern "C" fn(HandleValue, u32, zx_vaddr_t, usize, UserInOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_protect =
    extern "C" fn(HandleValue, zx_vm_option_t, zx_vaddr_t, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_unmap = extern "C" fn(HandleValue, zx_vaddr_t, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmar_unmap_handle_close_thread_exit =
    extern "C" fn(HandleValue, zx_vaddr_t, usize, HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_create = extern "C" fn(u64, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_create_child =
    extern "C" fn(HandleValue, u32, u64, u64, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_create_contiguous =
    extern "C" fn(HandleValue, usize, u32, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_create_physical =
    extern "C" fn(HandleValue, zx_paddr_t, usize, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_get_size = extern "C" fn(HandleValue, UserOutPtr<u64>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_get_stream_size = extern "C" fn(HandleValue, UserOutPtr<u64>)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_op_range =
    extern "C" fn(HandleValue, u32, u64, u64, UserInOutPtr<u8>, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_read = extern "C" fn(HandleValue, UserOutPtr<u8>, u64, usize)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_replace_as_executable =
    extern "C" fn(HandleValue, HandleValue, &mut HandleValue)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_set_cache_policy = extern "C" fn(HandleValue, u32)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_set_size = extern "C" fn(HandleValue, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_set_stream_size = extern "C" fn(HandleValue, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_transfer_data =
    extern "C" fn(HandleValue, u32, u64, u64, HandleValue, u64)->Result<(), Status>;

#[allow(non_camel_case_types)]
pub type vmo_write = extern "C" fn(HandleValue, UserInPtr<u8>, u64, usize)->Result<(), Status>;
