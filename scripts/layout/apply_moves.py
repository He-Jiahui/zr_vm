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
EXCLUDED_PREFIXES = (
    "lua/", "tests/third_party/", "third_party/", ".git/", ".codex/",
    "tests/acceptance/", "scripts/layout/",
)
TEXT_SUFFIXES = {
    ".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".inc", ".def",
    ".cmake", ".gdb", ".py", ".ps1", ".sh", ".md", ".txt", ".json",
    ".yml", ".yaml", ".toml", ".rs", ".js", ".ts", ".html", ".bat",
}
SCAN_PREFIXES = ("zr_vm_", "tests/", "scripts/", "docs/")


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


SAFE_TASK_TMP, REGISTERED_FIXTURE_ROOT = _registered_task_roots()
INCLUDE_RE = re.compile(rb'(?P<head>\#\s*include\s*")(?P<name>[^"\r\n]+)(?P<tail>")')
LEFT_PATH_CHAR = rb"A-Za-z0-9_.-"
RIGHT_PATH_CHAR = rb"A-Za-z0-9_.\\/-"


class MigrationError(Exception):
    """An unsafe, stale, or inconsistent migration input."""


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


@dataclass(frozen=True)
class Move:
    old: str
    new: str


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def repo_path(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def has_reparse(path: Path) -> bool:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    attrs = getattr(info, "st_file_attributes", 0)
    return stat.S_ISLNK(info.st_mode) or bool(attrs & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def safe_relative(value: str, label: str) -> str:
    value = value.replace("\\", "/")
    p = PurePosixPath(value)
    if not value or p.is_absolute() or re.match(r"^[A-Za-z]:", value):
        raise MigrationError(f"{label} must be a nonempty repository-relative path: {value!r}")
    if any(part in ("", ".", "..") for part in value.split("/")):
        raise MigrationError(f"{label} contains an unsafe path component: {value!r}")
    return p.as_posix()


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


def is_scan_file(rel: str, path: Path) -> bool:
    if excluded(rel) or not rel.startswith(SCAN_PREFIXES):
        return rel in {"CMakeLists.txt", "README.md"} and path.is_file()
    return path.suffix.lower() in TEXT_SUFFIXES or path.name in ("CMakeLists.txt",)


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


@lru_cache(maxsize=100000)
def canonical_path(path: Path, strict: bool) -> Path:
    """Cache exact Path.resolve semantics for the duration of a scan pass."""
    return path.resolve(strict=strict)


def clear_path_caches() -> None:
    canonical_path.cache_clear()
    resolve_file.cache_clear()
    load_cmake_fallback_context.cache_clear()


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


def context_variants(context: dict[str, tuple] | None, including: str) -> tuple[tuple[Path, ...], ...]:
    value = (context or {}).get(including)
    if not value:
        return ()
    if isinstance(value[0], Path):
        return (tuple(value),)
    return tuple(tuple(variant) for variant in value)


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


@lru_cache(maxsize=100000)
def resolve_file(candidate: Path, root: Path) -> str | None:
    try:
        rel = repo_path(canonical_path(candidate, True), canonical_path(root, False))
    except (FileNotFoundError, ValueError, OSError):
        return None
    if candidate.is_file():
        return rel
    return None


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


def validate_move_aliases(moves: list[Move]) -> None:
    choices: dict[str, tuple[str, bool]] = {}
    for move in moves:
        add_move_aliases(choices, move)


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


def add_path_alias(choices: dict[str, tuple[str, bool]], alias: str, replacement: str, is_directory: bool) -> None:
    key = alias.replace("\\", "/").rstrip("/")
    value = replacement.replace("\\", "/").rstrip("/")
    prior = choices.get(key)
    if prior and (prior[0] != value or prior[1] != is_directory):
        raise MigrationError(f"conflicting path rewrite alias {alias!r}: {prior[0]} vs {value}")
    choices[key] = (value, is_directory)


class PathRewriter:
    def __init__(self, trie: dict, protected_trie: dict | None = None):
        self.trie = trie
        self.protected_trie = protected_trie or {}

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


_PATH_TOKEN = re.compile(
    rb"(?<![A-Za-z0-9_.-])(?:[A-Za-z]:[\\/])?"
    rb"[A-Za-z0-9_.-]+(?:[\\/][A-Za-z0-9_.-]+)+(?![A-Za-z0-9_.-])"
)


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

    def replace_tokens(segment: bytes) -> bytes:
        return replace_path_tokens(segment, moves, root, dir_pairs, compiled_rewriter)

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


def validate_plan(plan_path: Path, expected: dict) -> None:
    try:
        pinned = json.loads(plan_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise MigrationError(f"cannot read plan {plan_path}: {exc}") from exc
    if pinned != expected:
        raise MigrationError("pinned plan is stale: mapping, move inputs, or rewrite inputs changed")


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
