#include <elf.h>

#include <algorithm>

#include "src/shim/mem_space.h"
#include "src/shim/shim_common.h"
#include "src/shim/vfs.h"

namespace {

// Restricted-address-space layout of a freshly exec'd image. kStackBottom
// must match kStackGuardTop in mem_space.cc.
constexpr uintptr_t kPieLoadBias = 0x0000000000400000ULL;
constexpr uintptr_t kInterpLoadBias = 0x0000002000000000ULL;
constexpr uintptr_t kStackBottom = 0x0000003ff0000000ULL;
constexpr size_t kStackSize = 8 * 1024 * 1024;  // 8 MB (RLIMIT_STACK default)
constexpr uintptr_t kStackTop = kStackBottom + kStackSize;
constexpr uintptr_t kMinBrkBase = 0x30000000ULL;

// Linux execve limits: each argv/envp string is at most MAX_ARG_STRLEN and
// strings + pointer arrays may use at most a quarter of the stack rlimit.
constexpr size_t kMaxArgStrlen = 32 * 4096;
constexpr size_t kMaxArgBytes = kStackSize / 4;
constexpr int kMaxInterpreterDepth = 4;  // nested #! scripts (Linux: 4 + 1)

struct PreparedSegment {
  uintptr_t start = 0;
  size_t len = 0;
  uint32_t prot = 0;
  std::shared_ptr<VmoRef> vmo;
  uint64_t file_offset = 0;
  uint64_t file_len = 0;
};

struct PreparedImage {
  uintptr_t entry = 0;
  uintptr_t load_bias = 0;
  uintptr_t phdr_vaddr = 0;
  uint16_t phent = 0;
  uint16_t phnum = 0;
  uintptr_t min_vaddr = 0;  // page-aligned, biased
  uintptr_t max_vaddr_end = 0;
  bool exec_stack = false;  // PT_GNU_STACK with PF_X
  std::string interp_path;
  VfsNode* node = nullptr;
  std::vector<PreparedSegment> segments;
};

uint32_t PFlagsToProt(uint32_t pf) {
  uint32_t p = 0;
  if (pf & PF_R) p |= kLinuxProtRead;
  if (pf & PF_W) p |= kLinuxProtWrite;
  if (pf & PF_X) p |= kLinuxProtExec;
  return p;
}

bool RangesOverlap(uintptr_t a, uintptr_t a_end, uintptr_t b, uintptr_t b_end) {
  return a < b_end && b < a_end;
}

// Parses and validates an ELF image and builds fully populated segment VMOs.
// Has no effect on the process's address space, so failures are recoverable.
int64_t PrepareElfImage(const LinuxProcess* proc,
                        const std::vector<uint8_t>& file, VfsNode* node,
                        uintptr_t default_pie_bias, PreparedImage* out) {
  if (file.size() < sizeof(Elf64_Ehdr)) return kENOEXEC;
  const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(file.data());
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 ||
      ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
      ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
#if defined(__x86_64__)
      ehdr->e_machine != EM_X86_64 ||
#elif defined(__aarch64__)
      ehdr->e_machine != EM_AARCH64 ||
#endif
      (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)) {
    return kENOEXEC;
  }
  if (ehdr->e_phentsize != sizeof(Elf64_Phdr) || ehdr->e_phnum == 0) {
    return kENOEXEC;
  }
  uint64_t ph_bytes = static_cast<uint64_t>(ehdr->e_phnum) * sizeof(Elf64_Phdr);
  if (ehdr->e_phoff > file.size() || ph_bytes > file.size() - ehdr->e_phoff) {
    return kENOEXEC;
  }
  const auto* phdrs =
      reinterpret_cast<const Elf64_Phdr*>(file.data() + ehdr->e_phoff);

