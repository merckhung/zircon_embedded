#if defined(__x86_64__)
#include <cpuid.h>
#endif

#include "src/shim/fd_ops.h"
#include "src/shim/shim_common.h"
#include "src/shim/signals.h"

extern "C" {
// 1 when the user extended state is saved with XSAVE, 0 for FXSAVE. Read by
// the assembly below; set once by XstateConfig() before any thread enters
// restricted mode.
__attribute__((visibility("hidden"))) uint8_t shim_use_xsave = 0;

zx_status_t restricted_enter_wrapper(uint32_t options, uintptr_t vector_table,
                                     uint64_t* reason_code, void* xstate);
void restricted_exit();
}

#if defined(__x86_64__)
// restricted_enter_wrapper(options, vector_table, reason_code, xstate):
// saves the callee-saved registers, the reason pointer, the xstate pointer and
// the SafeStack unsafe stack pointer (%fs:0x10) on the normal-mode stack, loads
// the Linux thread's x87/SSE/AVX state from `xstate`, and enters restricted
// mode with the stack pointer as the context value.
//
// restricted_exit(context, reason): the vector Zircon jumps to when restricted
// mode exits. It restores the stack, immediately saves the Linux thread's
// extended state into `xstate` (before any shim C++ code can clobber it;
// restricted and normal mode share the FPU registers), stores the reason code
// and returns 0 from restricted_enter_wrapper.
__asm__(
    ".text\n"
    ".globl restricted_enter_wrapper\n"
    "restricted_enter_wrapper:\n"
    "  push %rbx\n"
    "  push %rbp\n"
    "  push %r12\n"
    "  push %r13\n"
    "  push %r14\n"
    "  push %r15\n"
    "  push %rdx\n"
    "  push %rcx\n"
    "  mov %fs:0x10, %rax\n"
    "  push %rax\n"
    "  test %rcx, %rcx\n"
    "  jz .Lrew_enter\n"
    "  mov %rcx, %r8\n"
    "  mov $-1, %eax\n"
    "  mov $-1, %edx\n"
    "  cmpb $0, shim_use_xsave(%rip)\n"
    "  je .Lrew_fxrstor\n"
    "  xrstor64 (%r8)\n"
    "  jmp .Lrew_enter\n"
    ".Lrew_fxrstor:\n"
    "  fxrstor64 (%r8)\n"
    ".Lrew_enter:\n"
    "  mov %rsp, %rdx\n"
    "  call zx_restricted_enter@PLT\n"
    "  add $24, %rsp\n"
    "  pop %r15\n"
    "  pop %r14\n"
    "  pop %r13\n"
    "  pop %r12\n"
    "  pop %rbp\n"
    "  pop %rbx\n"
    "  ret\n"
    "\n"
    ".globl restricted_exit\n"
    "restricted_exit:\n"
    "  mov %rdi, %rsp\n"
    "  pop %rax\n"
    "  mov %rax, %fs:0x10\n"
    "  pop %r8\n"
    "  test %r8, %r8\n"
    "  jz .Lrex_saved\n"
    "  mov $-1, %eax\n"
    "  mov $-1, %edx\n"
    "  cmpb $0, shim_use_xsave(%rip)\n"
    "  je .Lrex_fxsave\n"
    "  xsave64 (%r8)\n"
    "  jmp .Lrex_saved\n"
    ".Lrex_fxsave:\n"
    "  fxsave64 (%r8)\n"
    ".Lrex_saved:\n"
    "  pop %rdx\n"
    "  mov %rsi, (%rdx)\n"
    "  pop %r15\n"
    "  pop %r14\n"
    "  pop %r13\n"
    "  pop %r12\n"
    "  pop %rbp\n"
    "  pop %rbx\n"
    "  xor %eax, %eax\n"
    "  ret\n");
