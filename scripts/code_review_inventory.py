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


def worktree_paths() -> set[str]:
    """收集已跟踪和新文件，不遍历被忽略的构建目录。"""
    output = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=REPO_ROOT,
    )
    paths = {p.decode("utf-8", "surrogateescape").replace("\\", "/")
             for p in output.split(b"\0") if p}
    return {path for path in paths if (REPO_ROOT / path).is_file()}


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


def render(rows: list[dict[str, str]]) -> str:
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue()


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


def review_source_path(name: str) -> Path | None:
    """仅接受仓库内现存文件的规范相对路径。"""
    if "\\" in name or ":" in name or any(part in {"", ".", ".."} for part in name.split("/")):
        return None
    source_path = (REPO_ROOT / name).resolve()
    return source_path if source_path.is_relative_to(REPO_ROOT) and source_path.is_file() else None


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
