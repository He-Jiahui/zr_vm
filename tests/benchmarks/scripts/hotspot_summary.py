#!/usr/bin/env python3
"""Summarize instruction-profile and callgrind artifacts for hotspot_report."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


CALLGRIND_TOTAL_RE = re.compile(r"^\s*([0-9,]+)\s+\(([\d.]+)%\)\s+PROGRAM TOTALS\s*$")
CALLGRIND_ENTRY_RE = re.compile(r"^\s*([0-9,]+)\s+\(([ \d.]+)%\)\s+(.+?)\s*$")
HELPER_HINT_RE = re.compile(
    r"(Value_Copy|Value_Reset|Stack_GetValue|SuperArray|GetMember|SetMember|GetByIndex|SetByIndex|PreCall|callsite)",
    re.IGNORECASE,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--case", required=True)
    parser.add_argument("--instruction-profile", required=True)
    parser.add_argument("--callgrind-out", required=True)
    parser.add_argument("--callgrind-annotate", required=True)
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--markdown-out", required=True)
    parser.add_argument("--top-n", type=int, default=3)
    return parser.parse_args()


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def format_profile_entries(entries: list[dict], top_n: int) -> list[dict]:
    ranked = sorted(entries, key=lambda item: int(item.get("count", 0)), reverse=True)
    return ranked[:top_n]


def profile_metric_map(profile: dict) -> dict[str, int]:
    return {
        str(item.get("name", "")): int(item.get("count", 0))
        for item in profile.get("memory", [])
        if item.get("name")
    }


def derived_profile_metrics(profile: dict) -> dict[str, float]:
    metrics = profile_metric_map(profile)
    allocation_count = metrics.get("allocation_count", 0)
    mark_count = metrics.get("mark_object_count", 0)
    return {
        "allocation_bytes_per_allocation": (
            metrics.get("allocation_bytes", 0) / allocation_count if allocation_count else 0.0
        ),
        "scan_bytes_per_marked_object": (
            metrics.get("scan_bytes", 0) / mark_count if mark_count else 0.0
        ),
    }


def parse_callgrind_annotate(path: Path, top_n: int) -> tuple[str, list[dict], dict | None]:
    total_ir = ""
    top_functions: list[dict] = []
    top_helper: dict | None = None
    seen_table_header = False

    with path.open("r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")
            if not total_ir:
                total_match = CALLGRIND_TOTAL_RE.match(line)
                if total_match:
                    total_ir = total_match.group(1)
                    continue

            if "file:function" in line and line.lstrip().startswith("Ir"):
                seen_table_header = True
                continue

            if not seen_table_header:
                continue

            if not line.strip():
                if top_functions:
                    break
                continue

            entry_match = CALLGRIND_ENTRY_RE.match(line)
            if entry_match is None:
                continue

            entry = {
                "ir": entry_match.group(1),
                "percent": entry_match.group(2).strip(),
                "symbol": entry_match.group(3),
            }
            if len(top_functions) < top_n:
                top_functions.append(entry)
            if top_helper is None and HELPER_HINT_RE.search(entry["symbol"]):
                top_helper = entry

            if len(top_functions) >= top_n and top_helper is not None:
                continue

    return total_ir, top_functions, top_helper


def render_lines(entries: list[dict], *, key_name: str) -> list[str]:
    lines = []
    for entry in entries:
        lines.append(f"{entry[key_name]} = {entry['count']}")
    return lines


def render_callgrind_lines(entries: list[dict]) -> list[str]:
    lines = []
    for entry in entries:
        lines.append(f"{entry['percent']}% | {entry['ir']} Ir | {entry['symbol']}")
    return lines


def build_markdown(summary: dict) -> str:
    lines = [
        f"### {summary['name']}",
        f"- Instruction profile: `{summary['instruction_profile']}`",
        f"- Callgrind artifact: `{summary['callgrind_out']}`",
        f"- Callgrind annotate: `{summary['callgrind_annotate']}`",
        f"- Callgrind total Ir: `{summary['callgrind']['total_ir'] or 'unknown'}`",
        "- Top functions:",
    ]
    top_functions = render_callgrind_lines(summary["callgrind"]["top_functions"])
    if top_functions:
        lines.extend(f"  {line}" for line in top_functions)
    else:
        lines.append("  unavailable")

    lines.append("- Top helpers:")
    top_helpers = render_lines(summary["profile"]["top_helpers"], key_name="name")
    if top_helpers:
        lines.extend(f"  {line}" for line in top_helpers)
    else:
        lines.append("  none recorded")

    lines.append("- Top slowpaths:")
    top_slowpaths = render_lines(summary["profile"]["top_slowpaths"], key_name="name")
    if top_slowpaths:
        lines.extend(f"  {line}" for line in top_slowpaths)
    else:
        lines.append("  none recorded")

    memory_rates = summary["profile"]["derived_memory_metrics"]
    lines.append(
        "- Memory rates: "
        f"`{memory_rates['allocation_bytes_per_allocation']:.2f}` allocation bytes/allocation, "
        f"`{memory_rates['scan_bytes_per_marked_object']:.2f}` scan bytes/marked object"
    )

    if summary["callgrind"]["top_helper"] is not None:
        helper = summary["callgrind"]["top_helper"]
        lines.append(
            f"- Top helper function in callgrind: `{helper['percent']}% | {helper['ir']} Ir | {helper['symbol']}`"
        )

    lines.append("")
    return "\n".join(lines)


def main() -> int:
    args = parse_args()

    instruction_profile_path = Path(args.instruction_profile)
    callgrind_out_path = Path(args.callgrind_out)
    callgrind_annotate_path = Path(args.callgrind_annotate)
    json_out_path = Path(args.json_out)
    markdown_out_path = Path(args.markdown_out)

    profile = read_json(instruction_profile_path)
    total_ir, top_functions, top_helper = parse_callgrind_annotate(callgrind_annotate_path, args.top_n)

    summary = {
        "name": args.case,
        "status": "available",
        "instruction_profile": str(instruction_profile_path),
        "callgrind_out": str(callgrind_out_path),
        "callgrind_annotate": str(callgrind_annotate_path),
        "callgrind": {
            "total_ir": total_ir,
            "top_functions": top_functions,
            "top_helper": top_helper,
        },
        "profile": {
            "top_instructions": format_profile_entries(profile.get("instructions", []), args.top_n),
            "top_helpers": format_profile_entries(profile.get("helpers", []), args.top_n),
            "top_slowpaths": format_profile_entries(profile.get("slowpaths", []), args.top_n),
            "derived_memory_metrics": derived_profile_metrics(profile),
        },
    }

    json_out_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    markdown_out_path.write_text(build_markdown(summary), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
