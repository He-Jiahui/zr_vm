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

# @unit scripts/layout/dormant_contexts.py::DormantContextError [class]
# ## Summary
# 为已被本模块明确识别的 File API 读取、JSON 索引或 checkout 身份错误提供可捕获的异常类型。
# ## Constraints and lifecycle
# 继承 RuntimeError，不持有文件句柄或生成目录；异常消息与 __cause__ 仅描述失败观测。只有显式包装分支使用此类型，其他类型、路径、解码或调用方回调异常仍可能直接传播，不能视为所有畸形输入的统一错误协议。
# ## References
# - scripts/layout/dormant_contexts.py:19
# - scripts/layout/dormant_contexts.py:290
# - scripts/layout/dormant_contexts.py:304
# - scripts/layout/dormant_contexts.py:312
# - scripts/layout/apply_moves.py:2681
# - scripts/layout/apply_moves.py:2745
# - tests/tooling/test_layout_dormant_contexts.py:208

# @unit scripts/layout/dormant_contexts.py::SourceContext [class]
# ## Summary
# 携带单个请求源的实际、恢复或未知包含上下文，供调用方分离 dormant 图与当前 File API 图。
# ## Fields and invariants
# - status: 由恢复器填写 actual、recovered 或 unknown；不是执行通过标志。
# - variants: 独立 ordered-root variant 的不可变外层/内层 tuple；路径所指文件仍可变化。
# - owners: 恢复模型中的排序去重目标名；actual 分支不补推 owner。
# - evidence: 解释实际组、wrapper fallback、排除或缺失证据的字符串 tuple。
# ## Constraints and lifecycle
# 这是 frozen dataclass 的值记录，字段冻结不冻结文件系统。recover_contexts 生成 actual/recovered/unknown 三种 status；直接构造不执行枚举校验。variants 保留各组内部 roots 顺序，不能把各组求并集；owners/evidence 描述观测来源。Path 和字符串不持有目录句柄、构建租约或源码快照，读取后树变化会使证据过时。
# ## References
# - scripts/layout/dormant_contexts.py:24
# - scripts/layout/dormant_contexts.py:23
# - scripts/layout/dormant_contexts.py:386
# - scripts/layout/dormant_contexts.py:390
# - scripts/layout/dormant_contexts.py:418
# - scripts/layout/dormant_contexts.py:420
# - scripts/layout/apply_moves.py:2683
# - scripts/layout/apply_moves.py:2689
# - scripts/layout/dormant_contexts.py:394

# @unit scripts/layout/dormant_contexts.py::_Registration [class]
# ## Summary
# 保存从一个字面 CMake 命令识别出的目标归属证据，并支持同一注册记录去重。
# ## Fields and invariants
# - target: 无动态变量的字面目标名。
# - cmake_file: 相对 root 的证据文件路径。
# - command: 被 _commands 规范为小写的源注册命令名。
# ## Constraints and lifecycle
# frozen dataclass 只保存 target、仓内 CMake 相对路径和命令名，不执行 CMake，也不证明条件分支或目标实际生成。记录由 _registrations 拥有，与任何打开的文件或 native target 生命周期无关。
# ## References
# - scripts/layout/dormant_contexts.py:32
# - scripts/layout/dormant_contexts.py:31
# - scripts/layout/dormant_contexts.py:232
# - scripts/layout/dormant_contexts.py:234
# - scripts/layout/dormant_contexts.py:403

# @unit scripts/layout/dormant_contexts.py::_SOURCE_COMMANDS [state]
# ## Summary
# 限定可提供字面源归属的声明命令集合；target_sources 由独立分支一同处理。
# ## Constraints and lifecycle
# 集合在模块装载时创建，正常调用只读。它只决定识别哪些声明拼写，不解释 target 的实际启用状态、函数执行、条件或生成器表达式。调用期间不得由外部并发修改此集合。
# ## References
# - scripts/layout/dormant_contexts.py:38
# - scripts/layout/dormant_contexts.py:221
# - scripts/layout/dormant_contexts.py:229
# - tests/tooling/test_layout_dormant_contexts.py:114

