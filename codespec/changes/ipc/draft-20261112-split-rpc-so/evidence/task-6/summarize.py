#!/usr/bin/env python3
"""TASK-6：拆分前后体积 / 依赖 / 导出证据采集与阈值判定。

用法（从任意目录）：
    python3 codespec/changes/ipc/draft-20261112-split-rpc-so/evidence/task-6/summarize.py

行为：
  1. 解压 evidence/task-6/ 下冻结的基线件与关闭态件到 /tmp/opencode/task6_work/；
  2. 重新采集 `stat` / `llvm-size -A` / `llvm-readelf -S -W` / `llvm-readelf -l -W` /
     `llvm-readelf -d` / `llvm-nm -D` 原始输出，写入 baseline.txt / after.txt /
     deps.txt / symbols.txt（全部覆盖写，可反复运行）；
  3. 打印四口径体积表（文件 / .text / ALLOC / LOAD MemSiz）与 AC-1.2 两项阈值判定；
  4. 两项阈值均达标时退出码 0，否则退出码 1。

口径定义（与 design.md / execution-plan.md 一致）：
  ALLOC      = `llvm-size -A` 的 Total 减去 `.gnu_debugdata`（该段不参与内存映射）
  LOAD MemSiz = `llvm-readelf -l` 中全部 LOAD 段的 MemSiz 之和（含页对齐）
  文件体积    = `stat -c '%s'` 的剥离后安装件大小
"""

import gzip
import hashlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# evidence/task-6 -> evidence -> draft-... -> ipc -> changes -> codespec -> <repo>
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "..", ".."))
CODE = os.path.abspath(os.path.join(REPO, "..", "..", ".."))
LLVM = os.environ.get(
    "LLVM_BIN",
    os.path.join(CODE, "prebuilts", "clang", "ohos", "linux-x86_64", "llvm", "bin"),
)
OUT = os.environ.get("OHOS_OUT", os.path.join(CODE, "out", "x86_64_virt"))
WORK = "/tmp/opencode/task6_work"

BASE_GZ = os.path.join(HERE, "baseline_libipc_single.z.so.gz")
OFF_GZ = os.path.join(HERE, "split_off_libipc_single.z.so.gz")

AFTER_SINGLE = os.path.join(OUT, "communication", "ipc", "libipc_single.z.so")
AFTER_RPC = os.path.join(OUT, "communication", "ipc", "libipc_rpc.z.so")
AFTER_DBINDER = os.path.join(OUT, "communication", "ipc", "libdbinder.z.so")
AFTER_TEST_SO = os.path.join(OUT, "communication", "ipc", "libipc_single_test.z.so")


def die(msg):
    sys.stderr.write("FATAL: %s\n" % msg)
    sys.exit(2)


def tool(name):
    path = os.path.join(LLVM, name)
    if not os.path.isfile(path):
        die("missing llvm tool: %s" % path)
    return path


def run(argv):
    proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return proc.returncode, proc.stdout.decode("utf-8", "replace")


def need(path):
    if not os.path.isfile(path):
        die("missing artifact: %s" % path)
    return path


def gunzip(src, dst):
    need(src)
    os.makedirs(WORK, exist_ok=True)
    with gzip.open(src, "rb") as fin, open(dst, "wb") as fout:
        shutil.copyfileobj(fin, fout)
    return dst


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def size_a(path):
    rc, out = run([tool("llvm-size"), "-A", path])
    if rc != 0:
        die("llvm-size failed on %s" % path)
    sections = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[0].startswith("."):
            try:
                sections[parts[0]] = int(parts[1])
            except ValueError:
                pass
        elif len(parts) == 2 and parts[0] == "Total":
            sections["Total"] = int(parts[1])
    return sections


def metrics(path):
    sec = size_a(path)
    total = sec.get("Total", 0)
    alloc = total - sec.get(".gnu_debugdata", 0)
    rc, out = run([tool("llvm-readelf"), "-l", "-W", path])
    if rc != 0:
        die("llvm-readelf -l failed on %s" % path)
    memsiz = 0
    for line in out.splitlines():
        fields = line.split()
        if len(fields) >= 6 and fields[0] == "LOAD":
            try:
                memsiz += int(fields[5], 0)
            except ValueError:
                pass
    return {
        "file": os.path.getsize(path),
        "text": sec.get(".text", 0),
        "alloc": alloc,
        "memsiz": memsiz,
        "md5": md5(path),
        "sections": sec,
    }


