// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT. Generated from FIDL library zx by zither, a Fuchsia platform
// tool.

extern "C" {

syscall_result wrapper_bti_create(SafeSyscallArgument<zx_handle_t>::RawType arg_iommu,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<uint64_t>::RawType arg_bti_id,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_bti_create(SafeSyscallArgument<zx_handle_t>::RawType arg_iommu,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<uint64_t>::RawType arg_bti_id,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(ZX_SYS_bti_create, pc, &VDso::ValidSyscallPC::bti_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_bti_create(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_iommu),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_bti_id), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_bti_pin(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                               SafeSyscallArgument<uint32_t>::RawType arg_options,
                               SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                               SafeSyscallArgument<uint64_t>::RawType arg_offset,
                               SafeSyscallArgument<uint64_t>::RawType arg_size,
                               SafeSyscallArgument<zx_paddr_t*>::RawType arg_addrs,
                               SafeSyscallArgument<size_t>::RawType arg_num_addrs,
                               SafeSyscallArgument<zx_handle_t*>::RawType arg_pmt, uint64_t pc);
syscall_result wrapper_bti_pin(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                               SafeSyscallArgument<uint32_t>::RawType arg_options,
                               SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                               SafeSyscallArgument<uint64_t>::RawType arg_offset,
                               SafeSyscallArgument<uint64_t>::RawType arg_size,
                               SafeSyscallArgument<zx_paddr_t*>::RawType arg_addrs,
                               SafeSyscallArgument<size_t>::RawType arg_num_addrs,
                               SafeSyscallArgument<zx_handle_t*>::RawType arg_pmt, uint64_t pc) {
  return do_syscall(ZX_SYS_bti_pin, pc, &VDso::ValidSyscallPC::bti_pin,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_pmt = ZX_HANDLE_INVALID;
                      auto result = sys_bti_pin(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmo),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_size),
                          make_user_out_ptr(SafeSyscallArgument<zx_paddr_t*>::Sanitize(arg_addrs)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_num_addrs), &out_handle_pmt);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_pmt))
                              .copy_to_user(out_handle_pmt);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_bti_release_quarantine(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                              uint64_t pc);
syscall_result wrapper_bti_release_quarantine(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                              uint64_t pc) {
  return do_syscall(
      ZX_SYS_bti_release_quarantine, pc, &VDso::ValidSyscallPC::bti_release_quarantine,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result =
            sys_bti_release_quarantine(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
        return result;
      });
}

syscall_result wrapper_channel_call_etc_finish(
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<zx_channel_call_etc_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc);
syscall_result wrapper_channel_call_etc_finish(
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<zx_channel_call_etc_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_call_etc_finish, pc, &VDso::ValidSyscallPC::channel_call_etc_finish,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_call_etc_finish(
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline),
            make_user_inout_ptr(
                SafeSyscallArgument<zx_channel_call_etc_args_t*>::Sanitize(arg_args)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_bytes)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_handles)));
        return result;
      });
}

syscall_result wrapper_channel_call_etc_noretry(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<zx_channel_call_etc_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc);
syscall_result wrapper_channel_call_etc_noretry(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<zx_channel_call_etc_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_call_etc_noretry, pc, &VDso::ValidSyscallPC::channel_call_etc_noretry,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_call_etc_noretry(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline),
            make_user_inout_ptr(
                SafeSyscallArgument<zx_channel_call_etc_args_t*>::Sanitize(arg_args)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_bytes)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_handles)));
        return result;
      });
}

syscall_result wrapper_channel_call_finish(
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<const zx_channel_call_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc);
syscall_result wrapper_channel_call_finish(
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<const zx_channel_call_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_call_finish, pc, &VDso::ValidSyscallPC::channel_call_finish,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_call_finish(
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline),
            make_user_in_ptr(
                SafeSyscallArgument<const zx_channel_call_args_t*>::Sanitize(arg_args)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_bytes)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_handles)));
        return result;
      });
}

syscall_result wrapper_channel_call_noretry(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<const zx_channel_call_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc);
syscall_result wrapper_channel_call_noretry(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
    SafeSyscallArgument<const zx_channel_call_args_t*>::RawType arg_args,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles, uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_call_noretry, pc, &VDso::ValidSyscallPC::channel_call_noretry,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_call_noretry(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline),
            make_user_in_ptr(
                SafeSyscallArgument<const zx_channel_call_args_t*>::Sanitize(arg_args)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_bytes)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_handles)));
        return result;
      });
}

syscall_result wrapper_channel_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                      uint64_t pc);
syscall_result wrapper_channel_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                      uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_create, pc, &VDso::ValidSyscallPC::channel_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out0 = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_out1 = ZX_HANDLE_INVALID;
        auto result = sys_channel_create(SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                         &out_handle_out0, &out_handle_out1);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out0))
                     .copy_to_user(out_handle_out0);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out1))
                     .copy_to_user(out_handle_out1);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_channel_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<void*>::RawType arg_bytes,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_handles,
                                    SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
                                    SafeSyscallArgument<uint32_t>::RawType arg_num_handles,
                                    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
                                    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles,
                                    uint64_t pc);
syscall_result wrapper_channel_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<void*>::RawType arg_bytes,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_handles,
                                    SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
                                    SafeSyscallArgument<uint32_t>::RawType arg_num_handles,
                                    SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
                                    SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_read, pc, &VDso::ValidSyscallPC::channel_read,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_read(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_bytes)),
            make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_handles)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_bytes),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_handles),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_bytes)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_handles)));
        return result;
      });
}

syscall_result wrapper_channel_read_etc(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<void*>::RawType arg_bytes,
                                        SafeSyscallArgument<zx_handle_info_t*>::RawType arg_handles,
                                        SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
                                        SafeSyscallArgument<uint32_t>::RawType arg_num_handles,
                                        SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
                                        SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles,
                                        uint64_t pc);
syscall_result wrapper_channel_read_etc(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<void*>::RawType arg_bytes,
                                        SafeSyscallArgument<zx_handle_info_t*>::RawType arg_handles,
                                        SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
                                        SafeSyscallArgument<uint32_t>::RawType arg_num_handles,
                                        SafeSyscallArgument<uint32_t*>::RawType arg_actual_bytes,
                                        SafeSyscallArgument<uint32_t*>::RawType arg_actual_handles,
                                        uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_read_etc, pc, &VDso::ValidSyscallPC::channel_read_etc,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_read_etc(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_bytes)),
            make_user_out_ptr(SafeSyscallArgument<zx_handle_info_t*>::Sanitize(arg_handles)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_bytes),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_handles),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_bytes)),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_handles)));
        return result;
      });
}

syscall_result wrapper_channel_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<const void*>::RawType arg_bytes,
                                     SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
                                     SafeSyscallArgument<const zx_handle_t*>::RawType arg_handles,
                                     SafeSyscallArgument<uint32_t>::RawType arg_num_handles,
                                     uint64_t pc);
syscall_result wrapper_channel_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<const void*>::RawType arg_bytes,
                                     SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
                                     SafeSyscallArgument<const zx_handle_t*>::RawType arg_handles,
                                     SafeSyscallArgument<uint32_t>::RawType arg_num_handles,
                                     uint64_t pc) {
  return do_syscall(
      ZX_SYS_channel_write, pc, &VDso::ValidSyscallPC::channel_write,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_channel_write(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_bytes)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_bytes),
            make_user_in_ptr(SafeSyscallArgument<const zx_handle_t*>::Sanitize(arg_handles)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_handles));
        return result;
      });
}

syscall_result wrapper_channel_write_etc(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<const void*>::RawType arg_bytes,
    SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
    SafeSyscallArgument<zx_handle_disposition_t*>::RawType arg_handles,
    SafeSyscallArgument<uint32_t>::RawType arg_num_handles, uint64_t pc);
syscall_result wrapper_channel_write_etc(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<const void*>::RawType arg_bytes,
    SafeSyscallArgument<uint32_t>::RawType arg_num_bytes,
    SafeSyscallArgument<zx_handle_disposition_t*>::RawType arg_handles,
    SafeSyscallArgument<uint32_t>::RawType arg_num_handles, uint64_t pc) {
  return do_syscall(ZX_SYS_channel_write_etc, pc, &VDso::ValidSyscallPC::channel_write_etc,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_channel_write_etc(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_bytes)),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_num_bytes),
                          make_user_inout_ptr(
                              SafeSyscallArgument<zx_handle_disposition_t*>::Sanitize(arg_handles)),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_num_handles));
                      return result;
                    });
}

syscall_result wrapper_clock_create(SafeSyscallArgument<uint64_t>::RawType arg_options,
                                    SafeSyscallArgument<const void*>::RawType arg_args,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc);
syscall_result wrapper_clock_create(SafeSyscallArgument<uint64_t>::RawType arg_options,
                                    SafeSyscallArgument<const void*>::RawType arg_args,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_clock_create, pc, &VDso::ValidSyscallPC::clock_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_clock_create(
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_options),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_args)),
                          &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_clock_get_boot_via_kernel(uint64_t pc);
syscall_result wrapper_clock_get_boot_via_kernel(uint64_t pc) {
  return do_syscall(ZX_SYS_clock_get_boot_via_kernel, pc,
                    &VDso::ValidSyscallPC::clock_get_boot_via_kernel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_clock_get_boot_via_kernel();
                      return result;
                    });
}

syscall_result wrapper_clock_get_details(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint64_t>::RawType arg_options,
                                         SafeSyscallArgument<void*>::RawType arg_details,
                                         uint64_t pc);
syscall_result wrapper_clock_get_details(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint64_t>::RawType arg_options,
                                         SafeSyscallArgument<void*>::RawType arg_details,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_clock_get_details, pc, &VDso::ValidSyscallPC::clock_get_details,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_clock_get_details(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_options),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_details)));
                      return result;
                    });
}

syscall_result wrapper_clock_get_monotonic_via_kernel(uint64_t pc);
syscall_result wrapper_clock_get_monotonic_via_kernel(uint64_t pc) {
  return do_syscall(ZX_SYS_clock_get_monotonic_via_kernel, pc,
                    &VDso::ValidSyscallPC::clock_get_monotonic_via_kernel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_clock_get_monotonic_via_kernel();
                      return result;
                    });
}

syscall_result wrapper_clock_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_time_t*>::RawType arg_now, uint64_t pc);
syscall_result wrapper_clock_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_time_t*>::RawType arg_now, uint64_t pc) {
  return do_syscall(ZX_SYS_clock_read, pc, &VDso::ValidSyscallPC::clock_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_clock_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<zx_time_t*>::Sanitize(arg_now)));
                      return result;
                    });
}

syscall_result wrapper_clock_update(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint64_t>::RawType arg_options,
                                    SafeSyscallArgument<const void*>::RawType arg_args,
                                    uint64_t pc);
syscall_result wrapper_clock_update(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint64_t>::RawType arg_options,
                                    SafeSyscallArgument<const void*>::RawType arg_args,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_clock_update, pc, &VDso::ValidSyscallPC::clock_update,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_clock_update(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_options),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_args)));
                      return result;
                    });
}

syscall_result wrapper_counter_add(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<int64_t>::RawType arg_value, uint64_t pc);
syscall_result wrapper_counter_add(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<int64_t>::RawType arg_value, uint64_t pc) {
  return do_syscall(ZX_SYS_counter_add, pc, &VDso::ValidSyscallPC::counter_add,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_counter_add(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                          SafeSyscallArgument<int64_t>::Sanitize(arg_value));
                      return result;
                    });
}

syscall_result wrapper_counter_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                      uint64_t pc);
syscall_result wrapper_counter_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_counter_create, pc, &VDso::ValidSyscallPC::counter_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_counter_create(
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_counter_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<int64_t*>::RawType arg_value, uint64_t pc);
syscall_result wrapper_counter_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<int64_t*>::RawType arg_value, uint64_t pc) {
  return do_syscall(ZX_SYS_counter_read, pc, &VDso::ValidSyscallPC::counter_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_counter_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<int64_t*>::Sanitize(arg_value)));
                      return result;
                    });
}

syscall_result wrapper_counter_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<int64_t>::RawType arg_value, uint64_t pc);
syscall_result wrapper_counter_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<int64_t>::RawType arg_value, uint64_t pc) {
  return do_syscall(ZX_SYS_counter_write, pc, &VDso::ValidSyscallPC::counter_write,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_counter_write(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                            SafeSyscallArgument<int64_t>::Sanitize(arg_value));
                      return result;
                    });
}

syscall_result wrapper_cprng_add_entropy(SafeSyscallArgument<const void*>::RawType arg_buffer,
                                         SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                         uint64_t pc);
syscall_result wrapper_cprng_add_entropy(SafeSyscallArgument<const void*>::RawType arg_buffer,
                                         SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_cprng_add_entropy, pc, &VDso::ValidSyscallPC::cprng_add_entropy,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_cprng_add_entropy(
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_cprng_draw_once(SafeSyscallArgument<void*>::RawType arg_buffer,
                                       SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                       uint64_t pc);
syscall_result wrapper_cprng_draw_once(SafeSyscallArgument<void*>::RawType arg_buffer,
                                       SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_cprng_draw_once, pc, &VDso::ValidSyscallPC::cprng_draw_once,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_cprng_draw_once(
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_debug_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<char*>::RawType arg_buffer,
                                  SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                  SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_debug_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<char*>::RawType arg_buffer,
                                  SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                  SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc) {
  return do_syscall(ZX_SYS_debug_read, pc, &VDso::ValidSyscallPC::debug_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_debug_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<char*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_debug_send_command(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                          SafeSyscallArgument<const char*>::RawType arg_buffer,
                                          SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                          uint64_t pc);
syscall_result wrapper_debug_send_command(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                          SafeSyscallArgument<const char*>::RawType arg_buffer,
                                          SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                          uint64_t pc) {
  return do_syscall(ZX_SYS_debug_send_command, pc, &VDso::ValidSyscallPC::debug_send_command,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_debug_send_command(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          make_user_in_ptr(SafeSyscallArgument<const char*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_debug_write(SafeSyscallArgument<const char*>::RawType arg_buffer,
                                   SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                   uint64_t pc);
syscall_result wrapper_debug_write(SafeSyscallArgument<const char*>::RawType arg_buffer,
                                   SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                   uint64_t pc) {
  return do_syscall(ZX_SYS_debug_write, pc, &VDso::ValidSyscallPC::debug_write,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_debug_write(
                          make_user_in_ptr(SafeSyscallArgument<const char*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_debuglog_create(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                       uint64_t pc);
syscall_result wrapper_debuglog_create(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_debuglog_create, pc, &VDso::ValidSyscallPC::debuglog_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_debuglog_create(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_debuglog_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<void*>::RawType arg_buffer,
                                     SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                     uint64_t pc);
syscall_result wrapper_debuglog_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<void*>::RawType arg_buffer,
                                     SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                     uint64_t pc) {
  return do_syscall(ZX_SYS_debuglog_read, pc, &VDso::ValidSyscallPC::debuglog_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_debuglog_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_debuglog_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<const void*>::RawType arg_buffer,
                                      SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                      uint64_t pc);
syscall_result wrapper_debuglog_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<const void*>::RawType arg_buffer,
                                      SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_debuglog_write, pc, &VDso::ValidSyscallPC::debuglog_write,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_debuglog_write(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_event_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc);
syscall_result wrapper_event_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_event_create, pc, &VDso::ValidSyscallPC::event_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_event_create(
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_eventpair_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                        uint64_t pc);
syscall_result wrapper_eventpair_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                        uint64_t pc) {
  return do_syscall(
      ZX_SYS_eventpair_create, pc, &VDso::ValidSyscallPC::eventpair_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out0 = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_out1 = ZX_HANDLE_INVALID;
        auto result = sys_eventpair_create(SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                           &out_handle_out0, &out_handle_out1);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out0))
                     .copy_to_user(out_handle_out0);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out1))
                     .copy_to_user(out_handle_out1);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_exception_get_process(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                             SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                             uint64_t pc);
syscall_result wrapper_exception_get_process(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                             SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                             uint64_t pc) {
  return do_syscall(ZX_SYS_exception_get_process, pc, &VDso::ValidSyscallPC::exception_get_process,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_exception_get_process(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_exception_get_thread(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                            SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                            uint64_t pc);
syscall_result wrapper_exception_get_thread(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                            SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                            uint64_t pc) {
  return do_syscall(ZX_SYS_exception_get_thread, pc, &VDso::ValidSyscallPC::exception_get_thread,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_exception_get_thread(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_fifo_create(SafeSyscallArgument<size_t>::RawType arg_elem_count,
                                   SafeSyscallArgument<size_t>::RawType arg_elem_size,
                                   SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                   uint64_t pc);
syscall_result wrapper_fifo_create(SafeSyscallArgument<size_t>::RawType arg_elem_count,
                                   SafeSyscallArgument<size_t>::RawType arg_elem_size,
                                   SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                   uint64_t pc) {
  return do_syscall(
      ZX_SYS_fifo_create, pc, &VDso::ValidSyscallPC::fifo_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out0 = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_out1 = ZX_HANDLE_INVALID;
        auto result = sys_fifo_create(SafeSyscallArgument<size_t>::Sanitize(arg_elem_count),
                                      SafeSyscallArgument<size_t>::Sanitize(arg_elem_size),
                                      SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                      &out_handle_out0, &out_handle_out1);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out0))
                     .copy_to_user(out_handle_out0);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out1))
                     .copy_to_user(out_handle_out1);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_fifo_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<size_t>::RawType arg_elem_size,
                                 SafeSyscallArgument<void*>::RawType arg_data,
                                 SafeSyscallArgument<size_t>::RawType arg_data_size,
                                 SafeSyscallArgument<size_t*>::RawType arg_actual_count,
                                 uint64_t pc);
syscall_result wrapper_fifo_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<size_t>::RawType arg_elem_size,
                                 SafeSyscallArgument<void*>::RawType arg_data,
                                 SafeSyscallArgument<size_t>::RawType arg_data_size,
                                 SafeSyscallArgument<size_t*>::RawType arg_actual_count,
                                 uint64_t pc) {
  return do_syscall(
      ZX_SYS_fifo_read, pc, &VDso::ValidSyscallPC::fifo_read,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_fifo_read(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<size_t>::Sanitize(arg_elem_size),
            make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_data)),
            SafeSyscallArgument<size_t>::Sanitize(arg_data_size),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual_count)));
        return result;
      });
}

syscall_result wrapper_fifo_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<size_t>::RawType arg_elem_size,
                                  SafeSyscallArgument<const void*>::RawType arg_data,
                                  SafeSyscallArgument<size_t>::RawType arg_count,
                                  SafeSyscallArgument<size_t*>::RawType arg_actual_count,
                                  uint64_t pc);
syscall_result wrapper_fifo_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<size_t>::RawType arg_elem_size,
                                  SafeSyscallArgument<const void*>::RawType arg_data,
                                  SafeSyscallArgument<size_t>::RawType arg_count,
                                  SafeSyscallArgument<size_t*>::RawType arg_actual_count,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_fifo_write, pc, &VDso::ValidSyscallPC::fifo_write,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_fifo_write(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<size_t>::Sanitize(arg_elem_size),
            make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_data)),
            SafeSyscallArgument<size_t>::Sanitize(arg_count),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual_count)));
        return result;
      });
}

