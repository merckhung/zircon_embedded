# `zircon_embedded` — Standalone Fuchsia Zircon Microkernel + Linux Shim + Xfce4 (`x86_64` & `ARM64`)

> All six minimal, desktop and Chromium QEMU variants have passed on x64 and ARM64.
> Both Chromium bundles were rebuilt from the pinned patched source. Final publication
> checks are tracked in [docs/REBUILD.md](docs/REBUILD.md).

`zircon_embedded` is a standalone operating system distribution built with **Bazel** and **C++20** (`*.cc` / `*.h`) that compiles the **64-bit Fuchsia Zircon Microkernel**, its **vDSO (`libzircon.so`)**, **`userboot`**, **Fuchsia `libc.so`**, **sysroot**, and host **`zbi`** tool from in-tree sources for both **`x86_64`** and **`ARM64` (`aarch64`)**, and boots directly into a custom PID 1 Linux ABI syscall shim (`zircon_linux_init`). It provides three bootable rootfs targets across both architectures:
1. **`//minimal_rootfs`** — Minimal **BusyBox** command-line rootfs and syscall test suites (`mm_test`, `proc_test`, `zx_test`).
2. **`//rootfs`** — **Xfce4 Graphical Desktop Environment** with an interactive **Xfce4 Terminal**.
3. **`//chromium_rootfs`** — **Xfce4 Graphical Desktop Environment + Prebuilt 64-bit Chromium Browser** loading **`https://www.yahoo.com/`** over a live Realtek RTL8139 PCI NIC + TCP/IPv4/TLS stack.

---

## Key Features

- **100% From-Source Zircon Microkernel & Userspace Bootstrap Builds (`//zircon:kernel_build`, `//zircon:kernel_build_arm64`)**:
  - **`x86_64`**: Replays the 3,793-edge upstream Zircon/Fuchsia build graph (`zircon/kernel_build_data/graph.json.gz`) via `bazel/kernel_build/replay.py`, producing `linux-x86-boot-shim.bin`, `physload`, `physboot`, `vmzircon`, code patches, `libzircon.so` vDSO, `userboot`, Fuchsia `libc.so`, the C/C++ sysroot, and host `zbi`.
  - **`ARM64` (`aarch64`)**: Replays the 3,512-edge upstream Zircon/Fuchsia build graph (`zircon/kernel_build_data_arm64/graph.json.gz`), producing `linux-arm64-boot-shim.bin`, `physboot`, `vmzircon`, code patches, `libzircon.so` vDSO, `userboot`, Fuchsia `libc.so`, the C/C++ sysroot, and host `zbi`.
  - Strict depfile provenance verification guarantees that zero files are loaded or referenced from external Fuchsia checkouts.
- **Pure Standalone Zircon Microkernel Boot**:
  - `x86_64`: Boots `linux-x86-boot-shim.bin` -> `physload` -> `physboot` -> `vmzircon` -> `userboot` -> `bin/zircon_linux_init`.
  - `ARM64`: Boots `linux-arm64-boot-shim.bin` -> `physboot` -> `vmzircon` -> `userboot` -> `bin/zircon_linux_init`.
  - Boots cleanly without any Fuchsia component framework (`component_manager`, FIDL, Starnix, or Scenic).
- **Hardware-Assisted Linux Syscall & Exception Emulation (`zx_restricted_enter`)**:
  - Uses Zircon's `ZX_PROCESS_SHARED` (`zx_process_create_shared`) and `zx_restricted_enter` to execute unmodified 64-bit `musl`-linked and `glibc`-linked Linux ELF binaries at native CPU speed.
  - Complete vector/FPU register preservation across context switches (`XSAVE`/`FXSAVE` on x86_64, NEON/VFP `load_fpu_registers`/`store_fpu_registers` on ARM64).
  - Translates Linux syscalls across both ABIs (`x86_64` table and `aarch64` table via `src/shim/arm64_syscall_map.h`).
  - Hardware exceptions (`#PF`, `#UD`, `#DE`, `#BP`, `#GP` on x86_64; translation faults, undefined instructions, data aborts, alignment faults on ARM64) translate into Linux signals (`SIGSEGV`, `SIGILL`, `SIGFPE`, `SIGTRAP`, `SIGBUS`) delivered via real `rt_sigframe` (`ucontext_t` + `sigcontext` + `fpstate`) frames on the user stack (or `sigaltstack`).
