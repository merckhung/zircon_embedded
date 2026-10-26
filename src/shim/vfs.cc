#include "src/shim/vfs.h"

#if defined(__x86_64__)
#include <cpuid.h>
#endif

#include <set>

#include "src/shim/fd_ops.h"
#include "src/shim/net_inet.h"
#include "src/shim/tty.h"

namespace {

std::recursive_mutex g_vfs_mtx;
VfsNode* g_root = nullptr;
std::atomic<uint64_t> g_next_ino{2};

constexpr uint32_t kSIfDir = 0040000;
constexpr uint32_t kSIfReg = 0100000;
constexpr uint32_t kSIfLnk = 0120000;
constexpr uint32_t kSIfChr = 0020000;
constexpr uint32_t kSIfSock = 0140000;

uint32_t TypeBits(VfsNodeType t) {
  switch (t) {
    case VfsNodeType::kDirectory:
      return kSIfDir;
    case VfsNodeType::kSymlink:
      return kSIfLnk;
    case VfsNodeType::kCharDevice:
      return kSIfChr;
    case VfsNodeType::kSocket:
      return kSIfSock;
    case VfsNodeType::kRegular:
    default:
      return kSIfReg;
  }
}

std::vector<std::string> SplitComponents(const std::string& path) {
  std::vector<std::string> parts;
  size_t i = 0;
  while (i < path.size()) {
    while (i < path.size() && path[i] == '/') i++;
    if (i >= path.size()) break;
    size_t j = i;
    while (j < path.size() && path[j] != '/') j++;
    std::string comp = path.substr(i, j - i);
    if (comp == ".") {
      // skip
    } else if (comp == "..") {
      if (!parts.empty()) parts.pop_back();
    } else {
      parts.push_back(comp);
    }
    i = j;
  }
  return parts;
}

uint64_t ParseOctal(const char* s, size_t len) {
  // Handle GNU base-256 extension if high bit set
  if (len > 0 && (static_cast<uint8_t>(s[0]) & 0x80) != 0) {
    uint64_t val = 0;
    for (size_t i = 1; i < len; ++i) {
      val = (val << 8) | static_cast<uint8_t>(s[i]);
    }
    return val;
  }
  uint64_t val = 0;
  for (size_t i = 0; i < len; ++i) {
    char c = s[i];
    if (c == ' ' || c == '\0') continue;
    if (c >= '0' && c <= '7') {
      val = (val << 3) + static_cast<uint64_t>(c - '0');
    } else {
      break;
    }
  }
  return val;
}

VfsNode* NewNode(const std::string& name, VfsNodeType type, uint32_t perm,
                 int64_t now) {
  VfsNode* n = new VfsNode();
  n->name = name;
  n->type = type;
  n->mode = TypeBits(type) | (perm & 07777);
  n->ino = Vfs::AllocInode();
  n->nlink = (type == VfsNodeType::kDirectory) ? 2 : 1;
  n->atime_ns = n->mtime_ns = n->ctime_ns = now;
  return n;
}

// Link `child` into `dir` under `name`. Updates directory link counts and
// timestamps; optionally reports IN_CREATE.
void InsertChild(VfsNode* dir, const std::string& name, VfsNode* child,
                 bool notify) {
  dir->children[name] = child;
  if (child->parent == nullptr) {
    child->parent = dir;
    child->name = name;
  }
  if (child->type == VfsNodeType::kDirectory) dir->nlink++;
  int64_t now = Vfs::NowNs();
  dir->mtime_ns = dir->ctime_ns = now;
  if (notify) {
    InotifyDirEvent(dir, kInCreate, name, 0,
                    child->type == VfsNodeType::kDirectory);
  }
}

// Remove entry `name` (whose node is `child`) from `dir`. Returns true if the
// node lost its last link.
bool RemoveChild(VfsNode* dir, const std::string& name, VfsNode* child) {
  dir->children.erase(name);
  if (child->type == VfsNodeType::kDirectory) {
    if (dir->nlink > 2) dir->nlink--;
    child->nlink = 0;
  } else if (child->nlink > 0) {
    child->nlink--;
  }
  int64_t now = Vfs::NowNs();
  dir->mtime_ns = dir->ctime_ns = now;
  child->ctime_ns = now;
  if (child->nlink > 0 && child->parent == dir && child->name == name) {
    // The node keeps other hard links; re-point its primary link to one of
    // them so NodePath() stays valid. (Search is bounded to the same dir.)
    for (const auto& kv : dir->children) {
      if (kv.second == child) {
        child->name = kv.first;
        break;
      }
    }
  }
  return child->nlink == 0;
}

// Resolves `abs` to a node. Errors: -ENOENT, -ENOTDIR, -ELOOP.
int64_t Resolve(const std::string& abs, bool follow_final, int depth,
                VfsNode** out) {
  if (depth > 40) return kELOOP;
  auto parts = SplitComponents(abs);
  VfsNode* cur = g_root;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (cur->type != VfsNodeType::kDirectory) return kENOTDIR;
    auto it = cur->children.find(parts[i]);
    if (it == cur->children.end()) return kENOENT;
    VfsNode* next = it->second;
    bool is_last = (i + 1 == parts.size());
    if (next->type == VfsNodeType::kSymlink && (!is_last || follow_final)) {
      std::string base = Vfs::NodePath(cur);
      std::string target = Vfs::NormalizePath(base, next->symlink_target);
      VfsNode* t = nullptr;
      int64_t rc = Resolve(target, true, depth + 1, &t);
      if (rc < 0) return rc;
      cur = t;
    } else {
      cur = next;
    }
  }
  *out = cur;
  return 0;
}

// Resolves the parent directory of `abs` and its final component.
int64_t ResolveParent(const std::string& abs, VfsNode** parent,
                      std::string* base) {
  auto parts = SplitComponents(abs);
  if (parts.empty()) return kEEXIST;  // "/" itself
  *base = parts.back();
  parts.pop_back();
  std::string pp = "/";
  for (const auto& p : parts) pp += "/" + p;
  VfsNode* dir = nullptr;
  int64_t rc = Resolve(Vfs::NormalizePath("/", pp), true, 0, &dir);
  if (rc < 0) return rc;
  if (dir->type != VfsNodeType::kDirectory) return kENOTDIR;
  *parent = dir;
  return 0;
}

// --- /proc helpers ---------------------------------------------------------

zx_handle_t InfoResource() {
  static std::mutex mtx;
  static zx_handle_t rsrc = ZX_HANDLE_INVALID;
  std::lock_guard<std::mutex> lock(mtx);
  if (rsrc == ZX_HANDLE_INVALID && g_sys_rsrc != ZX_HANDLE_INVALID) {
    zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM, ZX_RSRC_SYSTEM_INFO_BASE,
                       1, "info", 4, &rsrc);
  }
  return rsrc;
}

bool GetKmemStats(zx_info_kmem_stats_t* out) {
  zx_handle_t r = InfoResource();
  if (r == ZX_HANDLE_INVALID) return false;
  return zx_object_get_info(r, ZX_INFO_KMEM_STATS, out, sizeof(*out), nullptr,
                            nullptr) == ZX_OK;
}

std::vector<zx_info_cpu_stats_t> GetCpuStats() {
  std::vector<zx_info_cpu_stats_t> stats;
  zx_handle_t r = InfoResource();
  if (r == ZX_HANDLE_INVALID) return stats;
  uint32_t n = zx_system_get_num_cpus();
  stats.resize(n ? n : 1);
  size_t actual = 0, avail = 0;
  if (zx_object_get_info(r, ZX_INFO_CPU_STATS, stats.data(),
                         stats.size() * sizeof(zx_info_cpu_stats_t), &actual,
                         &avail) != ZX_OK) {
    stats.clear();
    return stats;
  }
  stats.resize(actual);
  return stats;
}

constexpr uint64_t kUserHz = 100;  // USER_HZ / sysconf(_SC_CLK_TCK)
uint64_t NsToTicks(uint64_t ns) { return ns / (1000000000ULL / kUserHz); }

std::shared_ptr<LinuxProcess> FindProcess(int pid) {
  std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
  auto it = g_processes.find(pid);
  if (it == g_processes.end()) return nullptr;
  return it->second;
}

struct ProcMemInfo {
  uint64_t vsize = 0;      // bytes mapped in the restricted (Linux) range
  uint64_t rss = 0;        // committed bytes in that range
  bool ok = false;
};