# @unit scripts/layout/dormant_contexts.py::_SCOPE_KEYWORDS [state]
# ## Summary
# 在源与包含目录参数中跳过 CMake 可见性及排序关键字，避免把它们识别为目标或路径。
# ## Constraints and lifecycle
# 按 token.upper() 匹配；只覆盖列出的字面词，不提供完整 CMake 参数语法。模块共享集合在一次扫描期间须稳定。
# ## References
# - scripts/layout/dormant_contexts.py:44
# - scripts/layout/dormant_contexts.py:223
# - scripts/layout/dormant_contexts.py:227
# - scripts/layout/dormant_contexts.py:260

# @unit scripts/layout/dormant_contexts.py::_COMMAND_START_RE [state]
# ## Summary
# 定位字面 CMake 命令的开括号，供有限平衡括号扫描器继续提取命令体。
# ## Constraints and lifecycle
# 匹配不区分命令大小写，限制标识符边界；不是 CMake 解析器，字符串、括号与注释的其余处理由 _commands/_strip_comments 协作。共享编译正则正常只读。
# ## References
# - scripts/layout/dormant_contexts.py:46
# - scripts/layout/dormant_contexts.py:86
# - scripts/layout/dormant_contexts.py:88

# @unit scripts/layout/dormant_contexts.py::_TOKEN_RE [state]
# ## Summary
# 在有限声明模型中分出双引号 token 或由空白/分号分隔的裸 token。
# ## Constraints and lifecycle
# 不能执行变量展开、生成器表达式或完整 CMake escape 规则；_tokens 仅额外还原反斜杠双引号。该正则不表示任意 CMake 文件已被解释。
# ## References
# - scripts/layout/dormant_contexts.py:47
# - scripts/layout/dormant_contexts.py:114
# - scripts/layout/dormant_contexts.py:116

# @unit scripts/layout/dormant_contexts.py::_INCLUDE_RE [state]
# ## Summary
# 从源码字节中提取双引号 include 的字面名称，作为 dormant header 传播的候选边。
# ## Constraints and lifecycle
# 字节正则不执行预处理、不剥离 C 注释，也不处理宏计算的 include 名；匹配结果仍须通过仓内目标与排除检查。名称 UTF-8 解码使用替换策略，不是源码编码认证。
# ## References
# - scripts/layout/dormant_contexts.py:48
# - scripts/layout/dormant_contexts.py:487
# - scripts/layout/dormant_contexts.py:488
# - scripts/layout/dormant_contexts.py:489
# - scripts/layout/dormant_contexts.py:492

# @unit scripts/layout/dormant_contexts.py::_strip_comments [function]
# ## Summary
# 在有限 CMake 扫描前移除普通 # 行注释，同时保留双引号中的 # 与原换行。
# ## Parameters
# @param[in] text 待扫描的 CMake 文本；调用期间保持其字符串值可用。
# ## Returns
# @return 返回新字符串；输入字符串不变。
# ## Constraints and lifecycle
# 局部 quoted/escaped/comment 状态只覆盖普通双引号和行注释规则，不支持完整 bracket argument/comment 语法。无文件句柄或外部写入；内存分配等未显式捕获异常可传播。
# ## References
# - scripts/layout/dormant_contexts.py:51
# - scripts/layout/dormant_contexts.py:59
# - scripts/layout/dormant_contexts.py:67
# - scripts/layout/dormant_contexts.py:75
# - scripts/layout/dormant_contexts.py:84
# - scripts/layout/dormant_contexts.py:247

# @unit scripts/layout/dormant_contexts.py::_commands [function]
# ## Summary
# 惰性地产生带平衡括号的字面命令名和命令体，供注册及 wrapper 识别复用。
# ## Parameters
# @param[in] text 由调用方持有的 CMake 文本。
# ## Returns
# @return 返回迭代器；逐次产生 (小写命令名, 命令体字符串)，无法闭合的尾部停止产生后续项。
# ## Constraints and lifecycle
# 函数为生成器，首次推进才开始过滤与扫描；暂停时只保留局部字符串和索引，没有打开的资源。双引号内括号与转义另行处理；不执行条件、函数、变量或动态嵌套语义，已产生的前缀不会因尾部不完整而回滚。调用方自行消费、关闭或释放迭代器。
# ## References
# - scripts/layout/dormant_contexts.py:82
# - scripts/layout/dormant_contexts.py:84
# - scripts/layout/dormant_contexts.py:93
# - scripts/layout/dormant_contexts.py:106
# - scripts/layout/dormant_contexts.py:108
# - scripts/layout/dormant_contexts.py:217
# - scripts/layout/dormant_contexts.py:255
# - scripts/layout/dormant_contexts.py:272

