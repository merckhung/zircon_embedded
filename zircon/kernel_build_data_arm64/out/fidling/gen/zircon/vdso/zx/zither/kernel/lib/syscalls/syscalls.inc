// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library zx by zither, a Fuchsia platform
// tool.

KERNEL_SYSCALL(bti_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (iommu, options, bti_id, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t iommu, uint32_t options,
                uint64_t bti_id, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(bti_pin, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8,
               (handle, options, vmo, offset, size, addrs, num_addrs, pmt),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmo, uint64_t offset,
                uint64_t size, zx_paddr_t* addrs, size_t num_addrs,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* pmt))

KERNEL_SYSCALL(bti_release_quarantine, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

VDSO_SYSCALL(cache_flush, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (addr, size, options),
             (const void* addr, size_t size, uint32_t options))

VDSO_SYSCALL(channel_call, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
             (handle, options, deadline, args, actual_bytes, actual_handles),
             (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
              zx_instant_mono_t deadline, const zx_channel_call_args_t* args,
              uint32_t* actual_bytes, uint32_t* actual_handles))

VDSO_SYSCALL(channel_call_etc, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
             (handle, options, deadline, args, actual_bytes, actual_handles),
             (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
              zx_instant_mono_t deadline, zx_channel_call_etc_args_t* args, uint32_t* actual_bytes,
              uint32_t* actual_handles))

INTERNAL_SYSCALL(channel_call_etc_finish, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
                 (deadline, args, actual_bytes, actual_handles),
                 (zx_instant_mono_t deadline, zx_channel_call_etc_args_t* args,
                  uint32_t* actual_bytes, uint32_t* actual_handles))

INTERNAL_SYSCALL(channel_call_etc_noretry, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
                 (handle, options, deadline, args, actual_bytes, actual_handles),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                  zx_instant_mono_t deadline, zx_channel_call_etc_args_t* args,
                  uint32_t* actual_bytes, uint32_t* actual_handles))

INTERNAL_SYSCALL(channel_call_finish, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
                 (deadline, args, actual_bytes, actual_handles),
                 (zx_instant_mono_t deadline, const zx_channel_call_args_t* args,
                  uint32_t* actual_bytes, uint32_t* actual_handles))

INTERNAL_SYSCALL(channel_call_noretry, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
                 (handle, options, deadline, args, actual_bytes, actual_handles),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                  zx_instant_mono_t deadline, const zx_channel_call_args_t* args,
                  uint32_t* actual_bytes, uint32_t* actual_handles))

KERNEL_SYSCALL(channel_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (options, out0, out1),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out0,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out1))

KERNEL_SYSCALL(
    channel_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8,
    (handle, options, bytes, handles, num_bytes, num_handles, actual_bytes, actual_handles),
    (_ZX_SYSCALL_ANNO(use_handle("FuchsiaUnchecked")) zx_handle_t handle, uint32_t options,
     void* bytes, _ZX_SYSCALL_ANNO(acquire_handle("FuchsiaUnchecked")) zx_handle_t* handles,
     uint32_t num_bytes, uint32_t num_handles, uint32_t* actual_bytes, uint32_t* actual_handles))

KERNEL_SYSCALL(channel_read_etc, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8,
               (handle, options, bytes, handles, num_bytes, num_handles, actual_bytes,
                actual_handles),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                void* bytes, zx_handle_info_t* handles, uint32_t num_bytes, uint32_t num_handles,
                uint32_t* actual_bytes, uint32_t* actual_handles))

KERNEL_SYSCALL(channel_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, options, bytes, num_bytes, handles, num_handles),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                const void* bytes, uint32_t num_bytes,
                _ZX_SYSCALL_ANNO(release_handle("Fuchsia")) const zx_handle_t* handles,
                uint32_t num_handles))

KERNEL_SYSCALL(channel_write_etc, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, options, bytes, num_bytes, handles, num_handles),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                const void* bytes, uint32_t num_bytes, zx_handle_disposition_t* handles,
                uint32_t num_handles))