std::vector<zx_info_maps_t> GetProcessMaps(LinuxProcess* p) {
  std::vector<zx_info_maps_t> maps;
  if (p->zx_proc == ZX_HANDLE_INVALID) return maps;
  maps.resize(256);
  size_t actual = 0, avail = 0;
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (zx_object_get_info(p->zx_proc, ZX_INFO_PROCESS_MAPS, maps.data(),
                           maps.size() * sizeof(zx_info_maps_t), &actual,
                           &avail) != ZX_OK) {
      maps.clear();
      return maps;
    }
    if (avail <= maps.size()) break;
    maps.resize(avail + 32);
  }
  maps.resize(actual);
  // Keep only the mappings inside the process's Linux address range (the
  // shared region holds the shim itself).
  std::vector<zx_info_maps_t> out;
  for (const auto& m : maps) {
    if (m.type != ZX_INFO_MAPS_TYPE_MAPPING) continue;
    if (m.base < p->vmar_base || m.base + m.size > p->vmar_base + p->vmar_len) {
      continue;
    }
    out.push_back(m);
  }
  return out;
}

ProcMemInfo GetProcMem(LinuxProcess* p) {
  ProcMemInfo info;
  auto maps = GetProcessMaps(p);
  if (maps.empty() && p->zx_proc == ZX_HANDLE_INVALID) return info;
  for (const auto& m : maps) {
    info.vsize += m.size;
    info.rss += m.u.mapping.committed_bytes;
  }
  info.ok = true;
  return info;
}

// comm of the thread-group leader (tid == pid), else the process comm.
std::string MainComm(LinuxProcess* p) {
  std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
  for (const auto& t : p->threads) {
    if (t->tid == p->pid && !t->comm.empty()) return t->comm.substr(0, 15);
  }
  return p->comm.substr(0, 15);
}

std::string FdLinkTarget(LinuxProcess* p, int fd) {
  std::shared_ptr<FileDescription> d;
  {
    std::lock_guard<std::recursive_mutex> lock(p->fd_mtx);
    auto it = p->fds.find(fd);
    if (it == p->fds.end()) return "";
    d = it->second.desc;
  }
  if (!d) return "";
  char buf[64];
  switch (d->kind) {
    case FdKind::kPipeRead:
    case FdKind::kPipeWrite:
      snprintf(buf, sizeof(buf), "pipe:[%llu]",
               static_cast<unsigned long long>(d->anon_ino));
      return buf;
    case FdKind::kUnixSocket:
      snprintf(buf, sizeof(buf), "socket:[%llu]",
               static_cast<unsigned long long>(d->anon_ino));
      return buf;
    case FdKind::kEventFd:
      return "anon_inode:[eventfd]";
    case FdKind::kTimerFd:
      return "anon_inode:[timerfd]";
    case FdKind::kSignalFd:
      return "anon_inode:[signalfd]";
    case FdKind::kInotify:
      return "anon_inode:inotify";
    case FdKind::kEpoll:
      return "anon_inode:[eventpoll]";
    default:
      break;
  }
  if (d->node) {
    std::lock_guard<std::recursive_mutex> vlock(g_vfs_mtx);
    std::string path = Vfs::NodePath(d->node);
    if (d->node->nlink == 0) path += " (deleted)";
    return path;
  }
  return d->path;
}

std::string Hex16(uint64_t v) {
  char b[20];
  snprintf(b, sizeof(b), "%016llx", static_cast<unsigned long long>(v));
  return b;
}

// CPU identification via the cpuid instruction.
std::string CpuInfoText() {
#if defined(__x86_64__)
  unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
  char vendor[13] = {};
  __cpuid(0, eax, ebx, ecx, edx);
  unsigned max_leaf = eax;
  memcpy(vendor + 0, &ebx, 4);
  memcpy(vendor + 4, &edx, 4);
  memcpy(vendor + 8, &ecx, 4);

  unsigned f1a = 0, f1b = 0, f1c = 0, f1d = 0;
  if (max_leaf >= 1) __cpuid(1, f1a, f1b, f1c, f1d);
  unsigned stepping = f1a & 0xF;
  unsigned model = (f1a >> 4) & 0xF;
  unsigned family = (f1a >> 8) & 0xF;
  if (family == 0xF) family += (f1a >> 20) & 0xFF;
  if (family == 0x6 || family >= 0xF) model |= ((f1a >> 16) & 0xF) << 4;

  unsigned f7b = 0, f7c = 0, f7d = 0, tmp = 0;
  if (max_leaf >= 7) __cpuid_count(7, 0, tmp, f7b, f7c, f7d);

  char brand[49] = {};
  unsigned ext_max = 0;
  __cpuid(0x80000000, ext_max, tmp, tmp, tmp);
  unsigned e1c = 0, e1d = 0;
  if (ext_max >= 0x80000001) __cpuid(0x80000001, tmp, tmp, e1c, e1d);
  if (ext_max >= 0x80000004) {
    unsigned* b = reinterpret_cast<unsigned*>(brand);
    __cpuid(0x80000002, b[0], b[1], b[2], b[3]);
    __cpuid(0x80000003, b[4], b[5], b[6], b[7]);
    __cpuid(0x80000004, b[8], b[9], b[10], b[11]);
  }
  std::string brand_s(brand);
  while (!brand_s.empty() && brand_s.front() == ' ') brand_s.erase(0, 1);

  // L2 cache size (KB) from extended leaf 0x80000006 if available.
  unsigned l2_kb = 0;
  if (ext_max >= 0x80000006) {
    unsigned c = 0;
    __cpuid(0x80000006, tmp, tmp, c, tmp);
    l2_kb = c >> 16;
  }
  // Base frequency (MHz) from leaf 0x16 when the CPU reports it.
  unsigned base_mhz = 0;
  if (max_leaf >= 0x16) {
    unsigned a = 0;
    __cpuid(0x16, a, tmp, tmp, tmp);
    base_mhz = a & 0xFFFF;
  }

  struct Flag {
    unsigned* reg;
    int bit;
    const char* name;
  };
  const Flag flags[] = {
      {&f1d, 0, "fpu"},     {&f1d, 4, "tsc"},       {&f1d, 5, "msr"},
      {&f1d, 6, "pae"},     {&f1d, 8, "cx8"},       {&f1d, 9, "apic"},
      {&f1d, 11, "sep"},    {&f1d, 13, "pge"},      {&f1d, 15, "cmov"},
      {&f1d, 16, "pat"},    {&f1d, 19, "clflush"},  {&f1d, 23, "mmx"},
      {&f1d, 24, "fxsr"},   {&f1d, 25, "sse"},      {&f1d, 26, "sse2"},
      {&f1d, 28, "ht"},     {&e1d, 11, "syscall"},  {&e1d, 20, "nx"},
      {&e1d, 26, "pdpe1gb"}, {&e1d, 27, "rdtscp"},  {&e1d, 29, "lm"},
      {&f1c, 0, "pni"},     {&f1c, 1, "pclmulqdq"}, {&f1c, 9, "ssse3"},
      {&f1c, 12, "fma"},    {&f1c, 13, "cx16"},     {&f1c, 19, "sse4_1"},
      {&f1c, 20, "sse4_2"}, {&f1c, 21, "x2apic"},   {&f1c, 22, "movbe"},
      {&f1c, 23, "popcnt"}, {&f1c, 25, "aes"},      {&f1c, 26, "xsave"},
      {&f1c, 28, "avx"},    {&f1c, 29, "f16c"},     {&f1c, 30, "rdrand"},
      {&f1c, 31, "hypervisor"}, {&e1c, 0, "lahf_lm"}, {&e1c, 5, "abm"},
      {&f7b, 3, "bmi1"},    {&f7b, 5, "avx2"},      {&f7b, 8, "bmi2"},
      {&f7b, 9, "erms"},    {&f7b, 16, "avx512f"},  {&f7b, 18, "rdseed"},
      {&f7b, 19, "adx"},    {&f7b, 29, "sha_ni"},   {&f7c, 22, "rdpid"},
  };
  std::string flag_str;
  for (const auto& f : flags) {
    if ((*f.reg >> f.bit) & 1) {
      if (!flag_str.empty()) flag_str += ' ';
      flag_str += f.name;
    }
  }

  uint32_t ncpus = zx_system_get_num_cpus();
  if (ncpus == 0) ncpus = 1;
  std::string res;
  char buf[512];
  for (uint32_t i = 0; i < ncpus; ++i) {
    snprintf(buf, sizeof(buf),
             "processor\t: %u\n"
             "vendor_id\t: %s\n"
             "cpu family\t: %u\n"
             "model\t\t: %u\n"
             "model name\t: %s\n"
             "stepping\t: %u\n",
             i, vendor, family, model, brand_s.c_str(), stepping);
    res += buf;
    if (base_mhz) {
      snprintf(buf, sizeof(buf), "cpu MHz\t\t: %u.000\n", base_mhz);
      res += buf;
    }
    if (l2_kb) {
      snprintf(buf, sizeof(buf), "cache size\t: %u KB\n", l2_kb);
      res += buf;
    }
    snprintf(buf, sizeof(buf), "siblings\t: %u\ncpu cores\t: %u\n", ncpus,
             ncpus);
    res += buf;
    res += "flags\t\t: " + flag_str + "\n\n";
  }
  return res;
#elif defined(__aarch64__)
  uint32_t ncpus = zx_system_get_num_cpus();
  if (ncpus == 0) ncpus = 1;
  std::string res;
  char buf[512];
  for (uint32_t i = 0; i < ncpus; ++i) {
    snprintf(buf, sizeof(buf),
             "processor\t: %u\n"
             "BogoMIPS\t: 48.00\n"
             "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics\n"
             "CPU implementer\t: 0x41\n"
             "CPU architecture: 8\n"
             "CPU variant\t: 0x1\n"
             "CPU part\t: 0xd07\n"
             "CPU revision\t: 1\n\n",
             i);
    res += buf;
  }
  return res;
#endif
}