# @unit scripts/layout/dormant_contexts.py::_tokens [function]
# ## Summary
# 把有限命令体转换为供字面路径与目标识别使用的有序 token 列表。
# ## Parameters
# @param[in] body _commands 产生或同语法前提的命令体字符串。
# ## Returns
# @return 返回新建 token 列表，保持匹配顺序；没有匹配时为空列表。
# ## Constraints and lifecycle
# 仅使用 _TOKEN_RE，并把反斜杠双引号转换成双引号，不做变量展开或完整 CMake 解码。输入不变，列表由调用方管理；无外部资源。
# ## References
# - scripts/layout/dormant_contexts.py:112
# - scripts/layout/dormant_contexts.py:114
# - scripts/layout/dormant_contexts.py:116
# - scripts/layout/dormant_contexts.py:218
# - scripts/layout/dormant_contexts.py:258

# @unit scripts/layout/dormant_contexts.py::_cmake_source_scope [function]
# ## Summary
# 为测试 cmake 片段选择其声明中的当前源码目录，使 tests/cmake/*.cmake 相对源按 tests 根解释。
# ## Parameters
# @param[in] root 实际 checkout 根路径，cmake_file 的解析路径须位于其中。
# @param[in] cmake_file 当前读取的仓内 CMake 文件路径。
# ## Returns
# @return 返回 tests 根或该 CMake 文件的父目录 Path。
# ## Constraints and lifecycle
# tests/cmake 下非 CMakeLists.txt 片段使用特例；其余按文件父目录。函数不打开或创建目录；resolve/relative_to 的未捕获错误可传播，返回 Path 不锁定目录或保证存在。
# ## References
# - scripts/layout/dormant_contexts.py:120
# - scripts/layout/dormant_contexts.py:121
# - scripts/layout/dormant_contexts.py:122
# - scripts/layout/dormant_contexts.py:135
# - scripts/layout/dormant_contexts.py:140
# - scripts/layout/dormant_contexts.py:157

# @unit scripts/layout/dormant_contexts.py::_source_path [function]
# ## Summary
# 把有限支持的 CMake 源 token 归一为 tests/ 下的仓内相对路径，拒绝无法静态归属的写法。
# ## Parameters
# @param[in] root 扫描 checkout 根。
# @param[in] cmake_file 提供相对源目录语境的仓内声明文件。
# @param[in] token 字面源参数，允许支持的源码目录变量前缀、裸相对路径或仓内绝对路径。
# ## Returns
# @return 成功返回 POSIX 拼写的 tests/ 相对路径；不支持、越界或所捕获的路径错误返回 None。
# ## Constraints and lifecycle
# 不要求目标文件存在或有特定后缀，不证明构建实际采用该源。支持边界由分支明示，动态变量不求值；调用方保持目录树稳定。无文件写入，未捕获的异常仍可传播。
# ## References
# - scripts/layout/dormant_contexts.py:127
# - scripts/layout/dormant_contexts.py:130
# - scripts/layout/dormant_contexts.py:134
# - scripts/layout/dormant_contexts.py:136
# - scripts/layout/dormant_contexts.py:142
# - scripts/layout/dormant_contexts.py:145
# - scripts/layout/dormant_contexts.py:229
# - tests/tooling/test_layout_dormant_contexts.py:84
# - tests/tooling/test_layout_dormant_contexts.py:85

# @unit scripts/layout/dormant_contexts.py::_literal_root [function]
# ## Summary
# 只从支持的源码目录变量前缀恢复仓内 include root，供 common-helper fallback 使用。
# ## Parameters
# @param[in] root checkout 根。
# @param[in] cmake_file 解释 CMAKE_CURRENT_SOURCE_DIR 的声明文件。
# @param[in] token 待判定的字面包含目录参数。
# ## Returns
# @return 返回解析后仍在 root 内的 Path；不支持的 token、越界或所捕获的错误返回 None。
# ## Constraints and lifecycle
# 不接受任意裸路径，不展开其他变量，也不验证返回目标是现存目录。路径只是观测值，不带目录所有权、租约或并发保护。
# ## References
# - scripts/layout/dormant_contexts.py:148
# - scripts/layout/dormant_contexts.py:152
# - scripts/layout/dormant_contexts.py:156
# - scripts/layout/dormant_contexts.py:159
# - scripts/layout/dormant_contexts.py:161
# - scripts/layout/dormant_contexts.py:262