KERNEL_SYSCALL(clock_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (options, args, out),
               (uint64_t options, const void* args,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

VDSO_SYSCALL(clock_get_boot, zx_instant_boot_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

INTERNAL_SYSCALL(clock_get_boot_via_kernel, zx_instant_boot_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

KERNEL_SYSCALL(clock_get_details, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, options, details),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t options,
                void* details))

VDSO_SYSCALL(clock_get_details_mapped, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
             (clock_addr, options, details),
             (const void* clock_addr, uint64_t options, void* details))

VDSO_SYSCALL(clock_get_monotonic, zx_instant_mono_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

INTERNAL_SYSCALL(clock_get_monotonic_via_kernel, zx_instant_mono_t, _ZX_SYSCALL_EXTERN_C, 0, (),
                 (void))

KERNEL_SYSCALL(clock_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, now),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_time_t* now))

VDSO_SYSCALL(clock_read_mapped, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (clock_addr, now),
             (const void* clock_addr, zx_time_t* now))

KERNEL_SYSCALL(clock_update, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, options, args),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t options,
                const void* args))

KERNEL_SYSCALL(counter_add, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, value),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, int64_t value))

KERNEL_SYSCALL(counter_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (options, out),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(counter_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, value),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, int64_t* value))

KERNEL_SYSCALL(counter_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, value),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, int64_t value))

KERNEL_SYSCALL(cprng_add_entropy, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (buffer, buffer_size),
               (const void* buffer, size_t buffer_size))

VDSO_SYSCALL(cprng_draw, void, _ZX_SYSCALL_EXTERN_C, 2, (buffer, buffer_size),
             (void* buffer, size_t buffer_size))

INTERNAL_SYSCALL(cprng_draw_once, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (buffer, buffer_size),
                 (void* buffer, size_t buffer_size))

VDSO_SYSCALL(deadline_after, zx_instant_mono_t, _ZX_SYSCALL_EXTERN_C, 1, (nanoseconds),
             (zx_duration_mono_t nanoseconds))

KERNEL_SYSCALL(debug_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, buffer, buffer_size, actual),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, char* buffer,
                size_t buffer_size, size_t* actual))

KERNEL_SYSCALL(debug_send_command, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (resource, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, const char* buffer,
                size_t buffer_size))

KERNEL_SYSCALL(debug_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (buffer, buffer_size),
               (const char* buffer, size_t buffer_size))

KERNEL_SYSCALL(debuglog_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (resource, options, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(debuglog_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, options, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                void* buffer, size_t buffer_size))

KERNEL_SYSCALL(debuglog_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, options, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                const void* buffer, size_t buffer_size))

KERNEL_SYSCALL(event_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (options, out),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(eventpair_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (options, out0, out1),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out0,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out1))

KERNEL_SYSCALL(exception_get_process, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(exception_get_thread, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(fifo_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (elem_count, elem_size, options, out0, out1),
               (size_t elem_count, size_t elem_size, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out0,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out1))

KERNEL_SYSCALL(fifo_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, elem_size, data, data_size, actual_count),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, size_t elem_size,
                void* data, size_t data_size, size_t* actual_count))

KERNEL_SYSCALL(fifo_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, elem_size, data, count, actual_count),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, size_t elem_size,
                const void* data, size_t count, size_t* actual_count))

KERNEL_SYSCALL(futex_get_owner, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (value_ptr, koid),
               (const zx_futex_t* value_ptr, zx_koid_t* koid))

KERNEL_SYSCALL(futex_requeue, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (value_ptr, wake_count, current_value, requeue_ptr, requeue_count,
                new_requeue_owner),
               (const zx_futex_t* value_ptr, uint32_t wake_count, int32_t current_value,
                const zx_futex_t* requeue_ptr, uint32_t requeue_count,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t new_requeue_owner))

KERNEL_SYSCALL(futex_requeue_single_owner, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (value_ptr, current_value, requeue_ptr, requeue_count, new_requeue_owner),
               (const zx_futex_t* value_ptr, int32_t current_value, const zx_futex_t* requeue_ptr,
                uint32_t requeue_count,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t new_requeue_owner))

BLOCKING_SYSCALL(futex_wait, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
                 (value_ptr, current_value, new_futex_owner, deadline),
                 (const zx_futex_t* value_ptr, int32_t current_value,
                  _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t new_futex_owner,
                  zx_instant_mono_t deadline))

KERNEL_SYSCALL(futex_wake, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (value_ptr, wake_count),
               (const zx_futex_t* value_ptr, uint32_t wake_count))