syscall_result wrapper_futex_get_owner(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
    SafeSyscallArgument<zx_koid_t*>::RawType arg_koid, uint64_t pc);
syscall_result wrapper_futex_get_owner(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
    SafeSyscallArgument<zx_koid_t*>::RawType arg_koid, uint64_t pc) {
  return do_syscall(
      ZX_SYS_futex_get_owner, pc, &VDso::ValidSyscallPC::futex_get_owner,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_futex_get_owner(
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_value_ptr)),
            make_user_out_ptr(SafeSyscallArgument<zx_koid_t*>::Sanitize(arg_koid)));
        return result;
      });
}

syscall_result wrapper_futex_requeue(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
    SafeSyscallArgument<uint32_t>::RawType arg_wake_count,
    SafeSyscallArgument<int32_t>::RawType arg_current_value,
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_requeue_ptr,
    SafeSyscallArgument<uint32_t>::RawType arg_requeue_count,
    SafeSyscallArgument<zx_handle_t>::RawType arg_new_requeue_owner, uint64_t pc);
syscall_result wrapper_futex_requeue(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
    SafeSyscallArgument<uint32_t>::RawType arg_wake_count,
    SafeSyscallArgument<int32_t>::RawType arg_current_value,
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_requeue_ptr,
    SafeSyscallArgument<uint32_t>::RawType arg_requeue_count,
    SafeSyscallArgument<zx_handle_t>::RawType arg_new_requeue_owner, uint64_t pc) {
  return do_syscall(
      ZX_SYS_futex_requeue, pc, &VDso::ValidSyscallPC::futex_requeue,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_futex_requeue(
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_value_ptr)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_wake_count),
            SafeSyscallArgument<int32_t>::Sanitize(arg_current_value),
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_requeue_ptr)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_requeue_count),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_new_requeue_owner));
        return result;
      });
}

syscall_result wrapper_futex_requeue_single_owner(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
    SafeSyscallArgument<int32_t>::RawType arg_current_value,
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_requeue_ptr,
    SafeSyscallArgument<uint32_t>::RawType arg_requeue_count,
    SafeSyscallArgument<zx_handle_t>::RawType arg_new_requeue_owner, uint64_t pc);
syscall_result wrapper_futex_requeue_single_owner(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
    SafeSyscallArgument<int32_t>::RawType arg_current_value,
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_requeue_ptr,
    SafeSyscallArgument<uint32_t>::RawType arg_requeue_count,
    SafeSyscallArgument<zx_handle_t>::RawType arg_new_requeue_owner, uint64_t pc) {
  return do_syscall(
      ZX_SYS_futex_requeue_single_owner, pc, &VDso::ValidSyscallPC::futex_requeue_single_owner,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_futex_requeue_single_owner(
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_value_ptr)),
            SafeSyscallArgument<int32_t>::Sanitize(arg_current_value),
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_requeue_ptr)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_requeue_count),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_new_requeue_owner));
        return result;
      });
}

syscall_result wrapper_futex_wait(SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
                                  SafeSyscallArgument<int32_t>::RawType arg_current_value,
                                  SafeSyscallArgument<zx_handle_t>::RawType arg_new_futex_owner,
                                  SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                  uint64_t pc);
syscall_result wrapper_futex_wait(SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
                                  SafeSyscallArgument<int32_t>::RawType arg_current_value,
                                  SafeSyscallArgument<zx_handle_t>::RawType arg_new_futex_owner,
                                  SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_futex_wait, pc, &VDso::ValidSyscallPC::futex_wait,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_futex_wait(
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_value_ptr)),
            SafeSyscallArgument<int32_t>::Sanitize(arg_current_value),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_new_futex_owner),
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline));
        return result;
      });
}

syscall_result wrapper_futex_wake(SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
                                  SafeSyscallArgument<uint32_t>::RawType arg_wake_count,
                                  uint64_t pc);
syscall_result wrapper_futex_wake(SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr,
                                  SafeSyscallArgument<uint32_t>::RawType arg_wake_count,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_futex_wake, pc, &VDso::ValidSyscallPC::futex_wake,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_futex_wake(
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_value_ptr)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_wake_count));
        return result;
      });
}

syscall_result wrapper_futex_wake_single_owner(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr, uint64_t pc);
syscall_result wrapper_futex_wake_single_owner(
    SafeSyscallArgument<const zx_futex_t*>::RawType arg_value_ptr, uint64_t pc) {
  return do_syscall(
      ZX_SYS_futex_wake_single_owner, pc, &VDso::ValidSyscallPC::futex_wake_single_owner,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_futex_wake_single_owner(
            make_user_in_ptr(SafeSyscallArgument<const zx_futex_t*>::Sanitize(arg_value_ptr)));
        return result;
      });
}

syscall_result wrapper_guest_create(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_guest_handle,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_vmar_handle,
                                    uint64_t pc);
syscall_result wrapper_guest_create(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_guest_handle,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_vmar_handle,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_guest_create, pc, &VDso::ValidSyscallPC::guest_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_guest_handle = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_vmar_handle = ZX_HANDLE_INVALID;
        auto result = sys_guest_create(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                                       SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                       &out_handle_guest_handle, &out_handle_vmar_handle);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_guest_handle))
                     .copy_to_user(out_handle_guest_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_vmar_handle))
                     .copy_to_user(out_handle_vmar_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_guest_set_trap(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                      SafeSyscallArgument<zx_vaddr_t>::RawType arg_addr,
                                      SafeSyscallArgument<size_t>::RawType arg_size,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_port_handle,
                                      SafeSyscallArgument<uint64_t>::RawType arg_key, uint64_t pc);
syscall_result wrapper_guest_set_trap(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                      SafeSyscallArgument<zx_vaddr_t>::RawType arg_addr,
                                      SafeSyscallArgument<size_t>::RawType arg_size,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_port_handle,
                                      SafeSyscallArgument<uint64_t>::RawType arg_key, uint64_t pc) {
  return do_syscall(ZX_SYS_guest_set_trap, pc, &VDso::ValidSyscallPC::guest_set_trap,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_guest_set_trap(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_kind),
                          SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_addr),
                          SafeSyscallArgument<size_t>::Sanitize(arg_size),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_port_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_key));
                      return result;
                    });
}

syscall_result wrapper_handle_close(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    uint64_t pc);
syscall_result wrapper_handle_close(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_handle_close, pc, &VDso::ValidSyscallPC::handle_close,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_handle_close(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_handle_close_many(
    SafeSyscallArgument<const zx_handle_t*>::RawType arg_handles,
    SafeSyscallArgument<size_t>::RawType arg_num_handles, uint64_t pc);
syscall_result wrapper_handle_close_many(
    SafeSyscallArgument<const zx_handle_t*>::RawType arg_handles,
    SafeSyscallArgument<size_t>::RawType arg_num_handles, uint64_t pc) {
  return do_syscall(
      ZX_SYS_handle_close_many, pc, &VDso::ValidSyscallPC::handle_close_many,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_handle_close_many(
            make_user_in_ptr(SafeSyscallArgument<const zx_handle_t*>::Sanitize(arg_handles)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_handles));
        return result;
      });
}

syscall_result wrapper_handle_duplicate(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<zx_rights_t>::RawType arg_rights,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc);
syscall_result wrapper_handle_duplicate(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<zx_rights_t>::RawType arg_rights,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_handle_duplicate, pc, &VDso::ValidSyscallPC::handle_duplicate,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_handle_duplicate(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_rights_t>::Sanitize(arg_rights), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_handle_replace(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_rights_t>::RawType arg_rights,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                      uint64_t pc);
syscall_result wrapper_handle_replace(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_rights_t>::RawType arg_rights,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_handle_replace, pc, &VDso::ValidSyscallPC::handle_replace,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_handle_replace(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_rights_t>::Sanitize(arg_rights), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_interrupt_ack(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     uint64_t pc);
syscall_result wrapper_interrupt_ack(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     uint64_t pc) {
  return do_syscall(ZX_SYS_interrupt_ack, pc, &VDso::ValidSyscallPC::interrupt_ack,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_interrupt_ack(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_interrupt_bind(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_port_handle,
                                      SafeSyscallArgument<uint64_t>::RawType arg_key,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      uint64_t pc);
syscall_result wrapper_interrupt_bind(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_port_handle,
                                      SafeSyscallArgument<uint64_t>::RawType arg_key,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_interrupt_bind, pc, &VDso::ValidSyscallPC::interrupt_bind,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_interrupt_bind(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_port_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_key),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options));
                      return result;
                    });
}

syscall_result wrapper_interrupt_create(SafeSyscallArgument<zx_handle_t>::RawType arg_src_obj,
                                        SafeSyscallArgument<uint32_t>::RawType arg_src_num,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out_handle,
                                        uint64_t pc);
syscall_result wrapper_interrupt_create(SafeSyscallArgument<zx_handle_t>::RawType arg_src_obj,
                                        SafeSyscallArgument<uint32_t>::RawType arg_src_num,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out_handle,
                                        uint64_t pc) {
  return do_syscall(
      ZX_SYS_interrupt_create, pc, &VDso::ValidSyscallPC::interrupt_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out_handle = ZX_HANDLE_INVALID;
        auto result = sys_interrupt_create(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_src_obj),
                                           SafeSyscallArgument<uint32_t>::Sanitize(arg_src_num),
                                           SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                           &out_handle_out_handle);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out_handle))
                     .copy_to_user(out_handle_out_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_interrupt_destroy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         uint64_t pc);
syscall_result wrapper_interrupt_destroy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         uint64_t pc) {
  return do_syscall(
      ZX_SYS_interrupt_destroy, pc, &VDso::ValidSyscallPC::interrupt_destroy,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_interrupt_destroy(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
        return result;
      });
}

syscall_result wrapper_interrupt_trigger(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint32_t>::RawType arg_options,
                                         SafeSyscallArgument<zx_time_t>::RawType arg_timestamp,
                                         uint64_t pc);
syscall_result wrapper_interrupt_trigger(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint32_t>::RawType arg_options,
                                         SafeSyscallArgument<zx_time_t>::RawType arg_timestamp,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_interrupt_trigger, pc, &VDso::ValidSyscallPC::interrupt_trigger,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_interrupt_trigger(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_time_t>::Sanitize(arg_timestamp));
                      return result;
                    });
}

syscall_result wrapper_interrupt_wait(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_time_t*>::RawType arg_out_timestamp,
                                      uint64_t pc);
syscall_result wrapper_interrupt_wait(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_time_t*>::RawType arg_out_timestamp,
                                      uint64_t pc) {
  return do_syscall(
      ZX_SYS_interrupt_wait, pc, &VDso::ValidSyscallPC::interrupt_wait,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_interrupt_wait(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            make_user_out_ptr(SafeSyscallArgument<zx_time_t*>::Sanitize(arg_out_timestamp)));
        return result;
      });
}

syscall_result wrapper_iob_allocate_id(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<zx_iob_allocate_id_options_t>::RawType arg_options,
    SafeSyscallArgument<uint32_t>::RawType arg_region_index,
    SafeSyscallArgument<const void*>::RawType arg_blob,
    SafeSyscallArgument<size_t>::RawType arg_blob_size,
    SafeSyscallArgument<uint32_t*>::RawType arg_id, uint64_t pc);
syscall_result wrapper_iob_allocate_id(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<zx_iob_allocate_id_options_t>::RawType arg_options,
    SafeSyscallArgument<uint32_t>::RawType arg_region_index,
    SafeSyscallArgument<const void*>::RawType arg_blob,
    SafeSyscallArgument<size_t>::RawType arg_blob_size,
    SafeSyscallArgument<uint32_t*>::RawType arg_id, uint64_t pc) {
  return do_syscall(ZX_SYS_iob_allocate_id, pc, &VDso::ValidSyscallPC::iob_allocate_id,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_iob_allocate_id(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_iob_allocate_id_options_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_region_index),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_blob)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_blob_size),
                          make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_id)));
                      return result;
                    });
}

syscall_result wrapper_iob_create(SafeSyscallArgument<uint64_t>::RawType arg_options,
                                  SafeSyscallArgument<const void*>::RawType arg_regions,
                                  SafeSyscallArgument<size_t>::RawType arg_num_regions,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_ep0_out,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_ep1_out,
                                  uint64_t pc);
