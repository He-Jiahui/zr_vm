#!/usr/bin/env python3
"""让审查文件清单与当前首方工作树保持一致。

清单用于覆盖率验收，不代替调用链证据。已跟踪文件可能仍是生成数据；
其他活跃会话新增的源码也不能因尚未跟踪而从审查范围消失。
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import subprocess
import sys
from pathlib import Path, PurePosixPath


# ## Summary
# 集中定义清单扫描的仓库根、TSV 字段、源文件与配置后缀，以及纳入/排除边界。
# 这些值共同决定哪些现存工作树文件会进入文件级清单，以及如何标注其角色。
# ## Constraints and lifecycle
# ROOT_CONFIG_FILES 只补入明确允许的根配置；GENERATED_FILES 是精确路径例外；
# SKIP_PARTS 按路径段排除，其中 expected/golden/snapshots 是名称规则，不足以证明内容由测试生成。
# TODO: 逐文件沿生成器和消费者核实这些名称命中项。当前 syntax_migration_inventory 的
# expected JSON 由 tests/scripts/test_syntax_migration_inventory.py 消费；前端 fixture 由
# tests/cli/syntax_migration_smoke.js 消费。确认人工维护项后再评估精确排除规则，
# 已检索到五个 JSON 内容被读取或比较；另一个 diagnostics.json 仅在
# tests/parser/test_syntax_reference_v1.c 的 required-file 检查中要求存在，维护归属仍待确认。
# 不要把整个 fixture 目录当成生成物，也不要在本次注释任务改变分类行为。
# FIELDS、COVERAGE_FIELDS 是 TSV 固定列契约，变更会影响清单兼容和批次校验。
# 配置后缀先于源码后缀分类。
# ## References
# classify 应用范围规则，load_existing 解析、render 序列化文件级列，两个 check 函数消费覆盖列。
# 范围依据见 docs/code-review/comment-standard.md 的“范围与清单”及“台账格式”。

REPO_ROOT = Path(__file__).resolve().parent.parent
INVENTORY = REPO_ROOT / "docs" / "code-review" / "inventory.tsv"
FIELDS = ("file", "module", "kind", "sha256", "status", "note")
COVERAGE_FIELDS = ("file", "unit", "kind", "callers", "intent", "constraints",
                   "evidence", "decision", "reason")
SOURCE_SUFFIXES = {
    ".c", ".cc", ".cpp", ".cs", ".css", ".cxx", ".def", ".gdb", ".h", ".hpp",
    ".html", ".inc", ".java", ".js", ".jsx", ".ll", ".lua", ".mjs",
    ".cjs", ".ps1", ".py", ".rs", ".sh", ".ts", ".tsx", ".zr",
    ".zri", ".zrp",
}
CONFIG_SUFFIXES = {".cmake", ".csproj", ".json", ".props", ".targets", ".toml", ".yaml", ".yml"}
SKIP_PARTS = {
    "third_party", "node_modules", "target", "build", "bin", "out",
    "dist", "generated", "__pycache__", "expected", "golden", "obj",
    "snapshots",
}
GENERATED_FILES = {
    "zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h",
}
ROOT_CONFIG_FILES = {"CMakeLists.txt", "zensical.toml", "requirements-docs.txt"}


# ## Summary
# 从 Git 索引与未跟踪列表取得候选路径，使尚未 stage 的并发首方源码也能被后续扫描。
# 此处只负责取当前工作树文件集合；产品范围筛选由 classify 单独执行。
# ## Parameters
# 无参数；仓库根固定为本模块加载时解析出的 REPO_ROOT。
# ## Returns
# 返回存在于磁盘上的仓库相对路径集合，并统一使用 / 分隔；删除文件不会留在结果中。
# ## Constraints and lifecycle
# --exclude-standard 会应用 Git 标准忽略来源，包括仓库/信息目录和用户排除规则；
# --cached 仍会返回已跟踪且命中忽略规则的路径；忽略过滤仅针对未跟踪项。
# 本函数不清空 core.excludesFile。Git 命令失败会向上传播，is_file() 只确认文件存在，
# 不证明它属于审查范围；路径筛选与模块归属由 expected_rows 的下一步完成。
# ## References
# 直接调用者是 expected_rows；其文件范围与忽略规则由本文件配置和注释规范共同约束。

def worktree_paths() -> set[str]:
    """收集已跟踪和新文件，不遍历被忽略的构建目录。"""
    output = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=REPO_ROOT,
    )
    paths = {p.decode("utf-8", "surrogateescape").replace("\\", "/")
             for p in output.split(b"\0") if p}
    return {path for path in paths if (REPO_ROOT / path).is_file()}


# ## Summary
# 将一个仓库相对路径归入首方模块和 code、test、script、fixture 或 config 角色，
# 供清单按可维护范围审查；它不检查文件内容，也不判断某个源文件是否参与当前构建。
# ## Parameters
# path 是使用 / 分隔的相对路径；调用方通常传入 worktree_paths 返回的路径。
# ## Returns
# 返回 (module, kind)；路径在根目录、生成物、锁文件或后缀规则之外时返回 None。
# ## Constraints and lifecycle
# 排除项按完整路径段匹配；根配置采用白名单。docs/plans 树、GENERATED_FILES 所列路径、
# .__codex_tmp_ 名称标记及锁文件均被跳过。配置后缀先判为 config；
# 只有 tests/fixtures/ 下的源码后缀归为 fixture，该目录的配置仍是 config，
# 其他 tests/ 源码为 test，scripts/ 与 .github/ 源码为 script。
# ## References
# 直接调用者是 expected_rows；完整范围定义见模块常量及 comment-standard.md 第 17–19 行。

def classify(path: str) -> tuple[str, str] | None:
    """按首方可维护边界分类，排除构建输出及提交进仓的临时工作副本。"""
    parts = PurePosixPath(path).parts
    if not parts:
        return None
    root = parts[0]
    if not (root.startswith("zr_vm_") or root in {"tests", "scripts", "docs", ".github"}
            or path in ROOT_CONFIG_FILES):
        return None
    if path in GENERATED_FILES or parts[:2] == ("docs", "plans"):
        return None
    if any(part in SKIP_PARTS for part in parts):
        return None
    name = parts[-1]
    if ".__codex_tmp_" in name:
        return None
    if name in {"package-lock.json", "Cargo.lock", "composer.lock"} or name.endswith("-lock.json"):
        return None
    suffix = PurePosixPath(path).suffix.lower()
    if path in ROOT_CONFIG_FILES or name in {"CMakeLists.txt", ".vscodeignore"} or suffix in CONFIG_SUFFIXES:
        kind = "config"
    elif suffix in SOURCE_SUFFIXES or name.endswith(".h.in"):
        kind = "fixture" if len(parts) > 1 and parts[:2] == ("tests", "fixtures") else "code"
    else:
        return None
    if root == "tests" and kind == "code":
        kind = "test"
    elif root in {"scripts", ".github"} and kind == "code":
        kind = "script"
    module = "/".join(parts[:2]) if root in {"tests", "docs"} and len(parts) > 1 else root
    return module, kind


# ## Summary
# 读取已有文件级清单并按仓库相对路径索引，作为重扫时的哈希比较输入。
# ## Parameters
# 无参数；读取位置由 INVENTORY 固定。
# ## Returns
# 返回 file 到 TSV 行字典的映射；清单不存在时返回空映射，后续文件将按待审处理。
# ## Constraints and lifecycle
# 接受当前六列格式或无 sha256 的旧五列格式，并要求状态为 pending/reviewed；
# 旧五列没有内容哈希，因此其旧 reviewed 状态不会通过 expected_rows 的哈希门槛。
# 表头或状态不合法时抛出 ValueError，文件读取和 CSV 错误也不会在此处吞掉。
# ## References
# 直接调用者是 main；状态是否保留最终由 expected_rows 对比文件内容哈希决定。


def load_existing() -> dict[str, dict[str, str]]:
    if not INVENTORY.exists():
        return {}
    with INVENTORY.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if tuple(reader.fieldnames or ()) not in {FIELDS, ("file", "module", "kind", "status", "note")}:
            raise ValueError(f"unexpected inventory header: {reader.fieldnames}")
        rows = {row["file"]: row for row in reader}
    for path, row in rows.items():
        if row["status"] not in {"pending", "reviewed"}:
            raise ValueError(f"invalid status for {path}: {row['status']}")
    return rows


# ## Summary
# 将现有清单状态与当前 Git 可见文件重新对齐，计算每个纳入文件的 SHA-256 和文件级结论。
# ## Parameters
# existing 是 load_existing 读取的按路径旧行映射；本函数不会原地修改它。
# ## Returns
# 按仓库相对路径排序返回固定字段行；新文件或哈希变化的文件为 pending。
# ## Constraints and lifecycle
# 仅当旧行 sha256 与当前原始字节哈希相同才保留旧状态；无哈希、内容变化或新路径均重置，
# note 则沿用非空旧值，否则使用 -。磁盘读取失败会传播；状态仅由内容一致性维持，
# 不能代替逐单元调用链审查，也不会在这里写回清单。
# ## References
# 直接调用者是 main；路径枚举和分类分别委托给 worktree_paths 与 classify。

def expected_rows(existing: dict[str, dict[str, str]]) -> list[dict[str, str]]:
    """保留未变化文件的审查结论；源码内容变化后要求重新核对。"""
    rows = []
    for path in sorted(worktree_paths()):
        result = classify(path)
        if result is None:
            continue
        module, kind = result
        previous = existing.get(path, {})
        digest = hashlib.sha256((REPO_ROOT / path).read_bytes()).hexdigest()
        status = previous.get("status", "pending") if previous.get("sha256") == digest else "pending"
        rows.append({"file": path, "module": module, "kind": kind,
                     "sha256": digest, "status": status,
                     "note": previous.get("note") or "-"})
    return rows


# ## Summary
# 将文件级行序列序列化为可稳定比较的 TSV 文本，供 sync 写盘及 check 做精确比较。
# ## Parameters
# rows 是待输出的字段字典序列；通常由已按路径排序的 expected_rows 提供。
# ## Returns
# 返回带固定表头和 LF 行尾的字符串；写入编码由 main 选择 UTF-8。
# ## Constraints and lifecycle
# 列次序严格取自 FIELDS；DictWriter 负责 TSV 引号/分隔符处理，额外字段会触发异常。
# 本函数只在内存中格式化，不创建目录或写文件。
# ## References
# 直接调用者是 main；其结果分别交给 INVENTORY.write_text 或与现有文本比较。

def render(rows: list[dict[str, str]]) -> str:
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue()


# ## Summary
# 对所有 coverage TSV 执行文件级最低覆盖检查，防止已标为完成的文件完全没有逐单元记录。
# ## Parameters
# known_files 是当前扫描得到的路径集合；reviewed_files 是其中状态为 reviewed 的路径集合。
# ## Returns
# 返回发现的问题列表；没有问题时为空，具体输出数量与退出码由 main 决定。
# ## Constraints and lifecycle
# 检查表头、已知文件、非空单元/种类、决策标签、no-comment 理由、TODO/BUG 证据及重复键，
# 并要求每个 reviewed_files 至少有一行。它不枚举源码定义、不核对 callers 方向或证据行号，
# 也不证明某文件的全部适用单元均已登记；这些仍须按规范独立审阅。
# ## References
# 直接调用者是 main --require-complete；逐单元语义要求见 comment-standard.md 第 23–32、93–95 行。

def check_coverage(known_files: set[str], reviewed_files: set[str]) -> list[str]:
    """确认已审文件至少有一条有效的逐单元审查记录。"""
    covered: set[str] = set()
    seen: set[tuple[str, str, str]] = set()
    problems: list[str] = []
    for path in sorted((INVENTORY.parent / "coverage").glob("*.tsv")):
        with path.open("r", encoding="utf-8", newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            if tuple(reader.fieldnames or ()) != COVERAGE_FIELDS:
                problems.append(f"{path}: invalid coverage header")
                continue
            for number, row in enumerate(reader, start=2):
                file = row["file"]
                key = (file, row["unit"], row["kind"])
                if file not in known_files or not row["unit"] or not row["kind"]:
                    problems.append(f"{path}:{number}: unknown file or empty unit/kind")
                if row["decision"] not in {"commented", "no-comment", "TODO", "BUG"}:
                    problems.append(f"{path}:{number}: invalid decision")
                if row["decision"] == "no-comment" and not row["reason"]:
                    problems.append(f"{path}:{number}: no-comment needs a reason")
                if row["decision"] in {"TODO", "BUG"} and not row["evidence"]:
                    problems.append(f"{path}:{number}: issue needs evidence")
                if key in seen:
                    problems.append(f"{path}:{number}: duplicate unit")
                seen.add(key)
                covered.add(file)
    for file in sorted(reviewed_files - covered):
        problems.append(f"{file}: no coverage row")
    return problems


# ## Summary
# 将 coverage 中的仓库相对路径解析为仓内现存文件，供文件字段和证据锚点共用路径门禁。
# ## Parameters
# name 是 / 分隔的相对路径文本；盘符、反斜杠及空段/点段/父段会被拒绝。
# ## Returns
# 返回解析后的现存文件 Path；路径不规范、越出仓库根或不是文件时返回 None。
# ## Constraints and lifecycle
# resolve() 会解析符号链接后再检查仓库边界，避免通过链接把证据指向仓外；
# 此门禁只验证路径与文件存在，不要求其出现在 inventory，也不判断锚点语义。
# ## References
# check_batch 分别用它校验台账的 file 字段和 evidence 中的源码路径。

def review_source_path(name: str) -> Path | None:
    """仅接受仓库内现存文件的规范相对路径。"""
    if "\\" in name or ":" in name or any(part in {"", ".", ".."} for part in name.split("/")):
        return None
    source_path = (REPO_ROOT / name).resolve()
    return source_path if source_path.is_relative_to(REPO_ROOT) and source_path.is_file() else None


# ## Summary
# 在采用新 coverage 批次前检查九列记录、单元重复和 path:line 锚点是否指向非空源码行。
# ## Parameters
# path 是待读 TSV 的路径；批次文件按 UTF-8 严格读取，本函数不写入它。
# ## Returns
# 返回格式、路径或锚点问题列表；成功时为空，批次读取异常转换为问题文本。
# ## Constraints and lifecycle
# 检查必填字段、列数、决策枚举、重复 (file, unit, kind)，并以 review_source_path 限定仓内现存文件；
# 每个分号分隔的锚点须含可解析行号，且该行非空。源码锚点以 errors="replace" 解码，
# 因此不校验源码编码；也不确认锚点与结论的语义关系、调用方向、单元完整性或 no-comment 理由。
# ## References
# 直接调用者是 main 的 check-batch 分支；人工复核责任见 comment-standard.md 第 93–95 行。


def check_batch(path: Path) -> list[str]:
    """检查新批次的逐单元台账及当前工作树中的行号证据。"""
    problems: list[str] = []
    seen: set[tuple[str, str, str]] = set()
    source_lines: dict[str, list[str]] = {}
    try:
        with path.open("r", encoding="utf-8", newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            if tuple(reader.fieldnames or ()) != COVERAGE_FIELDS:
                return [f"{path}: invalid coverage header"]
            for number, row in enumerate(reader, start=2):
                if None in row or any(
                    not (row.get(field) or "").strip() or
                    any(character in (row.get(field) or "") for character in "\t\r\n")
                    for field in COVERAGE_FIELDS
                ):
                    problems.append(f"{path}:{number}: missing or extra field")
                    continue
                key = (row["file"], row["unit"], row["kind"])
                if key in seen:
                    problems.append(f"{path}:{number}: duplicate unit")
                seen.add(key)
                if row["decision"] not in {"commented", "no-comment", "TODO", "BUG"}:
                    problems.append(f"{path}:{number}: invalid decision")
                if review_source_path(row["file"]) is None:
                    problems.append(f"{path}:{number}: unknown reviewed file")
                for anchor in row["evidence"].split(";"):
                    source, separator, line_text = anchor.strip().rpartition(":")
                    source_path = review_source_path(source)
                    if not separator or not line_text.isdecimal() or source_path is None:
                        problems.append(f"{path}:{number}: invalid evidence {anchor!r}")
                        continue
                    if source not in source_lines:
                        source_lines[source] = source_path.read_text(
                            encoding="utf-8", errors="replace").splitlines()
                    line = int(line_text)
                    if line < 1 or line > len(source_lines[source]) or not source_lines[source][line - 1].strip():
                        problems.append(f"{path}:{number}: empty or missing evidence line {anchor!r}")
    except (OSError, UnicodeError, csv.Error, ValueError) as error:
        return [f"{path}: {error}"]
    if not seen:
        problems.append(f"{path}: empty coverage batch")
    return problems


# ## Summary
# 提供 sync、check、check-batch 命令入口，把扫描、清单同步和完成门槛串成可审计流程。
# ## Parameters
# 参数由 argparse 从进程命令行解析：动作必填，批次路径只用于 check-batch，
# --require-complete 对 sync/check 附加无待审及逐单元覆盖要求。
# ## Returns
# 返回进程退出状态：成功为 0，清单过期或审查未完成为 1；参数错误由 argparse 终止并报错。
# ## Constraints and lifecycle
# check-batch 仅读批次与锚点源码；check 不写 INVENTORY，但会读取 Git 路径、候选源码哈希和清单。
# 若 check 发现清单过期，会在 --require-complete 的 coverage 检查前返回 1；
# 清单新鲜时该选项才继续读取 coverage TSV。sync 创建目录并写 INVENTORY；
# sync --require-complete 先写盘，再检查待审和 coverage；最终返回 1 时写入不会自动回滚。
# 已有模块 docstring 继续提供 argparse 描述，本注释不改变 __doc__。
# ## References
# 直接入口是文件末尾的 __main__ 守卫；命令用法见 comment-standard.md 第 21、95 行。


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("sync", "check", "check-batch"))
    parser.add_argument("batch", nargs="?", help="coverage TSV for check-batch")
    parser.add_argument("--require-complete", action="store_true",
                        help="also reject any pending file")
    args = parser.parse_args()
    if args.action == "check-batch":
        if args.require_complete:
            parser.error("--require-complete is not valid with check-batch")
        if not args.batch:
            parser.error("check-batch requires a coverage TSV")
        problems = check_batch(Path(args.batch))
        for problem in problems:
            print(problem, file=sys.stderr)
        if not problems:
            print(f"review batch: {args.batch} valid")
        return int(bool(problems))
    if args.batch:
        parser.error("batch path is only valid with check-batch")
    existing = load_existing()
    rows = expected_rows(existing)
    content = render(rows)
    if args.action == "sync":
        INVENTORY.parent.mkdir(parents=True, exist_ok=True)
        INVENTORY.write_text(content, encoding="utf-8", newline="\n")
    elif not INVENTORY.exists() or INVENTORY.read_text(encoding="utf-8") != content:
        print("review inventory is stale; run scripts/code_review_inventory.py sync", file=sys.stderr)
        return 1
    pending = sum(row["status"] == "pending" for row in rows)
    print(f"review inventory: {len(rows)} files, {pending} pending")
    if args.require_complete:
        known = {row["file"] for row in rows}
        reviewed = {row["file"] for row in rows if row["status"] == "reviewed"}
        problems = check_coverage(known, reviewed)
        for problem in problems[:20]:
            print(problem, file=sys.stderr)
        if len(problems) > 20:
            print(f"... and {len(problems) - 20} more coverage errors", file=sys.stderr)
        return int(pending != 0 or bool(problems))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
