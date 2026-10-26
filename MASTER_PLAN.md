# `zircon_embedded` — Master Implementation Plan

## 1. Executive Summary
`zircon_embedded` compiles the standalone 64-bit Fuchsia **Zircon Microkernel** (`x86_64` & `ARM64`), **vDSO (`libzircon.so`)**, **`userboot`**, **Fuchsia `libc.so`**, **sysroot**, and host **`zbi`** tool from in-tree sources under Bazel, and boots directly into a self-contained C++20 PID 1 Linux ABI personality and hardware device executive (`zircon_linux_init`) without Fuchsia's userspace component framework (`component_manager`, `starnix`, FIDL, Netstack, Scenic).

Using Zircon's hardware-assisted `zx_restricted_enter` (`SYSCALL`/`SVC` and architectural exception trap vector) and `ZX_PROCESS_SHARED` (`zx_process_create_shared`) split address spaces, unmodified 64-bit Linux ELF binaries (dynamically linked with `musl` or `glibc`) execute at native CPU speed on top of the Zircon microkernel across both `x86_64` and `ARM64`:

1. **From-Source Zircon Kernel & Bootstrap Builds (`//zircon:kernel_build`, `//zircon:kernel_build_arm64`)**:
   - `x86_64`: Executes all 3,793 compilation, Rust crate, archive, link, strip, and ZBI packaging steps from in-tree sources via `bazel/kernel_build/replay.py`.
   - `ARM64`: Executes all 3,512 compilation, Rust crate, archive, link, strip, and ZBI packaging steps from in-tree sources via `bazel/kernel_build/replay.py`.
   - Both builds verify via compiler depfiles that zero files are read from any external checkout.
2. **Minimal Rootfs (`//minimal_rootfs:verify`, `//minimal_rootfs:verify_arm64`)**:
   Boots the standalone 64-bit Zircon microkernel + ZBI ramdisk, passes `/bin/zx_test`, `/bin/mm_test` (49 memory-management checks), `/bin/proc_test` (41 process/signal/futex/timer/exception checks on x86_64; 40 on ARM64), and verifies interactive 64-bit BusyBox (`ash`) shell commands.
3. **Xfce4 Graphical Desktop Rootfs (`//rootfs:verify`, `//rootfs:verify_arm64`)**:
   Boots the standalone 64-bit Zircon microkernel + ZBI ramdisk into the full 64-bit **Xfce4 Graphical Desktop** (`dbus-daemon`, `xfconfd`, `Xvfb :0`, `x11_fb_bridge` -> Bochs VBE hardware LFB `/dev/fb0`, `xfwm4`, `xfdesktop`, `xfce4-panel`, and `xfce4-terminal`), verifies mapped X11 windows and EWMH window manager ownership with `/bin/x11_probe`, exercises scripted X11 `XTEST` keystrokes on both architectures and hardware keyboard events on x86_64 (`QMP send-key` -> `/dev/input/event0` -> `x11_fb_bridge` -> `XTEST` -> `xfce4-terminal` -> `/dev/pts/0` -> `/bin/sh`), and captures verified screenshots via QMP.
4. **Xfce4 Graphical Desktop + Chromium Rootfs (`//chromium_rootfs:verify`, `//chromium_rootfs:verify_arm64`)**:
   Boots into Xfce4 desktop and launches the prebuilt 64-bit Chromium browser pointing to `https://www.yahoo.com/`. Drives a Realtek RTL8139 PCI NIC over DMA ring buffers via Zircon BTI/IOMMU (`zx_iommu_create`, `zx_bti_create`, `zx_bti_pin`), resolves DNS, performs TLS handshake, renders Yahoo front page with Blink and V8, verifies window mapping via `/bin/x11_probe`, the actual secure document and a fresh host renderer challenge via `/bin/chromium_page_probe`, and network packet reception via `/proc/net/dev`, and captures verified screenshots via QMP.

---

## 2. System Architecture

