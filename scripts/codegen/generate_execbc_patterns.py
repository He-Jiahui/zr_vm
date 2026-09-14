#!/usr/bin/env python3
"""Generate deterministic ExecBC fusion metadata from the .def schema.

The generator intentionally understands only the small, symbolic schema.  A
pattern may not embed C/Python code; matcher semantics live in the C contract
and are selected by constraint bits.  ``--check`` validates the source without
writing, while the default mode writes the generated header atomically.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import tempfile
from typing import Iterable, Sequence


ROW_RE = re.compile(r"ZR_EXECBC_FUSION\s*\(")
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
OPCODE_NAMES = {
    "NOP",
    "CONSTANT",
    "CONVERT",
    "ARITHMETIC",
    "COPY",
    "MOVE",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "NEG",
    "COMPARE",
    "PLACE_BASE",
    "PLACE_PROJECT",
    "LOAD",
    "STORE",
    "CALL",
    "ALLOC",
    "BARRIER",
    "DROP",
    "BRANCH",
    "CONDITIONAL_BRANCH",
    "SWITCH",
    "INVOKE",
    "THROW",
    "SUSPEND",
    "RETURN",
    "PHI",
}
CONSTRAINT_NAMES = {
    "ZR_EXEC_BC_FUSION_CONSTRAINT_NONE",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_TYPED_OPERANDS",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_SAME_RESULT_OPERAND",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_NO_INTERVENING_EFFECT",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_RESULT_SINGLE_USE",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_LAYOUT_PROVEN",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_BINDING_RESOLVED",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_BRANCH_TARGET",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_PRESERVE_BOUNDARY",
    "ZR_EXEC_BC_FUSION_CONSTRAINT_INDEX_STORE_VARIANT",
}
RESULT_NAMES = {
    "ZR_EXEC_BC_FUSION_RESULT_NONE",
    "ZR_EXEC_BC_FUSION_RESULT_OF_HEAD",
    "ZR_EXEC_BC_FUSION_RESULT_OF_TAIL",
}
BOUNDARY_NAMES = {
    "ZR_EXEC_BC_FUSION_BOUNDARY_NONE",
    "ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG",
    "ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT",
    "ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION",
    "ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT",
}


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def split_fields(payload: str) -> list[str]:
    fields: list[str] = []
    start = 0
    depth = 0
    for index, char in enumerate(payload):
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth < 0:
                raise ValueError("unbalanced ')' in pattern row")
        elif char == "," and depth == 0:
            fields.append(payload[start:index].strip())
            start = index + 1
    if depth != 0:
        raise ValueError("unbalanced '(' in pattern row")
    fields.append(payload[start:].strip())
    return fields


def find_rows(text: str) -> list[tuple[str, ...]]:
    text = strip_comments(text)
    rows: list[tuple[str, ...]] = []
    cursor = 0
    while True:
        match = ROW_RE.search(text, cursor)
        if match is None:
            break
        opening = match.end() - 1
        depth = 1
        index = opening + 1
        while index < len(text) and depth:
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
            index += 1
        if depth:
            raise ValueError("unterminated ZR_EXECBC_FUSION row")
        fields = split_fields(text[opening + 1 : index - 1])
        if len(fields) != 7:
            raise ValueError(
                f"expected 7 fields, got {len(fields)} in row: {fields!r}"
            )
        rows.append(tuple(fields))
        cursor = index
    if not rows:
        raise ValueError("pattern schema has no rows")
    return rows


def read_schema(path: pathlib.Path, include_stack: tuple[pathlib.Path, ...] = ()) -> str:
    """Read a schema and expand only local quoted includes.

    Keeping include expansion here lets older build scripts use the former
    ``execbc_fusion_patterns.def`` spelling while the canonical source remains
    ``execbc_patterns.def``.  Includes are bounded and path-local so a schema
    cannot pull arbitrary files into generated metadata.
    """

    path = path.resolve()
    if path in include_stack:
        chain = " -> ".join(str(item) for item in (*include_stack, path))
        raise ValueError(f"recursive pattern schema include: {chain}")
    text = path.read_text(encoding="utf-8")

    def expand(match: re.Match[str]) -> str:
        included = (path.parent / match.group(1)).resolve()
        base = path.parent.resolve()
        try:
            included.relative_to(base)
        except ValueError:
            raise ValueError(f"pattern include escapes schema directory: {match.group(1)}")
        return read_schema(included, (*include_stack, path))

    return re.sub(r"^\s*#include\s+\"([^\"]+)\"\s*$", expand,
                  text, flags=re.MULTILINE)


def validate_rows(rows: Sequence[tuple[str, ...]]) -> None:
    names: set[str] = set()
    for name, head, tail, constraints, result, benefit, boundary in rows:
        for value in (name, head, tail):
            if not IDENT_RE.match(value):
                raise ValueError(f"invalid identifier {value!r}")
        if name in names:
            raise ValueError(f"duplicate pattern {name}")
        names.add(name)
        if head not in OPCODE_NAMES or tail not in OPCODE_NAMES:
            raise ValueError(f"unknown ExecIR opcode in {name}: {head}/{tail}")
        for value, label, known in (
            (constraints, "constraint", CONSTRAINT_NAMES),
            (result, "result", RESULT_NAMES),
            (boundary, "boundary", BOUNDARY_NAMES),
        ):
            # Constraint and boundary fields may be OR expressions.  Keep the
            # accepted language symbolic and intentionally narrow.
            parts = [part.strip() for part in value.split("|")]
            if (not parts or any(not IDENT_RE.match(part) for part in parts) or
                    any(part not in known for part in parts)):
                raise ValueError(f"invalid {label} expression in {name}: {value}")
            if len(parts) != len(set(parts)):
                raise ValueError(f"duplicate {label} in {name}: {value}")
        # ``str.isdigit`` accepts non-ASCII numerals which would render as
        # invalid C tokens (for example ``１２u``).  Keep the schema's numeric
        # language deliberately ASCII and deterministic.
        if re.fullmatch(r"[0-9]+", benefit) is None or int(benefit) == 0:
            raise ValueError(f"dispatch benefit must be positive in {name}")


def render(rows: Iterable[tuple[str, ...]]) -> str:
    rows = list(rows)
    lines = [
        "/* Generated by scripts/codegen/generate_execbc_patterns.py. */",
        "/* Do not edit this file; change execbc_patterns.def instead. */",
        "#ifndef ZR_VM_PARSER_EXECBC_FUSION_PATTERNS_GENERATED_H",
        "#define ZR_VM_PARSER_EXECBC_FUSION_PATTERNS_GENERATED_H",
        "",
        f"#define ZR_EXEC_BC_FUSION_GENERATED_PATTERN_COUNT {len(rows)}u",
        "",
        "#define ZR_EXEC_BC_FUSION_PATTERN_ROWS(X) \\",
    ]
    for index, row in enumerate(rows):
        suffix = (" " + "\\") if index + 1 < len(rows) else ""
        name, head, tail, constraints, result, benefit, boundary = row
        lines.append(
            f"    X({name}, ZR_EXEC_IR_OPCODE_{head}, ZR_EXEC_IR_OPCODE_{tail}, "
            f"{constraints}, {result}, {benefit}u, {boundary}){suffix}"
        )
    lines.extend(["", "#endif /* ZR_VM_PARSER_EXECBC_FUSION_PATTERNS_GENERATED_H */", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    rows = find_rows(read_schema(args.input))
    validate_rows(rows)
    generated = render(rows)
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != generated:
            raise SystemExit("generated fusion metadata is stale; regenerate it")
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=args.output.parent, delete=False
    ) as temporary:
        temporary.write(generated)
        temporary_path = pathlib.Path(temporary.name)
    temporary_path.replace(args.output)
    return 0


if __name__ == "__main__":
    main()