VDSO_SYSCALL(futex_wake_handle_close_thread_exit, void, _ZX_SYSCALL_EXTERN_C __NO_RETURN, 4,
             (value_ptr, wake_count, new_value, close_handle),
             (const zx_futex_t* value_ptr, uint32_t wake_count, int32_t new_value,
              _ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t close_handle))

KERNEL_SYSCALL(futex_wake_single_owner, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (value_ptr),
               (const zx_futex_t* value_ptr))

KERNEL_SYSCALL(guest_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (resource, options, guest_handle, vmar_handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* guest_handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* vmar_handle))

KERNEL_SYSCALL(guest_set_trap, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, kind, addr, size, port_handle, key),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t kind,
                zx_vaddr_t addr, size_t size,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t port_handle, uint64_t key))

VDSO_SYSCALL(handle_check_valid, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
             (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(handle_close, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(handle_close_many, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handles, num_handles),
               (_ZX_SYSCALL_ANNO(release_handle("Fuchsia")) const zx_handle_t* handles,
                size_t num_handles))

KERNEL_SYSCALL(handle_duplicate, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, rights, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_rights_t rights,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(handle_replace, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, rights, out),
               (_ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t handle, zx_rights_t rights,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(interrupt_ack, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(interrupt_bind, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, port_handle, key, options),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t port_handle, uint64_t key,
                uint32_t options))

KERNEL_SYSCALL(interrupt_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (src_obj, src_num, options, out_handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t src_obj, uint32_t src_num,
                uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out_handle))

KERNEL_SYSCALL(interrupt_destroy, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(interrupt_trigger, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (handle, options, timestamp),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                zx_time_t timestamp))

BLOCKING_SYSCALL(interrupt_wait, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, out_timestamp),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                  zx_time_t* out_timestamp))

KERNEL_SYSCALL(iob_allocate_id, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, options, region_index, blob, blob_size, id),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                zx_iob_allocate_id_options_t options, uint32_t region_index, const void* blob,
                size_t blob_size, uint32_t* id))

KERNEL_SYSCALL(iob_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (options, regions, num_regions, ep0_out, ep1_out),
               (uint64_t options, const void* regions, size_t num_regions,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* ep0_out,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* ep1_out))

KERNEL_SYSCALL(iob_create_shared_region, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (options, size, out),
               (uint64_t options, uint64_t size,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(iob_writev, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, options, region_index, vectors, num_vectors),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                zx_iob_write_options_t options, uint32_t region_index, const zx_iovec_t* vectors,
                size_t num_vectors))

KERNEL_SYSCALL(iommu_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (resource, type, desc, desc_size, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t type,
                const void* desc, size_t desc_size,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(ioports_release, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (resource, io_addr, len),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint16_t io_addr,
                uint32_t len))

KERNEL_SYSCALL(ioports_request, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (resource, io_addr, len),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint16_t io_addr,
                uint32_t len))

KERNEL_SYSCALL(job_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (parent_job, options, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t parent_job, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(job_set_critical, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (job, options, process),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t job, uint32_t options,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t process))

KERNEL_SYSCALL(job_set_policy, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, options, topic, policy, policy_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                uint32_t topic, const void* policy, uint32_t policy_size))

KERNEL_SYSCALL(ktrace_control, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (handle, action, options, ptr),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t action,
                uint32_t options, void* ptr))

KERNEL_SYSCALL(ktrace_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, data, offset, data_size, actual),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, void* data,
                uint32_t offset, size_t data_size, size_t* actual))

KERNEL_SYSCALL(membarrier_sync_process_data, void, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

KERNEL_SYSCALL(membarrier_sync_process_insn, void, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

KERNEL_SYSCALL(msi_allocate, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, count, out_allocation),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t count,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out_allocation))

KERNEL_SYSCALL(msi_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, options, msi_id, vmo, vmo_offset, out_interrupt),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                uint32_t msi_id, _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmo,
                size_t vmo_offset,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out_interrupt))

BLOCKING_SYSCALL(nanosleep, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (deadline),
                 (zx_instant_mono_t deadline))

KERNEL_SYSCALL(object_get_child, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (handle, koid, rights, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t koid,
                zx_rights_t rights, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(object_get_info, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, topic, buffer, buffer_size, actual, avail),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t topic,
                void* buffer, size_t buffer_size, size_t* actual, size_t* avail))

KERNEL_SYSCALL(object_get_property, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, property, value, value_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t property,
                void* value, size_t value_size))

