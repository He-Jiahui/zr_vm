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
    """供 CLI 和回归测试共享的诊断结果；计数用于定位扫描范围，错误决定验收。"""

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
    """为 manifest 与本地链接共用目录边界，resolve 后也排除符号链接越界。"""

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
    """在严格构建前检查源文件契约；无渲染器依赖，只核 front matter 必需键。"""

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
    # Zensical drops non-ASCII heading text, so "Complex 和 Quaternion" is complex-quaternion.
    title = re.sub(r"[^\w\- ]", "", title, flags=re.ASCII)
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
    """将导航清单的页面路径归一，以供后续和实际 Markdown 文件集合互证。"""

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
    """验证导航引用与源文件双向一致；发布入口依赖此检查拦截漏列页面。"""

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
    """链接与标题检查共用正文视图，避免把围栏代码示例当成导航目标。"""

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
    """解析当前 wiki 使用的行内本地链接，并以同一份标题缓存核对锚点。

    TODO: 此预检不解析 Markdown 的引用式链接；引入该写法前须扩展扫描并补回归样例。
    """

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
    """供 CI、作者本地命令及测试调用的统一入口，汇总所有可独立发现的源错误。

    site_dir 只在渲染完成后验证入口文件存在且非空，不代替渲染器的严格构建。
    """

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
    """将统一校验结果映射成 CI 退出码；错误逐项输出以便定位源文件。"""

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

# @class ValidationResult
#
## Summary
# 让调用者以不可变记录共享聚合诊断和扫描计数。
# 由 validate 在目录缺失或扫描结束时构造，供 CLI 和测试读取。
#
## Constraints and lifecycle
# errors 是诊断字符串元组；其非空性决定 main 的正常返回码。markdown_files 是枚举到的 Markdown 路径数；manifest_pages 是 pages 原列表长度；local_links 是识别到且未跳过的本地链接次数，包含失败目标；site_files 是可选站点检查统计的文件数。计数不代表内容、渲染或链接已全部验证。validate 构造时把内部 errors 列表冻结为元组；记录不持有打开文件或检查器缓存。NamedTuple 的类型注解不在运行时强制校验任意外部构造值。
#
## References
# - scripts/validate_wiki.py:315
# - scripts/validate_wiki.py:349
# - scripts/validate_wiki.py:373
# - tests/scripts/test_validate_wiki.py:61
# - tests/scripts/test_validate_wiki.py:64

# @fn _relative_path(path: Path, base: Path) -> str
#
## Summary
# 为诊断生成相对基准目录的 POSIX 风格路径字符串。
# 读取或检查失败后定位源文件；不得把返回字符串用于授权判断。
#
## Parameters
# @param[in] path 待显示的路径；函数不修改该对象。
# @param[in] base 用于相对化的基准目录。
#
## Returns
# @return 两条路径 resolve 后若存在相对关系，返回相对路径；relative_to 抛出 ValueError 时返回原 path 的 as_posix 字符串。
#
## Constraints and lifecycle
# 同步借用路径对象，不保存到全局；resolve 会观察文件系统并跟随符号链接，不提供并发修改下的快照。仅捕获 ValueError；其他解析或 I/O 异常可向上传播。
#
## References
# - scripts/validate_wiki.py:66
# - scripts/validate_wiki.py:68
# - scripts/validate_wiki.py:88
# - scripts/validate_wiki.py:193
# - scripts/validate_wiki.py:282

# @fn _inside(path: Path, directory: Path) -> bool
#
## Summary
# 以解析后的路径祖先关系检查目录包含性。
# 清单路径和本地链接检查在进行后续 is_file 检查前使用。
#
## Parameters
# @param[in] path 待核对的目标路径。
# @param[in] directory 允许的目录基准。
#
## Returns
# @return relative_to 成功时为 True；该操作抛出 ValueError 时为 False。
#
## Constraints and lifecycle
# 同步借用且不修改输入；resolve 会跟随符号链接。返回值只说明本次路径解析的包含关系，不证明存在性、可读权限或后续访问的原子性。未捕获的 resolve 异常仍可传播；调用者不能把所有异常视为 False。
#
## References
# - scripts/validate_wiki.py:178
# - scripts/validate_wiki.py:226
# - scripts/validate_wiki.py:295

