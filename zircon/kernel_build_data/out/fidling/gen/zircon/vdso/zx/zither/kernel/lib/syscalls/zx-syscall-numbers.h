// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library zx by zither, a Fuchsia platform
// tool.

#ifndef LIB_SYSCALLS_ZX_SYSCALL_NUMBERS_H_
#define LIB_SYSCALLS_ZX_SYSCALL_NUMBERS_H_

#define ZX_SYS_bti_create 0
#define ZX_SYS_bti_pin 1
#define ZX_SYS_bti_release_quarantine 2
#define ZX_SYS_channel_call_etc_finish 3
#define ZX_SYS_channel_call_etc_noretry 4
#define ZX_SYS_channel_call_finish 5
#define ZX_SYS_channel_call_noretry 6
#define ZX_SYS_channel_create 7
#define ZX_SYS_channel_read 8
#define ZX_SYS_channel_read_etc 9
#define ZX_SYS_channel_write 10
#define ZX_SYS_channel_write_etc 11
#define ZX_SYS_clock_create 12
#define ZX_SYS_clock_get_boot_via_kernel 13
#define ZX_SYS_clock_get_details 14
#define ZX_SYS_clock_get_monotonic_via_kernel 15
#define ZX_SYS_clock_read 16
#define ZX_SYS_clock_update 17
#define ZX_SYS_counter_add 18
#define ZX_SYS_counter_create 19
#define ZX_SYS_counter_read 20
#define ZX_SYS_counter_write 21
#define ZX_SYS_cprng_add_entropy 22
#define ZX_SYS_cprng_draw_once 23
#define ZX_SYS_debug_read 24
#define ZX_SYS_debug_send_command 25
#define ZX_SYS_debug_write 26
#define ZX_SYS_debuglog_create 27
#define ZX_SYS_debuglog_read 28
#define ZX_SYS_debuglog_write 29
#define ZX_SYS_event_create 30
#define ZX_SYS_eventpair_create 31
#define ZX_SYS_exception_get_process 32
#define ZX_SYS_exception_get_thread 33
#define ZX_SYS_fifo_create 34
#define ZX_SYS_fifo_read 35
#define ZX_SYS_fifo_write 36
#define ZX_SYS_futex_get_owner 37
#define ZX_SYS_futex_requeue 38
#define ZX_SYS_futex_requeue_single_owner 39
#define ZX_SYS_futex_wait 40
#define ZX_SYS_futex_wake 41
#define ZX_SYS_futex_wake_single_owner 42
#define ZX_SYS_guest_create 43
#define ZX_SYS_guest_set_trap 44
#define ZX_SYS_handle_close 45
#define ZX_SYS_handle_close_many 46
#define ZX_SYS_handle_duplicate 47
#define ZX_SYS_handle_replace 48
#define ZX_SYS_interrupt_ack 49
#define ZX_SYS_interrupt_bind 50
#define ZX_SYS_interrupt_create 51
#define ZX_SYS_interrupt_destroy 52
#define ZX_SYS_interrupt_trigger 53
#define ZX_SYS_interrupt_wait 54
#define ZX_SYS_iob_allocate_id 55
#define ZX_SYS_iob_create 56
#define ZX_SYS_iob_create_shared_region 57
#define ZX_SYS_iob_writev 58
#define ZX_SYS_iommu_create 59
#define ZX_SYS_ioports_release 60
#define ZX_SYS_ioports_request 61
#define ZX_SYS_job_create 62
#define ZX_SYS_job_set_critical 63
#define ZX_SYS_job_set_policy 64
#define ZX_SYS_ktrace_control 65
#define ZX_SYS_ktrace_read 66
#define ZX_SYS_membarrier_sync_process_data 67
#define ZX_SYS_membarrier_sync_process_insn 68
#define ZX_SYS_msi_allocate 69
#define ZX_SYS_msi_create 70
#define ZX_SYS_nanosleep 71
#define ZX_SYS_object_get_child 72
#define ZX_SYS_object_get_info 73
#define ZX_SYS_object_get_property 74
#define ZX_SYS_object_set_profile 75
#define ZX_SYS_object_set_property 76
#define ZX_SYS_object_signal 77
#define ZX_SYS_object_signal_peer 78
#define ZX_SYS_object_wait_async 79
#define ZX_SYS_object_wait_many 80
#define ZX_SYS_object_wait_one 81
#define ZX_SYS_pager_create 82
#define ZX_SYS_pager_create_vmo 83
#define ZX_SYS_pager_detach_vmo 84
#define ZX_SYS_pager_op_range 85
#define ZX_SYS_pager_query_dirty_ranges 86
#define ZX_SYS_pager_query_vmo_stats 87
#define ZX_SYS_pager_supply_pages 88
#define ZX_SYS_pmt_unpin 89
#define ZX_SYS_port_cancel 90
#define ZX_SYS_port_cancel_key 91
#define ZX_SYS_port_create 92
#define ZX_SYS_port_queue 93
#define ZX_SYS_port_wait 94
#define ZX_SYS_process_create 95
#define ZX_SYS_process_create_shared 96
#define ZX_SYS_process_exit 97
#define ZX_SYS_process_read_memory 98
#define ZX_SYS_process_start 99
#define ZX_SYS_process_write_memory 100
#define ZX_SYS_profile_create 101
#define ZX_SYS_resource_create 102
#define ZX_SYS_restricted_bind_state 103
#define ZX_SYS_restricted_enter 104
#define ZX_SYS_restricted_kick 105
#define ZX_SYS_restricted_unbind_state 106
#define ZX_SYS_sampler_create 107
#define ZX_SYS_sampler_read 108
#define ZX_SYS_sampler_start 109
#define ZX_SYS_sampler_stop 110
#define ZX_SYS_smc_call 111
#define ZX_SYS_socket_create 112
#define ZX_SYS_socket_read 113
#define ZX_SYS_socket_set_disposition 114
#define ZX_SYS_socket_write 115
#define ZX_SYS_stream_create 116
#define ZX_SYS_stream_readv 117
#define ZX_SYS_stream_readv_at 118
#define ZX_SYS_stream_seek 119
#define ZX_SYS_stream_writev 120
#define ZX_SYS_stream_writev_at 121
#define ZX_SYS_syscall_next_1 122
#define ZX_SYS_syscall_test_handle_create 123
#define ZX_SYS_syscall_test_rust_handle 124
#define ZX_SYS_syscall_test_rust_inoutptr 125
#define ZX_SYS_syscall_test_rust_inptr 126
#define ZX_SYS_syscall_test_rust_outptr 127
#define ZX_SYS_syscall_test_rust_wrapper 128
#define ZX_SYS_syscall_test_rust_0 129
#define ZX_SYS_syscall_test_rust_1 130
#define ZX_SYS_syscall_test_rust_2 131
#define ZX_SYS_syscall_test_rust_3 132
#define ZX_SYS_syscall_test_rust_4 133
#define ZX_SYS_syscall_test_rust_5 134
#define ZX_SYS_syscall_test_rust_6 135
#define ZX_SYS_syscall_test_rust_7 136
#define ZX_SYS_syscall_test_rust_8 137
#define ZX_SYS_syscall_test_widening_signed_narrow 138
#define ZX_SYS_syscall_test_widening_signed_wide 139
#define ZX_SYS_syscall_test_widening_unsigned_narrow 140
#define ZX_SYS_syscall_test_widening_unsigned_wide 141
#define ZX_SYS_syscall_test_wrapper 142
#define ZX_SYS_syscall_test_0 143
#define ZX_SYS_syscall_test_1 144
#define ZX_SYS_syscall_test_2 145
#define ZX_SYS_syscall_test_3 146
#define ZX_SYS_syscall_test_4 147
#define ZX_SYS_syscall_test_5 148
#define ZX_SYS_syscall_test_6 149
#define ZX_SYS_syscall_test_7 150
#define ZX_SYS_syscall_test_8 151
#define ZX_SYS_system_get_event 152
#define ZX_SYS_system_get_performance_info 153
#define ZX_SYS_system_mexec 154
#define ZX_SYS_system_mexec_payload_get 155
#define ZX_SYS_system_powerctl 156
#define ZX_SYS_system_set_performance_info 157
#define ZX_SYS_system_suspend_enter 158
#define ZX_SYS_system_watch_memory_stall 159
#define ZX_SYS_task_create_exception_channel 160
#define ZX_SYS_task_kill 161
#define ZX_SYS_task_suspend 162
#define ZX_SYS_task_suspend_token 163
#define ZX_SYS_thread_create 164
#define ZX_SYS_thread_exit 165
#define ZX_SYS_thread_legacy_yield 166
#define ZX_SYS_thread_raise_exception 167
#define ZX_SYS_thread_read_state 168
#define ZX_SYS_thread_set_rseq 169
#define ZX_SYS_thread_start_regs 170
#define ZX_SYS_thread_write_state 171
#define ZX_SYS_ticks_get_boot_via_kernel 172
#define ZX_SYS_ticks_get_via_kernel 173
#define ZX_SYS_timer_cancel 174
#define ZX_SYS_timer_create 175
#define ZX_SYS_timer_set 176
#define ZX_SYS_vcpu_create 177
#define ZX_SYS_vcpu_enter 178
#define ZX_SYS_vcpu_interrupt 179
#define ZX_SYS_vcpu_kick 180
#define ZX_SYS_vcpu_read_state 181
#define ZX_SYS_vcpu_write_state 182
#define ZX_SYS_vmar_allocate 183
#define ZX_SYS_vmar_destroy 184
#define ZX_SYS_vmar_map 185
#define ZX_SYS_vmar_map_clock 186
#define ZX_SYS_vmar_map_iob 187
#define ZX_SYS_vmar_op_range 188
#define ZX_SYS_vmar_protect 189
#define ZX_SYS_vmar_unmap 190
#define ZX_SYS_vmo_create 191
#define ZX_SYS_vmo_create_child 192
#define ZX_SYS_vmo_create_contiguous 193
#define ZX_SYS_vmo_create_physical 194
#define ZX_SYS_vmo_get_size 195
#define ZX_SYS_vmo_get_stream_size 196
#define ZX_SYS_vmo_op_range 197
#define ZX_SYS_vmo_read 198
#define ZX_SYS_vmo_replace_as_executable 199
#define ZX_SYS_vmo_set_cache_policy 200
#define ZX_SYS_vmo_set_size 201
#define ZX_SYS_vmo_set_stream_size 202
#define ZX_SYS_vmo_transfer_data 203
#define ZX_SYS_vmo_write 204
#define ZX_SYS_COUNT 205