syscall_result wrapper_iob_create(SafeSyscallArgument<uint64_t>::RawType arg_options,
                                  SafeSyscallArgument<const void*>::RawType arg_regions,
                                  SafeSyscallArgument<size_t>::RawType arg_num_regions,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_ep0_out,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_ep1_out,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_iob_create, pc, &VDso::ValidSyscallPC::iob_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_ep0_out = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_ep1_out = ZX_HANDLE_INVALID;
        auto result = sys_iob_create(
            SafeSyscallArgument<uint64_t>::Sanitize(arg_options),
            make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_regions)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_regions), &out_handle_ep0_out,
            &out_handle_ep1_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_ep0_out))
                     .copy_to_user(out_handle_ep0_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_ep1_out))
                     .copy_to_user(out_handle_ep1_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_iob_create_shared_region(SafeSyscallArgument<uint64_t>::RawType arg_options,
                                                SafeSyscallArgument<uint64_t>::RawType arg_size,
                                                SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                                uint64_t pc);
syscall_result wrapper_iob_create_shared_region(SafeSyscallArgument<uint64_t>::RawType arg_options,
                                                SafeSyscallArgument<uint64_t>::RawType arg_size,
                                                SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                                uint64_t pc) {
  return do_syscall(
      ZX_SYS_iob_create_shared_region, pc, &VDso::ValidSyscallPC::iob_create_shared_region,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
        auto result = sys_iob_create_shared_region(
            SafeSyscallArgument<uint64_t>::Sanitize(arg_options),
            SafeSyscallArgument<uint64_t>::Sanitize(arg_size), &out_handle_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                     .copy_to_user(out_handle_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_iob_writev(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_iob_write_options_t>::RawType arg_options,
                                  SafeSyscallArgument<uint32_t>::RawType arg_region_index,
                                  SafeSyscallArgument<const zx_iovec_t*>::RawType arg_vectors,
                                  SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                  uint64_t pc);
syscall_result wrapper_iob_writev(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_iob_write_options_t>::RawType arg_options,
                                  SafeSyscallArgument<uint32_t>::RawType arg_region_index,
                                  SafeSyscallArgument<const zx_iovec_t*>::RawType arg_vectors,
                                  SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_iob_writev, pc, &VDso::ValidSyscallPC::iob_writev,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_iob_writev(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_iob_write_options_t>::Sanitize(arg_options),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_region_index),
            make_user_in_ptr(SafeSyscallArgument<const zx_iovec_t*>::Sanitize(arg_vectors)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_vectors));
        return result;
      });
}

syscall_result wrapper_iommu_create(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                    SafeSyscallArgument<uint32_t>::RawType arg_type,
                                    SafeSyscallArgument<const void*>::RawType arg_desc,
                                    SafeSyscallArgument<size_t>::RawType arg_desc_size,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc);
syscall_result wrapper_iommu_create(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                    SafeSyscallArgument<uint32_t>::RawType arg_type,
                                    SafeSyscallArgument<const void*>::RawType arg_desc,
                                    SafeSyscallArgument<size_t>::RawType arg_desc_size,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_iommu_create, pc, &VDso::ValidSyscallPC::iommu_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_iommu_create(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_type),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_desc)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_desc_size), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_ioports_release(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                       SafeSyscallArgument<uint16_t>::RawType arg_io_addr,
                                       SafeSyscallArgument<uint32_t>::RawType arg_len, uint64_t pc);
syscall_result wrapper_ioports_release(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                       SafeSyscallArgument<uint16_t>::RawType arg_io_addr,
                                       SafeSyscallArgument<uint32_t>::RawType arg_len,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_ioports_release, pc, &VDso::ValidSyscallPC::ioports_release,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_ioports_release(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<uint16_t>::Sanitize(arg_io_addr),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_len));
                      return result;
                    });
}

syscall_result wrapper_ioports_request(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                       SafeSyscallArgument<uint16_t>::RawType arg_io_addr,
                                       SafeSyscallArgument<uint32_t>::RawType arg_len, uint64_t pc);
syscall_result wrapper_ioports_request(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                       SafeSyscallArgument<uint16_t>::RawType arg_io_addr,
                                       SafeSyscallArgument<uint32_t>::RawType arg_len,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_ioports_request, pc, &VDso::ValidSyscallPC::ioports_request,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_ioports_request(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<uint16_t>::Sanitize(arg_io_addr),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_len));
                      return result;
                    });
}

syscall_result wrapper_job_create(SafeSyscallArgument<zx_handle_t>::RawType arg_parent_job,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_job_create(SafeSyscallArgument<zx_handle_t>::RawType arg_parent_job,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(ZX_SYS_job_create, pc, &VDso::ValidSyscallPC::job_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_job_create(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_parent_job),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_job_set_critical(SafeSyscallArgument<zx_handle_t>::RawType arg_job,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t>::RawType arg_process,
                                        uint64_t pc);
syscall_result wrapper_job_set_critical(SafeSyscallArgument<zx_handle_t>::RawType arg_job,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t>::RawType arg_process,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_job_set_critical, pc, &VDso::ValidSyscallPC::job_set_critical,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_job_set_critical(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_job),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_process));
                      return result;
                    });
}

syscall_result wrapper_job_set_policy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<uint32_t>::RawType arg_topic,
                                      SafeSyscallArgument<const void*>::RawType arg_policy,
                                      SafeSyscallArgument<uint32_t>::RawType arg_policy_size,
                                      uint64_t pc);
syscall_result wrapper_job_set_policy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<uint32_t>::RawType arg_topic,
                                      SafeSyscallArgument<const void*>::RawType arg_policy,
                                      SafeSyscallArgument<uint32_t>::RawType arg_policy_size,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_job_set_policy, pc, &VDso::ValidSyscallPC::job_set_policy,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_job_set_policy(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_topic),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_policy)),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_policy_size));
                      return result;
                    });
}

syscall_result wrapper_ktrace_control(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_action,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<void*>::RawType arg_ptr, uint64_t pc);
syscall_result wrapper_ktrace_control(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_action,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<void*>::RawType arg_ptr, uint64_t pc) {
  return do_syscall(ZX_SYS_ktrace_control, pc, &VDso::ValidSyscallPC::ktrace_control,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_ktrace_control(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_action),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_inout_ptr(SafeSyscallArgument<void*>::Sanitize(arg_ptr)));
                      return result;
                    });
}

syscall_result wrapper_ktrace_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<void*>::RawType arg_data,
                                   SafeSyscallArgument<uint32_t>::RawType arg_offset,
                                   SafeSyscallArgument<size_t>::RawType arg_data_size,
                                   SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_ktrace_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<void*>::RawType arg_data,
                                   SafeSyscallArgument<uint32_t>::RawType arg_offset,
                                   SafeSyscallArgument<size_t>::RawType arg_data_size,
                                   SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc) {
  return do_syscall(ZX_SYS_ktrace_read, pc, &VDso::ValidSyscallPC::ktrace_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_ktrace_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_data)),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<size_t>::Sanitize(arg_data_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_membarrier_sync_process_data(uint64_t pc);
syscall_result wrapper_membarrier_sync_process_data(uint64_t pc) {
  return do_syscall(ZX_SYS_membarrier_sync_process_data, pc,
                    &VDso::ValidSyscallPC::membarrier_sync_process_data,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      /*void*/ sys_membarrier_sync_process_data();
                      return ZX_OK;
                    });
}

syscall_result wrapper_membarrier_sync_process_insn(uint64_t pc);
syscall_result wrapper_membarrier_sync_process_insn(uint64_t pc) {
  return do_syscall(ZX_SYS_membarrier_sync_process_insn, pc,
                    &VDso::ValidSyscallPC::membarrier_sync_process_insn,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      /*void*/ sys_membarrier_sync_process_insn();
                      return ZX_OK;
                    });
}

syscall_result wrapper_msi_allocate(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_count,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out_allocation,
                                    uint64_t pc);
syscall_result wrapper_msi_allocate(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_count,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out_allocation,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_msi_allocate, pc, &VDso::ValidSyscallPC::msi_allocate,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out_allocation = ZX_HANDLE_INVALID;
        auto result = sys_msi_allocate(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                       SafeSyscallArgument<uint32_t>::Sanitize(arg_count),
                                       &out_handle_out_allocation);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out_allocation))
                     .copy_to_user(out_handle_out_allocation);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_msi_create(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<uint32_t>::RawType arg_msi_id,
                                  SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                  SafeSyscallArgument<size_t>::RawType arg_vmo_offset,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out_interrupt,
                                  uint64_t pc);
syscall_result wrapper_msi_create(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<uint32_t>::RawType arg_msi_id,
                                  SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                  SafeSyscallArgument<size_t>::RawType arg_vmo_offset,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out_interrupt,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_msi_create, pc, &VDso::ValidSyscallPC::msi_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out_interrupt = ZX_HANDLE_INVALID;
        auto result = sys_msi_create(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                     SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                     SafeSyscallArgument<uint32_t>::Sanitize(arg_msi_id),
                                     SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmo),
                                     SafeSyscallArgument<size_t>::Sanitize(arg_vmo_offset),
                                     &out_handle_out_interrupt);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out_interrupt))
                     .copy_to_user(out_handle_out_interrupt);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_nanosleep(SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                 uint64_t pc);
syscall_result wrapper_nanosleep(SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                 uint64_t pc) {
  return do_syscall(
      ZX_SYS_nanosleep, pc, &VDso::ValidSyscallPC::nanosleep,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_nanosleep(SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline));
        return result;
      });
}

syscall_result wrapper_object_get_child(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint64_t>::RawType arg_koid,
                                        SafeSyscallArgument<zx_rights_t>::RawType arg_rights,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc);
syscall_result wrapper_object_get_child(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint64_t>::RawType arg_koid,
                                        SafeSyscallArgument<zx_rights_t>::RawType arg_rights,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_object_get_child, pc, &VDso::ValidSyscallPC::object_get_child,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_object_get_child(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_koid),
                          SafeSyscallArgument<zx_rights_t>::Sanitize(arg_rights), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_object_get_info(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_topic,
                                       SafeSyscallArgument<void*>::RawType arg_buffer,
                                       SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                       SafeSyscallArgument<size_t*>::RawType arg_actual,
                                       SafeSyscallArgument<size_t*>::RawType arg_avail,
                                       uint64_t pc);
syscall_result wrapper_object_get_info(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_topic,
                                       SafeSyscallArgument<void*>::RawType arg_buffer,
                                       SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                       SafeSyscallArgument<size_t*>::RawType arg_actual,
                                       SafeSyscallArgument<size_t*>::RawType arg_avail,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_object_get_info, pc, &VDso::ValidSyscallPC::object_get_info,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_object_get_info(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_topic),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_avail)));
                      return result;
                    });
}

syscall_result wrapper_object_get_property(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint32_t>::RawType arg_property,
                                           SafeSyscallArgument<void*>::RawType arg_value,
                                           SafeSyscallArgument<size_t>::RawType arg_value_size,
                                           uint64_t pc);
syscall_result wrapper_object_get_property(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint32_t>::RawType arg_property,
                                           SafeSyscallArgument<void*>::RawType arg_value,
                                           SafeSyscallArgument<size_t>::RawType arg_value_size,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_object_get_property, pc, &VDso::ValidSyscallPC::object_get_property,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_object_get_property(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_property),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_value)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_value_size));
                      return result;
                    });
}

syscall_result wrapper_object_set_profile(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<zx_handle_t>::RawType arg_profile,
                                          SafeSyscallArgument<uint32_t>::RawType arg_options,
                                          uint64_t pc);
syscall_result wrapper_object_set_profile(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<zx_handle_t>::RawType arg_profile,
                                          SafeSyscallArgument<uint32_t>::RawType arg_options,
                                          uint64_t pc) {
  return do_syscall(ZX_SYS_object_set_profile, pc, &VDso::ValidSyscallPC::object_set_profile,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_object_set_profile(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_profile),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options));
                      return result;
                    });
}

syscall_result wrapper_object_set_property(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint32_t>::RawType arg_property,
                                           SafeSyscallArgument<const void*>::RawType arg_value,
                                           SafeSyscallArgument<size_t>::RawType arg_value_size,
                                           uint64_t pc);
syscall_result wrapper_object_set_property(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint32_t>::RawType arg_property,
                                           SafeSyscallArgument<const void*>::RawType arg_value,
                                           SafeSyscallArgument<size_t>::RawType arg_value_size,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_object_set_property, pc, &VDso::ValidSyscallPC::object_set_property,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_object_set_property(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_property),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_value)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_value_size));
                      return result;
                    });
}

syscall_result wrapper_object_signal(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_clear_mask,
                                     SafeSyscallArgument<uint32_t>::RawType arg_set_mask,
                                     uint64_t pc);
syscall_result wrapper_object_signal(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_clear_mask,
                                     SafeSyscallArgument<uint32_t>::RawType arg_set_mask,
                                     uint64_t pc) {
  return do_syscall(ZX_SYS_object_signal, pc, &VDso::ValidSyscallPC::object_signal,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_object_signal(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                            SafeSyscallArgument<uint32_t>::Sanitize(arg_clear_mask),
                                            SafeSyscallArgument<uint32_t>::Sanitize(arg_set_mask));
                      return result;
                    });
}

syscall_result wrapper_object_signal_peer(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<uint32_t>::RawType arg_clear_mask,
                                          SafeSyscallArgument<uint32_t>::RawType arg_set_mask,
                                          uint64_t pc);
syscall_result wrapper_object_signal_peer(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<uint32_t>::RawType arg_clear_mask,
                                          SafeSyscallArgument<uint32_t>::RawType arg_set_mask,
                                          uint64_t pc) {
  return do_syscall(ZX_SYS_object_signal_peer, pc, &VDso::ValidSyscallPC::object_signal_peer,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_object_signal_peer(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_clear_mask),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_set_mask));
                      return result;
                    });
}

syscall_result wrapper_object_wait_async(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<zx_handle_t>::RawType arg_port,
                                         SafeSyscallArgument<uint64_t>::RawType arg_key,
                                         SafeSyscallArgument<zx_signals_t>::RawType arg_signals,
                                         SafeSyscallArgument<uint32_t>::RawType arg_options,
                                         uint64_t pc);
syscall_result wrapper_object_wait_async(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<zx_handle_t>::RawType arg_port,
                                         SafeSyscallArgument<uint64_t>::RawType arg_key,
                                         SafeSyscallArgument<zx_signals_t>::RawType arg_signals,
                                         SafeSyscallArgument<uint32_t>::RawType arg_options,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_object_wait_async, pc, &VDso::ValidSyscallPC::object_wait_async,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_object_wait_async(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_port),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_key),
                          SafeSyscallArgument<zx_signals_t>::Sanitize(arg_signals),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options));
                      return result;
                    });
}

syscall_result wrapper_object_wait_many(
    SafeSyscallArgument<zx_wait_item_t*>::RawType arg_items,
    SafeSyscallArgument<size_t>::RawType arg_num_items,
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline, uint64_t pc);
syscall_result wrapper_object_wait_many(
    SafeSyscallArgument<zx_wait_item_t*>::RawType arg_items,
    SafeSyscallArgument<size_t>::RawType arg_num_items,
    SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline, uint64_t pc) {
  return do_syscall(
      ZX_SYS_object_wait_many, pc, &VDso::ValidSyscallPC::object_wait_many,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_object_wait_many(
            make_user_inout_ptr(SafeSyscallArgument<zx_wait_item_t*>::Sanitize(arg_items)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_items),
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline));
        return result;
      });
}

syscall_result wrapper_object_wait_one(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<zx_signals_t>::RawType arg_signals,
                                       SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                       SafeSyscallArgument<zx_signals_t*>::RawType arg_observed,
                                       uint64_t pc);
syscall_result wrapper_object_wait_one(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<zx_signals_t>::RawType arg_signals,
                                       SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                       SafeSyscallArgument<zx_signals_t*>::RawType arg_observed,
                                       uint64_t pc) {
  return do_syscall(
      ZX_SYS_object_wait_one, pc, &VDso::ValidSyscallPC::object_wait_one,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_object_wait_one(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_signals_t>::Sanitize(arg_signals),
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline),
            make_user_out_ptr(SafeSyscallArgument<zx_signals_t*>::Sanitize(arg_observed)));
        return result;
      });
}

syscall_result wrapper_pager_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc);
syscall_result wrapper_pager_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_pager_create, pc, &VDso::ValidSyscallPC::pager_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_pager_create(
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_pager_create_vmo(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t>::RawType arg_port,
                                        SafeSyscallArgument<uint64_t>::RawType arg_key,
                                        SafeSyscallArgument<uint64_t>::RawType arg_size,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc);
syscall_result wrapper_pager_create_vmo(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_handle_t>::RawType arg_port,
                                        SafeSyscallArgument<uint64_t>::RawType arg_key,
                                        SafeSyscallArgument<uint64_t>::RawType arg_size,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_pager_create_vmo, pc, &VDso::ValidSyscallPC::pager_create_vmo,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_pager_create_vmo(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_port),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_key),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_size), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_pager_detach_vmo(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                        SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                        uint64_t pc);