KERNEL_SYSCALL(object_set_profile, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, profile, options),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t profile, uint32_t options))

KERNEL_SYSCALL(object_set_property, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, property, value, value_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t property,
                const void* value, size_t value_size))

KERNEL_SYSCALL(object_signal, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, clear_mask, set_mask),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t clear_mask,
                uint32_t set_mask))

KERNEL_SYSCALL(object_signal_peer, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (handle, clear_mask, set_mask),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t clear_mask,
                uint32_t set_mask))

KERNEL_SYSCALL(object_wait_async, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, port, key, signals, options),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t port, uint64_t key,
                zx_signals_t signals, uint32_t options))

BLOCKING_SYSCALL(object_wait_many, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
                 (items, num_items, deadline),
                 (zx_wait_item_t * items, size_t num_items, zx_instant_mono_t deadline))

BLOCKING_SYSCALL(object_wait_one, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
                 (handle, signals, deadline, observed),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_signals_t signals,
                  zx_instant_mono_t deadline, zx_signals_t* observed))

KERNEL_SYSCALL(pager_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (options, out),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(pager_create_vmo, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (pager, options, port, key, size, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager, uint32_t options,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t port, uint64_t key,
                uint64_t size, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(pager_detach_vmo, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (pager, vmo),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmo))

KERNEL_SYSCALL(pager_op_range, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (pager, op, pager_vmo, offset, length, data),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager, uint32_t op,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager_vmo, uint64_t offset,
                uint64_t length, uint64_t data))

KERNEL_SYSCALL(pager_query_dirty_ranges, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8,
               (pager, pager_vmo, offset, length, buffer, buffer_size, actual, avail),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager_vmo, uint64_t offset,
                uint64_t length, void* buffer, size_t buffer_size, size_t* actual, size_t* avail))

KERNEL_SYSCALL(pager_query_vmo_stats, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (pager, pager_vmo, options, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager_vmo, uint32_t options,
                void* buffer, size_t buffer_size))

KERNEL_SYSCALL(pager_supply_pages, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (pager, pager_vmo, offset, length, aux_vmo, aux_offset),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t pager_vmo, uint64_t offset,
                uint64_t length, _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t aux_vmo,
                uint64_t aux_offset))

KERNEL_SYSCALL(pmt_unpin, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(port_cancel, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, source, key),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t source, uint64_t key))

KERNEL_SYSCALL(port_cancel_key, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, options, key),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                uint64_t key))

KERNEL_SYSCALL(port_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (options, out),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(port_queue, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, packet),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                const zx_port_packet_t* packet))

BLOCKING_SYSCALL(port_wait, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, deadline, packet),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                  zx_instant_mono_t deadline, zx_port_packet_t* packet))

KERNEL_SYSCALL(process_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (job, name, name_size, options, proc_handle, vmar_handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t job, const char* name,
                size_t name_size, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* proc_handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* vmar_handle))

KERNEL_SYSCALL(process_create_shared, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (shared_proc, options, name, name_size, proc_handle, restricted_vmar_handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t shared_proc, uint32_t options,
                const char* name, size_t name_size,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* proc_handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* restricted_vmar_handle))

KERNEL_SYSCALL(process_exit, void, _ZX_SYSCALL_EXTERN_C __NO_RETURN, 1, (retcode),
               (int64_t retcode))

BLOCKING_SYSCALL(process_read_memory, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
                 (handle, vaddr, buffer, buffer_size, actual),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vaddr_t vaddr,
                  void* buffer, size_t buffer_size, size_t* actual))

KERNEL_SYSCALL(process_start, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, thread, entry, stack, arg1, arg2),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t thread, zx_vaddr_t entry,
                zx_vaddr_t stack, _ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t arg1,
                uintptr_t arg2))

BLOCKING_SYSCALL(process_write_memory, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
                 (handle, vaddr, buffer, buffer_size, actual),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vaddr_t vaddr,
                  const void* buffer, size_t buffer_size, size_t* actual))