#elif defined(__aarch64__)
extern "C" {
void load_fpu_registers(const void* src);
void store_fpu_registers(void* dst);
}
__asm__(
    ".text\n"
    ".globl restricted_exit\n"
    "restricted_exit:\n"
    "  mov sp, x0\n"
    "  ldr x0, [sp, #16*7 + 8]\n"
    "  cbz x0, 1f\n"
    "  bl store_fpu_registers\n"
    "1:\n"
    "  ldp x28, x27, [sp, #16*0]\n"
    "  ldp x26, x25, [sp, #16*1]\n"
    "  ldp x24, x23, [sp, #16*2]\n"
    "  ldp x22, x21, [sp, #16*3]\n"
    "  ldp x20, x19, [sp, #16*4]\n"
    "  ldp x2, x18, [sp, #16*5]\n"
    "  ldp x29, x30, [sp, #16*6]\n"
    "  add sp, sp, #16*8\n"
    "  str x1, [x2]\n"
    "  mov x0, xzr\n"
    "  ret\n"
    "\n"
    ".globl restricted_enter_wrapper\n"
    "restricted_enter_wrapper:\n"
    "  sub sp, sp, #16*8\n"
    "  stp x29, x30, [sp, #16*6]\n"
    "  add x29, sp, #16*6\n"
    "  stp x0, x3, [sp, #16*7]\n"
    "  stp x2, x18, [sp, #16*5]\n"
    "  stp x20, x19, [sp, #16*4]\n"
    "  stp x22, x21, [sp, #16*3]\n"
    "  stp x24, x23, [sp, #16*2]\n"
    "  stp x26, x25, [sp, #16*1]\n"
    "  stp x28, x27, [sp, #16*0]\n"
    "  cbz x3, 2f\n"
    "  mov x0, x3\n"
    "  bl load_fpu_registers\n"
    "  ldr x0, [sp, #16*7]\n"
    "2:\n"
    "  mov x2, sp\n"
    "  bl zx_restricted_enter\n"
    "  ldp x28, x27, [sp, #16*0]\n"
    "  ldp x26, x25, [sp, #16*1]\n"
    "  ldp x24, x23, [sp, #16*2]\n"
    "  ldp x22, x21, [sp, #16*3]\n"
    "  ldp x20, x19, [sp, #16*4]\n"
    "  ldp x2, x18, [sp, #16*5]\n"
    "  ldp x29, x30, [sp, #16*6]\n"
    "  add sp, sp, #16*8\n"
    "  ret\n"
    "\n"
    ".globl load_fpu_registers\n"
    "load_fpu_registers:\n"
    "  ldp q0, q1, [x0]\n"
    "  ldp q2, q3, [x0, #32]\n"
    "  ldp q4, q5, [x0, #64]\n"
    "  ldp q6, q7, [x0, #96]\n"
    "  ldp q8, q9, [x0, #128]\n"
    "  ldp q10, q11, [x0, #160]\n"
    "  ldp q12, q13, [x0, #192]\n"
    "  ldp q14, q15, [x0, #224]\n"
    "  ldp q16, q17, [x0, #256]\n"
    "  ldp q18, q19, [x0, #288]\n"
    "  ldp q20, q21, [x0, #320]\n"
    "  ldp q22, q23, [x0, #352]\n"
    "  ldp q24, q25, [x0, #384]\n"
    "  ldp q26, q27, [x0, #416]\n"
    "  ldp q28, q29, [x0, #448]\n"
    "  ldp q30, q31, [x0, #480]\n"
    "  ret\n"
    "\n"
    ".globl store_fpu_registers\n"
    "store_fpu_registers:\n"
    "  stp q0, q1, [x0]\n"
    "  stp q2, q3, [x0, #32]\n"
    "  stp q4, q5, [x0, #64]\n"
    "  stp q6, q7, [x0, #96]\n"
    "  stp q8, q9, [x0, #128]\n"
    "  stp q10, q11, [x0, #160]\n"
    "  stp q12, q13, [x0, #192]\n"
    "  stp q14, q15, [x0, #224]\n"
    "  stp q16, q17, [x0, #256]\n"
    "  stp q18, q19, [x0, #288]\n"
    "  stp q20, q21, [x0, #320]\n"
    "  stp q22, q23, [x0, #352]\n"
    "  stp q24, q25, [x0, #384]\n"
    "  stp q26, q27, [x0, #416]\n"
    "  stp q28, q29, [x0, #448]\n"
    "  stp q30, q31, [x0, #480]\n"
    "  ret\n");
