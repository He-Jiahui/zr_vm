#!/usr/bin/env python3
"""Preview and apply an audited repository source path migration.

The migration table is TSV with exactly two columns: old repository path and
new repository path.  Applying uses filesystem renames instead of ``git mv``
so the user's existing index (including staged changes) is never rewritten.
"""

from __future__ import annotations

import argparse
from functools import lru_cache
import hashlib
import importlib.util
import json
import os
import re
import stat
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable


VERSION = 1
# @unit scripts/layout/apply_moves.py::EXCLUDED_PREFIXES [state]
#
# ## Summary
# 固定不遍历或不改写的顶层历史/依赖/迁移工具前缀。
#
# ## Constraints and lifecycle
# 只是一部分策略；excluded 另检查任意深度docs/acceptance与组件，iter_files 另检查reparse。元组名称不构成目录授权或完整排除图。
#
# ## References
# - scripts/layout/apply_moves.py:42
# - scripts/layout/apply_moves.py:43
# - scripts/layout/apply_moves.py:44
# - scripts/layout/apply_moves.py:435
# - scripts/layout/apply_moves.py:442
# - scripts/layout/apply_moves.py:505
EXCLUDED_PREFIXES = (
    "lua/", "tests/third_party/", "third_party/", ".git/", ".codex/",
    "tests/acceptance/", "scripts/layout/",
)
# @unit scripts/layout/apply_moves.py::TEXT_SUFFIXES [state]
#
# ## Summary
# 给普通文本改写扫描限定可处理的扩展名集合。
#
# ## Constraints and lifecycle
# 扫描直接处理 bytes，不要求内容全为UTF-8；后缀不证明文件编码或来源。docs 虽列md等仍可由排除策略跳过。运行期间不得修改该共享集合。
#
# ## References
# - scripts/layout/apply_moves.py:61
# - scripts/layout/apply_moves.py:62
# - scripts/layout/apply_moves.py:63
# - scripts/layout/apply_moves.py:64
# - scripts/layout/apply_moves.py:469
# - scripts/layout/apply_moves.py:472
TEXT_SUFFIXES = {
    ".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".inc", ".def",
    ".cmake", ".gdb", ".py", ".ps1", ".sh", ".md", ".txt", ".json",
    ".yml", ".yaml", ".toml", ".rs", ".js", ".ts", ".html", ".bat",
}
# @unit scripts/layout/apply_moves.py::SCAN_PREFIXES [state]
#
# ## Summary
# 限制普通候选扫描在产品/tests/scripts/docs 名称域。
#
# ## Constraints and lifecycle
# docs 仍被更强排除策略拒绝；根 CMakeLists/README 为显式例外，不是全仓任意文件扫描许可。
#
# ## References
# - scripts/layout/apply_moves.py:79
# - scripts/layout/apply_moves.py:469
# - scripts/layout/apply_moves.py:470
# - scripts/layout/apply_moves.py:471
SCAN_PREFIXES = ("zr_vm_", "tests/", "scripts/", "docs/")


# @unit scripts/layout/apply_moves.py::_registered_task_roots [function]
#
# ## Summary
# 在模块导入时选择原子改写临时根和测试夹具根，供命令行与测试共用。
#
# ## Parameters
# 无参数。
#
# ## Returns
# 返回两个 Path；不显式创建任务目录，路径解析或标准库临时目录探测可能发生。
#
# ## Constraints and lifecycle
# 同时给出 TMP_ROOT/FIXTURE_ROOT 才采用两者；否则使用 TASK_ROOT 的历史布局，再按 Windows 固定 E 根或非 Windows tempfile 根回退。resolve 可访问文件系统并传播路径异常；这些名称本身不证明登记、权限、独占或无 reparse。导入后环境改变不会自动刷新全局快照。 非Windows回退调用 tempfile.gettempdir，其标准库目录探测可能创建并删除探测文件；运行者应同时给出专用两根以避免回退。
#
# ## References
# - scripts/layout/apply_moves.py:105
# - scripts/layout/apply_moves.py:107
# - scripts/layout/apply_moves.py:110
# - scripts/layout/apply_moves.py:112
# - scripts/layout/apply_moves.py:116
# - scripts/layout/apply_moves.py:119
# - scripts/layout/apply_moves.py:122
# - scripts/layout/apply_moves.py:143
def _registered_task_roots() -> tuple[Path, Path]:
    """Return the task's separately registered temporary and fixture roots."""
    tmp_override = os.environ.get("ZR_VM_LAYOUT_TMP_ROOT")
    fixture_override = os.environ.get("ZR_VM_LAYOUT_FIXTURE_ROOT")
    base_override = os.environ.get("ZR_VM_LAYOUT_TASK_ROOT")
    if tmp_override and fixture_override:
        return Path(tmp_override).resolve(), Path(fixture_override).resolve()
    if base_override:
        base = Path(base_override)
        # Accept the managed environment's cargo-targets/zr_vm/tmp root while
        # keeping task files under the explicitly registered task IDs.
        if base.name.lower() == "tmp":
            return (base / "layout-01a13033").resolve(), (base.parent / "tests/layout-01a13033").resolve()
        return (base / "tmp/layout-01a13033").resolve(), (base / "tests/layout-01a13033").resolve()
    if os.name == "nt":
        return (Path("E:/cargo-targets/zr_vm/tmp/layout-01a13033"),
                Path("E:/cargo-targets/zr_vm/tests/layout-01a13033"))
    portable = Path(tempfile.gettempdir()) / "zr-vm-layout-01a13033"
    return (portable / "tmp").resolve(), (portable / "tests").resolve()


# @unit scripts/layout/apply_moves.py::SAFE_TASK_TMP/REGISTERED_FIXTURE_ROOT [state]
#
# ## Summary
# 在导入时固定临时改写与测试夹具两个任务根。
#
# ## Constraints and lifecycle
# 名称来自环境/历史默认，不能自动证明当前登记或独占。main 限定 --root 到 fixture 子树、--tmp-dir 到临时子树；测试借同一 fixture 值。运行前须同时给定两个专用 E 根，模块加载后不会响应环境改变。
#
# ## References
# - scripts/layout/apply_moves.py:105
# - scripts/layout/apply_moves.py:107
# - scripts/layout/apply_moves.py:108
# - scripts/layout/apply_moves.py:110
# - scripts/layout/apply_moves.py:143
# - scripts/layout/apply_moves.py:2639
# - scripts/layout/apply_moves.py:2644
# - scripts/layout/apply_moves.py:2725
SAFE_TASK_TMP, REGISTERED_FIXTURE_ROOT = _registered_task_roots()
# @unit scripts/layout/apply_moves.py::INCLUDE_RE [state]
#
# ## Summary
# 在原始 bytes 中捕获 quoted include 的 head/name/tail，保持未改片段原字节。
#
# ## Constraints and lifecycle
# 不是C词法/预处理器，不限定行首或活动条件，不能看到宏生成和尖括号include。name 不含引号/CR/LF，实际解析与读取范围由各扫描器进一步限制。
#
# ## References
# - scripts/layout/apply_moves.py:161
# - scripts/layout/apply_moves.py:1575
# - scripts/layout/apply_moves.py:1576
# - scripts/layout/apply_moves.py:1591
# - scripts/layout/apply_moves.py:1597
# - scripts/layout/apply_moves.py:2132
# - scripts/layout/apply_moves.py:2394
# - scripts/layout/apply_moves.py:2567
INCLUDE_RE = re.compile(rb'(?P<head>\#\s*include\s*")(?P<name>[^"\r\n]+)(?P<tail>")')
LEFT_PATH_CHAR = rb"A-Za-z0-9_.-"
RIGHT_PATH_CHAR = rb"A-Za-z0-9_.\\/-"


# @unit scripts/layout/apply_moves.py::MigrationError [class]
#
# ## Summary
# 区分工具主动拒绝的不安全、陈旧或不一致迁移输入。
#
# ## Fields and invariants
# 继承 Exception，不自动回滚、关闭资源或改写状态。main 仅捕获此类并返回2；其它异常可能传播。
#
# ## Constraints and lifecycle
# 类型本身不授予路径、文件树或夹具的所有权；构造和使用受以上约束。
#
# ## References
# - scripts/layout/apply_moves.py:183
# - scripts/layout/apply_moves.py:184
# - scripts/layout/apply_moves.py:338
# - scripts/layout/apply_moves.py:2039
# - scripts/layout/apply_moves.py:2745
class MigrationError(Exception):
    """An unsafe, stale, or inconsistent migration input."""


