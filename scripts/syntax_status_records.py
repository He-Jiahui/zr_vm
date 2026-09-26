from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


# 冻结的 55 份语法里程碑记录分布；校验器以它检测文档新增、删除或迁移造成的漂移。
EXPECTED_DIRECTORY_COUNTS = {
    "01": 5,
    "02": 6,
    "03": 5,
    "04": 7,
    "05": 6,
    "06": 2,
    "07": 1,
    "10": 5,
    "12": 15,
    "13": 3,
}
EXPECTED_RECORD_COUNT = 55

# 该任务是 05 目录的支撑计划，不作为独立里程碑状态记录计数。
_EXCLUDED_SUPPORT_RECORD = (
    "05-property-unified-ast/m5-task4-property-import-bootstrap.md"
)
# 状态与完成时间仅从 Markdown 列表行读取；中英文标签共用报告协议。
_STATUS_PATTERN = re.compile(
    r"^\s*-\s*(?:Status|\u72b6\u6001)\s*[:\uff1a]\s*(?P<value>.+?)\s*$",
    re.IGNORECASE,
)
_TIME_PATTERN = re.compile(
    r"^\s*-\s*(?:Completion\s+time|Completed\s+at|\u5b8c\u6210\u65f6\u95f4)"
    r"\s*[:\uff1a]\s*(?P<value>.+?)\s*$",
    re.IGNORECASE,
)
# 允许已完成状态带限定说明，仍由下方 validate 检查时间字段与总数。
_ENGLISH_COMPLETION_PATTERN = re.compile(r"^completed(?:\b|_)", re.IGNORECASE)


@dataclass(frozen=True)
class SyntaxStatusRecord:
    # 单个 Markdown 记录的相对路径、分组及原始状态字段；缺失值保留为 None 供校验报告。
    relative_path: str
    directory: str
    status: str | None
    completion_time: str | None

    @property
    def is_complete(self) -> bool:
        # 仅把明确写有“已完成”或 completed 前缀的状态计入冻结集合。
        if self.status is None:
            return False
        normalized = self.status.strip().strip("`").strip()
        return "\u5df2\u5b8c\u6210" in normalized or _ENGLISH_COMPLETION_PATTERN.match(
            normalized
        ) is not None


@dataclass(frozen=True)
class SyntaxStatusReport:
    # 收集结果同时保留缺失/未完成路径，使文本、JSON 与退出码共享同一组问题。
    records: tuple[SyntaxStatusRecord, ...]
    directory_counts: dict[str, int]
    missing_status: tuple[str, ...]
    non_complete: tuple[str, ...]
    missing_time: tuple[str, ...]

    @property
    def complete_count(self) -> int:
        # 汇总使用 record 的同一完成定义，避免主报告与逐记录判定分叉。
        return sum(record.is_complete for record in self.records)

    def to_json(self) -> str:
        # 机器消费方需要完整记录与分类问题；排序由 collect 的路径选择阶段确定。
        payload = {
            "schemaVersion": 1,
            "total": len(self.records),
            "complete": self.complete_count,
            "missingStatus": list(self.missing_status),
            "nonComplete": list(self.non_complete),
            "missingTime": list(self.missing_time),
            "directoryCounts": self.directory_counts,
            "records": [
                {
                    "path": record.relative_path,
                    "directory": record.directory,
                    "status": record.status,
                    "completionTime": record.completion_time,
                }
                for record in self.records
            ],
        }
        return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


# 只选编号目录中的里程碑记录；implementation plan 和指定支撑记录不在 55 份合同内。
def _selected_markdown_paths(status_root: Path) -> Iterable[Path]:
    if not status_root.is_dir():
        return ()

    selected: list[Path] = []
    for child in sorted(status_root.iterdir(), key=lambda path: path.name):
        if not child.is_dir() or not child.name or not child.name[0].isdigit():
            continue
        for path in child.rglob("*.md"):
            relative_path = path.relative_to(status_root).as_posix()
            if path.name.endswith("-implementation-plan.md"):
                continue
            if relative_path == _EXCLUDED_SUPPORT_RECORD:
                continue
            selected.append(path)
    return sorted(selected, key=lambda path: path.relative_to(status_root).as_posix())


