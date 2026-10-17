"""Safety tests for ordered generated-include classification."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
MODULE = REPO / "scripts" / "layout" / "generated_includes.py"
SPEC = importlib.util.spec_from_file_location("generated_includes", MODULE)
assert SPEC and SPEC.loader
classifier = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = classifier
SPEC.loader.exec_module(classifier)

FIXTURE_ROOT = Path("E:/cargo-targets/zr_vm/tests/layout-01a13033/generated-includes-fixtures")
REAL_BUILD = Path("E:/cargo-targets/zr_vm/build/layout-01a13033/msvc-static-debug")


class GeneratedIncludeTests(unittest.TestCase):
    def setUp(self) -> None:
        FIXTURE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case-", dir=FIXTURE_ROOT)
        self.base = Path(self.temp.name)
        self.root = self.base / "repo"
        self.build = self.base / "build"
        self.root.mkdir()
        (self.build / "tests_generated").mkdir(parents=True)
        (self.root / "tests" / "synthetic_case").mkdir(parents=True)
        self.including = "tests/synthetic_case/source.c"
        (self.root / self.including).write_text("/* fixture source */\n", encoding="utf-8")

    def tearDown(self) -> None:
        # TemporaryDirectory owns only this test's unique child under registered E: tests root.
        self.temp.cleanup()

    def classify(self, name: str, variants: tuple[tuple[Path, ...], ...]):
        return classifier.classify_include(self.root, self.build, self.including, name, variants)

    def test_real_build_generated_header_is_classified_without_reading_contents(self) -> None:
        path = REAL_BUILD / "tests_generated" / classifier.GENERATED_HEADER
        if not path.is_file():
            self.skipTest("real configured build header is unavailable")
        roots = (REAL_BUILD / "tests_generated",)
        results = classifier.classify_include(
            REPO,
            REAL_BUILD,
            "tests/ffi/test_ffi_module.c",
            classifier.GENERATED_HEADER,
            (roots,),
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].kind, "generated")
        self.assertEqual(results[0].path, path.resolve())
        self.assertIsNone(results[0].relative_path)

    def test_exact_generated_output_is_accepted_and_same_ordered_result_kept_per_variant(self) -> None:
        generated = self.build / "tests_generated" / classifier.GENERATED_HEADER
        generated.write_bytes(b"opaque generated contents\x00")
        roots = (self.build / "tests_generated",)
        results = self.classify(classifier.GENERATED_HEADER, (roots, roots))
        self.assertEqual([result.kind for result in results], ["generated", "generated"])
        self.assertEqual([result.path for result in results], [generated.resolve(), generated.resolve()])

    def test_source_directory_shadow_wins_before_generated_root(self) -> None:
        local = self.root / "tests/synthetic_case" / classifier.GENERATED_HEADER
        local.write_text("local repository fixture\n", encoding="utf-8")
        (self.build / "tests_generated" / classifier.GENERATED_HEADER).write_text("generated", encoding="utf-8")
        results = self.classify(classifier.GENERATED_HEADER, ((self.build / "tests_generated",),))
        self.assertEqual(results[0].kind, "repository")
        self.assertEqual(results[0].path, local.resolve())
        self.assertEqual(results[0].relative_path, "tests/synthetic_case/" + classifier.GENERATED_HEADER)

    def test_first_existing_ordered_repository_root_wins(self) -> None:
        first = self.root / "tests/first"
        second = self.root / "tests/second"
        first.mkdir()
        second.mkdir()
        a = first / "synthetic_probe.h"
        b = second / "synthetic_probe.h"
        a.write_text("first\n", encoding="utf-8")
        b.write_text("second\n", encoding="utf-8")
        result = self.classify("synthetic_probe.h", ((first, second),))[0]
        self.assertEqual(result.kind, "repository")
        self.assertEqual(result.path, a.resolve())

    def test_repository_parent_relative_include_resolves_without_external_escape(self) -> None:
        parent = self.root / "tests/parent_probe.h"
        parent.write_text("repository parent target\n", encoding="utf-8")
        result = classifier.classify_include(
            self.root, self.build, self.including, "../parent_probe.h", ((),)
        )[0]
        self.assertEqual(result.kind, "repository")
        self.assertEqual(result.path, parent.resolve())
        self.assertEqual(result.relative_path, "tests/parent_probe.h")

    def test_generated_header_alias_is_not_treated_as_the_exact_generated_name(self) -> None:
        generated = self.build / "tests_generated" / classifier.GENERATED_HEADER
        generated.write_text("opaque\n", encoding="utf-8")
        result = self.classify("nested/../" + classifier.GENERATED_HEADER,
                               ((self.build / "tests_generated",),))[0]
        self.assertEqual(result.kind, "unsafe")
    def test_wrong_external_name_and_wrong_build_generated_root_are_unsafe(self) -> None:
        generated_root = self.build / "tests_generated"
        (generated_root / "other_generated.h").write_text("x", encoding="utf-8")
        other_build = self.base / "other-build"
        (other_build / "tests_generated").mkdir(parents=True)
        (other_build / "tests_generated" / classifier.GENERATED_HEADER).write_text("x", encoding="utf-8")
        wrong_name = self.classify("other_generated.h", ((generated_root,),))[0]
        wrong_build = self.classify(classifier.GENERATED_HEADER, ((other_build / "tests_generated",),))[0]
        self.assertEqual(wrong_name.kind, "unsafe")
        self.assertEqual(wrong_build.kind, "unsafe")

    def test_missing_header_is_unresolved_and_variants_are_not_collapsed(self) -> None:
        generated_root = self.build / "tests_generated"
        generated = generated_root / classifier.GENERATED_HEADER
        repo_header = self.root / "tests/synthetic_case" / classifier.GENERATED_HEADER
        results = self.classify(classifier.GENERATED_HEADER, ((generated_root,), ()))
        self.assertEqual([result.kind for result in results], ["unresolved", "unresolved"])
        generated.write_text("generated", encoding="utf-8")
        alternate_root = self.root / "tests/alternate"
        alternate_root.mkdir()
        alternate_header = alternate_root / classifier.GENERATED_HEADER
        alternate_header.write_text("repository variant", encoding="utf-8")
        mixed = self.classify(classifier.GENERATED_HEADER, ((generated_root,), (alternate_root,)))
        self.assertEqual([result.kind for result in mixed], ["generated", "repository"])
        alternate_header.unlink()
        partial = self.classify(classifier.GENERATED_HEADER, ((generated_root,), (self.root / "tests" / "absent",)))
        self.assertEqual([result.kind for result in partial], ["generated", "unresolved"])

    def test_acceptance_snapshot_and_vendor_paths_are_rejected(self) -> None:
        for relative in (
            "tests/acceptance/synthetic_case.h",
            "tests/snapshots/synthetic_case.h",
            "tests/third_party/synthetic_vendor/synthetic_case.h",
        ):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n", encoding="utf-8")
            include = path.relative_to(self.root).as_posix()
            source = "tests/synthetic_case/source.c"
            result = classifier.classify_include(self.root, self.build, source, include, ((self.root,),))[0]
            self.assertEqual(result.kind, "unsafe", relative)

    def test_reparse_candidate_is_rejected_when_symlink_creation_is_supported(self) -> None:
        target = self.root / "tests/synthetic_case/real_probe.h"
        target.write_text("safe target\n", encoding="utf-8")
        link = self.root / "tests/synthetic_case/symlink_probe.h"
        try:
            link.symlink_to(target)
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"symlink unavailable: {exc}")
        result = self.classify("symlink_probe.h", ((),))[0]
        self.assertEqual(result.kind, "unsafe")


if __name__ == "__main__":
    unittest.main()

# @unit REPO
# ## Summary
# 从当前测试文件定位真实checkout，供分类器装载和可选真实构建探针使用。
# ## Constraints and lifecycle
# 值由__file__解析后向上两级取得，不是合成repo目录self.root，也不是夹具输出根。该赋值不复制源码或创建目录；如果测试文件迁移，层级关系也须随之核实。
# ## References
# - tests/tooling/test_layout_generated_includes.py:11
# - tests/tooling/test_layout_generated_includes.py:12
# - tests/tooling/test_layout_generated_includes.py:49

# @unit MODULE
# ## Summary
# 指定需要从当前checkout装载的分类器源码文件。
# ## Constraints and lifecycle
# 这是基于REPO组成的Path，不是模块对象或源码副本。真正读取与执行由后续loader完成；此处没有生成目录和文件写入。
# ## References
# - tests/tooling/test_layout_generated_includes.py:12
# - tests/tooling/test_layout_generated_includes.py:13
# - tests/tooling/test_layout_generated_includes.py:17

# @unit SPEC
# ## Summary
# 保存显式文件位置加载分类器所需的模块规格。
# ## Constraints and lifecycle
# 使用固定模块名generated_includes。后续断言检查spec和loader，随后交给module_from_spec与exec_module；spec不是已成功执行的模块，也不代表装载异常已被捕获。
# ## References
# - tests/tooling/test_layout_generated_includes.py:13
# - tests/tooling/test_layout_generated_includes.py:14
# - tests/tooling/test_layout_generated_includes.py:15
# - tests/tooling/test_layout_generated_includes.py:17

# @unit classifier
# ## Summary
# 持有分类器的ModuleType对象，供测试访问常量和函数。
# ## Constraints and lifecycle
# module_from_spec先创建对象，加载协议随后在sys.modules发布并执行源码；此后所有case共用该模块。tearDown只清理夹具，不恢复该全局或sys.modules项。它与apply_moves按另一模块名加载的对象身份不同，虽来自同一物理源码。
# ## References
# - tests/tooling/test_layout_generated_includes.py:15
# - tests/tooling/test_layout_generated_includes.py:16
# - tests/tooling/test_layout_generated_includes.py:17
# - tests/tooling/test_layout_generated_includes.py:41
# - scripts/layout/apply_moves.py:75
# - scripts/layout/apply_moves.py:82

# @unit FIXTURE_ROOT
# ## Summary
# 固定专用测试的临时夹具父根，约束所有合成目录的落点。
# ## Constraints and lifecycle
# 字面值为E:/cargo-targets/zr_vm/tests/layout-01a13033/generated-includes-fixtures；TemporaryDirectory显式使用dir，TEMP/TMP不会覆盖它，apply_moves的环境覆盖项也不作用于此常量。运行者须确认该固定根的归属、权限及路径祖先；单个case仅拥有其随机子目录，不能据此清理共享父根。
# ## References
# - tests/tooling/test_layout_generated_includes.py:19
# - tests/tooling/test_layout_generated_includes.py:25
# - tests/tooling/test_layout_generated_includes.py:26
# - tests/tooling/test_layout_generated_includes.py:38
# - scripts/layout/apply_moves.py:41
# - scripts/layout/apply_moves.py:43

# @unit REAL_BUILD
# ## Summary
# 指定可选真实生成头探针使用的历史MSVC构建目录。
# ## Constraints and lifecycle
# 字面值固定为E:/cargo-targets/zr_vm/build/layout-01a13033/msvc-static-debug。探针只检查路径元数据，不配置、构建、执行或改写该构建；头不存在时skip。它不随TEMP/TMP改变，也不能把路径存在等同于当前编译行为证明。
# ## References
# - tests/tooling/test_layout_generated_includes.py:20
# - tests/tooling/test_layout_generated_includes.py:43
# - tests/tooling/test_layout_generated_includes.py:58

# @unit module::classifier_import
# ## Summary
# 在定义测试类前将显式ModuleType登记到sys.modules并执行分类器源码。
# ## Constraints and lifecycle
# 先断言SPEC及loader，再创建classifier、覆盖sys.modules中固定名称的项并exec_module。该登记使dataclass装饰器能查询模块；协议不保存或恢复旧项，exec_module异常后也不回滚部分装载。导入会执行源码定义及其标准库导入；未使用-B时Python可能写字节码缓存。它不是临时目录生命周期的一部分。
# ## References
# - tests/tooling/test_layout_generated_includes.py:14
# - tests/tooling/test_layout_generated_includes.py:13
# - tests/tooling/test_layout_generated_includes.py:15
# - tests/tooling/test_layout_generated_includes.py:16
# - tests/tooling/test_layout_generated_includes.py:17
# - scripts/layout/generated_includes.py:22

# @unit GeneratedIncludeTests
# ## Summary
# 用独立合成文件树检验分类顺序、安全区域及两个可选平台探针。
# ## Fields and invariants
# self.temp拥有唯一TemporaryDirectory；self.base及self.root/self.build是该目录内的Path；self.including是合成源路径字符串。路径值的存在不延长被清理文件的生命周期。
# ## Constraints and lifecycle
# unittest按test_方法发现10个case，经setUp创建随机子目录并在成功setUp后的测试结束时调用tearDown。断言失败或skip不等同立即终止整个进程；setUp异常则不保证tearDown被调用。合成repo/build都是小型人工夹具，不复制checkout、不运行CMake。共享父根及真实构建不归单个case清理；类不提供跨进程根目录或文件系统修改锁。
# ## References
# - tests/tooling/test_layout_generated_includes.py:23
# - tests/tooling/test_layout_generated_includes.py:24
# - tests/tooling/test_layout_generated_includes.py:38
# - tests/tooling/test_layout_generated_includes.py:43
# - tests/tooling/test_layout_generated_includes.py:148
# - tests/tooling/test_layout_generated_includes.py:161

# @unit GeneratedIncludeTests.setUp
# ## Summary
# 为一个case建立独有的合成仓根、构建根及最小源文件。
# ## Parameters
# @param[in,out] self 待初始化的unittest实例；成功后持有此case的临时目录所有权。
# ## Returns
# 成功返回None；初始化异常上抛，由runner记录错误，没有失败码或资源返回。
# ## Constraints and lifecycle
# 先保证固定父根存在，再创建case-前缀的随机TemporaryDirectory及repo/build/tests_generated结构。所有内容为人工字符串，不复制真实源码。属性初始化和写文件不是事务；若setUp中途异常，unittest不调用tearDown，不能依赖最终器承诺立即释放。不会清空共享父根或旧case。
# ## References
# - tests/tooling/test_layout_generated_includes.py:24
# - tests/tooling/test_layout_generated_includes.py:34
# - tests/tooling/test_layout_generated_includes.py:36
# - tests/tooling/test_layout_generated_includes.py:38
# - tests/tooling/test_layout_generated_includes.py:161

# @unit GeneratedIncludeTests.tearDown
# ## Summary
# 释放当前case拥有的TemporaryDirectory及其中合成文件。
# ## Parameters
# @param[in,out] self 已成功取得临时目录所有权的测试实例。
# ## Returns
# 完成清理后返回None；清理错误向runner传播，无独立失败码。
# ## Constraints and lifecycle
# 只能清理self.temp的随机子目录，不删除FIXTURE_ROOT，也不清理REAL_BUILD。成功setUp后的普通断言失败和skip仍由unittest调度此hook；setUp失败不保证调度。cleanup的文件系统异常可传播并被runner记为错误，不能承诺所有外部干预下都成功删除。
# ## References
# - tests/tooling/test_layout_generated_includes.py:36
# - tests/tooling/test_layout_generated_includes.py:38
# - tests/tooling/test_layout_generated_includes.py:24
# - tests/tooling/test_layout_generated_includes.py:161

# @unit GeneratedIncludeTests.classify
# ## Summary
# 把当前case的合成根与源路径传给分类器，集中保持夹具上下文。
# ## Parameters
# @param[in] self 已完成setUp的测试实例，提供稳定夹具路径。
# @param[in] name 待分类include字符串。
# @param[in] variants 按顺序排列的Path根元组。
# ## Returns
# @return 返回classify_include的结果元组，或传播其异常；不额外折叠结果。
# ## Constraints and lifecycle
# 只转发本实例的root/build/including和调用方的name/variants，不创建、修改或清理文件，也不捕获分类器异常。返回对象由分类器构造；这些Path值在tearDown后仍是Python值，但不保证对应文件继续存在。
# ## References
# - tests/tooling/test_layout_generated_includes.py:40
# - tests/tooling/test_layout_generated_includes.py:41
# - tests/tooling/test_layout_generated_includes.py:64
# - tests/tooling/test_layout_generated_includes.py:86
# - tests/tooling/test_layout_generated_includes.py:156
# - scripts/layout/generated_includes.py:130

# @unit GeneratedIncludeTests.test_real_build_generated_header_is_classified_without_reading_contents
# ## Summary
# 在历史真实构建头可用时检查其分类和返回路径。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 先以is_file探测固定REAL_BUILD生成头，不存在即skip；存在时用真实REPO和固定源路径字符串请求分类。仅断言一个结果、generated标签、解析后Path及无relative_path；不打开生成内容、不执行编译，也未用读拦截器证明所有内容API均未调用。即使跳过，此case的setUp已创建临时夹具，仍由tearDown清理。
# ## References
# - tests/tooling/test_layout_generated_includes.py:43
# - tests/tooling/test_layout_generated_includes.py:58
# - tests/tooling/test_layout_generated_includes.py:24
# - tests/tooling/test_layout_generated_includes.py:38

# @unit GeneratedIncludeTests.test_exact_generated_output_is_accepted_and_same_ordered_result_kept_per_variant
# ## Summary
# 检查精确生成头在两个相同变体中保留两个有序结果。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 在独有合成build写入含NUL的人工字节串；两个相同根变体均应返回generated和同一解析Path。只比较kind及path序列，不独立断言variant_index、不编译头，也不验证任意别名均可接受。
# ## References
# - tests/tooling/test_layout_generated_includes.py:60
# - tests/tooling/test_layout_generated_includes.py:66
# - scripts/layout/generated_includes.py:92
# - scripts/layout/generated_includes.py:117

# @unit GeneratedIncludeTests.test_source_directory_shadow_wins_before_generated_root
# ## Summary
# 检查源目录中的仓内同名头优先于外部生成目录。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 在合成源目录和合成build均写入同名头；断言repository、源目录Path和精确相对路径。它证明此普通文件场景的优先级，不证明所有非普通文件、链接或并发替换的解析顺序。
# ## References
# - tests/tooling/test_layout_generated_includes.py:68
# - tests/tooling/test_layout_generated_includes.py:75
# - scripts/layout/generated_includes.py:86
# - scripts/layout/generated_includes.py:109

# @unit GeneratedIncludeTests.test_first_existing_ordered_repository_root_wins
# ## Summary
# 检查两个仓内include根都含普通头时按输入顺序选第一个。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 只在case内建立first/second根和两个不同人工文件，断言结果为repository且Path指向first。未交换根再跑，也不把当前结果推广为非普通文件或不安全候选都会被跳过。
# ## References
# - tests/tooling/test_layout_generated_includes.py:77
# - tests/tooling/test_layout_generated_includes.py:88
# - scripts/layout/generated_includes.py:86
# - scripts/layout/generated_includes.py:110

# @unit GeneratedIncludeTests.test_repository_parent_relative_include_resolves_without_external_escape
# ## Summary
# 检查父相对include仍落在仓内时能返回仓内目标。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 创建合成tests/parent_probe.h，再由源目录请求../parent_probe.h，断言repository、目标Path与tests/parent_probe.h相对路径。场景未实际逃出仓根，不覆盖任意父路径逃逸输入。
# ## References
# - tests/tooling/test_layout_generated_includes.py:90
# - tests/tooling/test_layout_generated_includes.py:98
# - scripts/layout/generated_includes.py:91
# - scripts/layout/generated_includes.py:109

# @unit GeneratedIncludeTests.test_generated_header_alias_is_not_treated_as_the_exact_generated_name
# ## Summary
# 检查折叠后指向生成头的路径别名仍不获得精确白名单资格。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 创建生成头后请求nested/../加固定头名，虽然词法绝对候选可折叠到同一文件，请求字符串不等于精确名，断言unsafe。只检查kind，不把这个case当全部Windows drive、绝对路径或NUL拒绝的证明。
# ## References
# - tests/tooling/test_layout_generated_includes.py:100
# - tests/tooling/test_layout_generated_includes.py:105
# - scripts/layout/generated_includes.py:92
# - scripts/layout/generated_includes.py:121

# @unit GeneratedIncludeTests.test_wrong_external_name_and_wrong_build_generated_root_are_unsafe
# ## Summary
# 分别检查错误生成文件名与另一构建根中的同名头被拒绝。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 在case内当前generated目录写other_generated.h，另在同一case的other-build写精确头名；两个查询只断言unsafe。other-build不是共享真实构建，所有新文件均归self.temp；未验证所有外部对象类型。
# ## References
# - tests/tooling/test_layout_generated_includes.py:106
# - tests/tooling/test_layout_generated_includes.py:115
# - scripts/layout/generated_includes.py:112
# - scripts/layout/generated_includes.py:122

# @unit GeneratedIncludeTests.test_missing_header_is_unresolved_and_variants_are_not_collapsed
# ## Summary
# 检查缺失、仓内/生成混合及部分缺失变体的结果保留。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 先断言两个缺失变体均unresolved，再分别建立生成头和仓内alternate头，断言generated/repository；删除本case的alternate_header后断言generated/unresolved。仅此删除触及本case文件，不清理共享树；本函数不调用apply_moves，因此混合结果在这里不是抛错或全局拒绝。
# ## References
# - tests/tooling/test_layout_generated_includes.py:117
# - tests/tooling/test_layout_generated_includes.py:132
# - scripts/layout/generated_includes.py:127
# - scripts/layout/generated_includes.py:180
# - scripts/layout/apply_moves.py:922
# - scripts/layout/apply_moves.py:927

# @unit GeneratedIncludeTests.test_acceptance_snapshot_and_vendor_paths_are_rejected
# ## Summary
# 检查三个具体深层禁止组件使仓内存在文件分类为unsafe。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 在case内循环创建tests/acceptance、tests/snapshots和tests/third_party下的微小人工头，逐项断言unsafe。它不向实际checkout的接受测试或第三方目录写入，也没有覆盖_FORBIDDEN_ROOTS的全部名称或大小写组合。
# ## References
# - tests/tooling/test_layout_generated_includes.py:134
# - tests/tooling/test_layout_generated_includes.py:146
# - scripts/layout/generated_includes.py:16
# - scripts/layout/generated_includes.py:65
# - scripts/layout/generated_includes.py:103

# @unit GeneratedIncludeTests.test_reparse_candidate_is_rejected_when_symlink_creation_is_supported
# ## Summary
# 在能创建文件symlink的平台检查该候选被判为unsafe。
# ## Parameters
# @param[in] self 持有本case独有合成目录的测试实例。
# ## Returns
# 断言完成后返回None；断言失败或文件系统异常交给unittest，当前方法余下语句不执行。成功setUp后仍由runner调用tearDown；没有失败码或资源返回。
# ## Constraints and lifecycle
# 目标和链接均位于独有case内；symlink_to抛OSError或NotImplementedError时skip，不能把skip算作reparse分支已执行。成功时仅断言unsafe，未创建或验证NTFS junction、祖先reparse或全部链接类型。清理由本case的TemporaryDirectory承担，不跟随该链接清理外部对象。
# ## References
# - tests/tooling/test_layout_generated_includes.py:148
# - tests/tooling/test_layout_generated_includes.py:157
# - scripts/layout/generated_includes.py:44
# - scripts/layout/generated_includes.py:95
# - scripts/layout/generated_includes.py:98
