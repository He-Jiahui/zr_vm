"""Recover isolated include contexts for test sources omitted by File API.

This module deliberately does not merge recovered contexts into the active
File API graph. A caller can validate a dormant source and its dormant headers
separately while preserving every active compile-group variant unchanged.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import stat
from typing import Iterable


class DormantContextError(RuntimeError):
    """Raised when a File API reply is present but cannot be read safely."""


@dataclass(frozen=True)
class SourceContext:
    status: str
    variants: tuple[tuple[Path, ...], ...] = ()
    owners: tuple[str, ...] = ()
    evidence: tuple[str, ...] = ()


@dataclass(frozen=True)
class _Registration:
    target: str
    cmake_file: str
    command: str


_SOURCE_COMMANDS = {
    "add_executable",
    "add_library",
    "zr_vm_add_unity_test_target",
    "zr_vm_add_support_target",
}
_SCOPE_KEYWORDS = {"PRIVATE", "PUBLIC", "INTERFACE", "SYSTEM", "BEFORE"}
_COMMENT_RE = re.compile(r"(?m)#.*$")
_COMMAND_START_RE = re.compile(r"(?i)(?<![\w])([a-z_]\w*)\s*\(")
_TOKEN_RE = re.compile(r'"((?:\\.|[^"\\])*)"|([^\s;]+)')
_INCLUDE_RE = re.compile(rb'#\s*include\s*"(?P<name>[^"\r\n]+)"')


def _strip_comments(text: str) -> str:
    """Remove ordinary CMake comments without treating quoted # as comments."""
    output: list[str] = []
    quoted = False
    escaped = False
    comment = False
    for char in text:
        if comment:
            if char in "\r\n":
                comment = False
                output.append(char)
            continue
        if escaped:
            output.append(char)
            escaped = False
            continue
        if char == "\\" and quoted:
            output.append(char)
            escaped = True
            continue
        if char == '"':
            quoted = not quoted
            output.append(char)
            continue
        if char == "#" and not quoted:
            comment = True
            continue
        output.append(char)
    return "".join(output)


def _commands(text: str) -> Iterable[tuple[str, str]]:
    """Yield balanced command bodies; unsupported dynamic nesting stays inert."""
    clean = _strip_comments(text)
    cursor = 0
    while match := _COMMAND_START_RE.search(clean, cursor):
        name = match.group(1).lower()
        opening = match.end() - 1
        depth = 1
        quoted = False
        escaped = False
        index = opening + 1
        while index < len(clean) and depth:
            char = clean[index]
            if escaped:
                escaped = False
            elif char == "\\" and quoted:
                escaped = True
            elif char == '"':
                quoted = not quoted
            elif not quoted and char == "(":
                depth += 1
            elif not quoted and char == ")":
                depth -= 1
            index += 1
        if depth:
            return
        yield name, clean[opening + 1:index - 1]
        cursor = index


def _tokens(body: str) -> list[str]:
    tokens: list[str] = []
    for match in _TOKEN_RE.finditer(body):
        token = match.group(1) if match.group(1) is not None else match.group(2)
        tokens.append(token.replace('\\"', '"'))
    return tokens


def _cmake_source_scope(root: Path, cmake_file: Path) -> Path:
    relative_cmake = cmake_file.resolve(strict=False).relative_to(root.resolve()).as_posix()
    if relative_cmake.startswith("tests/cmake/") and cmake_file.name != "CMakeLists.txt":
        return root / "tests"
    return cmake_file.parent


def _source_path(root: Path, cmake_file: Path, token: str) -> str | None:
    value = token.replace("\\", "/")
    source_prefix = "${CMAKE_SOURCE_DIR}/"
    if value.startswith(source_prefix):
        path = root / value[len(source_prefix):]
    elif value.startswith("${CMAKE_SOURCE_DIR}"):
        path = root / value[len("${CMAKE_SOURCE_DIR}"):].lstrip("/")
    elif value.startswith("${CMAKE_CURRENT_SOURCE_DIR}/"):
        path = _cmake_source_scope(root, cmake_file) / value[len("${CMAKE_CURRENT_SOURCE_DIR}/"):]
    elif "$" in value or "$<" in value:
        return None
    else:
        candidate = Path(value)
        path = candidate if candidate.is_absolute() else _cmake_source_scope(root, cmake_file) / candidate
    try:
        relative = path.resolve(strict=False).relative_to(root.resolve()).as_posix()
    except (OSError, ValueError):
        return None
    return relative if relative.startswith("tests/") else None