# 对状态/时间取首个匹配列表项，避免正文中后续重复提及覆盖文件头记录。
def _first_match(lines: Iterable[str], pattern: re.Pattern[str]) -> str | None:
    for line in lines:
        match = pattern.match(line)
        if match is not None:
            return match.group("value").strip()
    return None


# CLI 和测试共用的只读收集入口；保留缺失字段而不在遍历中提前报错，方便一次显示全部漂移。
def collect_syntax_status_records(repository_root: Path) -> SyntaxStatusReport:
    status_root = repository_root / "docs" / "plans" / "syntax"
    records: list[SyntaxStatusRecord] = []
    directory_counts: dict[str, int] = {}

    for path in _selected_markdown_paths(status_root):
        relative_path = path.relative_to(status_root).as_posix()
        directory = relative_path.split("/", 1)[0].split("-", 1)[0]
        lines = path.read_text(encoding="utf-8").splitlines()
        record = SyntaxStatusRecord(
            relative_path=relative_path,
            directory=directory,
            status=_first_match(lines, _STATUS_PATTERN),
            completion_time=_first_match(lines, _TIME_PATTERN),
        )
        records.append(record)
        directory_counts[directory] = directory_counts.get(directory, 0) + 1

    missing_status = tuple(
        record.relative_path for record in records if record.status is None
    )
    non_complete = tuple(
        record.relative_path
        for record in records
        if record.status is not None and not record.is_complete
    )
    missing_time = tuple(
        record.relative_path for record in records if record.completion_time is None
    )
    return SyntaxStatusReport(
        records=tuple(records),
        directory_counts=dict(sorted(directory_counts.items())),
        missing_status=missing_status,
        non_complete=non_complete,
        missing_time=missing_time,
    )


# 将冻结数量/目录分布与每份状态合同一并核验；返回所有问题供 CLI 设置非零退出码。
def validate_syntax_status_records(report: SyntaxStatusReport) -> tuple[str, ...]:
    issues: list[str] = []
    if len(report.records) != EXPECTED_RECORD_COUNT:
        issues.append(
            f"expected {EXPECTED_RECORD_COUNT} records, found {len(report.records)}"
        )
    if report.directory_counts != EXPECTED_DIRECTORY_COUNTS:
        issues.append(
            "directory distribution differs: "
            f"expected {EXPECTED_DIRECTORY_COUNTS}, found {report.directory_counts}"
        )
    if report.missing_status:
        issues.append("missing status: " + ", ".join(report.missing_status))
    if report.non_complete:
        issues.append("non-complete status: " + ", ".join(report.non_complete))
    if report.missing_time:
        issues.append("missing completion time: " + ", ".join(report.missing_time))
    return tuple(issues)


# 人工模式用固定键和 ERROR 行输出，不隐藏任何发现的问题。
def _format_text(report: SyntaxStatusReport, issues: tuple[str, ...]) -> str:
    distribution = " ".join(
        f"{directory}={count}"
        for directory, count in report.directory_counts.items()
    )
    lines = [
        f"TOTAL={len(report.records)}",
        f"COMPLETE={report.complete_count}",
        f"MISSING_STATUS={len(report.missing_status)}",
        f"NON_COMPLETE={len(report.non_complete)}",
        f"MISSING_TIME={len(report.missing_time)}",
        distribution,
    ]
    lines.extend(f"ERROR={issue}" for issue in issues)
    return "\n".join(lines) + "\n"


# 文档状态 gate 的命令行入口：JSON/text 仅改变呈现，退出码始终反映同一次 validate 结果。
def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify the frozen docs/plans/syntax 55-record status set."
    )
    parser.add_argument(
        "--repository",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the parent of scripts/)",
    )
    parser.add_argument("--format", choices=("text", "json"), default="text")
    arguments = parser.parse_args(argv)

    report = collect_syntax_status_records(arguments.repository.resolve())
    issues = validate_syntax_status_records(report)
    if arguments.format == "json":
        sys.stdout.write(report.to_json())
    else:
        sys.stdout.write(_format_text(report, issues))
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