- **Real Virtual Memory, Copy-on-Write Fork, & Shared Memory**:
  - Per-process VMAR address-space allocator (`src/shim/mem_space.cc`) enforcing hardware page permissions (`PROT_READ`, `PROT_WRITE`, `PROT_EXEC`, `PROT_NONE`, `W^X`) via `zx_vmar_protect` and `ZX_RIGHT_EXECUTE`.
  - Copy-on-write `fork()` via `ZX_VMO_CHILD_SNAPSHOT`, `MAP_SHARED` file/anonymous VMO coherence, `mremap`, `madvise` (`MADV_DONTNEED`/`MADV_FREE`), `mincore`, anonymous `memfd_create`, and SysV shared memory (`shmget`/`shmat`/`shmdt`/`shmctl`).
- **Userspace Hardware Drivers via Zircon Resource Handles**:
  - Harvests `PA_MMIO_RESOURCE`, `PA_IOPORT_RESOURCE` (x86_64), `PA_SMC_RESOURCE` (ARM64), and `PA_SYSTEM_RESOURCE` at boot to drive:
  - **PCI Bochs VBE Framebuffer (`1234:1111`)**: Strictly matched on PCI bus, programmed via VBE DISPI I/O ports or MMIO to `1024x768x32` linear framebuffer mapped from PCI BAR0 and exposed as `/dev/fb0`.
  - **PCI Realtek RTL8139 Ethernet NIC (`10EC:8139`) & IPv4/UDP/TCP Stack (`src/shim/net_inet.cc`)**: Uses Zircon's Stub IOMMU + Bus Transaction Initiator (`zx_iommu_create`, `zx_bti_create`, `zx_vmo_create_contiguous`, `zx_bti_pin`) to allocate physically contiguous DMA ring buffers and drives the RTL8139 NIC with an in-shim Ethernet, ARP, IPv4, ICMP, UDP, TCP, `AF_INET`, and `AF_NETLINK` (`NETLINK_ROUTE`) stack (`eth0` at `10.0.2.15/24`, gateway `10.0.2.2`, DNS `10.0.2.3`).
  - **PS/2 Keyboard & Mouse (`x86_64`)**: Exposed as Linux `evdev` `/dev/input/event0`. The current ARM64 configuration uses scripted X11 `XTEST` input; hardware keyboard input is verified on x86_64.
  - **Hardware RTC**: Seeds Zircon's UTC clock and the shim's wall-clock time (`CLOCK_REALTIME`, `gettimeofday`, filesystem and SysV shm timestamps).
  - **Serial UART + Zircon Debuglog**: Exposed as `/dev/console` and `/dev/ttyS0`.
- **Linux Desktop IPC, PTY Line Discipline, & Dynamic `/proc`**:
  - Full `AF_UNIX` (`SOCK_STREAM`, `SOCK_DGRAM`, `SOCK_SEQPACKET`, separate abstract `@` namespace, per-message `SCM_RIGHTS` and `SCM_CREDENTIALS`, `SO_PEERCRED`).
  - Per-key `futex` waiter queues (`WAIT`, `WAKE`, `WAIT_BITSET`, `WAKE_BITSET`, `REQUEUE`, `CMP_REQUEUE`, `WAKE_OP`), `epoll` (`EPOLLET`, `EPOLLONESHOT`), `poll`/`ppoll`/`select`/`pselect6`, `eventfd`, `timerfd`, `signalfd`, `inotify`, POSIX advisory record locks (`F_GETLK`/`F_SETLK`/`F_SETLKW`) + `flock`.
  - Unix98 pseudo-terminals (`/dev/ptmx`, `/dev/pts/<N>`, controlling TTYs `/dev/tty`, `ISIG`/`ICANON`/`VERASE`/`VKILL`/`VEOF`, `TIOCPKT`, `TIOCSPTLCK`, `TIOCSWINSZ` -> `SIGWINCH`).
  - Dynamic `/proc` backed by real Zircon introspection: `ZX_INFO_CPU_STATS`, `ZX_INFO_KMEM_STATS`, `ZX_INFO_PROCESS_MAPS`, and `ZX_INFO_TASK_RUNTIME`.