def _literal_root(root: Path, cmake_file: Path, token: str) -> Path | None:
    value = token.replace("\\", "/")
    source_prefix = "${CMAKE_SOURCE_DIR}/"
    current_prefix = "${CMAKE_CURRENT_SOURCE_DIR}/"
    if value == "${CMAKE_SOURCE_DIR}":
        candidate = root
    elif value.startswith(source_prefix):
        candidate = root / value[len(source_prefix):]
    elif value.startswith(current_prefix):
        candidate = _cmake_source_scope(root, cmake_file) / value[len(current_prefix):]
    else:
        return None
    try:
        candidate.resolve(strict=False).relative_to(root.resolve())
    except (OSError, ValueError):
        return None
    return candidate.resolve(strict=False)


def _is_reparse_or_symlink(path: Path, stat_result=None) -> bool:
    try:
        info = stat_result if stat_result is not None else os.lstat(path)
    except OSError:
        return True
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400)


def _cmake_files(root: Path, exclude=None) -> list[Path]:
    tests = root / "tests"
    if (not tests.is_dir() or _is_reparse_or_symlink(tests)
            or (exclude is not None and exclude("tests"))):
        return []
    result: list[Path] = []

    def visit(directory: Path) -> None:
        try:
            with os.scandir(directory) as scan:
                entries = sorted(scan, key=lambda entry: entry.name.lower())
        except OSError:
            return
        for entry in entries:
            path = Path(entry.path)
            relative = path.relative_to(root).as_posix()
            if exclude is not None and exclude(relative):
                continue
            try:
                info = entry.stat(follow_symlinks=False)
            except OSError:
                continue
            if _is_reparse_or_symlink(path, info):
                continue
            if entry.is_dir(follow_symlinks=False):
                visit(path)
            elif entry.is_file(follow_symlinks=False) and (
                    path.name == "CMakeLists.txt" or path.suffix.lower() == ".cmake"):
                result.append(path)

    visit(tests)
    return sorted(result)


def _registrations(root: Path, exclude=None) -> tuple[dict[str, list[_Registration]], set[str]]:
    source_owners: dict[str, list[_Registration]] = {}
    common_calls: set[str] = set()
    for cmake_file in _cmake_files(root, exclude):
        try:
            text = cmake_file.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        for command, body in _commands(text):
            tokens = _tokens(body)
            if not tokens:
                continue
            if command in _SOURCE_COMMANDS or command == "target_sources":
                target = tokens[0]
                if "$" in target or target.upper() in _SCOPE_KEYWORDS:
                    continue
                rest = tokens[1:]
                for token in rest:
                    if token.upper() in _SCOPE_KEYWORDS:
                        continue
                    source = _source_path(root, cmake_file, token)
                    if source is None:
                        continue
                    registration = _Registration(target, cmake_file.relative_to(root).as_posix(), command)
                    values = source_owners.setdefault(source, [])
                    if registration not in values:
                        values.append(registration)
            elif command == "zr_vm_apply_common_test_settings":
                if tokens[0] and "$" not in tokens[0]:
                    common_calls.add(tokens[0])
    return source_owners, common_calls


def _common_wrapper_roots(root: Path, exclude=None) -> tuple[tuple[Path, ...], set[str]]:
    top = root / "tests/CMakeLists.txt"
    if exclude is not None and exclude("tests/CMakeLists.txt"):
        return (), set()
    try:
        text = _strip_comments(top.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError):
        return (), set()
    helper = re.search(
        r"(?is)function\s*\(\s*zr_vm_apply_common_test_settings\b(.*?)\bendfunction\s*\(\s*\)", text)
    if helper is None:
        return (), set()
    roots: list[Path] = []
    for command, body in _commands(helper.group(1)):
        if command != "target_include_directories":
            continue
        tokens = _tokens(body)
        for token in tokens[1:]:
            if token.upper() in _SCOPE_KEYWORDS or "$target_name" in token.lower():
                continue
            resolved = _literal_root(root, top, token)
            if resolved is not None and resolved not in roots:
                roots.append(resolved)
    wrappers: set[str] = set()
    function_re = re.compile(r"(?is)function\s*\(\s*([a-z_]\w*)\b(.*?)\bendfunction\s*\(\s*\)")
    for match in function_re.finditer(text):
        name, body = match.group(1).lower(), match.group(2)
        if name == "zr_vm_apply_common_test_settings":
            continue
        if any(command == "zr_vm_apply_common_test_settings"
               for command, _ in _commands(body)):
            wrappers.add(name)
    return tuple(roots), wrappers