syscall_result wrapper_pager_detach_vmo(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                        SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_pager_detach_vmo, pc, &VDso::ValidSyscallPC::pager_detach_vmo,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_pager_detach_vmo(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmo));
                      return result;
                    });
}

syscall_result wrapper_pager_op_range(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                      SafeSyscallArgument<uint32_t>::RawType arg_op,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
                                      SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                      SafeSyscallArgument<uint64_t>::RawType arg_length,
                                      SafeSyscallArgument<uint64_t>::RawType arg_data, uint64_t pc);
syscall_result wrapper_pager_op_range(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                      SafeSyscallArgument<uint32_t>::RawType arg_op,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
                                      SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                      SafeSyscallArgument<uint64_t>::RawType arg_length,
                                      SafeSyscallArgument<uint64_t>::RawType arg_data,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_pager_op_range, pc, &VDso::ValidSyscallPC::pager_op_range,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_pager_op_range(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_op),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager_vmo),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_length),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_data));
                      return result;
                    });
}

syscall_result wrapper_pager_query_dirty_ranges(
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
    SafeSyscallArgument<uint64_t>::RawType arg_offset,
    SafeSyscallArgument<uint64_t>::RawType arg_length,
    SafeSyscallArgument<void*>::RawType arg_buffer,
    SafeSyscallArgument<size_t>::RawType arg_buffer_size,
    SafeSyscallArgument<size_t*>::RawType arg_actual,
    SafeSyscallArgument<size_t*>::RawType arg_avail, uint64_t pc);
syscall_result wrapper_pager_query_dirty_ranges(
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
    SafeSyscallArgument<uint64_t>::RawType arg_offset,
    SafeSyscallArgument<uint64_t>::RawType arg_length,
    SafeSyscallArgument<void*>::RawType arg_buffer,
    SafeSyscallArgument<size_t>::RawType arg_buffer_size,
    SafeSyscallArgument<size_t*>::RawType arg_actual,
    SafeSyscallArgument<size_t*>::RawType arg_avail, uint64_t pc) {
  return do_syscall(ZX_SYS_pager_query_dirty_ranges, pc,
                    &VDso::ValidSyscallPC::pager_query_dirty_ranges,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_pager_query_dirty_ranges(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager_vmo),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_length),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_avail)));
                      return result;
                    });
}

syscall_result wrapper_pager_query_vmo_stats(
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<void*>::RawType arg_buffer,
    SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc);
syscall_result wrapper_pager_query_vmo_stats(
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
    SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<void*>::RawType arg_buffer,
    SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc) {
  return do_syscall(ZX_SYS_pager_query_vmo_stats, pc, &VDso::ValidSyscallPC::pager_query_vmo_stats,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_pager_query_vmo_stats(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager_vmo),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_pager_supply_pages(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                          SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
                                          SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                          SafeSyscallArgument<uint64_t>::RawType arg_length,
                                          SafeSyscallArgument<zx_handle_t>::RawType arg_aux_vmo,
                                          SafeSyscallArgument<uint64_t>::RawType arg_aux_offset,
                                          uint64_t pc);
syscall_result wrapper_pager_supply_pages(SafeSyscallArgument<zx_handle_t>::RawType arg_pager,
                                          SafeSyscallArgument<zx_handle_t>::RawType arg_pager_vmo,
                                          SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                          SafeSyscallArgument<uint64_t>::RawType arg_length,
                                          SafeSyscallArgument<zx_handle_t>::RawType arg_aux_vmo,
                                          SafeSyscallArgument<uint64_t>::RawType arg_aux_offset,
                                          uint64_t pc) {
  return do_syscall(ZX_SYS_pager_supply_pages, pc, &VDso::ValidSyscallPC::pager_supply_pages,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_pager_supply_pages(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_pager_vmo),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_length),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_aux_vmo),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_aux_offset));
                      return result;
                    });
}

syscall_result wrapper_pmt_unpin(SafeSyscallArgument<zx_handle_t>::RawType arg_handle, uint64_t pc);
syscall_result wrapper_pmt_unpin(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 uint64_t pc) {
  return do_syscall(ZX_SYS_pmt_unpin, pc, &VDso::ValidSyscallPC::pmt_unpin,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_pmt_unpin(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_port_cancel(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<zx_handle_t>::RawType arg_source,
                                   SafeSyscallArgument<uint64_t>::RawType arg_key, uint64_t pc);
syscall_result wrapper_port_cancel(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<zx_handle_t>::RawType arg_source,
                                   SafeSyscallArgument<uint64_t>::RawType arg_key, uint64_t pc) {
  return do_syscall(ZX_SYS_port_cancel, pc, &VDso::ValidSyscallPC::port_cancel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_port_cancel(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_source),
                                          SafeSyscallArgument<uint64_t>::Sanitize(arg_key));
                      return result;
                    });
}

syscall_result wrapper_port_cancel_key(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<uint64_t>::RawType arg_key, uint64_t pc);
syscall_result wrapper_port_cancel_key(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<uint64_t>::RawType arg_key,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_port_cancel_key, pc, &VDso::ValidSyscallPC::port_cancel_key,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_port_cancel_key(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_key));
                      return result;
                    });
}

syscall_result wrapper_port_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_port_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                   uint64_t pc) {
  return do_syscall(ZX_SYS_port_create, pc, &VDso::ValidSyscallPC::port_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_port_create(
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_port_queue(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<const zx_port_packet_t*>::RawType arg_packet,
                                  uint64_t pc);
syscall_result wrapper_port_queue(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<const zx_port_packet_t*>::RawType arg_packet,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_port_queue, pc, &VDso::ValidSyscallPC::port_queue,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_port_queue(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            make_user_in_ptr(SafeSyscallArgument<const zx_port_packet_t*>::Sanitize(arg_packet)));
        return result;
      });
}

syscall_result wrapper_port_wait(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                 SafeSyscallArgument<zx_port_packet_t*>::RawType arg_packet,
                                 uint64_t pc);
syscall_result wrapper_port_wait(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<zx_instant_mono_t>::RawType arg_deadline,
                                 SafeSyscallArgument<zx_port_packet_t*>::RawType arg_packet,
                                 uint64_t pc) {
  return do_syscall(
      ZX_SYS_port_wait, pc, &VDso::ValidSyscallPC::port_wait,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_port_wait(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_instant_mono_t>::Sanitize(arg_deadline),
            make_user_out_ptr(SafeSyscallArgument<zx_port_packet_t*>::Sanitize(arg_packet)));
        return result;
      });
}

syscall_result wrapper_process_create(SafeSyscallArgument<zx_handle_t>::RawType arg_job,
                                      SafeSyscallArgument<const char*>::RawType arg_name,
                                      SafeSyscallArgument<size_t>::RawType arg_name_size,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_proc_handle,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_vmar_handle,
                                      uint64_t pc);
syscall_result wrapper_process_create(SafeSyscallArgument<zx_handle_t>::RawType arg_job,
                                      SafeSyscallArgument<const char*>::RawType arg_name,
                                      SafeSyscallArgument<size_t>::RawType arg_name_size,
                                      SafeSyscallArgument<uint32_t>::RawType arg_options,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_proc_handle,
                                      SafeSyscallArgument<zx_handle_t*>::RawType arg_vmar_handle,
                                      uint64_t pc) {
  return do_syscall(
      ZX_SYS_process_create, pc, &VDso::ValidSyscallPC::process_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_proc_handle = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_vmar_handle = ZX_HANDLE_INVALID;
        auto result = sys_process_create(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_job),
            make_user_in_ptr(SafeSyscallArgument<const char*>::Sanitize(arg_name)),
            SafeSyscallArgument<size_t>::Sanitize(arg_name_size),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_proc_handle,
            &out_handle_vmar_handle);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_proc_handle))
                     .copy_to_user(out_handle_proc_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_vmar_handle))
                     .copy_to_user(out_handle_vmar_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_process_create_shared(
    SafeSyscallArgument<zx_handle_t>::RawType arg_shared_proc,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<const char*>::RawType arg_name,
    SafeSyscallArgument<size_t>::RawType arg_name_size,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_proc_handle,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_restricted_vmar_handle, uint64_t pc);
syscall_result wrapper_process_create_shared(
    SafeSyscallArgument<zx_handle_t>::RawType arg_shared_proc,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<const char*>::RawType arg_name,
    SafeSyscallArgument<size_t>::RawType arg_name_size,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_proc_handle,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_restricted_vmar_handle, uint64_t pc) {
  return do_syscall(
      ZX_SYS_process_create_shared, pc, &VDso::ValidSyscallPC::process_create_shared,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_proc_handle = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_restricted_vmar_handle = ZX_HANDLE_INVALID;
        auto result = sys_process_create_shared(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_shared_proc),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_in_ptr(SafeSyscallArgument<const char*>::Sanitize(arg_name)),
            SafeSyscallArgument<size_t>::Sanitize(arg_name_size), &out_handle_proc_handle,
            &out_handle_restricted_vmar_handle);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_proc_handle))
                     .copy_to_user(out_handle_proc_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(
                     SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_restricted_vmar_handle))
                     .copy_to_user(out_handle_restricted_vmar_handle);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_process_exit(SafeSyscallArgument<int64_t>::RawType arg_retcode, uint64_t pc);
syscall_result wrapper_process_exit(SafeSyscallArgument<int64_t>::RawType arg_retcode,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_process_exit, pc, &VDso::ValidSyscallPC::process_exit,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        /*noreturn*/ sys_process_exit(SafeSyscallArgument<int64_t>::Sanitize(arg_retcode));
        /* NOTREACHED */
        return ZX_ERR_BAD_STATE;
      });
}

syscall_result wrapper_process_read_memory(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<zx_vaddr_t>::RawType arg_vaddr,
                                           SafeSyscallArgument<void*>::RawType arg_buffer,
                                           SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                           SafeSyscallArgument<size_t*>::RawType arg_actual,
                                           uint64_t pc);
syscall_result wrapper_process_read_memory(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<zx_vaddr_t>::RawType arg_vaddr,
                                           SafeSyscallArgument<void*>::RawType arg_buffer,
                                           SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                           SafeSyscallArgument<size_t*>::RawType arg_actual,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_process_read_memory, pc, &VDso::ValidSyscallPC::process_read_memory,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_process_read_memory(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_vaddr),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_process_start(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<zx_handle_t>::RawType arg_thread,
                                     SafeSyscallArgument<zx_vaddr_t>::RawType arg_entry,
                                     SafeSyscallArgument<zx_vaddr_t>::RawType arg_stack,
                                     SafeSyscallArgument<zx_handle_t>::RawType arg_arg1,
                                     SafeSyscallArgument<uintptr_t>::RawType arg_arg2, uint64_t pc);
syscall_result wrapper_process_start(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<zx_handle_t>::RawType arg_thread,
                                     SafeSyscallArgument<zx_vaddr_t>::RawType arg_entry,
                                     SafeSyscallArgument<zx_vaddr_t>::RawType arg_stack,
                                     SafeSyscallArgument<zx_handle_t>::RawType arg_arg1,
                                     SafeSyscallArgument<uintptr_t>::RawType arg_arg2,
                                     uint64_t pc) {
  return do_syscall(ZX_SYS_process_start, pc, &VDso::ValidSyscallPC::process_start,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_process_start(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_thread),
                                            SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_entry),
                                            SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_stack),
                                            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_arg1),
                                            SafeSyscallArgument<uintptr_t>::Sanitize(arg_arg2));
                      return result;
                    });
}

syscall_result wrapper_process_write_memory(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                            SafeSyscallArgument<zx_vaddr_t>::RawType arg_vaddr,
                                            SafeSyscallArgument<const void*>::RawType arg_buffer,
                                            SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                            SafeSyscallArgument<size_t*>::RawType arg_actual,
                                            uint64_t pc);
syscall_result wrapper_process_write_memory(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                            SafeSyscallArgument<zx_vaddr_t>::RawType arg_vaddr,
                                            SafeSyscallArgument<const void*>::RawType arg_buffer,
                                            SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                            SafeSyscallArgument<size_t*>::RawType arg_actual,
                                            uint64_t pc) {
  return do_syscall(ZX_SYS_process_write_memory, pc, &VDso::ValidSyscallPC::process_write_memory,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_process_write_memory(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_vaddr),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_profile_create(
    SafeSyscallArgument<zx_handle_t>::RawType arg_profile_rsrc,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<const zx_profile_info_t*>::RawType arg_profile,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_profile_create(
    SafeSyscallArgument<zx_handle_t>::RawType arg_profile_rsrc,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<const zx_profile_info_t*>::RawType arg_profile,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(
      ZX_SYS_profile_create, pc, &VDso::ValidSyscallPC::profile_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
        auto result = sys_profile_create(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_profile_rsrc),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_in_ptr(SafeSyscallArgument<const zx_profile_info_t*>::Sanitize(arg_profile)),
            &out_handle_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                     .copy_to_user(out_handle_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_resource_create(SafeSyscallArgument<zx_handle_t>::RawType arg_parent_rsrc,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<uint64_t>::RawType arg_base,
                                       SafeSyscallArgument<size_t>::RawType arg_size,
                                       SafeSyscallArgument<const char*>::RawType arg_name,
                                       SafeSyscallArgument<size_t>::RawType arg_name_size,
                                       SafeSyscallArgument<zx_handle_t*>::RawType arg_resource_out,
                                       uint64_t pc);
syscall_result wrapper_resource_create(SafeSyscallArgument<zx_handle_t>::RawType arg_parent_rsrc,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<uint64_t>::RawType arg_base,
                                       SafeSyscallArgument<size_t>::RawType arg_size,
                                       SafeSyscallArgument<const char*>::RawType arg_name,
                                       SafeSyscallArgument<size_t>::RawType arg_name_size,
                                       SafeSyscallArgument<zx_handle_t*>::RawType arg_resource_out,
                                       uint64_t pc) {
  return do_syscall(
      ZX_SYS_resource_create, pc, &VDso::ValidSyscallPC::resource_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_resource_out = ZX_HANDLE_INVALID;
        auto result = sys_resource_create(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_parent_rsrc),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            SafeSyscallArgument<uint64_t>::Sanitize(arg_base),
            SafeSyscallArgument<size_t>::Sanitize(arg_size),
            make_user_in_ptr(SafeSyscallArgument<const char*>::Sanitize(arg_name)),
            SafeSyscallArgument<size_t>::Sanitize(arg_name_size), &out_handle_resource_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_resource_out))
                     .copy_to_user(out_handle_resource_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_restricted_bind_state(
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
    SafeSyscallArgument<zx_exception_report_t*>::RawType arg_out_exception, uint64_t pc);
syscall_result wrapper_restricted_bind_state(
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
    SafeSyscallArgument<zx_exception_report_t*>::RawType arg_out_exception, uint64_t pc) {
  return do_syscall(
      ZX_SYS_restricted_bind_state, pc, &VDso::ValidSyscallPC::restricted_bind_state,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
        auto result = sys_restricted_bind_state(
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out,
            make_user_out_ptr(
                SafeSyscallArgument<zx_exception_report_t*>::Sanitize(arg_out_exception)));
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                     .copy_to_user(out_handle_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_restricted_enter(
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<uintptr_t>::RawType arg_vector_table_ptr,
    SafeSyscallArgument<uintptr_t>::RawType arg_context, uint64_t pc);
syscall_result wrapper_restricted_enter(
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<uintptr_t>::RawType arg_vector_table_ptr,
    SafeSyscallArgument<uintptr_t>::RawType arg_context, uint64_t pc) {
  return do_syscall(ZX_SYS_restricted_enter, pc, &VDso::ValidSyscallPC::restricted_enter,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_restricted_enter(
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uintptr_t>::Sanitize(arg_vector_table_ptr),
                          SafeSyscallArgument<uintptr_t>::Sanitize(arg_context));
                      return result;
                    });
}

syscall_result wrapper_restricted_kick(SafeSyscallArgument<zx_handle_t>::RawType arg_thread,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       uint64_t pc);
syscall_result wrapper_restricted_kick(SafeSyscallArgument<zx_handle_t>::RawType arg_thread,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_restricted_kick, pc, &VDso::ValidSyscallPC::restricted_kick,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_restricted_kick(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_thread),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options));
                      return result;
                    });
}

syscall_result wrapper_restricted_unbind_state(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                               uint64_t pc);
syscall_result wrapper_restricted_unbind_state(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                               uint64_t pc) {
  return do_syscall(
      ZX_SYS_restricted_unbind_state, pc, &VDso::ValidSyscallPC::restricted_unbind_state,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result =
            sys_restricted_unbind_state(SafeSyscallArgument<uint32_t>::Sanitize(arg_options));
        return result;
      });
}

syscall_result wrapper_sampler_create(
    SafeSyscallArgument<zx_handle_t>::RawType arg_rsrc,
    SafeSyscallArgument<uint64_t>::RawType arg_options,
    SafeSyscallArgument<const zx_sampler_config_t*>::RawType arg_config,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_per_cpu_buffers, uint64_t pc);
syscall_result wrapper_sampler_create(
    SafeSyscallArgument<zx_handle_t>::RawType arg_rsrc,
    SafeSyscallArgument<uint64_t>::RawType arg_options,
    SafeSyscallArgument<const zx_sampler_config_t*>::RawType arg_config,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_per_cpu_buffers, uint64_t pc) {
  return do_syscall(
      ZX_SYS_sampler_create, pc, &VDso::ValidSyscallPC::sampler_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_per_cpu_buffers = ZX_HANDLE_INVALID;
        auto result = sys_sampler_create(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_rsrc),
            SafeSyscallArgument<uint64_t>::Sanitize(arg_options),
            make_user_in_ptr(SafeSyscallArgument<const zx_sampler_config_t*>::Sanitize(arg_config)),
            &out_handle_per_cpu_buffers);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_per_cpu_buffers))
                     .copy_to_user(out_handle_per_cpu_buffers);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_sampler_read(SafeSyscallArgument<zx_handle_t>::RawType arg_iobuffer,
                                    SafeSyscallArgument<void*>::RawType arg_data,
                                    SafeSyscallArgument<size_t>::RawType arg_data_size,
                                    SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_sampler_read(SafeSyscallArgument<zx_handle_t>::RawType arg_iobuffer,
                                    SafeSyscallArgument<void*>::RawType arg_data,
                                    SafeSyscallArgument<size_t>::RawType arg_data_size,
                                    SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc) {
  return do_syscall(ZX_SYS_sampler_read, pc, &VDso::ValidSyscallPC::sampler_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_sampler_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_iobuffer),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_data)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_data_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_sampler_start(SafeSyscallArgument<zx_handle_t>::RawType arg_iobuffer,
                                     uint64_t pc);
syscall_result wrapper_sampler_start(SafeSyscallArgument<zx_handle_t>::RawType arg_iobuffer,
                                     uint64_t pc) {
  return do_syscall(
      ZX_SYS_sampler_start, pc, &VDso::ValidSyscallPC::sampler_start,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_sampler_start(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_iobuffer));
        return result;
      });
}

syscall_result wrapper_sampler_stop(SafeSyscallArgument<zx_handle_t>::RawType arg_iobuffer,
                                    uint64_t pc);
syscall_result wrapper_sampler_stop(SafeSyscallArgument<zx_handle_t>::RawType arg_iobuffer,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_sampler_stop, pc, &VDso::ValidSyscallPC::sampler_stop,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_sampler_stop(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_iobuffer));
        return result;
      });
}