# @fn _read_utf8(path: Path, errors: list[str], docs_root: Path) -> str | None
#
## Summary
# 将单页 UTF-8 读取失败转换成可继续聚合的诊断。
# _validate_markdown 开始检查页面时读取完整文本。
#
## Parameters
# @param[in] path 待读取的文件路径。
# @param[in,out] errors 接收新增诊断的列表；不先清空既有内容。
# @param[in] docs_root 诊断路径的显示基准。
#
## Returns
# @return 读取成功返回完整字符串，空文件返回空字符串；捕获 UnicodeDecodeError 或 OSError 后追加诊断并返回 None。
#
## Constraints and lifecycle
# Path.read_text 管理本次文件打开和关闭；不返回文件句柄。仅 errors 被追加，调用期间不得由其他执行流并发修改该列表。异常处理中的路径格式化也可能抛出未捕获异常；不承诺所有失败都可转换成 None。
#
## References
# - scripts/validate_wiki.py:84
# - scripts/validate_wiki.py:64
# - scripts/validate_wiki.py:65
# - scripts/validate_wiki.py:67

# @fn _front_matter_end(lines: list[str]) -> int | None
#
## Summary
# 在行列表中寻找首个 front matter 结束分隔行。
# 页面预检据此区分无头部、未闭合头部和必需键扫描区间。
#
## Parameters
# @param[in] lines 按 splitlines 得到的页面行列表；调用期间保持稳定。
#
## Returns
# @return 首行去除首尾空白后为 --- 时，返回后续首个同样为 --- 的行索引；否则或找不到结束行时返回 None。
#
## Constraints and lifecycle
# 只读借用列表，不解释 YAML 的值、嵌套或语法；None 同时表示缺起始行及缺结束行，调用者另读首行分类。索引采用零基，结束分隔行不属于键扫描内容。
#
## References
# - scripts/validate_wiki.py:90
# - scripts/validate_wiki.py:91
# - scripts/validate_wiki.py:97

# @fn _validate_markdown(path: Path, docs_root: Path, errors: list[str]) -> str | None
#
## Summary
# 在渲染前检查单页可读性、来源键与文本卫生。
# validate 对枚举到的每个 Markdown 文件调用，返回的文本继续用于链接缓存。
#
## Parameters
# @param[in] path 当前页面的文件路径。
# @param[in] docs_root 用于显示诊断路径的文档根目录。
# @param[in,out] errors 顺序追加本页诊断的列表，不清空已有项。
#
## Returns
# @return 读取失败返回 None；读取成功则返回原文本，即使该页已经追加格式诊断。
#
## Constraints and lifecycle
# 只检查必需 front matter 键名是否出现，不校验 YAML 值。逐行检查尾部空白、空 ATX 标题，并按围栏标记行的奇偶数作粗略闭合检查；不等同完整 Markdown 解析，也不同于 _content_lines 的字符和长度状态机。不修改文件；errors 需要调用方独占写入。后续链接检查仍可对有格式错误但可读的页面产生诊断。
#
## References
# - scripts/validate_wiki.py:320
# - scripts/validate_wiki.py:102
# - scripts/validate_wiki.py:108
# - scripts/validate_wiki.py:114
# - tests/scripts/test_validate_wiki.py:88
# - tests/scripts/test_validate_wiki.py:90

# @fn _slugify_heading(title: str) -> str
#
## Summary
# 按预检器的近似规则生成标题锚点片段。
# _heading_anchors 为每个 ATX 标题生成基础 slug，随后处理重复序号。
#
## Parameters
# @param[in] title 已由标题正则提取的标题字符串。
#
## Returns
# @return 移除指定 Markdown 标记、转小写、删去非 ASCII 字母数字及允许字符，再以连字符合并空白后的字符串；结果可以为空。
#
## Constraints and lifecycle
# 输入字符串只读，结果独立。该算法近似文档主题的锚点规则，不保证与任意渲染器、插件、HTML 显式锚点或 Unicode 标题规则完全一致；最终站点由独立的严格构建检查。
#
## References
# - scripts/validate_wiki.py:135
# - scripts/validate_wiki.py:124
# - requirements-docs.txt:1
# - .github/workflows/wiki-pages.yml:72