def dump(path, title, extra_note):
    """生成单文件原始测量块。"""
    lines = [title]
    if extra_note:
        lines.append("# " + extra_note)
    lines.append("# file: %s" % path)
    lines.append("")
    lines.append("$ stat -c '%%s %%y'")
    rc, out = run(["stat", "-c", "%s %y", path])
    lines.append(out.strip())
    for name, argv in (
        ("llvm-size -A", [tool("llvm-size"), "-A", path]),
        ("llvm-readelf -S -W", [tool("llvm-readelf"), "-S", "-W", path]),
        ("llvm-readelf -l -W", [tool("llvm-readelf"), "-l", "-W", path]),
        ("llvm-readelf -d", [tool("llvm-readelf"), "-d", path]),
    ):
        lines.append("")
        lines.append("$ " + name)
        rc, out = run(argv)
        lines.append(out.rstrip())
    lines.append("")
    return "\n".join(lines)


def needed(path):
    rc, out = run([tool("llvm-readelf"), "-d", path])
    return [ln.split("[")[1].rstrip("]") for ln in out.splitlines() if "(NEEDED)" in ln]


def defined_exports(path):
    rc, out = run([tool("llvm-nm"), "-D", "--defined-only", path])
    return [ln.strip() for ln in out.splitlines() if ln.strip()]


def refs(path, pattern):
    rc, out = run([tool("llvm-nm"), "-D", path])
    return [ln.strip() for ln in out.splitlines() if pattern in ln]


def fmt(n):
    return "{:,}".format(n)


def pct(before, after):
    if before == 0:
        return 0.0
    return (before - after) * 100.0 / before