- **Runtime Verification Checks**:
  - Computed verification markers (e.g. arithmetic `$((6*7))` -> `42`, `$((7*9))` -> `63`) ensure test assertions cannot be satisfied by input echo.
  - Strict X11 window tree querying via `/bin/x11_probe` verifies real window mapping, EWMH window manager ownership, and process IDs.
  - The Chromium probe reads the actual document through loopback DevTools, requiring a complete secure Yahoo HTTPS document with visible text. The host supplies a fresh nonce and arithmetic challenge and checks the renderer's answer, so a stale marker or an error-page title cannot satisfy the check.
  - Realtek RTL8139 PCI DMA packet and byte accounting via `/proc/net/dev` checks that network traffic occurred; counters alone do not prove page rendering.

---

## Quick Start (Bazel Build & Verification)

Configure toolchain paths in `user.bazelrc` (see [`user.bazelrc.example`](user.bazelrc.example) for defaults).

### 1. Compile Zircon Microkernel, vDSO, Userboot, Libc, Sysroot & ZBI Tool From Source

#### x86_64 (3,793 build edges):
```bash
bazel build //zircon:kernel_build
```

#### ARM64 (3,512 build edges):
```bash
bazel build //zircon:kernel_build_arm64
```

---

### 2. Deliverable 1: Minimal BusyBox Rootfs + Syscall Test Suites

Boots Zircon + minimal BusyBox rootfs in QEMU, runs `/bin/zx_test`, `/bin/mm_test` (49 memory checks), `/bin/proc_test` (40 process/signal/futex/timer/exception checks), and verifies interactive BusyBox shell execution:

#### x86_64:
```bash
bazel run //minimal_rootfs:verify
```

#### ARM64:
```bash
bazel run //minimal_rootfs:verify_arm64
```

---

### 3. Deliverable 2: Xfce4 Graphical Desktop + Terminal Rootfs

Boots Zircon + Xfce4 rootfs in QEMU, starts `dbus-daemon`, `xfconfd`, `Xvfb :0`, `x11_fb_bridge`, `xfwm4`, `xfdesktop`, `xfce4-panel`, and `xfce4-terminal`, verifies live EWMH window manager and mapped windows via `/bin/x11_probe`, types commands through X11 `XTEST` on both architectures and hardware QEMU QMP `send-key` on x86_64 (keyboard -> `/dev/input/event0` -> `x11_fb_bridge` -> `XTEST` -> `xfce4-terminal` -> `/dev/pts/0` -> `/bin/sh`), and captures screenshots via QMP:

#### x86_64:
```bash
bazel run //rootfs:verify
```

#### ARM64:
```bash
bazel run //rootfs:verify_arm64
```

To run interactively with a GTK display window on your local screen:
```bash
bazel run //rootfs:run_qemu          # x86_64
bazel run //rootfs:run_qemu_arm64    # ARM64
```

---

### 4. Deliverable 3: Xfce4 Graphical Desktop + Prebuilt Chromium (`www.yahoo.com`)

Packages the prebuilt 64-bit Linux Chromium browser into the Xfce4 rootfs, boots on QEMU with a Realtek RTL8139 PCI NIC (`-netdev user,id=net0 -device rtl8139,netdev=net0`), launches Chromium pointing to `https://www.yahoo.com/`, verifies the actual secure document and a fresh renderer challenge via `/bin/chromium_page_probe`, checks window state and NIC counters, and captures a screenshot:

#### x86_64:
```bash
bazel run //chromium_rootfs:verify
```

#### ARM64:
```bash
bazel run //chromium_rootfs:verify_arm64
```

To launch interactively on your screen with GTK display:
```bash
bazel run //chromium_rootfs:run_qemu          # x86_64
bazel run //chromium_rootfs:run_qemu_arm64    # ARM64
```

