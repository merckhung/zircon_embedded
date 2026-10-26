"""Repository rule to fetch and cache Alpine Linux v3.20 APK packages for BusyBox & Xfce4."""

ALPINE_MIRROR = "https://dl-cdn.alpinelinux.org/alpine/v3.20"

def _alpine_repo_impl(rctx):
    arch = rctx.attr.arch
    rctx.download(
        url = ALPINE_MIRROR + "/main/" + arch + "/APKINDEX.tar.gz",
        output = "main_APKINDEX.tar.gz",
    )
    rctx.download(
        url = ALPINE_MIRROR + "/community/" + arch + "/APKINDEX.tar.gz",
        output = "community_APKINDEX.tar.gz",
    )

    rctx.file("resolve_apks.py", """\
import collections
import json
import tarfile

pkgs = {}
provides = {}

for repo in ["main", "community"]:
    with tarfile.open(f"{repo}_APKINDEX.tar.gz", "r:gz") as tf:
        data = tf.extractfile("APKINDEX").read().decode("utf-8", errors="ignore")
    for block in data.strip().split("\\n\\n"):
        info = {"repo": repo, "deps": [], "provides": []}
        for line in block.split("\\n"):
            if line.startswith("P:"):
                info["name"] = line[2:]
            elif line.startswith("V:"):
                info["ver"] = line[2:]
            elif line.startswith("D:"):
                info["deps"] = [d for d in line[2:].split() if not d.startswith("!")]
            elif line.startswith("p:"):
                info["provides"] = line[2:].split()
        if "name" in info:
            pkgs[info["name"]] = info
            provides[info["name"]] = info["name"]
            for p in info["provides"]:
                tok = p.split("=")[0].split("<")[0].split(">")[0]
                provides.setdefault(tok, info["name"])

def resolve(roots):
    visited = set()
    q = collections.deque(roots)
    while q:
        cur = q.popleft()
        tok = cur.split("=")[0].split("<")[0].split(">")[0].split("~")[0]
        real = provides.get(tok)
        if not real or real in visited:
            continue
        visited.add(real)
        for d in pkgs[real]["deps"]:
            q.append(d)
    out = []
    for p in sorted(visited):
        out.append({
            "name": p,
            "repo": pkgs[p]["repo"],
            "ver": pkgs[p]["ver"],
            "filename": f"{p}-{pkgs[p]['ver']}.apk",
        })
    return out

minimal = resolve(["musl", "busybox", "busybox-binsh"])
xfce4 = resolve([
    "musl",
    "busybox",
    "busybox-binsh",
    "xvfb",
    "xkbcomp",
    "xkeyboard-config",
    "font-misc-misc",
    "font-cursor-misc",
    "ttf-dejavu",
    "xfwm4",
    "xfdesktop",
    "xfce4-panel",
    "xfce4-terminal",
    "dbus",
    "hicolor-icon-theme",
    "adwaita-icon-theme",
    "adwaita-xfce-icon-theme",
    "xsetroot",
    "xrdb",
])

with open("resolved.json", "w") as f:
    json.dump({"minimal": minimal, "xfce4": xfce4}, f, indent=2)
""")

    res = rctx.execute(["python3", "resolve_apks.py"])
    if res.return_code != 0:
        fail("Failed to resolve Alpine APK dependencies: " + res.stderr)

    resolved = json.decode(rctx.read("resolved.json"))
    all_pkgs = {}
    for entry in resolved["minimal"] + resolved["xfce4"]:
        all_pkgs[entry["filename"]] = entry

    for fname, entry in all_pkgs.items():
        url = "{mirror}/{repo}/{arch}/{fname}".format(
            mirror = ALPINE_MIRROR,
            repo = entry["repo"],
            arch = arch,
            fname = fname,
        )
        rctx.download(
            url = url,
            output = "apks/" + fname,
        )

    min_list = ["\"apks/" + e["filename"] + "\"" for e in resolved["minimal"]]
    xfce_list = ["\"apks/" + e["filename"] + "\"" for e in resolved["xfce4"]]

    rctx.file("BUILD.bazel", """\
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "minimal_apks",
    srcs = [
        {min_srcs}
    ],
)

filegroup(
    name = "xfce4_apks",
    srcs = [
        {xfce_srcs}
    ],
)
""".format(
        min_srcs = ",\n        ".join(min_list),
        xfce_srcs = ",\n        ".join(xfce_list),
    ))

alpine_repo = repository_rule(
    implementation = _alpine_repo_impl,
    attrs = {
        "arch": attr.string(default = "x86_64", values = ["x86_64", "aarch64"]),
    },
)

def _alpine_ext_impl(mctx):
    alpine_repo(name = "alpine_x86_64", arch = "x86_64")
    alpine_repo(name = "alpine_aarch64", arch = "aarch64")

alpine_ext = module_extension(
    implementation = _alpine_ext_impl,
)