std::mutex g_proc_ino_mtx;
std::map<std::string, uint64_t> g_proc_inos;

// Splits a /proc path into components with "self" resolved to the caller.
bool ParseProcPath(LinuxProcess* caller, const std::string& norm,
                   std::vector<std::string>* parts) {
  if (norm != "/proc" && norm.rfind("/proc/", 0) != 0) return false;
  *parts = SplitComponents(norm.substr(5));
  if (!parts->empty() && (*parts)[0] == "self" && parts->size() > 1) {
    (*parts)[0] = std::to_string(caller->pid);
  }
  return true;
}

bool IsAllDigits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

bool FbSysfsPath(const std::string& norm, std::vector<std::string>* parts) {
  if (norm.rfind("/sys/", 0) != 0) return false;
  if (!GetFbHardwareInfo().active) return false;
  *parts = SplitComponents(norm.substr(4));
  static const char* kPrefix[] = {"class", "graphics", "fb0"};
  for (size_t i = 0; i < parts->size() && i < 3; ++i) {
    if ((*parts)[i] != kPrefix[i]) return false;
  }
  return !parts->empty() && parts->size() <= 4;
}

const char* kFbSysfsFiles[] = {"name", "virtual_size", "bits_per_pixel",
                               "stride"};

}  // namespace

std::recursive_mutex& Vfs::Mutex() { return g_vfs_mtx; }
VfsNode* Vfs::Root() { return g_root; }

int64_t Vfs::NowNs() { return ShimRealtimeNs(); }

uint64_t Vfs::AllocInode() { return g_next_ino.fetch_add(1); }

std::string Vfs::NodePath(VfsNode* node) {
  if (!node || node == g_root) return "/";
  std::vector<std::string> parts;
  VfsNode* cur = node;
  int guard = 0;
  while (cur && cur != g_root && guard++ < 4096) {
    parts.push_back(cur->name);
    cur = cur->parent;
  }
  std::string res;
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    res += "/" + *it;
  }
  return res.empty() ? "/" : res;
}

void Vfs::Init() {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  if (g_root) return;

  int64_t now = NowNs();
  g_root = NewNode("", VfsNodeType::kDirectory, 0755, now);
  g_root->ino = 1;
  g_root->parent = g_root;

  // Kernel-provided mount points only. Everything else (/etc, /bin, /var,
  // /run, /root, ...) comes from the rootfs image and its init scripts.
  Mkdirs("/dev", 0755);
  Mkdirs("/dev/pts", 0755);
  Mkdirs("/dev/shm", 01777);
  Mkdirs("/dev/input", 0755);
  Mkdirs("/proc", 0555);
  Mkdirs("/sys", 0555);
  Mkdirs("/tmp", 01777);

  // Character devices implemented by the shim (see OpenPathCommon in
  // syscalls_fs.cc, which dispatches on rdev). /dev/fb0 and
  // /dev/input/event0 are created by InitHardware() only when the hardware
  // exists.
  CreateDevice("/dev/console", 0600, (5ULL << 8) | 1);
  CreateDevice("/dev/tty", 0666, (5ULL << 8) | 0);
  CreateDevice("/dev/ttyS0", 0660, (4ULL << 8) | 64);
  CreateDevice("/dev/ptmx", 0666, (5ULL << 8) | 2);
  CreateDevice("/dev/null", 0666, (1ULL << 8) | 3);
  CreateDevice("/dev/zero", 0666, (1ULL << 8) | 5);
  CreateDevice("/dev/full", 0666, (1ULL << 8) | 7);
  CreateDevice("/dev/random", 0666, (1ULL << 8) | 8);
  CreateDevice("/dev/urandom", 0666, (1ULL << 8) | 9);
}

std::string Vfs::NormalizePath(const std::string& cwd,
                               const std::string& raw_path) {
  std::string combined;
  if (!raw_path.empty() && raw_path[0] == '/') {
    combined = raw_path;
  } else {
    combined = cwd + "/" + raw_path;
  }
  auto parts = SplitComponents(combined);
  if (parts.empty()) return "/";
  std::string out;
  for (const auto& p : parts) {
    out += "/" + p;
  }
  return out;
}

VfsNode* Vfs::Lookup(const std::string& cwd, const std::string& path,
                     bool follow_final_symlink, int symlink_depth) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* out = nullptr;
  if (Resolve(NormalizePath(cwd, path), follow_final_symlink, symlink_depth,
              &out) < 0) {
    return nullptr;
  }
  return out;
}

int64_t Vfs::LookupErr(const std::string& abs_path, bool follow_final,
                       VfsNode** out) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  return Resolve(NormalizePath("/", abs_path), follow_final, 0, out);
}

VfsNode* Vfs::LookupParent(const std::string& cwd, const std::string& path,
                           std::string* out_basename) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* parent = nullptr;
  if (ResolveParent(NormalizePath(cwd, path), &parent, out_basename) < 0) {
    return nullptr;
  }
  return parent;
}

int64_t Vfs::MakeDir(const std::string& abs_path, uint32_t mode,
                     VfsNode** out) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* parent = nullptr;
  std::string base;
  int64_t rc = ResolveParent(NormalizePath("/", abs_path), &parent, &base);
  if (rc < 0) return rc;
  if (parent->children.count(base)) return kEEXIST;
  VfsNode* d = NewNode(base, VfsNodeType::kDirectory, mode, NowNs());
  InsertChild(parent, base, d, true);
  if (out) *out = d;
  return 0;
}

int64_t Vfs::MakeNode(const std::string& abs_path, VfsNodeType type,
                      uint32_t perm, uint64_t rdev, VfsNode** out) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* parent = nullptr;
  std::string base;
  int64_t rc = ResolveParent(NormalizePath("/", abs_path), &parent, &base);
  if (rc < 0) return rc;
  if (parent->children.count(base)) return kEEXIST;
  VfsNode* n = NewNode(base, type, perm, NowNs());
  n->rdev = rdev;
  InsertChild(parent, base, n, true);
  if (out) *out = n;
  return 0;
}

int64_t Vfs::MakeSymlink(const std::string& abs_path,
                         const std::string& target) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  if (target.empty()) return kENOENT;
  VfsNode* parent = nullptr;
  std::string base;
  int64_t rc = ResolveParent(NormalizePath("/", abs_path), &parent, &base);
  if (rc < 0) return rc;
  if (parent->children.count(base)) return kEEXIST;
  VfsNode* n = NewNode(base, VfsNodeType::kSymlink, 0777, NowNs());
  n->symlink_target = target;
  InsertChild(parent, base, n, true);
  return 0;
}

int64_t Vfs::Link(const std::string& old_abs, const std::string& new_abs,
                  bool follow_old) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* src = nullptr;
  int64_t rc = Resolve(NormalizePath("/", old_abs), follow_old, 0, &src);
  if (rc < 0) return rc;
  if (src->type == VfsNodeType::kDirectory) return kEPERM;
  VfsNode* parent = nullptr;
  std::string base;
  rc = ResolveParent(NormalizePath("/", new_abs), &parent, &base);
  if (rc < 0) return rc;
  if (parent->children.count(base)) return kEEXIST;
  src->nlink++;
  src->ctime_ns = NowNs();
  InsertChild(parent, base, src, true);
  InotifySelfEvent(src, kInAttrib);
  return 0;
}

