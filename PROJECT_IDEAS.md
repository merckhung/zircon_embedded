# Project Ideas Catalog (`~/Code`)

This document collects project ideas across two categories:
1. **Part I — Distinct Non-Kernel / Non-Embedded Systems, GPU & Compiler Projects** (Vulkan tooling, SIMD Shader JITs, Dynamic Binary Translation, TBDR GPU Architecture Simulation, and Multi-Format Binary Analysis).
2. **Part II — Unconventional Embedded OS / Kernel Projects (`*_embedded`)** (building on the pattern of `darwin_embedded.github` and `nt51_xfce4_embedded`).

---

## Part I: Distinct Projects Beyond Embedded Systems & Kernels

### 1. Custom Vulkan Layer + Live Frame Scrubber ("Mini-RenderDoc")
* **Domain**: Graphics / GPU Tooling (`C++20` + Vulkan 1.3 + Web/ImGui Frontend)
* **Concept**: A standalone intercepted Vulkan layer (`VK_LAYER_frame_xray`) that hooks `vkCreateShaderModule`, `vkCmdBindPipeline`, `vkCmdDraw*`, `vkCmdDispatch`, and `vkQueueSubmit` on any unmodified Vulkan application (or ANGLE/Dawn workload).
* **Key Capabilities**:
  - Automatically injects GPU timestamp/occlusion queries and captures intermediate color/depth attachments between draw calls.
  - Parses and decompiles bound SPIR-V shaders on the fly.
  - Supports **live shader hot-editing and pipeline state overrides** (wireframe overlay, overdraw heatmap, NaN/Inf pixel detector, mipmap level visualizer) and re-submits the captured command buffer in real time.

---

### 2. SPIR-V to `x86_64` AVX2 / AVX-512 SIMD Shader JIT Compiler
* **Domain**: Compilers / GPU Compute & Rendering (`C++` + Direct `x86_64` Codegen)
* **Concept**: A clean, self-contained JIT compiler (similar to the core of SwiftShader/llvmpipe, in ~3,000–5,000 lines of zero-dependency C++) that takes a compiled Vulkan **SPIR-V** compute or fragment shader and JIT-compiles it directly into **8-wide AVX2 or 16-wide AVX-512 `x86_64` machine code**.
* **Key Capabilities**:
  - Executes 8 or 16 shader invocations (`gl_GlobalInvocationID` or $2\times 2$ pixel quads) simultaneously in SIMD lanes using per-lane execution masks (`%ymm` / `%zmm` / `%k1`–`%k7`) for divergent `if`/`else`/`loop` control flow.
  - Includes an interactive side-by-side inspector showing the input SPIR-V SSA control-flow graph (CFG), register liveness intervals, and the emitted `x86_64` vector assembly.

---

### 3. `riscv64` (or `arm64`) $\rightarrow$ `x86_64` Dynamic Binary Translator (DBT) with Trace Inspector
* **Domain**: Virtualization / Binary Translation (in the spirit of Berberis, Box64, and Rosetta 2)
* **Concept**: A pure Linux userspace dynamic binary translator that loads an unmodified `riscv64` (or `aarch64`) Linux ELF executable into an `x86_64` process, lifts basic blocks on demand into a compact SSA micro-IR, performs linear-scan register allocation, and emits native `x86_64` machine code into a chained basic-block cache.
* **Key Capabilities**:
  - Forwards guest Linux syscalls directly to the host kernel with near-zero overhead.
  - Thunks guest shared library calls (`libc`, `libm`, `libpthread`, `libEGL`, `libvulkan`) directly to native host `x86_64` `.so` libraries.
  - Features a live profiling UI displaying translated basic blocks, block-chaining hit rates, guest-to-host register allocation maps, and hot loop traces.

---