syscall_result wrapper_smc_call(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<const zx_smc_parameters_t*>::RawType arg_parameters,
    SafeSyscallArgument<zx_smc_result_t*>::RawType arg_out_smc_result, uint64_t pc);
syscall_result wrapper_smc_call(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<const zx_smc_parameters_t*>::RawType arg_parameters,
    SafeSyscallArgument<zx_smc_result_t*>::RawType arg_out_smc_result, uint64_t pc) {
  return do_syscall(
      ZX_SYS_smc_call, pc, &VDso::ValidSyscallPC::smc_call,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_smc_call(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            make_user_in_ptr(
                SafeSyscallArgument<const zx_smc_parameters_t*>::Sanitize(arg_parameters)),
            make_user_out_ptr(SafeSyscallArgument<zx_smc_result_t*>::Sanitize(arg_out_smc_result)));
        return result;
      });
}

syscall_result wrapper_socket_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                     uint64_t pc);
syscall_result wrapper_socket_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out0,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out1,
                                     uint64_t pc) {
  return do_syscall(
      ZX_SYS_socket_create, pc, &VDso::ValidSyscallPC::socket_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out0 = ZX_HANDLE_INVALID;
        zx_handle_t out_handle_out1 = ZX_HANDLE_INVALID;
        auto result = sys_socket_create(SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                        &out_handle_out0, &out_handle_out1);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out0))
                     .copy_to_user(out_handle_out0);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out1))
                     .copy_to_user(out_handle_out1);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_socket_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<void*>::RawType arg_buffer,
                                   SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                   SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_socket_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<void*>::RawType arg_buffer,
                                   SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                   SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc) {
  return do_syscall(ZX_SYS_socket_read, pc, &VDso::ValidSyscallPC::socket_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_socket_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_socket_set_disposition(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_disposition,
    SafeSyscallArgument<uint32_t>::RawType arg_disposition_peer, uint64_t pc);
syscall_result wrapper_socket_set_disposition(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_disposition,
    SafeSyscallArgument<uint32_t>::RawType arg_disposition_peer, uint64_t pc) {
  return do_syscall(ZX_SYS_socket_set_disposition, pc,
                    &VDso::ValidSyscallPC::socket_set_disposition,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_socket_set_disposition(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_disposition),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_disposition_peer));
                      return result;
                    });
}

syscall_result wrapper_socket_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<const void*>::RawType arg_buffer,
                                    SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                    SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_socket_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<const void*>::RawType arg_buffer,
                                    SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                    SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc) {
  return do_syscall(ZX_SYS_socket_write, pc, &VDso::ValidSyscallPC::socket_write,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_socket_write(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size),
                          make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
                      return result;
                    });
}

syscall_result wrapper_stream_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                     SafeSyscallArgument<zx_off_t>::RawType arg_seek,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out_stream,
                                     uint64_t pc);
syscall_result wrapper_stream_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                     SafeSyscallArgument<zx_off_t>::RawType arg_seek,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out_stream,
                                     uint64_t pc) {
  return do_syscall(
      ZX_SYS_stream_create, pc, &VDso::ValidSyscallPC::stream_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out_stream = ZX_HANDLE_INVALID;
        auto result = sys_stream_create(SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                                        SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmo),
                                        SafeSyscallArgument<zx_off_t>::Sanitize(arg_seek),
                                        &out_handle_out_stream);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out_stream))
                     .copy_to_user(out_handle_out_stream);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_stream_readv(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_iovec_t*>::RawType arg_vectors,
                                    SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                    SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_stream_readv(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_iovec_t*>::RawType arg_vectors,
                                    SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                    SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc) {
  return do_syscall(
      ZX_SYS_stream_readv, pc, &VDso::ValidSyscallPC::stream_readv,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_stream_readv(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_out_ptr(SafeSyscallArgument<zx_iovec_t*>::Sanitize(arg_vectors)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_vectors),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
        return result;
      });
}

syscall_result wrapper_stream_readv_at(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<zx_off_t>::RawType arg_offset,
                                       SafeSyscallArgument<zx_iovec_t*>::RawType arg_vectors,
                                       SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                       SafeSyscallArgument<size_t*>::RawType arg_actual,
                                       uint64_t pc);
syscall_result wrapper_stream_readv_at(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_options,
                                       SafeSyscallArgument<zx_off_t>::RawType arg_offset,
                                       SafeSyscallArgument<zx_iovec_t*>::RawType arg_vectors,
                                       SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                       SafeSyscallArgument<size_t*>::RawType arg_actual,
                                       uint64_t pc) {
  return do_syscall(
      ZX_SYS_stream_readv_at, pc, &VDso::ValidSyscallPC::stream_readv_at,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_stream_readv_at(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            SafeSyscallArgument<zx_off_t>::Sanitize(arg_offset),
            make_user_out_ptr(SafeSyscallArgument<zx_iovec_t*>::Sanitize(arg_vectors)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_vectors),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
        return result;
      });
}

syscall_result wrapper_stream_seek(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<zx_stream_seek_origin_t>::RawType arg_whence,
                                   SafeSyscallArgument<int64_t>::RawType arg_offset,
                                   SafeSyscallArgument<zx_off_t*>::RawType arg_out_seek,
                                   uint64_t pc);
syscall_result wrapper_stream_seek(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                   SafeSyscallArgument<zx_stream_seek_origin_t>::RawType arg_whence,
                                   SafeSyscallArgument<int64_t>::RawType arg_offset,
                                   SafeSyscallArgument<zx_off_t*>::RawType arg_out_seek,
                                   uint64_t pc) {
  return do_syscall(
      ZX_SYS_stream_seek, pc, &VDso::ValidSyscallPC::stream_seek,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_stream_seek(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_stream_seek_origin_t>::Sanitize(arg_whence),
            SafeSyscallArgument<int64_t>::Sanitize(arg_offset),
            make_user_out_ptr(SafeSyscallArgument<zx_off_t*>::Sanitize(arg_out_seek)));
        return result;
      });
}

syscall_result wrapper_stream_writev(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<const zx_iovec_t*>::RawType arg_vectors,
                                     SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                     SafeSyscallArgument<size_t*>::RawType arg_actual, uint64_t pc);
syscall_result wrapper_stream_writev(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<const zx_iovec_t*>::RawType arg_vectors,
                                     SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                     SafeSyscallArgument<size_t*>::RawType arg_actual,
                                     uint64_t pc) {
  return do_syscall(
      ZX_SYS_stream_writev, pc, &VDso::ValidSyscallPC::stream_writev,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_stream_writev(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            make_user_in_ptr(SafeSyscallArgument<const zx_iovec_t*>::Sanitize(arg_vectors)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_vectors),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
        return result;
      });
}

syscall_result wrapper_stream_writev_at(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_off_t>::RawType arg_offset,
                                        SafeSyscallArgument<const zx_iovec_t*>::RawType arg_vectors,
                                        SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                        SafeSyscallArgument<size_t*>::RawType arg_actual,
                                        uint64_t pc);
syscall_result wrapper_stream_writev_at(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<zx_off_t>::RawType arg_offset,
                                        SafeSyscallArgument<const zx_iovec_t*>::RawType arg_vectors,
                                        SafeSyscallArgument<size_t>::RawType arg_num_vectors,
                                        SafeSyscallArgument<size_t*>::RawType arg_actual,
                                        uint64_t pc) {
  return do_syscall(
      ZX_SYS_stream_writev_at, pc, &VDso::ValidSyscallPC::stream_writev_at,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_stream_writev_at(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            SafeSyscallArgument<zx_off_t>::Sanitize(arg_offset),
            make_user_in_ptr(SafeSyscallArgument<const zx_iovec_t*>::Sanitize(arg_vectors)),
            SafeSyscallArgument<size_t>::Sanitize(arg_num_vectors),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_actual)));
        return result;
      });
}