VfsNode* Vfs::Mkdirs(const std::string& path, uint32_t mode) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  auto parts = SplitComponents(path);
  VfsNode* cur = g_root;
  for (const auto& comp : parts) {
    if (cur->type != VfsNodeType::kDirectory) return nullptr;
    auto it = cur->children.find(comp);
    if (it != cur->children.end()) {
      cur = it->second;
      if (cur->type == VfsNodeType::kSymlink) {
        VfsNode* resolved = nullptr;
        if (Resolve(NormalizePath(NodePath(cur->parent), cur->symlink_target),
                    true, 0, &resolved) < 0) {
          return nullptr;
        }
        cur = resolved;
      }
    } else {
      VfsNode* dir = NewNode(comp, VfsNodeType::kDirectory, mode, NowNs());
      InsertChild(cur, comp, dir, true);
      cur = dir;
    }
  }
  return cur;
}

VfsNode* Vfs::CreateFile(const std::string& cwd, const std::string& path,
                         uint32_t mode) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  std::string norm = NormalizePath(cwd, path);
  VfsNode* existing = nullptr;
  if (Resolve(norm, true, 0, &existing) == 0) return existing;
  VfsNode* parent = nullptr;
  std::string base;
  if (ResolveParent(norm, &parent, &base) < 0) return nullptr;
  auto it = parent->children.find(base);
  if (it != parent->children.end()) {
    // Dangling symlink: create its target.
    if (it->second->type == VfsNodeType::kSymlink) {
      return CreateFile(NodePath(parent), it->second->symlink_target, mode);
    }
    return it->second;
  }
  VfsNode* n = NewNode(base, VfsNodeType::kRegular, mode, NowNs());
  InsertChild(parent, base, n, true);
  return n;
}

VfsNode* Vfs::CreateSymlink(const std::string& cwd, const std::string& path,
                            const std::string& target) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* parent = nullptr;
  std::string base;
  if (ResolveParent(NormalizePath(cwd, path), &parent, &base) < 0) {
    return nullptr;
  }
  auto it = parent->children.find(base);
  if (it != parent->children.end()) RemoveChild(parent, base, it->second);
  VfsNode* n = NewNode(base, VfsNodeType::kSymlink, 0777, NowNs());
  n->symlink_target = target;
  InsertChild(parent, base, n, true);
  return n;
}

VfsNode* Vfs::CreateDevice(const std::string& path, uint32_t mode,
                           uint64_t rdev) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  VfsNode* parent = nullptr;
  std::string base;
  if (ResolveParent(NormalizePath("/", path), &parent, &base) < 0) {
    return nullptr;
  }
  auto it = parent->children.find(base);
  if (it != parent->children.end()) RemoveChild(parent, base, it->second);
  VfsNode* n = NewNode(base, VfsNodeType::kCharDevice, mode, NowNs());
  n->rdev = rdev;
  InsertChild(parent, base, n, true);
  return n;
}

int64_t Vfs::Unlink(const std::string& cwd, const std::string& path,
                    bool rmdir_only) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  std::string norm = NormalizePath(cwd, path);
  if (norm == "/") return rmdir_only ? kEBUSY : kEISDIR;
  if (rmdir_only) {
    // rmdir("x/.") is EINVAL on Linux.
    size_t e = path.find_last_not_of('/');
    if (e != std::string::npos && e >= 0 &&
        (path.substr(0, e + 1) == "." ||
         (e >= 1 && path.compare(e - 1, 2, "/.") == 0))) {
      return kEINVAL;
    }
  }
  VfsNode* parent = nullptr;
  std::string base;
  int64_t rc = ResolveParent(norm, &parent, &base);
  if (rc < 0) return rc;
  auto it = parent->children.find(base);
  if (it == parent->children.end()) return kENOENT;
  VfsNode* target = it->second;
  bool is_dir = target->type == VfsNodeType::kDirectory;
  if (rmdir_only) {
    if (!is_dir) return kENOTDIR;
    if (!target->children.empty()) return kENOTEMPTY;
  } else {
    if (is_dir) return kEISDIR;
  }
  if (target->type == VfsNodeType::kSocket) SocketPathUnlinked(target);
  bool last = RemoveChild(parent, base, target);
  InotifyDirEvent(parent, kInDelete, base, 0, is_dir);
  InotifySelfEvent(target, last ? kInDeleteSelf : kInAttrib);
  return 0;
}

int64_t Vfs::Rename(const std::string& cwd, const std::string& oldpath,
                    const std::string& newpath, uint32_t flags) {
  constexpr uint32_t kNoReplace = 1, kExchange = 2, kWhiteout = 4;
  if (flags & ~(kNoReplace | kExchange | kWhiteout)) return kEINVAL;
  if ((flags & kWhiteout) != 0) return kEINVAL;  // overlayfs-only feature
  if ((flags & kNoReplace) && (flags & kExchange)) return kEINVAL;

  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  std::string old_norm = NormalizePath(cwd, oldpath);
  std::string new_norm = NormalizePath(cwd, newpath);
  if (old_norm == "/" || new_norm == "/") return kEBUSY;
  VfsNode *old_parent = nullptr, *new_parent = nullptr;
  std::string old_base, new_base;
  int64_t rc = ResolveParent(old_norm, &old_parent, &old_base);
  if (rc < 0) return rc;
  rc = ResolveParent(new_norm, &new_parent, &new_base);
  if (rc < 0) return rc;

  auto it = old_parent->children.find(old_base);
  if (it == old_parent->children.end()) return kENOENT;
  VfsNode* node = it->second;
  bool node_is_dir = node->type == VfsNodeType::kDirectory;

  // A directory cannot be moved below itself.
  if (node_is_dir) {
    for (VfsNode* p = new_parent; p && p != g_root; p = p->parent) {
      if (p == node) return kEINVAL;
    }
  }

  auto nit = new_parent->children.find(new_base);
  VfsNode* existing = (nit == new_parent->children.end()) ? nullptr : nit->second;

  if (flags & kExchange) {
    if (!existing) return kENOENT;
    if (existing->type == VfsNodeType::kDirectory) {
      for (VfsNode* p = old_parent; p && p != g_root; p = p->parent) {
        if (p == existing) return kEINVAL;
      }
    }
    old_parent->children[old_base] = existing;
    new_parent->children[new_base] = node;
    if (node->parent == old_parent && node->name == old_base) {
      node->parent = new_parent;
      node->name = new_base;
    }
    if (existing->parent == new_parent && existing->name == new_base) {
      existing->parent = old_parent;
      existing->name = old_base;
    }
    bool ex_dir = existing->type == VfsNodeType::kDirectory;
    if (old_parent != new_parent && node_is_dir != ex_dir) {
      if (node_is_dir) {
        new_parent->nlink++;
        old_parent->nlink--;
      } else {
        old_parent->nlink++;
        new_parent->nlink--;
      }
    }
    int64_t now = NowNs();
    node->ctime_ns = existing->ctime_ns = now;
    old_parent->mtime_ns = old_parent->ctime_ns = now;
    new_parent->mtime_ns = new_parent->ctime_ns = now;
    uint32_t c1 = InotifyNextCookie(), c2 = InotifyNextCookie();
    InotifyDirEvent(old_parent, kInMovedFrom, old_base, c1, node_is_dir);
    InotifyDirEvent(new_parent, kInMovedTo, new_base, c1, node_is_dir);
    InotifyDirEvent(new_parent, kInMovedFrom, new_base, c2, ex_dir);
    InotifyDirEvent(old_parent, kInMovedTo, old_base, c2, ex_dir);
    return 0;
  }

  if (existing == node) return 0;  // same inode: POSIX says do nothing
  if (existing) {
    if (flags & kNoReplace) return kEEXIST;
    bool ex_dir = existing->type == VfsNodeType::kDirectory;
    if (node_is_dir && !ex_dir) return kENOTDIR;
    if (!node_is_dir && ex_dir) return kEISDIR;
    if (ex_dir && !existing->children.empty()) return kENOTEMPTY;
  }

  // Detach from old location.
  old_parent->children.erase(it);
  if (node_is_dir && old_parent->nlink > 2) old_parent->nlink--;

  bool replaced_last = false;
  if (existing) {
    if (existing->type == VfsNodeType::kSocket) SocketPathUnlinked(existing);
    replaced_last = RemoveChild(new_parent, new_base, existing);
  }

  new_parent->children[new_base] = node;
  if (node_is_dir) new_parent->nlink++;
  if (node->parent == old_parent && node->name == old_base) {
    node->parent = new_parent;
    node->name = new_base;
  } else if (node_is_dir) {
    node->parent = new_parent;
    node->name = new_base;
  }
  int64_t now = NowNs();
  node->ctime_ns = now;
  old_parent->mtime_ns = old_parent->ctime_ns = now;
  new_parent->mtime_ns = new_parent->ctime_ns = now;

  uint32_t cookie = InotifyNextCookie();
  InotifyDirEvent(old_parent, kInMovedFrom, old_base, cookie, node_is_dir);
  InotifyDirEvent(new_parent, kInMovedTo, new_base, cookie, node_is_dir);
  InotifySelfEvent(node, kInMoveSelf);
  if (existing) {
    InotifySelfEvent(existing, replaced_last ? kInDeleteSelf : kInAttrib);
  }
  return 0;
}

