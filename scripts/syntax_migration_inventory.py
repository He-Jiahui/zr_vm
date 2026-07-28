#!/usr/bin/env python3
"""Read-only inventory protocol for Syntax 06A legacy migration forms."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from dataclasses import asdict, dataclass
from enum import Enum
from pathlib import Path
from typing import Iterable


class SourceKind(str, Enum):
    ZR_SOURCE = "zrSource"
    EMBEDDED_ZR_FIXTURE = "embeddedZrFixture"
    CURRENT_DOCUMENTATION = "currentDocumentation"


class MigrationClassification(str, Enum):
    MACHINE_APPLICABLE = "machineApplicable"
    MAYBE_INCORRECT = "maybeIncorrect"
    REQUIRES_REVIEW = "requiresReview"
    BLOCKED = "blocked"
    TARGET_NOT_PROMOTED = "targetNotPromoted"


@dataclass(frozen=True)
class SourcePosition:
    line: int
    column: int


@dataclass(frozen=True)
class SourceRange:
    start: SourcePosition
    end: SourcePosition


@dataclass(frozen=True)
class ScannedFile:
    file: str
    source_kind: SourceKind


@dataclass(frozen=True)
class InventoryFinding:
    file: str
    source_kind: SourceKind
    source_range: SourceRange
    legacy_form: str
    classification: MigrationClassification
    target_plan: str
    reason: str


@dataclass(frozen=True)
class InventoryExclusion:
    file: str
    reason: str


@dataclass(frozen=True)
class MigrationRule:
    classification: MigrationClassification
    target_plan: str
    reason: str


@dataclass(frozen=True)
class InventoryReport:
    scanned_files: tuple[ScannedFile, ...]
    exclusions: tuple[InventoryExclusion, ...]
    findings: tuple[InventoryFinding, ...]
    scanner_version: str = "1"
    selected_roots: tuple[str, ...] = ()

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

    def to_text(self) -> str:
        lines = [
            f"Syntax 06A migration inventory (scanner {self.scanner_version})",
            f"selected roots: {', '.join(self.selected_roots)}",
            f"scanned files: {len(self.scanned_files)}",
            f"excluded files: {len(self.exclusions)}",
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


def _source_kind_for(path: Path, relative_path: Path) -> SourceKind | None:
    if path.suffix == ".zr":
        return SourceKind.ZR_SOURCE
    if path.suffix in {".c", ".cc", ".cpp", ".h"} and "embedded" in relative_path.parts:
        return SourceKind.EMBEDDED_ZR_FIXTURE
    if path.suffix == ".md" and "docs" in relative_path.parts:
        return SourceKind.CURRENT_DOCUMENTATION
    return None


def _classification_counts(
    findings: tuple[InventoryFinding, ...],
) -> dict[str, int]:
    counts = {classification.value: 0 for classification in MigrationClassification}
    for finding in findings:
        counts[finding.classification.value] += 1
    return counts


def _target_plan_counts(
    findings: tuple[InventoryFinding, ...],
) -> dict[str, int]:
    counts: dict[str, int] = {}
    for finding in findings:
        counts[finding.target_plan] = counts.get(finding.target_plan, 0) + 1
    return dict(sorted(counts.items()))


def _iter_files(root: Path) -> Iterable[tuple[Path, Path]]:
    for path in root.rglob("*"):
        if path.is_file():
            yield path, path.relative_to(root)


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
        MigrationClassification.TARGET_NOT_PROMOTED,
        "12",
        "target_plan_not_promoted",
    ),
    "percentAwait": MigrationRule(
        MigrationClassification.TARGET_NOT_PROMOTED,
        "12",
        "target_plan_not_promoted",
    ),
    "percentExtern": MigrationRule(
        MigrationClassification.TARGET_NOT_PROMOTED,
        "10",
        "target_plan_not_promoted",
    ),
    "percentTest": MigrationRule(
        MigrationClassification.TARGET_NOT_PROMOTED,
        "14",
        "target_plan_not_promoted",
    ),
    "percentCompileTime": MigrationRule(
        MigrationClassification.TARGET_NOT_PROMOTED,
        "11",
        "target_plan_not_promoted",
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
        MigrationClassification.REQUIRES_REVIEW,
        "04",
        "ownership_bridge_binding_required",
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
        MigrationClassification.TARGET_NOT_PROMOTED,
        "08",
        "target_plan_not_promoted",
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
    "legacyDollarConstruct": MigrationRule(
        MigrationClassification.MACHINE_APPLICABLE,
        "03",
        "static_constructor_has_current_syntax",
    ),
    "legacyDynamicDollarConstruct": MigrationRule(
        MigrationClassification.REQUIRES_REVIEW,
        "06A",
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

    for match in re.finditer(r"\bfunc\b", mask):
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
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form="legacyDefinitionArrow",
        )

    for match in re.finditer(r"=>", mask):
        _append_finding(
            findings,
            file=file,
            source_kind=source_kind,
            text=text,
            start=match.start(),
            end=match.end(),
            base=base,
            legacy_form="legacyFunctionTypeArrow",
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


def _iter_embedded_string_payloads(
    text: str,
) -> Iterable[tuple[str, SourcePosition, int, int]]:
    offset = 0
    while offset < len(text):
        current = text[offset]
        following = text[offset + 1] if offset + 1 < len(text) else ""
        if current == "/" and following == "/":
            end = text.find("\n", offset)
            offset = len(text) if end < 0 else end + 1
            continue
        if current == "/" and following == "*":
            end = text.find("*/", offset + 2)
            offset = len(text) if end < 0 else end + 2
            continue
        if current not in {'"', "'", "`"}:
            offset += 1
            continue
        quote = current
        start = offset + 1
        content: list[str] = []
        offset = start
        while offset < len(text):
            current = text[offset]
            if current == "\\" and offset + 1 < len(text):
                escaped = text[offset + 1]
                content.append("\n" if escaped == "n" else escaped)
                offset += 2
                continue
            if current == quote:
                break
            content.append(current)
            offset += 1
        yield "".join(content), _position_at(text, start), start, offset
        offset += 1


def _embedded_context_is_zr_input(
    host_text: str,
    literal_start: int,
    payload: str,
) -> bool:
    preceding = host_text[max(0, literal_start - 256) : literal_start]
    has_input_context = bool(
        re.search(
        r"\b(?:source|src|script|program|input|fixture|code|text)\w*\s*=\s*$",
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
            r"(?:class|struct|resource|interface|enum|union|module|import|"
            r"let|var|return|if|while|for|foreach|switch|using|new)\b|"
            r"\$[A-Za-z_(]|[A-Z][A-Za-z0-9_]*\s*\()",
            payload,
        )
    )
    return has_zr_opening and (has_input_context or has_parser_context)


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
        for payload, base, literal_start, literal_end in _iter_embedded_string_payloads(text):
            direct_source_input = _embedded_context_is_zr_input(
                text,
                literal_start,
                payload,
            )
            contiguous_source_segment = (
                source_sequence
                and text[previous_literal_end + 1 : literal_start].strip() == ""
            )
            if require_embedded_source_context and not (
                direct_source_input or contiguous_source_segment
            ):
                source_sequence = False
                previous_literal_end = literal_end
                continue
            source_sequence = direct_source_input or contiguous_source_segment
            findings.extend(
                _scan_zr_text(
                    payload,
                    file=file,
                    source_kind=source_kind,
                    base=base,
                )
            )
            previous_literal_end = literal_end
        return findings
    return _scan_documentation(text, file=file)


def _scan_documentation(
    text: str,
    *,
    file: str,
) -> list[InventoryFinding]:
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


_REPOSITORY_SELECTED_ROOTS = (
    "tests",
    "examples",
    "docs",
    "extensions",
    "zr_vm_lib_*",
    "zr_vm_library",
)
_BINARY_ARTIFACT_SUFFIXES = {".zro", ".zri", ".zrs"}
_EMBEDDED_HOST_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".js", ".ts"}


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


def repository_candidate_paths(root: Path) -> tuple[str, ...]:
    root = root.resolve()
    return tuple(
        sorted(
            path.as_posix()
            for path in _tracked_files(root)
            if _is_repository_candidate(path)
        )
    )


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


def _repository_source_kind(relative_path: Path) -> SourceKind:
    if relative_path.suffix.lower() == ".zr":
        return SourceKind.ZR_SOURCE
    if relative_path.suffix.lower() == ".md":
        return SourceKind.CURRENT_DOCUMENTATION
    return SourceKind.EMBEDDED_ZR_FIXTURE


def build_repository_inventory(root: Path) -> InventoryReport:
    root = root.resolve()
    scanned_files: list[ScannedFile] = []
    exclusions: list[InventoryExclusion] = []
    findings: list[InventoryFinding] = []
    for relative_file in repository_candidate_paths(root):
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
        findings.extend(path_findings)

    scanned_files.sort(key=lambda entry: (entry.file, entry.source_kind.value))
    exclusions.sort(key=lambda entry: (entry.file, entry.reason))
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
        exclusions=tuple(exclusions),
        findings=tuple(findings),
        scanner_version="1",
        selected_roots=_REPOSITORY_SELECTED_ROOTS,
    )


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