def main():
    baseline = gunzip(BASE_GZ, os.path.join(WORK, "baseline_libipc_single.z.so"))
    off = gunzip(OFF_GZ, os.path.join(WORK, "split_off_libipc_single.z.so"))
    for p in (AFTER_SINGLE, AFTER_RPC, AFTER_DBINDER, AFTER_TEST_SO):
        need(p)

    subjects = [
        ("baseline", baseline),
        ("on-single", AFTER_SINGLE),
        ("on-rpc", AFTER_RPC),
        ("off-single", off),
        ("on-test-so", AFTER_TEST_SO),
        ("on-dbinder", AFTER_DBINDER),
    ]
    m = {name: metrics(path) for name, path in subjects}

    # ---------- baseline.txt ----------
    with open(os.path.join(HERE, "baseline.txt"), "w") as f:
        f.write("# TASK-6 拆分前基线原始测量（冻结件，可复验）\n")
        f.write("# 冻结来源: out/x86_64_virt/packages/phone/system/lib64/platformsdk/"
                "libipc_single.z.so\n")
        f.write("# 证据: evidence/task-6/baseline_libipc_single.z.so.gz"
                "（md5 %s）\n" % m["baseline"]["md5"])
        f.write("# 说明: 该副本为 pre-change 安装件（mtime 11-12 01:29），"
                "导出 167 条、含 DBinderDatabusInvoker、无 IpcRpcGetOps，"
                "与 HEAD 构建形态一致；stripped 不影响段级测量。\n\n")
        f.write(dump(baseline, "== baseline: libipc_single.z.so ==", None))

    # ---------- after.txt ----------
    with open(os.path.join(HERE, "after.txt"), "w") as f:
        f.write("# TASK-6 拆分后原始测量（当前 out/x86_64_virt 产物）\n")
        f.write("# 构建: ./build.sh --product-name x86_64_virt --ccache "
                "--deps-guard=false --build-target foundation/communication/ipc:ipc_components\n")
        f.write("# 开关: ipc_feature_rpc_split_enabled = true（默认）\n")
        f.write("# 时间: 2026-11-13（U-1 + TASK-5 装配后）\n\n")
        f.write(dump(AFTER_SINGLE, "== after: libipc_single.z.so ==",
                     "核心库：不含 RPC 实现"))
        f.write("\n")
        f.write(dump(AFTER_RPC, "== after: libipc_rpc.z.so ==",
                     "RPC 库：含 entry TU 与 4 个迁出 TU"))
        f.write("\n")
        f.write(dump(off, "== off-state: libipc_single.z.so ==",
                     "ipc_feature_rpc_split_enabled=false 冻结件"
                     "（TASK-4，evidence/task-6/split_off_libipc_single.z.so.gz）"))
        f.write("\n")
        f.write(dump(AFTER_TEST_SO, "== after: libipc_single_test.z.so ==",
                     "测试 so：静态形态（IPC_RPC_STATIC_LINK），仅构建期参考"))

    # ---------- deps.txt ----------
    with open(os.path.join(HERE, "deps.txt"), "w") as f:
        f.write("# TASK-6 依赖方向原始证据\n\n")
        for name, path in subjects:
            f.write("## %s (%s)\n" % (name, os.path.basename(path)))
            for lib in needed(path):
                f.write("NEEDED  %s\n" % lib)
            f.write("\n")
        f.write("## 反向依赖判定\n")
        rev = "libipc_rpc.z.so" in needed(AFTER_SINGLE)
        f.write("libipc_single NEEDED libipc_rpc.z.so: %s\n" % ("YES" if rev else "NO"))
        f.write("libipc_rpc NEEDED libipc_single.z.so: %s\n"
                % ("YES" if "libipc_single.z.so" in needed(AFTER_RPC) else "NO"))
        f.write("libdbinder NEEDED libipc_rpc.z.so: %s\n"
                % ("YES" if "libipc_rpc.z.so" in needed(AFTER_DBINDER) else "NO"))

    # ---------- symbols.txt ----------
    single_exports = defined_exports(AFTER_SINGLE)
    rpc_exports = defined_exports(AFTER_RPC)
    base_exports = defined_exports(baseline)
    off_exports = defined_exports(off)
    with open(os.path.join(HERE, "symbols.txt"), "w") as f:
        f.write("# TASK-6 导出面与加载点原始证据\n\n")
        f.write("## 导出计数（llvm-nm -D --defined-only | wc -l）\n")
        f.write("baseline(single)          %d\n" % len(base_exports))
        f.write("on-single                 %d\n" % len(single_exports))
        f.write("on-rpc                    %d\n" % len(rpc_exports))
        f.write("off-single                %d\n" % len(off_exports))
        f.write("\n## libipc_rpc.z.so 导出清单\n")
        f.write("\n".join(rpc_exports) + "\n")
        f.write("\n## 入口符号可见性\n")
        for name, path in (("on-rpc", AFTER_RPC), ("on-single", AFTER_SINGLE),
                           ("off-single", off), ("baseline", baseline)):
            hits = refs(path, "IpcRpc")
            f.write("%-10s IpcRpc* 动态符号 %d 条\n" % (name, len(hits)))
            for ln in hits:
                f.write("    %s\n" % ln)
        f.write("\n## 核心库是否仍引用 IpcRpc（符号 + 重定位）\n")
        for name, path in (("on-single", AFTER_SINGLE), ("off-single", off),
                           ("baseline", baseline)):
            n = len(refs(path, "IpcRpc"))
            rc, out = run([tool("llvm-readelf"), "-r", path])
            r = len([ln for ln in out.splitlines() if "IpcRpc" in ln])
            f.write("%-10s nm -D 命中 %d / readelf -r 命中 %d\n" % (name, n, r))
        f.write("\n## 核心库 dlopen/dlsym 引用点\n")
        for name, path in (("on-single", AFTER_SINGLE), ("off-single", off),
                           ("baseline", baseline)):
            rc, out = run([tool("llvm-nm"), "-D", path])
            hits = [ln.strip() for ln in out.splitlines()
                    if "dlopen" in ln or "dlsym" in ln]
            f.write("%-10s %d 条\n" % (name, len(hits)))
            for ln in hits:
                f.write("    %s\n" % ln)
        f.write("\n## 迁出 TU 的符号归属（定义侧，dynsym）\n")
        for name, path in (("on-single", AFTER_SINGLE), ("on-rpc", AFTER_RPC)):
            for pat in ("DBinderDatabusInvoker", "DBinderSoftbusClient",
                        "DatabusSocketListener", "DBinderSessionObject",
                        "DBinderCallbackStub"):
                rc, out = run([tool("llvm-nm"), "-D", "--defined-only", path])
                hits = [ln for ln in out.splitlines() if pat in ln]
                f.write("%-10s %-26s %d\n" % (name, pat, len(hits)))
        f.write("\n## 符号归属（未剥离件 --defined-only，含 internal）\n")
        unstripped = os.path.join(OUT, "lib.unstripped", "communication", "ipc")
        for name in ("libipc_single.z.so", "libipc_rpc.z.so"):
            p = os.path.join(unstripped, name)
            if not os.path.isfile(p):
                f.write("%-28s MISSING (skip)\n" % name)
                continue
            rc, out = run([tool("llvm-nm"), "--defined-only", p])
            for pat in ("DBinderDatabusInvoker", "DBinderSoftbusClient",
                        "DatabusSocketListener", "DBinderSessionObject",
                        "DBinderCallbackStub", "IpcRpcGetOps"):
                hits = [ln for ln in out.splitlines() if pat in ln]
                f.write("%-28s %-26s %d\n" % (name, pat, len(hits)))

    # ---------- 打印四口径表 ----------
    print("TASK-6 four-metric table\n")
    hdr = "%-10s %14s %14s %14s %14s" % ("subject", "file(B)", ".text(B)", "ALLOC(B)",
                                         "MemSiz(B)")
    print(hdr)
    print("-" * len(hdr))
    for name, _ in subjects:
        v = m[name]
        print("%-10s %14s %14s %14s %14s"
              % (name, fmt(v["file"]), fmt(v["text"]), fmt(v["alloc"]), fmt(v["memsiz"])))
    print("")
    print("baseline md5   %s" % m["baseline"]["md5"])
    print("on-single md5  %s" % m["on-single"]["md5"])
    print("on-rpc md5     %s" % m["on-rpc"]["md5"])
    print("off-single md5 %s" % m["off-single"]["md5"])
    print("")

    file_drop = pct(m["baseline"]["file"], m["on-single"]["file"])
    text_drop = pct(m["baseline"]["text"], m["on-single"]["text"])
    print("AC-1.2 file size drop   (baseline -> on-single): %+.2f%%  threshold >= 10%%  %s"
          % (file_drop, "PASS" if file_drop >= 10.0 else "FAIL"))
    print("AC-1.2 .text drop       (baseline -> on-single): %+.2f%%  threshold >= 20%%  %s"
          % (text_drop, "PASS" if text_drop >= 20.0 else "FAIL"))
    print("")
    print("supplementary (IPC-only process): ALLOC %s -> %s (%+.2f%%), MemSiz %s -> %s"
          % (fmt(m["baseline"]["alloc"]), fmt(m["on-single"]["alloc"]),
             pct(m["baseline"]["alloc"], m["on-single"]["alloc"]),
             fmt(m["baseline"]["memsiz"]), fmt(m["on-single"]["memsiz"])))
    print("supplementary (RPC process):      ALLOC %s -> %s + %s = %s (%+.2f%%)"
          % (fmt(m["baseline"]["alloc"]), fmt(m["on-single"]["alloc"]),
             fmt(m["on-rpc"]["alloc"]),
             fmt(m["on-single"]["alloc"] + m["on-rpc"]["alloc"]),
             -pct(m["baseline"]["alloc"],
                  m["on-single"]["alloc"] + m["on-rpc"]["alloc"])))
    print("supplementary (off vs baseline):  file %+d, .text %+d, ALLOC %+d"
          % (m["off-single"]["file"] - m["baseline"]["file"],
             m["off-single"]["text"] - m["baseline"]["text"],
             m["off-single"]["alloc"] - m["baseline"]["alloc"]))
    print("")
    print("wrote: baseline.txt after.txt deps.txt symbols.txt")

    ok = file_drop >= 10.0 and text_drop >= 20.0
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())