void Vfs::Notify(VfsNode* node, uint32_t mask) {
  if (!node) return;
  InotifySelfEvent(node, mask);
  VfsNode* parent = node->parent;
  if (parent && parent != node && node->nlink > 0) {
    auto it = parent->children.find(node->name);
    if (it != parent->children.end() && it->second == node) {
      InotifyDirEvent(parent, mask, node->name, 0,
                      node->type == VfsNodeType::kDirectory);
    }
  }
}

void Vfs::TouchModify(VfsNode* node) {
  if (!node) return;
  node->mtime_ns = node->ctime_ns = NowNs();
  Notify(node, kInModify);
}

void Vfs::TouchChange(VfsNode* node) {
  if (!node) return;
  node->ctime_ns = NowNs();
  Notify(node, kInAttrib);
}

void Vfs::UsageStats(uint64_t* bytes, uint64_t* nodes) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  uint64_t b = 0, n = 0;
  std::set<VfsNode*> seen;
  std::vector<VfsNode*> stack{g_root};
  while (!stack.empty()) {
    VfsNode* cur = stack.back();
    stack.pop_back();
    if (!seen.insert(cur).second) continue;
    n++;
    b += cur->data.size() + cur->symlink_target.size();
    for (const auto& kv : cur->children) stack.push_back(kv.second);
  }
  *bytes = b;
  *nodes = n;
}

void Vfs::EnsureSharedVmo(VfsNode* node, size_t min_size) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  if (!node) return;
  size_t needed = std::max(min_size, node->data.size());
  needed = (needed + 4095ULL) & ~4095ULL;
  if (needed == 0) needed = 4096;

  if (node->shared_vmo == ZX_HANDLE_INVALID) {
    zx_handle_t vmo = ZX_HANDLE_INVALID;
    if (zx_vmo_create(needed, ZX_VMO_RESIZABLE, &vmo) == ZX_OK) {
      if (!node->data.empty()) {
        zx_vmo_write(vmo, node->data.data(), 0, node->data.size());
      }
      node->shared_vmo = vmo;
      node->shared_vmo_size = needed;
      uintptr_t host_ptr = 0;
      if (zx_vmar_map(zx_vmar_root_self(),
                      ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0, vmo, 0, needed,
                      &host_ptr) == ZX_OK) {
        node->shared_host_ptr = host_ptr;
      }
    }
  } else if (needed > node->shared_vmo_size) {
    if (node->shared_host_ptr != 0) {
      zx_vmar_unmap(zx_vmar_root_self(), node->shared_host_ptr,
                    node->shared_vmo_size);
      node->shared_host_ptr = 0;
    }
    zx_vmo_set_size(node->shared_vmo, needed);
    node->shared_vmo_size = needed;
    uintptr_t host_ptr = 0;
    if (zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE,
                    0, node->shared_vmo, 0, needed, &host_ptr) == ZX_OK) {
      node->shared_host_ptr = host_ptr;
    }
  }
}

void Vfs::SyncNodeFromSharedVmo(VfsNode* node) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  if (!node || node->shared_vmo == ZX_HANDLE_INVALID) return;
  if (!node->data.empty()) {
    if (node->shared_host_ptr != 0 &&
        node->data.size() <= node->shared_vmo_size) {
      memcpy(node->data.data(),
             reinterpret_cast<const void*>(node->shared_host_ptr),
             node->data.size());
    } else {
      zx_vmo_read(node->shared_vmo, node->data.data(), 0, node->data.size());
    }
  }
}

void Vfs::SyncNodeToSharedVmo(VfsNode* node) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  if (!node || node->shared_vmo == ZX_HANDLE_INVALID) return;
  EnsureSharedVmo(node, node->data.size());
  if (!node->data.empty()) {
    if (node->shared_host_ptr != 0 &&
        node->data.size() <= node->shared_vmo_size) {
      memcpy(reinterpret_cast<void*>(node->shared_host_ptr),
             node->data.data(), node->data.size());
    } else {
      zx_vmo_write(node->shared_vmo, node->data.data(), 0, node->data.size());
    }
  }
}

size_t Vfs::LoadTarImage(const uint8_t* tar_data, size_t tar_len) {
  std::lock_guard<std::recursive_mutex> lock(g_vfs_mtx);
  size_t offset = 0;
  size_t files_loaded = 0;
  std::string next_long_name;
  std::string next_long_link;
  int64_t now = NowNs();

  while (offset + 512 <= tar_len) {
    const uint8_t* hdr = tar_data + offset;
    bool all_zero = true;
    for (int i = 0; i < 512; ++i) {
      if (hdr[i] != 0) {
        all_zero = false;
        break;
      }
    }
    if (all_zero) break;

    const char* h = reinterpret_cast<const char*>(hdr);
    uint64_t size = ParseOctal(h + 124, 12);
    uint32_t mode = static_cast<uint32_t>(ParseOctal(h + 100, 8)) & 07777;
    uint32_t uid = static_cast<uint32_t>(ParseOctal(h + 108, 8));
    uint32_t gid = static_cast<uint32_t>(ParseOctal(h + 116, 8));
    int64_t mtime_ns =
        static_cast<int64_t>(ParseOctal(h + 136, 12)) * 1000000000LL;
    char typeflag = h[156];

    const uint8_t* content = tar_data + offset + 512;
    size_t aligned_size = (size + 511ULL) & ~511ULL;
    if (offset + 512 + aligned_size > tar_len) break;

    if (typeflag == 'L' || typeflag == 'K') {
      size_t str_len = size;
      while (str_len > 0 && content[str_len - 1] == '\0') str_len--;
      std::string s(reinterpret_cast<const char*>(content), str_len);
      if (typeflag == 'L') next_long_name = s;
      else next_long_link = s;
      offset += 512 + aligned_size;
      continue;
    }
    if (typeflag == 'x' || typeflag == 'g') {
      // PAX extended header
      offset += 512 + aligned_size;
      continue;
    }

    std::string path;
    if (!next_long_name.empty()) {
      path = next_long_name;
      next_long_name.clear();
    } else {
      std::string name(h, strnlen(h, 100));
      std::string prefix(h + 345, strnlen(h + 345, 155));
      path = prefix.empty() ? name : (prefix + "/" + name);
    }

    std::string linkname;
    if (!next_long_link.empty()) {
      linkname = next_long_link;
      next_long_link.clear();
    } else {
      linkname.assign(h + 157, strnlen(h + 157, 100));
    }

    std::string norm = NormalizePath("/", path);
    offset += 512 + aligned_size;
    if (norm == "/") continue;
    auto parts = SplitComponents(norm);
    std::string dir_path = "/";
    for (size_t i = 0; i + 1 < parts.size(); ++i) dir_path += "/" + parts[i];
    VfsNode* parent = Mkdirs(dir_path, 0755);
    if (!parent || parent->type != VfsNodeType::kDirectory) continue;
    const std::string& base = parts.back();

    auto replace_existing = [&]() {
      auto it = parent->children.find(base);
      if (it != parent->children.end()) RemoveChild(parent, base, it->second);
    };

    VfsNode* n = nullptr;
    if (typeflag == '5') {
      auto it = parent->children.find(base);
      if (it != parent->children.end() &&
          it->second->type == VfsNodeType::kDirectory) {
        n = it->second;
        n->mode = kSIfDir | (mode ? mode : 0755);
      } else {
        replace_existing();
        n = NewNode(base, VfsNodeType::kDirectory, mode ? mode : 0755, now);
        InsertChild(parent, base, n, false);
      }
    } else if (typeflag == '2') {
      replace_existing();
      n = NewNode(base, VfsNodeType::kSymlink, 0777, now);
      n->symlink_target = linkname;
      InsertChild(parent, base, n, false);
      files_loaded++;
    } else if (typeflag == '1') {
      // Hard link: share the target node.
      VfsNode* target = nullptr;
      if (Resolve(NormalizePath("/", linkname), false, 0, &target) == 0 &&
          target->type != VfsNodeType::kDirectory) {
        replace_existing();
        target->nlink++;
        InsertChild(parent, base, target, false);
        files_loaded++;
      }
      continue;  // keep the target's own metadata
    } else if (typeflag == '3') {
      replace_existing();
      n = NewNode(base, VfsNodeType::kCharDevice, mode, now);
      uint64_t maj = ParseOctal(h + 329, 8), min = ParseOctal(h + 337, 8);
      n->rdev = (maj << 8) | (min & 0xff) | ((min & ~0xffULL) << 12);
      InsertChild(parent, base, n, false);
    } else if (typeflag == '0' || typeflag == '\0' || typeflag == '7') {
      replace_existing();
      n = NewNode(base, VfsNodeType::kRegular, mode, now);
      n->data.assign(content, content + size);
      InsertChild(parent, base, n, false);
      files_loaded++;
    } else {
      // FIFOs ('6') and block devices ('4') are not supported by the VFS.
      continue;
    }
    if (n) {
      n->uid = uid;
      n->gid = gid;
      n->mtime_ns = n->atime_ns = mtime_ns;
      n->ctime_ns = now;
    }
  }

  return files_loaded;
}