#endif

namespace {

std::mutex g_spawn_mtx;

#if defined(__x86_64__)
struct XstateCfg {
  bool xsave = false;
  size_t size = 512;
  uint64_t xcr0 = 0;
  uint32_t mxcsr_mask = 0xFFBF;
};

const XstateCfg& XstateConfig() {
  static const XstateCfg cfg = [] {
    XstateCfg c;
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    __cpuid(1, eax, ebx, ecx, edx);
    const bool osxsave = ((ecx >> 27) & 1U) != 0;
    if (osxsave) {
      uint32_t lo = 0, hi = 0;
      __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
      c.xcr0 = (static_cast<uint64_t>(hi) << 32) | lo;
      __cpuid_count(0xD, 0, eax, ebx, ecx, edx);
      // EBX: size of the XSAVE area for the features enabled in XCR0.
      if (ebx >= 576) {
        c.xsave = true;
        c.size = ebx;
      }
    }
    alignas(64) uint8_t fx[512] = {};
    __asm__ volatile("fxsave64 %0" : "=m"(fx));
    uint32_t mask = 0;
    memcpy(&mask, fx + 28, sizeof(mask));
    c.mxcsr_mask = (mask != 0) ? mask : 0xFFBF;
    shim_use_xsave = c.xsave ? 1 : 0;
    return c;
  }();
  return cfg;
}

size_t XstateAllocSize() { return (XstateConfig().size + 63) & ~static_cast<size_t>(63); }
#elif defined(__aarch64__)
size_t XstateAllocSize() { return 512; }
#endif

// Writes 0 to *clear_child_tid and wakes one futex waiter (pthread_join).
void ClearChildTid(LinuxProcess* proc, LinuxThread* thrd) {
  if (thrd->clear_child_tid == 0) return;
  uint32_t zero = 0;
  if (CopyToUser(proc, thrd->clear_child_tid, &zero, sizeof(zero))) {
    FutexWakeAddress(proc, thrd->clear_child_tid, 1);
  }
  thrd->clear_child_tid = 0;
}

void FinalizeThreadExit(LinuxProcess* proc, LinuxThread* thrd) {
  FutexExitRobustList(proc, thrd);
  ClearChildTid(proc, thrd);

  if (thrd->state_map_addr != 0) {
    zx_vmar_unmap(zx_vmar_root_self(), thrd->state_map_addr, 4096);
    thrd->state_map_addr = 0;
    thrd->state = nullptr;
  }
  if (thrd->state_vmo != ZX_HANDLE_INVALID) {
    zx_handle_close(thrd->state_vmo);
    thrd->state_vmo = ZX_HANDLE_INVALID;
  }
  {
    std::lock_guard<std::mutex> wl(thrd->wake_mtx);
    if (thrd->wake_event != ZX_HANDLE_INVALID) {
      zx_handle_close(thrd->wake_event);
      thrd->wake_event = ZX_HANDLE_INVALID;
    }
    if (thrd->zx_thread != ZX_HANDLE_INVALID) {
      zx_handle_close(thrd->zx_thread);
      thrd->zx_thread = ZX_HANDLE_INVALID;
    }
  }
  if (thrd->xstate != nullptr) {
    XstateFree(thrd->xstate);
    thrd->xstate = nullptr;
  }
  ForgetThreadSchedState(thrd);

  bool last = false;
  {
    std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
    if (thrd->tid == proc->pid) proc->leader_exit_code = thrd->exit_code;
    for (auto it = proc->threads.begin(); it != proc->threads.end(); ++it) {
      if (it->get() == thrd) {
        proc->threads.erase(it);
        break;
      }
    }
    last = proc->threads.empty();
  }
  if (last) {
    OnLastThreadExit(proc);
  } else {
    // An execve() in another thread may be waiting for this thread to go.
    WakeProcessThreads(proc);
  }
}

// Linux FPU exception si_code from the unmasked exception flags.
int FpeCode(uint32_t err) {
  if (err & 0x001) return 7;   // FPE_FLTINV
  if (err & 0x004) return 3;   // FPE_FLTDIV
  if (err & 0x008) return 4;   // FPE_FLTOVF
  if (err & 0x012) return 5;   // FPE_FLTUND
  if (err & 0x020) return 6;   // FPE_FLTRES
  return 0;
}

// Translates a restricted-mode exception into the Linux signal the kernel
// would raise (arch/x86/kernel/traps.c, mm/fault.c).
void HandleRestrictedException(LinuxProcess* proc, LinuxThread* thrd) {
  auto* exc = reinterpret_cast<zx_restricted_exception_t*>(thrd->state);
#if defined(__x86_64__)
  const auto& arch = exc->exception.context.arch.u.x86_64;
  const uint32_t type = exc->exception.header.type;
  const uint32_t vector = static_cast<uint32_t>(arch.vector);
  const uint64_t ip = exc->state.ip;
  thrd->fault_trapno = vector;
  thrd->fault_err = arch.err_code;
  thrd->fault_cr2 = (vector == 14) ? arch.cr2 : 0;

  LinuxSiginfo info;
  switch (vector) {
    case 0:  // #DE
      info = MakeSiginfoFault(kSIGFPE, 1 /* FPE_INTDIV */, ip);
      break;
    case 1:  // #DB
      info = MakeSiginfoFault(kSIGTRAP,
                              type == ZX_EXCP_HW_BREAKPOINT ? 4 /* TRAP_HWBKPT */
                                                            : 2 /* TRAP_TRACE */,
                              ip);
      break;
    case 3:  // #BP (int3)
      info = MakeSiginfo(kSIGTRAP, kSI_KERNEL);
      break;
    case 6:  // #UD
      info = MakeSiginfoFault(kSIGILL, 2 /* ILL_ILLOPN */, ip);
      break;
    case 12:  // #SS
      info = MakeSiginfo(kSIGBUS, kSI_KERNEL);
      break;
    case 14: {  // #PF
      const auto status = static_cast<zx_status_t>(exc->exception.context.synth_code);
      if (status == ZX_ERR_OUT_OF_RANGE || status == ZX_ERR_NO_MEMORY) {
        // Access beyond the end of the backing object (e.g. a file mapping
        // past EOF), or the page could not be allocated: Linux VM_FAULT_SIGBUS.
        info = MakeSiginfoFault(kSIGBUS, 2 /* BUS_ADRERR */, arch.cr2);
      } else {
        const bool protection = (arch.err_code & 1) != 0 || status == ZX_ERR_ACCESS_DENIED;
        info = MakeSiginfoFault(kSIGSEGV, protection ? 2 /* SEGV_ACCERR */ : 1 /* SEGV_MAPERR */,
                                arch.cr2);
      }
      break;
    }
    case 16: {  // #MF (x87)
      const auto* fx = static_cast<const uint8_t*>(thrd->xstate);
      uint16_t fcw = 0, fsw = 0;
      memcpy(&fcw, fx + 0, 2);
      memcpy(&fsw, fx + 2, 2);
      const int code = FpeCode(fsw & ~fcw & 0x3f);
      if (code == 0) return;  // spurious: Linux sends nothing
      info = MakeSiginfoFault(kSIGFPE, code, ip);
      break;
    }
    case 17:  // #AC
      info = MakeSiginfoFault(kSIGBUS, 1 /* BUS_ADRALN */, 0);
      break;
    case 19: {  // #XM (SSE)
      const auto* fx = static_cast<const uint8_t*>(thrd->xstate);
      uint32_t mxcsr = 0;
      memcpy(&mxcsr, fx + 24, 4);
      const int code = FpeCode(mxcsr & ~(mxcsr >> 7) & 0x3f);
      if (code == 0) return;
      info = MakeSiginfoFault(kSIGFPE, code, ip);
      break;
    }
    default:  // #GP, #OF, #BR, policy errors, ...
      info = MakeSiginfo(kSIGSEGV, kSI_KERNEL);
      break;
  }

  ShimLogf("[restricted] PID %d (%s) TID %d: exception type=0x%x vec=%u err=0x%llx "
           "cr2=0x%llx rip=0x%llx rsp=0x%llx -> signal %d code %d\n",
           proc->pid, proc->comm.c_str(), thrd->tid, type, vector,
           static_cast<unsigned long long>(arch.err_code),
           static_cast<unsigned long long>(arch.cr2), static_cast<unsigned long long>(ip),
           static_cast<unsigned long long>(exc->state.rsp), info.si_signo, info.si_code);
#elif defined(__aarch64__)
  const auto& arch = exc->exception.context.arch.u.arm_64;
  const uint32_t type = exc->exception.header.type;
  const uint32_t esr = arch.esr;
  const uint64_t far = arch.far;
  const uint64_t pc = exc->state.pc;
  const uint32_t ec = (esr >> 26) & 0x3f;
  thrd->fault_trapno = ec;
  thrd->fault_err = esr;
  thrd->fault_cr2 = far;

  LinuxSiginfo info;
  if (ec == 0x20 || ec == 0x21 || ec == 0x24 || ec == 0x25) {  // Data/Instruction abort
    const auto status = static_cast<zx_status_t>(exc->exception.context.synth_code);
    if (status == ZX_ERR_OUT_OF_RANGE || status == ZX_ERR_NO_MEMORY) {
      info = MakeSiginfoFault(kSIGBUS, 2 /* BUS_ADRERR */, far);
    } else {
      const bool protection = status == ZX_ERR_ACCESS_DENIED;
      info = MakeSiginfoFault(kSIGSEGV, protection ? 2 /* SEGV_ACCERR */ : 1 /* SEGV_MAPERR */,
                              far);
    }
  } else if (ec == 0x30 || ec == 0x31 || ec == 0x38 || ec == 0x39) {  // Breakpoint/Watchpoint
    info = MakeSiginfo(kSIGTRAP, kSI_KERNEL);
  } else if (ec == 0x2c) {  // Trapped FP
    info = MakeSiginfoFault(kSIGFPE, 1 /* FPE_INTDIV */, pc);
  } else {
    info = MakeSiginfoFault(kSIGILL, 2 /* ILL_ILLOPN */, pc);
  }

  ShimLogf("[restricted] PID %d (%s) TID %d: exception type=0x%x esr=0x%x "
           "far=0x%llx pc=0x%llx sp=0x%llx lr=0x%llx x0=0x%llx x1=0x%llx tpidr=0x%llx -> signal %d code %d\n",
           proc->pid, proc->comm.c_str(), thrd->tid, type, esr,
           static_cast<unsigned long long>(far), static_cast<unsigned long long>(pc),
           static_cast<unsigned long long>(exc->state.sp),
           static_cast<unsigned long long>(exc->state.x[30]),
           static_cast<unsigned long long>(exc->state.x[0]),
           static_cast<unsigned long long>(exc->state.x[1]),
           static_cast<unsigned long long>(exc->state.tpidr_el0),
           info.si_signo, info.si_code);
#endif
  ForceFaultSignal(proc, thrd, info);
}

}  // namespace