syscall_result wrapper_syscall_next_1(SafeSyscallArgument<int32_t>::RawType arg_arg, uint64_t pc);
syscall_result wrapper_syscall_next_1(SafeSyscallArgument<int32_t>::RawType arg_arg, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_next_1, pc, &VDso::ValidSyscallPC::syscall_next_1,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_next_1(SafeSyscallArgument<int32_t>::Sanitize(arg_arg));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_handle_create(
    SafeSyscallArgument<zx_status_t>::RawType arg_return_value,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_syscall_test_handle_create(
    SafeSyscallArgument<zx_status_t>::RawType arg_return_value,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(
      ZX_SYS_syscall_test_handle_create, pc, &VDso::ValidSyscallPC::syscall_test_handle_create,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
        auto result = sys_syscall_test_handle_create(
            SafeSyscallArgument<zx_status_t>::Sanitize(arg_return_value), &out_handle_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                     .copy_to_user(out_handle_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_syscall_test_rust_handle(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t*>::RawType arg_value, uint64_t pc);
syscall_result wrapper_syscall_test_rust_handle(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t*>::RawType arg_value, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_handle, pc,
                    &VDso::ValidSyscallPC::syscall_test_rust_handle,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_rust_handle(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_value)));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_inoutptr(SafeSyscallArgument<int32_t*>::RawType arg_ptr,
                                                  uint64_t pc);
syscall_result wrapper_syscall_test_rust_inoutptr(SafeSyscallArgument<int32_t*>::RawType arg_ptr,
                                                  uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_inoutptr, pc,
                    &VDso::ValidSyscallPC::syscall_test_rust_inoutptr,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_rust_inoutptr(
                          make_user_inout_ptr(SafeSyscallArgument<int32_t*>::Sanitize(arg_ptr)));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_inptr(SafeSyscallArgument<const int32_t*>::RawType arg_ptr,
                                               SafeSyscallArgument<int32_t*>::RawType arg_value,
                                               uint64_t pc);
syscall_result wrapper_syscall_test_rust_inptr(SafeSyscallArgument<const int32_t*>::RawType arg_ptr,
                                               SafeSyscallArgument<int32_t*>::RawType arg_value,
                                               uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_inptr, pc,
                    &VDso::ValidSyscallPC::syscall_test_rust_inptr,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_rust_inptr(
                          make_user_in_ptr(SafeSyscallArgument<const int32_t*>::Sanitize(arg_ptr)),
                          make_user_out_ptr(SafeSyscallArgument<int32_t*>::Sanitize(arg_value)));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_outptr(SafeSyscallArgument<int32_t>::RawType arg_value,
                                                SafeSyscallArgument<int32_t*>::RawType arg_ptr,
                                                uint64_t pc);
syscall_result wrapper_syscall_test_rust_outptr(SafeSyscallArgument<int32_t>::RawType arg_value,
                                                SafeSyscallArgument<int32_t*>::RawType arg_ptr,
                                                uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_outptr, pc,
                    &VDso::ValidSyscallPC::syscall_test_rust_outptr,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_rust_outptr(
                          SafeSyscallArgument<int32_t>::Sanitize(arg_value),
                          make_user_out_ptr(SafeSyscallArgument<int32_t*>::Sanitize(arg_ptr)));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_wrapper(SafeSyscallArgument<int32_t>::RawType arg_a,
                                                 SafeSyscallArgument<int32_t>::RawType arg_b,
                                                 SafeSyscallArgument<int32_t>::RawType arg_c,
                                                 uint64_t pc);
syscall_result wrapper_syscall_test_rust_wrapper(SafeSyscallArgument<int32_t>::RawType arg_a,
                                                 SafeSyscallArgument<int32_t>::RawType arg_b,
                                                 SafeSyscallArgument<int32_t>::RawType arg_c,
                                                 uint64_t pc) {
  return do_syscall(
      ZX_SYS_syscall_test_rust_wrapper, pc, &VDso::ValidSyscallPC::syscall_test_rust_wrapper,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_syscall_test_rust_wrapper(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                    SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                    SafeSyscallArgument<int32_t>::Sanitize(arg_c));
        return result;
      });
}

syscall_result wrapper_syscall_test_rust_0(uint64_t pc);
syscall_result wrapper_syscall_test_rust_0(uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_0, pc, &VDso::ValidSyscallPC::syscall_test_rust_0,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_rust_0();
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_1(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           uint64_t pc);
syscall_result wrapper_syscall_test_rust_1(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_1, pc, &VDso::ValidSyscallPC::syscall_test_rust_1,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_1(SafeSyscallArgument<int32_t>::Sanitize(arg_a));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_2(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           uint64_t pc);
syscall_result wrapper_syscall_test_rust_2(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_2, pc, &VDso::ValidSyscallPC::syscall_test_rust_2,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_2(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_3(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           uint64_t pc);
syscall_result wrapper_syscall_test_rust_3(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_3, pc, &VDso::ValidSyscallPC::syscall_test_rust_3,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_3(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_c));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_4(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           SafeSyscallArgument<int32_t>::RawType arg_d,
                                           uint64_t pc);
syscall_result wrapper_syscall_test_rust_4(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           SafeSyscallArgument<int32_t>::RawType arg_d,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_4, pc, &VDso::ValidSyscallPC::syscall_test_rust_4,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_4(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_d));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_5(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           SafeSyscallArgument<int32_t>::RawType arg_d,
                                           SafeSyscallArgument<int32_t>::RawType arg_e,
                                           uint64_t pc);
syscall_result wrapper_syscall_test_rust_5(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           SafeSyscallArgument<int32_t>::RawType arg_d,
                                           SafeSyscallArgument<int32_t>::RawType arg_e,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_5, pc, &VDso::ValidSyscallPC::syscall_test_rust_5,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_5(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_e));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_6(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           SafeSyscallArgument<int32_t>::RawType arg_d,
                                           SafeSyscallArgument<int32_t>::RawType arg_e,
                                           SafeSyscallArgument<int32_t>::RawType arg_f,
                                           uint64_t pc);
syscall_result wrapper_syscall_test_rust_6(SafeSyscallArgument<int32_t>::RawType arg_a,
                                           SafeSyscallArgument<int32_t>::RawType arg_b,
                                           SafeSyscallArgument<int32_t>::RawType arg_c,
                                           SafeSyscallArgument<int32_t>::RawType arg_d,
                                           SafeSyscallArgument<int32_t>::RawType arg_e,
                                           SafeSyscallArgument<int32_t>::RawType arg_f,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_6, pc, &VDso::ValidSyscallPC::syscall_test_rust_6,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_6(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_e),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_f));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_7(
    SafeSyscallArgument<int32_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int32_t>::RawType arg_c, SafeSyscallArgument<int32_t>::RawType arg_d,
    SafeSyscallArgument<int32_t>::RawType arg_e, SafeSyscallArgument<int32_t>::RawType arg_f,
    SafeSyscallArgument<int32_t>::RawType arg_g, uint64_t pc);
syscall_result wrapper_syscall_test_rust_7(
    SafeSyscallArgument<int32_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int32_t>::RawType arg_c, SafeSyscallArgument<int32_t>::RawType arg_d,
    SafeSyscallArgument<int32_t>::RawType arg_e, SafeSyscallArgument<int32_t>::RawType arg_f,
    SafeSyscallArgument<int32_t>::RawType arg_g, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_7, pc, &VDso::ValidSyscallPC::syscall_test_rust_7,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_7(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_e),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_f),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_g));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_rust_8(
    SafeSyscallArgument<int32_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int32_t>::RawType arg_c, SafeSyscallArgument<int32_t>::RawType arg_d,
    SafeSyscallArgument<int32_t>::RawType arg_e, SafeSyscallArgument<int32_t>::RawType arg_f,
    SafeSyscallArgument<int32_t>::RawType arg_g, SafeSyscallArgument<int32_t>::RawType arg_h,
    uint64_t pc);
syscall_result wrapper_syscall_test_rust_8(
    SafeSyscallArgument<int32_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int32_t>::RawType arg_c, SafeSyscallArgument<int32_t>::RawType arg_d,
    SafeSyscallArgument<int32_t>::RawType arg_e, SafeSyscallArgument<int32_t>::RawType arg_f,
    SafeSyscallArgument<int32_t>::RawType arg_g, SafeSyscallArgument<int32_t>::RawType arg_h,
    uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_rust_8, pc, &VDso::ValidSyscallPC::syscall_test_rust_8,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_rust_8(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_e),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_f),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_g),
                                                  SafeSyscallArgument<int32_t>::Sanitize(arg_h));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_widening_signed_narrow(
    SafeSyscallArgument<int64_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int16_t>::RawType arg_c, SafeSyscallArgument<int8_t>::RawType arg_d,
    uint64_t pc);
syscall_result wrapper_syscall_test_widening_signed_narrow(
    SafeSyscallArgument<int64_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int16_t>::RawType arg_c, SafeSyscallArgument<int8_t>::RawType arg_d,
    uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_widening_signed_narrow, pc,
                    &VDso::ValidSyscallPC::syscall_test_widening_signed_narrow,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_widening_signed_narrow(
                          SafeSyscallArgument<int64_t>::Sanitize(arg_a),
                          SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                          SafeSyscallArgument<int16_t>::Sanitize(arg_c),
                          SafeSyscallArgument<int8_t>::Sanitize(arg_d));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_widening_signed_wide(
    SafeSyscallArgument<int64_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int16_t>::RawType arg_c, SafeSyscallArgument<int8_t>::RawType arg_d,
    uint64_t pc);
syscall_result wrapper_syscall_test_widening_signed_wide(
    SafeSyscallArgument<int64_t>::RawType arg_a, SafeSyscallArgument<int32_t>::RawType arg_b,
    SafeSyscallArgument<int16_t>::RawType arg_c, SafeSyscallArgument<int8_t>::RawType arg_d,
    uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_widening_signed_wide, pc,
                    &VDso::ValidSyscallPC::syscall_test_widening_signed_wide,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_widening_signed_wide(
                          SafeSyscallArgument<int64_t>::Sanitize(arg_a),
                          SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                          SafeSyscallArgument<int16_t>::Sanitize(arg_c),
                          SafeSyscallArgument<int8_t>::Sanitize(arg_d));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_widening_unsigned_narrow(
    SafeSyscallArgument<uint64_t>::RawType arg_a, SafeSyscallArgument<uint32_t>::RawType arg_b,
    SafeSyscallArgument<uint16_t>::RawType arg_c, SafeSyscallArgument<uint8_t>::RawType arg_d,
    uint64_t pc);
syscall_result wrapper_syscall_test_widening_unsigned_narrow(
    SafeSyscallArgument<uint64_t>::RawType arg_a, SafeSyscallArgument<uint32_t>::RawType arg_b,
    SafeSyscallArgument<uint16_t>::RawType arg_c, SafeSyscallArgument<uint8_t>::RawType arg_d,
    uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_widening_unsigned_narrow, pc,
                    &VDso::ValidSyscallPC::syscall_test_widening_unsigned_narrow,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_widening_unsigned_narrow(
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_a),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_b),
                          SafeSyscallArgument<uint16_t>::Sanitize(arg_c),
                          SafeSyscallArgument<uint8_t>::Sanitize(arg_d));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_widening_unsigned_wide(
    SafeSyscallArgument<uint64_t>::RawType arg_a, SafeSyscallArgument<uint32_t>::RawType arg_b,
    SafeSyscallArgument<uint16_t>::RawType arg_c, SafeSyscallArgument<uint8_t>::RawType arg_d,
    uint64_t pc);
syscall_result wrapper_syscall_test_widening_unsigned_wide(
    SafeSyscallArgument<uint64_t>::RawType arg_a, SafeSyscallArgument<uint32_t>::RawType arg_b,
    SafeSyscallArgument<uint16_t>::RawType arg_c, SafeSyscallArgument<uint8_t>::RawType arg_d,
    uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_widening_unsigned_wide, pc,
                    &VDso::ValidSyscallPC::syscall_test_widening_unsigned_wide,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_widening_unsigned_wide(
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_a),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_b),
                          SafeSyscallArgument<uint16_t>::Sanitize(arg_c),
                          SafeSyscallArgument<uint8_t>::Sanitize(arg_d));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_wrapper(SafeSyscallArgument<int32_t>::RawType arg_a,
                                            SafeSyscallArgument<int32_t>::RawType arg_b,
                                            SafeSyscallArgument<int32_t>::RawType arg_c,
                                            uint64_t pc);
syscall_result wrapper_syscall_test_wrapper(SafeSyscallArgument<int32_t>::RawType arg_a,
                                            SafeSyscallArgument<int32_t>::RawType arg_b,
                                            SafeSyscallArgument<int32_t>::RawType arg_c,
                                            uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_wrapper, pc, &VDso::ValidSyscallPC::syscall_test_wrapper,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_wrapper(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                                   SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                                   SafeSyscallArgument<int32_t>::Sanitize(arg_c));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_0(uint64_t pc);
syscall_result wrapper_syscall_test_0(uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_0, pc, &VDso::ValidSyscallPC::syscall_test_0,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_syscall_test_0();
                      return result;
                    });
}

syscall_result wrapper_syscall_test_1(SafeSyscallArgument<int32_t>::RawType arg_a, uint64_t pc);
syscall_result wrapper_syscall_test_1(SafeSyscallArgument<int32_t>::RawType arg_a, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_1, pc, &VDso::ValidSyscallPC::syscall_test_1,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_1(SafeSyscallArgument<int32_t>::Sanitize(arg_a));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_2(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b, uint64_t pc);
syscall_result wrapper_syscall_test_2(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_2, pc, &VDso::ValidSyscallPC::syscall_test_2,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_2(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_3(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c, uint64_t pc);
syscall_result wrapper_syscall_test_3(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_3, pc, &VDso::ValidSyscallPC::syscall_test_3,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_3(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_c));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_4(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d, uint64_t pc);
syscall_result wrapper_syscall_test_4(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_4, pc, &VDso::ValidSyscallPC::syscall_test_4,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_4(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_d));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_5(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e, uint64_t pc);
syscall_result wrapper_syscall_test_5(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_5, pc, &VDso::ValidSyscallPC::syscall_test_5,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_5(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_e));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_6(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e,
                                      SafeSyscallArgument<int32_t>::RawType arg_f, uint64_t pc);
syscall_result wrapper_syscall_test_6(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e,
                                      SafeSyscallArgument<int32_t>::RawType arg_f, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_6, pc, &VDso::ValidSyscallPC::syscall_test_6,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_6(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_e),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_f));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_7(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e,
                                      SafeSyscallArgument<int32_t>::RawType arg_f,
                                      SafeSyscallArgument<int32_t>::RawType arg_g, uint64_t pc);
syscall_result wrapper_syscall_test_7(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e,
                                      SafeSyscallArgument<int32_t>::RawType arg_f,
                                      SafeSyscallArgument<int32_t>::RawType arg_g, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_7, pc, &VDso::ValidSyscallPC::syscall_test_7,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_7(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_e),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_f),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_g));
                      return result;
                    });
}

syscall_result wrapper_syscall_test_8(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e,
                                      SafeSyscallArgument<int32_t>::RawType arg_f,
                                      SafeSyscallArgument<int32_t>::RawType arg_g,
                                      SafeSyscallArgument<int32_t>::RawType arg_h, uint64_t pc);
syscall_result wrapper_syscall_test_8(SafeSyscallArgument<int32_t>::RawType arg_a,
                                      SafeSyscallArgument<int32_t>::RawType arg_b,
                                      SafeSyscallArgument<int32_t>::RawType arg_c,
                                      SafeSyscallArgument<int32_t>::RawType arg_d,
                                      SafeSyscallArgument<int32_t>::RawType arg_e,
                                      SafeSyscallArgument<int32_t>::RawType arg_f,
                                      SafeSyscallArgument<int32_t>::RawType arg_g,
                                      SafeSyscallArgument<int32_t>::RawType arg_h, uint64_t pc) {
  return do_syscall(ZX_SYS_syscall_test_8, pc, &VDso::ValidSyscallPC::syscall_test_8,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_syscall_test_8(SafeSyscallArgument<int32_t>::Sanitize(arg_a),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_b),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_c),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_d),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_e),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_f),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_g),
                                             SafeSyscallArgument<int32_t>::Sanitize(arg_h));
                      return result;
                    });
}

syscall_result wrapper_system_get_event(SafeSyscallArgument<zx_handle_t>::RawType arg_root_job,
                                        SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_event,
                                        uint64_t pc);
syscall_result wrapper_system_get_event(SafeSyscallArgument<zx_handle_t>::RawType arg_root_job,
                                        SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_event,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_system_get_event, pc, &VDso::ValidSyscallPC::system_get_event,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_event = ZX_HANDLE_INVALID;
                      auto result = sys_system_get_event(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_root_job),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_kind), &out_handle_event);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_event))
                              .copy_to_user(out_handle_event);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_system_get_performance_info(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_topic,
    SafeSyscallArgument<size_t>::RawType arg_count, SafeSyscallArgument<void*>::RawType arg_info,
    SafeSyscallArgument<size_t*>::RawType arg_output_count, uint64_t pc);
syscall_result wrapper_system_get_performance_info(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_topic,
    SafeSyscallArgument<size_t>::RawType arg_count, SafeSyscallArgument<void*>::RawType arg_info,
    SafeSyscallArgument<size_t*>::RawType arg_output_count, uint64_t pc) {
  return do_syscall(
      ZX_SYS_system_get_performance_info, pc, &VDso::ValidSyscallPC::system_get_performance_info,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_system_get_performance_info(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_topic),
            SafeSyscallArgument<size_t>::Sanitize(arg_count),
            make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_info)),
            make_user_out_ptr(SafeSyscallArgument<size_t*>::Sanitize(arg_output_count)));
        return result;
      });
}

syscall_result wrapper_system_mexec(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                    SafeSyscallArgument<zx_handle_t>::RawType arg_kernel_vmo,
                                    SafeSyscallArgument<zx_handle_t>::RawType arg_bootimage_vmo,
                                    uint64_t pc);
syscall_result wrapper_system_mexec(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                    SafeSyscallArgument<zx_handle_t>::RawType arg_kernel_vmo,
                                    SafeSyscallArgument<zx_handle_t>::RawType arg_bootimage_vmo,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_system_mexec, pc, &VDso::ValidSyscallPC::system_mexec,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_system_mexec(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_kernel_vmo),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_bootimage_vmo));
                      return result;
                    });
}

syscall_result wrapper_system_mexec_payload_get(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<void*>::RawType arg_buffer,
    SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc);
syscall_result wrapper_system_mexec_payload_get(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<void*>::RawType arg_buffer,
    SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc) {
  return do_syscall(ZX_SYS_system_mexec_payload_get, pc,
                    &VDso::ValidSyscallPC::system_mexec_payload_get,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_system_mexec_payload_get(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_system_powerctl(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_cmd,
    SafeSyscallArgument<const zx_system_powerctl_arg_t*>::RawType arg_arg, uint64_t pc);
syscall_result wrapper_system_powerctl(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_cmd,
    SafeSyscallArgument<const zx_system_powerctl_arg_t*>::RawType arg_arg, uint64_t pc) {
  return do_syscall(
      ZX_SYS_system_powerctl, pc, &VDso::ValidSyscallPC::system_powerctl,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_system_powerctl(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_cmd),
            make_user_in_ptr(
                SafeSyscallArgument<const zx_system_powerctl_arg_t*>::Sanitize(arg_arg)));
        return result;
      });
}

syscall_result wrapper_system_set_performance_info(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_topic,
    SafeSyscallArgument<const void*>::RawType arg_info,
    SafeSyscallArgument<size_t>::RawType arg_count, uint64_t pc);
syscall_result wrapper_system_set_performance_info(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_topic,
    SafeSyscallArgument<const void*>::RawType arg_info,
    SafeSyscallArgument<size_t>::RawType arg_count, uint64_t pc) {
  return do_syscall(ZX_SYS_system_set_performance_info, pc,
                    &VDso::ValidSyscallPC::system_set_performance_info,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_system_set_performance_info(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_topic),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_info)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_count));
                      return result;
                    });
}

syscall_result wrapper_system_suspend_enter(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<zx_instant_boot_t>::RawType arg_resume_deadline,
    SafeSyscallArgument<zx_system_suspend_option_t>::RawType arg_options,
    SafeSyscallArgument<zx_wake_source_report_header_t*>::RawType arg_header,
    SafeSyscallArgument<zx_wake_source_report_entry_t*>::RawType arg_entries,
    SafeSyscallArgument<uint32_t>::RawType arg_num_entries,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_entries, uint64_t pc);
