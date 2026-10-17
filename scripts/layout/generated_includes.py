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

# @unit GENERATED_HEADER
# ## Summary
# 限定外部构建目录中唯一允许分类为生成头的文件名。
# ## Constraints and lifecycle
# 这是精确字符串白名单，不是扩展名或目录前缀规则。反斜杠规范化后的请求名仍须与它完全一致；带./或折叠后等价的父路径别名不自动获得生成头资格。仓内候选由仓内分支优先处理；常量不提供文件内容或编译有效性证明。
# ## References
# - scripts/layout/generated_includes.py:15
# - scripts/layout/generated_includes.py:85
# - scripts/layout/generated_includes.py:92
# - scripts/layout/generated_includes.py:112
# - tests/tooling/test_layout_generated_includes.py:100
# - tests/tooling/test_layout_generated_includes.py:105

# @unit _FORBIDDEN_ROOTS
# ## Summary
# 列出仓内路径任一组件命中时禁止作为安全改写目标的名称。
# ## Constraints and lifecycle
# 查询使用每个组件的casefold结果作精确成员检查，不是子串搜索，也不只检查顶层目录。该元组本身不遍历文件系统；命中且存在的候选可能遮蔽后续根，须交回unsafe。它不替代reparse检查。
# ## References
# - scripts/layout/generated_includes.py:16
# - scripts/layout/generated_includes.py:19
# - scripts/layout/generated_includes.py:65
# - scripts/layout/generated_includes.py:103
# - tests/tooling/test_layout_generated_includes.py:134
# - tests/tooling/test_layout_generated_includes.py:146

# @unit IncludeResolution
# ## Summary
# 保存一个编译变体的分类结果，供调用方比较解析身份。
# ## Fields and invariants
# kind为分类标签；path为可选Path；relative_path仅供仓内目标使用；variant_index为输入变体的零基下标。不同名称动态加载同一源码会产生不同的Python类型对象。
# ## Constraints and lifecycle
# frozen且slots限制普通属性重赋值，但Literal注解不会运行期校验手动构造值。repository结果携带解析后的Path和仓内POSIX相对路径；generated没有仓内改写目标；unresolved通常无Path；unsafe的Path可为冲突候选或None。variant_index保存零基变体位置，不用于去重。结果只保存Python值引用，不持有文件句柄或锁，也不延长路径指向对象的存在期。
# ## References
# - scripts/layout/generated_includes.py:23
# - scripts/layout/generated_includes.py:22
# - scripts/layout/generated_includes.py:29
# - scripts/layout/generated_includes.py:98
# - scripts/layout/generated_includes.py:109
# - scripts/layout/generated_includes.py:117
# - scripts/layout/generated_includes.py:127
# - scripts/layout/generated_includes.py:150
# - scripts/layout/generated_includes.py:183

# @unit _absolute
# ## Summary
# 以指定基准生成词法绝对Path，保留后续单独检查链接的机会。
# ## Parameters
# @param[in] path 待转换的Path；可为相对路径。
# @param[in] base 相对path使用的基准Path；需要确定结果时应已为绝对路径。
# ## Returns
# @return 返回新构造的绝对Path；返回值不代表该文件存在或可读取。
# ## Constraints and lifecycle
# 不调用resolve，不认证路径存在、目录类型、链接安全或包含关系。相对path相对于base拼接；调用方应提供预期的基准并在操作期间保持工作目录稳定。只构造Path值，不改写入参或文件；底层路径转换异常传播。
# ## References
# - scripts/layout/generated_includes.py:32
# - scripts/layout/generated_includes.py:33
# - scripts/layout/generated_includes.py:87
# - scripts/layout/generated_includes.py:147
# - scripts/layout/generated_includes.py:182

# @unit _under
# ## Summary
# 检查一个Path是否能按原生路径语义表示为另一个Path的相对后缀。
# ## Parameters
# @param[in] path 待检查的Path。
# @param[in] parent 希望作为词法祖先的Path。
# ## Returns
# @return relative_to成功时返回True，否则对ValueError返回False；不修改任何路径。
# ## Constraints and lifecycle
# 这是词法包含关系，不访问磁盘，不解析符号链接或认证文件身份。调用方应先统一路径基准；大小写规则服从当前Path平台。正常不匹配由ValueError转为False，其余调用错误不在本函数内统一捕获。
# ## References
# - scripts/layout/generated_includes.py:36
# - scripts/layout/generated_includes.py:41
# - scripts/layout/generated_includes.py:91
# - scripts/layout/generated_includes.py:93

# @unit _has_reparse_component
# ## Summary
# 在不主动跟随链接的情况下逐组件检查路径中的链接或Windows reparse标记。
# ## Parameters
# @param[in] path 待逐组件检查的Path；不要求末端存在。
# ## Returns
# @return 发现链接、reparse标记或非FileNotFoundError的OSError返回True；否则False。未捕获的参数或路径转换错误可传播。
# ## Constraints and lifecycle
# 从词法绝对路径的anchor之后逐个lstat；anchor本身不单独lstat。不存在的组件继续检查，其他OSError按不安全返回True。检查兼顾符号链接模式位与Windows文件属性，但不读取文件内容、不持有检查结果对应的句柄或锁。结果只是当时元数据观察，不能抵御检查后的并发替换。
# ## References
# - scripts/layout/generated_includes.py:44
# - scripts/layout/generated_includes.py:62
# - scripts/layout/generated_includes.py:95
# - scripts/layout/generated_includes.py:149
# - scripts/layout/generated_includes.py:172
# - tests/tooling/test_layout_generated_includes.py:148
# - tests/tooling/test_layout_generated_includes.py:157