#if defined(__x86_64__)
size_t XstateSize() { return XstateConfig().size; }
bool XstateUsesXsave() { return XstateConfig().xsave; }
uint64_t XstateFeatures() { return XstateConfig().xcr0; }

void XstateReset(void* p) {
  auto* b = static_cast<uint8_t*>(p);
  memset(b, 0, XstateAllocSize());
  const uint16_t fcw = 0x037F;   // x87 reset control word
  const uint32_t mxcsr = 0x1F80;  // all SSE exceptions masked
  memcpy(b + 0, &fcw, sizeof(fcw));
  memcpy(b + 24, &mxcsr, sizeof(mxcsr));
  // XSAVE header (bytes 512..575) all zero: XSTATE_BV=0 -> every component in
  // its initial configuration, MXCSR still loaded from the legacy area.
}

void* XstateAlloc() {
  void* p = aligned_alloc(64, XstateAllocSize());
  if (p != nullptr) XstateReset(p);
  return p;
}

void XstateFree(void* p) { free(p); }

void XstateLoadFromUser(void* dst, const uint8_t* src, size_t len, bool full) {
  const XstateCfg& cfg = XstateConfig();
  auto* b = static_cast<uint8_t*>(dst);
  memset(b, 0, XstateAllocSize());
  if (cfg.xsave && full && len >= cfg.size) {
    memcpy(b, src, cfg.size);
  } else {
    memcpy(b, src, std::min<size_t>(len, 512));
    if (cfg.xsave) {
      const uint64_t legacy = 0x3;  // x87 | SSE present; others reset
      memcpy(b + 512, &legacy, sizeof(legacy));
    }
  }
  uint32_t mxcsr = 0;
  memcpy(&mxcsr, b + 24, 4);
  mxcsr &= cfg.mxcsr_mask;
  memcpy(b + 24, &mxcsr, 4);
  memcpy(b + 28, &cfg.mxcsr_mask, 4);
  if (cfg.xsave) {
    uint64_t bv = 0;
    memcpy(&bv, b + 512, 8);
    bv &= cfg.xcr0;
    memcpy(b + 512, &bv, 8);
    memset(b + 520, 0, 64 - 8);  // XCOMP_BV = 0 (standard form), reserved = 0
  }
}
#elif defined(__aarch64__)
size_t XstateSize() { return 512; }
bool XstateUsesXsave() { return false; }
uint64_t XstateFeatures() { return 0; }