#ifndef __ASSEMBLER__

// Indexed by syscall number.
inline constexpr const char* kSyscallNames[] = {
    "zx_bti_create",
    "zx_bti_pin",
    "zx_bti_release_quarantine",
    "zx_channel_call_etc_finish",
    "zx_channel_call_etc_noretry",
    "zx_channel_call_finish",
    "zx_channel_call_noretry",
    "zx_channel_create",
    "zx_channel_read",
    "zx_channel_read_etc",
    "zx_channel_write",
    "zx_channel_write_etc",
    "zx_clock_create",
    "zx_clock_get_boot_via_kernel",
    "zx_clock_get_details",
    "zx_clock_get_monotonic_via_kernel",
    "zx_clock_read",
    "zx_clock_update",
    "zx_counter_add",
    "zx_counter_create",
    "zx_counter_read",
    "zx_counter_write",
    "zx_cprng_add_entropy",
    "zx_cprng_draw_once",
    "zx_debug_read",
    "zx_debug_send_command",
    "zx_debug_write",
    "zx_debuglog_create",
    "zx_debuglog_read",
    "zx_debuglog_write",
    "zx_event_create",
    "zx_eventpair_create",
    "zx_exception_get_process",
    "zx_exception_get_thread",
    "zx_fifo_create",
    "zx_fifo_read",
    "zx_fifo_write",
    "zx_futex_get_owner",
    "zx_futex_requeue",
    "zx_futex_requeue_single_owner",
    "zx_futex_wait",
    "zx_futex_wake",
    "zx_futex_wake_single_owner",
    "zx_guest_create",
    "zx_guest_set_trap",
    "zx_handle_close",
    "zx_handle_close_many",
    "zx_handle_duplicate",
    "zx_handle_replace",
    "zx_interrupt_ack",
    "zx_interrupt_bind",
    "zx_interrupt_create",
    "zx_interrupt_destroy",
    "zx_interrupt_trigger",
    "zx_interrupt_wait",
    "zx_iob_allocate_id",
    "zx_iob_create",
    "zx_iob_create_shared_region",
    "zx_iob_writev",
    "zx_iommu_create",
    "zx_ioports_release",
    "zx_ioports_request",
    "zx_job_create",
    "zx_job_set_critical",
    "zx_job_set_policy",
    "zx_ktrace_control",
    "zx_ktrace_read",
    "zx_membarrier_sync_process_data",
    "zx_membarrier_sync_process_insn",
    "zx_msi_allocate",
    "zx_msi_create",
    "zx_nanosleep",
    "zx_object_get_child",
    "zx_object_get_info",
    "zx_object_get_property",
    "zx_object_set_profile",
    "zx_object_set_property",
    "zx_object_signal",
    "zx_object_signal_peer",
    "zx_object_wait_async",
    "zx_object_wait_many",
    "zx_object_wait_one",
    "zx_pager_create",
    "zx_pager_create_vmo",
    "zx_pager_detach_vmo",
    "zx_pager_op_range",
    "zx_pager_query_dirty_ranges",
    "zx_pager_query_vmo_stats",
    "zx_pager_supply_pages",
    "zx_pmt_unpin",
    "zx_port_cancel",
    "zx_port_cancel_key",
    "zx_port_create",
    "zx_port_queue",
    "zx_port_wait",
    "zx_process_create",
    "zx_process_create_shared",
    "zx_process_exit",
    "zx_process_read_memory",
    "zx_process_start",
    "zx_process_write_memory",
    "zx_profile_create",
    "zx_resource_create",
    "zx_restricted_bind_state",
    "zx_restricted_enter",
    "zx_restricted_kick",
    "zx_restricted_unbind_state",
    "zx_sampler_create",
    "zx_sampler_read",
    "zx_sampler_start",
    "zx_sampler_stop",
    "zx_smc_call",
    "zx_socket_create",
    "zx_socket_read",
    "zx_socket_set_disposition",
    "zx_socket_write",
    "zx_stream_create",
    "zx_stream_readv",
    "zx_stream_readv_at",
    "zx_stream_seek",
    "zx_stream_writev",
    "zx_stream_writev_at",
    "zx_syscall_next_1",
    "zx_syscall_test_handle_create",
    "zx_syscall_test_rust_handle",
    "zx_syscall_test_rust_inoutptr",
    "zx_syscall_test_rust_inptr",
    "zx_syscall_test_rust_outptr",
    "zx_syscall_test_rust_wrapper",
    "zx_syscall_test_rust_0",
    "zx_syscall_test_rust_1",
    "zx_syscall_test_rust_2",
    "zx_syscall_test_rust_3",
    "zx_syscall_test_rust_4",
    "zx_syscall_test_rust_5",
    "zx_syscall_test_rust_6",
    "zx_syscall_test_rust_7",
    "zx_syscall_test_rust_8",
    "zx_syscall_test_widening_signed_narrow",
    "zx_syscall_test_widening_signed_wide",
    "zx_syscall_test_widening_unsigned_narrow",
    "zx_syscall_test_widening_unsigned_wide",
    "zx_syscall_test_wrapper",
    "zx_syscall_test_0",
    "zx_syscall_test_1",
    "zx_syscall_test_2",
    "zx_syscall_test_3",
    "zx_syscall_test_4",
    "zx_syscall_test_5",
    "zx_syscall_test_6",
    "zx_syscall_test_7",
    "zx_syscall_test_8",
    "zx_system_get_event",
    "zx_system_get_performance_info",
    "zx_system_mexec",
    "zx_system_mexec_payload_get",
    "zx_system_powerctl",
    "zx_system_set_performance_info",
    "zx_system_suspend_enter",
    "zx_system_watch_memory_stall",
    "zx_task_create_exception_channel",
    "zx_task_kill",
    "zx_task_suspend",
    "zx_task_suspend_token",
    "zx_thread_create",
    "zx_thread_exit",
    "zx_thread_legacy_yield",
    "zx_thread_raise_exception",
    "zx_thread_read_state",
    "zx_thread_set_rseq",
    "zx_thread_start_regs",
    "zx_thread_write_state",
    "zx_ticks_get_boot_via_kernel",
    "zx_ticks_get_via_kernel",
    "zx_timer_cancel",
    "zx_timer_create",
    "zx_timer_set",
    "zx_vcpu_create",
    "zx_vcpu_enter",
    "zx_vcpu_interrupt",
    "zx_vcpu_kick",
    "zx_vcpu_read_state",
    "zx_vcpu_write_state",
    "zx_vmar_allocate",
    "zx_vmar_destroy",
    "zx_vmar_map",
    "zx_vmar_map_clock",
    "zx_vmar_map_iob",
    "zx_vmar_op_range",
    "zx_vmar_protect",
    "zx_vmar_unmap",
    "zx_vmo_create",
    "zx_vmo_create_child",
    "zx_vmo_create_contiguous",
    "zx_vmo_create_physical",
    "zx_vmo_get_size",
    "zx_vmo_get_stream_size",
    "zx_vmo_op_range",
    "zx_vmo_read",
    "zx_vmo_replace_as_executable",
    "zx_vmo_set_cache_policy",
    "zx_vmo_set_size",
    "zx_vmo_set_stream_size",
    "zx_vmo_transfer_data",
    "zx_vmo_write",
};

#endif  // #ifndef __ASSEMBLER__

#endif  // LIB_SYSCALLS_ZX_SYSCALL_NUMBERS_H_