# @unit scripts/layout/dormant_contexts.py::_is_reparse_or_symlink [function]
# ## Summary
# 以符号链接模式和 Windows reparse 属性拒绝不应遍历的路径，无法读取元数据时采用拒绝结果。
# ## Parameters
# @param[in] path 需检查的路径；未提供 stat_result 时使用 lstat。
# @param[in] stat_result 可选的该路径不跟随链接的 stat 结果，由调用方保证对应关系与时效。
# ## Returns
# @return 检测到 symlink/reparse 或 lstat 抛 OSError 时返回 True，否则返回 False。
# ## Constraints and lifecycle
# 不取得持续有效的目录句柄，不防检查后替换。传入缓存 stat 可避免重复查询，但不能据此证明后续文件操作安全；无所有权转移。
# ## References
# - scripts/layout/dormant_contexts.py:167
# - scripts/layout/dormant_contexts.py:169
# - scripts/layout/dormant_contexts.py:170
# - scripts/layout/dormant_contexts.py:172
# - scripts/layout/dormant_contexts.py:177
# - scripts/layout/dormant_contexts.py:197

# @unit scripts/layout/dormant_contexts.py::_cmake_files [function]
# ## Summary
# 枚举 tests 树中可读取、未排除且不经 symlink/reparse 的 CMake 声明文件。
# ## Parameters
# @param[in] root checkout 根；扫描仅从其 tests 子目录开始。
# @param[in] exclude 可选相对路径过滤回调，True 表示跳过；异常与闭包状态由调用方负责。
# ## Returns
# @return 返回排序后的 Path 列表；tests 不可用、被拒绝或部分目录读取失败时可为空或不完整。
# ## Constraints and lifecycle
# 目录迭代器在 with 内消费并关闭，之后递归处理条目；不写目录。过滤可在根和各层触发。错误跳过不是全量扫描成功证明；文件树须由调用方稳定，检查不抵御并发替换。visit 的闭包只借用本次 root/exclude/result。
# ## References
# - scripts/layout/dormant_contexts.py:175
# - scripts/layout/dormant_contexts.py:177
# - scripts/layout/dormant_contexts.py:182
# - scripts/layout/dormant_contexts.py:184
# - scripts/layout/dormant_contexts.py:186
# - scripts/layout/dormant_contexts.py:191
# - scripts/layout/dormant_contexts.py:197
# - scripts/layout/dormant_contexts.py:205
# - scripts/layout/dormant_contexts.py:212

# @unit scripts/layout/dormant_contexts.py::_cmake_files::visit [function]
# ## Summary
# 在一次 CMake 枚举中递归收集合格文件，遇不可读目录或条目时继续保留可观测的其他结果。
# ## Parameters
# @param[in] directory 已通过上层筛选的当前目录 Path。
# ## Returns
# 没有 Python 返回值；将找到的路径追加到本次外层 result。
# ## Constraints and lifecycle
# 闭包借用 root、exclude，并追加本次 result；调用完成不保存独立资源。scandir 由 with 关闭，entry.stat 不跟随链接；递归前再次筛选 reparse，不能提供跨操作原子快照。回调异常不被吞掉。
# ## References
# - scripts/layout/dormant_contexts.py:182
# - scripts/layout/dormant_contexts.py:184
# - scripts/layout/dormant_contexts.py:185
# - scripts/layout/dormant_contexts.py:186
# - scripts/layout/dormant_contexts.py:194
# - scripts/layout/dormant_contexts.py:197
# - scripts/layout/dormant_contexts.py:200
# - scripts/layout/dormant_contexts.py:203
# - scripts/layout/dormant_contexts.py:205

