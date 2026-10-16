"""Safely classify quoted includes that may resolve to a generated test header.

The module never reads file contents and never returns an external file as a
rewrite target. It preserves each ordered compile-context variant separately.
"""
from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path, PureWindowsPath
import stat
from typing import Literal


GENERATED_HEADER = "zr_vm_ffi_fixture_path.h"
_FORBIDDEN_ROOTS = (
    ".git", ".codex", "lua", "third_party", "vendor", "acceptance",
    "snapshot", "snapshots", "history",
)


@dataclass(frozen=True, slots=True)
class IncludeResolution:
    """One compile variant's first-existing include result."""

    kind: Literal["repository", "generated", "unresolved", "unsafe"]
    path: Path | None = None
    relative_path: str | None = None
    variant_index: int = 0


def _absolute(path: Path, base: Path) -> Path:
    return Path(os.path.abspath(path if path.is_absolute() else base / path))


def _under(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _has_reparse_component(path: Path) -> bool:
    """Check lexical ancestors without following links or junctions."""
    absolute = Path(os.path.abspath(path))
    parts = absolute.parts
    if not parts:
        return False
    cursor = Path(parts[0])
    for component in parts[1:]:
        cursor = cursor / component
        try:
            info = cursor.lstat()
        except FileNotFoundError:
            continue
        except OSError:
            return True
        attrs = getattr(info, "st_file_attributes", 0)
        if stat.S_ISLNK(info.st_mode) or attrs & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400):
            return True
    return False


def _excluded(relative: Path) -> bool:
    return any(part.casefold() in _FORBIDDEN_ROOTS for part in relative.parts)


def _regular_file(path: Path) -> bool:
    try:
        return stat.S_ISREG(path.lstat().st_mode)
    except OSError:
        return False


def _first_existing(
    root: Path,
    build: Path,
    source_dir: Path,
    name: str,
    roots: tuple[Path, ...],
    variant_index: int,
) -> IncludeResolution:
    generated_dir = build / "tests_generated"
    generated_file = generated_dir / GENERATED_HEADER
    candidates = [source_dir / name]
    candidates.extend(_absolute(include_root, root) / name for include_root in roots)

    for raw_candidate in candidates:
        candidate = Path(os.path.abspath(raw_candidate))
        in_repo = _under(candidate, root)
        is_exact_generated = name == GENERATED_HEADER and candidate == generated_file
        in_generated_dir = _under(candidate, generated_dir)

        if _has_reparse_component(candidate):
            # A linked candidate that exists in the search order can shadow later roots.
            if candidate.exists() or candidate.is_symlink():
                return IncludeResolution("unsafe", candidate, None, variant_index)
            continue

        if in_repo:
            relative = candidate.relative_to(root)
            if _excluded(relative):
                if candidate.exists():
                    return IncludeResolution("unsafe", candidate, None, variant_index)
                continue
            if _regular_file(candidate):
                resolved = candidate.resolve(strict=True)
                return IncludeResolution("repository", resolved, relative.as_posix(), variant_index)
            continue

        if is_exact_generated:
            if not _regular_file(candidate):
                if candidate.exists():
                    return IncludeResolution("unsafe", candidate, None, variant_index)
                continue
            return IncludeResolution("generated", candidate.resolve(strict=True), None, variant_index)

        # A present file outside the checkout and the one exact generated output
        # could shadow a later safe candidate, so surface it as unsafe.
        if candidate.exists():
            return IncludeResolution("unsafe", candidate, None, variant_index)
        # A different file below this build's generated directory is forbidden.
        if in_generated_dir and name != GENERATED_HEADER:
            continue

    return IncludeResolution("unresolved", None, None, variant_index)


def classify_include(
    root: Path,
    build: Path,
    including: str,
    name: str,
    variants: tuple[tuple[Path, ...], ...],
) -> tuple[IncludeResolution, ...]:
    """Classify a quoted include independently for each ordered compile variant.

    The including source's directory is searched first, then each variant's
    include roots in order. Only a regular direct child named
    ``tests_generated/zr_vm_ffi_fixture_path.h`` under the supplied build root
    receives the ``generated`` classification. All other external files are
    unsafe when present. No contents are read.
    """
    if not isinstance(root, Path) or not isinstance(build, Path):
        raise TypeError("root and build must be pathlib.Path values")
    root_abs = _absolute(root, Path.cwd())
    build_abs = _absolute(build, Path.cwd())
    if _has_reparse_component(root_abs) or _has_reparse_component(build_abs):
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))
    try:
        root_real = root_abs.resolve(strict=True)
        build_real = build_abs.resolve(strict=True)
    except OSError:
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))
    if not root_real.is_dir() or not build_real.is_dir():
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))

    normalized = name.replace("\\", "/")
    name_path = Path(normalized)
    windows_name = PureWindowsPath(normalized)
    if (not normalized or name_path.is_absolute() or windows_name.is_absolute()
            or windows_name.drive or "\x00" in normalized):
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))
    if not isinstance(including, str):
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))
    source_path = Path(including.replace("\\", "/"))
    if source_path.is_absolute():
        source_abs = _absolute(source_path, root_real)
    else:
        source_abs = _absolute(source_path, root_real)
    if _has_reparse_component(source_abs):
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))
    try:
        source_abs.relative_to(root_real)
    except ValueError:
        return tuple(IncludeResolution("unsafe", None, None, i) for i in range(len(variants)))
    source_dir = source_abs.parent

    results: list[IncludeResolution] = []
    for index, variant in enumerate(variants):
        roots_abs = tuple(_absolute(path, root_real) for path in variant)
        results.append(_first_existing(root_real, build_real, source_dir, normalized, roots_abs, index))
    return tuple(results)