KERNEL_SYSCALL(profile_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (profile_rsrc, options, profile, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t profile_rsrc, uint32_t options,
                const zx_profile_info_t* profile,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(resource_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 7,
               (parent_rsrc, options, base, size, name, name_size, resource_out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t parent_rsrc, uint32_t options,
                uint64_t base, size_t size, const char* name, size_t name_size,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* resource_out))

KERNEL_SYSCALL(restricted_bind_state, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (options, out, out_exception),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out,
                zx_exception_report_t* out_exception))

BLOCKING_SYSCALL(restricted_enter, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
                 (options, vector_table_ptr, context),
                 (uint32_t options, uintptr_t vector_table_ptr, uintptr_t context))

KERNEL_SYSCALL(restricted_kick, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (thread, options),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t thread, uint32_t options))

KERNEL_SYSCALL(restricted_unbind_state, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (options),
               (uint32_t options))

KERNEL_SYSCALL(sampler_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (rsrc, options, config, per_cpu_buffers),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t rsrc, uint64_t options,
                const zx_sampler_config_t* config,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* per_cpu_buffers))

KERNEL_SYSCALL(sampler_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (iobuffer, data, data_size, actual),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t iobuffer, void* data,
                size_t data_size, size_t* actual))

KERNEL_SYSCALL(sampler_start, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (iobuffer),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t iobuffer))

KERNEL_SYSCALL(sampler_stop, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (iobuffer),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t iobuffer))

KERNEL_SYSCALL(smc_call, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, parameters, out_smc_result),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                const zx_smc_parameters_t* parameters, zx_smc_result_t* out_smc_result))

KERNEL_SYSCALL(socket_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (options, out0, out1),
               (uint32_t options, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out0,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out1))

KERNEL_SYSCALL(socket_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, options, buffer, buffer_size, actual),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                void* buffer, size_t buffer_size, size_t* actual))

KERNEL_SYSCALL(socket_set_disposition, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (handle, disposition, disposition_peer),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t disposition,
                uint32_t disposition_peer))

KERNEL_SYSCALL(socket_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, options, buffer, buffer_size, actual),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                const void* buffer, size_t buffer_size, size_t* actual))

KERNEL_SYSCALL(stream_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (options, vmo, seek, out_stream),
               (uint32_t options, _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmo,
                zx_off_t seek, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out_stream))

BLOCKING_SYSCALL(stream_readv, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
                 (handle, options, vectors, num_vectors, actual),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                  zx_iovec_t* vectors, size_t num_vectors, size_t* actual))

BLOCKING_SYSCALL(stream_readv_at, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
                 (handle, options, offset, vectors, num_vectors, actual),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                  zx_off_t offset, zx_iovec_t* vectors, size_t num_vectors, size_t* actual))

KERNEL_SYSCALL(stream_seek, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, whence, offset, out_seek),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                zx_stream_seek_origin_t whence, int64_t offset, zx_off_t* out_seek))

BLOCKING_SYSCALL(stream_writev, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
                 (handle, options, vectors, num_vectors, actual),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                  const zx_iovec_t* vectors, size_t num_vectors, size_t* actual))

BLOCKING_SYSCALL(stream_writev_at, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
                 (handle, options, offset, vectors, num_vectors, actual),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                  zx_off_t offset, const zx_iovec_t* vectors, size_t num_vectors, size_t* actual))

KERNEL_SYSCALL(syscall_next_1, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (arg), (int32_t arg))

KERNEL_SYSCALL(syscall_test_handle_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2,
               (return_value, out),
               (zx_status_t return_value,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(syscall_test_rust_handle, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, value),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t* value))

KERNEL_SYSCALL(syscall_test_rust_inoutptr, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (ptr),
               (int32_t* ptr))

KERNEL_SYSCALL(syscall_test_rust_inptr, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (ptr, value),
               (const int32_t* ptr, int32_t* value))

KERNEL_SYSCALL(syscall_test_rust_outptr, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (value, ptr),
               (int32_t value, int32_t* ptr))

KERNEL_SYSCALL(syscall_test_rust_wrapper, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (a, b, c),
               (int32_t a, int32_t b, int32_t c))

KERNEL_SYSCALL(syscall_test_rust_0, zx_status_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

KERNEL_SYSCALL(syscall_test_rust_1, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (a), (int32_t a))

KERNEL_SYSCALL(syscall_test_rust_2, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (a, b),
               (int32_t a, int32_t b))

KERNEL_SYSCALL(syscall_test_rust_3, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (a, b, c),
               (int32_t a, int32_t b, int32_t c))

KERNEL_SYSCALL(syscall_test_rust_4, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (a, b, c, d),
               (int32_t a, int32_t b, int32_t c, int32_t d))

KERNEL_SYSCALL(syscall_test_rust_5, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5, (a, b, c, d, e),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e))