void XstateReset(void* p) {
  if (p) memset(p, 0, 512);
}

void* XstateAlloc() {
  void* p = aligned_alloc(64, 512);
  if (p != nullptr) XstateReset(p);
  return p;
}

void XstateFree(void* p) { free(p); }

void XstateLoadFromUser(void* dst, const uint8_t* src, size_t len, bool full) {
  (void)full;
  if (dst && src) {
    memcpy(dst, src, std::min<size_t>(len, static_cast<size_t>(512)));
  }
}
#endif

void RunLinuxThreadLoop(std::shared_ptr<LinuxProcess> proc,
                        std::shared_ptr<LinuxThread> thrd) {
  t_current_thread = thrd.get();
  if (thrd->state == nullptr) {
    zx_status_t st = zx_restricted_bind_state(0, &thrd->state_vmo, nullptr);
    if (st != ZX_OK) {
      ShimLogf("[restricted] zx_restricted_bind_state failed: %d\n", st);
      return;
    }
    uintptr_t map_addr = 0;
    st = zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                     thrd->state_vmo, 0, 4096, &map_addr);
    if (st != ZX_OK) {
      ShimLogf("[restricted] zx_vmar_map state_vmo failed: %d\n", st);
      return;
    }
    thrd->state_map_addr = map_addr;
    thrd->state = reinterpret_cast<zx_restricted_state_t*>(map_addr);
    *thrd->state = thrd->saved_sig_state;
    memset(&thrd->saved_sig_state, 0, sizeof(thrd->saved_sig_state));
  }

  {
    std::lock_guard<std::mutex> wl(thrd->wake_mtx);
    if (thrd->zx_thread == ZX_HANDLE_INVALID) {
      zx_handle_duplicate(zx_thread_self(), ZX_RIGHT_SAME_RIGHTS, &thrd->zx_thread);
    }
    if (thrd->wake_event == ZX_HANDLE_INVALID) {
      zx_event_create(0, &thrd->wake_event);
    }
  }
  if (thrd->xstate == nullptr) thrd->xstate = XstateAlloc();
  if (thrd->xstate == nullptr || thrd->wake_event == ZX_HANDLE_INVALID) {
    // Out of resources before the thread ever ran: Linux would fail the
    // clone; here the thread already exists, so the process is killed.
    ShimLogf("[restricted] PID %d TID %d: cannot allocate thread state\n", proc->pid, thrd->tid);
    GroupExit(proc.get(), kSIGKILL);
  }

  // CLONE_CHILD_SETTID: Linux writes the TID in the child's address space,
  // which is only reachable from a thread of the child.
  if (thrd->set_child_tid != 0) {
    int32_t tid = thrd->tid;
    CopyToUser(proc.get(), thrd->set_child_tid, &tid, sizeof(tid));
    thrd->set_child_tid = 0;
  }

  ApplyThreadSchedProfile(proc.get(), thrd.get());
  HandleSignalsOnReturn(proc.get(), thrd.get(), false, 0, 0);

  while (!proc->exited.load() && !thrd->kill_requested.load()) {
    uint64_t reason = ~0ULL;
    zx_status_t st = restricted_enter_wrapper(
        0, reinterpret_cast<uintptr_t>(&restricted_exit), &reason, thrd->xstate);
    if (st != ZX_OK) {
      // Zircon rejected the register state (e.g. a non-canonical rip restored
      // by rt_sigreturn). Linux faults on the return to user mode: SIGSEGV.
#if defined(__x86_64__)
      ShimLogf("[restricted] PID %d TID %d enter failed: %d (rip=0x%llx) -> SIGSEGV\n",
               proc->pid, thrd->tid, st, static_cast<unsigned long long>(thrd->state->ip));
#elif defined(__aarch64__)
      ShimLogf("[restricted] PID %d TID %d enter failed: %d (pc=0x%llx) -> SIGSEGV\n",
               proc->pid, thrd->tid, st, static_cast<unsigned long long>(thrd->state->pc));
#endif
      thrd->fault_trapno = 13;
      thrd->fault_err = 0;
      thrd->fault_cr2 = 0;
      ForceFaultSignal(proc.get(), thrd.get(), MakeSiginfo(kSIGSEGV, kSI_KERNEL));
      HandleSignalsOnReturn(proc.get(), thrd.get(), false, 0, 0);
      continue;
    }

    if (reason == ZX_RESTRICTED_REASON_SYSCALL) {
#if defined(__x86_64__)
      const uint64_t sc_num = thrd->state->rax;
      const int64_t ret = DispatchLinuxSyscall(proc.get(), thrd.get());
      if (proc->exited.load() || thrd->kill_requested.load()) break;
      if (sc_num == 15) {
        // rt_sigreturn restored the complete register state from the frame.
        HandleSignalsOnReturn(proc.get(), thrd.get(), false, 0, 0);
      } else {
        thrd->state->rax = static_cast<uint64_t>(ret);
        HandleSignalsOnReturn(proc.get(), thrd.get(), true, ret, sc_num);
      }
#elif defined(__aarch64__)
      const uint64_t sc_num = thrd->state->x[8];
      thrd->orig_x0 = thrd->state->x[0];
      const int64_t ret = DispatchLinuxSyscall(proc.get(), thrd.get());
      if (proc->exited.load() || thrd->kill_requested.load()) break;
      if (sc_num == 139) {
        HandleSignalsOnReturn(proc.get(), thrd.get(), false, 0, 0);
      } else {
        thrd->state->x[0] = static_cast<uint64_t>(ret);
        HandleSignalsOnReturn(proc.get(), thrd.get(), true, ret, sc_num);
      }
#endif
    } else if (reason == ZX_RESTRICTED_REASON_EXCEPTION) {
      HandleRestrictedException(proc.get(), thrd.get());
      HandleSignalsOnReturn(proc.get(), thrd.get(), false, 0, 0);
    } else {
      // ZX_RESTRICTED_REASON_KICK: a signal, stop or exit request was posted.
      HandleSignalsOnReturn(proc.get(), thrd.get(), false, 0, 0);
    }
  }

  FinalizeThreadExit(proc.get(), thrd.get());
}

void SpawnLinuxThreadInProcess(std::shared_ptr<LinuxProcess> proc,
                               std::shared_ptr<LinuxThread> thrd) {
  {
    std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
    proc->threads.push_back(thrd);
  }
  std::lock_guard<std::mutex> lock(g_spawn_mtx);
  zx_handle_t prev = thrd_set_zx_process(proc->zx_proc);
  std::thread worker([proc, thrd]() { RunLinuxThreadLoop(proc, thrd); });
  thrd_set_zx_process(prev);
  worker.detach();
}