  uintptr_t min_vaddr = UINTPTR_MAX;
  for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
    const Elf64_Phdr& ph = phdrs[i];
    if (ph.p_type == PT_LOAD) {
      if (ph.p_memsz == 0) continue;
      if (ph.p_filesz > ph.p_memsz) return kENOEXEC;
      if (ph.p_offset > file.size() || ph.p_filesz > file.size() - ph.p_offset) {
        return kENOEXEC;
      }
      if (ph.p_vaddr > UINT64_MAX - ph.p_memsz - kPageSize) return kENOEXEC;
      min_vaddr = std::min<uintptr_t>(min_vaddr, ph.p_vaddr);
    } else if (ph.p_type == PT_INTERP) {
      if (!out->interp_path.empty()) return kENOEXEC;  // duplicate PT_INTERP
      if (ph.p_filesz < 2 || ph.p_offset > file.size() ||
          ph.p_filesz > file.size() - ph.p_offset) {
        return kENOEXEC;
      }
      const char* p = reinterpret_cast<const char*>(file.data() + ph.p_offset);
      if (p[ph.p_filesz - 1] != '\0') return kENOEXEC;  // must be terminated
      out->interp_path.assign(p, strnlen(p, ph.p_filesz));
      if (out->interp_path.empty()) return kENOEXEC;
    } else if (ph.p_type == PT_GNU_STACK) {
      out->exec_stack = (ph.p_flags & PF_X) != 0;
    }
  }
  if (min_vaddr == UINTPTR_MAX) return kENOEXEC;

  uintptr_t bias = 0;
  if (ehdr->e_type == ET_DYN && min_vaddr == 0) {
    bias = default_pie_bias;
  }

  out->node = node;
  out->load_bias = bias;
  out->entry = bias + ehdr->e_entry;
  out->phent = ehdr->e_phentsize;
  out->phnum = ehdr->e_phnum;
  out->phdr_vaddr = 0;
  out->min_vaddr = PageRoundDown(bias + min_vaddr);
  out->max_vaddr_end = 0;

  for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
    if (phdrs[i].p_type == PT_PHDR) out->phdr_vaddr = bias + phdrs[i].p_vaddr;
  }

  for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
    const Elf64_Phdr& ph = phdrs[i];
    if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;

    uintptr_t seg_start = PageRoundDown(bias + ph.p_vaddr);
    uint64_t seg_end = 0;
    if (!PageRoundUp(bias + ph.p_vaddr + ph.p_memsz, &seg_end)) return kENOEXEC;
    size_t map_len = seg_end - seg_start;
    uintptr_t page_offset = (bias + ph.p_vaddr) - seg_start;
    if (!RangeInVmar(proc, seg_start, map_len)) {
      ShimLogf("[elf_loader] PT_LOAD 0x%llx+0x%llx is outside the address space\n",
               static_cast<unsigned long long>(seg_start),
               static_cast<unsigned long long>(map_len));
      return kENOMEM;
    }

    if (out->phdr_vaddr == 0 && ph.p_offset <= ehdr->e_phoff &&
        ehdr->e_phoff < ph.p_offset + ph.p_filesz) {
      out->phdr_vaddr = bias + ph.p_vaddr + (ehdr->e_phoff - ph.p_offset);
    }

    PreparedSegment seg;
    seg.start = seg_start;
    seg.len = map_len;
    seg.prot = PFlagsToProt(ph.p_flags);

    struct SegCacheKey {
      VfsNode* node;
      int64_t mtime_ns;
      uint16_t ph_idx;
      uintptr_t seg_start;
      size_t map_len;
      bool operator<(const SegCacheKey& o) const {
        if (node != o.node) return node < o.node;
        if (mtime_ns != o.mtime_ns) return mtime_ns < o.mtime_ns;
        if (ph_idx != o.ph_idx) return ph_idx < o.ph_idx;
        if (seg_start != o.seg_start) return seg_start < o.seg_start;
        return map_len < o.map_len;
      }
    };
    static std::mutex g_seg_cache_mtx;
    static std::map<SegCacheKey, std::shared_ptr<VmoRef>> g_seg_cache;

    std::shared_ptr<VmoRef> tmpl_vmo;
    if (node != nullptr) {
      std::lock_guard<std::mutex> lock(g_seg_cache_mtx);
      auto it = g_seg_cache.find(
          SegCacheKey{node, node->mtime_ns, i, seg_start, map_len});
      if (it != g_seg_cache.end()) tmpl_vmo = it->second;
    }

    if (!tmpl_vmo) {
      if (CreateAnonVmo(map_len, &tmpl_vmo) != ZX_OK) return kENOMEM;
      // When this segment's first page is shared with an earlier PT_LOAD, keep
      // the earlier segment's bytes that precede p_vaddr in that page (the page
      // itself takes this segment's protection, as with Linux's MAP_FIXED).
      if (page_offset > 0) {
        for (const auto& prev : out->segments) {
          if (prev.start <= seg_start && seg_start < prev.start + prev.len) {
            std::vector<uint8_t> head(page_offset);
            if (zx_vmo_read(prev.vmo->handle, head.data(),
                            seg_start - prev.start, page_offset) != ZX_OK ||
                zx_vmo_write(tmpl_vmo->handle, head.data(), 0, page_offset) !=
                    ZX_OK) {
              return kENOMEM;
            }
            break;
          }
        }
      }
      if (ph.p_filesz > 0 &&
          zx_vmo_write(tmpl_vmo->handle, file.data() + ph.p_offset, page_offset,
                       ph.p_filesz) != ZX_OK) {
        return kENOMEM;
      }
      if (node != nullptr) {
        std::lock_guard<std::mutex> lock(g_seg_cache_mtx);
        g_seg_cache[SegCacheKey{node, node->mtime_ns, i, seg_start, map_len}] =
            tmpl_vmo;
      }
    }

    zx_handle_t snap = ZX_HANDLE_INVALID;
    if (zx_vmo_create_child(tmpl_vmo->handle, ZX_VMO_CHILD_SNAPSHOT, 0, map_len,
                            &snap) == ZX_OK) {
      snap = VmoMakeExecutable(snap);
      if (snap == ZX_HANDLE_INVALID) return kENOMEM;
      seg.vmo = std::make_shared<VmoRef>(snap);
    } else {
      seg.vmo = tmpl_vmo;
    }

    // p_offset and p_vaddr are congruent modulo the page size for loadable
    // ELF files, so the segment's first page starts at this file offset.
    seg.file_offset = (ph.p_offset >= page_offset) ? ph.p_offset - page_offset : 0;
    seg.file_len = page_offset + ph.p_filesz;
    out->segments.push_back(std::move(seg));
    out->max_vaddr_end = std::max<uintptr_t>(out->max_vaddr_end, seg_end);
  }
  return 0;
}

