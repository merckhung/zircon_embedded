#ifndef ZIRCON_EMBEDDED_SRC_SHIM_VFS_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_VFS_H_

#include "src/shim/shim_common.h"

enum class VfsNodeType {
  kRegular,
  kDirectory,
  kSymlink,
  kCharDevice,
  kSocket,  // AF_UNIX socket bound to a filesystem path
};

struct VfsNode {
  // Name and parent of the link through which the node was created (for
  // hard-linked regular files this is one of possibly several links).
  std::string name;
  VfsNodeType type = VfsNodeType::kRegular;
  uint32_t mode = 0100644;  // includes S_IFMT bits
  uint32_t uid = 0;
  uint32_t gid = 0;
  uint64_t ino = 1;
  uint64_t rdev = 0;
  uint32_t nlink = 1;  // directories: 2 + number of subdirectories
  // Wall-clock timestamps (ns since the Unix epoch), see Vfs::NowNs().
  int64_t atime_ns = 0;
  int64_t mtime_ns = 0;
  int64_t ctime_ns = 0;

  VfsNode* parent = nullptr;
  std::map<std::string, VfsNode*> children;

  std::vector<uint8_t> data;
  std::string symlink_target;

  // Shared Zircon VMO for MAP_SHARED file mappings (e.g. /tmp/Xvfb_screen0)
  zx_handle_t shared_vmo = ZX_HANDLE_INVALID;
  size_t shared_vmo_size = 0;
  uintptr_t shared_host_ptr = 0;
};

class Vfs {
 public:
  static void Init();
  static std::recursive_mutex& Mutex();
  static VfsNode* Root();

  // Current wall-clock time in ns since the Unix epoch, read from the
  // process-wide UTC reference clock (the same clock the shim's
  // CLOCK_REALTIME is derived from).
  static int64_t NowNs();
  static uint64_t AllocInode();

  static std::string NormalizePath(const std::string& cwd,
                                   const std::string& raw_path);
  static std::string NodePath(VfsNode* node);

  static VfsNode* Lookup(const std::string& cwd, const std::string& path,
                         bool follow_final_symlink = true,
                         int symlink_depth = 0);
  // Like Lookup() but reports the Linux error (-ENOENT, -ENOTDIR, -ELOOP).
  static int64_t LookupErr(const std::string& abs_path, bool follow_final,
                           VfsNode** out);

  static VfsNode* LookupParent(const std::string& cwd, const std::string& path,
                               std::string* out_basename);

  // Error-reporting creation primitives (absolute, normalized paths). They
  // never follow a symlink in the final component and fail with -EEXIST if
  // the name exists, -ENOENT if the parent is missing, -ENOTDIR if a parent
  // component is not a directory.
  static int64_t MakeDir(const std::string& abs_path, uint32_t mode,
                         VfsNode** out);
  static int64_t MakeNode(const std::string& abs_path, VfsNodeType type,
                          uint32_t perm, uint64_t rdev, VfsNode** out);
  static int64_t MakeSymlink(const std::string& abs_path,
                             const std::string& target);
  // Hard link `new_abs` -> existing node at `old_abs`.
  static int64_t Link(const std::string& old_abs, const std::string& new_abs,
                      bool follow_old);

  // Legacy helpers (create intermediate directories / return existing nodes).
  static VfsNode* Mkdirs(const std::string& path, uint32_t mode = 0755);
  static VfsNode* CreateFile(const std::string& cwd, const std::string& path,
                             uint32_t mode = 0644);
  static VfsNode* CreateSymlink(const std::string& cwd, const std::string& path,
                                const std::string& target);
  static VfsNode* CreateDevice(const std::string& path, uint32_t mode,
                               uint64_t rdev);
  static int64_t Unlink(const std::string& cwd, const std::string& path,
                        bool rmdir_only);
  static int64_t Rename(const std::string& cwd, const std::string& oldpath,
                        const std::string& newpath, uint32_t flags = 0);

  // Timestamp / attribute helpers (caller holds Mutex()).
  static void TouchModify(VfsNode* node);  // data changed: mtime+ctime, IN_MODIFY
  static void TouchChange(VfsNode* node);  // metadata changed: ctime, IN_ATTRIB
  // Deliver an inotify event about `node` to watches on the node itself and
  // on its parent directory (with the entry name).
  static void Notify(VfsNode* node, uint32_t mask);

  // Statistics for statfs(): bytes of file data held and number of nodes.
  static void UsageStats(uint64_t* bytes, uint64_t* nodes);

  static void EnsureSharedVmo(VfsNode* node, size_t min_size);
  static void SyncNodeFromSharedVmo(VfsNode* node);
  static void SyncNodeToSharedVmo(VfsNode* node);

  static size_t LoadTarImage(const uint8_t* tar_data, size_t tar_len);

  // Synthesized /proc and /sys content.
  enum class ProcEntryType { kNone, kFile, kDir, kSymlink };
  struct ProcEntry {
    std::string name;
    ProcEntryType type;
  };
  // Classifies a normalized path inside /proc (or a synthesized /sys file).
  // `proc` is the caller (for "self").
  static ProcEntryType ProcLookup(LinuxProcess* proc,
                                  const std::string& norm_path);
  static bool GenerateDynamicProcFile(LinuxProcess* proc,
                                      const std::string& norm_path,
                                      std::string* out_content);
  static bool IsDynamicProcDir(LinuxProcess* proc, const std::string& norm_path,
                               std::vector<ProcEntry>* out_entries);
  static bool ReadProcSymlink(LinuxProcess* proc, const std::string& norm_path,
                              std::string* out_target);
  // Stable unique inode number for a synthesized path.
  static uint64_t ProcInode(const std::string& norm_path);
};

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_VFS_H_