KERNEL_SYSCALL(syscall_test_rust_6, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6, (a, b, c, d, e, f),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f))

KERNEL_SYSCALL(syscall_test_rust_7, zx_status_t, _ZX_SYSCALL_EXTERN_C, 7, (a, b, c, d, e, f, g),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g))

KERNEL_SYSCALL(syscall_test_rust_8, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8, (a, b, c, d, e, f, g, h),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g,
                int32_t h))

KERNEL_SYSCALL(syscall_test_widening_signed_narrow, int64_t, _ZX_SYSCALL_EXTERN_C, 4, (a, b, c, d),
               (int64_t a, int32_t b, int16_t c, int8_t d))

KERNEL_SYSCALL(syscall_test_widening_signed_wide, int64_t, _ZX_SYSCALL_EXTERN_C, 4, (a, b, c, d),
               (int64_t a, int32_t b, int16_t c, int8_t d))

KERNEL_SYSCALL(syscall_test_widening_unsigned_narrow, uint64_t, _ZX_SYSCALL_EXTERN_C, 4,
               (a, b, c, d), (uint64_t a, uint32_t b, uint16_t c, uint8_t d))

KERNEL_SYSCALL(syscall_test_widening_unsigned_wide, uint64_t, _ZX_SYSCALL_EXTERN_C, 4, (a, b, c, d),
               (uint64_t a, uint32_t b, uint16_t c, uint8_t d))

KERNEL_SYSCALL(syscall_test_wrapper, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (a, b, c),
               (int32_t a, int32_t b, int32_t c))

KERNEL_SYSCALL(syscall_test_0, zx_status_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

KERNEL_SYSCALL(syscall_test_1, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (a), (int32_t a))

KERNEL_SYSCALL(syscall_test_2, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (a, b), (int32_t a, int32_t b))

KERNEL_SYSCALL(syscall_test_3, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (a, b, c),
               (int32_t a, int32_t b, int32_t c))

KERNEL_SYSCALL(syscall_test_4, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (a, b, c, d),
               (int32_t a, int32_t b, int32_t c, int32_t d))

KERNEL_SYSCALL(syscall_test_5, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5, (a, b, c, d, e),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e))

KERNEL_SYSCALL(syscall_test_6, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6, (a, b, c, d, e, f),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f))

KERNEL_SYSCALL(syscall_test_7, zx_status_t, _ZX_SYSCALL_EXTERN_C, 7, (a, b, c, d, e, f, g),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g))

KERNEL_SYSCALL(syscall_test_8, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8, (a, b, c, d, e, f, g, h),
               (int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g,
                int32_t h))

VDSO_SYSCALL(system_get_dcache_line_size, uint32_t, _ZX_SYSCALL_EXTERN_C __CONST, 0, (), (void))

KERNEL_SYSCALL(system_get_event, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (root_job, kind, event),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t root_job, uint32_t kind,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* event))

VDSO_SYSCALL(system_get_features, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (kind, features),
             (uint32_t kind, uint32_t* features))

VDSO_SYSCALL(system_get_num_cpus, uint32_t, _ZX_SYSCALL_EXTERN_C __CONST, 0, (), (void))

VDSO_SYSCALL(system_get_page_size, uint32_t, _ZX_SYSCALL_EXTERN_C __CONST, 0, (), (void))

KERNEL_SYSCALL(system_get_performance_info, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (resource, topic, count, info, output_count),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t topic,
                size_t count, void* info, size_t* output_count))

VDSO_SYSCALL(system_get_physmem, uint64_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

VDSO_SYSCALL(system_get_version_string, zx_string_view_t, _ZX_SYSCALL_EXTERN_C __CONST, 0, (),
             (void))

KERNEL_SYSCALL(system_mexec, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (resource, kernel_vmo, bootimage_vmo),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t kernel_vmo,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t bootimage_vmo))

KERNEL_SYSCALL(system_mexec_payload_get, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (resource, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, void* buffer,
                size_t buffer_size))

KERNEL_SYSCALL(system_powerctl, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (resource, cmd, arg),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t cmd,
                const zx_system_powerctl_arg_t* arg))