std::string ExpandExecPath(LinuxProcess* proc, const std::string& raw_path) {
  std::string cur = Vfs::NormalizePath(proc->cwd, raw_path);
  for (int i = 0; i < 8; ++i) {
    if (cur == "/proc/self") {
      cur = "/proc/" + std::to_string(proc->pid);
      continue;
    }
    if (cur.rfind("/proc/self/", 0) == 0) {
      cur = "/proc/" + std::to_string(proc->pid) + cur.substr(10);
      continue;
    }
    std::string target;
    if (Vfs::ReadProcSymlink(proc, cur, &target) && !target.empty() &&
        target[0] == '/') {
      cur = Vfs::NormalizePath("/", target);
      continue;
    }
    break;
  }
  return cur;
}

// Looks up an executable in the VFS and returns its VfsNode without copying
// the underlying byte vector.
int64_t LookupExecutableNode(LinuxProcess* proc, const std::string& path,
                             std::string* resolved_out, VfsNode** node_out) {
  std::string expanded = ExpandExecPath(proc, path);
  std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
  VfsNode* node = Vfs::Lookup(proc->cwd, expanded, true, 0);
  if (!node) return kENOENT;
  if (node->type != VfsNodeType::kRegular) return kEACCES;
  if (node->shared_vmo != ZX_HANDLE_INVALID) Vfs::SyncNodeFromSharedVmo(node);
  *resolved_out = expanded;
  *node_out = node;
  return 0;
}