# @fn _heading_anchors(text: str) -> set[str]
#
## Summary
# 为一页可见正文建立含重复标题后缀的锚点集合。
# _validate_links 在遍历链接前，为传入的每页文本建立一次缓存。
#
## Parameters
# @param[in] text 页面完整文本；本函数不读取文件。
#
## Returns
# @return 新建的锚点集合；重复基础 slug 按出现次数生成 -1、-2 等后缀，空 slug 不加入。
#
## Constraints and lifecycle
# 只对 _content_lines 产出的围栏外 ATX 标题匹配，不保证识别全部 Markdown 标题或显式锚点。临时计数字典与集合限于本次调用；返回集合由调用者持有，输入文本不修改。不同基础标题产生的同名结果由 set 合并。
#
## References
# - scripts/validate_wiki.py:280
# - scripts/validate_wiki.py:131
# - scripts/validate_wiki.py:138
# - scripts/validate_wiki.py:140

# @fn _manifest_path(manifest: dict, docs_root: Path, errors: list[str]) -> tuple[set[str], int]
#
## Summary
# 检查 pages 数组并归一可参与集合比较的页面路径。
# _validate_manifest 用其路径集合与实际 Markdown 文件集合进行双向比较。
#
## Parameters
# @param[in] manifest 已确认根为 dict 的清单对象；读取 pages 而不修改。
# @param[in] docs_root 清单相对路径的解析目录。
# @param[in,out] errors 接收页面对象、标识和路径诊断；保留旧诊断。
#
## Returns
# @return 二元组为路径集合及 pages 原始列表长度；pages 非列表时为 (set(), 0)，该计数不是有效页面数。
#
## Constraints and lifecycle
# 非对象项会被跳过；raw_path 不是非空字符串，或路径为绝对路径、含 .. 部件时，诊断后跳过对应路径。无效或重复的 page_id 只追加诊断，不阻止随后处理 raw_path。符合初步路径形状后即加入集合，再检查目录包含性、文件存在及 .md 后缀，所以返回集合仍可包含随后被诊断为无效的路径；集合也会合并重复路径，重复另有诊断。同步借用清单，局部集合独立；只追加 errors，不创建或修复文件，不提供文件系统快照。
#
## References
# - scripts/validate_wiki.py:229
# - scripts/validate_wiki.py:148
# - scripts/validate_wiki.py:170
# - scripts/validate_wiki.py:177
# - scripts/validate_wiki.py:184
# - tests/scripts/test_validate_wiki.py:75

# @fn _validate_manifest(manifest_path: Path, docs_root: Path, errors: list[str]) -> tuple[int, set[str]]
#
## Summary
# 聚合导航清单的结构、目标、父级和页面全集诊断。
# validate 在页面扫描后、链接扫描前调用；清单不存在由上层单独处理。
#
## Parameters
# @param[in] manifest_path 待读取的 UTF-8 JSON 清单文件。
# @param[in] docs_root 页面和 section 的相对路径基准。
# @param[in,out] errors 追加诊断，不清空既有内容。
#
## Returns
# @return 返回 pages 原列表长度及归一页面路径集合；读入或 JSON 解析失败、根非对象时返回 (0, set())。
#
## Constraints and lifecycle
# schema 以 != 1 比较，非完整 JSON Schema 验证。section 标识与路径逐项检查；页面全集排除根目录 README.md，其余枚举到的 *.md 与 pages 集合双向比较。父级非 None 时用 section_ids 成员测试；不承诺任意畸形字段都转成诊断，例如不可哈希父级可使该测试抛出异常。只捕获清单读入和 JSON 解析阶段列出的异常；后续目录枚举或解析异常可传播，已追加诊断不回滚。不修改清单或页面，局部集合在返回后仅由结果持有。
#
## References
# - scripts/validate_wiki.py:329
# - scripts/validate_wiki.py:192
# - scripts/validate_wiki.py:198
# - scripts/validate_wiki.py:229
# - scripts/validate_wiki.py:233
# - scripts/validate_wiki.py:243