KERNEL_SYSCALL(system_set_performance_info, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (resource, topic, info, count),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t topic,
                const void* info, size_t count))

KERNEL_SYSCALL(system_suspend_enter, zx_status_t, _ZX_SYSCALL_EXTERN_C, 7,
               (resource, resume_deadline, options, header, entries, num_entries, actual_entries),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource,
                zx_instant_boot_t resume_deadline, zx_system_suspend_option_t options,
                zx_wake_source_report_header_t* header, zx_wake_source_report_entry_t* entries,
                uint32_t num_entries, uint32_t* actual_entries))

KERNEL_SYSCALL(system_watch_memory_stall, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (resource, kind, threshold, window, event),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, uint32_t kind,
                zx_duration_t threshold, zx_duration_t window,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* event))

KERNEL_SYSCALL(task_create_exception_channel, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (handle, options, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(task_kill, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(task_suspend, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, token),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* token))

KERNEL_SYSCALL(task_suspend_token, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, token),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* token))

KERNEL_SYSCALL(thread_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (process, name, name_size, options, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t process, const char* name,
                size_t name_size, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(thread_exit, void, _ZX_SYSCALL_EXTERN_C __NO_RETURN, 0, (), (void))

KERNEL_SYSCALL(thread_legacy_yield, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (options),
               (uint32_t options))

KERNEL_SYSCALL(thread_raise_exception, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3,
               (options, type, context),
               (uint32_t options, uint32_t type, const zx_exception_context_t* context))

KERNEL_SYSCALL(thread_read_state, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, kind, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t kind,
                void* buffer, size_t buffer_size))

KERNEL_SYSCALL(thread_set_rseq, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (vmo, offset, size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmo, uint64_t offset,
                uint64_t size))

VDSO_SYSCALL(thread_start, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
             (handle, thread_entry, stack, arg1, arg2),
             (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vaddr_t thread_entry,
              zx_vaddr_t stack, uintptr_t arg1, uintptr_t arg2))

KERNEL_SYSCALL(thread_start_regs, zx_status_t, _ZX_SYSCALL_EXTERN_C, 7,
               (handle, pc, sp, arg1, arg2, tp, abi_reg),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t pc,
                uint64_t sp, uint64_t arg1, uint64_t arg2, uint64_t tp, uint64_t abi_reg))

KERNEL_SYSCALL(thread_write_state, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, kind, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t kind,
                const void* buffer, size_t buffer_size))

VDSO_SYSCALL(ticks_get, zx_instant_mono_ticks_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

VDSO_SYSCALL(ticks_get_boot, zx_instant_boot_ticks_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

INTERNAL_SYSCALL(ticks_get_boot_via_kernel, zx_instant_boot_ticks_t, _ZX_SYSCALL_EXTERN_C, 0, (),
                 (void))

INTERNAL_SYSCALL(ticks_get_via_kernel, zx_instant_mono_ticks_t, _ZX_SYSCALL_EXTERN_C, 0, (), (void))

VDSO_SYSCALL(ticks_per_second, zx_ticks_t, _ZX_SYSCALL_EXTERN_C __CONST, 0, (), (void))

KERNEL_SYSCALL(timer_cancel, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(timer_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (options, clock_id, out),
               (uint32_t options, zx_clock_t clock_id,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(timer_set, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, deadline, slack),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_time_t deadline,
                zx_duration_t slack))

KERNEL_SYSCALL(vcpu_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (guest, options, entry, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t guest, uint32_t options,
                zx_vaddr_t entry, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

BLOCKING_SYSCALL(vcpu_enter, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, packet),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle,
                  zx_port_packet_t* packet))

KERNEL_SYSCALL(vcpu_interrupt, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, vector),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t vector))

KERNEL_SYSCALL(vcpu_kick, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(vcpu_read_state, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, kind, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t kind,
                void* buffer, size_t buffer_size))

KERNEL_SYSCALL(vcpu_write_state, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (handle, kind, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t kind,
                const void* buffer, size_t buffer_size))

KERNEL_SYSCALL(vmar_allocate, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (parent_vmar, options, offset, size, child_vmar, child_addr),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t parent_vmar,
                zx_vm_option_t options, size_t offset, size_t size,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* child_vmar,
                zx_vaddr_t* child_addr))

KERNEL_SYSCALL(vmar_destroy, zx_status_t, _ZX_SYSCALL_EXTERN_C, 1, (handle),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle))

