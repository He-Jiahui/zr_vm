#!/usr/bin/env python3
"""Read-only inventory protocol for Syntax 06A legacy migration forms."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from dataclasses import asdict, dataclass, replace
from enum import Enum
from pathlib import Path
from typing import Iterable

from syntax_migration_embedded import iter_embedded_string_payloads as _iter_embedded_string_payloads


# JSON 报告中保留扫描来源，使消费方区分真实 .zr、宿主字面量与当前文档示例。
class SourceKind(str, Enum):
    ZR_SOURCE = "zrSource"
    EMBEDDED_ZR_FIXTURE = "embeddedZrFixture"
    CURRENT_DOCUMENTATION = "currentDocumentation"


# 分类只记录迁移责任与可自动化程度；扫描器本身不改写输入或批准迁移。
class MigrationClassification(str, Enum):
    MACHINE_APPLICABLE = "machineApplicable"
    MAYBE_INCORRECT = "maybeIncorrect"
    REQUIRES_REVIEW = "requiresReview"
    BLOCKED = "blocked"
    TARGET_NOT_PROMOTED = "targetNotPromoted"


@dataclass(frozen=True)
class SourcePosition:
    # 对外统一使用 1-based 行列，供 fixture golden 与编辑器定位消费。
    line: int
    column: int


@dataclass(frozen=True)
class SourceRange:
    # finding 的起止位置必须在同一来源文本坐标系中，嵌入式宿主输入尤其依赖此契约。
    start: SourcePosition
    end: SourcePosition


@dataclass(frozen=True)
class ScannedFile:
    # 已扫描路径单独列出，即使没有发现旧语法也可证明它进入了候选边界。
    file: str
    source_kind: SourceKind


@dataclass(frozen=True)
class InventoryFinding:
    # 将旧语法位置、分类、目标计划和稳定理由捆绑，避免报告只给出裸文本命中。
    file: str
    source_kind: SourceKind
    source_range: SourceRange
    legacy_form: str
    classification: MigrationClassification
    target_plan: str
    reason: str


@dataclass(frozen=True)
class InventoryExclusion:
    # 被选择却不扫描的文件保留可审计原因，而非从报告静默消失。
    file: str
    reason: str


@dataclass(frozen=True)
class InventoryAllowlistedFinding:
    # 负向测试中的预期旧写法仍输出到独立列表，不计入待迁移 findings。
    file: str
    line: int
    column: int
    legacy_form: str
    reason: str


@dataclass(frozen=True)
class InventoryReviewedCurrentFinding:
    # 已晋升为当前语法的候选保留审查痕迹，不再误报为 legacy finding。
    file: str
    source_kind: SourceKind
    source_range: SourceRange
    syntax_form: str
    reason: str


@dataclass(frozen=True)
class MigrationRule:
    # 规则表只决定已识别形式的归类，具体绑定/所有权证明留给目标计划。
    classification: MigrationClassification
    target_plan: str
    reason: str


@dataclass(frozen=True)
class InventoryReport:
    # 同一快照支持机器 JSON、人工文本和 fixture 合同三种投影；字段顺序保持稳定。
    scanned_files: tuple[ScannedFile, ...]
    exclusions: tuple[InventoryExclusion, ...]
    findings: tuple[InventoryFinding, ...]
    allowlisted_findings: tuple[InventoryAllowlistedFinding, ...] = ()
    reviewed_current_findings: tuple[InventoryReviewedCurrentFinding, ...] = ()
    scanner_version: str = "1"
    selected_roots: tuple[str, ...] = ()

    # CLI 默认输出；对路径和 finding 排序由构造方保证，输出固定 LF 供跨主机基线比较。
    def to_json(self) -> str:
        return json.dumps(
            {
                "schemaVersion": 1,
                "scannerVersion": self.scanner_version,
                "selectedRoots": list(self.selected_roots),
                "unknownCount": 0,
                "scannedFiles": [
                    {"file": entry.file, "sourceKind": entry.source_kind.value}
                    for entry in self.scanned_files
                ],
                "exclusions": [asdict(entry) for entry in self.exclusions],
                "allowlistedFindings": [
                    {
                        "file": entry.file,
                        "line": entry.line,
                        "column": entry.column,
                        "legacyForm": entry.legacy_form,
                        "reason": entry.reason,
                    }
                    for entry in self.allowlisted_findings
                ],
                "reviewedCurrentFindings": [
                    {
                        "file": entry.file,
                        "sourceKind": entry.source_kind.value,
                        "range": {
                            "start": asdict(entry.source_range.start),
                            "end": asdict(entry.source_range.end),
                        },
                        "syntaxForm": entry.syntax_form,
                        "reason": entry.reason,
                    }
                    for entry in self.reviewed_current_findings
                ],
                "findings": [
                    {
                        "file": entry.file,
                        "sourceKind": entry.source_kind.value,
                        "range": {
                            "start": asdict(entry.source_range.start),
                            "end": asdict(entry.source_range.end),
                        },
                        "legacyForm": entry.legacy_form,
                        "classification": entry.classification.value,
                        "targetPlan": entry.target_plan,
                        "reason": entry.reason,
                    }
                    for entry in self.findings
                ],
                "classificationCounts": _classification_counts(self.findings),
                "targetPlanCounts": _target_plan_counts(self.findings),
            },
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        ) + "\n"

    # 人工审阅模式突出分类数量与每个位置，避免必须解析完整 JSON 才能定位迁移点。
    def to_text(self) -> str:
        lines = [
            f"Syntax 06A migration inventory (scanner {self.scanner_version})",
            f"selected roots: {', '.join(self.selected_roots)}",
            f"scanned files: {len(self.scanned_files)}",
            f"excluded files: {len(self.exclusions)}",
            f"allowlisted findings: {len(self.allowlisted_findings)}",
            f"reviewed current findings: {len(self.reviewed_current_findings)}",
            f"findings: {len(self.findings)}",
            "classification counts:",
        ]
        lines.extend(
            f"  {name}: {count}"
            for name, count in _classification_counts(self.findings).items()
        )
        lines.append("target plan counts:")
        lines.extend(
            f"  {name}: {count}"
            for name, count in _target_plan_counts(self.findings).items()
        )
        lines.append("findings:")
        lines.extend(
            "  {file}:{line}:{column} {form} -> {classification} "
            "({target_plan}; {reason})".format(
                file=finding.file,
                line=finding.source_range.start.line,
                column=finding.source_range.start.column,
                form=finding.legacy_form,
                classification=finding.classification.value,
                target_plan=finding.target_plan,
                reason=finding.reason,
            )
            for finding in self.findings
        )
        return "\n".join(lines) + "\n"

    # 合成 fixture 只冻结每种旧形式的单一合同；相同形式冲突应使基线构建失败。
    def to_fixture_json(self) -> str:
        contracts: dict[str, tuple[str, str, str]] = {}
        for finding in self.findings:
            contract = (
                finding.classification.value,
                finding.target_plan,
                finding.reason,
            )
            previous = contracts.setdefault(finding.legacy_form, contract)
            if previous != contract:
                raise ValueError(
                    f"fixture form has conflicting contracts: {finding.legacy_form}"
                )
        return json.dumps(
            {
                "schemaVersion": 1,
                "classificationCounts": _classification_counts(self.findings),
                "contracts": [
                    {
                        "legacyForm": legacy_form,
                        "classification": contract[0],
                        "targetPlan": contract[1],
                        "reason": contract[2],
                    }
                    for legacy_form, contract in sorted(contracts.items())
                ],
            },
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        ) + "\n"


# build_inventory 的合成 fixture 边界：embedded/docs 目录名用于区分宿主片段与文档，
# 不等同于下方 repository 模式的全仓 Git 候选策略。
def _source_kind_for(path: Path, relative_path: Path) -> SourceKind | None:
    if path.suffix == ".zr":
        return SourceKind.ZR_SOURCE
    if path.suffix in {".c", ".cc", ".cpp", ".h"} and "embedded" in relative_path.parts:
        return SourceKind.EMBEDDED_ZR_FIXTURE
    if path.suffix == ".md" and "docs" in relative_path.parts:
        return SourceKind.CURRENT_DOCUMENTATION
    return None


# 两种报告投影共用分类统计；零命中的分类仍保留，保证 JSON schema 稳定。
def _classification_counts(
    findings: tuple[InventoryFinding, ...],
) -> dict[str, int]:
    counts = {classification.value: 0 for classification in MigrationClassification}
    for finding in findings:
        counts[finding.classification.value] += 1
    return counts


# 将归属计划汇总给人工迁移安排，输出排序不受发现文件次序影响。
def _target_plan_counts(
    findings: tuple[InventoryFinding, ...],
) -> dict[str, int]:
    counts: dict[str, int] = {}
    for finding in findings:
        counts[finding.target_plan] = counts.get(finding.target_plan, 0) + 1
    return dict(sorted(counts.items()))


# 合成 fixture 模式允许扫描未跟踪文件；仓库 CLI 使用 Git 候选列表，不调用此遍历。
def _iter_files(root: Path) -> Iterable[tuple[Path, Path]]:
    for path in root.rglob("*"):
        if path.is_file():
            yield path, path.relative_to(root)


# 以基点把片段内 offset 投影为对外行列；仅首行继承 base.column。
def _position_at(
    text: str,
    offset: int,
    base: SourcePosition = SourcePosition(line=1, column=1),
) -> SourcePosition:
    preceding = text[:offset]
    newline_count = preceding.count("\n")
    if newline_count == 0:
        return SourcePosition(line=base.line, column=base.column + offset)
    return SourcePosition(
        line=base.line + newline_count,
        column=offset - preceding.rfind("\n"),
    )


# _append_finding 统一建立范围，避免各正则路径自行实现坐标换算。
def _range_for(
    text: str,
    start: int,
    end: int,
    base: SourcePosition,
) -> SourceRange:
    return SourceRange(
        start=_position_at(text, start, base),
        end=_position_at(text, end, base),
    )


# 保留字符长度与换行后屏蔽注释/字符串，使后续正则不把说明文字或字符串内容当旧语法。
# 它是迁移清单词法边界，不承担完整 Zr parser 的嵌套语法判断。
def _code_mask(text: str) -> str:
    masked = list(text)
    offset = 0
    while offset < len(text):
        current = text[offset]
        following = text[offset + 1] if offset + 1 < len(text) else ""
        if current == "/" and following == "/":
            end = text.find("\n", offset)
            if end < 0:
                end = len(text)
            for index in range(offset, end):
                masked[index] = " "
            offset = end
            continue
        if current == "/" and following == "*":
            end = text.find("*/", offset + 2)
            end = len(text) if end < 0 else end + 2
            for index in range(offset, end):
                if masked[index] != "\n":
                    masked[index] = " "
            offset = end
            continue
        if current in {'"', "'", "`"}:
            quote = current
            end = offset + 1
            while end < len(text):
                if text[end] == "\\":
                    end += 2
                    continue
                if text[end] == quote:
                    end += 1
                    break
                end += 1
            for index in range(offset, min(end, len(text))):
                if masked[index] != "\n":
                    masked[index] = " "
            offset = end
            continue
        offset += 1
    return "".join(masked)


# 已识别形式必须出现在规则表中；报告位置来自原文本，归类来自唯一合同。
def _append_finding(
    findings: list[InventoryFinding],
    *,
    file: str,
    source_kind: SourceKind,
    text: str,
    start: int,
    end: int,
    base: SourcePosition,
    legacy_form: str,
) -> None:
    rule = _MIGRATION_RULES[legacy_form]
    findings.append(
        InventoryFinding(
            file=file,
            source_kind=source_kind,
            source_range=_range_for(text, start, end, base),
            legacy_form=legacy_form,
            classification=rule.classification,
            target_plan=rule.target_plan,
            reason=rule.reason,
        )
    )


# 被移除的百分号写法映射到稳定报告名称；未知指令另列 blocked，而非默默丢弃。
_PERCENT_FORMS = {
    "module": "percentModule",
    "import": "percentImport",
    "async": "percentAsync",
    "await": "percentAwait",
    "extern": "percentExtern",
    "test": "percentTest",
    "compileTime": "percentCompileTime",
    "func": "percentFunc",
    "owned": "percentOwned",
    "release": "percentRelease",
    "upgrade": "percentUpgrade",
    "weak": "percentWeak",
    "shared": "percentShared",
    "detach": "percentDetach",
    "unique": "percentUnique",
    "in": "percentIn",
    "ref": "percentRef",
    "out": "percentOut",
    "borrow": "percentBorrow",
    "loan": "percentLoan",
    "borrowed": "percentBorrowed",
    "loaned": "percentLoaned",
    "type": "percentType",
    "using": "percentUsing",
}


# 此表反映目标计划已晋升的边界；词法可匹配不代表绑定/资源证明足以自动改写。
_MIGRATION_RULES = {
    "percentModule": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "06A",
        "module_directive_has_current_syntax",
    ),
    "percentImport": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "06A",
        "import_binding_context_required",
    ),
    "percentAsync": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "12",
        "async_keyword_has_current_syntax",
    ),
    "percentAwait": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "12",
        "await_keyword_has_current_syntax",
    ),
    "percentExtern": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "10",
        "native_extern_has_current_syntax",
    ),
    "percentTest": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "14",
        "test_metadata_function_required",
    ),
    "percentCompileTime": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "11",
        "comptime_keyword_has_current_syntax",
    ),
    "percentFunc": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "06A",
        "function_type_has_current_syntax",
    ),
    "percentOwned": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "04",
        "resource_keyword_has_current_syntax",
    ),
    "percentRelease": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "04",
        "owner_builtin_has_current_syntax",
    ),
    "percentUpgrade": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "04",
        "owner_builtin_has_current_syntax",
    ),
    "percentWeak": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "04",
        "owner_builtin_has_current_syntax",
    ),
    "percentShared": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "04",
        "owner_builtin_has_current_syntax",
    ),
    "percentDetach": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "04",
        "owner_builtin_has_current_syntax",
    ),
    "percentUnique": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "04",
        "fresh_resource_proof_required",
    ),
    "percentIn": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "02",
        "parameter_contract_has_current_syntax",
    ),
    "percentRef": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "02",
        "parameter_contract_has_current_syntax",
    ),
    "percentOut": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "02",
        "parameter_contract_has_current_syntax",
    ),
    "percentBorrow": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "02",
        "borrow_place_and_escape_proof_required",
    ),
    "percentLoan": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "02",
        "loan_place_and_escape_proof_required",
    ),
    "percentBorrowed": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "02",
        "borrow_place_and_escape_proof_required",
    ),
    "percentLoaned": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "02",
        "loan_place_and_escape_proof_required",
    ),
    "percentType": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "08",
        "runtime_type_query_has_current_syntax",
    ),
    "percentUsing": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "06A",
        "using_role_resolution_required",
    ),
    "legacyFuncKeyword": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "06A",
        "function_keyword_has_current_syntax",
    ),
    "keywordlessFunction": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "06A",
        "function_keyword_has_current_syntax",
    ),
    "legacyDefinitionArrow": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "06A",
        "function_definition_return_delimiter_has_current_syntax",
    ),
    "legacyFunctionTypeArrow": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "06A",
        "function_type_arrow_has_current_syntax",
    ),
    "legacyLambdaWithoutFn": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "06A",
        "lambda_keyword_has_current_syntax",
    ),
    "legacyDollarConstruct": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "03",
        "static_constructor_has_current_syntax",
    ),
    "legacyDynamicDollarConstruct": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "08",
        "dynamic_constructor_requires_reflection_review",
    ),
    "legacyBareTypeCall": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "06A",
        "call_vs_constructor_binding_required",
    ),
    "legacyNewStruct": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "06A",
        "new_struct_binding_required",
    ),
    "nativePrototypeFactory": MigrationRule(
        MigrationClassification.TARGET_NOT_PROMOTED,
        "10",
        "target_plan_not_promoted",
    ),
    "legacyPropertyAccessor": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "05",
        "property_pairing_required",
    ),
    "unrecognizedPercentDirective": MigrationRule(
        MigrationClassification.BLOCKED,
        "06A",
        "unrecognized_legacy_directive",
    ),
}


# arrow 分类只回溯同一局部语句的参数括号，避免越过分隔符借用前一个调用的上下文。
def _callable_open_before_arrow(text: str, arrow_start: int) -> int | None:
    close = arrow_start
    while close > 0 and text[close - 1] != ")":
        if text[close - 1] in "\n;{}":
            return None
        close -= 1
    if close == 0:
        return None

    depth = 0
    for offset in range(close - 1, -1, -1):
        if text[offset] == ")":
            depth += 1
        elif text[offset] == "(":
            depth -= 1
            if depth == 0:
                return offset
    return None


# 为 arrow 判断提取紧邻括号的 callable 名；传入的是已屏蔽注释/字符串的文本。
def _word_before_offset(text: str, offset: int) -> str:
    end = offset
    while end > 0 and text[end - 1].isspace():
        end -= 1
    start = end
    while start > 0 and (text[start - 1].isalnum() or text[start - 1] == "_"):
        start -= 1
    return text[start:end]


# .zr、文档围栏和宿主字面量共用的旧写法检测器；返回审查候选而非解析器诊断。
# 大写调用/new 等依赖语义绑定的形式在 repository 模式另列 reviewedCurrentFindings。
def _scan_zr_text(
    text: str,
    *,
    file: str,
    source_kind: SourceKind,
    base: SourcePosition = SourcePosition(line=1, column=1),
) -> list[InventoryFinding]:
    mask = _code_mask(text)
    findings: list[InventoryFinding] = []

    for match in re.finditer(r"%([A-Za-z_][A-Za-z0-9_]*)", mask):
        form = _PERCENT_FORMS.get(
            match.group(1),
            "unrecognizedPercentDirective",
        )
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form=form,
        )

    for match in re.finditer(r"(?<!%)\bfunc\b", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form="legacyFuncKeyword",
        )

    for match in re.finditer(r"(?m)^\s*(?!func\b)([A-Za-z_][A-Za-z0-9_]*)\s*\([^\n{};]*\)\s*->", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(1),
            end=match.end(1),
            base=base,
            legacy_form="keywordlessFunction",
        )

    for match in re.finditer(r"->", mask):
        open_offset = _callable_open_before_arrow(mask, match.start())
        callable_word = _word_before_offset(mask, open_offset) if open_offset is not None else ""
        suffix = mask[match.end() :]
        if callable_word == "func" or (
            callable_word == "fn" and not re.match(r"\s*\{", suffix)
        ):
            continue
        legacy_form = (
            "legacyLambdaWithoutFn"
            if re.match(r"\s*\{", suffix)
            else "legacyDefinitionArrow"
        )
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form=legacy_form,
        )

    for match in re.finditer(r"=>", mask):
        open_offset = _callable_open_before_arrow(mask, match.start())
        callable_word = _word_before_offset(mask, open_offset) if open_offset is not None else ""
        if callable_word == "fn":
            continue
        line_start = mask.rfind("\n", 0, match.start()) + 1
        line_prefix = mask[line_start : match.start()]
        if not re.search(r"\)\s*(?::\s*[^=;\n]+)?$", line_prefix):
            continue
        preceding = mask[:open_offset].rstrip() if open_offset is not None else ""
        legacy_form = (
            "legacyFunctionTypeArrow"
            if callable_word == "func" or preceding.endswith(":")
            else "legacyLambdaWithoutFn"
        )
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form=legacy_form,
        )

    for match in re.finditer(r"\$\(", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form="legacyDynamicDollarConstruct",
        )

    for match in re.finditer(r"\$[A-Za-z_][A-Za-z0-9_]*", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form="legacyDollarConstruct",
        )

    for match in re.finditer(r"\bnew\s+([A-Z][A-Za-z0-9_]*)\s*\(", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.start(1),
            base=base,
            legacy_form="legacyNewStruct",
        )

    for match in re.finditer(r"\bnativeFactory\s*\(", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.start() + len("nativeFactory"),
            base=base,
            legacy_form="nativePrototypeFactory",
        )

    for match in re.finditer(r"\b([A-Z][A-Za-z0-9_]*)\s*\(", mask):
        preceding = mask[: match.start(1)].rstrip()
        if preceding.endswith("$") or re.search(r"\bnew$", preceding):
            continue
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(1),
            end=match.end(1),
            base=base,
            legacy_form="legacyBareTypeCall",
        )

    for match in re.finditer(r"\b(?:get|set)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.start(1),
            base=base,
            legacy_form="legacyPropertyAccessor",
        )

    return findings


# 全仓宿主文件只扫描具有 source/parse 等输入上下文且以 Zr 形式开头的字面量，
# 防止格式化字符串和期望诊断文本成为待迁移输入。
def _embedded_context_is_zr_input(
    host_text: str,
    literal_start: int,
    payload: str,
) -> bool:
    preceding = host_text[max(0, literal_start - 256) : literal_start]
    has_input_context = bool(
        re.search(
            r"\b(?:"
            r"(?:source|src|script|program|input|fixture|code|content|document)\w*"
            r"|[A-Za-z_][A-Za-z0-9_]*(?:source|src|script|program|input|fixture|code|content|document)"
            r")\s*=\s*(?:[\"'`])?$",
            preceding,
            flags=re.IGNORECASE,
        )
    )
    has_parser_context = bool(
        re.search(
        r"\b(?:parse|compile|analy[sz]e|execute|eval|load)\w*\s*\(\s*$",
        preceding,
        flags=re.IGNORECASE,
        )
    )
    has_zr_opening = bool(
        re.match(
            r"\s*(?:%[A-Za-z_][A-Za-z0-9_]{2,}|func\b|"
            r"(?:class|struct|resource|interface|enum|union|module|import|native|"
            r"let|var|return|if|while|for|foreach|switch|using|new)\b|"
            r"\$[A-Za-z_(]|[A-Z][A-Za-z0-9_]*\s*\()",
            payload,
        )
    )
    return has_zr_opening and (has_input_context or has_parser_context)


# 按来源选择扫描入口；repository 模式启用宿主输入过滤，fixture 模式可扫描全部字面量。
def _scan_path(
    path: Path,
    *,
    file: str,
    source_kind: SourceKind,
    require_embedded_source_context: bool = False,
) -> list[InventoryFinding]:
    text = path.read_text(encoding="utf-8")
    if source_kind == SourceKind.ZR_SOURCE:
        return _scan_zr_text(text, file=file, source_kind=source_kind)
    if source_kind == SourceKind.EMBEDDED_ZR_FIXTURE:
        findings: list[InventoryFinding] = []
        source_sequence = False
        previous_literal_end = -1
        for fragment in _iter_embedded_string_payloads(text):
            payload = fragment.text
            literal_start, literal_end = fragment.literal_start, fragment.literal_end
            direct_source_input = _embedded_context_is_zr_input(
                text,
                literal_start,
                payload,
            )
            # C 相邻字面量属于同一 source 序列，后续片段即使不再以 Zr 关键字开头也要扫描。
            contiguous_source_segment = (
                source_sequence
                and text[previous_literal_end + 1 : literal_start - 1].strip() == ""
            )
            if require_embedded_source_context and not (
                direct_source_input or contiguous_source_segment
            ):
                source_sequence = False
                previous_literal_end = literal_end
                continue
            source_sequence = direct_source_input or contiguous_source_segment
            for finding in _scan_zr_text(payload, file=file, source_kind=source_kind):
                span = finding.source_range
                findings.append(replace(finding, source_range=SourceRange(
                    start=_position_at(text, fragment.source_offset(span.start.line, span.start.column)),
                    end=_position_at(text, fragment.source_offset(span.end.line, span.end.column)),
                )))
            previous_literal_end = literal_end
        return findings
    return _scan_documentation(text, file=file)


# 只扫描显式 ```zr 围栏，历史解释中的普通 Markdown 文本不进入语法候选。
def _scan_documentation(
    text: str,
    *,
    file: str,
) -> list[InventoryFinding]:
    # TODO: 当前仓库的五处 wiki ```zr 围栏令 repository gate 产生 findings，
    # 包括 error-handling/grammar-reference/patterns-ownership-recipes/
    # type-expression-reference/math-api。需逐份核对是过时示例还是明确的反例，
    # 再修正文档或建立有证据的负向清单；见 test_repository_inventory_is_closed_deterministic_and_excludes_non_source_inputs。
    findings: list[InventoryFinding] = []
    lines = text.splitlines(keepends=True)
    in_zr_fence = False
    block_start = 0
    block: list[str] = []
    for line_index, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not in_zr_fence and stripped == "```zr":
            in_zr_fence = True
            block_start = line_index + 1
            block = []
            continue
        if in_zr_fence and stripped == "```":
            findings.extend(
                _scan_zr_text(
                    "".join(block),
                    file=file,
                    source_kind=SourceKind.CURRENT_DOCUMENTATION,
                    base=SourcePosition(line=block_start, column=1),
                )
            )
            in_zr_fence = False
            continue
        if in_zr_fence:
            block.append(line)
    return findings


# 测试的合成 fixture 入口：遍历本地目录并保留固定分类合同，不套用全仓排除/allowlist。
def build_inventory(root: Path) -> InventoryReport:
    root = root.resolve()
    scanned_files: list[ScannedFile] = []
    findings: list[InventoryFinding] = []
    for path, relative_path in _iter_files(root):
        source_kind = _source_kind_for(path, relative_path)
        if source_kind is not None:
            relative_file = relative_path.as_posix()
            scanned_files.append(
                ScannedFile(
                    file=relative_file,
                    source_kind=source_kind,
                )
            )
            findings.extend(
                _scan_path(
                    path,
                    file=relative_file,
                    source_kind=source_kind,
                )
            )

    scanned_files.sort(key=lambda entry: (entry.file, entry.source_kind.value))
    findings.sort(
        key=lambda entry: (
            entry.file,
            entry.source_range.start.line,
            entry.source_range.start.column,
            entry.legacy_form,
        )
    )
    return InventoryReport(
        scanned_files=tuple(scanned_files),
        exclusions=(),
        findings=tuple(findings),
    )


# 报告公开的扫描范围声明；实际候选由 Git 文件列表及下方分类谓词决定。
_REPOSITORY_SELECTED_ROOTS = (
    "tests",
    "examples",
    "docs",
    "extensions",
    "zr_vm_lib_*",
    "zr_vm_library",
)
# 二进制进入候选后显式排除，宿主源码仅从可能嵌入 Zr 文本的扩展名中选择。
_BINARY_ARTIFACT_SUFFIXES = {".zro", ".zri", ".zrs"}
_EMBEDDED_HOST_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".js", ".ts"}
# 负向测试依 file/column/form 稳定识别；新增相同位置的真实旧写法需人工复核此表。
_REPOSITORY_FINDING_ALLOWLIST = {
    (
        "tests/parser/test_percent_syntax_cutover.c",
        42,
        "unrecognizedPercentDirective",
    ): "expectedUnknownPercentNegative",
    (
        "tests/language_server/test_lsp_current_syntax_formatting_cases.h",
        14,
        "percentCompileTime",
    ): "expectedRemovedSyntaxFormattingNegative",
    (
        "tests/language_server/test_lsp_current_syntax_formatting_cases.h",
        28,
        "percentFunc",
    ): "expectedRemovedSyntaxFormattingNegative",
    (
        "tests/language_server/test_lsp_current_syntax_formatting_cases.h",
        39,
        "legacyFunctionTypeArrow",
    ): "expectedRemovedSyntaxFormattingNegative",
    (
        "tests/task/test_task_runtime.c",
        27,
        "unrecognizedPercentDirective",
    ): "expectedRemovedPercentTypeNegative",
    (
        "tests/task/test_task_runtime.c",
        24,
        "unrecognizedPercentDirective",
    ): "expectedRemovedPercentTypeNegative",
    (
        "tests/language_server/test_lsp_project_features.c",
        10,
        "percentImport",
    ): "expectedRemovedImportSemanticTokenNegative",
    (
        "tests/language_server/test_lsp_interface.c",
        21,
        "percentUnique",
    ): "expectedRemovedOwnershipDiagnosticNegative",
    (
        "tests/parser/test_compiler_features.c",
        46,
        "percentUsing",
    ): "expectedRemovedUsingFieldNegative",
    (
        "tests/parser/test_compiler_features.c",
        67,
        "percentUnique",
    ): "expectedRemovedOwnershipFieldNegative",
    (
        "tests/parser/test_parser.c",
        47,
        "percentUsing",
    ): "expectedRemovedUsingFieldNegative",
    (
        "tests/parser/test_property_consumer_contracts.c",
        20,
        "legacyPropertyAccessor",
    ): "expectedLegacyPropertyMigrationInput",
    (
        "tests/parser/test_property_consumer_contracts.c",
        27,
        "legacyPropertyAccessor",
    ): "expectedLegacyPropertyMigrationInput",
    (
        "tests/parser/test_property_consumer_contracts.c",
        24,
        "legacyPropertyAccessor",
    ): "expectedLegacyPropertyMigrationInput",
}
# 当前 parser 可接受但词面似旧语法的形式单列，而不再阻断迁移 gate。
_REPOSITORY_REVIEWED_CURRENT_FORMS = {
    "legacyBareTypeCall",
    "legacyNewStruct",
}

# Repository inventory callers commonly request the same snapshot more than once
# (for determinism checks and CLI serialization). Keep the cache process-local and
# invalidate it from the candidate files' filesystem metadata.
_repository_inventory_cache: dict[
    Path,
    tuple[tuple[tuple[str, int, int, int, int, int], ...], InventoryReport],
] = {}


# 重复请求同一仓库快照时以候选文件元数据失效缓存；调用方若要求强一致，须意识到
# 相同元数据但内容被外部改写时此签名无法察觉。
def _repository_inventory_signature(
    root: Path,
    candidate_paths: tuple[str, ...],
) -> tuple[tuple[str, int, int, int, int, int], ...]:
    signature: list[tuple[str, int, int, int, int, int]] = []
    for relative_file in candidate_paths:
        try:
            stat_result = (root / relative_file).stat()
        except FileNotFoundError:
            signature.append((relative_file, -1, -1, -1, -1, -1))
            continue
        signature.append(
            (
                relative_file,
                stat_result.st_mtime_ns,
                stat_result.st_ctime_ns,
                stat_result.st_size,
                stat_result.st_mode,
                stat_result.st_ino,
            )
        )
    return tuple(signature)


# 全仓模式只审 Git 跟踪路径，避免工作树临时生成物污染跨主机基线。
def _tracked_files(root: Path) -> tuple[Path, ...]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=True,
        capture_output=True,
        text=True,
    )
    return tuple(
        Path(raw_path)
        for raw_path in result.stdout.split("\0")
        if raw_path
    )


# 先按根目录与后缀宽选，再让 exclusion 阶段给二进制/历史/生成物写出原因。
def _is_repository_candidate(path: Path) -> bool:
    parts = path.parts
    suffix = path.suffix.lower()
    if suffix in _BINARY_ARTIFACT_SUFFIXES:
        return True
    if parts and parts[0] == "docs":
        return suffix == ".md"
    if parts and parts[0] in {"tests", "examples", "extensions"}:
        return suffix == ".zr" or suffix in _EMBEDDED_HOST_SUFFIXES
    if parts and (parts[0].startswith("zr_vm_lib_") or parts[0] == "zr_vm_library"):
        return suffix == ".zr"
    return False


# 对外候选清单只包含当前仍存在的跟踪文件；调用方可用它与 report.scanned/exclusions 对账。
def repository_candidate_paths(root: Path) -> tuple[str, ...]:
    root = root.resolve()
    return tuple(
        sorted(
            path.as_posix()
            for path in _tracked_files(root)
            if _is_repository_candidate(path) and (root / path).is_file()
        )
    )


# 历史计划、负向/迁移 fixture 与构建产物不代表当前源码；排除理由保留在报告中。
def _repository_exclusion_reason(relative_path: Path) -> str | None:
    parts = relative_path.parts
    normalized = relative_path.as_posix()
    if relative_path.suffix.lower() in _BINARY_ARTIFACT_SUFFIXES:
        return "binaryArtifact"
    if "third_party" in parts:
        return "thirdParty"
    if normalized.startswith("docs/plans/") or normalized.startswith("docs/superpowers/"):
        return "historicalPlan"
    if normalized.startswith("tests/fixtures/syntax_migration_inventory/"):
        return "inventorySelfFixture"
    if normalized.startswith("tests/fixtures/reference/"):
        return "expectedDiagnosticFixture"
    if any("legacy" in part.lower() or "migration" in part.lower() for part in parts):
        return "legacyOrMigrationFixture"
    if "negative" in parts:
        return "expectedDiagnosticFixture"
    if "bin" in parts or "build" in parts or "obj" in parts or "tests_generated" in parts:
        return "generatedOutput"
    return None


# 路径已经经过候选过滤，剩余类型进入 .zr、Markdown 或宿主字符串扫描之一。
def _repository_source_kind(relative_path: Path) -> SourceKind:
    if relative_path.suffix.lower() == ".zr":
        return SourceKind.ZR_SOURCE
    if relative_path.suffix.lower() == ".md":
        return SourceKind.CURRENT_DOCUMENTATION
    return SourceKind.EMBEDDED_ZR_FIXTURE


# 仓库 CLI 与基线测试入口：从 Git 候选生成可审计的 scanned/exclusion/findings 分区，
# 保留 allowlist 与已晋升当前语法记录；输出固定排序供跨平台逐字节比较。
# 规则表不会因编辑 Markdown 自动更新，新增当前文档片段可能使 gate 出现新 finding。
def build_repository_inventory(root: Path) -> InventoryReport:
    root = root.resolve()
    candidate_paths = repository_candidate_paths(root)
    signature = _repository_inventory_signature(root, candidate_paths)
    cached = _repository_inventory_cache.get(root)
    if cached is not None and cached[0] == signature:
        return cached[1]

    scanned_files: list[ScannedFile] = []
    exclusions: list[InventoryExclusion] = []
    findings: list[InventoryFinding] = []
    allowlisted_findings: list[InventoryAllowlistedFinding] = []
    reviewed_current_findings: list[InventoryReviewedCurrentFinding] = []
    for relative_file in candidate_paths:
        relative_path = Path(relative_file)
        reason = _repository_exclusion_reason(relative_path)
        if reason is not None:
            exclusions.append(InventoryExclusion(file=relative_file, reason=reason))
            continue
        source_kind = _repository_source_kind(relative_path)
        try:
            path_findings = _scan_path(
                root / relative_path,
                file=relative_file,
                source_kind=source_kind,
                require_embedded_source_context=True,
            )
        except FileNotFoundError:
            exclusions.append(
                InventoryExclusion(file=relative_file, reason="missingTrackedFile")
            )
            continue
        scanned_files.append(
            ScannedFile(file=relative_file, source_kind=source_kind)
        )
        for finding in path_findings:
            allowlist_reason = _REPOSITORY_FINDING_ALLOWLIST.get(
                (
                    finding.file,
                    finding.source_range.start.column,
                    finding.legacy_form,
                )
            )
            if allowlist_reason is not None:
                allowlisted_findings.append(
                    InventoryAllowlistedFinding(
                        file=finding.file,
                        line=finding.source_range.start.line,
                        column=finding.source_range.start.column,
                        legacy_form=finding.legacy_form,
                        reason=allowlist_reason,
                    )
                )
                continue
            if finding.legacy_form in _REPOSITORY_REVIEWED_CURRENT_FORMS:
                reviewed_current_findings.append(
                    InventoryReviewedCurrentFinding(
                        file=finding.file,
                        source_kind=finding.source_kind,
                        source_range=finding.source_range,
                        syntax_form=finding.legacy_form,
                        reason="currentParserSemanticBindingValidated",
                    )
                )
                continue
            findings.append(finding)

    scanned_files.sort(key=lambda entry: (entry.file, entry.source_kind.value))
    exclusions.sort(key=lambda entry: (entry.file, entry.reason))
    allowlisted_findings.sort(
        key=lambda entry: (entry.file, entry.line, entry.column, entry.legacy_form)
    )
    reviewed_current_findings.sort(
        key=lambda entry: (
            entry.file,
            entry.source_range.start.line,
            entry.source_range.start.column,
            entry.syntax_form,
        )
    )
    findings.sort(
        key=lambda entry: (
            entry.file,
            entry.source_range.start.line,
            entry.source_range.start.column,
            entry.legacy_form,
        )
    )
    report = InventoryReport(
        scanned_files=tuple(scanned_files),
        exclusions=tuple(exclusions),
        findings=tuple(findings),
        allowlisted_findings=tuple(allowlisted_findings),
        reviewed_current_findings=tuple(reviewed_current_findings),
        scanner_version="3",
        selected_roots=_REPOSITORY_SELECTED_ROOTS,
    )
    _repository_inventory_cache[root] = (signature, report)
    return report


# 只读命令行入口；--output 只写报告目标，不迁移或改写任何源文件。
def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Read-only Syntax 06A legacy ZR migration inventory."
    )
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--format", choices=("json", "text"), default="json")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    report = build_repository_inventory(args.root)
    rendered = report.to_json() if args.format == "json" else report.to_text()
    if args.output is None:
        print(rendered, end="")
    else:
        args.output.write_bytes(rendered.encode("utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# @unit SourceKind [type]
# ## Summary
# 把输入划分为 Zr 文件、宿主字面量和当前文档三类，使消费者按来源解释 finding 坐标。
# ## Fields
# ZR_SOURCE 对应 zrSource；EMBEDDED_ZR_FIXTURE 对应 embeddedZrFixture；CURRENT_DOCUMENTATION 对应 currentDocumentation。
# ## Constraints and lifecycle
# str Enum 的 value 是 JSON 协议值；不是文件格式探测器或信任级别。fixture 与仓库模式通过不同路径规则选择它。
# 枚举及成员不持有待释放资源；类本身不验证传入路径或文本。
# ## References
# scripts/syntax_migration_inventory.py:19
# scripts/syntax_migration_inventory.py:245
# scripts/syntax_migration_inventory.py:1159
# tests/scripts/test_syntax_migration_inventory.py:104

# @unit MigrationClassification [type]
# ## Summary
# 记录某种旧写法的迁移责任和可自动化程度，供排期和报告统计使用。
# ## Fields
# machineApplicable、maybeIncorrect、requiresReview、blocked、targetNotPromoted 为五种对外值。
# ## Constraints and lifecycle
# 分类是规则表给出的政策数据，不会触发改写，也不证明语义绑定、所有权或目标计划已验证。
# 五个字符串值构成报告协议；maybeIncorrect 即使当前没有规则使用，也保留零计数。
# ## References
# scripts/syntax_migration_inventory.py:26
# scripts/syntax_migration_inventory.py:259
# scripts/syntax_migration_inventory.py:411
# tests/scripts/test_syntax_migration_inventory.py:114

# @unit SourcePosition [type]
# ## Summary
# 保存扫描文本的一基字符行列，连接片段定位、宿主映射和 JSON 报告。
# ## Fields
# line 与 column 共同指向同一文本中的字符边界；尾后边界也可表达。
# ## Constraints and lifecycle
# frozen dataclass 只限制通常的属性重赋值，不检查 line/column 为正数；构造者须遵守坐标域。
# 列是 Python str 字符计数，不是 UTF-8 字节或 LSP UTF-16 单位；read_text 换行规范化后的文本才是定位对象。
# ## References
# scripts/syntax_migration_inventory.py:35
# scripts/syntax_migration_inventory.py:283
# scripts/syntax_migration_inventory.py:882
# tests/scripts/test_syntax_migration_embedded_positions.py:21

# @unit SourceRange [type]
# ## Summary
# 将 finding 的起点和尾后终点绑定为同一来源文本中的范围。
# ## Fields
# start 与 end 都是 SourcePosition；end 为尾后边界。
# ## Constraints and lifecycle
# 构造者负责两端属于同一文本并满足起点不晚于终点；自动构造器不检查排序或边界。
# 嵌入片段的两端分别映射回宿主后再组成范围，不能只移动起点或假定转义前后长度相同。
# ## References
# scripts/syntax_migration_inventory.py:42
# scripts/syntax_migration_inventory.py:299
# scripts/syntax_migration_inventory.py:881
# tests/scripts/test_syntax_migration_embedded_positions.py:21

# @unit ScannedFile [type]
# ## Summary
# 记录实际完成扫描的输入，即使没有 finding 也让消费者看到候选覆盖边界。
# ## Fields
# file 与 source_kind 必须解释同一个输入；扫描集合不等于全部跟踪文件。
# ## Constraints and lifecycle
# file 是构造方给出的相对路径字符串，source_kind 是同次分派选择的来源；此记录不自行读取文件。
# frozen 记录没有文件句柄；被缓存报告持有时可独立保留，不保证磁盘文件以后仍存在或未变。
# ## References
# scripts/syntax_migration_inventory.py:49
# scripts/syntax_migration_inventory.py:938
# scripts/syntax_migration_inventory.py:1202
# tests/scripts/test_syntax_migration_inventory.py:112

# @unit InventoryFinding [type]
# ## Summary
# 把旧形式的位置、迁移分类和责任计划作为一个不可拆开的报告条目交付。
# ## Fields
# file/source_kind/source_range 共同标识来源；legacy_form 对应规则键；classification/target_plan/reason 来自同一 MigrationRule。
# ## Constraints and lifecycle
# 字段一致性由 _append_finding 和规则表保证；直接构造不验证范围、枚举或理由。
# 条目持有字符串和坐标值，不借用已打开文件；它描述扫描时文本，不随磁盘或规则表后续变化更新。
# ## References
# scripts/syntax_migration_inventory.py:56
# scripts/syntax_migration_inventory.py:356
# scripts/syntax_migration_inventory.py:1205
# tests/scripts/test_syntax_migration_inventory.py:319

# @unit InventoryExclusion [type]
# ## Summary
# 为已选中却未扫描的候选保留明确理由，供消费者与 scanned_files 对账。
# ## Fields
# file 指向候选相对路径；reason 是稳定的排除类别。
# ## Constraints and lifecycle
# 排除理由来自路径规则或扫描时缺失；该记录不证明输入由生成器产生、没有维护价值或可安全删除。
# 记录只持有路径和理由字符串；与实际文件的生命周期无所有权关系。
# ## References
# scripts/syntax_migration_inventory.py:68
# scripts/syntax_migration_inventory.py:1136
# scripts/syntax_migration_inventory.py:1187
# scripts/syntax_migration_inventory.py:1199
# tests/scripts/test_syntax_migration_inventory.py:443

# @unit InventoryAllowlistedFinding [type]
# ## Summary
# 单列允许存在于负向测试中的旧写法，保留出现位置而不计入待迁移 findings。
# ## Fields
# line 保留本次实际行号；column 与 legacy_form 参与 allowlist 查找；reason 来自命中的表项。
# ## Constraints and lifecycle
# 匹配键仅为 file、起点 column、legacy_form，不含 line；插入行不会改变匹配，但同文件同列同形式的其他出现也可能匹配。
# 这是静态路径表分类，不执行负向测试或证明某个输入已经被 parser 拒绝。
# ## References
# scripts/syntax_migration_inventory.py:75
# scripts/syntax_migration_inventory.py:980
# scripts/syntax_migration_inventory.py:1206
# tests/scripts/test_syntax_migration_inventory.py:428

# @unit InventoryReviewedCurrentFinding [type]
# ## Summary
# 保留词面似旧形式但被政策集合列为当前候选的出现，避免混入待迁移列表。
# ## Fields
# syntax_form 来自原 legacy_form；source_kind/source_range 仍解释该次输入；reason 使用固定分类理由。
# ## Constraints and lifecycle
# 此分支按形式名集合归类，不在扫描时调用 parser 或语义绑定器；reason 文本不是每个片段均已独立验证的证明。
# 来源与范围沿用原 finding；记录不改写输入，也不延长任何文件句柄生命周期。
# ## References
# scripts/syntax_migration_inventory.py:85
# scripts/syntax_migration_inventory.py:1053
# scripts/syntax_migration_inventory.py:1224
# tests/scripts/test_syntax_migration_inventory.py:347

# @unit MigrationRule [type]
# ## Summary
# 为识别出的旧形式提供一组稳定的分类、目标计划和理由，集中迁移政策。
# ## Fields
# classification、target_plan、reason 必须作为同一政策合同一起使用。
# ## Constraints and lifecycle
# 规则值是 frozen 记录，但保存它的字典可变；扫描期间调用方应保持规则和百分号映射一致且不并发改写。
# 规则不执行目标计划或自动转换；缺少所需规则键会使 _append_finding 抛 KeyError。
# ## References
# scripts/syntax_migration_inventory.py:95
# scripts/syntax_migration_inventory.py:367
# scripts/syntax_migration_inventory.py:411
# tests/scripts/test_syntax_migration_inventory.py:319

# @unit InventoryReport [type]
# ## Summary
# 持有一次构造结果的扫描分区，供 JSON、人工文本和 fixture 合同三个投影共同消费。
# ## Fields
# scanned_files/exclusions 分开表示完成扫描与跳过的候选；findings、allowlisted_findings、reviewed_current_findings 表示三类出现。
# scanner_version 与 selected_roots 是构造方声明的报告上下文，不是内容哈希或全仓一致性令牌。
# ## Constraints and lifecycle
# frozen 和 tuple 使通常的条目替换受限，但类型注解不做运行时校验；手工构造者必须提供相容的记录与枚举。
# build_inventory 与 build_repository_inventory 在构造报告前负责排序、分类和分区；dataclass 自动 __init__ 只保存传入字段。投影不再扫描或重排报告元组；to_fixture_json 另将新建的 contracts 数组按形式名排序。默认 scanner_version 为1，仓库构造器显式使用3。
# 报告只持有 Python 值；缓存可延长其进程内寿命，但不保证之后的文件内容一致。无显式 Free 或文件句柄。
# ## References
# scripts/syntax_migration_inventory.py:103
# scripts/syntax_migration_inventory.py:114
# scripts/syntax_migration_inventory.py:929
# scripts/syntax_migration_inventory.py:1258
# tests/scripts/test_syntax_migration_inventory.py:341

# @unit InventoryReport.to_json [function]
# ## Summary
# 将同一报告投影为消费者依赖的 JSON 文本，保持字段协议、Unicode 文本和末尾 LF。
# ## Parameters
# self [in] 字段相容且在调用期间稳定的 InventoryReport；不转移其条目。
# ## Returns
# 返回新 str；sort_keys 只排序 JSON 对象键，数组沿用报告顺序；固定 unknownCount=0 不表示不存在未知指令或遗漏。
# ## Constraints and lifecycle
# 不读取输入文件或刷新缓存；枚举 value、asdict、JSON 编码及分配异常传播，self 不被修改。
# 字符串由 Python 引用管理；输出编码及文件写入由调用方另行负责。
# ## References
# scripts/syntax_migration_inventory.py:114
# scripts/syntax_migration_inventory.py:120
# scripts/syntax_migration_inventory.py:164
# scripts/syntax_migration_inventory.py:1282
# tests/scripts/test_syntax_migration_inventory.py:98
# tests/scripts/test_syntax_migration_inventory.py:341

# @unit InventoryReport.to_text [function]
# ## Summary
# 为人工审阅展示报告规模、分类、计划和逐项位置，不重新进行扫描。
# ## Parameters
# self [in] 具有有效条目、枚举和坐标的报告，只读借用。
# ## Returns
# 返回带末尾 LF 的新 str；分类保留零项，计划名按字符串排序，条目顺序沿用报告。
# ## Constraints and lifecycle
# 不为 findings、allowlisted 或 reviewed-current 再做政策判断；输出只展开 findings 的逐项详情。
# 格式化或分配异常传播；不修改报告、不打开文件，也不保证终端能够编码所有字符。
# ## References
# scripts/syntax_migration_inventory.py:173
# scripts/syntax_migration_inventory.py:186
# scripts/syntax_migration_inventory.py:191
# scripts/syntax_migration_inventory.py:194
# scripts/syntax_migration_inventory.py:1282

# @unit InventoryReport.to_fixture_json [function]
# ## Summary
# 把合成 fixture 中每个旧形式的唯一分类合同序列化为稳定基线。
# ## Parameters
# self [in] 其 findings 用于提炼合同的报告；其他分区不进入 contracts。
# ## Returns
# 返回带末尾 LF 的 JSON str；同一 legacy_form 出现不同合同则抛 ValueError，不返回部分文本。
# ## Constraints and lifecycle
# classificationCounts 仍按 finding 出现次数统计，contracts 则按形式去重；不能把合同条数当命中数。
# 局部字典及编码失败不改写 self 或磁盘 golden；生成文本的持有由 Python 管理。
# ## References
# scripts/syntax_migration_inventory.py:210
# scripts/syntax_migration_inventory.py:218
# scripts/syntax_migration_inventory.py:220
# scripts/syntax_migration_inventory.py:226
# tests/scripts/test_syntax_migration_inventory.py:331

# @unit _source_kind_for [function]
# ## Summary
# 在合成 fixture 遍历中用后缀和相对路径目录约定决定扫描来源。
# ## Parameters
# path [in] 待分派 Path；此函数不打开或验证存在性。
# relative_path [in] 相对于同一 fixture 根的路径；目录段用于 embedded/docs 约定。
# ## Returns
# 返回 SourceKind；不符合 .zr、embedded 中 C/C++/H、docs 中 .md 的路径返回 None。
# ## Constraints and lifecycle
# 后缀匹配区分大小写；不能套用仓库模式的根选择和小写后缀规则。
# 只做路径分类，没有资源取得或输入修改；路径对象须与调用方当前遍历条目相符。
# ## References
# scripts/syntax_migration_inventory.py:245
# scripts/syntax_migration_inventory.py:933
# scripts/syntax_migration_inventory.py:934
# tests/scripts/test_syntax_migration_inventory.py:104

# @unit _classification_counts [function]
# ## Summary
# 为报告各投影统计每种迁移分类的出现次数，并保留零计数类别。
# ## Parameters
# findings [in] 可重复遍历、调用期间稳定且 classification 为已知枚举的条目序列。
# ## Returns
# 返回新的类别字符串到整数计数字典；不会修改条目。
# ## Constraints and lifecycle
# 实际依赖可迭代行为，不执行输入类型检查；未知 classification 或无效字段导致异常。
# 计数按出现累加，不按文件或旧形式去重；没有 I/O 或共享可变状态。
# ## References
# scripts/syntax_migration_inventory.py:256
# scripts/syntax_migration_inventory.py:164
# scripts/syntax_migration_inventory.py:186
# scripts/syntax_migration_inventory.py:226
# tests/scripts/test_syntax_migration_inventory.py:343

# @unit _target_plan_counts [function]
# ## Summary
# 按目标计划聚合命中次数，使机器和人工投影使用同一统计口径。
# ## Parameters
# findings [in] 调用期间稳定、target_plan 为可比较字符串的 finding 序列。
# ## Returns
# 返回按计划名字符串排序的新计数字典。
# ## Constraints and lifecycle
# 顺序是字符串顺序，不是计划号的数值或依赖顺序；每条出现都计数。
# 不做目标计划晋升判断，不修改报告或文件；字段、比较和分配异常传播。
# ## References
# scripts/syntax_migration_inventory.py:266
# scripts/syntax_migration_inventory.py:165
# scripts/syntax_migration_inventory.py:191

# @unit _iter_files [function]
# ## Summary
# 为合成 fixture 模式惰性枚举本地文件，不依赖 Git 跟踪状态。
# ## Parameters
# root [in] 在迭代期间保持可遍历的 fixture 根 Path。
# ## Returns
# 返回生成器；每次推进交付文件 Path 及相对 root 的 Path，不承诺发现顺序。
# ## Constraints and lifecycle
# rglob 与 is_file 决定输入边界，不能视为不可信路径隔离或普通文件所有权认证；仓库模式不用此生成器。
# 推进时才访问文件系统，相关异常可能在消费期间出现；关闭或丢弃迭代器释放 Python 引用，不持有跨 yield 的显式打开文件。
# ## References
# scripts/syntax_migration_inventory.py:276
# scripts/syntax_migration_inventory.py:933
# scripts/syntax_migration_inventory.py:245

# @unit _position_at [function]
# ## Summary
# 将文本字符偏移投影为一基位置，首行继承基点列，后续行从列1重新计。
# ## Parameters
# text [in] 本次定位所依据的 Python str。
# offset [in] 由调用者保证位于0到len(text)之间的字符边界；函数不校验或统一夹取该范围。
# base [in] 此文本首字符在外部坐标系中的一基位置。
# ## Returns
# 返回新的 SourcePosition；offset 为尾后边界时也可定位。
# ## Constraints and lifecycle
# 按换行字符和 str 长度计算，不恢复 UTF-8 字节、CRLF 物理字节或 UTF-16 单位。
# 无文件 I/O 或共享状态；负数或过大 offset 不具有这里约定的定位意义。
# ## References
# scripts/syntax_migration_inventory.py:283
# scripts/syntax_migration_inventory.py:306
# scripts/syntax_migration_inventory.py:307
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883
# tests/scripts/test_syntax_migration_embedded_positions.py:17

# @unit _range_for [function]
# ## Summary
# 集中构造同一文本的起点及尾后终点，供 finding 保存一致坐标系。
# ## Parameters
# text [in] 两个偏移所依据的同一 str。
# start [in] 起点字符偏移；调用方保证0<=start<=end。
# end [in] 尾后字符偏移；调用方保证end<=len(text)。
# base [in] 该片段首字符的外部一基位置。
# ## Returns
# 返回新的 SourceRange，分别调用 _position_at 转换两端。
# ## Constraints and lifecycle
# 函数不检查范围排序或边界；不能把 decoded 终点直接当宿主终点。
# 仅构造值对象，不改写输入、申请文件资源或借用可失效缓冲区。
# ## References
# scripts/syntax_migration_inventory.py:299
# scripts/syntax_migration_inventory.py:372
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit _code_mask [function]
# ## Summary
# 为启发式旧形式匹配屏蔽注释和引号内容，保留字符长度与换行位置。
# ## Parameters
# text [in] 待扫描的 Python str；函数不修改它。
# ## Returns
# 返回同长度掩码 str；被屏蔽的非换行字符变为空格。
# ## Constraints and lifecycle
# 只识别 //、非嵌套 /* */ 及单/双/反引号，反斜杠跳过下一字符；未闭合片段消耗到EOF而不报告语法错误。
# 它不是完整 Zr lexer，不能从掩码无命中推断语法有效，也不能保证所有求余形式均被语义区分。
# 只使用局部状态；分配异常传播，没有 I/O。
# ## References
# scripts/syntax_migration_inventory.py:313
# scripts/syntax_migration_inventory.py:319
# scripts/syntax_migration_inventory.py:327
# scripts/syntax_migration_inventory.py:335
# scripts/syntax_migration_inventory.py:636
# tests/scripts/test_syntax_migration_inventory.py:125

# @unit _append_finding [function]
# ## Summary
# 把一次正则候选与集中政策及位置合成为完整 finding，避免匹配路径各自解释责任。
# ## Parameters
# findings [in,out] 接收条目的 list；此前已有条目不会被本函数重算。
# file [in] 在报告中原样保存的相对路径标签。
# source_kind [in] 与本次文本来源一致的枚举。
# text [in] 原始待定位文本，须与start/end/base对应。
# start [in] 起点字符边界。
# end [in] 尾后字符边界；须满足start<=end<=len(text)。
# base [in] 此片段的外部一基基点。
# legacy_form [in] 必须存在于当前 _MIGRATION_RULES 的形式名。
# ## Returns
# 成功追加一条记录并返回None；规则或输入不满足约束时通过Python异常传达，不发布独立状态码。
# ## Constraints and lifecycle
# 先查规则再构造范围和记录，最后 append；缺键抛 KeyError。不得并发修改 findings 或规则表。
# 不验证路径、文本语法或绑定；不写文件。整个上层扫描发生异常时没有发布完整报告的承诺。
# ## References
# scripts/syntax_migration_inventory.py:356
# scripts/syntax_migration_inventory.py:367
# scripts/syntax_migration_inventory.py:368
# scripts/syntax_migration_inventory.py:644
# scripts/syntax_migration_inventory.py:781

# @unit _PERCENT_FORMS [state]
# ## Summary
# 将已知百分号标识符映射为稳定旧形式名，供词法扫描选择政策。
# ## Constraints and lifecycle
# 键匹配区分大小写；未命中由扫描器使用unrecognizedPercentDirective，表值须存在于_MIGRATION_RULES。字典可变，调用方不得在扫描期间改写。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:382
# scripts/syntax_migration_inventory.py:639
# scripts/syntax_migration_inventory.py:640
# scripts/syntax_migration_inventory.py:367

# @unit _MIGRATION_RULES [state]
# ## Summary
# 集中旧形式的分类、责任计划和理由，防止不同来源扫描各自决定迁移政策。
# ## Constraints and lifecycle
# 字典可变而值为frozen记录；表项覆盖扫描器可能发布的形式名。不执行转换、绑定检查或计划晋升，修改须与fixture合同保持一致。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:411
# scripts/syntax_migration_inventory.py:367
# tests/scripts/test_syntax_migration_inventory.py:319
# tests/scripts/test_syntax_migration_inventory.py:331

# @unit _callable_open_before_arrow [function]
# ## Summary
# 为箭头分类寻找紧邻的闭括号及其配对开括号，限制回溯到当前局部语句。
# ## Parameters
# text [in] 已经 _code_mask 处理的文本。
# arrow_start [in] 当前箭头在text中的合法起点偏移。
# ## Returns
# 找到配对括号时返回开括号索引；遇边界、没有闭括号或没有配对开括号时返回 None。
# ## Constraints and lifecycle
# 前向回找闭括号时以换行、分号和花括号为停止边界，之后做括号深度回溯；不是完整 callable/parser 判断。
# 调用方仍负责解释前词与箭头后缀；无 I/O、输入修改或持久资源。
# ## References
# scripts/syntax_migration_inventory.py:596
# scripts/syntax_migration_inventory.py:680
# scripts/syntax_migration_inventory.py:704

# @unit _word_before_offset [function]
# ## Summary
# 从掩码文本中提取括号前相邻单词，帮助区分 fn/func 等箭头上下文。
# ## Parameters
# text [in] 已屏蔽注释与字符串的文本。
# offset [in] 调用方给出的合法开括号索引或其他词边界。
# ## Returns
# 返回前一个由 isalnum 或下划线组成的单词片段，无法形成时为空串。
# ## Constraints and lifecycle
# 会跳过前置空白；isalnum 接受 Unicode 字符，不等同于完整语言标识符规则。
# 不验证偏移和 callable 语义，且不修改输入；只产生 Python 字符串。
# ## References
# scripts/syntax_migration_inventory.py:617
# scripts/syntax_migration_inventory.py:681
# scripts/syntax_migration_inventory.py:705

# @unit _scan_zr_text [function]
# ## Summary
# 对纯 Zr 文本、文档围栏或解码片段执行共享的启发式旧写法检测，返回人工可审的候选。
# ## Parameters
# text [in] 待检测的str；坐标按此文本计算。
# file [in] 报告用来源路径标签。
# source_kind [in] 调用方已经选择的来源类型。
# base [in] 文本首字符的一基位置，默认(1,1)。
# ## Returns
# 返回新 finding list；未知百分号指令归 blocked；空列表不证明语言解析或绑定成功。
# ## Constraints and lifecycle
# 各正则独立运行，结果按检测分组追加，可能存在重叠候选；最终位置排序由上层构造器承担。
# 箭头、构造器、属性等判断仅依词面与有限上下文；大写调用/new 的 reviewed-current 分流在仓库构造器中完成。
# 规则表与输入须在调用期间稳定；异常不转成诊断对象，不执行代码或改写任何文件。
# ## References
# scripts/syntax_migration_inventory.py:629
# scripts/syntax_migration_inventory.py:636
# scripts/syntax_migration_inventory.py:639
# scripts/syntax_migration_inventory.py:679
# scripts/syntax_migration_inventory.py:703
# scripts/syntax_migration_inventory.py:777
# scripts/syntax_migration_inventory.py:854
# scripts/syntax_migration_inventory.py:914
# tests/scripts/test_syntax_migration_inventory.py:125

# @unit _embedded_context_is_zr_input [function]
# ## Summary
# 在仓库宿主扫描中要求有限 source/parse 调用上下文与疑似 Zr 起始形式同时出现，减少诊断和格式串误报。
# ## Parameters
# host_text [in] 宿主文本str，与literal_start同一坐标域。
# literal_start [in] 开引号之后的合法字符偏移，供截取前256个字符。
# payload [in] 当前字面量有限解码后的str。
# ## Returns
# 返回bool，仅表示启发式准入，不证明字面量最终交给Zr解析器。
# ## Constraints and lifecycle
# 上下文只看有限赋值名/调用名前缀及开头规则，不是C/JS语法或数据流分析；可能不能识别其他合法宿主构造。
# 不与相邻 payload 拼接；邻接准入传播由 _scan_path 决定。函数无 I/O 或持久状态。
# ## References
# scripts/syntax_migration_inventory.py:809
# scripts/syntax_migration_inventory.py:814
# scripts/syntax_migration_inventory.py:832
# scripts/syntax_migration_inventory.py:862
# tests/scripts/test_syntax_migration_inventory.py:494
# tests/scripts/test_syntax_migration_inventory.py:528

# @unit _scan_path [function]
# ## Summary
# 读取一个已分派输入，并将纯源、宿主片段或文档围栏的 finding 统一交回上层。
# ## Parameters
# path [in] 可按UTF-8读取的文件Path；读取异常向上传播。
# file [in] 报告标签，须由调用方与path相配。
# source_kind [in] 已选择的SourceKind；已知三类是正常调用约定。
# require_embedded_source_context [in] True时仅保留有输入上下文或同一邻接序列准入的宿主片段。
# ## Returns
# 返回finding list；不会返回已完成一部分扫描的报告对象。
# ## Constraints and lifecycle
# read_text 使用通用换行；报告坐标属于读入str，不是原始字节或UTF-16位置。
# 嵌入路径逐片扫描并用 source_offset 分别投影两端；邻接仅继承准入，不把 payload 合并为一段或识别跨片段拆开的token。
# 所有非ZR_SOURCE/EMBEDDED分支均落入文档扫描，不另做enum值校验。读取、解码、映射、正则和分配异常均可传播。
# 函数不迁移输入、不打开持久句柄，也不对不可信路径提供隔离。
# ## References
# scripts/syntax_migration_inventory.py:845
# scripts/syntax_migration_inventory.py:852
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:872
# scripts/syntax_migration_inventory.py:879
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:887
# scripts/syntax_migration_inventory.py:944
# scripts/syntax_migration_inventory.py:1191
# tests/scripts/test_syntax_migration_inventory.py:519
# tests/scripts/test_syntax_migration_embedded_positions.py:14

# @unit _scan_documentation [function]
# ## Summary
# 仅扫描闭合的显式 zr Markdown 围栏，并将片段行号还原为文档行号。
# ## Parameters
# text [in] 已读入的Markdown str。
# file [in] finding使用的文档路径标签。
# ## Returns
# 返回新finding list；只发布看到结束围栏的代码块，EOF未闭合块不进入结果。
# ## Constraints and lifecycle
# 只认strip后等于```zr的开头和等于```的结尾，不解释一般Markdown嵌套或其他围栏语法。
# TODO: 核对 docs/wiki 中被旧注记列出的反例/过时示例及 repository 空findings/golden 合同；已知入口为本函数与专用仓库测试，历史“五处”数量须与当次输入重新核对，不能代替当前结果。
# 只做候选提取，不决定文档示例应删除还是进入有证据的负向清单；不改写文档或golden。
# ## References
# scripts/syntax_migration_inventory.py:891
# scripts/syntax_migration_inventory.py:896
# scripts/syntax_migration_inventory.py:907
# scripts/syntax_migration_inventory.py:912
# scripts/syntax_migration_inventory.py:914
# tests/scripts/test_syntax_migration_inventory.py:333
# tests/scripts/test_syntax_migration_inventory.py:342

# @unit build_inventory [function]
# ## Summary
# 为人工合成 fixture 构建扫描和分类报告，隔离于仓库 Git 候选与排除政策。
# ## Parameters
# root [in] fixture 根Path；解析为绝对路径后遍历，允许未跟踪文件。
# ## Returns
# 返回排序后的新InventoryReport；exclusions为空，使用默认报告版本和空selected_roots。
# ## Constraints and lifecycle
# 通过 _source_kind_for 选择文件，宿主扫描不开启repository上下文过滤；不使用全仓cache/allowlist/reviewed-current分流。
# 遍历、读取、排序与分配异常传播；没有输入写入或全树原子快照。需要稳定结果时调用方须保持fixture不变。
# 生成器由for消费；所有报告值可由Python独立持有，无需显式Free。
# ## References
# scripts/syntax_migration_inventory.py:929
# scripts/syntax_migration_inventory.py:933
# scripts/syntax_migration_inventory.py:944
# scripts/syntax_migration_inventory.py:951
# scripts/syntax_migration_inventory.py:960
# tests/scripts/test_syntax_migration_inventory.py:97
# tests/scripts/test_syntax_migration_inventory.py:211

# @unit _REPOSITORY_SELECTED_ROOTS [state]
# ## Summary
# 给仓库报告声明有限根范围，便于消费者解释候选来源。
# ## Constraints and lifecycle
# tuple只是报告字段；真实宽选仍由_is_repository_candidate执行，包含glob样名称不表示使用glob遍历。值不持有文件资源。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:968
# scripts/syntax_migration_inventory.py:1109
# scripts/syntax_migration_inventory.py:1265

# @unit _BINARY_ARTIFACT_SUFFIXES [state]
# ## Summary
# 把二进制后缀先纳入候选再显式排除，以便报告保留理由。
# ## Constraints and lifecycle
# 集合可变；候选和排除两个阶段必须读取同一政策。后缀判断不检查内容或生成归属，不能用于删除授权。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:977
# scripts/syntax_migration_inventory.py:1112
# scripts/syntax_migration_inventory.py:1139
# tests/scripts/test_syntax_migration_inventory.py:450

# @unit _EMBEDDED_HOST_SUFFIXES [state]
# ## Summary
# 限定仓库哪些宿主后缀可进入字面量扫描，控制有限词法模型的输入域。
# ## Constraints and lifecycle
# 只在tests/examples/extensions根使用；case-normalized后缀命中不证明文件一定包含Zr或完整支持该宿主语言。集合在扫描窗口须稳定。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:978
# scripts/syntax_migration_inventory.py:1117
# scripts/syntax_migration_inventory.py:845

# @unit _REPOSITORY_FINDING_ALLOWLIST [state]
# ## Summary
# 为现有负向测试保留静态旧形式身份，避免其进入待迁移findings。
# ## Constraints and lifecycle
# 键为file/起点column/legacy_form，没有line，也不执行测试；reason是政策数据。调用方须核对新增相同键出现，不把表命中当语义拒绝证明。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:980
# scripts/syntax_migration_inventory.py:1206
# scripts/syntax_migration_inventory.py:1215
# tests/scripts/test_syntax_migration_inventory.py:355

# @unit _REPOSITORY_REVIEWED_CURRENT_FORMS [state]
# ## Summary
# 把大写调用和new等当前词面候选单列，保留可审记录而不触发旧形式门禁。
# ## Constraints and lifecycle
# 集合仅含legacyBareTypeCall/legacyNewStruct；按形式名分流，不逐片段调用parser或证明绑定。集合改变会影响报告及golden政策。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:1053
# scripts/syntax_migration_inventory.py:1224
# scripts/syntax_migration_inventory.py:1226
# tests/scripts/test_syntax_migration_inventory.py:347

# @unit _repository_inventory_cache [state]
# ## Summary
# 在同一进程按解析后的仓库根缓存元数据签名及完整报告，复用稳定请求。
# ## Constraints and lifecycle
# 字典项只有完整新报告构造后发布；签名不含内容哈希。缓存保留根和记录对象，无自动逐根逐出；没有锁，调用方应串行处理显式清理、规则修改及需要稳定结果的请求。
# 这些状态由模块导入建立；除缓存由构造器写入外，正常调用把政策值作为只读配置。
# ## References
# scripts/syntax_migration_inventory.py:1061
# scripts/syntax_migration_inventory.py:1174
# scripts/syntax_migration_inventory.py:1175
# scripts/syntax_migration_inventory.py:1267
# tests/scripts/test_syntax_migration_inventory.py:67

# @unit _repository_inventory_signature [function]
# ## Summary
# 为进程内缓存建立候选路径及文件元数据签名，避免相同可观察快照重复扫描。
# ## Parameters
# root [in] 已解析的仓库根Path。
# candidate_paths [in] 本次已经排序的相对候选路径tuple，遍历期间应稳定。
# ## Returns
# 返回逐路径的(path,mtime_ns,ctime_ns,size,mode,ino)元组；FileNotFoundError用全-1元数据占位。
# ## Constraints and lifecycle
# 不读取内容或计算内容哈希；所有列保持相同的内容改写无法被此签名察觉。
# stat与后续读取不是原子操作，其他OSError不被包装。没有锁或文件句柄跨调用保留。
# ## References
# scripts/syntax_migration_inventory.py:1069
# scripts/syntax_migration_inventory.py:1076
# scripts/syntax_migration_inventory.py:1078
# scripts/syntax_migration_inventory.py:1173
# tests/scripts/test_syntax_migration_inventory.py:67

# @unit _tracked_files [function]
# ## Summary
# 从实际仓库索引取得NUL分隔的跟踪路径，为repository候选选择提供输入。
# ## Parameters
# root [in] git -C 使用的仓库根Path，调用方须允许只读Git子进程。
# ## Returns
# Git成功时返回Path tuple；Git缺失、非零退出或文本解码错误通过Python异常传达。
# ## Constraints and lifecycle
# 执行git ls-files -z，未请求--others，不包括未跟踪文件；结果仍需存在性与候选分类过滤。
# text=True按当前Python默认文本编码解释输出，UTF-8执行模式由环境或解释器配置提供；函数没有子进程timeout。
# 同步等待并捕获stdout/stderr，不修改索引或源码；不是对Git之外目录的信任认证。
# ## References
# scripts/syntax_migration_inventory.py:1094
# scripts/syntax_migration_inventory.py:1096
# scripts/syntax_migration_inventory.py:1129
# tests/scripts/test_syntax_migration_inventory.py:57

# @unit _is_repository_candidate [function]
# ## Summary
# 根据当前扫描根和扩展名宽选候选，随后由排除阶段解释不扫描的原因。
# ## Parameters
# path [in] Git返回的相对路径Path，按目录段和小写后缀判断。
# ## Returns
# 返回是否属于候选；二进制后缀在根判断之前即被选入，以便显式排除。
# ## Constraints and lifecycle
# docs只选Markdown；tests/examples/extensions选zr或宿主后缀；库根只选zr。它不检查文件存在、内容或来源。
# selected_roots是报告声明，实际规则以此函数为准；不提供路径隔离或生命周期管理。
# ## References
# scripts/syntax_migration_inventory.py:1109
# scripts/syntax_migration_inventory.py:1112
# scripts/syntax_migration_inventory.py:1114
# scripts/syntax_migration_inventory.py:1116
# scripts/syntax_migration_inventory.py:1118
# scripts/syntax_migration_inventory.py:1130

# @unit repository_candidate_paths [function]
# ## Summary
# 生成当前工作树仍存在的Git跟踪候选列表，供扫描与分区对账共用。
# ## Parameters
# root [in] 仓库根Path；函数resolve后交给Git并检查对应路径。
# ## Returns
# 返回排序后的POSIX相对路径tuple；缺失或非is_file条目不进入结果。
# ## Constraints and lifecycle
# 不包含未跟踪输入；is_file为观察时条件，之后仍可能消失或变化，且不是不跟随符号链接的安全认证。
# Git/路径/权限等异常可传播；不扫描内容或修改文件。
# ## References
# scripts/syntax_migration_inventory.py:1124
# scripts/syntax_migration_inventory.py:1129
# scripts/syntax_migration_inventory.py:1130
# scripts/syntax_migration_inventory.py:1172
# tests/scripts/test_syntax_migration_inventory.py:64
# tests/scripts/test_syntax_migration_inventory.py:447

# @unit _repository_exclusion_reason [function]
# ## Summary
# 为已选候选按明确优先级给出排除类别，使报告不静默丢弃路径。
# ## Parameters
# relative_path [in] 同一仓库中的相对Path，按目录段与POSIX字符串匹配。
# ## Returns
# 返回稳定reason字符串；没有命中规则时返回None以继续扫描。
# ## Constraints and lifecycle
# 二进制、third_party、历史计划、自身fixture、参考负向输入、含legacy/migration片段、negative和产物目录按源码顺序判断。
# 这是命名政策，不验证内容是否生成、负向用例是否运行或能否删除；没有文件I/O。
# ## References
# scripts/syntax_migration_inventory.py:1136
# scripts/syntax_migration_inventory.py:1139
# scripts/syntax_migration_inventory.py:1143
# scripts/syntax_migration_inventory.py:1149
# scripts/syntax_migration_inventory.py:1153
# scripts/syntax_migration_inventory.py:1185
# tests/scripts/test_syntax_migration_inventory.py:456

# @unit _repository_source_kind [function]
# ## Summary
# 把已经通过候选与排除阶段的路径映射到三种扫描入口。
# ## Parameters
# relative_path [in] 由上层完成候选选择和排除的相对Path。
# ## Returns
# 小写zr后缀返回ZR_SOURCE，md返回CURRENT_DOCUMENTATION，其余返回EMBEDDED_ZR_FIXTURE。
# ## Constraints and lifecycle
# 函数本身不拒绝任意其他后缀，也不验证存在性；上层过滤是正常调用前提。
# 不读取文件、不修改路径或取得资源。
# ## References
# scripts/syntax_migration_inventory.py:1159
# scripts/syntax_migration_inventory.py:1189
# scripts/syntax_migration_inventory.py:1191

# @unit build_repository_inventory [function]
# ## Summary
# 为CLI和仓库基线构造Git候选的扫描/排除分区，并把允许旧例和当前候选形式从待迁移finding中分开。
# ## Parameters
# root [in] 实际仓库根Path；resolve后的根是进程cache键，输入文件与政策表应在调用窗口稳定。
# ## Returns
# 缓存签名相同时返回缓存的同一个报告对象；否则成功扫描后构造、排序并发布新报告；异常不返回局部报告。
# ## Constraints and lifecycle
# 签名基于文件元数据而非内容；枚举、stat、读取与最后cache发布没有全仓原子快照或同步锁。调用方要求可重放结果时须自行固定输入并串行管理cache/规则修改。
# 只有扫描时FileNotFoundError被记为missingTrackedFile；解码、权限、Git、其他I/O和分配异常传播，旧cache项不会被提前替换。
# allowlist按file/column/form匹配；reviewed-current仅按形式名分流，不在这里执行Parser语义验证。
# 固定输出排序不证明底层树在构造期间未变；缓存持有报告和根Path直到进程结束或显式移除，不持有打开文件。
# ## References
# scripts/syntax_migration_inventory.py:1170
# scripts/syntax_migration_inventory.py:1173
# scripts/syntax_migration_inventory.py:1175
# scripts/syntax_migration_inventory.py:1191
# scripts/syntax_migration_inventory.py:1197
# scripts/syntax_migration_inventory.py:1206
# scripts/syntax_migration_inventory.py:1224
# scripts/syntax_migration_inventory.py:1258
# scripts/syntax_migration_inventory.py:1267
# tests/scripts/test_syntax_migration_inventory.py:84
# tests/scripts/test_syntax_migration_inventory.py:337

# @unit main [function]
# ## Summary
# 提供显式CLI输出入口，把实际仓库报告写到stdout或调用方指定的UTF-8文件。
# ## Parameters
# argv [in] argparse参数列表；None表示读取进程sys.argv。root默认当前目录，format仅json/text，output可省略。
# ## Returns
# 正常渲染和输出后返回0，即使仍有findings；参数解析可SystemExit，扫描或输出错误通过异常退出。
# ## Constraints and lifecycle
# --output直接write_bytes指定目标，不创建父目录、不提供原子替换或输入重叠保护；调用方必须选择可写且不覆盖输入的报告路径。失败时该目标可能已部分写入。
# stdout编码由流配置决定；文件输出显式UTF-8并继承渲染末尾LF。CLI不执行迁移或把finding数量转成失败码。
# 同步扫描可能调用Git且无内部timeout；外层执行者负责预算、路径与缓存环境。
# ## References
# scripts/syntax_migration_inventory.py:1272
# scripts/syntax_migration_inventory.py:1276
# scripts/syntax_migration_inventory.py:1281
# scripts/syntax_migration_inventory.py:1283
# scripts/syntax_migration_inventory.py:1286
# scripts/syntax_migration_inventory.py:1291
# tests/scripts/test_syntax_migration_inventory.py:563
