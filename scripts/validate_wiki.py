#!/usr/bin/env python3
"""Validate the source contract and generated entry point for the ZrVm wiki.

The checker intentionally uses only the Python standard library so that it can
run before the documentation renderer is installed in CI or in a contributor's
checkout.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable, NamedTuple
from urllib.parse import unquote


REQUIRED_FRONT_MATTER = (
    "related_code",
    "implementation_files",
    "plan_sources",
    "tests",
    "doc_type",
)
EXTERNAL_SCHEMES = ("http:", "https:", "mailto:", "tel:", "data:")
LINK_PATTERN = re.compile(r"\]\((?:<(?P<bracketed>[^>]+)>|(?P<plain>[^)\s]+))\)")
HEADING_PATTERN = re.compile(r"^\s{0,3}(?P<marks>#{1,6})\s+(?P<title>.+?)\s*#*\s*$")
FRONT_MATTER_KEY_PATTERN = re.compile(r"^(?P<key>[A-Za-z_][A-Za-z0-9_-]*)\s*:")


class ValidationResult(NamedTuple):
    errors: tuple[str, ...]
    markdown_files: int
    manifest_pages: int
    local_links: int
    site_files: int = 0


def _relative_path(path: Path, base: Path) -> str:
    """Return a stable, repository-style path for diagnostics."""

    try:
        return path.resolve().relative_to(base.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def _inside(path: Path, directory: Path) -> bool:
    try:
        path.resolve().relative_to(directory.resolve())
    except ValueError:
        return False
    return True


def _read_utf8(path: Path, errors: list[str], docs_root: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        errors.append(f"{_relative_path(path, docs_root)} is not valid UTF-8: {exc}")
    except OSError as exc:
        errors.append(f"cannot read {_relative_path(path, docs_root)}: {exc}")
    return None


def _front_matter_end(lines: list[str]) -> int | None:
    if not lines or lines[0].strip() != "---":
        return None
    for index in range(1, len(lines)):
        if lines[index].strip() == "---":
            return index
    return None


def _validate_markdown(path: Path, docs_root: Path, errors: list[str]) -> str | None:
    text = _read_utf8(path, errors, docs_root)
    if text is None:
        return None

    relative = _relative_path(path, docs_root)
    lines = text.splitlines()
    front_matter_end = _front_matter_end(lines)
    if front_matter_end is None:
        if lines and lines[0].strip() == "---":
            errors.append(f"{relative} has unterminated front matter")
        else:
            errors.append(f"{relative} is missing front matter")
    else:
        fields = {
            match.group("key")
            for line in lines[1:front_matter_end]
            if (match := FRONT_MATTER_KEY_PATTERN.match(line))
        }
        for field in REQUIRED_FRONT_MATTER:
            if field not in fields:
                errors.append(f"{relative} missing front matter field: {field}")

    fence_count = 0
    for line_number, line in enumerate(lines, start=1):
        if line.rstrip(" \t") != line:
            errors.append(f"{relative}:{line_number} has trailing whitespace")
        if re.match(r"^\s{0,3}#{1,6}\s*$", line):
            errors.append(f"{relative}:{line_number} has an empty heading")
        if re.match(r"^\s{0,3}(```|~~~)", line):
            fence_count += 1
    if fence_count % 2:
        errors.append(f"{relative} has an unbalanced fenced code block")
    return text


def _slugify_heading(title: str) -> str:
    """Approximate the slug used by common Markdown documentation themes."""

    title = re.sub(r"[`*_~]", "", title).strip().lower()
    title = re.sub(r"[^\w\-\u0080-\uffff ]", "", title, flags=re.UNICODE)
    return re.sub(r"[\s-]+", "-", title).strip("-")


def _heading_anchors(text: str) -> set[str]:
    anchors: set[str] = set()
    slug_counts: dict[str, int] = {}
    for line in _content_lines(text):
        match = HEADING_PATTERN.match(line)
        if not match:
            continue
        slug = _slugify_heading(match.group("title"))
        if not slug:
            continue
        occurrence = slug_counts.get(slug, 0)
        slug_counts[slug] = occurrence + 1
        anchors.add(slug if occurrence == 0 else f"{slug}-{occurrence}")
    return anchors


def _manifest_path(manifest: dict, docs_root: Path, errors: list[str]) -> tuple[set[str], int]:
    pages = manifest.get("pages")
    if not isinstance(pages, list):
        errors.append("manifest pages must be an array")
        return set(), 0

    page_paths: set[str] = set()
    page_ids: set[str] = set()
    for index, page in enumerate(pages):
        if not isinstance(page, dict):
            errors.append(f"manifest pages[{index}] must be an object")
            continue
        page_id = page.get("id")
        raw_path = page.get("path")
        if not isinstance(page_id, str) or not page_id:
            errors.append(f"manifest pages[{index}] has an invalid id")
        elif page_id in page_ids:
            errors.append(f"manifest has duplicate page id: {page_id}")
        else:
            page_ids.add(page_id)
        if not isinstance(raw_path, str) or not raw_path:
            errors.append(f"manifest pages[{index}] has an invalid path")
            continue
        normalized = Path(raw_path.replace("\\", "/"))
        if normalized.is_absolute() or ".." in normalized.parts:
            errors.append(f"manifest page path escapes docs root: {raw_path}")
            continue
        path = (docs_root / normalized).resolve()
        normalized_text = normalized.as_posix()
        if normalized_text in page_paths:
            errors.append(f"manifest has duplicate page path: {normalized_text}")
        page_paths.add(normalized_text)
        if not _inside(path, docs_root):
            errors.append(f"manifest page path escapes docs root: {raw_path}")
        elif not path.is_file():
            errors.append(f"manifest page target does not exist: {raw_path}")
        elif path.suffix.lower() != ".md":
            errors.append(f"manifest page target is not Markdown: {raw_path}")
    return page_paths, len(pages)


def _validate_manifest(manifest_path: Path, docs_root: Path, errors: list[str]) -> tuple[int, set[str]]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        errors.append(f"cannot parse {_relative_path(manifest_path, docs_root)}: {exc}")
        return 0, set()
    if not isinstance(manifest, dict):
        errors.append("manifest root must be an object")
        return 0, set()
    if manifest.get("schema") != 1:
        errors.append("manifest schema must be 1")

    sections = manifest.get("sections")
    section_ids: set[str] = set()
    if not isinstance(sections, list):
        errors.append("manifest sections must be an array")
    else:
        for index, section in enumerate(sections):
            if not isinstance(section, dict):
                errors.append(f"manifest sections[{index}] must be an object")
                continue
            section_id = section.get("id")
            raw_path = section.get("path")
            if not isinstance(section_id, str) or not section_id:
                errors.append(f"manifest sections[{index}] has an invalid id")
            elif section_id in section_ids:
                errors.append(f"manifest has duplicate section id: {section_id}")
            else:
                section_ids.add(section_id)
            if not isinstance(raw_path, str) or not raw_path:
                errors.append(f"manifest sections[{index}] has an invalid path")
                continue
            normalized = Path(raw_path.replace("\\", "/"))
            if normalized.is_absolute() or ".." in normalized.parts:
                errors.append(f"manifest section path escapes docs root: {raw_path}")
                continue
            target = (docs_root / normalized).resolve()
            if not _inside(target, docs_root) or not target.is_file():
                errors.append(f"manifest section target does not exist: {raw_path}")

    page_paths, page_count = _manifest_path(manifest, docs_root, errors)
    expected_paths = {
        path.relative_to(docs_root).as_posix()
        for path in docs_root.rglob("*.md")
        if path.relative_to(docs_root).as_posix() != "README.md"
    }
    for missing in sorted(expected_paths - page_paths):
        errors.append(f"Markdown file missing from manifest: {missing}")
    for extra in sorted(page_paths - expected_paths):
        errors.append(f"manifest page is not a wiki Markdown file: {extra}")
    for index, page in enumerate(manifest.get("pages", []) if isinstance(manifest.get("pages"), list) else []):
        if not isinstance(page, dict):
            continue
        parent = page.get("parent")
        if parent is not None and parent not in section_ids:
            errors.append(f"manifest pages[{index}] references unknown parent: {parent}")
    return page_count, page_paths


def _is_external_link(target: str) -> bool:
    lowered = target.lower()
    return lowered.startswith(EXTERNAL_SCHEMES) or target.startswith("/")


def _content_lines(text: str) -> Iterable[str]:
    """Yield Markdown lines outside fenced code blocks."""

    fence_character: str | None = None
    fence_length = 0
    for line in text.splitlines(keepends=True):
        marker = re.match(r"^\s{0,3}(?P<fence>`{3,}|~{3,})", line)
        if marker:
            fence = marker.group("fence")
            if fence_character is None:
                fence_character = fence[0]
                fence_length = len(fence)
            elif fence[0] == fence_character and len(fence) >= fence_length:
                fence_character = None
                fence_length = 0
            continue
        if fence_character is None:
            yield line


def _validate_links(documents: dict[Path, str], docs_root: Path, errors: list[str]) -> int:
    local_links = 0
    anchor_cache = {path: _heading_anchors(text) for path, text in documents.items()}
    for source, text in documents.items():
        relative_source = _relative_path(source, docs_root)
        for line in _content_lines(text):
            for match in LINK_PATTERN.finditer(line):
                target = match.group("bracketed") or match.group("plain") or ""
                target = unquote(target).strip()
                if not target or _is_external_link(target):
                    continue
                path_part, separator, anchor = target.partition("#")
                if not path_part:
                    target_path = source
                else:
                    target_path = (source.parent / path_part).resolve()
                local_links += 1
                if not _inside(target_path, docs_root) or not target_path.is_file():
                    errors.append(f"{relative_source} has missing link target: {target}")
                    continue
                if target_path.suffix.lower() != ".md":
                    continue
                if separator and anchor and anchor.lower() not in anchor_cache.get(target_path, set()):
                    errors.append(f"{relative_source} has missing link anchor: {target}")
    return local_links


def validate(root: Path, *, site_dir: Path | None = None) -> ValidationResult:
    """Validate a checkout and return structured counts plus all errors."""

    root = Path(root).resolve()
    docs_root = root / "docs" / "wiki"
    errors: list[str] = []
    if not docs_root.is_dir():
        return ValidationResult((f"wiki directory does not exist: {docs_root}",), 0, 0, 0, 0)

    markdown_files = sorted(docs_root.rglob("*.md"))
    documents: dict[Path, str] = {}
    for path in markdown_files:
        text = _validate_markdown(path, docs_root, errors)
        if text is not None:
            documents[path.resolve()] = text

    manifest_path = docs_root / "manifest.json"
    if not manifest_path.is_file():
        errors.append("wiki manifest does not exist: docs/wiki/manifest.json")
        manifest_pages = 0
    else:
        manifest_pages, _ = _validate_manifest(manifest_path, docs_root, errors)

    local_links = _validate_links(documents, docs_root, errors)

    site_files = 0
    if site_dir is not None:
        site_dir = Path(site_dir)
        if not site_dir.is_absolute():
            site_dir = root / site_dir
        site_dir = site_dir.resolve()
        if not site_dir.is_dir() or not (site_dir / "index.html").is_file():
            errors.append(f"site output missing index.html: {site_dir}")
        else:
            try:
                site_files = sum(1 for item in site_dir.rglob("*") if item.is_file())
                if (site_dir / "index.html").stat().st_size == 0:
                    errors.append(f"site output index.html is empty: {site_dir}")
            except OSError as exc:
                errors.append(f"cannot inspect site output {site_dir}: {exc}")

    return ValidationResult(tuple(errors), len(markdown_files), manifest_pages, local_links, site_files)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path.cwd(),
        help="repository root (default: current directory)",
    )
    parser.add_argument(
        "--site-dir",
        type=Path,
        help="also require a generated site/index.html",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    result = validate(args.root, site_dir=args.site_dir)
    for error in result.errors:
        print(f"ERROR: {error}", file=sys.stderr)
    summary = (
        f"wiki validation: {result.markdown_files} Markdown files, "
        f"{result.manifest_pages} manifest pages, {result.local_links} local links"
    )
    if args.site_dir is not None:
        summary += f", {result.site_files} site files"
    print(summary)
    if result.errors:
        print(f"wiki validation failed: {len(result.errors)} error(s)", file=sys.stderr)
        return 1
    print("wiki validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