# @unit _excluded
# ## Summary
# 按禁止组件集合判断一个仓内相对路径是否属于禁止区域。
# ## Parameters
# @param[in] relative 已按调用方仓根解释的相对Path。
# ## Returns
# @return 任意组件命中时True，否则False。
# ## Constraints and lifecycle
# 调用方负责先取得相对于仓根的路径；这里不验证根、存在性或reparse。任意组件casefold后精确命中_FORBIDDEN_ROOTS即可拒绝，深层目录同样适用；不修改元组或入参。
# ## References
# - scripts/layout/generated_includes.py:65
# - scripts/layout/generated_includes.py:66
# - scripts/layout/generated_includes.py:102
# - scripts/layout/generated_includes.py:103
# - tests/tooling/test_layout_generated_includes.py:134
# - tests/tooling/test_layout_generated_includes.py:146

# @unit _regular_file
# ## Summary
# 用末端lstat模式位判断候选是否为普通文件。
# ## Parameters
# @param[in] path 待检查的候选Path。
# ## Returns
# @return lstat成功且模式为普通文件时True；其余模式或OSError时False。
# ## Constraints and lifecycle
# 不读取内容，OSError一律折为False；不会自行验证所有祖先均非链接。因此它只承担文件类型检查，完整路径安全依赖调用方先做reparse和范围检查。检查与随后resolve之间没有锁。
# ## References
# - scripts/layout/generated_includes.py:69
# - scripts/layout/generated_includes.py:73
# - scripts/layout/generated_includes.py:107
# - scripts/layout/generated_includes.py:113

# @unit _first_existing
# ## Summary
# 在一个有序编译变体内选出可接受的候选，保留会遮蔽后续根的不安全结果。
# ## Parameters
# @param[in] root 已解析且稳定的仓根Path。
# @param[in] build 已解析且稳定的构建根Path。
# @param[in] source_dir 由including词法绝对路径的parent取得的搜索起点；不由此参数声明证明其位于仓内。
# @param[in] name 已规范化分隔符的请求名；生成头资格仍要求精确字符串。
# @param[in] roots 本变体按搜索顺序提供的Path元组。
# @param[in] variant_index 写入结果的零基变体位置。
# ## Returns
# @return 返回此变体的IncludeResolution；无可接受或遮蔽候选时为unresolved。失败不会改写文件或入参，未捕获的路径异常传播。
# ## Constraints and lifecycle
# 由classify_include在基准已检查后调用：先搜索source_dir，再按roots顺序搜索，不去重。仓内普通文件优先，仓内禁止区域或链接候选存在时返回unsafe；普通仓内非文件候选可继续搜索。仓外仅当前build的精确生成头例外，其余存在的外部对象返回unsafe。读取的只是路径元数据，不读取生成内容、不生成改写目标。并发改动可能使后续严格resolve失败，异常不会被包装成全局事务。
# ## References
# - scripts/layout/generated_includes.py:76
# - scripts/layout/generated_includes.py:127
# - scripts/layout/generated_includes.py:183
# - tests/tooling/test_layout_generated_includes.py:68
# - tests/tooling/test_layout_generated_includes.py:88
# - tests/tooling/test_layout_generated_includes.py:100
# - tests/tooling/test_layout_generated_includes.py:132

# @unit classify_include
# ## Summary
# 分别分类每个有序编译变体中的引号include，保留结果顺序和基数。
# ## Parameters
# @param[in] root 仓根Path，可相对当前工作目录。
# @param[in] build 当前构建根Path，可相对当前工作目录。
# @param[in] including 按仓根解释的源路径字符串；该值不会被改写。
# @param[in] name 引号include的原始字符串，反斜杠会先转为正斜杠。
# @param[in] variants 每个变体的有序include根元组；不去重、不折叠。
# ## Returns
# @return 返回与variants一一对应的IncludeResolution元组；空变体集合返回空元组。已处理的不安全输入用unsafe标签表达，未捕获异常传播；不产生文件副作用。
# ## Constraints and lifecycle
# root/build须为Path，否则抛TypeError；variants应为有限Path元组的元组，name应为str。检查根的reparse、存在性和目录类型，再拒绝绝对/带Windows drive/NUL的请求名；including非str或词法逃出仓根时逐变体返回unsafe。检查的是including路径本身，未认证它是实际文件；搜索起点取其parent，不能由该检查单独推得parent在仓内。父相对include可在仓内解析。各变体独立处理，混合结果不会由这里合并或拒绝；apply_moves另比较身份一致性。调用期间应保持工作目录与路径树稳定；没有快照、文件锁或内容读取，未捕获的参数/路径异常可传播。
# ## References
# - scripts/layout/generated_includes.py:130
# - scripts/layout/generated_includes.py:184
# - scripts/layout/apply_moves.py:919
# - scripts/layout/apply_moves.py:927
# - tests/tooling/test_layout_generated_includes.py:48
# - tests/tooling/test_layout_generated_includes.py:93
# - tests/tooling/test_layout_generated_includes.py:145