# @fn _is_external_link(target: str) -> bool
#
## Summary
# 判定链接是否应跳过文档树内的本地目标检查。
# _validate_links 完成 URL 解码和首尾空白移除后使用。
#
## Parameters
# @param[in] target 已归一待分类的链接目标字符串。
#
## Returns
# @return 已列出的外部 scheme 前缀大小写不敏感命中，或字符串以 / 开始时返回 True；否则为 False。
#
## Constraints and lifecycle
# 函数不访问网络、不验证 URI，也不验证站点根路径的部署目标。scheme 表属于模块级约定；未列出的 scheme 不自动视为外部。只读借用字符串，无缓存及持久状态。
#
## References
# - scripts/validate_wiki.py:287
# - scripts/validate_wiki.py:250
# - scripts/validate_wiki.py:27

# @fn _content_lines(text: str) -> Iterable[str]
#
## Summary
# 延迟产出围栏代码之外的原始文本行。
# 标题和链接扫描共享此正文视图，减少代码示例被解释为导航目标。
#
## Parameters
# @param[in] text 保持行尾信息的源文本；生成器在迭代期间持有该字符串。
#
## Returns
# @return 一个单次推进的生成器，按原顺序 yield 围栏外的行，并保留行尾字符。
#
## Constraints and lifecycle
# 调用本函数只创建生成器，splitlines 与状态推进从首次迭代才开始；没有打开文件等必须 close 的外部资源。最多三个由正则 \s 匹配的前导空白字符之后，三枚以上反引号或波浪号可开启围栏；同字符且长度不短于开启标记可关闭，标记行自身总跳过。未闭合围栏后的内容不产出。该规则不是完整 Markdown 词法解析，不排除全部行内代码或 HTML 内容；不得并发推进同一生成器。
#
## References
# - scripts/validate_wiki.py:131
# - scripts/validate_wiki.py:283
# - scripts/validate_wiki.py:258
# - scripts/validate_wiki.py:265
# - scripts/validate_wiki.py:270

# @fn _validate_links(documents: dict[Path, str], docs_root: Path, errors: list[str]) -> int
#
## Summary
# 聚合行内本地链接的目标及近似标题锚点诊断。
# validate 将成功读取的页面文本传入；先为全部输入页面建立标题缓存，再逐页匹配。
#
## Parameters
# @param[in] documents 解析路径到页面文本的映射；调用期间不得修改。
# @param[in] docs_root 允许本地目标位于其中的文档目录。
# @param[in,out] errors 追加链接诊断，保留此前页面和清单诊断。
#
## Returns
# @return 未被空目标或外部分类跳过的本地链接匹配次数；计数包含缺目标及缺锚点链接，不是唯一有效链接数。
#
## Constraints and lifecycle
# 只识别 LINK_PATTERN 的行内写法，URL 解码后以 # 分割路径和锚点；仅锚点链接指回源页。先检查目录包含性及文件存在，再对 .md 目标检查非空锚点；非 Markdown 文件只查目标。标题缓存只来自传入映射，近似规则不保证渲染器全部锚点语义；路径检查无原子快照。临时缓存不保存到全局；只追加 errors，不改输入或文件。TODO: 此预检不解析 Markdown 的引用式链接；引入该写法前须扩展扫描并补回归样例。
#
## References
# - scripts/validate_wiki.py:331
# - scripts/validate_wiki.py:276
# - scripts/validate_wiki.py:280
# - scripts/validate_wiki.py:294
# - scripts/validate_wiki.py:300
# - tests/scripts/test_validate_wiki.py:104
# - tests/scripts/test_validate_wiki.py:106
# - tests/scripts/test_validate_wiki.py:108