// ---------------------------------------------------------------------------
// /proc and the framebuffer part of /sys
// ---------------------------------------------------------------------------

uint64_t Vfs::ProcInode(const std::string& norm_path) {
  std::lock_guard<std::mutex> lock(g_proc_ino_mtx);
  auto it = g_proc_inos.find(norm_path);
  if (it != g_proc_inos.end()) return it->second;
  uint64_t ino = AllocInode();
  g_proc_inos[norm_path] = ino;
  return ino;
}

Vfs::ProcEntryType Vfs::ProcLookup(LinuxProcess* proc,
                                   const std::string& norm_path) {
  std::vector<std::string> parts;
  if (FbSysfsPath(norm_path, &parts)) {
    if (parts.size() < 4) return ProcEntryType::kDir;
    for (const char* f : kFbSysfsFiles) {
      if (parts[3] == f) return ProcEntryType::kFile;
    }
    return ProcEntryType::kNone;
  }
  if (!ParseProcPath(proc, norm_path, &parts)) return ProcEntryType::kNone;
  if (parts.empty()) return ProcEntryType::kDir;
  const std::string& p0 = parts[0];
  if (parts.size() == 1) {
    if (p0 == "self") return ProcEntryType::kSymlink;
    if (p0 == "zircon" || p0 == "net") return ProcEntryType::kDir;
    static const char* kFiles[] = {"version", "cpuinfo", "meminfo", "mounts",
                                   "uptime", "stat", "filesystems"};
    for (const char* f : kFiles) {
      if (p0 == f) return ProcEntryType::kFile;
    }
  }
  if (p0 == "net") {
    if (parts.size() == 2 && (parts[1] == "dev" || parts[1] == "route")) {
      return ProcEntryType::kFile;
    }
    return ProcEntryType::kNone;
  }
  if (p0 == "zircon") {
    if (parts.size() == 2 &&
        (parts[1] == "info" || parts[1] == "processes" || parts[1] == "vmos")) {
      return ProcEntryType::kFile;
    }
    return ProcEntryType::kNone;
  }
  if (!IsAllDigits(p0)) return ProcEntryType::kNone;
  auto target = FindProcess(atoi(p0.c_str()));
  if (!target) return ProcEntryType::kNone;
  if (parts.size() == 1) return ProcEntryType::kDir;
  const std::string& sub = parts[1];
  if (parts.size() == 2) {
    if (sub == "stat" || sub == "status" || sub == "cmdline" || sub == "comm" ||
        sub == "maps" || sub == "mounts") {
      return ProcEntryType::kFile;
    }
    if (sub == "exe" || sub == "cwd" || sub == "root") {
      return ProcEntryType::kSymlink;
    }
    if (sub == "fd") return ProcEntryType::kDir;
    return ProcEntryType::kNone;
  }
  if (parts.size() == 3 && sub == "fd" && IsAllDigits(parts[2])) {
    std::lock_guard<std::recursive_mutex> lock(target->fd_mtx);
    if (target->fds.count(atoi(parts[2].c_str()))) {
      return ProcEntryType::kSymlink;
    }
  }
  return ProcEntryType::kNone;
}

bool Vfs::IsDynamicProcDir(LinuxProcess* proc, const std::string& norm_path,
                           std::vector<ProcEntry>* out) {
  if (ProcLookup(proc, norm_path) != ProcEntryType::kDir) return false;
  if (!out) return true;
  out->clear();
  std::vector<std::string> parts;
  if (FbSysfsPath(norm_path, &parts)) {
    static const char* kSub[] = {"class", "graphics", "fb0"};
    if (parts.size() < 3) {
      out->push_back({kSub[parts.size()], ProcEntryType::kDir});
    } else {
      for (const char* f : kFbSysfsFiles) out->push_back({f, ProcEntryType::kFile});
    }
    return true;
  }
  ParseProcPath(proc, norm_path, &parts);
  if (parts.empty()) {
    *out = {{"self", ProcEntryType::kSymlink},
            {"zircon", ProcEntryType::kDir},
            {"net", ProcEntryType::kDir},
            {"version", ProcEntryType::kFile},
            {"cpuinfo", ProcEntryType::kFile},
            {"meminfo", ProcEntryType::kFile},
            {"mounts", ProcEntryType::kFile},
            {"uptime", ProcEntryType::kFile},
            {"stat", ProcEntryType::kFile},
            {"filesystems", ProcEntryType::kFile}};
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    for (const auto& kv : g_processes) {
      if (kv.second->reaped) continue;
      out->push_back({std::to_string(kv.first), ProcEntryType::kDir});
    }
    return true;
  }
  if (parts[0] == "net") {
    *out = {{"dev", ProcEntryType::kFile}, {"route", ProcEntryType::kFile}};
    return true;
  }
  if (parts[0] == "zircon") {
    *out = {{"info", ProcEntryType::kFile},
            {"processes", ProcEntryType::kFile},
            {"vmos", ProcEntryType::kFile}};
    return true;
  }
  if (parts.size() == 1) {
    *out = {{"stat", ProcEntryType::kFile},    {"status", ProcEntryType::kFile},
            {"cmdline", ProcEntryType::kFile}, {"comm", ProcEntryType::kFile},
            {"maps", ProcEntryType::kFile},    {"mounts", ProcEntryType::kFile},
            {"exe", ProcEntryType::kSymlink},  {"cwd", ProcEntryType::kSymlink},
            {"root", ProcEntryType::kSymlink}, {"fd", ProcEntryType::kDir}};
    return true;
  }
  // /proc/<pid>/fd
  auto target = FindProcess(atoi(parts[0].c_str()));
  if (!target) return false;
  std::lock_guard<std::recursive_mutex> lock(target->fd_mtx);
  for (const auto& kv : target->fds) {
    out->push_back({std::to_string(kv.first), ProcEntryType::kSymlink});
  }
  return true;
}

bool Vfs::ReadProcSymlink(LinuxProcess* proc, const std::string& norm_path,
                          std::string* out_target) {
  if (ProcLookup(proc, norm_path) != ProcEntryType::kSymlink) return false;
  std::vector<std::string> parts;
  ParseProcPath(proc, norm_path, &parts);
  if (parts.size() == 1) {  // /proc/self
    *out_target = std::to_string(proc->pid);
    return true;
  }
  auto target = FindProcess(atoi(parts[0].c_str()));
  if (!target) return false;
  if (parts[1] == "exe") {
    *out_target = target->exe_path;
  } else if (parts[1] == "cwd") {
    *out_target = target->cwd;
  } else if (parts[1] == "root") {
    *out_target = "/";
  } else {
    *out_target = FdLinkTarget(target.get(), atoi(parts[2].c_str()));
    if (out_target->empty()) return false;
  }
  return true;
}