def _read_file_api(root: Path, build: Path, configuration: str) -> tuple[
        dict[str, list[tuple[Path, ...]]], dict[str, list[tuple[Path, ...]]]]:
    """Return raw per-source variants and target C-group variants."""
    reply = build / ".cmake/api/v1/reply"
    try:
        indexes = sorted(reply.glob("index-*.json"), key=lambda item: item.stat().st_mtime_ns, reverse=True)
        if not indexes:
            return {}, {}
        index = json.loads(indexes[0].read_text(encoding="utf-8"))
        codemodel_name = next(obj["jsonFile"] for obj in index.get("objects", [])
                              if obj.get("kind") == "codemodel")
        codemodel = json.loads((reply / codemodel_name).read_text(encoding="utf-8"))
    except (OSError, KeyError, StopIteration, json.JSONDecodeError) as exc:
        raise DormantContextError(f"invalid CMake File API reply in {reply}: {exc}") from exc
    configurations = codemodel.get("configurations", [])
    named = [item for item in configurations if item.get("name") == configuration]
    if len(named) == 1:
        config = named[0]
    elif len(configurations) == 1 and not configurations[0].get("name"):
        config = configurations[0]
    else:
        return {}, {}
    source_root = Path(codemodel.get("paths", {}).get("source", str(root)))
    if not source_root.is_absolute():
        source_root = root / source_root
    source_root = source_root.resolve(strict=False)
    if source_root != root.resolve(strict=False):
        raise DormantContextError(
            f"CMake File API source root {source_root} does not match requested checkout {root.resolve(strict=False)}")
    raw: dict[str, list[tuple[Path, ...]]] = {}
    targets: dict[str, list[tuple[Path, ...]]] = {}
    for target_ref in config.get("targets", []):
        try:
            target = json.loads((reply / target_ref["jsonFile"]).read_text(encoding="utf-8"))
        except (OSError, KeyError, json.JSONDecodeError) as exc:
            raise DormantContextError(f"invalid CMake target reply {target_ref}: {exc}") from exc
        target_name = target.get("name", target_ref.get("name", ""))
        sources = target.get("sources", [])
        for group in target.get("compileGroups", []):
            if group.get("language") != "C":
                continue
            roots: list[Path] = []
            for entry in group.get("includes", []):
                path = Path(entry.get("path", ""))
                if not path.is_absolute():
                    path = source_root / path
                path = path.resolve(strict=False)
                if path not in roots:
                    roots.append(path)
            variant = tuple(roots)
            target_variants = targets.setdefault(target_name, [])
            if variant not in target_variants:
                target_variants.append(variant)
            for source_index in group.get("sourceIndexes", []):
                if not isinstance(source_index, int) or source_index < 0 or source_index >= len(sources):
                    continue
                path = Path(sources[source_index].get("path", ""))
                if not path.is_absolute():
                    path = source_root / path
                try:
                    rel = path.resolve(strict=False).relative_to(root.resolve()).as_posix()
                except (OSError, ValueError):
                    continue
                variants = raw.setdefault(rel, [])
                if variant not in variants:
                    variants.append(variant)
    return raw, targets


def _raw_variants(value: object) -> tuple[tuple[Path, ...], ...]:
    if not value:
        return ()
    variants = value if isinstance(value, (tuple, list)) else ()
    if variants and isinstance(variants[0], Path):
        return (tuple(variants),)
    result: list[tuple[Path, ...]] = []
    for variant in variants:
        try:
            item = tuple(Path(path).resolve(strict=False) for path in variant)
        except TypeError:
            continue
        if item not in result:
            result.append(item)
    return tuple(result)