---

## Repository Layout

- `bazel/kernel_build/`: Ninja graph exporter (`export_fuchsia_kernel_graph.py`), command sanitizer (`cmdclean.py`), BuildID stamper (`buildid_stamp.py`), and parallel build executor (`replay.py`) with depfile provenance checking.
- `bazel/alpine_repo.bzl`: Fetches official Alpine packages (`x86_64` and `aarch64`) with SHA-256 verification.
- `zircon/kernel/`, `zircon/system/`, `sdk/`, `src/lib/`, `third_party/`, `build/`: In-tree Zircon microkernel, vDSO, userboot, libc, sysroot, and host tool sources.
- `zircon/kernel_build_data/`: x86_64 build graph (`graph.json.gz`, 3,793 edges) and non-compiled GN/FIDL/syscall text outputs.
- `zircon/kernel_build_data_arm64/`: ARM64 build graph (`graph.json.gz`, 3,512 edges) and non-compiled GN/FIDL/syscall text outputs.
- `src/shim/`: C++20 Linux ABI syscall shim and hardware driver executive (`zircon_linux_init`), including:
  - `arm64_syscall_map.h`: ARM64 Linux syscall number translation table.
  - `net_inet.cc`: RTL8139 PCI DMA + IPv4/UDP/TCP/Netlink networking stack.
  - `restricted_exec.cc`: Hardware-assisted `zx_restricted_enter` loop with x86_64 and ARM64 vector state preservation.
- `userland/`: Diagnostic test suites (`zx_test`, `tests/mm_test`, `tests/proc_test`), X11 window tree inspector (`x11_probe`, `x11_client`), and X11-to-Bochs-VBE framebuffer/evdev bridge (`x11_fb_bridge`).
- `minimal_rootfs/`: Alpine BusyBox rootfs packaging and QEMU verification runners (`x86_64` and `arm64`).
- `rootfs/`: Full Alpine Xfce4 desktop rootfs packaging and QEMU verification runners (`x86_64` and `arm64`).
- `chromium_rootfs/`: 3rd rootfs packaging Xfce4 + prebuilt 64-bit Chromium browser, `glibc` runtime libraries, CA certificates, and QEMU verification/interactive runners (`x86_64` and `arm64`).
- `chromium/prebuilt/`: Rebuilt Linux `x86_64` Chromium binaries (`chrome`, `chrome_crashpad_handler`), SwiftShader/Vulkan shared libraries, and assets. ANGLE is linked statically into Chrome.
- `chromium/prebuilt_arm64/`: Linux `arm64` Chromium bundle rebuilt from the pinned patched source; QEMU verification passes.
- `docs/screenshots/`: Fresh verified desktop/terminal QMP captures for both architectures and a fresh verified x64 Chromium capture; ARM64 Chromium capture awaits its rebuilt browser.

---

## Screenshots

All desktop, terminal and Chromium images below are fresh, inspected QMP captures from the rebuilt variants:

### ARM64 (`aarch64`)
- **Xfce4 Desktop Home Screen**: [`docs/screenshots/xfce4_desktop_arm64.png`](docs/screenshots/xfce4_desktop_arm64.png)
- **Xfce4 Terminal Session**: [`docs/screenshots/xfce4_terminal_arm64.png`](docs/screenshots/xfce4_terminal_arm64.png)
- **Chromium Browser Rendering `www.yahoo.com`**: [`docs/screenshots/xfce4_chromium_yahoo_arm64.png`](docs/screenshots/xfce4_chromium_yahoo_arm64.png)

### x86_64
- **Xfce4 Desktop Home Screen**: [`docs/screenshots/xfce4_desktop.png`](docs/screenshots/xfce4_desktop.png)
- **Xfce4 Terminal Session**: [`docs/screenshots/xfce4_terminal.png`](docs/screenshots/xfce4_terminal.png)
- **Chromium Browser Rendering `www.yahoo.com`**: [`docs/screenshots/xfce4_chromium_yahoo.png`](docs/screenshots/xfce4_chromium_yahoo.png)
