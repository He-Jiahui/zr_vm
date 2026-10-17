"""用最小临时 wiki 隔离源校验规则，并以真实仓库覆盖 CI 的集成入口。"""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "scripts" / "validate_wiki.py"
SPEC = importlib.util.spec_from_file_location("validate_wiki", SCRIPT_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load {SCRIPT_PATH}")
validate_wiki = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validate_wiki)


def write_document(path: Path, body: str, *, title: str = "Test page") -> None:
    """构造符合其余元数据要求的页面，让每个用例只引入目标错误。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "---\n"
        "related_code: []\n"
        "implementation_files: []\n"
        "plan_sources: []\n"
        "tests: []\n"
        "doc_type: test\n"
        "---\n\n"
        f"# {title}\n\n"
        f"{body}\n",
        encoding="utf-8",
    )


def write_manifest(root: Path, paths: list[str]) -> None:
    """为临时页面建立可控导航；清单用例可引入缺项，其余用例免受导航错误干扰。"""

    manifest = {
        "schema": 1,
        "title": "Test Wiki",
        "sections": [],
        "pages": [
            {"id": path.replace("/", "-").removesuffix(".md"), "path": path, "kind": "guide", "parent": None}
            for path in paths
        ],
    }
    (root / "docs/wiki").mkdir(parents=True, exist_ok=True)
    (root / "docs/wiki/manifest.json").write_text(
        json.dumps(manifest), encoding="utf-8"
    )


class WikiValidationTests(unittest.TestCase):
    """覆盖真实文档入口及各类独立失败，避免发布流程吞掉源诊断。"""

    def test_repository_wiki_is_valid(self) -> None:
        result = validate_wiki.validate(REPO_ROOT)

        self.assertEqual(result.errors, ())
        self.assertEqual(result.markdown_files, 116)
        self.assertEqual(result.manifest_pages, 115)
        self.assertGreaterEqual(result.local_links, 100)

    def test_missing_manifest_page_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_document(root / "docs/wiki/index.md", "A valid page.")
            write_manifest(root, ["index.md", "missing.md"])

            result = validate_wiki.validate(root)

            self.assertTrue(
                any("manifest page target does not exist" in error for error in result.errors)
            )

    def test_missing_required_front_matter_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs/wiki"
            docs.mkdir(parents=True)
            (docs / "index.md").write_text(
                "---\nrelated_code: []\n---\n\n# Incomplete\n", encoding="utf-8"
            )
            write_manifest(root, ["index.md"])

            result = validate_wiki.validate(root)

            self.assertTrue(any("missing front matter field" in error for error in result.errors))

    def test_local_link_and_anchor_failures_are_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_document(
                root / "docs/wiki/index.md",
                "[missing](missing.md) and [bad anchor](target.md#does-not-exist) "
                "and [local anchor](#does-not-exist).\n\n"
                "```markdown\n[fenced](missing-from-example.md)\n```",
            )
            write_document(root / "docs/wiki/target.md", "A target.", title="Target")
            write_manifest(root, ["index.md", "target.md"])

            result = validate_wiki.validate(root)

            self.assertTrue(any("missing link target" in error for error in result.errors))
            self.assertTrue(any("missing link anchor" in error for error in result.errors))
            self.assertFalse(any("missing-from-example.md" in error for error in result.errors))

    def test_site_output_requires_index(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_document(root / "docs/wiki/index.md", "A valid page.")
            write_manifest(root, ["index.md"])
            site_dir = root / "site"
            site_dir.mkdir()

            result = validate_wiki.validate(root, site_dir=site_dir)

            self.assertTrue(any("site output missing index.html" in error for error in result.errors))


if __name__ == "__main__":
    unittest.main(verbosity=2)

# @unit module loader state [state]
# ## Summary
# 在测试模块导入时，从同一 checkout 的明确文件路径建立校验器模块对象，供五个用例调用，并固定真实仓库用例的根。
# ## Fields
# REPO_ROOT 取当前测试文件 resolve 后的 parents[2]；SCRIPT_PATH 指向该根下 scripts/validate_wiki.py，不由 cwd 或测试参数选择。
# SPEC、validate_wiki 分别持有加载描述及执行后的模块对象；SPEC 或 loader 缺失即在测试发现前抛 RuntimeError。
# ## Constraints and lifecycle
# exec_module 使用非 __main__ 名称，不调用校验器 CLI；校验器的定义、正则和只读约定状态在此建立，validate 的扫描到各测试方法才开始。
# 本段没有把 validate_wiki 显式写入 sys.modules；当前校验器的 NamedTuple 定义及本测试读取字段的路径不要求这里额外注册，不把该加载器当作满足任意模块自省要求的通用导入接口。
# 模块对象被本测试模块持有到解释器结束；使用默认字节码策略时加载器可能读写 pycache，缓存位置及禁写设置由运行环境控制。
# 导入期间无临时 wiki 创建；模块执行异常可使测试发现失败。运行时应保证脚本内容稳定，不并发改写模块状态或源码。
# ## References
# tests/scripts/test_validate_wiki.py:10
# tests/scripts/test_validate_wiki.py:12
# tests/scripts/test_validate_wiki.py:13
# tests/scripts/test_validate_wiki.py:15
# tests/scripts/test_validate_wiki.py:16
# tests/scripts/test_validate_wiki.py:59
# tests/scripts/test_validate_wiki.py:72
# tests/scripts/test_validate_wiki.py:88
# tests/scripts/test_validate_wiki.py:104
# tests/scripts/test_validate_wiki.py:118
# scripts/validate_wiki.py:33
# scripts/validate_wiki.py:389

# @unit write_document [function]
# ## Summary
# 在调用方拥有的临时目录写出带完整必需 front matter 和标题的页面，让负向用例集中观察导航、链接或站点问题。
# ## Parameters
# path [in] 目标页面 Path；当前调用者传入自己 TemporaryDirectory 下的 docs/wiki 页面，函数本身不做根边界校验。
# body [in] 原样插入页面正文的 str；可以有意含失效链接或 fenced 示例。
# title [in] 作为一级标题原样插入的 str，关键字参数默认 Test page；不做 Markdown 转义或内容验证。
# ## Returns
# 成功写入后返回 None；目录创建、编码或写入异常向调用方传播，没有单独失败码。
# ## Constraints and lifecycle
# 先创建父目录，再以 UTF-8 和平台默认文本换行写入目标，覆盖已有文件；不是原子写入，异常不回滚已建目录或部分输出。
# 固定写入 related_code、implementation_files、plan_sources、tests、doc_type 五个键，只提供当前校验器要求的键名，不证明页面语义或链接正确。
# 不持有打开句柄或全局缓存；文件清理由拥有临时根的外层上下文承担，同一目标的并发写入不受保护。
# ## References
# tests/scripts/test_validate_wiki.py:19
# tests/scripts/test_validate_wiki.py:22
# tests/scripts/test_validate_wiki.py:23
# tests/scripts/test_validate_wiki.py:25
# tests/scripts/test_validate_wiki.py:31
# tests/scripts/test_validate_wiki.py:69
# tests/scripts/test_validate_wiki.py:95
# tests/scripts/test_validate_wiki.py:101
# tests/scripts/test_validate_wiki.py:113
# scripts/validate_wiki.py:20
# scripts/validate_wiki.py:102

# @unit write_manifest [function]
# ## Summary
# 为临时页面建立最小导航清单，使各用例显式控制导航目标集合及缺失条目。
# ## Parameters
# root [in] 当前用例拥有的临时根 Path；在其下创建 docs/wiki 并覆盖 manifest.json，不限制任意外部传入路径。
# paths [in] 页面相对路径字符串列表；迭代期间保持稳定，内容原样成为 page.path，id 只替换斜线并去掉末尾 .md。
# ## Returns
# 成功写入后返回 None；不合法参数或 I/O、JSON 编码失败通过异常传达，不返回局部清单。
# ## Constraints and lifecycle
# 写入 schema=1、空 sections，page.kind 固定 guide、parent 固定 None；辅助函数不验证路径存在、包含性或 id 唯一性，这些由被测校验器检查。
# 清单先在内存构造，再创建目录并写 JSON；默认 JSON 转义及平台文本换行由 json.dumps/write_text 决定，写入不具有事务回滚。
# 当前四个用例仅向独立临时根写入；正常和异常展开时由外层 TemporaryDirectory 清理，不修改真实仓库清单。
# ## References
# tests/scripts/test_validate_wiki.py:37
# tests/scripts/test_validate_wiki.py:40
# tests/scripts/test_validate_wiki.py:45
# tests/scripts/test_validate_wiki.py:49
# tests/scripts/test_validate_wiki.py:50
# tests/scripts/test_validate_wiki.py:70
# tests/scripts/test_validate_wiki.py:86
# tests/scripts/test_validate_wiki.py:102
# tests/scripts/test_validate_wiki.py:114
# scripts/validate_wiki.py:144
# scripts/validate_wiki.py:229

# @unit WikiValidationTests [type]
# ## Summary
# 由 unittest 发现并执行一个真实仓库集成用例与四个独立临时场景，验证校验器可观察的诊断与有限计数合同。
# ## Fields
# 五个 test_* 方法由 unittest 的模块/类发现机制注册，不存在源码内逐个直接调用。
# 本类未定义 setUp/tearDown；临时目录均由各方法自己的 with 上下文拥有，真实仓库用例只读 checkout。
# ## Constraints and lifecycle
# 负向场景以 any/substring 检查目标诊断，通常允许同时聚合其他相关诊断，不约定完整错误数量或顺序。
# 某个断言失败会结束该方法并由 unittest 记录，默认运行器继续其他用例；未捕获的 I/O 等异常记为 error，不是测试函数返回状态码。
# 这些用例不渲染站点、不执行校验器 CLI，也不证明完整 Markdown/YAML、所有链接语法或并发文件系统快照。
# 四个临时场景进入上下文后按 Python 异常展开清理；构造失败、进程强制结束或清理自身异常不保证目录已回收。
# ## References
# tests/scripts/test_validate_wiki.py:55
# tests/scripts/test_validate_wiki.py:58
# tests/scripts/test_validate_wiki.py:66
# tests/scripts/test_validate_wiki.py:78
# tests/scripts/test_validate_wiki.py:92
# tests/scripts/test_validate_wiki.py:110
# tests/scripts/test_validate_wiki.py:124
# .github/workflows/wiki-pages.yml:62

# @unit WikiValidationTests.test_repository_wiki_is_valid [function]
# ## Summary
# 对实际 checkout 的 Wiki 执行集成预检，先要求没有诊断，再核对固定页面/导航基线及最低本地链接数量。
# ## Parameters
# self [in] 由 unittest 创建和管理的本用例实例；仅借用其断言接口，不向其他方法传递结果或资源。
# ## Returns
# 全部断言满足则返回 None；断言不满足抛 AssertionError，由 unittest 记录失败，后续断言不再执行；未捕获校验异常记录为 error。
# ## Constraints and lifecycle
# REPO_ROOT 来自当前测试文件位置；本方法不创建或修改页面，但会读取当前 Wiki、manifest 和相关本地目标，调用期间内容需由运行方固定。
# errors==() 在页数与链接计数之前检查；不能由后面的断言文字推断它们每次均已到达。
# 116 Markdown、115 manifest pages 和至少100条本地链接是明确的固定断言，不是动态生成或容许任意新增页面的规则。
# 没有传入 site_dir，因此不检查构建输出、index.html 或渲染器效果；结果不保留打开文件或需要本方法释放的资源。
# ## References
# tests/scripts/test_validate_wiki.py:58
# tests/scripts/test_validate_wiki.py:59
# tests/scripts/test_validate_wiki.py:61
# tests/scripts/test_validate_wiki.py:62
# tests/scripts/test_validate_wiki.py:63
# tests/scripts/test_validate_wiki.py:64
# scripts/validate_wiki.py:305
# scripts/validate_wiki.py:317
# scripts/validate_wiki.py:324
# scripts/validate_wiki.py:331
# scripts/validate_wiki.py:349

# @unit WikiValidationTests.test_missing_manifest_page_is_reported [function]
# ## Summary
# 用只含 index 页的临时 Wiki 和额外 missing.md 导航条目，验证不存在的清单目标会被诊断。
# ## Parameters
# self [in] unittest 管理的本用例实例，只借用断言接口。
# ## Returns
# 目标诊断存在时返回 None；否则抛 AssertionError 交运行器记录。构建 fixture 或校验时的其他异常可使本方法以 error 结束。
# ## Constraints and lifecycle
# write_document 提供正常必需键，write_manifest 有意列出不存在的 missing.md；不创建该页面。
# 只断言 errors 中至少包含 manifest page target does not exist，不要求唯一诊断、固定数量或具体次序；后续清单集合检查也可报告额外问题。
# 临时根不用于真实 Wiki；进入 with 后正常和异常展开均由 TemporaryDirectory 清理，方法不返回目录或文件借用。
# 不覆盖路径逃逸、重复 id、parent 类型或任意畸形 JSON 清单。
# ## References
# tests/scripts/test_validate_wiki.py:66
# tests/scripts/test_validate_wiki.py:67
# tests/scripts/test_validate_wiki.py:69
# tests/scripts/test_validate_wiki.py:70
# tests/scripts/test_validate_wiki.py:72
# tests/scripts/test_validate_wiki.py:75
# scripts/validate_wiki.py:329
# scripts/validate_wiki.py:178
# scripts/validate_wiki.py:180
# scripts/validate_wiki.py:181
# scripts/validate_wiki.py:238

# @unit WikiValidationTests.test_missing_required_front_matter_is_reported [function]
# ## Summary
# 写入已闭合但只含 related_code 的 front matter，验证缺少其他必需来源键能产生诊断。
# ## Parameters
# self [in] unittest 管理的本用例实例，只借用断言接口。
# ## Returns
# 至少一个缺键诊断存在时返回 None；断言失败抛 AssertionError，其他未捕获异常作为 error 交运行器。
# ## Constraints and lifecycle
# 本方法直接写入不完整页面，绕过 write_document 的完整键模板，并生成与页面相配的最小 manifest。
# 只检查 missing front matter field 子串，不分别断言四个缺失键、诊断数量或 YAML 值验证；闭合分隔行保证本场景进入键名检查分支。
# 只使用独立临时根，UTF-8 文本写入可能使用平台换行；正常及异常展开由 with 清理，不改变仓库源。
# 不覆盖无起始分隔行、未闭合头部或无法读取/解码的页面。
# ## References
# tests/scripts/test_validate_wiki.py:78
# tests/scripts/test_validate_wiki.py:79
# tests/scripts/test_validate_wiki.py:82
# tests/scripts/test_validate_wiki.py:83
# tests/scripts/test_validate_wiki.py:84
# tests/scripts/test_validate_wiki.py:86
# tests/scripts/test_validate_wiki.py:88
# tests/scripts/test_validate_wiki.py:90
# scripts/validate_wiki.py:90
# scripts/validate_wiki.py:97
# scripts/validate_wiki.py:102
# scripts/validate_wiki.py:104

# @unit WikiValidationTests.test_local_link_and_anchor_failures_are_reported [function]
# ## Summary
# 用两个临时页面同时观察缺少本地目标、缺少标题锚点及 fenced 示例链接被跳过的有限行为。
# ## Parameters
# self [in] unittest 管理的本用例实例，只借用断言接口。
# ## Returns
# 三个断言均满足则返回 None；任一失败抛 AssertionError 并跳过该方法后续断言，运行器仍可继续其他方法。
# ## Constraints and lifecycle
# index 含 missing.md、target.md#does-not-exist 和自身不存在锚点；target 页只有 Target 标题，清单列出这两个真实页面。
# 分别只断言至少一个 missing link target、至少一个 missing link anchor，再断言所有诊断都未提及 fenced 示例文件名；未区分两个锚点各自命中或固定本地链接计数。
# 代码围栏覆盖的是给定成对反引号样例，不证明引用式、行内代码、HTML 或全部 Markdown 链接/围栏语法。
# 所有文件位于同一独立 TemporaryDirectory；进入 with 后异常也触发上下文清理，校验器只读这些文件。
# ## References
# tests/scripts/test_validate_wiki.py:92
# tests/scripts/test_validate_wiki.py:93
# tests/scripts/test_validate_wiki.py:95
# tests/scripts/test_validate_wiki.py:97
# tests/scripts/test_validate_wiki.py:99
# tests/scripts/test_validate_wiki.py:101
# tests/scripts/test_validate_wiki.py:102
# tests/scripts/test_validate_wiki.py:104
# tests/scripts/test_validate_wiki.py:106
# tests/scripts/test_validate_wiki.py:107
# tests/scripts/test_validate_wiki.py:108
# scripts/validate_wiki.py:283
# scripts/validate_wiki.py:295
# scripts/validate_wiki.py:296
# scripts/validate_wiki.py:300
# scripts/validate_wiki.py:301
# scripts/validate_wiki.py:265

# @unit WikiValidationTests.test_site_output_requires_index [function]
# ## Summary
# 在正常临时 Wiki 旁建立空 site 目录，验证可选站点检查拒绝缺少 index.html 的输出。
# ## Parameters
# self [in] unittest 管理的本用例实例，只借用断言接口。
# ## Returns
# errors 包含缺入口诊断时返回 None；否则抛 AssertionError，由 unittest 记录失败；未捕获 I/O 异常可作为 error 结束。
# ## Constraints and lifecycle
# 显式传入已存在且为空的绝对 site_dir；此场景进入目录存在而 index.html 不存在的分支，不是目录不存在或空 index 文件分支。
# 只断言 site output missing index.html 子串，不检查 site_files 计数、非空入口成功路径或页面内容；本方法不调用站点构建器。
# 页面、manifest 与空 site 均归同一临时根拥有；with 上下文负责正常及异常展开清理，不返回文件系统借用。
# ## References
# tests/scripts/test_validate_wiki.py:110
# tests/scripts/test_validate_wiki.py:111
# tests/scripts/test_validate_wiki.py:113
# tests/scripts/test_validate_wiki.py:114
# tests/scripts/test_validate_wiki.py:115
# tests/scripts/test_validate_wiki.py:116
# tests/scripts/test_validate_wiki.py:118
# tests/scripts/test_validate_wiki.py:120
# scripts/validate_wiki.py:334
# scripts/validate_wiki.py:339
# scripts/validate_wiki.py:340