struct StackImage {
  std::vector<uint8_t> bytes;  // contents of [sp, kStackTop)
  uintptr_t sp = 0;
};

// Builds the initial Linux x86_64 process stack (strings, argv/envp/auxv).
int64_t BuildInitialStack(const std::vector<std::string>& args,
                          const std::vector<std::string>& env,
                          const std::string& execfn,
                          const PreparedImage& main_img, uintptr_t interp_base,
                          StackImage* out) {
  // Size accounting with Linux's limits; never write past the buffer.
#if defined(__aarch64__)
  const std::string kPlatform = "aarch64";
  uint64_t strings = 16 /* AT_RANDOM */ + execfn.size() + 1 + kPlatform.size() + 1;
#else
  uint64_t strings = 16 /* AT_RANDOM */ + execfn.size() + 1;
#endif
  for (const auto* list : {&args, &env}) {
    for (const auto& s : *list) {
      if (s.size() + 1 > kMaxArgStrlen) return kE2BIG;
      strings += s.size() + 1;
    }
  }
  uint64_t ptr_bytes = (args.size() + env.size() + 2) * sizeof(uint64_t);
  if (strings + ptr_bytes > kMaxArgBytes) return kE2BIG;

  struct AuxvEntry {
    uint64_t a_type;
    uint64_t a_val;
  };
  uint8_t rand_bytes[16];
  zx_cprng_draw(rand_bytes, sizeof(rand_bytes));

  // Strings occupy the top of the stack; the pointer block sits below them,
  // and sp (pointing at argc) is 16-byte aligned as the ABI requires.
  const uintptr_t strings_start = kStackTop - strings;
  const uintptr_t at_random_vaddr = kStackTop - 16;
  const uintptr_t at_execfn_vaddr = at_random_vaddr - (execfn.size() + 1);
#if defined(__aarch64__)
  const uintptr_t at_platform_vaddr = at_execfn_vaddr - (kPlatform.size() + 1);
#endif

  std::vector<AuxvEntry> auxv = {
      {AT_PHDR, main_img.phdr_vaddr},
      {AT_PHENT, main_img.phent},
      {AT_PHNUM, main_img.phnum},
      {AT_PAGESZ, kPageSize},
      {AT_BASE, interp_base},
      {AT_FLAGS, 0},
      {AT_ENTRY, main_img.entry},
      {AT_UID, 0},
      {AT_EUID, 0},
      {AT_GID, 0},
      {AT_EGID, 0},
      {AT_SECURE, 0},
      {AT_RANDOM, at_random_vaddr},
#if defined(__aarch64__)
      {AT_PLATFORM, at_platform_vaddr},
      {AT_HWCAP, 0xff},
#endif
      {AT_EXECFN, at_execfn_vaddr},
      {AT_NULL, 0},
  };
  uint64_t qwords = 1 + args.size() + 1 + env.size() + 1 + 2 * auxv.size();
  uintptr_t sp = ((strings_start & ~uintptr_t{15}) - qwords * 8) & ~uintptr_t{15};
  size_t total = kStackTop - sp;
  if (total > kStackSize) return kE2BIG;

  out->bytes.assign(total, 0);
  out->sp = sp;
  auto at = [&](uintptr_t vaddr) -> uint8_t* {
    return out->bytes.data() + (vaddr - sp);
  };
  memcpy(at(at_random_vaddr), rand_bytes, sizeof(rand_bytes));
  memcpy(at(at_execfn_vaddr), execfn.c_str(), execfn.size() + 1);
#if defined(__aarch64__)
  memcpy(at(at_platform_vaddr), kPlatform.c_str(), kPlatform.size() + 1);
  uintptr_t cursor = at_platform_vaddr;
#else
  uintptr_t cursor = at_execfn_vaddr;
#endif
  std::vector<uint64_t> env_ptrs, arg_ptrs;
  for (const auto& e : env) {
    cursor -= e.size() + 1;
    memcpy(at(cursor), e.c_str(), e.size() + 1);
    env_ptrs.push_back(cursor);
  }
  for (const auto& a : args) {
    cursor -= a.size() + 1;
    memcpy(at(cursor), a.c_str(), a.size() + 1);
    arg_ptrs.push_back(cursor);
  }

  uint64_t* q = reinterpret_cast<uint64_t*>(at(sp));
  size_t qi = 0;
  q[qi++] = args.size();
  for (uint64_t p : arg_ptrs) q[qi++] = p;
  q[qi++] = 0;
  for (uint64_t p : env_ptrs) q[qi++] = p;
  q[qi++] = 0;
  for (const auto& a : auxv) {
    q[qi++] = a.a_type;
    q[qi++] = a.a_val;
  }
  return 0;
}

}  // namespace