syscall_result wrapper_system_suspend_enter(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<zx_instant_boot_t>::RawType arg_resume_deadline,
    SafeSyscallArgument<zx_system_suspend_option_t>::RawType arg_options,
    SafeSyscallArgument<zx_wake_source_report_header_t*>::RawType arg_header,
    SafeSyscallArgument<zx_wake_source_report_entry_t*>::RawType arg_entries,
    SafeSyscallArgument<uint32_t>::RawType arg_num_entries,
    SafeSyscallArgument<uint32_t*>::RawType arg_actual_entries, uint64_t pc) {
  return do_syscall(
      ZX_SYS_system_suspend_enter, pc, &VDso::ValidSyscallPC::system_suspend_enter,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_system_suspend_enter(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
            SafeSyscallArgument<zx_instant_boot_t>::Sanitize(arg_resume_deadline),
            SafeSyscallArgument<zx_system_suspend_option_t>::Sanitize(arg_options),
            make_user_out_ptr(
                SafeSyscallArgument<zx_wake_source_report_header_t*>::Sanitize(arg_header)),
            make_user_out_ptr(
                SafeSyscallArgument<zx_wake_source_report_entry_t*>::Sanitize(arg_entries)),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_num_entries),
            make_user_out_ptr(SafeSyscallArgument<uint32_t*>::Sanitize(arg_actual_entries)));
        return result;
      });
}

syscall_result wrapper_system_watch_memory_stall(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_kind,
    SafeSyscallArgument<zx_duration_t>::RawType arg_threshold,
    SafeSyscallArgument<zx_duration_t>::RawType arg_window,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_event, uint64_t pc);
syscall_result wrapper_system_watch_memory_stall(
    SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
    SafeSyscallArgument<uint32_t>::RawType arg_kind,
    SafeSyscallArgument<zx_duration_t>::RawType arg_threshold,
    SafeSyscallArgument<zx_duration_t>::RawType arg_window,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_event, uint64_t pc) {
  return do_syscall(
      ZX_SYS_system_watch_memory_stall, pc, &VDso::ValidSyscallPC::system_watch_memory_stall,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_event = ZX_HANDLE_INVALID;
        auto result = sys_system_watch_memory_stall(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_kind),
            SafeSyscallArgument<zx_duration_t>::Sanitize(arg_threshold),
            SafeSyscallArgument<zx_duration_t>::Sanitize(arg_window), &out_handle_event);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_event))
                     .copy_to_user(out_handle_event);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_task_create_exception_channel(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_task_create_exception_channel(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(ZX_SYS_task_create_exception_channel, pc,
                    &VDso::ValidSyscallPC::task_create_exception_channel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_task_create_exception_channel(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_task_kill(SafeSyscallArgument<zx_handle_t>::RawType arg_handle, uint64_t pc);
syscall_result wrapper_task_kill(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 uint64_t pc) {
  return do_syscall(ZX_SYS_task_kill, pc, &VDso::ValidSyscallPC::task_kill,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_task_kill(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_task_suspend(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_token,
                                    uint64_t pc);
syscall_result wrapper_task_suspend(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_token,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_task_suspend, pc, &VDso::ValidSyscallPC::task_suspend,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_token = ZX_HANDLE_INVALID;
        auto result = sys_task_suspend(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                       &out_handle_token);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_token))
                     .copy_to_user(out_handle_token);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_task_suspend_token(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<zx_handle_t*>::RawType arg_token,
                                          uint64_t pc);
syscall_result wrapper_task_suspend_token(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<zx_handle_t*>::RawType arg_token,
                                          uint64_t pc) {
  return do_syscall(
      ZX_SYS_task_suspend_token, pc, &VDso::ValidSyscallPC::task_suspend_token,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_token = ZX_HANDLE_INVALID;
        auto result = sys_task_suspend_token(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                             &out_handle_token);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_token))
                     .copy_to_user(out_handle_token);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_thread_create(SafeSyscallArgument<zx_handle_t>::RawType arg_process,
                                     SafeSyscallArgument<const char*>::RawType arg_name,
                                     SafeSyscallArgument<size_t>::RawType arg_name_size,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                     uint64_t pc);
syscall_result wrapper_thread_create(SafeSyscallArgument<zx_handle_t>::RawType arg_process,
                                     SafeSyscallArgument<const char*>::RawType arg_name,
                                     SafeSyscallArgument<size_t>::RawType arg_name_size,
                                     SafeSyscallArgument<uint32_t>::RawType arg_options,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                     uint64_t pc) {
  return do_syscall(ZX_SYS_thread_create, pc, &VDso::ValidSyscallPC::thread_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_thread_create(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_process),
                          make_user_in_ptr(SafeSyscallArgument<const char*>::Sanitize(arg_name)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_name_size),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_thread_exit(uint64_t pc);
syscall_result wrapper_thread_exit(uint64_t pc) {
  return do_syscall(ZX_SYS_thread_exit, pc, &VDso::ValidSyscallPC::thread_exit,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      /*noreturn*/ sys_thread_exit();
                      /* NOTREACHED */
                      return ZX_ERR_BAD_STATE;
                    });
}

syscall_result wrapper_thread_legacy_yield(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                           uint64_t pc);
syscall_result wrapper_thread_legacy_yield(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                           uint64_t pc) {
  return do_syscall(
      ZX_SYS_thread_legacy_yield, pc, &VDso::ValidSyscallPC::thread_legacy_yield,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_thread_legacy_yield(SafeSyscallArgument<uint32_t>::Sanitize(arg_options));
        return result;
      });
}

syscall_result wrapper_thread_raise_exception(
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<uint32_t>::RawType arg_type,
    SafeSyscallArgument<const zx_exception_context_t*>::RawType arg_context, uint64_t pc);
syscall_result wrapper_thread_raise_exception(
    SafeSyscallArgument<uint32_t>::RawType arg_options,
    SafeSyscallArgument<uint32_t>::RawType arg_type,
    SafeSyscallArgument<const zx_exception_context_t*>::RawType arg_context, uint64_t pc) {
  return do_syscall(
      ZX_SYS_thread_raise_exception, pc, &VDso::ValidSyscallPC::thread_raise_exception,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_thread_raise_exception(
            SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_type),
            make_user_in_ptr(
                SafeSyscallArgument<const zx_exception_context_t*>::Sanitize(arg_context)));
        return result;
      });
}

syscall_result wrapper_thread_read_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                         SafeSyscallArgument<void*>::RawType arg_buffer,
                                         SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                         uint64_t pc);
syscall_result wrapper_thread_read_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                         SafeSyscallArgument<void*>::RawType arg_buffer,
                                         SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_thread_read_state, pc, &VDso::ValidSyscallPC::thread_read_state,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_thread_read_state(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_kind),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_thread_set_rseq(SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                       SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                       SafeSyscallArgument<uint64_t>::RawType arg_size,
                                       uint64_t pc);
syscall_result wrapper_thread_set_rseq(SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                       SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                       SafeSyscallArgument<uint64_t>::RawType arg_size,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_thread_set_rseq, pc, &VDso::ValidSyscallPC::thread_set_rseq,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_thread_set_rseq(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmo),
                                              SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                                              SafeSyscallArgument<uint64_t>::Sanitize(arg_size));
                      return result;
                    });
}

syscall_result wrapper_thread_start_regs(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint64_t>::RawType arg_pc,
                                         SafeSyscallArgument<uint64_t>::RawType arg_sp,
                                         SafeSyscallArgument<uint64_t>::RawType arg_arg1,
                                         SafeSyscallArgument<uint64_t>::RawType arg_arg2,
                                         SafeSyscallArgument<uint64_t>::RawType arg_tp,
                                         SafeSyscallArgument<uint64_t>::RawType arg_abi_reg,
                                         uint64_t pc);
syscall_result wrapper_thread_start_regs(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                         SafeSyscallArgument<uint64_t>::RawType arg_pc,
                                         SafeSyscallArgument<uint64_t>::RawType arg_sp,
                                         SafeSyscallArgument<uint64_t>::RawType arg_arg1,
                                         SafeSyscallArgument<uint64_t>::RawType arg_arg2,
                                         SafeSyscallArgument<uint64_t>::RawType arg_tp,
                                         SafeSyscallArgument<uint64_t>::RawType arg_abi_reg,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_thread_start_regs, pc, &VDso::ValidSyscallPC::thread_start_regs,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_thread_start_regs(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_pc),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_sp),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_arg1),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_arg2),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_tp),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_abi_reg));
                      return result;
                    });
}

syscall_result wrapper_thread_write_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                          SafeSyscallArgument<const void*>::RawType arg_buffer,
                                          SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                          uint64_t pc);
syscall_result wrapper_thread_write_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                          SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                          SafeSyscallArgument<const void*>::RawType arg_buffer,
                                          SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                          uint64_t pc) {
  return do_syscall(ZX_SYS_thread_write_state, pc, &VDso::ValidSyscallPC::thread_write_state,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_thread_write_state(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_kind),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_ticks_get_boot_via_kernel(uint64_t pc);
syscall_result wrapper_ticks_get_boot_via_kernel(uint64_t pc) {
  return do_syscall(ZX_SYS_ticks_get_boot_via_kernel, pc,
                    &VDso::ValidSyscallPC::ticks_get_boot_via_kernel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_ticks_get_boot_via_kernel();
                      return result;
                    });
}

syscall_result wrapper_ticks_get_via_kernel(uint64_t pc);
syscall_result wrapper_ticks_get_via_kernel(uint64_t pc) {
  return do_syscall(ZX_SYS_ticks_get_via_kernel, pc, &VDso::ValidSyscallPC::ticks_get_via_kernel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_ticks_get_via_kernel();
                      return result;
                    });
}

syscall_result wrapper_timer_cancel(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    uint64_t pc);
syscall_result wrapper_timer_cancel(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_timer_cancel, pc, &VDso::ValidSyscallPC::timer_cancel,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_timer_cancel(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_timer_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_clock_t>::RawType arg_clock_id,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc);
syscall_result wrapper_timer_create(SafeSyscallArgument<uint32_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_clock_t>::RawType arg_clock_id,
                                    SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_timer_create, pc, &VDso::ValidSyscallPC::timer_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_timer_create(
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_clock_t>::Sanitize(arg_clock_id), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_timer_set(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<zx_time_t>::RawType arg_deadline,
                                 SafeSyscallArgument<zx_duration_t>::RawType arg_slack,
                                 uint64_t pc);
syscall_result wrapper_timer_set(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<zx_time_t>::RawType arg_deadline,
                                 SafeSyscallArgument<zx_duration_t>::RawType arg_slack,
                                 uint64_t pc) {
  return do_syscall(ZX_SYS_timer_set, pc, &VDso::ValidSyscallPC::timer_set,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_timer_set(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                        SafeSyscallArgument<zx_time_t>::Sanitize(arg_deadline),
                                        SafeSyscallArgument<zx_duration_t>::Sanitize(arg_slack));
                      return result;
                    });
}

syscall_result wrapper_vcpu_create(SafeSyscallArgument<zx_handle_t>::RawType arg_guest,
                                   SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<zx_vaddr_t>::RawType arg_entry,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_vcpu_create(SafeSyscallArgument<zx_handle_t>::RawType arg_guest,
                                   SafeSyscallArgument<uint32_t>::RawType arg_options,
                                   SafeSyscallArgument<zx_vaddr_t>::RawType arg_entry,
                                   SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                   uint64_t pc) {
  return do_syscall(ZX_SYS_vcpu_create, pc, &VDso::ValidSyscallPC::vcpu_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_vcpu_create(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_guest),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_entry), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_vcpu_enter(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_port_packet_t*>::RawType arg_packet,
                                  uint64_t pc);
syscall_result wrapper_vcpu_enter(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_port_packet_t*>::RawType arg_packet,
                                  uint64_t pc) {
  return do_syscall(
      ZX_SYS_vcpu_enter, pc, &VDso::ValidSyscallPC::vcpu_enter,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_vcpu_enter(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            make_user_out_ptr(SafeSyscallArgument<zx_port_packet_t*>::Sanitize(arg_packet)));
        return result;
      });
}

syscall_result wrapper_vcpu_interrupt(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_vector,
                                      uint64_t pc);
syscall_result wrapper_vcpu_interrupt(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<uint32_t>::RawType arg_vector,
                                      uint64_t pc) {
  return do_syscall(ZX_SYS_vcpu_interrupt, pc, &VDso::ValidSyscallPC::vcpu_interrupt,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_vcpu_interrupt(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                             SafeSyscallArgument<uint32_t>::Sanitize(arg_vector));
                      return result;
                    });
}

syscall_result wrapper_vcpu_kick(SafeSyscallArgument<zx_handle_t>::RawType arg_handle, uint64_t pc);
syscall_result wrapper_vcpu_kick(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 uint64_t pc) {
  return do_syscall(ZX_SYS_vcpu_kick, pc, &VDso::ValidSyscallPC::vcpu_kick,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_vcpu_kick(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_vcpu_read_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                       SafeSyscallArgument<void*>::RawType arg_buffer,
                                       SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                       uint64_t pc);
syscall_result wrapper_vcpu_read_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                       SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                       SafeSyscallArgument<void*>::RawType arg_buffer,
                                       SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                       uint64_t pc) {
  return do_syscall(ZX_SYS_vcpu_read_state, pc, &VDso::ValidSyscallPC::vcpu_read_state,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vcpu_read_state(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_kind),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_vcpu_write_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                        SafeSyscallArgument<const void*>::RawType arg_buffer,
                                        SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                        uint64_t pc);
syscall_result wrapper_vcpu_write_state(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_kind,
                                        SafeSyscallArgument<const void*>::RawType arg_buffer,
                                        SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_vcpu_write_state, pc, &VDso::ValidSyscallPC::vcpu_write_state,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vcpu_write_state(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_kind),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_vmar_allocate(SafeSyscallArgument<zx_handle_t>::RawType arg_parent_vmar,
                                     SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                     SafeSyscallArgument<size_t>::RawType arg_offset,
                                     SafeSyscallArgument<size_t>::RawType arg_size,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_child_vmar,
                                     SafeSyscallArgument<zx_vaddr_t*>::RawType arg_child_addr,
                                     uint64_t pc);
syscall_result wrapper_vmar_allocate(SafeSyscallArgument<zx_handle_t>::RawType arg_parent_vmar,
                                     SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                     SafeSyscallArgument<size_t>::RawType arg_offset,
                                     SafeSyscallArgument<size_t>::RawType arg_size,
                                     SafeSyscallArgument<zx_handle_t*>::RawType arg_child_vmar,
                                     SafeSyscallArgument<zx_vaddr_t*>::RawType arg_child_addr,
                                     uint64_t pc) {
  return do_syscall(
      ZX_SYS_vmar_allocate, pc, &VDso::ValidSyscallPC::vmar_allocate,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_child_vmar = ZX_HANDLE_INVALID;
        auto result = sys_vmar_allocate(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_parent_vmar),
            SafeSyscallArgument<zx_vm_option_t>::Sanitize(arg_options),
            SafeSyscallArgument<size_t>::Sanitize(arg_offset),
            SafeSyscallArgument<size_t>::Sanitize(arg_size), &out_handle_child_vmar,
            make_user_out_ptr(SafeSyscallArgument<zx_vaddr_t*>::Sanitize(arg_child_addr)));
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_child_vmar))
                     .copy_to_user(out_handle_child_vmar);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_vmar_destroy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    uint64_t pc);
syscall_result wrapper_vmar_destroy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_vmar_destroy, pc, &VDso::ValidSyscallPC::vmar_destroy,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_vmar_destroy(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle));
                      return result;
                    });
}

syscall_result wrapper_vmar_map(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                SafeSyscallArgument<size_t>::RawType arg_vmar_offset,
                                SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                SafeSyscallArgument<uint64_t>::RawType arg_vmo_offset,
                                SafeSyscallArgument<size_t>::RawType arg_len,
                                SafeSyscallArgument<zx_vaddr_t*>::RawType arg_mapped_addr,
                                uint64_t pc);