# @unit scripts/layout/dormant_contexts.py::_registrations [function]
# ## Summary
# 从测试 CMake 文本收集源的字面目标注册，以及直接 common-test helper 调用的目标名。
# ## Parameters
# @param[in] root 扫描 checkout 根。
# @param[in] exclude 传递给 CMake 枚举的可选路径排除回调。
# ## Returns
# @return 返回 (源相对路径到去重注册列表的字典, common-helper 字面目标集合)。
# ## Constraints and lifecycle
# 只认列出的源声明及 target_sources；跳过动态目标与不支持的源路径。逐文件 OSError/UnicodeDecodeError 会跳过该文件，不能保证覆盖不可读声明；其余未捕获错误传播。容器为本次新建，不修改调用方图，不解释 if/function 的执行条件。
# ## References
# - scripts/layout/dormant_contexts.py:209
# - scripts/layout/dormant_contexts.py:212
# - scripts/layout/dormant_contexts.py:214
# - scripts/layout/dormant_contexts.py:215
# - scripts/layout/dormant_contexts.py:221
# - scripts/layout/dormant_contexts.py:229
# - scripts/layout/dormant_contexts.py:232
# - scripts/layout/dormant_contexts.py:236
# - scripts/layout/dormant_contexts.py:380
# - tests/tooling/test_layout_dormant_contexts.py:114

# @unit scripts/layout/dormant_contexts.py::_common_wrapper_roots [function]
# ## Summary
# 在 tests/CMakeLists.txt 中静态识别 common-test helper 的字面 roots，并找出调用该 helper 的 wrapper 名。
# ## Parameters
# @param[in] root checkout 根。
# @param[in] exclude 可选过滤回调，允许排除 tests/CMakeLists.txt。
# ## Returns
# @return 返回 (按出现顺序去重的仓内 roots tuple, wrapper 小写名称集合)；缺证据或所捕获的读取失败返回空值。
# ## Constraints and lifecycle
# 正则只识别有限 function/endfunction 形状，命令与路径仍使用本模块有限解析规则；不执行函数或条件，不推断任意 wrapper。只读声明，不打开返回目录；元数据和文件内容需调用期间稳定。
# ## References
# - scripts/layout/dormant_contexts.py:242
# - scripts/layout/dormant_contexts.py:244
# - scripts/layout/dormant_contexts.py:247
# - scripts/layout/dormant_contexts.py:250
# - scripts/layout/dormant_contexts.py:255
# - scripts/layout/dormant_contexts.py:262
# - scripts/layout/dormant_contexts.py:266
# - scripts/layout/dormant_contexts.py:271
# - scripts/layout/dormant_contexts.py:381
# - tests/tooling/test_layout_dormant_contexts.py:172
# - tests/tooling/test_layout_dormant_contexts.py:183

# @unit scripts/layout/dormant_contexts.py::_read_file_api [function]
# ## Summary
# 读取选定构建树最新 File API 索引，提取该 checkout 的逐源 C include variants 及按目标分组的 C variants。
# ## Parameters
# @param[in] root 期望的 checkout 根，用于 codemodel source-root 身份比较。
# @param[in] build 调用方提供且保持稳定的构建树，只读取 .cmake/api/v1/reply 下关联 JSON。
# @param[in] configuration 所需配置名；仅采用唯一同名配置或唯一未命名配置。
# ## Returns
# @return 返回 (逐源 variants 字典, 逐目标 variants 字典)；无索引或无唯一可用配置时返回两个空字典。
# ## Constraints and lifecycle
# 索引按 mtime_ns 选最新；JSON 中的相对 include/source 路径按 source root 解释，仅 C compile group 被保留，不合格 sourceIndexes 被跳过。特定 OSError/KeyError/StopIteration/JSONDecodeError 及 source-root 不匹配被包装为 DormantContextError；未声明的类型/解码/路径错误可传播。不是任意 JSON shape/path 的安全认证，不创建 query/reply 或执行 CMake。每个 roots 顺序独立，调用方负责文件、构建树和线程稳定性。
# ## References
# - scripts/layout/dormant_contexts.py:277
# - scripts/layout/dormant_contexts.py:280
# - scripts/layout/dormant_contexts.py:282
# - scripts/layout/dormant_contexts.py:289
# - scripts/layout/dormant_contexts.py:291
# - scripts/layout/dormant_contexts.py:303
# - scripts/layout/dormant_contexts.py:311
# - scripts/layout/dormant_contexts.py:316
# - scripts/layout/dormant_contexts.py:330
# - scripts/layout/dormant_contexts.py:343
# - scripts/layout/dormant_contexts.py:376
# - scripts/layout/dormant_contexts.py:379
# - tests/tooling/test_layout_dormant_contexts.py:207
# - tests/tooling/test_layout_dormant_contexts.py:209