### 4. Tile-Based Deferred Renderer (TBDR) GPU Architecture Simulator
* **Domain**: Hardware Architecture Simulation / Graphics (`C++` + Interactive Visualizer)
* **Concept**: A cycle-approximate software simulator of a mobile **Tile-Based GPU** (inspired by Arm Mali and Imagination PowerVR) executing real vertex and fragment shaders.
* **Key Capabilities**:
  - Simulates the hardware two-phase pipeline:
    1. **Geometry & Tile Binning Phase**: Transforms vertices, clips triangles, and bins primitive pointers into $16\times 16$ or $32\times 32$ screen-space tile lists in simulated SRAM.
    2. **Fragment & On-Chip Tile Memory Phase**: Schedules warps/quads per tile, performs Hidden Surface Removal (HSR / Early-Z) and blending entirely inside on-chip tile SRAM, and bursts resolved tiles out to simulated DRAM.
  - Reports hardware performance counters (SRAM vs. DRAM bandwidth saved by tiling, warp divergence, quad overdraw, register pressure) and provides an animated step-by-step replay of shader cores processing each tile.

---

### 5. Multi-Format Binary X-Ray & Semantic Diff Engine (`ELF` / `Mach-O` / `PE+` / `DEX/OAT`)
* **Domain**: Reverse Engineering / Systems Tooling (`Rust` or `C++` + Web UI)
* **Concept**: A unified binary inspector and structural diffing engine that parses **Linux `ELF64`**, **macOS `Mach-O`**, **Windows `PE32+`**, and **Android `APEX`/`OAT`/`DEX`** binaries from scratch.
* **Key Capabilities**:
  - Maps every byte of a binary to its symbol, relocation, unwind info (`eh_frame` / `__unwind_info` / `.pdata`), C++ vtable/RTTI, and string reference.
  - Compares two builds—or the *same* C++ library compiled for Linux ELF vs. Darwin Mach-O vs. Windows PE—at the control-flow-graph (CFG) and symbol level to highlight code-size regressions and cross-ABI codegen differences.

---

## Part II: Unconventional Embedded OS / Kernel Projects (`*_embedded`)

1. **`haiku_gui_linux_embedded`**
   - **Stack**: 64-bit Linux `x86_64` kernel (`bzImage`) + pure-userspace MIT `libroot.so` (Haiku ports, areas, semaphores, threads over `/dev/shm` & `futex`) + upstream Haiku `app_server` (AGG + FreeType + `/dev/fb0` & `/dev/input/event*`) + `libbe.so` + Haiku `Deskbar` & `Terminal`.
2. **`zircon_embedded` — Standalone Fuchsia Zircon Microkernel + Linux Shim + Xfce4**
   - **Stack**: Zircon microkernel (`x86_64` / `arm64`) + ZBI ramdisk + lightweight PID 1 Linux syscall shim (`zx_restricted_enter` / VDSO trampoline) + BusyBox (`zx_object_get_info` for real `ps`/`zxtest`) + Xfce4 + Chromium/SwiftShader.
3. **`nt52_amd64_embedded` — 64-bit Windows Server 2003 SP1 (`WRK-v1.2`) Kernel + 64-bit Linux Syscall Shim**
   - **Stack**: 64-bit `ntoskrnl.exe` (`amd64` from `WRK-v1.2` / `srv03rtm`) + 64-bit Linux ELF loader & `syscall` compatibility shim + 64-bit Alpine Xfce4 + 64-bit Chromium/SwiftShader without 32-bit address space limits.
4. **`illumos_embedded` — OpenSolaris / illumos `unix` + `genunix` Kernel + BrandZ / Native Xfce4**
   - **Stack**: SunOS 5.x `unix`/`genunix` (`x86_64`) built with Bazel + native `libc.so.1` or `lx` BrandZ Linuxulator + BusyBox + Xfce4.
5. **`plan9_embedded` — 9front (`pc64` / `bcm64` Pi 4B) Kernel + 9P `/dev/draw` X11 Bridge**
   - **Stack**: Plan 9 kernel on `x86_64`/`arm64` + Linux ELF shim translating syscalls and X11 framebuffer blits directly into Plan 9 `/dev/draw`, `/dev/mouse`, `/dev/kbd`, and `/proc` file operations.