syscall_result wrapper_vmar_map(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                SafeSyscallArgument<size_t>::RawType arg_vmar_offset,
                                SafeSyscallArgument<zx_handle_t>::RawType arg_vmo,
                                SafeSyscallArgument<uint64_t>::RawType arg_vmo_offset,
                                SafeSyscallArgument<size_t>::RawType arg_len,
                                SafeSyscallArgument<zx_vaddr_t*>::RawType arg_mapped_addr,
                                uint64_t pc) {
  return do_syscall(
      ZX_SYS_vmar_map, pc, &VDso::ValidSyscallPC::vmar_map,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_vmar_map(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_vm_option_t>::Sanitize(arg_options),
            SafeSyscallArgument<size_t>::Sanitize(arg_vmar_offset),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmo),
            SafeSyscallArgument<uint64_t>::Sanitize(arg_vmo_offset),
            SafeSyscallArgument<size_t>::Sanitize(arg_len),
            make_user_out_ptr(SafeSyscallArgument<zx_vaddr_t*>::Sanitize(arg_mapped_addr)));
        return result;
      });
}

syscall_result wrapper_vmar_map_clock(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                      SafeSyscallArgument<size_t>::RawType arg_vmar_offset,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_clock,
                                      SafeSyscallArgument<size_t>::RawType arg_len,
                                      SafeSyscallArgument<zx_vaddr_t*>::RawType arg_mapped_addr,
                                      uint64_t pc);
syscall_result wrapper_vmar_map_clock(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                      SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                      SafeSyscallArgument<size_t>::RawType arg_vmar_offset,
                                      SafeSyscallArgument<zx_handle_t>::RawType arg_clock,
                                      SafeSyscallArgument<size_t>::RawType arg_len,
                                      SafeSyscallArgument<zx_vaddr_t*>::RawType arg_mapped_addr,
                                      uint64_t pc) {
  return do_syscall(
      ZX_SYS_vmar_map_clock, pc, &VDso::ValidSyscallPC::vmar_map_clock,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_vmar_map_clock(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_vm_option_t>::Sanitize(arg_options),
            SafeSyscallArgument<size_t>::Sanitize(arg_vmar_offset),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_clock),
            SafeSyscallArgument<size_t>::Sanitize(arg_len),
            make_user_out_ptr(SafeSyscallArgument<zx_vaddr_t*>::Sanitize(arg_mapped_addr)));
        return result;
      });
}

syscall_result wrapper_vmar_map_iob(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                    SafeSyscallArgument<size_t>::RawType arg_vmar_offset,
                                    SafeSyscallArgument<zx_handle_t>::RawType arg_ep,
                                    SafeSyscallArgument<uint32_t>::RawType arg_region_index,
                                    SafeSyscallArgument<uint64_t>::RawType arg_region_offset,
                                    SafeSyscallArgument<size_t>::RawType arg_region_length,
                                    SafeSyscallArgument<zx_vaddr_t*>::RawType arg_mapped_addr,
                                    uint64_t pc);
syscall_result wrapper_vmar_map_iob(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                    SafeSyscallArgument<size_t>::RawType arg_vmar_offset,
                                    SafeSyscallArgument<zx_handle_t>::RawType arg_ep,
                                    SafeSyscallArgument<uint32_t>::RawType arg_region_index,
                                    SafeSyscallArgument<uint64_t>::RawType arg_region_offset,
                                    SafeSyscallArgument<size_t>::RawType arg_region_length,
                                    SafeSyscallArgument<zx_vaddr_t*>::RawType arg_mapped_addr,
                                    uint64_t pc) {
  return do_syscall(
      ZX_SYS_vmar_map_iob, pc, &VDso::ValidSyscallPC::vmar_map_iob,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        auto result = sys_vmar_map_iob(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_vm_option_t>::Sanitize(arg_options),
            SafeSyscallArgument<size_t>::Sanitize(arg_vmar_offset),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_ep),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_region_index),
            SafeSyscallArgument<uint64_t>::Sanitize(arg_region_offset),
            SafeSyscallArgument<size_t>::Sanitize(arg_region_length),
            make_user_out_ptr(SafeSyscallArgument<zx_vaddr_t*>::Sanitize(arg_mapped_addr)));
        return result;
      });
}

syscall_result wrapper_vmar_op_range(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_op,
                                     SafeSyscallArgument<zx_vaddr_t>::RawType arg_address,
                                     SafeSyscallArgument<size_t>::RawType arg_size,
                                     SafeSyscallArgument<void*>::RawType arg_buffer,
                                     SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                     uint64_t pc);
syscall_result wrapper_vmar_op_range(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                     SafeSyscallArgument<uint32_t>::RawType arg_op,
                                     SafeSyscallArgument<zx_vaddr_t>::RawType arg_address,
                                     SafeSyscallArgument<size_t>::RawType arg_size,
                                     SafeSyscallArgument<void*>::RawType arg_buffer,
                                     SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                     uint64_t pc) {
  return do_syscall(ZX_SYS_vmar_op_range, pc, &VDso::ValidSyscallPC::vmar_op_range,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmar_op_range(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_op),
                          SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_address),
                          SafeSyscallArgument<size_t>::Sanitize(arg_size),
                          make_user_inout_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_vmar_protect(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_vaddr_t>::RawType arg_addr,
                                    SafeSyscallArgument<size_t>::RawType arg_len, uint64_t pc);
syscall_result wrapper_vmar_protect(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<zx_vm_option_t>::RawType arg_options,
                                    SafeSyscallArgument<zx_vaddr_t>::RawType arg_addr,
                                    SafeSyscallArgument<size_t>::RawType arg_len, uint64_t pc) {
  return do_syscall(ZX_SYS_vmar_protect, pc, &VDso::ValidSyscallPC::vmar_protect,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmar_protect(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<zx_vm_option_t>::Sanitize(arg_options),
                          SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_addr),
                          SafeSyscallArgument<size_t>::Sanitize(arg_len));
                      return result;
                    });
}

syscall_result wrapper_vmar_unmap(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_vaddr_t>::RawType arg_addr,
                                  SafeSyscallArgument<size_t>::RawType arg_len, uint64_t pc);
syscall_result wrapper_vmar_unmap(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                  SafeSyscallArgument<zx_vaddr_t>::RawType arg_addr,
                                  SafeSyscallArgument<size_t>::RawType arg_len, uint64_t pc) {
  return do_syscall(ZX_SYS_vmar_unmap, pc, &VDso::ValidSyscallPC::vmar_unmap,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_vmar_unmap(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                         SafeSyscallArgument<zx_vaddr_t>::Sanitize(arg_addr),
                                         SafeSyscallArgument<size_t>::Sanitize(arg_len));
                      return result;
                    });
}

syscall_result wrapper_vmo_create(SafeSyscallArgument<uint64_t>::RawType arg_size,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_vmo_create(SafeSyscallArgument<uint64_t>::RawType arg_size,
                                  SafeSyscallArgument<uint32_t>::RawType arg_options,
                                  SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_create, pc, &VDso::ValidSyscallPC::vmo_create,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_vmo_create(
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_size),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_vmo_create_child(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                        SafeSyscallArgument<uint64_t>::RawType arg_size,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc);
syscall_result wrapper_vmo_create_child(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                        SafeSyscallArgument<uint32_t>::RawType arg_options,
                                        SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                        SafeSyscallArgument<uint64_t>::RawType arg_size,
                                        SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                        uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_create_child, pc, &VDso::ValidSyscallPC::vmo_create_child,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_vmo_create_child(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_size), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_vmo_create_contiguous(
    SafeSyscallArgument<zx_handle_t>::RawType arg_bti,
    SafeSyscallArgument<size_t>::RawType arg_size,
    SafeSyscallArgument<uint32_t>::RawType arg_alignment_log2,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_vmo_create_contiguous(
    SafeSyscallArgument<zx_handle_t>::RawType arg_bti,
    SafeSyscallArgument<size_t>::RawType arg_size,
    SafeSyscallArgument<uint32_t>::RawType arg_alignment_log2,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(
      ZX_SYS_vmo_create_contiguous, pc, &VDso::ValidSyscallPC::vmo_create_contiguous,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
        auto result = sys_vmo_create_contiguous(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_bti),
            SafeSyscallArgument<size_t>::Sanitize(arg_size),
            SafeSyscallArgument<uint32_t>::Sanitize(arg_alignment_log2), &out_handle_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                     .copy_to_user(out_handle_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_vmo_create_physical(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                           SafeSyscallArgument<zx_paddr_t>::RawType arg_paddr,
                                           SafeSyscallArgument<size_t>::RawType arg_size,
                                           SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                           uint64_t pc);
syscall_result wrapper_vmo_create_physical(SafeSyscallArgument<zx_handle_t>::RawType arg_resource,
                                           SafeSyscallArgument<zx_paddr_t>::RawType arg_paddr,
                                           SafeSyscallArgument<size_t>::RawType arg_size,
                                           SafeSyscallArgument<zx_handle_t*>::RawType arg_out,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_create_physical, pc, &VDso::ValidSyscallPC::vmo_create_physical,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
                      auto result = sys_vmo_create_physical(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_resource),
                          SafeSyscallArgument<zx_paddr_t>::Sanitize(arg_paddr),
                          SafeSyscallArgument<size_t>::Sanitize(arg_size), &out_handle_out);
                      if (result != ZX_OK)
                        return result;
                      result =
                          make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                              .copy_to_user(out_handle_out);
                      if (result != ZX_OK) {
                        Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
                      }
                      return result;
                    });
}

syscall_result wrapper_vmo_get_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint64_t*>::RawType arg_size, uint64_t pc);
syscall_result wrapper_vmo_get_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint64_t*>::RawType arg_size, uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_get_size, pc, &VDso::ValidSyscallPC::vmo_get_size,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_get_size(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<uint64_t*>::Sanitize(arg_size)));
                      return result;
                    });
}

syscall_result wrapper_vmo_get_stream_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint64_t*>::RawType arg_size,
                                           uint64_t pc);
syscall_result wrapper_vmo_get_stream_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint64_t*>::RawType arg_size,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_get_stream_size, pc, &VDso::ValidSyscallPC::vmo_get_stream_size,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_get_stream_size(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<uint64_t*>::Sanitize(arg_size)));
                      return result;
                    });
}

syscall_result wrapper_vmo_op_range(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_op,
                                    SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                    SafeSyscallArgument<uint64_t>::RawType arg_size,
                                    SafeSyscallArgument<void*>::RawType arg_buffer,
                                    SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                    uint64_t pc);
syscall_result wrapper_vmo_op_range(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint32_t>::RawType arg_op,
                                    SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                    SafeSyscallArgument<uint64_t>::RawType arg_size,
                                    SafeSyscallArgument<void*>::RawType arg_buffer,
                                    SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                    uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_op_range, pc, &VDso::ValidSyscallPC::vmo_op_range,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_op_range(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_op),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_size),
                          make_user_inout_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_vmo_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                SafeSyscallArgument<void*>::RawType arg_buffer,
                                SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc);
syscall_result wrapper_vmo_read(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                SafeSyscallArgument<void*>::RawType arg_buffer,
                                SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_read, pc, &VDso::ValidSyscallPC::vmo_read,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_read(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_out_ptr(SafeSyscallArgument<void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}

syscall_result wrapper_vmo_replace_as_executable(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<zx_handle_t>::RawType arg_vmex,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc);
syscall_result wrapper_vmo_replace_as_executable(
    SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
    SafeSyscallArgument<zx_handle_t>::RawType arg_vmex,
    SafeSyscallArgument<zx_handle_t*>::RawType arg_out, uint64_t pc) {
  return do_syscall(
      ZX_SYS_vmo_replace_as_executable, pc, &VDso::ValidSyscallPC::vmo_replace_as_executable,
      [&](ProcessDispatcher* current_process) -> uint64_t {
        zx_handle_t out_handle_out = ZX_HANDLE_INVALID;
        auto result = sys_vmo_replace_as_executable(
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
            SafeSyscallArgument<zx_handle_t>::Sanitize(arg_vmex), &out_handle_out);
        if (result != ZX_OK)
          return result;
        result = make_user_out_ptr(SafeSyscallArgument<zx_handle_t*>::Sanitize(arg_out))
                     .copy_to_user(out_handle_out);
        if (result != ZX_OK) {
          Thread::Current::SignalPolicyException(ZX_EXCP_POLICY_CODE_HANDLE_LEAK, 0u);
        }
        return result;
      });
}

syscall_result wrapper_vmo_set_cache_policy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                            SafeSyscallArgument<uint32_t>::RawType arg_cache_policy,
                                            uint64_t pc);
syscall_result wrapper_vmo_set_cache_policy(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                            SafeSyscallArgument<uint32_t>::RawType arg_cache_policy,
                                            uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_set_cache_policy, pc, &VDso::ValidSyscallPC::vmo_set_cache_policy,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_set_cache_policy(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_cache_policy));
                      return result;
                    });
}

syscall_result wrapper_vmo_set_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint64_t>::RawType arg_size, uint64_t pc);
syscall_result wrapper_vmo_set_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                    SafeSyscallArgument<uint64_t>::RawType arg_size, uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_set_size, pc, &VDso::ValidSyscallPC::vmo_set_size,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result =
                          sys_vmo_set_size(SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                                           SafeSyscallArgument<uint64_t>::Sanitize(arg_size));
                      return result;
                    });
}

syscall_result wrapper_vmo_set_stream_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint64_t>::RawType arg_size,
                                           uint64_t pc);
syscall_result wrapper_vmo_set_stream_size(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                           SafeSyscallArgument<uint64_t>::RawType arg_size,
                                           uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_set_stream_size, pc, &VDso::ValidSyscallPC::vmo_set_stream_size,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_set_stream_size(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_size));
                      return result;
                    });
}

syscall_result wrapper_vmo_transfer_data(SafeSyscallArgument<zx_handle_t>::RawType arg_dst_vmo,
                                         SafeSyscallArgument<uint32_t>::RawType arg_options,
                                         SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                         SafeSyscallArgument<uint64_t>::RawType arg_length,
                                         SafeSyscallArgument<zx_handle_t>::RawType arg_src_vmo,
                                         SafeSyscallArgument<uint64_t>::RawType arg_src_offset,
                                         uint64_t pc);
syscall_result wrapper_vmo_transfer_data(SafeSyscallArgument<zx_handle_t>::RawType arg_dst_vmo,
                                         SafeSyscallArgument<uint32_t>::RawType arg_options,
                                         SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                         SafeSyscallArgument<uint64_t>::RawType arg_length,
                                         SafeSyscallArgument<zx_handle_t>::RawType arg_src_vmo,
                                         SafeSyscallArgument<uint64_t>::RawType arg_src_offset,
                                         uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_transfer_data, pc, &VDso::ValidSyscallPC::vmo_transfer_data,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_transfer_data(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_dst_vmo),
                          SafeSyscallArgument<uint32_t>::Sanitize(arg_options),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_length),
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_src_vmo),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_src_offset));
                      return result;
                    });
}

syscall_result wrapper_vmo_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<const void*>::RawType arg_buffer,
                                 SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                 SafeSyscallArgument<size_t>::RawType arg_buffer_size, uint64_t pc);
syscall_result wrapper_vmo_write(SafeSyscallArgument<zx_handle_t>::RawType arg_handle,
                                 SafeSyscallArgument<const void*>::RawType arg_buffer,
                                 SafeSyscallArgument<uint64_t>::RawType arg_offset,
                                 SafeSyscallArgument<size_t>::RawType arg_buffer_size,
                                 uint64_t pc) {
  return do_syscall(ZX_SYS_vmo_write, pc, &VDso::ValidSyscallPC::vmo_write,
                    [&](ProcessDispatcher* current_process) -> uint64_t {
                      auto result = sys_vmo_write(
                          SafeSyscallArgument<zx_handle_t>::Sanitize(arg_handle),
                          make_user_in_ptr(SafeSyscallArgument<const void*>::Sanitize(arg_buffer)),
                          SafeSyscallArgument<uint64_t>::Sanitize(arg_offset),
                          SafeSyscallArgument<size_t>::Sanitize(arg_buffer_size));
                      return result;
                    });
}
}