# @unit scripts/layout/dormant_contexts.py::_raw_variants [function]
# ## Summary
# 把调用链支持的 roots/variants 容器转换为去重的 tuple-of-tuples，保留每组和各组的顺序。
# ## Parameters
# @param[in] value 空值、Path 的单组 tuple/list，或由可转换为 Path 的组构成的 tuple/list。
# ## Returns
# @return 返回标准化 variants；空/不支持外层或转换抛 TypeError 的组被略过。
# ## Constraints and lifecycle
# Path 单组分支直接组成 tuple，其余组逐路径 resolve；不承诺任意嵌套对象均可接受。只捕获每组的 TypeError，其他错误可传播；不修改输入容器，不获取目录所有权或保证路径存在。
# ## References
# - scripts/layout/dormant_contexts.py:346
# - scripts/layout/dormant_contexts.py:349
# - scripts/layout/dormant_contexts.py:350
# - scripts/layout/dormant_contexts.py:355
# - scripts/layout/dormant_contexts.py:356
# - scripts/layout/dormant_contexts.py:378
# - scripts/layout/dormant_contexts.py:388
# - scripts/layout/dormant_contexts.py:467

# @unit scripts/layout/dormant_contexts.py::recover_contexts [function]
# ## Summary
# 为缺少实际源上下文的字面注册测试源恢复有序 roots，并用明确状态区分实际、恢复和未知证据。
# ## Parameters
# @param[in] root checkout 根；调用方稳定本次 CMake 声明与文件系统。
# @param[in] build 待只读查询的 File API 构建树。
# @param[in] wanted_sources 可迭代的规范仓内源相对路径字符串；不把其拼写清理视为越界认证。
# @param[in] configuration 选择 File API 配置的名称，默认 Debug。
# @param[in] raw_context 可选已有实际源 variants 映射；提供时仍读取 File API 的目标组，不修改该映射。
# @param[in] exclude 可选相对路径过滤回调；其异常与状态由调用方管理。
# ## Returns
# @return 返回新字典，逐请求源给出 SourceContext；路径被排除、未注册或任一 owner 缺乏完整证据时为 unknown，而非推测 roots。
# ## Constraints and lifecycle
# 实际 variants 优先返回，不追加恢复 roots；规范化不等于原容器对象身份保留。缺少实际源时逐 owner 采用其当前 C compile groups；仅在 wrapper 调用被证明且 common roots 非空时回退。保持 variant 内顺序，不合并为单个 roots 集；任一 owner 不完整使整个源 unknown。函数无文件写入；读取、标准化或回调异常可中断并使局部结果不发布，调用方图和输入不因本函数改写。返回 Path 只是时间点观测，不持有构建租约或同步锁。
# ## References
# - scripts/layout/dormant_contexts.py:363
# - scripts/layout/dormant_contexts.py:374
# - scripts/layout/dormant_contexts.py:376
# - scripts/layout/dormant_contexts.py:379
# - scripts/layout/dormant_contexts.py:385
# - scripts/layout/dormant_contexts.py:388
# - scripts/layout/dormant_contexts.py:396
# - scripts/layout/dormant_contexts.py:404
# - scripts/layout/dormant_contexts.py:409
# - scripts/layout/dormant_contexts.py:417
# - scripts/layout/apply_moves.py:2677
# - scripts/layout/apply_moves.py:2683
# - tests/tooling/test_layout_dormant_contexts.py:101
# - tests/tooling/test_layout_dormant_contexts.py:136
# - tests/tooling/test_layout_dormant_contexts.py:197
# - tests/tooling/test_layout_dormant_contexts.py:124
# - tests/tooling/test_layout_dormant_contexts.py:150
# - tests/tooling/test_layout_dormant_contexts.py:172
# - tests/tooling/test_layout_dormant_contexts.py:183
# - tests/tooling/test_layout_dormant_contexts.py:209
# - tests/tooling/test_layout_dormant_contexts.py:228