int64_t LoadLinuxExecutable(LinuxProcess* proc, const std::string& path,
                            const std::vector<std::string>& args,
                            const std::vector<std::string>& env,
                            uintptr_t* out_entry, uintptr_t* out_sp) {
  // ---- Phase 1: resolve and validate everything. No side effects. ----
  std::string resolved_path = path;
  std::vector<std::string> effective_args = args;
  VfsNode* exe_node = nullptr;
  bool found_binary = false;
  for (int depth = 0; depth <= kMaxInterpreterDepth; ++depth) {
    VfsNode* node = nullptr;
    std::string expanded;
    int64_t rc = LookupExecutableNode(proc, resolved_path, &expanded, &node);
    if (rc < 0) return rc;
    const std::vector<uint8_t>& bytes = node->data;
    if (bytes.size() < 4) return kENOEXEC;

    if (bytes[0] == '#' && bytes[1] == '!') {
      size_t line_end = 2;
      while (line_end < bytes.size() && bytes[line_end] != '\n' &&
             bytes[line_end] != '\r') {
        line_end++;
      }
      std::string line(reinterpret_cast<const char*>(bytes.data() + 2),
                       line_end - 2);
      size_t start = 0;
      while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
        start++;
      line = line.substr(start);
      while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
        line.pop_back();
      if (line.empty()) return kENOEXEC;

      std::string interp;
      std::string opt_arg;
      size_t sp = line.find_first_of(" \t");
      if (sp == std::string::npos) {
        interp = line;
      } else {
        interp = line.substr(0, sp);
        size_t arg_start = sp;
        while (arg_start < line.size() &&
               (line[arg_start] == ' ' || line[arg_start] == '\t'))
          arg_start++;
        opt_arg = line.substr(arg_start);
      }

      std::vector<std::string> new_args;
      new_args.push_back(interp);
      if (!opt_arg.empty()) new_args.push_back(opt_arg);
      new_args.push_back(expanded);
      for (size_t i = 1; i < effective_args.size(); ++i) {
        new_args.push_back(effective_args[i]);
      }
      effective_args = std::move(new_args);
      resolved_path = interp;
      continue;
    }

    exe_node = node;
    resolved_path = expanded;
    found_binary = true;
    break;
  }
  if (!found_binary) return kELOOP;  // too many nested #! interpreters

  PreparedImage main_img;
  int64_t rc = PrepareElfImage(proc, exe_node->data, exe_node, kPieLoadBias, &main_img);
  if (rc < 0) return rc;

  PreparedImage interp_img;
  bool has_interp = !main_img.interp_path.empty();
  if (has_interp) {
    VfsNode* interp_node = nullptr;
    std::string interp_expanded;
    rc = LookupExecutableNode(proc, main_img.interp_path, &interp_expanded,
                              &interp_node);
    if (rc < 0) {
      ShimLogf("[elf_loader] PT_INTERP %s: %lld\n", main_img.interp_path.c_str(),
               static_cast<long long>(rc));
      return rc;
    }
    rc = PrepareElfImage(proc, interp_node->data, interp_node, kInterpLoadBias,
                         &interp_img);
    if (rc < 0) return rc;
    // The interpreter must be a plain ELF image (Linux: -ELIBBAD).
    if (!interp_img.interp_path.empty()) return -80;  // ELIBBAD
  }

  // The fixed layout must not overlap: main image, interpreter, stack.
  const uintptr_t stack_guard_lo = kStackBottom - 256 * kPageSize;
  if (RangesOverlap(main_img.min_vaddr, main_img.max_vaddr_end, stack_guard_lo,
                    kStackTop) ||
      (has_interp &&
       (RangesOverlap(main_img.min_vaddr, main_img.max_vaddr_end,
                      interp_img.min_vaddr, interp_img.max_vaddr_end) ||
        RangesOverlap(interp_img.min_vaddr, interp_img.max_vaddr_end,
                      stack_guard_lo, kStackTop)))) {
    return kENOMEM;
  }
  if (!RangeInVmar(proc, kStackBottom, kStackSize)) return kENOMEM;

  StackImage stack;
  rc = BuildInitialStack(effective_args, env, resolved_path, main_img,
                         has_interp ? interp_img.load_bias : 0, &stack);
  if (rc < 0) return rc;

  MemRegion stack_region;
  stack_region.kind = RegionKind::kAnonPrivate;
  stack_region.prot = kLinuxProtRead | kLinuxProtWrite |
                      (main_img.exec_stack ? kLinuxProtExec : 0);
  if (CreateAnonVmo(kStackSize, &stack_region.vmo) != ZX_OK) return kENOMEM;
  if (zx_vmo_write(stack_region.vmo->handle, stack.bytes.data(),
                   kStackSize - stack.bytes.size(), stack.bytes.size()) != ZX_OK) {
    return kENOMEM;
  }

  // ---- Phase 2: point of no return. Replace the address space. ----
  // From here on a failure leaves the process without a usable image; the
  // only possible failures are kernel resource exhaustion, and the caller
  // must terminate the process (Linux delivers SIGKILL in this situation).
  {
    std::lock_guard<std::mutex> lock(proc->map_mtx);
    TearDownAddressSpaceLocked(proc);

    auto map_image = [&](const PreparedImage& img) -> int64_t {
      for (const auto& seg : img.segments) {
        MemRegion r;
        r.kind = RegionKind::kFilePrivate;
        r.prot = seg.prot;
        r.vmo = seg.vmo;
        r.node = img.node;
        r.file_offset = seg.file_offset;
        r.file_len = seg.file_len;
        // Later segments replace a shared first page, as on Linux.
        zx_status_t st = MapRegionLocked(proc, seg.start, seg.len, std::move(r),
                                         /*overwrite=*/true);
        if (st != ZX_OK) {
          ShimLogf("[elf_loader] zx_vmar_map failed at 0x%llx (len=0x%llx): %d\n",
                   static_cast<unsigned long long>(seg.start),
                   static_cast<unsigned long long>(seg.len), st);
          return kENOMEM;
        }
      }
      return 0;
    };
    rc = map_image(main_img);
    if (rc == 0 && has_interp) rc = map_image(interp_img);
    if (rc == 0 &&
        MapRegionLocked(proc, kStackBottom, kStackSize, std::move(stack_region),
                        /*overwrite=*/false) != ZX_OK) {
      rc = kENOMEM;
    }
    if (rc < 0) {
      TearDownAddressSpaceLocked(proc);
      return rc;
    }

    uintptr_t brk_base = std::max<uintptr_t>(
        kMinBrkBase, PageRoundDown(main_img.max_vaddr_end + 0x100000ULL));
    proc->brk_start = brk_base;
    proc->brk_current = brk_base;
    proc->brk_mapped_end = brk_base;
    FlushSharedMappingsLocked(proc);
  }

  proc->exe_path = resolved_path;
  size_t last_slash = resolved_path.find_last_of('/');
  proc->comm = (last_slash == std::string::npos)
                   ? resolved_path
                   : resolved_path.substr(last_slash + 1);
  proc->cmdline.clear();
  for (const auto& a : effective_args) {
    proc->cmdline += a;
    proc->cmdline.push_back('\0');
  }

  *out_entry = has_interp ? interp_img.entry : main_img.entry;
  *out_sp = stack.sp;
  return 0;
}