```
+-------------------------------------------------------------------------------+
|               64-bit Linux Userland (Alpine x86_64 / aarch64 ELF)             |
|  Minimal Rootfs: BusyBox (/bin/sh, uname, ps, ls) + zx_test, mm_test,         |
|                  proc_test                                                    |
|  Xfce4 Rootfs:   dbus-daemon, xfconfd, Xvfb :0, x11_fb_bridge, x11_probe,     |
|                  xfwm4, xfdesktop, xfce4-panel, xfce4-terminal (/dev/pts/0)   |
|  Chromium Rootfs:Xfce4 stack + 64-bit Chromium Browser (www.yahoo.com)        |
+-------------------------------------------------------------------------------+
                        |                              ^
     Linux Syscall Trap | SYSCALL / SVC / Exception    | zx_restricted_enter()
     (x86_64 & aarch64) v                              | (Vector / FPU state)
+-------------------------------------------------------------------------------+
|         PID 1 Zircon Linux Shim Executive (//src/shim:zircon_linux_init)      |
|  * Process, Thread & Signal Manager:                                          |
|    zx_process_create_shared (ZX_PROCESS_SHARED), fork/vfork/clone/execve/     |
|    wait4/waitid, hardware exception -> SIGSEGV/SIGILL/SIGFPE/SIGTRAP/SIGBUS,  |
|    real rt_sigframe delivery + rt_sigreturn + sigaltstack + SA_RESTART,       |
|    per-key futex waiter queues, ITIMER_REAL/alarm/timer_create, RTC UTC time  |
|  * Memory & ELF64 Loader (mem_space.cc, syscalls_mem.cc, elf_loader.cc):      |
|    Per-process VMAR range allocator, real page permissions (zx_vmar_protect + |
|    ZX_RIGHT_EXECUTE), COW fork (ZX_VMO_CHILD_SNAPSHOT), MAP_SHARED coherence, |
|    mremap, madvise, mincore, memfd_create, SysV shm, PT_LOAD + PT_INTERP      |
|  * VFS, TTY & IPC (vfs.cc, syscalls_fs.cc, fs_syscalls.cc, fs_poll.cc,        |
|    tty.cc, syscalls_ipc.cc):                                                  |
|    In-memory VFS with hard links, permissions, timestamps, advisory locks     |
|    (fcntl F_SETLK/flock), inotify, pipes, AF_UNIX (STREAM/DGRAM/SEQPACKET +   |
|    SCM_RIGHTS/SCM_CREDENTIALS), PTY line discipline (ISIG/ICANON/TIOCPKT/     |
|    TIOCSCTTY/SIGWINCH), epoll (EPOLLET/EPOLLONESHOT), poll, select, eventfd,  |
|    timerfd, signalfd, dynamic /proc & /proc/zircon/{info,processes,vmos}      |
|  * Networking Stack (net_inet.cc, net_inet.h):                                |
|    Realtek RTL8139 PCI DMA driver (10EC:8139) via zx_bti_pin + zx_iommu,      |
|    in-shim Ethernet, ARP, IPv4, ICMP, UDP, TCP, AF_INET, and AF_NETLINK       |
|  * Hardware Drivers (hardware.cc, hardware_fb_input.cc, time.cc):             |
|    - 16550 / PL011 UART + zx_debug_write/read -> /dev/console, /dev/ttyS0     |
|    - PCI Bochs VBE Display (1234:1111, BAR0) -> /dev/fb0 (1024x768x32)        |
|    - x86_64 PS/2 Keyboard & Mouse -> /dev/input/event0                |
|    - Hardware RTC (CMOS 0x70/0x71 or PL031) -> UTC clock & wall-clock time    |
+-------------------------------------------------------------------------------+
                        |                              ^
                        | Zircon vDSO Syscalls         | Handles & Resources
                        v                              |
+-------------------------------------------------------------------------------+
|         Standalone Fuchsia Zircon Microkernel (x86_64 & ARM64)                |
|  x86_64: linux-x86-boot-shim.bin -> physload -> physboot -> vmzircon -> userboot|
|  ARM64:  linux-arm64-boot-shim.bin -> physboot -> vmzircon -> userboot               |
+-------------------------------------------------------------------------------+
```

---

## 3. Verification Matrix

Current source build and runtime results are recorded in
`docs/rebuild-evidence.json`. x64 Chromium passes the secure HTTP 200 document
and fresh renderer challenge checks. ARM64 Chromium also passes these checks, with an inspected fresh QMP capture.

| Target | Architecture | Result | Key Proof Evidence |
|---|---|---|---|
| `//zircon:kernel_build` | `x86_64` | **Built** | 3,793 graph edges; 3,551 depfiles checked |
| `//zircon:kernel_build_arm64` | `arm64` | **Built** | 3,512 graph edges; 3,310 depfiles checked |
| `//minimal_rootfs:verify` | `x86_64` | **PASS** | 49/49 memory checks, 41/41 process checks, `zx_test`, computed shell markers |
| `//minimal_rootfs:verify_arm64` | `arm64` | **PASS** | 49/49 memory checks, 40/40 process checks, `zx_test`, computed shell markers |
| `//rootfs:verify` | `x86_64` | **PASS** | Live X11 windows/WM, PTY result 42, QMP PS/2 result 63, fresh screenshots |
| `//rootfs:verify_arm64` | `arm64` | **PASS** | Live X11 windows/WM, XTEST-to-PTY result 42, fresh screenshots |
| `//chromium_rootfs:verify` | `x86_64` | **PASS** | Fresh browser, secure Yahoo HTTP 200 document, fresh renderer nonce/product, live RTL8139 traffic and inspected QMP screenshot |
| `//chromium_rootfs:verify_arm64` | `arm64` | **PASS** | Fresh browser, secure Yahoo HTTP 200 document, fresh renderer nonce/product, live RTL8139 traffic and inspected QMP screenshot |