# @fn validate(root: Path, *, site_dir: Path | None = None) -> ValidationResult
#
## Summary
# 同步扫描源文档、导航与链接，并可附加站点入口检查。
# 供作者命令、CI 和定向测试调用；站点检查用于渲染之后，函数自身不调用渲染器。
#
## Parameters
# @param[in] root 仓库根路径；转换为 Path 并 resolve 后使用其 docs/wiki 子目录。
# @param[in] site_dir 可选站点目录；相对路径以已解析的 root 为基准，绝对路径直接使用。
#
## Returns
# @return 新建的 ValidationResult；目录缺失时返回单条诊断和零计数，正常扫描结束后冻结聚合诊断及描述性计数。
#
## Constraints and lifecycle
# 页面可读但格式有误时仍继续缓存和链接检查；不可读页不加入文本映射。manifest 只在存在时读取；其原列表数量不保证有效。site_dir 只检查目录、index.html 存在及非空，并统计文件数量，不解析 HTML、不保证每页生成正确，也不要求该站点位于 root 内。文件系统在多次读取与 stat 间可能变化；仅实现中明确捕获的异常转为诊断，其余可传播。每次调用使用独立局部容器，不保存调用方参数或打开文件；并发使用需调用方保证目录内容与模块状态的稳定，不提供锁或快照。
#
## References
# - scripts/validate_wiki.py:372
# - scripts/validate_wiki.py:315
# - scripts/validate_wiki.py:317
# - scripts/validate_wiki.py:322
# - scripts/validate_wiki.py:329
# - scripts/validate_wiki.py:331
# - scripts/validate_wiki.py:339
# - scripts/validate_wiki.py:349
# - tests/scripts/test_validate_wiki.py:59
# - tests/scripts/test_validate_wiki.py:118

# @fn _build_parser() -> argparse.ArgumentParser
#
## Summary
# 建立供 CLI 使用的新 argparse 参数解析器。
# main 在开始解析命令行时调用一次。
#
## Returns
# @return 一个新建的可变 ArgumentParser，描述来自现有模块 docstring，并注册 --root 与 --site-dir。
#
## Constraints and lifecycle
# 本函数不解析 argv、不执行校验。--root 默认值在建立解析器时通过 Path.cwd 获取；两个选项均以 Path 类作为 argparse 的转换回调，目录存在性和路径 resolve 由后续 validate 处理。返回对象由调用者持有；没有持久缓存，cwd 获取失败或解析器构造异常可传播。
#
## References
# - scripts/validate_wiki.py:371
# - scripts/validate_wiki.py:353
# - scripts/validate_wiki.py:356
# - scripts/validate_wiki.py:357
# - scripts/validate_wiki.py:362

# @fn main(argv: Iterable[str] | None = None) -> int
#
## Summary
# 把正常完成的校验结果转换为 CLI 输出和退出码。
# 脚本入口调用；现有工作流分别在源扫描和严格站点构建之后配置调用。
#
## Parameters
# @param[in] argv 传给 argparse 的参数可迭代对象；为 None 时由 argparse 使用进程参数，迭代器可被消费。
#
## Returns
# @return 校验正常返回且 errors 为空时返回 0，否则打印诊断后返回 1。
#
## Constraints and lifecycle
# 先解析参数再调用 validate；将各条错误和失败总数写 stderr，将扫描摘要及成功消息写 stdout。摘要计数不决定成功。argparse 的帮助或参数错误会以 SystemExit 结束，不属于本函数返回 0/1 的路径；未捕获的校验、路径或流写入异常也可传播。同步借用 argv 和标准流，不保存参数、不关闭标准流，不建立站点、fixture 或日志文件。
#
## References
# - scripts/validate_wiki.py:390
# - scripts/validate_wiki.py:371
# - scripts/validate_wiki.py:374
# - scripts/validate_wiki.py:381
# - scripts/validate_wiki.py:384
# - scripts/validate_wiki.py:386
# - .github/workflows/wiki-pages.yml:66
# - .github/workflows/wiki-pages.yml:75