bool Vfs::GenerateDynamicProcFile(LinuxProcess* proc,
                                  const std::string& norm_path,
                                  std::string* out_content) {
  if (ProcLookup(proc, norm_path) != ProcEntryType::kFile) return false;
  char buf[4096];
  std::vector<std::string> parts;

  if (FbSysfsPath(norm_path, &parts)) {
    const auto& fb = GetFbHardwareInfo();
    const std::string& f = parts[3];
    if (f == "name") {
      snprintf(buf, sizeof(buf), "bochs-vbe\n");
    } else if (f == "virtual_size") {
      snprintf(buf, sizeof(buf), "%u,%u\n", fb.width, fb.height);
    } else if (f == "bits_per_pixel") {
      snprintf(buf, sizeof(buf), "32\n");
    } else {  // stride
      snprintf(buf, sizeof(buf), "%u\n", fb.stride_bytes);
    }
    *out_content = buf;
    return true;
  }

  ParseProcPath(proc, norm_path, &parts);
  const std::string& p0 = parts[0];

  if (p0 == "net") {
    if (parts[1] == "dev") {
      *out_content = NetGenerateProcNetDev();
      return true;
    }
    if (parts[1] == "route") {
      *out_content = NetGenerateProcNetRoute();
      return true;
    }
    return false;
  }

  if (p0 == "zircon") {
    const std::string& f = parts[1];
    if (f == "info") {
      zx_info_kmem_stats_t kmem{};
      bool kmem_ok = GetKmemStats(&kmem);
      zx_info_handle_basic_t proc_basic{};
      zx_object_get_info(proc->zx_proc, ZX_INFO_HANDLE_BASIC, &proc_basic,
                         sizeof(proc_basic), nullptr, nullptr);
      snprintf(buf, sizeof(buf),
#if defined(__aarch64__)
               "Zircon Microkernel (arm64) + Linux Syscall Shim\n"
#else
               "Zircon Microkernel (x86_64) + Linux Syscall Shim\n"
#endif
               "zx_system_get_version_string: %s\n"
               "zx_system_get_num_cpus:       %u\n"
               "zx_system_get_physmem:        %llu MB\n"
               "current_process_koid:         %llu\n",
               zx_system_get_version_string().c_str, zx_system_get_num_cpus(),
               static_cast<unsigned long long>(zx_system_get_physmem() /
                                               (1024 * 1024)),
               static_cast<unsigned long long>(proc_basic.koid));
      std::string res = buf;
      if (kmem_ok) {
        snprintf(buf, sizeof(buf),
                 "kmem_total_bytes:             %llu\n"
                 "kmem_free_bytes:              %llu\n"
                 "kmem_vmo_bytes:               %llu\n"
                 "kmem_mmu_overhead_bytes:      %llu\n",
                 static_cast<unsigned long long>(kmem.total_bytes),
                 static_cast<unsigned long long>(kmem.free_bytes),
                 static_cast<unsigned long long>(kmem.vmo_bytes),
                 static_cast<unsigned long long>(kmem.mmu_overhead_bytes));
        res += buf;
      } else {
        res += "kmem_stats:                   unavailable (no info resource)\n";
      }
      *out_content = res;
      return true;
    }
    if (f == "processes") {
      std::string res =
          "PID   PPID  ZX_KOID     TASK_RUNTIME_NS   QUEUE_NS          COMM\n";
      std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
      for (const auto& kv : g_processes) {
        const auto& p = kv.second;
        zx_info_handle_basic_t basic{};
        zx_info_task_runtime_t rt{};
        if (p->zx_proc != ZX_HANDLE_INVALID) {
          zx_object_get_info(p->zx_proc, ZX_INFO_HANDLE_BASIC, &basic,
                             sizeof(basic), nullptr, nullptr);
          zx_object_get_info(p->zx_proc, ZX_INFO_TASK_RUNTIME, &rt, sizeof(rt),
                             nullptr, nullptr);
        }
        std::string tinfo;
        for (const auto& t : p->threads) {
          char tb[32];
          snprintf(tb, sizeof(tb), "%d:%llu%s ", t->tid,
                   static_cast<unsigned long long>(t->last_sc.load()),
                   t->in_syscall.load() ? "*" : "");
          tinfo += tb;
        }
        snprintf(buf, sizeof(buf),
                 "%-5d %-5d %-11llu %-17llu %-17llu %-16s [%s] %s\n", p->pid,
                 p->ppid, static_cast<unsigned long long>(basic.koid),
                 static_cast<unsigned long long>(rt.cpu_time),
                 static_cast<unsigned long long>(rt.queue_time),
                 p->comm.c_str(), tinfo.c_str(), p->last_path.c_str());
        res += buf;
      }
      *out_content = res;
      return true;
    }
    // vmos
    std::vector<zx_info_vmo_t> vmos(256);
    size_t actual = 0, avail = 0;
    zx_object_get_info(proc->zx_proc, ZX_INFO_PROCESS_VMOS, vmos.data(),
                       vmos.size() * sizeof(zx_info_vmo_t), &actual, &avail);
    std::string res = "VMO_KOID    SIZE_KB   COMMITTED_KB  NAME\n";
    for (size_t i = 0; i < actual; ++i) {
      snprintf(buf, sizeof(buf), "%-11llu %-9llu %-13llu %s\n",
               static_cast<unsigned long long>(vmos[i].koid),
               static_cast<unsigned long long>(vmos[i].size_bytes / 1024),
               static_cast<unsigned long long>(vmos[i].committed_bytes / 1024),
               vmos[i].name);
      res += buf;
    }
    *out_content = res;
    return true;
  }

  if (parts.size() == 1) {
    if (p0 == "version") {
      // Same identity the uname(2) emulation reports.
      snprintf(buf, sizeof(buf),
               "Linux version 6.6.0-zircon-shim (Fuchsia Zircon %s) #1 SMP "
               "Fuchsia Zircon Restricted Mode\n",
               zx_system_get_version_string().c_str);
      *out_content = buf;
      return true;
    }
    if (p0 == "cpuinfo") {
      *out_content = CpuInfoText();
      return true;
    }
    if (p0 == "meminfo") {
      zx_info_kmem_stats_t kmem{};
      if (!GetKmemStats(&kmem)) return false;
      uint64_t vfs_bytes = 0, vfs_nodes = 0;
      UsageStats(&vfs_bytes, &vfs_nodes);
      // There is no page cache, buffer cache or swap in this system, so those
      // are genuinely 0. Files of the in-memory filesystem are reported as
      // Shmem (as tmpfs pages are on Linux).
      snprintf(buf, sizeof(buf),
               "MemTotal:       %8llu kB\n"
               "MemFree:        %8llu kB\n"
               "MemAvailable:   %8llu kB\n"
               "Buffers:        %8llu kB\n"
               "Cached:         %8llu kB\n"
               "SwapCached:     %8llu kB\n"
               "Shmem:          %8llu kB\n"
               "SwapTotal:      %8llu kB\n"
               "SwapFree:       %8llu kB\n",
               static_cast<unsigned long long>(kmem.total_bytes / 1024),
               static_cast<unsigned long long>(kmem.free_bytes / 1024),
               static_cast<unsigned long long>(kmem.free_bytes / 1024), 0ULL,
               0ULL, 0ULL, static_cast<unsigned long long>(vfs_bytes / 1024),
               0ULL, 0ULL);
      *out_content = buf;
      return true;
    }
    if (p0 == "mounts") {
      // The shim has exactly two filesystems: the in-memory root filesystem
      // (memory-backed like tmpfs, which also holds /dev, /tmp and /sys
      // nodes; statfs reports TMPFS_MAGIC) and the synthesized procfs.
      // atime is never updated, hence noatime.
      *out_content =
          "tmpfs / tmpfs rw,noatime 0 0\n"
          "proc /proc proc rw,nosuid,nodev,noexec,noatime 0 0\n";
      return true;
    }
    if (p0 == "filesystems") {
      *out_content = "nodev\ttmpfs\nnodev\tproc\n";
      return true;
    }
    if (p0 == "uptime") {
      uint64_t ns = zx_clock_get_monotonic();
      uint64_t idle = 0;
      for (const auto& s : GetCpuStats()) idle += s.idle_time;
      snprintf(buf, sizeof(buf), "%llu.%02llu %llu.%02llu\n",
               static_cast<unsigned long long>(ns / 1000000000ULL),
               static_cast<unsigned long long>((ns / 10000000ULL) % 100),
               static_cast<unsigned long long>(idle / 1000000000ULL),
               static_cast<unsigned long long>((idle / 10000000ULL) % 100));
      *out_content = buf;
      return true;
    }
    if (p0 == "stat") {
      auto stats = GetCpuStats();
      if (stats.empty()) return false;
      uint64_t now = zx_clock_get_monotonic();
      // Zircon reports idle time per CPU but does not split busy time into
      // user/system: all non-idle time is reported in the "user" column.
      std::string res;
      uint64_t tot_busy = 0, tot_idle = 0, intr = 0, ctxt = 0;
      std::string per_cpu;
      for (const auto& s : stats) {
        uint64_t idle = static_cast<uint64_t>(s.idle_time);
        uint64_t busy = now > idle ? now - idle : 0;
        tot_busy += busy;
        tot_idle += idle;
        intr += s.ints + s.timer_ints + s.reschedule_ipis + s.generic_ipis;
        ctxt += s.context_switches;
        snprintf(buf, sizeof(buf), "cpu%u %llu 0 0 %llu 0 0 0 0 0 0\n",
                 s.cpu_number, static_cast<unsigned long long>(NsToTicks(busy)),
                 static_cast<unsigned long long>(NsToTicks(idle)));
        per_cpu += buf;
      }
      snprintf(buf, sizeof(buf), "cpu  %llu 0 0 %llu 0 0 0 0 0 0\n",
               static_cast<unsigned long long>(NsToTicks(tot_busy)),
               static_cast<unsigned long long>(NsToTicks(tot_idle)));
      res = buf;
      res += per_cpu;
      int64_t boot_sec =
          (NowNs() - static_cast<int64_t>(zx_clock_get_boot())) / 1000000000LL;
      snprintf(buf, sizeof(buf), "intr %llu\nctxt %llu\nbtime %lld\n",
               static_cast<unsigned long long>(intr),
               static_cast<unsigned long long>(ctxt),
               static_cast<long long>(boot_sec));
      res += buf;
      *out_content = res;
      return true;
    }
    return false;
  }

  // Per-process files.
  auto target = FindProcess(atoi(p0.c_str()));
  if (!target) return false;
  LinuxProcess* tp = target.get();
  const std::string& sub = parts[1];

  if (sub == "comm") {
    *out_content = MainComm(tp) + "\n";
    return true;
  }
  if (sub == "cmdline") {
    *out_content = tp->cmdline.empty() ? (tp->exe_path + '\0') : tp->cmdline;
    return true;
  }
  if (sub == "mounts") {
    return GenerateDynamicProcFile(proc, "/proc/mounts", out_content);
  }

  // Thread state: 'R' if any thread is executing Linux code (not inside a
  // syscall), 'S' if all are inside syscalls, 'Z' once exited.
  char state = 'S';
  size_t nthreads = 0;
  uint64_t blocked_mask = 0;
  {
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    nthreads = tp->threads.size();
    for (const auto& t : tp->threads) {
      if (!t->in_syscall.load()) state = 'R';
    }
    if (!tp->threads.empty()) blocked_mask = tp->threads.front()->sigmask;
  }
  if (tp->exited.load()) state = 'Z';
  uint64_t ignored = 0, caught = 0;
  {
    std::lock_guard<std::mutex> sig_lock(tp->sig_mtx);
    for (int s = 1; s <= 64; ++s) {
      if (tp->sigactions[s].sa_handler == 1) ignored |= 1ULL << (s - 1);
      else if (tp->sigactions[s].sa_handler > 1) caught |= 1ULL << (s - 1);
    }
  }
  uint64_t pending = tp->pending_signals.load();
  ProcMemInfo mem = GetProcMem(tp);

  if (sub == "status") {
    const char* state_name = state == 'R'   ? "R (running)"
                             : state == 'Z' ? "Z (zombie)"
                                            : "S (sleeping)";
    std::string res;
    snprintf(buf, sizeof(buf),
             "Name:\t%s\n"
             "Umask:\t%04o\n"
             "State:\t%s\n"
             "Tgid:\t%d\n"
             "Pid:\t%d\n"
             "PPid:\t%d\n"
             "TracerPid:\t0\n"
             "Uid:\t%u\t%u\t%u\t%u\n"
             "Gid:\t%u\t%u\t%u\t%u\n",
             MainComm(tp).c_str(), tp->umask_val, state_name, tp->pid,
             tp->pid, tp->ppid, tp->ruid, tp->euid, tp->suid, tp->fsuid,
             tp->rgid, tp->egid, tp->sgid, tp->fsgid);
    res = buf;
    res += "Groups:\t";
    for (uint32_t g : tp->groups) res += std::to_string(g) + " ";
    res += "\n";
    if (mem.ok) {
      snprintf(buf, sizeof(buf), "VmSize:\t%8llu kB\nVmRSS:\t%8llu kB\n",
               static_cast<unsigned long long>(mem.vsize / 1024),
               static_cast<unsigned long long>(mem.rss / 1024));
      res += buf;
    }
    snprintf(buf, sizeof(buf),
             "Threads:\t%zu\n"
             "SigPnd:\t%s\n"
             "ShdPnd:\t%s\n"
             "SigBlk:\t%s\n"
             "SigIgn:\t%s\n"
             "SigCgt:\t%s\n",
             nthreads, Hex16(0).c_str(), Hex16(pending).c_str(),
             Hex16(blocked_mask).c_str(), Hex16(ignored).c_str(),
             Hex16(caught).c_str());
    res += buf;
    *out_content = res;
    return true;
  }

  if (sub == "stat") {
    zx_info_task_runtime_t rt{};
    zx_info_process_t pinfo{};
    if (tp->zx_proc != ZX_HANDLE_INVALID) {
      zx_object_get_info(tp->zx_proc, ZX_INFO_TASK_RUNTIME, &rt, sizeof(rt),
                         nullptr, nullptr);
      zx_object_get_info(tp->zx_proc, ZX_INFO_PROCESS, &pinfo, sizeof(pinfo),
                         nullptr, nullptr);
    }
    uint64_t start_ticks = 0;
    if (pinfo.flags & ZX_INFO_PROCESS_FLAG_STARTED) {
      start_ticks = NsToTicks(static_cast<uint64_t>(pinfo.start_time));
    }
    uint64_t tty_rdev = 0;
    int tpgid = -1;
    int fg = 0;
    if (TtySessionInfo(tp->sid, &tty_rdev, &fg)) tpgid = fg > 0 ? fg : -1;
    // Linux encodes tty_nr as (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12).
    uint32_t tty_nr = static_cast<uint32_t>(tty_rdev);
    // Fields this shim does not track (fault counters, code/stack addresses,
    // wchan, scheduler policy, CPU last run on) are reported as 0, which is
    // also what Linux shows for them to unprivileged readers. Zircon does not
    // split CPU time into user/system, so all of it is reported as utime.
    snprintf(buf, sizeof(buf),
             "%d (%s) %c %d %d %d %u %d 0 0 0 0 0 %llu 0 0 0 %d %d %zu 0 %llu "
             "%llu %llu 18446744073709551615 0 0 0 0 0 %llu %llu %llu %llu 0 0 "
             "0 %d 0 0 0 0 0 0 0 0 0 0 0 0 0 %d\n",
             tp->pid, MainComm(tp).c_str(), state, tp->ppid, tp->pgid,
             tp->sid, tty_nr, tpgid,
             static_cast<unsigned long long>(NsToTicks(rt.cpu_time)),
             20 + tp->nice, tp->nice, nthreads,
             static_cast<unsigned long long>(start_ticks),
             static_cast<unsigned long long>(mem.vsize),
             static_cast<unsigned long long>(mem.rss / 4096),
             static_cast<unsigned long long>(pending),
             static_cast<unsigned long long>(blocked_mask),
             static_cast<unsigned long long>(ignored),
             static_cast<unsigned long long>(caught), tp->exit_signal,
             tp->exited.load() ? tp->exit_status : 0);
    *out_content = buf;
    return true;
  }

  if (sub == "maps") {
    auto maps = GetProcessMaps(tp);
    std::vector<SharedMapping> shared;
    {
      std::lock_guard<std::mutex> lock(tp->map_mtx);
      shared = tp->shared_mappings;
    }
    std::string res;
    for (const auto& m : maps) {
      uint32_t f = m.u.mapping.mmu_flags;
      bool is_shared = false;
      std::string name = m.name;
      for (const auto& sm : shared) {
        if (m.base >= sm.vaddr && m.base < sm.vaddr + sm.size) {
          is_shared = true;
          if (sm.backing_node) {
            std::lock_guard<std::recursive_mutex> vlock(g_vfs_mtx);
            name = NodePath(sm.backing_node);
          }
          break;
        }
      }
      snprintf(buf, sizeof(buf), "%08llx-%08llx %c%c%c%c %08llx 00:00 0",
               static_cast<unsigned long long>(m.base),
               static_cast<unsigned long long>(m.base + m.size),
               (f & ZX_VM_PERM_READ) ? 'r' : '-',
               (f & ZX_VM_PERM_WRITE) ? 'w' : '-',
               (f & ZX_VM_PERM_EXECUTE) ? 'x' : '-', is_shared ? 's' : 'p',
               static_cast<unsigned long long>(m.u.mapping.vmo_offset));
      res += buf;
      if (!name.empty()) res += std::string(" ") + name;
      res += "\n";
    }
    *out_content = res;
    return true;
  }
  return false;
}