# @unit scripts/layout/dormant_contexts.py::_resolve_quoted [function]
# ## Summary
# 在包含源父目录及本 variant 的 ordered roots 中寻找首个现存且通过仓内路径筛选的双引号目标。
# ## Parameters
# @param[in] root 仓内判定的 checkout 根。
# @param[in] including 当前包含文件的仓内相对路径。
# @param[in] name 从有限 include 匹配器得到的名称，反斜杠会归一。
# @param[in] roots 本 variant 的有序搜索根 tuple；先搜索包含源父目录。
# ## Returns
# @return 找到时返回仓内 POSIX 相对路径；候选不合格或全部未命中时返回 None。
# ## Constraints and lifecycle
# 元数据上使用 is_file，且目标须经 _safe_repo_file 筛选；不读取候选内容，不把外部根变成可发布的仓外结果。只捕获显式 relative_to 错误，其他异常可能传播；多次路径检查不防并发替换。
# ## References
# - scripts/layout/dormant_contexts.py:424
# - scripts/layout/dormant_contexts.py:428
# - scripts/layout/dormant_contexts.py:429
# - scripts/layout/dormant_contexts.py:430
# - scripts/layout/dormant_contexts.py:433
# - scripts/layout/dormant_contexts.py:436
# - scripts/layout/dormant_contexts.py:489

# @unit scripts/layout/dormant_contexts.py::_safe_repo_file [function]
# ## Summary
# 检查规范相对路径的每个现存组件及最终解析范围，供 dormant 图过滤不安全目标。
# ## Parameters
# @param[in] root 实际 checkout 根。
# @param[in] relative 待判定的仓内相对路径字符串。
# ## Returns
# @return 通过检查时返回 Path，否则在显式拒绝/捕获分支返回 None。
# ## Constraints and lifecycle
# 拒绝绝对或被检查到的非法组件、symlink/reparse、缺失及解析越界；不创建或打开文件。函数本身不证明最终对象是 regular file，不认证内容或分配来源；路径检查与后续读取之间仍由调用方保证稳定。未捕获错误可传播。
# ## References
# - scripts/layout/dormant_contexts.py:441
# - scripts/layout/dormant_contexts.py:443
# - scripts/layout/dormant_contexts.py:449
# - scripts/layout/dormant_contexts.py:452
# - scripts/layout/dormant_contexts.py:455
# - scripts/layout/dormant_contexts.py:436
# - scripts/layout/dormant_contexts.py:468
# - scripts/layout/dormant_contexts.py:481
# - scripts/layout/dormant_contexts.py:492

# @unit scripts/layout/dormant_contexts.py::propagate_dormant_headers [function]
# ## Summary
# 在独立 dormant 字典中沿字面双引号 include 传播各个 ordered-root variant，不向实际 File API 图求并或写回。
# ## Parameters
# @param[in] root checkout 根，调用期间保持被查树稳定。
# @param[in] dormant_contexts 源相对路径到 variants 的映射；不修改输入字典或其容器。
# @param[in] exclude 可选相对路径过滤回调，适用于 seed、当前源及解析目标；异常由调用方处理。
# ## Returns
# @return 返回新字典，保留通过初始筛选的 seeds，并加入解析到的仓内目标及各自去重 variants。
# ## Constraints and lifecycle
# visited 按 (相对路径, roots tuple) 去重，允许有限包含环终止。正则不执行预处理，也不区分注释内样本；每条候选仍须通过排除和仓内检查。读取源的 OSError 会跳过该节点扩展，并不回滚已记录 seeds/边；其他未捕获异常可传播。无外部写入或打开句柄保留；Path 不延长源、构建或线程安全有效期。
# ## References
# - scripts/layout/dormant_contexts.py:461
# - scripts/layout/dormant_contexts.py:466
# - scripts/layout/dormant_contexts.py:470
# - scripts/layout/dormant_contexts.py:476
# - scripts/layout/dormant_contexts.py:480
# - scripts/layout/dormant_contexts.py:485
# - scripts/layout/dormant_contexts.py:487
# - scripts/layout/dormant_contexts.py:490
# - scripts/layout/dormant_contexts.py:494
# - scripts/layout/apply_moves.py:2686
# - scripts/layout/apply_moves.py:2687
# - tests/tooling/test_layout_dormant_contexts.py:238
# - tests/tooling/test_layout_dormant_contexts.py:241