def recover_contexts(root: Path, build: Path, wanted_sources: Iterable[str],
                     configuration: str = "Debug",
                     raw_context: dict[str, object] | None = None,
                     exclude=None) -> dict[str, SourceContext]:
    """Recover contexts only for directly registered test sources lacking raw context.

    Existing source variants are returned unchanged with ``status='actual'``.
    Dormant sources borrow ordered C compile groups from their exact registered
    owners. If the owner has no current C group, only literal common Unity
    wrapper roots are accepted. Unregistered sources remain ``unknown``.
    """
    root = root.resolve(strict=False)
    if raw_context is None:
        raw, target_variants = _read_file_api(root, build, configuration)
    else:
        raw = {key: list(_raw_variants(value)) for key, value in raw_context.items()}
        _, target_variants = _read_file_api(root, build, configuration)
    owners_by_source, common_calls = _registrations(root, exclude)
    common_roots, common_wrappers = _common_wrapper_roots(root, exclude)
    result: dict[str, SourceContext] = {}
    for item in wanted_sources:
        source = item.replace("\\", "/").lstrip("./")
        if exclude is not None and exclude(source):
            result[source] = SourceContext("unknown", (), (), ("source excluded by caller",))
            continue
        actual = _raw_variants(raw.get(source))
        if actual:
            result[source] = SourceContext("actual", actual, (), ("File API sourceIndexes",))
            continue
        registrations = owners_by_source.get(source, [])
        if not registrations:
            result[source] = SourceContext("unknown", (), (), ("no direct literal test CMake registration",))
            continue
        owners = tuple(sorted({entry.target for entry in registrations}))
        variants: list[tuple[Path, ...]] = []
        evidence: list[str] = []
        incomplete_owner = False
        for owner in owners:
            owner_variants = target_variants.get(owner, [])
            owner_registrations = [entry for entry in registrations if entry.target == owner]
            registration_commands = {entry.command.lower() for entry in owner_registrations}
            if owner in target_variants:
                for owner_variant in owner_variants:
                    if owner_variant not in variants:
                        variants.append(owner_variant)
                evidence.append(f"owner {owner}: current File API C compile groups")
            elif (registration_commands & common_wrappers or owner in common_calls) and common_roots:
                variant = common_roots
                if variant not in variants:
                    variants.append(variant)
                evidence.append(f"owner {owner}: literal common test wrapper roots")
            else:
                incomplete_owner = True
                evidence.append(f"owner {owner}: no current C group or proven common wrapper roots")
        if variants and not incomplete_owner:
            result[source] = SourceContext("recovered", tuple(variants), owners, tuple(evidence))
        else:
            result[source] = SourceContext("unknown", (), owners, tuple(evidence))
    return result


def _resolve_quoted(root: Path, including: str, name: str,
                    roots: tuple[Path, ...]) -> str | None:
    parent = (root / including).parent
    relative_name = Path(name.replace("\\", "/"))
    for base in (parent, *roots):
        candidate = Path(os.path.abspath(base / relative_name))
        if not candidate.is_file():
            continue
        try:
            relative = candidate.relative_to(root.resolve()).as_posix()
        except (OSError, ValueError):
            continue
        if _safe_repo_file(root, relative) is not None:
            return relative
    return None


def _safe_repo_file(root: Path, relative: str) -> Path | None:
    relative_path = Path(relative.replace("\\", "/"))
    if relative_path.is_absolute() or any(part in {"", ".", ".."} for part in relative_path.parts):
        return None
    current = root.resolve()
    for part in relative_path.parts:
        current = current / part
        try:
            info = os.lstat(current)
        except OSError:
            return None
        if Path(current).is_symlink() or getattr(info, "st_file_attributes", 0) & 0x400:
            return None
    try:
        current.resolve(strict=True).relative_to(root.resolve())
    except (OSError, ValueError):
        return None
    return current


def propagate_dormant_headers(root: Path,
                              dormant_contexts: dict[str, tuple[tuple[Path, ...], ...]],
                              exclude=None) -> dict[str, tuple[tuple[Path, ...], ...]]:
    """Propagate within a separate dormant graph, excluding unsafe or declined paths."""
    root = root.resolve(strict=False)
    result: dict[str, list[tuple[Path, ...]]] = {
        source: list(_raw_variants(variants)) for source, variants in dormant_contexts.items()
        if (exclude is None or not exclude(source)) and _safe_repo_file(root, source) is not None
    }
    pending = [(source, variant) for source, variants in result.items() for variant in variants]
    visited: set[tuple[str, tuple[Path, ...]]] = set()
    while pending:
        including, roots = pending.pop()
        if exclude is not None and exclude(including):
            continue
        state = (including, roots)
        if state in visited:
            continue
        visited.add(state)
        try:
            source_path = _safe_repo_file(root, including)
            if source_path is None:
                continue
            data = source_path.read_bytes()
        except OSError:
            continue
        for match in _INCLUDE_RE.finditer(data):
            name = match.group("name").decode("utf-8", errors="replace")
            target = _resolve_quoted(root, including, name, roots)
            if target is None or (exclude is not None and exclude(target)):
                continue
            if _safe_repo_file(root, target) is None:
                continue
            variants = result.setdefault(target, [])
            if roots not in variants:
                variants.append(roots)
                pending.append((target, roots))
    return {source: tuple(variants) for source, variants in result.items()}