KERNEL_SYSCALL(vmar_map, zx_status_t, _ZX_SYSCALL_EXTERN_C, 7,
               (handle, options, vmar_offset, vmo, vmo_offset, len, mapped_addr),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vm_option_t options,
                size_t vmar_offset, _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmo,
                uint64_t vmo_offset, size_t len, zx_vaddr_t* mapped_addr))

KERNEL_SYSCALL(vmar_map_clock, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, options, vmar_offset, clock, len, mapped_addr),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vm_option_t options,
                size_t vmar_offset, _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t clock,
                size_t len, zx_vaddr_t* mapped_addr))

KERNEL_SYSCALL(vmar_map_iob, zx_status_t, _ZX_SYSCALL_EXTERN_C, 8,
               (handle, options, vmar_offset, ep, region_index, region_offset, region_length,
                mapped_addr),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vm_option_t options,
                size_t vmar_offset, _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t ep,
                uint32_t region_index, uint64_t region_offset, size_t region_length,
                zx_vaddr_t* mapped_addr))

KERNEL_SYSCALL(vmar_op_range, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (handle, op, address, size, buffer, buffer_size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t op,
                zx_vaddr_t address, size_t size, void* buffer, size_t buffer_size))

KERNEL_SYSCALL(vmar_protect, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4, (handle, options, addr, len),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vm_option_t options,
                zx_vaddr_t addr, size_t len))

KERNEL_SYSCALL(vmar_unmap, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, addr, len),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, zx_vaddr_t addr,
                size_t len))

VDSO_SYSCALL(vmar_unmap_handle_close_thread_exit, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
             (vmar_handle, addr, size, close_handle),
             (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmar_handle, zx_vaddr_t addr,
              size_t size, _ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t close_handle))

KERNEL_SYSCALL(vmo_create, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (size, options, out),
               (uint64_t size, uint32_t options,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(vmo_create_child, zx_status_t, _ZX_SYSCALL_EXTERN_C, 5,
               (handle, options, offset, size, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t options,
                uint64_t offset, uint64_t size,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(vmo_create_contiguous, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (bti, size, alignment_log2, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t bti, size_t size,
                uint32_t alignment_log2,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(vmo_create_physical, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
               (resource, paddr, size, out),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t resource, zx_paddr_t paddr,
                size_t size, _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(vmo_get_size, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t* size))

KERNEL_SYSCALL(vmo_get_stream_size, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t* size))

BLOCKING_SYSCALL(vmo_op_range, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
                 (handle, op, offset, size, buffer, buffer_size),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t op,
                  uint64_t offset, uint64_t size, void* buffer, size_t buffer_size))

BLOCKING_SYSCALL(vmo_read, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
                 (handle, buffer, offset, buffer_size),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, void* buffer,
                  uint64_t offset, size_t buffer_size))

KERNEL_SYSCALL(vmo_replace_as_executable, zx_status_t, _ZX_SYSCALL_EXTERN_C, 3, (handle, vmex, out),
               (_ZX_SYSCALL_ANNO(release_handle("Fuchsia")) zx_handle_t handle,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t vmex,
                _ZX_SYSCALL_ANNO(acquire_handle("Fuchsia")) zx_handle_t* out))

KERNEL_SYSCALL(vmo_set_cache_policy, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, cache_policy),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint32_t cache_policy))

KERNEL_SYSCALL(vmo_set_size, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t size))

KERNEL_SYSCALL(vmo_set_stream_size, zx_status_t, _ZX_SYSCALL_EXTERN_C, 2, (handle, size),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, uint64_t size))

KERNEL_SYSCALL(vmo_transfer_data, zx_status_t, _ZX_SYSCALL_EXTERN_C, 6,
               (dst_vmo, options, offset, length, src_vmo, src_offset),
               (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t dst_vmo, uint32_t options,
                uint64_t offset, uint64_t length,
                _ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t src_vmo, uint64_t src_offset))

BLOCKING_SYSCALL(vmo_write, zx_status_t, _ZX_SYSCALL_EXTERN_C, 4,
                 (handle, buffer, offset, buffer_size),
                 (_ZX_SYSCALL_ANNO(use_handle("Fuchsia")) zx_handle_t handle, const void* buffer,
                  uint64_t offset, size_t buffer_size))