# @unit scripts/layout/apply_moves.py::layout_helper [function]
#
# ## Summary
# 按有限白名单延迟装载相邻检查器，保持调用者 sys.path 不变。
#
# ## Parameters
# - name: 只接受 dormant_contexts 或 generated_includes；其余值抛 MigrationError。
#
# ## Returns
# 返回按名称缓存的模块对象；不返回检查结果。
#
# ## Constraints and lifecycle
# sys.modules 在 exec_module 前发布以支持 dataclass 等模块协议。加载失败不恢复旧绑定；异常不会进入成功 lru_cache。最多缓存两个模块，各实例/全局可能跨调用存活，源文件在装载和使用期间须稳定；这里不建立沙箱或文件锁。
#
# ## References
# - scripts/layout/apply_moves.py:210
# - scripts/layout/apply_moves.py:213
# - scripts/layout/apply_moves.py:216
# - scripts/layout/apply_moves.py:219
# - scripts/layout/apply_moves.py:220
# - scripts/layout/apply_moves.py:221
# - scripts/layout/apply_moves.py:2141
# - scripts/layout/apply_moves.py:2668
@lru_cache(maxsize=2)
def layout_helper(name: str):
    """Load an adjacent checker without changing the caller's import path."""
    if name not in {"dormant_contexts", "generated_includes"}:
        raise MigrationError(f"unknown layout helper {name}")
    module_name = f"_zr_vm_layout_{name}"
    spec = importlib.util.spec_from_file_location(module_name, Path(__file__).with_name(name + ".py"))
    if spec is None or spec.loader is None:
        raise MigrationError(f"cannot load layout helper {name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


# @unit scripts/layout/apply_moves.py::Move [class]
#
# ## Summary
# 保存一个 basename 不变的旧/新逻辑路径对，作为迁移计划与改写规则的共同身份。
#
# ## Fields and invariants
# Fields: old 为源逻辑名，new 为目的逻辑名，均按字符串使用。frozen dataclass 阻止普通字段赋值，但构造器不验证路径；load_moves 才校验，测试也可直接构造。生成的 init/eq/hash 不是额外手写物理定义，不代表文件已移动或拥有该文件。
#
# ## Constraints and lifecycle
# 类型本身不授予路径、文件树或夹具的所有权；构造和使用受以上约束。
#
# ## References
# - scripts/layout/apply_moves.py:245
# - scripts/layout/apply_moves.py:246
# - scripts/layout/apply_moves.py:247
# - scripts/layout/apply_moves.py:248
# - scripts/layout/apply_moves.py:387
# - scripts/layout/apply_moves.py:398
# - scripts/layout/apply_moves.py:950
# - scripts/layout/apply_moves.py:1969
@dataclass(frozen=True)
class Move:
    old: str
    new: str


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# @unit scripts/layout/apply_moves.py::repo_path [function]
#
# ## Summary
# 把已经确定属于根目录的 Path 表达为 POSIX 风格相对名称。
#
# ## Parameters
# - path: 待相对化 Path；不在 root 内时 relative_to 抛 ValueError。
# - root: 与 path 使用同一词法/规范化约定的根 Path。
#
# ## Returns
# 返回相对路径字符串。
#
# ## Constraints and lifecycle
# 自身不 resolve、不检查存在性/reparse 或文件类型；调用方决定路径身份。
#
# ## References
# - scripts/layout/apply_moves.py:276
# - scripts/layout/apply_moves.py:277
# - scripts/layout/apply_moves.py:504
# - scripts/layout/apply_moves.py:1648
# - scripts/layout/apply_moves.py:2266
def repo_path(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


# @unit scripts/layout/apply_moves.py::has_reparse [function]
#
# ## Summary
# 在跟随路径前识别叶节点符号链接和 Windows reparse 标志。
#
# ## Parameters
# - path: 执行 lstat 的 Path。
#
# ## Returns
# 节点不存在时返回 False；符号链接或 reparse 位存在时返回 True，否则 False。
#
# ## Constraints and lifecycle
# 只检查该节点，不遍历祖先。只吞 FileNotFoundError；权限等其他 OSError 可传播。返回 False 不是可读、普通文件或持久安全的证明。
#
# ## References
# - scripts/layout/apply_moves.py:303
# - scripts/layout/apply_moves.py:305
# - scripts/layout/apply_moves.py:306
# - scripts/layout/apply_moves.py:309
# - scripts/layout/apply_moves.py:507
# - scripts/layout/apply_moves.py:1695
# - scripts/layout/apply_moves.py:1734
# - scripts/layout/apply_moves.py:2733
def has_reparse(path: Path) -> bool:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    attrs = getattr(info, "st_file_attributes", 0)
    return stat.S_ISLNK(info.st_mode) or bool(attrs & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


# @unit scripts/layout/apply_moves.py::safe_relative [function]
#
# ## Summary
# 在读取迁移表时规范化分隔符并拒绝明显逃逸的相对路径语法。
#
# ## Parameters
# - value: 候选 str；反斜杠转为斜杠。
# - label: 仅用于异常诊断的来源标签。
#
# ## Returns
# 返回 PurePosixPath 的相对字符串；不符合语法时抛 MigrationError。
#
# ## Constraints and lifecycle
# 拒绝空值、绝对/盘符路径和空、点、父目录组件；不访问文件系统，不认证存在性、文件名可创建性、Unicode 支持、保留名或 reparse。load_moves 另对字段 strip 和保护区作约束。
#
# ## References
# - scripts/layout/apply_moves.py:334
# - scripts/layout/apply_moves.py:335
# - scripts/layout/apply_moves.py:337
# - scripts/layout/apply_moves.py:339
# - scripts/layout/apply_moves.py:341
# - scripts/layout/apply_moves.py:387
def safe_relative(value: str, label: str) -> str:
    value = value.replace("\\", "/")
    p = PurePosixPath(value)
    if not value or p.is_absolute() or re.match(r"^[A-Za-z]:", value):
        raise MigrationError(f"{label} must be a nonempty repository-relative path: {value!r}")
    if any(part in ("", ".", "..") for part in value.split("/")):
        raise MigrationError(f"{label} contains an unsafe path component: {value!r}")
    return p.as_posix()


# @unit scripts/layout/apply_moves.py::load_moves [function]
#
# ## Summary
# 完整读取有限 TSV 映射，先拒绝不一致输入，再发布 Move 列表。
#
# ## Parameters
# - path: UTF-8-sig 文本表路径；文件应在读取期间稳定。
#
# ## Returns
# 返回按表顺序的新 Move 列表；格式/路径约束失败抛 MigrationError。
#
# ## Constraints and lifecycle
# 空行、注释和两列标题跳过；两侧 strip，保留 basename，拒绝保护树、同址、casefold 重复源/目的与链环。只把读取 OSError 包装为 MigrationError，Unicode/参数类型异常不一概包装。尚不验证源/目的实际状态，交给 preflight。
#
# ## References
# - scripts/layout/apply_moves.py:373
# - scripts/layout/apply_moves.py:376
# - scripts/layout/apply_moves.py:380
# - scripts/layout/apply_moves.py:383
# - scripts/layout/apply_moves.py:387
# - scripts/layout/apply_moves.py:388
# - scripts/layout/apply_moves.py:390
# - scripts/layout/apply_moves.py:394
# - scripts/layout/apply_moves.py:396
# - scripts/layout/apply_moves.py:399
# - scripts/layout/apply_moves.py:401
# - scripts/layout/apply_moves.py:403
# - scripts/layout/apply_moves.py:406
# - scripts/layout/apply_moves.py:2652
def load_moves(path: Path) -> list[Move]:
    rows: list[Move] = []
    try:
        lines = path.read_text(encoding="utf-8-sig").splitlines()
    except OSError as exc:
        raise MigrationError(f"cannot read move table {path}: {exc}") from exc
    for number, line in enumerate(lines, 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) == 2 and [c.strip().lower() for c in cols] == ["old_path", "new_path"]:
            continue
        if len(cols) != 2:
            raise MigrationError(f"{path}:{number}: expected old_path<TAB>new_path")
        old, new = (safe_relative(c.strip(), f"{path}:{number}") for c in cols)
        if old == new:
            raise MigrationError(f"{path}:{number}: source and destination are identical")
        if PurePosixPath(old).name != PurePosixPath(new).name:
            raise MigrationError(f"{path}:{number}: file names must be preserved")
        for side, value in (("source", old), ("destination", new)):
            parts = value.split("/")
            if any(part in {"third_party", "node_modules", ".git", ".codex", "lua"} for part in parts):
                raise MigrationError(f"{path}:{number}: {side} is inside a protected dependency tree")
            if value.startswith(("lua/", ".git/", ".codex/", "tests/acceptance/", "scripts/layout/")) or "docs" in parts:
                raise MigrationError(f"{path}:{number}: {side} is in a protected or migration-history path")
        rows.append(Move(old, new))
    olds = [m.old.casefold() for m in rows]
    news = [m.new.casefold() for m in rows]
    if len(olds) != len(set(olds)):
        raise MigrationError("move table contains duplicate source paths")
    if len(news) != len(set(news)):
        raise MigrationError("move table contains duplicate destination paths")
    old_set = set(olds)
    if any(n in old_set for n in news):
        raise MigrationError("move table chains or cycles paths; use a single final destination per row")
    return rows


# @unit scripts/layout/apply_moves.py::excluded [function]
#
# ## Summary
# 为扫描与外部 dormant 回调提供同一有限排除策略。
#
# ## Parameters
# - rel: 仓内逻辑相对名称；本函数只替换分隔符。
#
# ## Returns
# 返回当前策略是否排除此名称。
#
# ## Constraints and lifecycle
# 包括任意深度 docs、tests/acceptance、第三方/元数据目录、迁移脚本树与自身测试夹具。不是路径安全验证器；不 resolve 或查文件。作为回调进入相邻模块时仍使用同一全局策略。
#
# ## References
# - scripts/layout/apply_moves.py:435
# - scripts/layout/apply_moves.py:438
# - scripts/layout/apply_moves.py:439
# - scripts/layout/apply_moves.py:441
# - scripts/layout/apply_moves.py:442
# - scripts/layout/apply_moves.py:470
# - scripts/layout/apply_moves.py:2506
# - scripts/layout/apply_moves.py:2679
# - scripts/layout/apply_moves.py:2686
def excluded(rel: str) -> bool:
    normalized = rel.replace("\\", "/")
    parts = normalized.rstrip("/").split("/")
    documentation = "docs" in parts
    module_acceptance = any(parts[index:index + 2] == ["tests", "acceptance"]
                            for index in range(len(parts) - 1))
    return (normalized == "tests/tooling/test_layout_moves.py" or documentation or module_acceptance
            or normalized.startswith(EXCLUDED_PREFIXES) or any(
        part in {"third_party", "node_modules", ".git", ".codex", "lua"}
        for part in parts))


# @unit scripts/layout/apply_moves.py::is_scan_file [function]
#
# ## Summary
# 选择迁移文本候选，区别扫描策略与真实文件认证。
#
# ## Parameters
# - rel: 候选仓内逻辑名称。
# - path: 用于 suffix/name 或根例外 is_file 的 Path。
#
# ## Returns
# 返回是否属于允许扫描的后缀/名称域。
#
# ## Constraints and lifecycle
# 普通候选预期来自 iter_files；这里不对所有分支调用 is_file。根 CMakeLists/README 是显式例外；排除策略仍限制其它路径。
#
# ## References
# - scripts/layout/apply_moves.py:469
# - scripts/layout/apply_moves.py:470
# - scripts/layout/apply_moves.py:471
# - scripts/layout/apply_moves.py:472
# - scripts/layout/apply_moves.py:1649
# - scripts/layout/apply_moves.py:2073
def is_scan_file(rel: str, path: Path) -> bool:
    if excluded(rel) or not rel.startswith(SCAN_PREFIXES):
        return rel in {"CMakeLists.txt", "README.md"} and path.is_file()
    return path.suffix.lower() in TEXT_SUFFIXES or path.name in ("CMakeLists.txt",)


# @unit scripts/layout/apply_moves.py::iter_files [function]
#
# ## Summary
# 惰性枚举允许遍历的目录树，为扫描提供有限路径流。
#
# ## Parameters
# - root: 调用方控制的扫描根 Path。
#
# ## Returns
# 返回生成器；消费时逐个 yield 路径。
#
# ## Constraints and lifecycle
# 不跟随 walk 的链接目录，并剔除发现的 reparse 节点、docs 与保护树。无 onerror 处理器，os.walk 的不可访问目录不构成完整遍历证明。消费期间目录须稳定；无打开文件句柄长期借用，也不创建文件。
#
# ## References
# - scripts/layout/apply_moves.py:498
# - scripts/layout/apply_moves.py:499
# - scripts/layout/apply_moves.py:505
# - scripts/layout/apply_moves.py:507
# - scripts/layout/apply_moves.py:510
# - scripts/layout/apply_moves.py:513
# - scripts/layout/apply_moves.py:515
# - scripts/layout/apply_moves.py:1647
def iter_files(root: Path) -> Iterable[Path]:
    for base, dirs, files in os.walk(root, topdown=True, followlinks=False):
        bp = Path(base)
        kept = []
        for name in dirs:
            p = bp / name
            rel = repo_path(p, root) + "/"
            if (rel.startswith(EXCLUDED_PREFIXES) or name == "docs"
                    or any(part in {"third_party", "node_modules"} for part in rel.rstrip("/").split("/"))
                    or has_reparse(p)):
                continue
            kept.append(name)
        dirs[:] = kept
        for name in files:
            p = bp / name
            if has_reparse(p):
                continue
            yield p


# @unit scripts/layout/apply_moves.py::module_private_root [function]
#
# ## Summary
# 从仓内名称提取第一个 src 后的模块私有搜索根与剩余后缀。
#
# ## Parameters
# - old: 使用斜杠分隔的逻辑路径。
#
# ## Returns
# 存在 src 及下一组件时返回 (root, suffix)，否则 None。
#
# ## Constraints and lifecycle
# 只是字符串结构推断，不验证模块身份、目录存在或最后组件文件类型；空 suffix 有实际意义，目录别名不会据此缩写整个私有根。
#
# ## References
# - scripts/layout/apply_moves.py:540
# - scripts/layout/apply_moves.py:543
# - scripts/layout/apply_moves.py:546
# - scripts/layout/apply_moves.py:548
# - scripts/layout/apply_moves.py:549
# - scripts/layout/apply_moves.py:644
# - scripts/layout/apply_moves.py:1020
def module_private_root(old: str) -> tuple[str, str] | None:
    parts = old.split("/")
    try:
        i = parts.index("src")
    except ValueError:
        return None
    if i + 1 >= len(parts):
        return None
    module = "/".join(parts[:i + 2])
    return module, "/".join(parts[i + 2:])


def mapped(path: str, move_map: dict[str, str]) -> str:
    return move_map.get(path, path)


# @unit scripts/layout/apply_moves.py::canonical_path [function]
#
# ## Summary
# 缓存一次扫描所需的 Path.resolve 结果，统一严格与非严格语义。
#
# ## Parameters
# - path: 待 resolve 的 Path，作为缓存键的一部分。
# - strict: 透传给 Path.resolve；为真时要求目标存在。
#
# ## Returns
# 返回规范 Path；保留 resolve 的异常行为。
#
# ## Constraints and lifecycle
# 缓存可在文件树变化后陈旧；扫描入口通过 clear_path_caches 刷新。lru_cache 并不锁住文件树，调用者须禁止并行改名/链接替换。
#
# ## References
# - scripts/layout/apply_moves.py:578
# - scripts/layout/apply_moves.py:579
# - scripts/layout/apply_moves.py:581
# - scripts/layout/apply_moves.py:608
# - scripts/layout/apply_moves.py:817
# - scripts/layout/apply_moves.py:1400
@lru_cache(maxsize=100000)
def canonical_path(path: Path, strict: bool) -> Path:
    """Cache exact Path.resolve semantics for the duration of a scan pass."""
    return path.resolve(strict=strict)


# @unit scripts/layout/apply_moves.py::clear_path_caches [function]
#
# ## Summary
# 在新的可变文件树扫描前丢弃路径与 dormant CMake 恢复缓存。
#
# ## Parameters
# 无参数。
#
# ## Returns
# 正常完成返回 None；不创建或删除文件。
#
# ## Constraints and lifecycle
# 清 canonical_path、resolve_file、load_cmake_fallback_context 三者；不清 layout_helper 或 path_rewriter。不能替代源树快照或跨线程互斥。
#
# ## References
# - scripts/layout/apply_moves.py:607
# - scripts/layout/apply_moves.py:608
# - scripts/layout/apply_moves.py:609
# - scripts/layout/apply_moves.py:610
# - scripts/layout/apply_moves.py:1639
# - scripts/layout/apply_moves.py:2120
# - scripts/layout/apply_moves.py:2439
# - scripts/layout/apply_moves.py:2547
def clear_path_caches() -> None:
    canonical_path.cache_clear()
    resolve_file.cache_clear()
    load_cmake_fallback_context.cache_clear()


# @unit scripts/layout/apply_moves.py::include_roots [function]
#
# ## Summary
# 在没有权威逐源上下文时构造模块/公共 include 的有限推断根。
#
# ## Parameters
# - root: 仓根 Path。
# - including: 原树中包含者的逻辑名称。
# - name: 原 include 名称，用于公共模块前缀推断。
# - move_map: 协作签名保留项；当前函数不读取此映射。
# - context: 平面 include 根列表形式；正常调用中无权威 variants 时为空。
#
# ## Returns
# 返回按构造顺序的 (Path, logical_root_or_None) 列表。
#
# ## Constraints and lifecycle
# 私有 src/public include/仓根是启发式；嵌套保留模块避免同时探测顶层同名公共根。结果不证明所有根存在、可读或编译器真正使用；权威 variants 在 resolve_include 另行先处理。
#
# ## References
# - scripts/layout/apply_moves.py:641
# - scripts/layout/apply_moves.py:644
# - scripts/layout/apply_moves.py:650
# - scripts/layout/apply_moves.py:656
# - scripts/layout/apply_moves.py:664
# - scripts/layout/apply_moves.py:667
# - scripts/layout/apply_moves.py:669
# - scripts/layout/apply_moves.py:670
# - scripts/layout/apply_moves.py:774
def include_roots(root: Path, including: str, name: str, move_map: dict[str, str],
                  context: dict[str, tuple]) -> list[tuple[Path, str | None]]:
    """Return include search roots with their repository-relative logical roots."""
    src = module_private_root(including)
    roots: list[tuple[Path, str | None]] = []
    module_dir = None
    if src:
        module_src, _ = src
        module_dir = "/".join(module_src.split("/")[:-2])
        roots.extend([
            (root / module_src, module_src),
            (root / module_dir / "include", module_dir + "/include"),
        ])
    # Public include spelling is module-qualified. Consult only that module's
    # real include root instead of probing every module for every directive.
    normalized = name.replace("\\", "/")
    public_module = normalized.split("/", 1)[0]
    if public_module.startswith("zr_vm_"):
        # A retained legacy bundle can contain a nested module with the same
        # public spelling as a top-level module. In that case its adjacent
        # include tree is the contextless fallback for sources in the bundle;
        # probing the repository-level module as well creates a false
        # ambiguity. Explicit File API contexts remain authoritative above.
        bundled_module = bool(module_dir and module_dir.split("/")[-1] == public_module)
        if not bundled_module:
            public = root / public_module / "include"
            if public.is_dir():
                roots.append((public, public_module + "/include"))
    roots.extend((include_dir, None) for include_dir in context.get(including, ()))
    roots.append((root, ""))
    return roots


# @unit scripts/layout/apply_moves.py::context_variants [function]
#
# ## Summary
# 把平面旧上下文或变体集合规范为有序根元组的元组。
#
# ## Parameters
# - context: 逻辑源名到平面 Path 序列或变体序列的映射，可为 None。
# - including: 精确查询的源名。
#
# ## Returns
# 缺失/空值返回空元组，否则返回规范变体。
#
# ## Constraints and lifecycle
# 只通过首元素是否 Path 区分表示，不验证所有元素类型、路径或集合真实性；不复制 Path 对象，不并集多个变体。
#
# ## References
# - scripts/layout/apply_moves.py:696
# - scripts/layout/apply_moves.py:697
# - scripts/layout/apply_moves.py:698
# - scripts/layout/apply_moves.py:700
# - scripts/layout/apply_moves.py:702
# - scripts/layout/apply_moves.py:744
def context_variants(context: dict[str, tuple] | None, including: str) -> tuple[tuple[Path, ...], ...]:
    value = (context or {}).get(including)
    if not value:
        return ()
    if isinstance(value[0], Path):
        return (tuple(value),)
    return tuple(tuple(variant) for variant in value)


# @unit scripts/layout/apply_moves.py::resolve_include [function]
#
# ## Summary
# 在改名前树上解析引用，保持各真实编译变体的有序与一致身份。
#
# ## Parameters
# - root: 受控仓根。
# - including: 原包含者名称，所属目录始终先查。
# - name: 原 quoted include 名称，反斜杠仅用于路径拼接规范化。
# - move_map: 原文件到新逻辑名称的映射。
# - context: 真实逐源 variants；存在时优先于 fallback。
# - fallback_context: 独立 dormant 上下文，只在无真实 variants 时使用。
#
# ## Returns
# 返回 (old_target,new_target) 或 None；部分变体缺失或变体身份不同、启发式多命中时抛 MigrationError。
#
# ## Constraints and lifecycle
# 每个权威变体按顺序取首个可解析文件，不合并根；源码自身目录可一致遮蔽各根。无 variants 时汇总启发式候选并拒绝多文件。resolve_file 将仓外/缺失排为 None；这里不是任意路径/reparse/生成文件的全认证，generated 特例由 unresolved_includes 单独比较。
#
# ## References
# - scripts/layout/apply_moves.py:737
# - scripts/layout/apply_moves.py:742
# - scripts/layout/apply_moves.py:744
# - scripts/layout/apply_moves.py:745
# - scripts/layout/apply_moves.py:752
# - scripts/layout/apply_moves.py:763
# - scripts/layout/apply_moves.py:768
# - scripts/layout/apply_moves.py:769
# - scripts/layout/apply_moves.py:774
# - scripts/layout/apply_moves.py:778
# - scripts/layout/apply_moves.py:783
# - scripts/layout/apply_moves.py:787
def resolve_include(root: Path, including: str, name: str, move_map: dict[str, str],
                    context: dict[str, tuple] | None = None,
                    fallback_context: dict[str, tuple] | None = None) -> tuple[str, str] | None:
    # Resolve against the pre-migration tree: relative includes in the moved
    # file must retain the resolution they had before either file is renamed.
    candidates: list[tuple[Path, str | None]] = [((root / including).parent, None)]
    active_context = context or {}
    variants = context_variants(active_context, including)
    fallback_variants = context_variants(fallback_context, including) if not variants else ()
    if variants or fallback_variants:
        # A dormant registration is authoritative for this one consumer just
        # like an ordered compile-group context, but it never enters the
        # File API propagation graph.
        search_variants = variants or fallback_variants
        outcomes: list[tuple[str, str] | None] = []
        for variant in search_variants:
            found = None
            variant_candidates = list(candidates)
            variant_candidates.extend((path, None) for path in variant)
            seen: set[str] = set()
            for base, _logical in variant_candidates:
                key = str(canonical_path(base, False))
                if key in seen:
                    continue
                seen.add(key)
                candidate = base / Path(name.replace("\\", "/"))
                resolved = resolve_file(candidate, root)
                if resolved is not None:
                    found = (resolved, mapped(resolved, move_map))
                    break
            outcomes.append(found)
        nonempty = [outcome for outcome in outcomes if outcome is not None]
        if nonempty and (len(nonempty) != len(outcomes) or len(set(nonempty)) != 1):
            raise MigrationError(f"quoted include {name!r} from {including} resolves differently across compile contexts: {outcomes}")
        return nonempty[0] if nonempty else None
    # Dormant CMake declarations supplement ordinary module/public inference
    # only when there is no actual File API context. They never propagate.
    candidates.extend(include_roots(root, including, name, move_map, active_context))
    for variant in context_variants(fallback_context, including):
        candidates.extend((path, None) for path in variant)
    resolved: dict[str, str] = {}
    for base, logical in candidates:
        candidate = base / Path(name.replace("\\", "/"))
        old_rel = resolve_file(candidate, root)
        if old_rel is not None:
            resolved[old_rel] = mapped(old_rel, move_map)
    if len(resolved) > 1:
        raise MigrationError(f"ambiguous quoted include {name!r} from {including}: " + ", ".join(sorted(resolved)))
    if resolved:
        old_rel, target_rel = next(iter(resolved.items()))
        return old_rel, target_rel
    return None


# @unit scripts/layout/apply_moves.py::resolve_file [function]
#
# ## Summary
# 把现存候选解析为仓内文件名称并缓存结果。
#
# ## Parameters
# - candidate: 要 strict resolve 的 Path。
# - root: 包含性比较的仓根 Path。
#
# ## Returns
# 目标可规范为仓内且 candidate.is_file 为真时返回相对字符串，否则 None。
#
# ## Constraints and lifecycle
# resolve/relative_to 的 FileNotFoundError、ValueError、OSError 转 None；随后 is_file 不等同逐祖先 reparse 检查。缓存须在树变化后清除，不承诺任意恶意地址或链接安全。
#
# ## References
# - scripts/layout/apply_moves.py:814
# - scripts/layout/apply_moves.py:815
# - scripts/layout/apply_moves.py:817
# - scripts/layout/apply_moves.py:818
# - scripts/layout/apply_moves.py:820
# - scripts/layout/apply_moves.py:821
# - scripts/layout/apply_moves.py:609
@lru_cache(maxsize=100000)
def resolve_file(candidate: Path, root: Path) -> str | None:
    try:
        rel = repo_path(canonical_path(candidate, True), canonical_path(root, False))
    except (FileNotFoundError, ValueError, OSError):
        return None
    if candidate.is_file():
        return rel
    return None


# @unit scripts/layout/apply_moves.py::rewritten_include [function]
#
# ## Summary
# 在保留原解析身份后计算可从新布局找到目标的 include 拼写。
#
# ## Parameters
# - name: 原 include 字符串。
# - including: 原消费者逻辑名。
# - move_map: 完整文件迁移表。
# - root: 原树根 Path。
# - context: 实际编译根映射。
# - require_resolution: 为真时未解析必须拒绝；否则保持原 include。
# - fallback_context: 可选 dormant 变体。
#
# ## Returns
# 需要重写时返回新拼写；保持时返回 None；歧义或必需解析失败抛 MigrationError。
#
# ## Constraints and lifecycle
# 保护目标不改；私有根优先产生模块私有相对拼写，公共转私有可返回新私有 suffix。返回文本没有写文件，调用者负责保留分隔符/BOM和实际落盘。
#
# ## References
# - scripts/layout/apply_moves.py:856
# - scripts/layout/apply_moves.py:859
# - scripts/layout/apply_moves.py:861
# - scripts/layout/apply_moves.py:863
# - scripts/layout/apply_moves.py:867
# - scripts/layout/apply_moves.py:870
# - scripts/layout/apply_moves.py:872
# - scripts/layout/apply_moves.py:878
# - scripts/layout/apply_moves.py:881
# - scripts/layout/apply_moves.py:1586
def rewritten_include(name: str, including: str, move_map: dict[str, str], root: Path,
                      context: dict[str, tuple[Path, ...]], require_resolution: bool = False,
                      fallback_context: dict[str, tuple] | None = None) -> str | None:
    including_moved = mapped(including, move_map) != including
    legacy_parent = name.startswith(("../", "..\\")) or "/../" in name or "\\..\\" in name
    resolved = resolve_include(root, including, name, move_map, context, fallback_context)
    if not resolved:
        if require_resolution:
            raise MigrationError(f"cannot resolve mapped/private quoted include {name!r} from {including}; supply the configured build with --cmake-build")
        return None
    old_target, new_target = resolved
    if excluded(old_target) or excluded(new_target):
        return None
    private = module_private_root(old_target)
    if old_target == new_target and not legacy_parent and (not private or not including_moved):
        return None
    if private and module_private_root(new_target):
        root_rel = private[0]
        try:
            return Path(new_target).relative_to(Path(root_rel)).as_posix()
        except ValueError:
            pass
    # Public-to-private header demotion: retain a path that resolves from the
    # owning module's private include root.
    new_private = module_private_root(new_target)
    if new_private:
        return new_private[1]
    return new_target


# @unit scripts/layout/apply_moves.py::path_aliases [function]
#
# ## Summary
# 列出一个移动源的完整、私有短名与公共 include 历史别名。
#
# ## Parameters
# - move: Move 记录；这里不重新验证其路径。
#
# ## Returns
# 返回按长度降序排列的字符串元组。
#
# ## Constraints and lifecycle
# 包括反斜杠形式；集合去重，同长度顺序不作为稳定协议。只构造文本，无文件访问；目录规则由 directory_aliases 处理。
#
# ## References
# - scripts/layout/apply_moves.py:908
# - scripts/layout/apply_moves.py:909
# - scripts/layout/apply_moves.py:911
# - scripts/layout/apply_moves.py:915
# - scripts/layout/apply_moves.py:918
# - scripts/layout/apply_moves.py:923
# - scripts/layout/apply_moves.py:952
def path_aliases(move: Move) -> tuple[str, ...]:
    values = {move.old, move.old.replace("/", "\\")}
    private = module_private_root(move.old)
    if private:
        module, suffix = private
        values.add("src/" + module.split("/")[-1] + "/" + suffix)
        values.add(module.split("/")[-1] + "/" + suffix)
    parts = move.old.split("/")
    if "include" in parts:
        index = parts.index("include")
        if index + 2 < len(parts) and parts[index + 1] == parts[index - 1]:
            module_name = parts[index + 1]
            public_name = "/".join(parts[index + 1:])
            values.add(public_name)
            values.add(public_name.replace("/", "\\"))
    return tuple(sorted(values, key=len, reverse=True))


# @unit scripts/layout/apply_moves.py::add_move_aliases [function]
#
# ## Summary
# 把文件别名加入改写选择表，并区分完整路径与公共降级短名。
#
# ## Parameters
# - choices: [in,out]别名到 (replacement,is_directory) 的可变表。
# - move: 已审定的源/目的记录。
#
# ## Returns
# 完成返回 None；冲突经 add_path_alias 抛 MigrationError。
#
# ## Constraints and lifecycle
# 多次加入可在晚期冲突前留下已加入部分，不是字典事务；调用者目前使用新局部表。公共到私有短名用私有 suffix，完整路径仍用完整目的。
#
# ## References
# - scripts/layout/apply_moves.py:950
# - scripts/layout/apply_moves.py:951
# - scripts/layout/apply_moves.py:952
# - scripts/layout/apply_moves.py:953
# - scripts/layout/apply_moves.py:954
# - scripts/layout/apply_moves.py:958
# - scripts/layout/apply_moves.py:959
# - scripts/layout/apply_moves.py:1080
def add_move_aliases(choices: dict[str, tuple[str, bool]], move: Move) -> None:
    public_move = "include" in move.old.split("/") and module_private_root(move.new) is not None
    for alias in path_aliases(move):
        full_path_alias = alias.replace("\\", "/") == move.old
        if public_move and not full_path_alias:
            private = module_private_root(move.new)
            replacement = private[1] if private else move.new
        else:
            replacement = move.new
        add_path_alias(choices, alias, replacement, False)


# @unit scripts/layout/apply_moves.py::validate_move_aliases [function]
#
# ## Summary
# 在任何文件移动前用局部表检查所有文件别名的一致性。
#
# ## Parameters
# - moves: 有限 Move 列表。
#
# ## Returns
# 一致时 None；首个冲突抛 MigrationError。
#
# ## Constraints and lifecycle
# 只验证别名冲突，不认证文件状态或保留目录完整性；失败局部表丢弃，不写仓库。
#
# ## References
# - scripts/layout/apply_moves.py:981
# - scripts/layout/apply_moves.py:982
# - scripts/layout/apply_moves.py:984
# - scripts/layout/apply_moves.py:2653
def validate_move_aliases(moves: list[Move]) -> None:
    choices: dict[str, tuple[str, bool]] = {}
    for move in moves:
        add_move_aliases(choices, move)


# @unit scripts/layout/apply_moves.py::directory_aliases [function]
#
# ## Summary
# 仅为具有一致目标结构且无未映射后代的目录生成前缀别名。
#
# ## Parameters
# - root_text: 仓根文本，用于检查旧目录的可枚举后代。
# - move_pairs: 完整旧/新文件对元组；预期来自已验证 Move 列表。
#
# ## Returns
# 返回按旧目录长度降序的 (old_dir,new_dir) 元组。
#
# ## Constraints and lifecycle
# 跳过空私有 suffix，避免重写整个模块搜索根。现有旧目录逐后代核对；不存在的旧目录仅按映射结构推断。无锁，os.walk 排除/不可读范围仍限制完整性，不是任意文件系统的安全认证。
#
# ## References
# - scripts/layout/apply_moves.py:1014
# - scripts/layout/apply_moves.py:1017
# - scripts/layout/apply_moves.py:1020
# - scripts/layout/apply_moves.py:1025
# - scripts/layout/apply_moves.py:1030
# - scripts/layout/apply_moves.py:1037
# - scripts/layout/apply_moves.py:1038
# - scripts/layout/apply_moves.py:1039
# - scripts/layout/apply_moves.py:1042
# - scripts/layout/apply_moves.py:1045
# - scripts/layout/apply_moves.py:1642
def directory_aliases(root_text: str, move_pairs: tuple[tuple[str, str], ...]) -> tuple[tuple[str, str], ...]:
    root = Path(root_text)
    move_map = dict(move_pairs)
    parents = {"/".join(old.split("/")[:i]) for old, _ in move_pairs for i in range(1, len(old.split("/")))}
    results: set[tuple[str, str]] = set()
    for old_dir in parents:
        private_dir = module_private_root(old_dir)
        if private_dir and not private_dir[1]:
            # The module's private root is a search root used by unrelated
            # consumers; moving one complete subtree must not alias that root.
            continue
        rows = [(old, new) for old, new in move_pairs if old.startswith(old_dir + "/")]
        target_dirs: set[str] = set()
        valid = True
        for old, new in rows:
            suffix = old[len(old_dir) + 1:]
            if not new.endswith("/" + suffix):
                valid = False
                break
            target_dirs.add(new[:-(len(suffix) + 1)])
        if not valid or len(target_dirs) != 1:
            continue
        old_path = root / old_dir
        if old_path.is_dir():
            descendant_files = [repo_path(path, root) for path in iter_files(old_path)]
            if any(rel not in move_map for rel in descendant_files):
                continue
            new_dir = next(iter(target_dirs))
            if any(move_map[rel] != new_dir + "/" + rel[len(old_dir) + 1:] for rel in descendant_files):
                continue
        results.add((old_dir, next(iter(target_dirs))))
    return tuple(sorted(results, key=lambda pair: len(pair[0]), reverse=True))


# @unit scripts/layout/apply_moves.py::path_rewriter [function]
#
# ## Summary
# 预编译文件/目录别名与新目标保护 trie，供大量字节改写复用。
#
# ## Parameters
# - root_text: 仅参与 lru_cache 键的根身份文本；当前本体不读取此根。
# - move_pairs: 有限旧/新文件对。
# - dir_pairs: 已审定可整体改写的目录对。
#
# ## Returns
# 返回共享缓存的 PathRewriter 实例；别名冲突抛 MigrationError。
#
# ## Constraints and lifecycle
# 建立 replacement trie 和目标保护 trie；原/新分隔符规范化。对象及内部 dict 是可变借用，所有调用者须只读使用缓存结果，文件树身份不由这里验证。TODO: scripts/layout/apply_moves.py:1098 的 ascii/errors=ignore 会丢弃别名非 ASCII 字符，而 scripts/layout/apply_moves.py:334 未限制该域；需核对迁移表支持的字符域与匹配一致性，现有测试无该向量，不作已复现 BUG。
#
# ## References
# - scripts/layout/apply_moves.py:1075
# - scripts/layout/apply_moves.py:1076
# - scripts/layout/apply_moves.py:1080
# - scripts/layout/apply_moves.py:1083
# - scripts/layout/apply_moves.py:1092
# - scripts/layout/apply_moves.py:1098
# - scripts/layout/apply_moves.py:1104
# - scripts/layout/apply_moves.py:1107
# - scripts/layout/apply_moves.py:334
# - scripts/layout/apply_moves.py:390
@lru_cache(maxsize=2)
def path_rewriter(root_text: str, move_pairs: tuple[tuple[str, str], ...], dir_pairs: tuple[tuple[str, str], ...]):
    choices: dict[str, tuple[str, bool]] = {}
    protected_paths: dict[str, bool] = {}
    for old_path, new_path in move_pairs:
        add_move_aliases(choices, Move(old_path, new_path))
        old_aliases = {alias.replace("\\", "/") for alias in path_aliases(Move(old_path, new_path))}
        protected_paths.update({
            alias: False for alias in path_aliases(Move(new_path, new_path))
            if alias.replace("\\", "/") not in old_aliases})
    for old_dir, new_dir in dir_pairs:
        private = module_private_root(old_dir)
        aliases = {old_dir, old_dir.replace("/", "\\")}
        if private and private[1]:
            short = private[0].split("/")[-1] + "/" + private[1]
            aliases.update({short, "src/" + short})
        for alias in aliases:
            add_path_alias(choices, alias, new_dir, True)
        protected_paths[new_dir] = True
        protected_paths[new_dir.replace("/", "\\")] = True
    trie: dict = {}
    for alias, (replacement, is_directory) in choices.items():
        node = trie
        for byte in alias.encode("ascii", errors="ignore"):
            node = node.setdefault(byte, {})
        node[None] = (replacement, is_directory)
    protected_trie: dict = {}
    for alias, is_directory in protected_paths.items():
        node = protected_trie
        for byte in alias.replace("\\", "/").encode("ascii", errors="ignore"):
            node = node.setdefault(byte, {})
        node[None] = (alias.replace("\\", "/"), is_directory)
    return PathRewriter(trie, protected_trie)


# @unit scripts/layout/apply_moves.py::add_path_alias [function]
#
# ## Summary
# 规范别名分隔符后登记替换，防止同名存在不同改写规则。
#
# ## Parameters
# - choices: [in,out]构建中的局部选择表。
# - alias: 旧文本；反斜杠转斜杠并删末尾斜杠。
# - replacement: 目的文本，使用同样分隔符规范化。
# - is_directory: 标识是否允许目录后缀继续匹配。
#
# ## Returns
# 同规则重复可覆盖同值并返回 None；冲突抛 MigrationError。
#
# ## Constraints and lifecycle
# 只拒绝已有别名的不同 replacement/目录位，不做路径合法性、存在性或字符域认证；已登记其它项不回滚。
#
# ## References
# - scripts/layout/apply_moves.py:1133
# - scripts/layout/apply_moves.py:1134
# - scripts/layout/apply_moves.py:1136
# - scripts/layout/apply_moves.py:1137
# - scripts/layout/apply_moves.py:1139
def add_path_alias(choices: dict[str, tuple[str, bool]], alias: str, replacement: str, is_directory: bool) -> None:
    key = alias.replace("\\", "/").rstrip("/")
    value = replacement.replace("\\", "/").rstrip("/")
    prior = choices.get(key)
    if prior and (prior[0] != value or prior[1] != is_directory):
        raise MigrationError(f"conflicting path rewrite alias {alias!r}: {prior[0]} vs {value}")
    choices[key] = (value, is_directory)


# @unit scripts/layout/apply_moves.py::PathRewriter [class]
#
# ## Summary
# 借用预编译替换/保护 trie，对任意 bytes 片段执行有限路径词法改写。
#
# ## Fields and invariants
# Fields: trie 为旧别名树，protected_trie 为新目的保护树；节点以字节为键，None 叶保存 (replacement,is_directory)。__init__ 不拷贝，lru_cache 可共享对象；调用方不得并发改内部 dict。实例不拥有文件句柄或磁盘树，不是路径安全/语言语法验证器。
#
# ## Constraints and lifecycle
# 类型本身不授予路径、文件树或夹具的所有权；构造和使用受以上约束。
#
# ## References
# - scripts/layout/apply_moves.py:1076
# - scripts/layout/apply_moves.py:1095
# - scripts/layout/apply_moves.py:1100
# - scripts/layout/apply_moves.py:1101
# - scripts/layout/apply_moves.py:1106
# - scripts/layout/apply_moves.py:1107
# - scripts/layout/apply_moves.py:1163
# - scripts/layout/apply_moves.py:1185
# - scripts/layout/apply_moves.py:1261
class PathRewriter:
    # @unit scripts/layout/apply_moves.py::PathRewriter.__init__ [function]
    #
    # ## Summary
    # 保存两棵预构建 trie，区分可替换旧名和必须保持的新目标。
    #
    # ## Parameters
    # - self: 正在初始化的改写器。
    # - trie: 借用的 replacement dict；构造后调用方不得并发修改。
    # - protected_trie: 可选保护 dict；falsey 时替换为空新 dict。
    #
    # ## Returns
    # 初始化完成隐式返回 None。
    #
    # ## Constraints and lifecycle
    # 不深拷贝或验证 trie 节点格式；路径/字符域正确性由构建方负责。对象可被 lru_cache 共享，内部引用不是线程隔离副本。
    #
    # ## References
    # - scripts/layout/apply_moves.py:1185
    # - scripts/layout/apply_moves.py:1186
    # - scripts/layout/apply_moves.py:1187
    # - scripts/layout/apply_moves.py:1107
    def __init__(self, trie: dict, protected_trie: dict | None = None):
        self.trie = trie
        self.protected_trie = protected_trie or {}

    # @unit scripts/layout/apply_moves.py::PathRewriter._match [function]
    #
    # ## Summary
    # 从一个候选边界沿 trie 找到满足右边界的最长别名。
    #
    # ## Parameters
    # - self: 提供匹配方法的改写器，本函数只读。
    # - trie: 结构符合 path_rewriter 约定的 trie。
    # - data: 不可变原始 bytes。
    # - start: 调用者给出的合法起始索引。
    #
    # ## Returns
    # 返回 (exclusive_end,replacement,directory_consumed_separator) 或 None。
    #
    # ## Constraints and lifecycle
    # 反斜杠按斜杠边进入；目录命中可连同分隔符消费，文件命中须右侧不在有限 ASCII 路径字符域。不是通用文件名/编码解析器；不访问文件系统。
    #
    # ## References
    # - scripts/layout/apply_moves.py:1216
    # - scripts/layout/apply_moves.py:1220
    # - scripts/layout/apply_moves.py:1222
    # - scripts/layout/apply_moves.py:1226
    # - scripts/layout/apply_moves.py:1229
    # - scripts/layout/apply_moves.py:1231
    # - scripts/layout/apply_moves.py:1233
    # - scripts/layout/apply_moves.py:1270
    # - scripts/layout/apply_moves.py:1275
    def _match(self, trie: dict, data: bytes, start: int):
        node = trie
        cursor = start
        longest = None
        while cursor < len(data):
            byte = data[cursor]
            node = node.get(byte) or (node.get(47) if byte == 92 else None)
            if node is None:
                return longest
            cursor += 1
            terminal = node.get(None)
            if terminal:
                replacement, is_directory = terminal
                if is_directory and cursor < len(data) and data[cursor] in (47, 92):
                    longest = (cursor + 1, replacement, True)
                elif cursor == len(data) or data[cursor] not in b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.\\/-":
                    longest = (cursor, replacement, False)
        return longest

    # @unit scripts/layout/apply_moves.py::PathRewriter.replace [function]
    #
    # ## Summary
    # 在字节片段内保护已迁移目标并替换满足词法边界的旧别名。
    #
    # ## Parameters
    # - self: 只读使用的改写器；trie 不得并发改变。
    # - data: 待处理 bytes；包括非路径文本及任意字节。
    #
    # ## Returns
    # 返回内容重写后的 bytes；不承诺对象身份。
    #
    # ## Constraints and lifecycle
    # 优先跳过保护目标，采用有限左/右 ASCII 边界，保留原片段斜杠多数风格。输出由新 bytearray 构建，输入不改；匹配不等于 C/Python/CMake 语义解析，也不验证磁盘路径。
    #
    # ## References
    # - scripts/layout/apply_moves.py:1261
    # - scripts/layout/apply_moves.py:1265
    # - scripts/layout/apply_moves.py:1267
    # - scripts/layout/apply_moves.py:1269
    # - scripts/layout/apply_moves.py:1274
    # - scripts/layout/apply_moves.py:1278
    # - scripts/layout/apply_moves.py:1280
    # - scripts/layout/apply_moves.py:1282
    # - scripts/layout/apply_moves.py:1289
    # - scripts/layout/apply_moves.py:1333
    def replace(self, data: bytes) -> bytes:
        output = bytearray()
        cursor = 0
        index = 0
        left_chars = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-"
        while index < len(data):
            boundary = index == 0 or data[index - 1] not in left_chars
            byte = data[index]
            if boundary and (byte in self.protected_trie or (byte == 92 and 47 in self.protected_trie)):
                protected = self._match(self.protected_trie, data, index)
                if protected:
                    index = protected[0]
                    continue
            if boundary and (byte in self.trie or (byte == 92 and 47 in self.trie)):
                match = self._match(self.trie, data, index)
                if match:
                    end, replacement, is_directory = match
                    consumed = end - (1 if is_directory and end > index and data[end - 1] in (47, 92) else 0)
                    old = data[index:consumed]
                    separator = b"\\" if old.count(b"\\") > old.count(b"/") else b"/"
                    output.extend(data[cursor:index])
                    output.extend(replacement.replace("/", separator.decode("ascii")).encode("utf-8"))
                    if is_directory and end > consumed:
                        output.extend(separator)
                    cursor = end
                    index = end
                    continue
            index += 1
        output.extend(data[cursor:])
        return bytes(output)


# @unit scripts/layout/apply_moves.py::replace_path_tokens [function]
#
# ## Summary
# 在一般文本改写前保留真实未映射文件的完整 token 区间。
#
# ## Parameters
# - data: 原始字节片段。
# - moves: 空列表时直接返回同一 data；其它为已审定映射。
# - root: 受控原仓根。
# - dir_pairs: 目录别名对。
# - compiled: 可选与 moves/dir_pairs 一致的预编译改写器。
#
# ## Returns
# 返回 bytes；下层路径/改写错误可传播。
#
# ## Constraints and lifecycle
# 按受保护 spans 分段处理，真实未映射嵌套路径不被短别名误改。调用者保证 compiled 与输入一致；不做跨调用认证/锁定。
#
# ## References
# - scripts/layout/apply_moves.py:1320
# - scripts/layout/apply_moves.py:1323
# - scripts/layout/apply_moves.py:1326
# - scripts/layout/apply_moves.py:1327
# - scripts/layout/apply_moves.py:1329
# - scripts/layout/apply_moves.py:1332
# - scripts/layout/apply_moves.py:1334
# - scripts/layout/apply_moves.py:1338
def replace_path_tokens(data: bytes, moves: list[Move], root: Path,
                        dir_pairs: tuple[tuple[str, str], ...],
                        compiled: PathRewriter | None = None) -> bytes:
    if not moves:
        return data
    move_map = {move.old: move.new for move in moves}
    protected = existing_unmapped_path_spans(data, root, move_map)
    rewriter = compiled or path_rewriter(
        str(root.resolve()), tuple(move_map.items()), dir_pairs)
    if protected:
        output = bytearray()
        cursor = 0
        for start, end in protected:
            output.extend(rewriter.replace(data[cursor:start]))
            output.extend(data[start:end])
            cursor = end
        output.extend(rewriter.replace(data[cursor:]))
        return bytes(output)
    return rewriter.replace(data)


# @unit scripts/layout/apply_moves.py::_PATH_TOKEN [state]
#
# ## Summary
# 限定真实未映射文件保护器可识别的 ASCII 路径 token 域。
#
# ## Constraints and lifecycle
# 支持可选盘符和至少两个分隔组件；不覆盖所有空格/Unicode文件名。只是候选提取，后续严格resolve、仓内/映射集合与祖先reparse检查才能决定保护。
#
# ## References
# - scripts/layout/apply_moves.py:1357
# - scripts/layout/apply_moves.py:1358
# - scripts/layout/apply_moves.py:1359
# - scripts/layout/apply_moves.py:1360
# - scripts/layout/apply_moves.py:1403
# - scripts/layout/apply_moves.py:1416
# - scripts/layout/apply_moves.py:1431
_PATH_TOKEN = re.compile(
    rb"(?<![A-Za-z0-9_.-])(?:[A-Za-z]:[\\/])?"
    rb"[A-Za-z0-9_.-]+(?:[\\/][A-Za-z0-9_.-]+)+(?![A-Za-z0-9_.-])"
)


# @unit scripts/layout/apply_moves.py::existing_unmapped_path_spans [function]
#
# ## Summary
# 识别可保护的现存仓内未映射文件 token，避免后缀别名误伤。
#
# ## Parameters
# - data: 按有限 ASCII 路径正则扫描的 bytes。
# - root: 规范仓根。
# - move_map: 源/目的完整名称集合，二者都不能被此保护规则遮蔽。
#
# ## Returns
# 返回按文本出现次序的 (start,end) 字节区间元组。
#
# ## Constraints and lifecycle
# 仅接受无 ..、在根内、resolve_file 成功且逐祖先无 reparse 的 token；相关解码/路径错误跳过。检查有 TOCTOU 窗口且不是全部 Unicode/空格路径解析；不打开或读取文件内容。
#
# ## References
# - scripts/layout/apply_moves.py:1357
# - scripts/layout/apply_moves.py:1391
# - scripts/layout/apply_moves.py:1400
# - scripts/layout/apply_moves.py:1403
# - scripts/layout/apply_moves.py:1407
# - scripts/layout/apply_moves.py:1413
# - scripts/layout/apply_moves.py:1416
# - scripts/layout/apply_moves.py:1419
# - scripts/layout/apply_moves.py:1431
# - scripts/layout/apply_moves.py:1434
# - scripts/layout/apply_moves.py:1436
def existing_unmapped_path_spans(data: bytes, root: Path,
                                 move_map: dict[str, str]) -> tuple[tuple[int, int], ...]:
    """Find exact existing checkout-file tokens that are not mapped paths.

    A real, unmapped file can contain a suffix matching a legacy path alias.
    Protect only tokens that resolve to regular existing files inside this
    checkout, and never protect move sources or destinations. Reparse paths
    are intentionally ineligible so this guard cannot relax path safety.
    """
    root_real = canonical_path(root, False)
    mapped_paths = set(move_map) | set(move_map.values())
    spans: list[tuple[int, int]] = []
    for match in _PATH_TOKEN.finditer(data):
        try:
            token = match.group().decode("utf-8")
            candidate = Path(token)
            if any(part == ".." for part in candidate.parts):
                continue
            if not candidate.is_absolute():
                candidate = root / candidate
            candidate_abs = Path(os.path.abspath(candidate))
            try:
                candidate_abs.relative_to(root_real)
            except ValueError:
                continue
            resolved_rel = resolve_file(candidate_abs, root_real)
            if resolved_rel is None or resolved_rel in mapped_paths:
                continue
            current = candidate_abs
            has_reparse_component = False
            while current != root_real:
                if has_reparse(current):
                    has_reparse_component = True
                    break
                current = current.parent
                try:
                    current.relative_to(root_real)
                except ValueError:
                    has_reparse_component = True
                    break
            if has_reparse_component or has_reparse(root_real):
                continue
            spans.append(match.span())
        except (UnicodeDecodeError, OSError, ValueError):
            continue
    return tuple(spans)


# @unit scripts/layout/apply_moves.py::include_candidate_names [function]
#
# ## Summary
# 构造需要严格解析的 .h/.inc/.def 旧 include 候选集合。
#
# ## Parameters
# - moves: 已审定 Move 列表。
#
# ## Returns
# 返回不可变 frozenset；可能含完整、短名、basename 与私有 suffix。
#
# ## Constraints and lifecycle
# 只按旧文件后缀纳入，不证明目标存在或任何消费者真正使用；用于减少一般 include 的不必要改写。
#
# ## References
# - scripts/layout/apply_moves.py:1461
# - scripts/layout/apply_moves.py:1464
# - scripts/layout/apply_moves.py:1466
# - scripts/layout/apply_moves.py:1467
# - scripts/layout/apply_moves.py:1470
# - scripts/layout/apply_moves.py:1475
# - scripts/layout/apply_moves.py:1524
def include_candidate_names(moves: list[Move]) -> frozenset[str]:
    names: set[str] = set()
    for move in moves:
        if Path(move.old).suffix.lower() not in {".h", ".inc", ".def"}:
            continue
        names.update(path_aliases(move))
        names.add(Path(move.old).name)
        private = module_private_root(move.old)
        if private and private[1]:
            names.add(private[1])
        parts = move.old.split("/")
        if "include" in parts:
            index = parts.index("include")
            if index + 2 < len(parts) and parts[index + 1] == parts[index - 1]:
                names.add("/".join(parts[index + 1:]))
    return frozenset(names)


# @unit scripts/layout/apply_moves.py::rewrite_bytes [function]
#
# ## Summary
# 先按原语义解析 quoted include，再在其余片段执行路径 token 改写。
#
# ## Parameters
# - data: 源文件原 bytes，BOM/行尾保留在未改片段。
# - rel: 源文件原逻辑名。
# - moves: 已审定移动列表。
# - move_map: 与 moves 一致的精确映射。
# - root: 原树根。
# - dir_pairs: 可选预计算目录对，None 时计算。
# - context: 可选真实编译根。
# - fallback_context: 可选独立 dormant 根。
# - compiled_rewriter: 与本批映射一致的只读改写器。
# - candidates: 可选一致的 include 候选集合。
#
# ## Returns
# 返回新 bytes；无磁盘写入；必需解析/歧义或下层异常可传播。
#
# ## Constraints and lifecycle
# INCLUDE_RE 是文本正则，不执行 C 预处理，可能看到注释/非活动分支中的字面量。先保护原 include 解析身份，再改普通路径；此顺序保证相对父路径不被目录别名先污染。额外预计算参数必须来自同批输入。
#
# ## References
# - scripts/layout/apply_moves.py:1515
# - scripts/layout/apply_moves.py:1521
# - scripts/layout/apply_moves.py:1523
# - scripts/layout/apply_moves.py:1524
# - scripts/layout/apply_moves.py:1575
# - scripts/layout/apply_moves.py:1584
# - scripts/layout/apply_moves.py:1586
# - scripts/layout/apply_moves.py:1593
# - scripts/layout/apply_moves.py:1597
# - scripts/layout/apply_moves.py:1598
# - scripts/layout/apply_moves.py:1599
# - scripts/layout/apply_moves.py:1602
def rewrite_bytes(data: bytes, rel: str, moves: list[Move], move_map: dict[str, str], root: Path,
                  dir_pairs: tuple[tuple[str, str], ...] | None = None,
                  context: dict[str, tuple[Path, ...]] | None = None,
                  fallback_context: dict[str, tuple] | None = None,
                  compiled_rewriter: PathRewriter | None = None,
                  candidates: frozenset[str] | None = None) -> bytes:
    pairs = tuple((m.old, m.new) for m in moves)
    if dir_pairs is None:
        dir_pairs = directory_aliases(str(root.resolve()), pairs)
    candidates = candidates if candidates is not None else include_candidate_names(moves)
    normalized_candidates = {candidate.replace("\\", "/") for candidate in candidates}
    active_context = context or {}

    # @unit scripts/layout/apply_moves.py::rewrite_bytes.replace_tokens [function]
    #
    # ## Summary
    # 把当前文件中非 include 的字节片段送入同批改写器。
    #
    # ## Parameters
    # - segment: 从原 data 切出的不含当前匹配 include 的片段。
    #
    # ## Returns
    # 返回重写后的 bytes。
    #
    # ## Constraints and lifecycle
    # 闭包只借用本次 moves/root/dir_pairs/compiled_rewriter；不读取下一个文件、不改变外部映射。
    #
    # ## References
    # - scripts/layout/apply_moves.py:1547
    # - scripts/layout/apply_moves.py:1548
    # - scripts/layout/apply_moves.py:1598
    # - scripts/layout/apply_moves.py:1601
    def replace_tokens(segment: bytes) -> bytes:
        return replace_path_tokens(segment, moves, root, dir_pairs, compiled_rewriter)

    # @unit scripts/layout/apply_moves.py::rewrite_bytes.include_sub [function]
    #
    # ## Summary
    # 保留原 include 拼写身份，并按必要解析规则重写一个匹配。
    #
    # ## Parameters
    # - match: 来自 INCLUDE_RE 的当前 re.Match[bytes]，须有 name/head/tail 组。
    #
    # ## Returns
    # 返回完整 include 字节片段；无法 UTF-8 解码或非候选时原样返回。
    #
    # ## Constraints and lifecycle
    # 移动消费者、父相对路径或候选名称进入解析；required 失败不静默保持。输出只改变 name，保留 head/tail 与原是否使用反斜杠的风格；闭包输入须稳定。
    #
    # ## References
    # - scripts/layout/apply_moves.py:1575
    # - scripts/layout/apply_moves.py:1576
    # - scripts/layout/apply_moves.py:1578
    # - scripts/layout/apply_moves.py:1581
    # - scripts/layout/apply_moves.py:1584
    # - scripts/layout/apply_moves.py:1586
    # - scripts/layout/apply_moves.py:1587
    # - scripts/layout/apply_moves.py:1589
    # - scripts/layout/apply_moves.py:1591
    # - scripts/layout/apply_moves.py:1599
    def include_sub(match: re.Match[bytes]) -> bytes:
        raw = match.group("name")
        try:
            name = raw.decode("utf-8")
        except UnicodeDecodeError:
            return match.group(0)
        legacy_parent = name.startswith(("../", "..\\")) or "/../" in name or "\\..\\" in name
        moved_consumer = mapped(rel, move_map) != rel
        candidate_name = name.replace("\\", "/")
        if not moved_consumer and not legacy_parent and candidate_name not in normalized_candidates:
            return match.group(0)
        changed = rewritten_include(name, rel, move_map, root, active_context,
                                    require_resolution=legacy_parent or candidate_name in normalized_candidates,
                                    fallback_context=fallback_context)
        if changed is None:
            return match.group(0)
        return match.group("head") + changed.replace("/", "\\" if "\\" in name else "/").encode("utf-8") + match.group("tail")

    # Resolve and rewrite quoted includes against their original spelling
    # before broad path-token rewrites run over the rest of the file.
    output = bytearray()
    cursor = 0
    for match in INCLUDE_RE.finditer(data):
        output.extend(replace_tokens(data[cursor:match.start()]))
        output.extend(include_sub(match))
        cursor = match.end()
    output.extend(replace_tokens(data[cursor:]))
    return bytes(output)


# @unit scripts/layout/apply_moves.py::scan_rewrites [function]
#
# ## Summary
# 扫描有限允许文本域并收集需要改变的前后字节对，不落盘。
#
# ## Parameters
# - root: 受控原树根。
# - moves: 已审定映射。
# - context: 可选真实编译根。
# - progress: 每 500 个候选向 stderr 输出进度。
# - fallback_context: 独立 dormant 根。
#
# ## Returns
# 返回 rel -> (original,updated) 字典，仅含确实改变的文件。
#
# ## Constraints and lifecycle
# 入口清路径缓存并共用预编译别名；读取 OSError 转 MigrationError。结果是内存快照而非锁定输入，apply 前必须重扫比较；遍历排除与无权限目录限制全量性。
#
# ## References
# - scripts/layout/apply_moves.py:1636
# - scripts/layout/apply_moves.py:1639
# - scripts/layout/apply_moves.py:1642
# - scripts/layout/apply_moves.py:1643
# - scripts/layout/apply_moves.py:1647
# - scripts/layout/apply_moves.py:1652
# - scripts/layout/apply_moves.py:1655
# - scripts/layout/apply_moves.py:1656
# - scripts/layout/apply_moves.py:1658
# - scripts/layout/apply_moves.py:1660
# - scripts/layout/apply_moves.py:1662
# - scripts/layout/apply_moves.py:2720
def scan_rewrites(root: Path, moves: list[Move], context: dict[str, tuple[Path, ...]] | None = None,
                  progress: bool = False,
                  fallback_context: dict[str, tuple] | None = None) -> dict[str, tuple[bytes, bytes]]:
    clear_path_caches()
    move_map = {m.old: m.new for m in moves}
    pairs = tuple((m.old, m.new) for m in moves)
    dir_pairs = directory_aliases(str(root.resolve()), pairs)
    compiled_rewriter = path_rewriter(str(root.resolve()), pairs, dir_pairs)
    candidates = include_candidate_names(moves)
    result: dict[str, tuple[bytes, bytes]] = {}
    scanned = 0
    for path in iter_files(root):
        rel = repo_path(path, root)
        if not is_scan_file(rel, path):
            continue
        scanned += 1
        if progress and scanned % 500 == 0:
            print(f"scanned {scanned} candidate files; {len(result)} require rewrites", file=sys.stderr)
        try:
            original = path.read_bytes()
        except OSError as exc:
            raise MigrationError(f"cannot read {rel}: {exc}") from exc
        updated = rewrite_bytes(original, rel, moves, move_map, root, dir_pairs, context,
                                fallback_context, compiled_rewriter, candidates)
        if updated != original:
            result[rel] = (original, updated)
    return result


# @unit scripts/layout/apply_moves.py::ancestors_safe [function]
#
# ## Summary
# 在预检阶段拒绝路径链中的 reparse 和非目录祖先。
#
# ## Parameters
# - root: 已选定仓根，本函数从其下组件开始。
# - rel: 已通过 safe_relative 的逻辑相对名。
# - include_leaf: 为假只查祖先，为真也查叶节点。
#
# ## Returns
# 通过时 None；发现阻挡抛 MigrationError。
#
# ## Constraints and lifecycle
# 不检查 root 自身，不加锁；叶文件在 include_leaf=True 时允许。组件检查与后续 rename 之间仍可能变化，调用者须提供稳定且独占的树。
#
# ## References
# - scripts/layout/apply_moves.py:1689
# - scripts/layout/apply_moves.py:1691
# - scripts/layout/apply_moves.py:1693
# - scripts/layout/apply_moves.py:1695
# - scripts/layout/apply_moves.py:1697
# - scripts/layout/apply_moves.py:1732
# - scripts/layout/apply_moves.py:1733
def ancestors_safe(root: Path, rel: str, include_leaf: bool = True) -> None:
    parts = rel.split("/")
    limit = len(parts) if include_leaf else len(parts) - 1
    current = root
    for component in parts[:limit]:
        current = current / component
        if has_reparse(current):
            raise MigrationError(f"symlink/junction is not allowed in migration path: {repo_path(current, root)}")
        if current.exists() and current != root and not current.is_dir() and (component != parts[-1] or not include_leaf):
            raise MigrationError(f"non-directory ancestor blocks migration path: {repo_path(current, root)}")


# @unit scripts/layout/apply_moves.py::preflight [function]
#
# ## Summary
# 把整批映射划为尚未移动或已完成状态，拒绝混合树及冲突。
#
# ## Parameters
# - root: 受控、稳定的根。
# - moves: 文件名保留且无链环的 Move 列表。
#
# ## Returns
# 返回 (pending_old_names,completed_old_names) 两个 set；失败抛 MigrationError。
#
# ## Constraints and lifecycle
# 检查源/目的祖先、目的 reparse、源为文件、目标碰撞；源缺失且目的为文件视为已完成，但不在这里比对目标内容，plan 校验另做。任意 pending 与 completed 共存即拒绝。
#
# ## References
# - scripts/layout/apply_moves.py:1727
# - scripts/layout/apply_moves.py:1732
# - scripts/layout/apply_moves.py:1733
# - scripts/layout/apply_moves.py:1734
# - scripts/layout/apply_moves.py:1736
# - scripts/layout/apply_moves.py:1737
# - scripts/layout/apply_moves.py:1739
# - scripts/layout/apply_moves.py:1742
# - scripts/layout/apply_moves.py:1746
# - scripts/layout/apply_moves.py:1748
def preflight(root: Path, moves: list[Move]) -> tuple[set[str], set[str]]:
    pending: set[str] = set()
    completed: set[str] = set()
    for move in moves:
        source, target = root / move.old, root / move.new
        ancestors_safe(root, move.old)
        ancestors_safe(root, move.new, include_leaf=False)
        if has_reparse(target):
            raise MigrationError(f"destination is a symlink/junction: {move.new}")
        if source.exists():
            if target.exists():
                raise MigrationError(f"destination already exists: {move.new}")
            if not source.is_file():
                raise MigrationError(f"move source is not a regular file: {move.old}")
            pending.add(move.old)
        elif target.is_file():
            completed.add(move.old)
        else:
            raise MigrationError(f"neither source nor migrated destination exists: {move.old} -> {move.new}")
    if pending and completed:
        raise MigrationError("partial migration detected; refusing to apply a mixed old/new tree")
    return pending, completed


# @unit scripts/layout/apply_moves.py::make_plan [function]
#
# ## Summary
# 把当前映射及内容快照序列化为可比较的有限计划记录。
#
# ## Parameters
# - root: 规范仓根。
# - table: 仓内映射表 Path。
# - moves: 完整映射列表。
# - rewrites: rel -> (before,after) 快照。
# - pending: 仍在旧路径的源名 set。
#
# ## Returns
# 返回新 dict；文件读取/相对化异常可传播。
#
# ## Constraints and lifecycle
# 记录仓根、表 hash、pending 源 hash 和排序的改写前/后 hash；不写 JSON、不锁源，也不是任意计划来源认证。
#
# ## References
# - scripts/layout/apply_moves.py:1779
# - scripts/layout/apply_moves.py:1781
# - scripts/layout/apply_moves.py:1782
# - scripts/layout/apply_moves.py:1783
# - scripts/layout/apply_moves.py:1784
# - scripts/layout/apply_moves.py:1785
# - scripts/layout/apply_moves.py:1789
# - scripts/layout/apply_moves.py:1791
# - scripts/layout/apply_moves.py:2703
def make_plan(root: Path, table: Path, moves: list[Move], rewrites: dict[str, tuple[bytes, bytes]], pending: set[str]) -> dict:
    return {
        "version": VERSION,
        "repository": str(root.resolve()),
        "mapping": repo_path(table, root),
        "mapping_sha256": sha256(table.read_bytes()),
        "moves": [
            {"old": m.old, "new": m.new, "sha256": sha256((root / m.old).read_bytes())}
            for m in moves if m.old in pending
        ],
        "rewrites": [
            {"path": rel, "sha256": sha256(before), "planned_sha256": sha256(after)}
            for rel, (before, after) in sorted(rewrites.items())
        ],
    }


# @unit scripts/layout/apply_moves.py::validate_plan [function]
#
# ## Summary
# 在应用前要求磁盘 JSON 与本次重新计算计划逐值一致。
#
# ## Parameters
# - plan_path: 调用方选定的计划文件。
# - expected: 本次 make_plan 得出的有限 dict。
#
# ## Returns
# 一致时 None；读取/JSON语法错误或不相等抛 MigrationError。
#
# ## Constraints and lifecycle
# 精确结构比较包含 hash；不把摘要当签名，也不认证计划文件来源。UnicodeDecodeError 等非列举异常不一概包装。
#
# ## References
# - scripts/layout/apply_moves.py:1818
# - scripts/layout/apply_moves.py:1820
# - scripts/layout/apply_moves.py:1821
# - scripts/layout/apply_moves.py:1823
# - scripts/layout/apply_moves.py:1824
# - scripts/layout/apply_moves.py:2718
def validate_plan(plan_path: Path, expected: dict) -> None:
    try:
        pinned = json.loads(plan_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise MigrationError(f"cannot read plan {plan_path}: {exc}") from exc
    if pinned != expected:
        raise MigrationError("pinned plan is stale: mapping, move inputs, or rewrite inputs changed")


# @unit scripts/layout/apply_moves.py::validate_applied_plan [function]
#
# ## Summary
# 在已完全移动的树上验证原计划的目的和改写后内容。
#
# ## Parameters
# - plan_path: 原冻结计划 JSON。
# - root: 当前仓根。
# - table: 当前映射表。
# - moves: 完整 Move 列表。
#
# ## Returns
# 全部一致返回 None；已核字段/文件不一致抛 MigrationError。
#
# ## Constraints and lifecycle
# 只在 main 的 completed 分支用于重入；先核版本/根/表 hash/完整旧名集合，再核新路径与 planned hash。依赖计划 schema，异常类型/重复行等不是通用不可信 JSON 防护；无文件写入。
#
# ## References
# - scripts/layout/apply_moves.py:1856
# - scripts/layout/apply_moves.py:1861
# - scripts/layout/apply_moves.py:1865
# - scripts/layout/apply_moves.py:1866
# - scripts/layout/apply_moves.py:1871
# - scripts/layout/apply_moves.py:1872
# - scripts/layout/apply_moves.py:1874
# - scripts/layout/apply_moves.py:1875
# - scripts/layout/apply_moves.py:1877
# - scripts/layout/apply_moves.py:1880
# - scripts/layout/apply_moves.py:2714
def validate_applied_plan(plan_path: Path, root: Path, table: Path, moves: list[Move]) -> None:
    try:
        pinned = json.loads(plan_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise MigrationError(f"cannot read plan {plan_path}: {exc}") from exc
    if (pinned.get("version") != VERSION or pinned.get("repository") != str(root.resolve())
            or pinned.get("mapping") != repo_path(table, root)
            or pinned.get("mapping_sha256") != sha256(table.read_bytes())):
        raise MigrationError("pinned plan is stale: repository or mapping changed")
    move_rows = {row.get("old"): row for row in pinned.get("moves", [])}
    if set(move_rows) != {move.old for move in moves}:
        raise MigrationError("pinned plan does not contain this complete migration")
    rewritten_hashes = {row.get("path"): row.get("planned_sha256") for row in pinned.get("rewrites", [])}
    for move in moves:
        row = move_rows[move.old]
        target = root / move.new
        if row.get("new") != move.new or not target.is_file() or has_reparse(target):
            raise MigrationError(f"pinned plan does not match migrated destination: {move.new}")
        expected_hash = rewritten_hashes.get(move.old, row.get("sha256"))
        if sha256(target.read_bytes()) != expected_hash or (root / move.old).exists():
            raise MigrationError(f"pinned plan destination bytes are stale: {move.new}")
    for row in pinned.get("rewrites", []):
        target_rel = next((move.new for move in moves if move.old == row.get("path")), row.get("path"))
        target = root / target_rel
        if not target.is_file() or sha256(target.read_bytes()) != row.get("planned_sha256"):
            raise MigrationError(f"pinned plan rewrite output is stale: {target_rel}")


# @unit scripts/layout/apply_moves.py::atomic_write [function]
#
# ## Summary
# 用临时文件、fsync 和同卷 replace 发布一次内容改写，保留可表达的 mode 位。
#
# ## Parameters
# - path: 已有目标文件；其 stat/mode 与目录须受调用者控制。
# - data: 待完整写入 bytes。
# - temp_dir: 调用方已授权、与目标支持 os.replace 的专用临时根。
#
# ## Returns
# 成功返回 None；读写/chmod/replace/unlink 等异常可传播。
#
# ## Constraints and lifecycle
# 临时目录可创建；文件 context manager 关闭流，finally 尝试 unlink 临时名，只吞 FileNotFoundError。单次 replace 不等于整批事务，不保证跨卷、所有元数据、崩溃恢复或并发写保护。
#
# ## References
# - scripts/layout/apply_moves.py:1912
# - scripts/layout/apply_moves.py:1913
# - scripts/layout/apply_moves.py:1914
# - scripts/layout/apply_moves.py:1915
# - scripts/layout/apply_moves.py:1917
# - scripts/layout/apply_moves.py:1919
# - scripts/layout/apply_moves.py:1920
# - scripts/layout/apply_moves.py:1921
# - scripts/layout/apply_moves.py:1922
# - scripts/layout/apply_moves.py:1923
# - scripts/layout/apply_moves.py:1926
def atomic_write(path: Path, data: bytes, temp_dir: Path) -> None:
    mode = stat.S_IMODE(path.stat().st_mode)
    temp_dir.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix="layout-01a13033-", suffix=".tmp", dir=temp_dir)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temp_name, mode)
        os.replace(temp_name, path)
    finally:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass


# @unit scripts/layout/apply_moves.py::apply [function]
#
# ## Summary
# 对已预检的整批文件先改名后改写，普通异常时进行带内容守门的尽力回滚。
#
# ## Parameters
# - root: 稳定且由调用者独占 mutation 的仓根。
# - moves: 全部仍为 pending 的已审定映射。
# - rewrites: 与当前文件逐字节一致的前/后快照。
# - temp_dir: 已由 main 验证范围/reparse 且支持目标 replace 的临时根。
# - plan: 与本批源 hash 一致的 make_plan 记录。
#
# ## Returns
# 成功返回 None；受 try 捕获的 Exception 转 MigrationError 并附回滚问题。
#
# ## Constraints and lifecycle
# 先读取全部源并核 hash、核 rewrite inputs，随后建目的目录、rename、核字节、逐文件 atomic_write。只回滚已记录且内容仍等于本批输出的项，不覆盖并发变更；目录 rmdir 失败被保留。BaseException/进程崩溃不在此回滚协议内；preflight 前置检查不可省略。
#
# ## References
# - scripts/layout/apply_moves.py:1969
# - scripts/layout/apply_moves.py:1970
# - scripts/layout/apply_moves.py:1973
# - scripts/layout/apply_moves.py:1976
# - scripts/layout/apply_moves.py:1981
# - scripts/layout/apply_moves.py:1983
# - scripts/layout/apply_moves.py:1990
# - scripts/layout/apply_moves.py:1997
# - scripts/layout/apply_moves.py:1999
# - scripts/layout/apply_moves.py:2001
# - scripts/layout/apply_moves.py:2007
# - scripts/layout/apply_moves.py:2009
# - scripts/layout/apply_moves.py:2011
# - scripts/layout/apply_moves.py:2013
# - scripts/layout/apply_moves.py:2015
# - scripts/layout/apply_moves.py:2021
# - scripts/layout/apply_moves.py:2025
# - scripts/layout/apply_moves.py:2031
# - scripts/layout/apply_moves.py:2036
# - scripts/layout/apply_moves.py:2039
def apply(root: Path, moves: list[Move], rewrites: dict[str, tuple[bytes, bytes]], temp_dir: Path, plan: dict) -> None:
    original_moves = {m.old: (root / m.old).read_bytes() for m in moves}
    expected_move_hashes = {row["old"]: row["sha256"] for row in plan["moves"]}
    for old, contents in original_moves.items():
        if sha256(contents) != expected_move_hashes.get(old):
            raise MigrationError(f"move source changed during apply preflight: {old}")
    for rel, (original, _) in rewrites.items():
        if not (root / rel).is_file() or (root / rel).read_bytes() != original:
            raise MigrationError(f"rewrite source changed during apply preflight: {rel}")
    created_dirs: list[Path] = []
    moved: list[Move] = []
    written: list[tuple[Path, bytes, bytes]] = []
    try:
        # Create all missing destination directories before the first rename.
        required = sorted({(root / m.new).parent for m in moves}, key=lambda p: len(p.parts))
        for directory in required:
            missing: list[Path] = []
            cursor = directory
            while not cursor.exists():
                missing.append(cursor)
                cursor = cursor.parent
            if not cursor.is_dir() or has_reparse(cursor):
                raise MigrationError(f"destination parent is not a safe directory: {cursor}")
            for item in reversed(missing):
                item.mkdir()
                created_dirs.append(item)
        for move in moves:
            source, target = root / move.old, root / move.new
            if not source.is_file() or target.exists() or has_reparse(target):
                raise MigrationError(f"move inputs changed during apply: {move.old} -> {move.new}")
            os.rename(source, target)
            moved.append(move)
            if sha256(target.read_bytes()) != expected_move_hashes[move.old]:
                raise MigrationError(f"move bytes changed during rename: {move.old} -> {move.new}")
        move_lookup = {m.old: m.new for m in moves}
        for rel, (original, updated) in sorted(rewrites.items()):
            target_rel = move_lookup.get(rel, rel)
            target = root / target_rel
            if not target.is_file() or target.read_bytes() != original:
                raise MigrationError(f"rewrite input changed during apply: {target_rel}")
            atomic_write(target, updated, temp_dir)
            written.append((target, original, updated))
    except Exception as exc:
        rollback_errors: list[str] = []
        for target, original, updated in reversed(written):
            try:
                if target.is_file() and sha256(target.read_bytes()) == sha256(updated):
                    atomic_write(target, original, temp_dir)
                else:
                    rollback_errors.append(f"changed rewrite target left untouched: {target}")
            except OSError as rollback_exc:
                rollback_errors.append(f"could not restore {target}: {rollback_exc}")
        for move in reversed(moved):
            source, target = root / move.old, root / move.new
            try:
                expected = original_moves[move.old]
                if target.is_file() and sha256(target.read_bytes()) == sha256(expected) and not source.exists():
                    os.rename(target, source)
                else:
                    rollback_errors.append(f"changed move target left untouched: {move.new}")
            except OSError as rollback_exc:
                rollback_errors.append(f"could not restore {move.old}: {rollback_exc}")
        for directory in reversed(created_dirs):
            try:
                directory.rmdir()
            except OSError:
                pass
        detail = f"apply failed and was rolled back: {exc}"
        if rollback_errors:
            detail += "; rollback issues: " + "; ".join(rollback_errors)
        raise MigrationError(detail) from exc


# @unit scripts/layout/apply_moves.py::residuals [function]
#
# ## Summary
# 在允许扫描域查找仍能被当前路径改写规则改变的旧 token。
#
# ## Parameters
# - root: 当前受控树。
# - moves: 本批映射。
#
# ## Returns
# 返回诊断字符串列表；读取失败作为列表项保留。
#
# ## Constraints and lifecycle
# 按文本规则比较，不证明任意二进制、Unicode路径或仓外引用无残留；不处理 include 的实际编译语义，也不清路径缓存。
#
# ## References
# - scripts/layout/apply_moves.py:2067
# - scripts/layout/apply_moves.py:2069
# - scripts/layout/apply_moves.py:2071
# - scripts/layout/apply_moves.py:2073
# - scripts/layout/apply_moves.py:2076
# - scripts/layout/apply_moves.py:2077
# - scripts/layout/apply_moves.py:2080
# - scripts/layout/apply_moves.py:2082
# - scripts/layout/apply_moves.py:2693
def residuals(root: Path, moves: list[Move]) -> list[str]:
    pairs = tuple((m.old, m.new) for m in moves)
    dir_pairs = directory_aliases(str(root.resolve()), pairs)
    found: list[str] = []
    for path in iter_files(root):
        rel = repo_path(path, root)
        if not is_scan_file(rel, path):
            continue
        try:
            data = path.read_bytes()
        except OSError as exc:
            found.append(f"{rel}: unreadable ({exc})")
            continue
        if replace_path_tokens(data, moves, root, dir_pairs) != data:
            found.append(f"{rel}: mapped old-path token remains")
    return found


# @unit scripts/layout/apply_moves.py::unresolved_includes [function]
#
# ## Summary
# 在有限源后缀中检查 quoted include 的上下文身份，单独处理允许的生成头。
#
# ## Parameters
# - root: 受控仓根。
# - context: 真实逐源 roots。
# - fallback_context: 独立 dormant roots。
# - build: 可选配置树；只授权精确生成头识别路径，不执行构建。
#
# ## Returns
# 返回问题字符串列表；部分读取/解码失败跳过，下层歧义或其它错误仍可抛出。
#
# ## Constraints and lifecycle
# 入口清路径缓存；标准头前缀名单跳过，正则不解释预处理条件。精确 zr_vm_ffi_fixture_path.h 在变体身份一致且为 repository/generated 时可通过，不读生成内容；混合身份形成问题。其它 include 必须能按有限根解析，结论仅覆盖实际枚举/读取范围。
#
# ## References
# - scripts/layout/apply_moves.py:2117
# - scripts/layout/apply_moves.py:2120
# - scripts/layout/apply_moves.py:2124
# - scripts/layout/apply_moves.py:2126
# - scripts/layout/apply_moves.py:2130
# - scripts/layout/apply_moves.py:2135
# - scripts/layout/apply_moves.py:2137
# - scripts/layout/apply_moves.py:2140
# - scripts/layout/apply_moves.py:2141
# - scripts/layout/apply_moves.py:2142
# - scripts/layout/apply_moves.py:2144
# - scripts/layout/apply_moves.py:2146
# - scripts/layout/apply_moves.py:2150
# - scripts/layout/apply_moves.py:2152
def unresolved_includes(root: Path, context: dict[str, tuple[Path, ...]] | None = None,
                        fallback_context: dict[str, tuple] | None = None,
                        build: Path | None = None) -> list[str]:
    clear_path_caches()
    problems: list[str] = []
    for path in iter_files(root):
        rel = repo_path(path, root)
        if not rel.startswith(("zr_vm_", "tests/", "scripts/")) or excluded(rel):
            continue
        if path.suffix.lower() not in {".c", ".h", ".cc", ".cpp", ".hpp", ".inc", ".def"}:
            continue
        try:
            data = path.read_bytes()
        except OSError:
            continue
        for match in INCLUDE_RE.finditer(data):
            try:
                name = match.group("name").decode("utf-8")
            except UnicodeDecodeError:
                continue
            if name.startswith(("stdio.h", "stdint.h", "stdlib.h", "stdbool.h", "stddef.h", "string.h", "assert.h", "errno.h", "math.h", "pthread.h", "windows.h", "wchar.h", "limits.h", "time.h", "stdatomic.h", "inttypes.h", "signal.h", "setjmp.h", "stdarg.h", "float.h", "ctype.h", "locale.h", "fcntl.h", "unistd.h", "sys/", "netinet/", "arpa/", "winsock2.h", "ws2tcpip.h", "process.h", "io.h", "direct.h", "intrin.h", "sal.h", "emmintrin.h", "xmmintrin.h")):
                continue
            variants = context_variants(context, rel) or context_variants(fallback_context, rel)
            if build is not None and name == "zr_vm_ffi_fixture_path.h" and variants:
                generated = layout_helper("generated_includes")
                outcomes = generated.classify_include(root, build, rel, name, variants)
                identities = {(item.kind, item.path) for item in outcomes}
                if len(identities) == 1 and outcomes[0].kind in {"repository", "generated"}:
                    continue
                if any(item.kind != "unresolved" for item in outcomes):
                    detail = [(item.kind, str(item.path) if item.path else None) for item in outcomes]
                    problems.append(f"{rel}: unsafe or inconsistent quoted include {name} across compile contexts: {detail}")
                    continue
            if resolve_include(root, rel, name, {}, context, fallback_context) is None:
                status = "actual" if context_variants(context, rel) else "dormant" if variants else "unverified"
                problems.append(f"{rel}: unresolved quoted include {name} (context: {status})")
    return problems


# @unit scripts/layout/apply_moves.py::load_cmake_fallback_context [function]
#
# ## Summary
# 从 dormant CMake 字面注册恢复单个消费者的有限 include 根，不执行 CMake。
#
# ## Parameters
# - root_text: 根路径文本，亦参与 lru_cache 键。
# - wanted_sources: 需要恢复的逻辑源名元组；只有唯一注册可发布。
#
# ## Returns
# 返回 source -> (ordered_roots,)；注册歧义或无字面根证据抛 MigrationError。
#
# ## Constraints and lifecycle
# 有限 regex/分词不解释条件、括号字符串/复杂表达式，# 注释剥除也不是完整 CMake 词法。只允许有限变量前缀/仓内根；wrapper 需证明调用共享 helper 才借字面公共根。缓存须随文件变化清除；真实 File API 不由这里覆盖或并集。
#
# ## References
# - scripts/layout/apply_moves.py:2189
# - scripts/layout/apply_moves.py:2190
# - scripts/layout/apply_moves.py:2198
# - scripts/layout/apply_moves.py:2199
# - scripts/layout/apply_moves.py:2200
# - scripts/layout/apply_moves.py:2247
# - scripts/layout/apply_moves.py:2251
# - scripts/layout/apply_moves.py:2253
# - scripts/layout/apply_moves.py:2262
# - scripts/layout/apply_moves.py:2269
# - scripts/layout/apply_moves.py:2274
# - scripts/layout/apply_moves.py:2286
# - scripts/layout/apply_moves.py:2306
# - scripts/layout/apply_moves.py:2312
# - scripts/layout/apply_moves.py:2320
# - scripts/layout/apply_moves.py:2340
# - scripts/layout/apply_moves.py:2342
@lru_cache(maxsize=2)
def load_cmake_fallback_context(root_text: str, wanted_sources: tuple[str, ...]) -> dict[str, tuple[tuple[Path, ...], ...]]:
    """Read literal source/root pairs from dormant test CMake fragments.

    This understands only direct source arguments and literal target include
    roots. It does not evaluate CMake or widen actual File API contexts.
    """
    root = Path(root_text)
    wanted = set(wanted_sources)
    command_re = re.compile(r"(?is)(?<![\w])([a-z_]\w*)\s*\(([^()]*)\)")
    token_re = re.compile(r'"[^"\r\n]*"|[^\s]+')
    source_commands = {"zr_vm_add_unity_test_target", "zr_vm_add_support_target", "add_executable"}
    registrations: dict[str, list[tuple[Path, str, str]]] = {}

    # @unit scripts/layout/apply_moves.py::load_cmake_fallback_context.cmake_path [function]
    #
    # ## Summary
    # 把有限 CMake token 转为根内候选 Path，供 dormant 字面恢复器使用。
    #
    # ## Parameters
    # - token: 已分词的字符串，可带普通双引号。
    # - fragment: token 所属 CMake 文件，用于 CURRENT_SOURCE_DIR/相对路径。
    #
    # ## Returns
    # 可解释时返回非严格 canonical Path；未知变量返回 None。
    #
    # ## Constraints and lifecycle
    # 仅识别 SOURCE_DIR/CURRENT_SOURCE_DIR 两前缀；不展开其它变量/generator expression，不在本函数单独保证仓内/存在性，外层 relative_to 过滤。
    #
    # ## References
    # - scripts/layout/apply_moves.py:2230
    # - scripts/layout/apply_moves.py:2231
    # - scripts/layout/apply_moves.py:2232
    # - scripts/layout/apply_moves.py:2235
    # - scripts/layout/apply_moves.py:2238
    # - scripts/layout/apply_moves.py:2241
    # - scripts/layout/apply_moves.py:2243
    # - scripts/layout/apply_moves.py:2245
    # - scripts/layout/apply_moves.py:2262
    # - scripts/layout/apply_moves.py:2295
    # - scripts/layout/apply_moves.py:2330
    def cmake_path(token: str, fragment: Path) -> Path | None:
        value = token.strip().strip('"').replace("\\", "/")
        if "$" in value or "$<" in value:
            prefix = "$" + "{CMAKE_SOURCE_DIR}/"
            current = "$" + "{CMAKE_CURRENT_SOURCE_DIR}/"
            if value.startswith(prefix):
                value = value[len(prefix):]
                return canonical_path(root / value, False)
            if value.startswith(current):
                value = value[len(current):]
                return canonical_path(fragment.parent / value, False)
            return None
        path = Path(value)
        if path.is_absolute():
            return canonical_path(path, False)
        return canonical_path(fragment.parent / path, False)

    fragments = sorted((root / "tests/cmake").rglob("*.cmake"))
    for fragment in fragments:
        try:
            text = fragment.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        text = re.sub(r"(?m)#.*$", "", text)
        for match in command_re.finditer(text):
            if match.group(1).lower() not in source_commands:
                continue
            tokens = [token.strip('"') for token in token_re.findall(match.group(2))]
            if not tokens:
                continue
            target = tokens[0]
            for token in tokens[1:]:
                path = cmake_path(token, fragment)
                if path is None:
                    continue
                try:
                    source_rel = repo_path(path, canonical_path(root, False))
                except ValueError:
                    continue
                if source_rel in wanted:
                    registrations.setdefault(source_rel, []).append((fragment, target, match.group(1).lower()))

    result: dict[str, tuple[tuple[Path, ...], ...]] = {}
    for source_rel, entries in registrations.items():
        if len(entries) != 1:
            locations = ", ".join(f"{path.relative_to(root).as_posix()}:{target}" for path, target, _kind in entries)
            raise MigrationError(f"ambiguous dormant CMake registration for {source_rel}: {locations}")
        _fragment, target, registration_kind = entries[0]
        roots: list[Path] = []
        root_evidence = 0
        for fragment in (entries[0][0],):
            try:
                text = re.sub(r"(?m)#.*$", "", fragment.read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError):
                continue
            for match in command_re.finditer(text):
                if match.group(1).lower() != "target_include_directories":
                    continue
                tokens = [token.strip('"') for token in token_re.findall(match.group(2))]
                if not tokens or tokens[0] != target:
                    continue
                root_evidence += 1
                for token in tokens[1:]:
                    if token.upper() in {"PRIVATE", "PUBLIC", "INTERFACE", "SYSTEM"}:
                        continue
                    path = cmake_path(token, fragment)
                    if path is None:
                        continue
                    try:
                        path.relative_to(canonical_path(root, False))
                    except ValueError:
                        continue
                    if path not in roots:
                        roots.append(path)
        # Wrapper targets inherit only the literal roots from the verified
        # shared test helper. Plain add_executable registrations stay local.
        if registration_kind in {"zr_vm_add_unity_test_target", "zr_vm_add_support_target"}:
            top_cmake = root / "tests/CMakeLists.txt"
            try:
                top_text = re.sub(r"(?m)#.*$", "", top_cmake.read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError):
                top_text = ""
            wrapper = re.search(
                rf"(?is)function\s*\(\s*{re.escape(registration_kind)}\b(.*?)\bendfunction\s*\(\s*\)",
                top_text,
            )
            helper = re.search(
                r"(?is)function\s*\(\s*zr_vm_apply_common_test_settings\b(.*?)\bendfunction\s*\(\s*\)",
                top_text,
            )
            calls_helper = wrapper and re.search(
                r"(?i)zr_vm_apply_common_test_settings\s*\(\s*\$\{target_name\}\s*\)", wrapper.group(1))
            if calls_helper and helper:
                common_dirs = re.search(
                    r"(?is)target_include_directories\s*\(\s*\$\{target_name\}(.*?)\)", helper.group(1))
                if common_dirs:
                    tokens = [token.strip('"') for token in token_re.findall(common_dirs.group(1))]
                    for token in tokens[1:]:
                        if token.upper() in {"PRIVATE", "PUBLIC", "INTERFACE", "SYSTEM"}:
                            continue
                        path = cmake_path(token, top_cmake)
                        if path is None:
                            continue
                        try:
                            path.relative_to(canonical_path(root, False))
                        except ValueError:
                            continue
                        if path not in roots:
                            roots.append(path)
                        root_evidence += 1
        if not root_evidence or not roots:
            raise MigrationError(f"no literal repository include-root evidence for dormant CMake source {source_rel} ({target})")
        result[source_rel] = (tuple(roots),)
    return result


# @unit scripts/layout/apply_moves.py::contextless_mapped_include_consumers [function]
#
# ## Summary
# 找出无真实上下文且引用本批旧/新别名的测试消费者。
#
# ## Parameters
# - root: 受控仓根。
# - moves: 本批 Move 列表。
# - context: 真实逐源上下文，已有键跳过。
#
# ## Returns
# 返回排序、去重的测试源名元组。
#
# ## Constraints and lifecycle
# 扫描有限C家族后缀的字面 include；读失败跳过，解码采用 replacement。只检测候选名字，不证明实际活动编译或精确解析，不把 dormant 根传播到真实图。
#
# ## References
# - scripts/layout/apply_moves.py:2374
# - scripts/layout/apply_moves.py:2376
# - scripts/layout/apply_moves.py:2379
# - scripts/layout/apply_moves.py:2384
# - scripts/layout/apply_moves.py:2386
# - scripts/layout/apply_moves.py:2388
# - scripts/layout/apply_moves.py:2391
# - scripts/layout/apply_moves.py:2395
# - scripts/layout/apply_moves.py:2396
# - scripts/layout/apply_moves.py:2399
# - scripts/layout/apply_moves.py:2658
def contextless_mapped_include_consumers(root: Path, moves: list[Move],
                                         context: dict[str, tuple]) -> tuple[str, ...]:
    names = set(include_candidate_names(moves))
    for move in moves:
        names.update(path_aliases(move))
        names.update(path_aliases(Move(move.new, move.new)))
        private = module_private_root(move.new)
        if private:
            names.add(private[1])
    wanted: set[str] = set()
    for path in iter_files(root):
        rel = repo_path(path, root)
        if not rel.startswith("tests/") or excluded(rel) or rel in context:
            continue
        if path.suffix.lower() not in {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inc", ".def"}:
            continue
        try:
            data = path.read_bytes()
        except OSError:
            continue
        for match in INCLUDE_RE.finditer(data):
            name = match.group("name").decode("utf-8", errors="replace")
            if name in names:
                wanted.add(rel)
                break
    return tuple(sorted(wanted))


# @unit scripts/layout/apply_moves.py::load_cmake_context [function]
#
# ## Summary
# 读取已有 File API codemodel 的逐源有序 include 变体，不生成构建图。
#
# ## Parameters
# - build: 已存在的配置树，reply 文件须稳定且来自可信 CMake。
# - root: 期望的源根，须与 codemodel 一致。
# - configuration: 所选配置名，默认 Debug；单个未命名配置可回退。
#
# ## Returns
# 返回 source -> tuple(ordered_roots_variants)；列举的索引/JSON/配置错误转 MigrationError。
#
# ## Constraints and lifecycle
# 选最新 mtime index；根外 include 仅保留精确 build/tests_generated，保留词法路径给生成检查器拒 reparse。compileGroups 必须有合法 sourceIndexes，只绑定对应源，保护源排除。不是任意恶意 jsonFile 路径/schema/编码认证，stat/类型等未列举异常可传播；不调用 CMake。
#
# ## References
# - scripts/layout/apply_moves.py:2437
# - scripts/layout/apply_moves.py:2439
# - scripts/layout/apply_moves.py:2441
# - scripts/layout/apply_moves.py:2445
# - scripts/layout/apply_moves.py:2448
# - scripts/layout/apply_moves.py:2455
# - scripts/layout/apply_moves.py:2457
# - scripts/layout/apply_moves.py:2464
# - scripts/layout/apply_moves.py:2468
# - scripts/layout/apply_moves.py:2472
# - scripts/layout/apply_moves.py:2483
# - scripts/layout/apply_moves.py:2487
# - scripts/layout/apply_moves.py:2492
# - scripts/layout/apply_moves.py:2495
# - scripts/layout/apply_moves.py:2497
# - scripts/layout/apply_moves.py:2506
# - scripts/layout/apply_moves.py:2512
# - scripts/layout/apply_moves.py:2514
def load_cmake_context(build: Path, root: Path, configuration: str = "Debug") -> dict[str, tuple[tuple[Path, ...], ...]]:
    """Read per-source include roots from an existing CMake File API reply."""
    clear_path_caches()
    reply = build / ".cmake/api/v1/reply"
    indexes = sorted(reply.glob("index-*.json"), key=lambda p: p.stat().st_mtime_ns, reverse=True)
    if not indexes:
        raise MigrationError(f"no CMake File API reply in {reply}; configure with the codemodel query first")
    try:
        index = json.loads(indexes[0].read_text(encoding="utf-8"))
        codemodel_ref = next(obj["jsonFile"] for obj in index.get("objects", []) if obj.get("kind") == "codemodel")
        codemodel = json.loads((reply / codemodel_ref).read_text(encoding="utf-8"))
    except (OSError, KeyError, StopIteration, json.JSONDecodeError) as exc:
        raise MigrationError(f"invalid CMake File API index {indexes[0]}: {exc}") from exc
    source_root = canonical_path(root, False)
    context: dict[str, list[tuple[Path, ...]]] = {}
    configurations = codemodel.get("configurations", [])
    if not configurations:
        raise MigrationError("CMake File API codemodel has no configurations")
    named = [item for item in configurations if item.get("name") == configuration]
    if len(named) != 1:
        if len(configurations) == 1 and not configurations[0].get("name"):
            config = configurations[0]
        else:
            available = ", ".join(str(item.get("name", "<unnamed>")) for item in configurations)
            raise MigrationError(f"CMake configuration {configuration!r} unavailable; found: {available}")
    else:
        config = named[0]
    codemodel_source = Path(codemodel.get("paths", {}).get("source", str(source_root)))
    if not codemodel_source.is_absolute():
        codemodel_source = source_root / codemodel_source
    codemodel_source = canonical_path(codemodel_source, False)
    if codemodel_source != source_root:
        raise MigrationError(f"CMake File API belongs to a different source root: {codemodel_source}")
    for target_ref in config.get("targets", []):
        try:
            target = json.loads((reply / target_ref["jsonFile"]).read_text(encoding="utf-8"))
        except (OSError, KeyError, json.JSONDecodeError) as exc:
            raise MigrationError(f"invalid CMake target reply {target_ref}: {exc}") from exc
        for group in target.get("compileGroups", []):
            roots: list[Path] = []
            for include in group.get("includes", []):
                include_path = Path(include.get("path", ""))
                if not include_path.is_absolute():
                    include_path = codemodel_source / include_path
                # Preserve lexical roots so the generated-input checker can
                # reject linked ancestors instead of losing them to resolve().
                include_path = Path(os.path.abspath(include_path))
                try:
                    include_path.relative_to(source_root)
                except ValueError:
                    if include_path != Path(os.path.abspath(build / "tests_generated")):
                        continue
                if include_path not in roots:
                    roots.append(include_path)
            target_sources = target.get("sources", [])
            if "sourceIndexes" not in group:
                raise MigrationError(f"CMake compile group in {target_ref.get('jsonFile')} lacks sourceIndexes")
            indexes = group.get("sourceIndexes", [])
            if any(not isinstance(index, int) or index < 0 or index >= len(target_sources) for index in indexes):
                raise MigrationError(f"CMake compile group in {target_ref.get('jsonFile')} has invalid sourceIndexes")
            sources = [target_sources[index] for index in indexes]
            for source in sources:
                source_path = Path(source.get("path", ""))
                if not source_path.is_absolute():
                    source_path = codemodel_source / source_path
                try:
                    rel = repo_path(canonical_path(source_path, False), source_root)
                except (OSError, ValueError):
                    continue
                if excluded(rel):
                    continue
                variant = tuple(roots)
                variants = context.setdefault(rel, [])
                if variant not in variants:
                    variants.append(variant)
    if not context:
        raise MigrationError(f"CMake File API reply at {reply} contains no in-repository source/include contexts")
    return {rel: tuple(variants) for rel, variants in context.items()}


# @unit scripts/layout/apply_moves.py::propagate_include_context [function]
#
# ## Summary
# 只沿可解析 quoted include 边传递每个独立编译根变体。
#
# ## Parameters
# - root: 与输入 context 同一原树根。
# - context: 按逻辑源名组织的真实变体；值按只读协议使用。
#
# ## Returns
# 返回新顶层 dict；每个头可能含多个独立 roots 元组。
#
# ## Constraints and lifecycle
# 不把不同变体合并成根并集。使用 (including,roots) visited 保证有限图终止，读取失败无新边；保护/非扫描目标不传播。缓存清除不等同图冻结；下层解析歧义会抛出。
#
# ## References
# - scripts/layout/apply_moves.py:2545
# - scripts/layout/apply_moves.py:2547
# - scripts/layout/apply_moves.py:2548
# - scripts/layout/apply_moves.py:2549
# - scripts/layout/apply_moves.py:2556
# - scripts/layout/apply_moves.py:2557
# - scripts/layout/apply_moves.py:2563
# - scripts/layout/apply_moves.py:2565
# - scripts/layout/apply_moves.py:2571
# - scripts/layout/apply_moves.py:2575
# - scripts/layout/apply_moves.py:2578
# - scripts/layout/apply_moves.py:2581
def propagate_include_context(root: Path, context: dict[str, tuple]) -> dict[str, tuple]:
    """Propagate compile roots only along resolvable quoted-include edges."""
    clear_path_caches()
    result = dict(context)
    pending = [(rel, variant) for rel, variants in context.items() for variant in context_variants(context, rel)]
    visited: set[tuple[str, tuple[Path, ...]]] = set()
    parsed_includes: dict[str, tuple[str, ...]] = {}
    while pending:
        including, roots = pending.pop()
        if excluded(including):
            continue
        state = (including, roots)
        if state in visited:
            continue
        visited.add(state)
        names = parsed_includes.get(including)
        if names is None:
            try:
                data = (root / including).read_bytes()
            except OSError:
                parsed_includes[including] = ()
                continue
            names = tuple(match.group("name").decode("utf-8", errors="replace")
                          for match in INCLUDE_RE.finditer(data))
            parsed_includes[including] = names
        for name in names:
            resolved = resolve_include(root, including, name, {}, {including: (roots,)})
            if not resolved:
                continue
            target = resolved[0]
            if excluded(target) or not is_scan_file(target, root / target):
                continue
            variants = context_variants(result, target)
            if roots not in variants:
                result[target] = (*variants, roots)
                pending.append((target, roots))
    return result


# @unit scripts/layout/apply_moves.py::main [function]
#
# ## Summary
# 为预览、计划、检查和应用协调输入守门，区别普通返回与可传播异常。
#
# ## Parameters
# - argv: 显式 CLI 字符串列表，或 None 让 argparse 读取 sys.argv。
#
# ## Returns
# 正常/已完成返回0；检查发现问题返回1；捕获 MigrationError 返回2。argparse help/参数错误可 SystemExit，未包装 I/O/类型/编码异常可传播。
#
# ## Constraints and lifecycle
# root 仅实际 checkout 或登记 fixture 子树；临时根另验证包含性与 reparse。默认预览不改源码，但 --plan 可写 JSON。apply 前重扫比较，--check 只覆盖有限域；所有源/映射/配置/计划应稳定且独占 mutation，不写 Git index。调用 main 可触发文件移动；调用方须在明确授权的根内执行。
#
# ## References
# - scripts/layout/apply_moves.py:2629
# - scripts/layout/apply_moves.py:2630
# - scripts/layout/apply_moves.py:2632
# - scripts/layout/apply_moves.py:2634
# - scripts/layout/apply_moves.py:2636
# - scripts/layout/apply_moves.py:2640
# - scripts/layout/apply_moves.py:2645
# - scripts/layout/apply_moves.py:2651
# - scripts/layout/apply_moves.py:2654
# - scripts/layout/apply_moves.py:2657
# - scripts/layout/apply_moves.py:2667
# - scripts/layout/apply_moves.py:2677
# - scripts/layout/apply_moves.py:2692
# - scripts/layout/apply_moves.py:2693
# - scripts/layout/apply_moves.py:2698
# - scripts/layout/apply_moves.py:2704
# - scripts/layout/apply_moves.py:2706
# - scripts/layout/apply_moves.py:2708
# - scripts/layout/apply_moves.py:2712
# - scripts/layout/apply_moves.py:2714
# - scripts/layout/apply_moves.py:2718
# - scripts/layout/apply_moves.py:2720
# - scripts/layout/apply_moves.py:2722
# - scripts/layout/apply_moves.py:2724
# - scripts/layout/apply_moves.py:2727
# - scripts/layout/apply_moves.py:2733
# - scripts/layout/apply_moves.py:2735
# - scripts/layout/apply_moves.py:2744
# - scripts/layout/apply_moves.py:2745
# - scripts/layout/apply_moves.py:2747
def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--map", dest="map_path", type=Path, default=Path("scripts/layout/layout_map.tsv"))
    parser.add_argument("--dry-run", action="store_true", help="preview only (the default)")
    parser.add_argument("--apply", action="store_true", help="perform filesystem moves and rewrites")
    parser.add_argument("--check", action="store_true", help="check old-path residuals and quoted includes")
    parser.add_argument("--plan", type=Path, help="write/use a pinned JSON plan; with --apply, validate it first")
    parser.add_argument("--cmake-build", type=Path, help="configured CMake build tree with a File API codemodel reply")
    parser.add_argument("--cmake-config", default="Debug", help="CMake File API configuration (default: Debug)")
    parser.add_argument("--tmp-dir", type=Path, default=SAFE_TASK_TMP, help="registered task temporary area for atomic rewrite files")
    args = parser.parse_args(argv)
    root = (args.root or Path(__file__).resolve().parents[2]).resolve()
    try:
        actual_root = Path(__file__).resolve().parents[2]
        fixture_root = REGISTERED_FIXTURE_ROOT
        if args.root and root != actual_root:
            try:
                root.relative_to(fixture_root)
            except ValueError as exc:
                raise MigrationError(f"--root is restricted to this checkout or registered fixtures under {fixture_root}") from exc
        table = args.map_path if args.map_path.is_absolute() else root / args.map_path
        table = table.resolve(strict=True)
        moves = load_moves(table)
        validate_move_aliases(moves)
        context = load_cmake_context(args.cmake_build.resolve(), root, args.cmake_config) if args.cmake_build else {}
        raw_context = context
        fallback_context = {}
        if not (args.check and args.cmake_build):
            wanted_sources = contextless_mapped_include_consumers(root, moves, context)
            fallback_context = load_cmake_fallback_context(str(root.resolve()), wanted_sources) if wanted_sources else {}
        if context:
            print(f"loaded raw CMake File API contexts for {len(context)} source paths; propagating quoted-include contexts", file=sys.stderr)
            context = propagate_include_context(root, context)
            print(f"propagated include contexts to {len(context)} paths and {sum(len(context_variants(context, rel)) for rel in context)} variants", file=sys.stderr)
        if args.cmake_build:
            print(f"loaded CMake File API context for {len(context)} source files and {sum(len(v) for v in context.values())} source/include pairs")
        if args.check:
            if args.cmake_build:
                dormant = layout_helper("dormant_contexts")
                wanted_sources = tuple(
                    repo_path(path, root) for path in iter_files(root)
                    if path.suffix.lower() == ".c"
                    and repo_path(path, root).startswith("tests/")
                    and not excluded(repo_path(path, root))
                    and repo_path(path, root) not in context
                )
                try:
                    recovered = dormant.recover_contexts(
                        root, args.cmake_build.resolve(), wanted_sources,
                        args.cmake_config, raw_context=raw_context, exclude=excluded,
                    )
                except dormant.DormantContextError as exc:
                    raise MigrationError(str(exc)) from exc
                seeds = {rel: item.variants for rel, item in recovered.items() if item.status == "recovered"}
                fallback_context = {
                    rel: variants for rel, variants in
                    dormant.propagate_dormant_headers(root, seeds, exclude=excluded).items()
                    if rel not in context
                }
                unknown = sum(item.status == "unknown" for item in recovered.values())
                print(f"separate dormant contexts: {len(seeds)} sources, {len(fallback_context)} paths; "
                      f"{unknown} sources have no proven compile context", file=sys.stderr)
            pending, _ = preflight(root, moves)
            issues = residuals(root, moves) + unresolved_includes(root, context, fallback_context, args.cmake_build)
            if pending:
                issues.append(f"{len(pending)} mapped source paths are still present")
            if issues:
                print("\n".join(issues), file=sys.stderr)
                return 1
            print(f"check passed: {len(moves)} mappings; no permitted residuals or unresolved quoted includes")
            return 0
        pending, completed = preflight(root, moves)
        rewrites = scan_rewrites(root, moves, context, progress=True, fallback_context=fallback_context) if pending else {}
        plan = make_plan(root, table, moves, rewrites, pending)
        if args.plan and not args.apply:
            args.plan.parent.mkdir(parents=True, exist_ok=True)
            args.plan.write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8", newline="\n")
            print(f"pinned plan written: {args.plan}")
        if args.apply:
            if completed:
                if args.plan:
                    try:
                        validate_plan(args.plan, plan)
                    except MigrationError:
                        validate_applied_plan(args.plan, root, table, moves)
                print(f"already migrated: {len(completed)} mappings; no changes")
                return 0
            if args.plan:
                validate_plan(args.plan, plan)
            # Repeat all mutable-input checks immediately before the first rename.
            current_rewrites = scan_rewrites(root, moves, context, fallback_context=fallback_context)
            current = make_plan(root, table, moves, current_rewrites, pending)
            if current != plan:
                raise MigrationError("inputs changed during preflight; refusing to apply")
            tmp_dir = args.tmp_dir.resolve()
            safe_tmp = SAFE_TASK_TMP.resolve()
            try:
                tmp_dir.relative_to(safe_tmp)
            except ValueError as exc:
                raise MigrationError(f"--tmp-dir must be under registered task temp root {safe_tmp}") from exc
            for path in (tmp_dir, *tmp_dir.parents):
                if path == safe_tmp.parent:
                    break
                if has_reparse(path):
                    raise MigrationError(f"--tmp-dir contains a symlink/junction: {path}")
            apply(root, moves, rewrites, tmp_dir, plan)
            print(f"applied {len(moves)} file moves and {len(rewrites)} exact path/include rewrites")
        else:
            print(f"preview: {len(pending)} file moves, {len(rewrites)} files with rewrites; no writes performed")
            for move in moves:
                state = "already moved" if move.old in completed else "move"
                print(f"{state}: {move.old} -> {move.new}")
            for rel, (before, after) in sorted(rewrites.items()):
                print(f"rewrite: {rel} ({len(before)} -> {len(after)} bytes)")
        return 0
    except MigrationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
