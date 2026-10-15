---
related_code:
  - tests/parser/ssa_state_map_fixture.h
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/parser/test_ssa_state_map_liveness.c
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/cmake/ssa-cleanup-tests.cmake
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - tests/parser/ssa_state_map_fixture.h
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/code-review/batches/20261013-review-plan.md
  - "user: shared parser fixture contract review, 2026-10-15"
tests:
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/parser/test_ssa_state_map_liveness.c
  - tests/cmake/ssa-cleanup-tests.cmake
  - tests/cmake/ssa-tests.cmake
doc_type: code-review-batch
---

# SSA state-map 共享测试夹具契约复审

## 范围与单元计数

本批只审查 `tests/parser/ssa_state_map_fixture.h`，逐单元台账共 13 项：两个静态状态对象（`function`、`diagnostic`）、十个函数（`setUp`、`tearDown`、`value`、`blocks`、`edges`、`predecessors`、`emit`、`checkpoint`、`assert_only_root`、`build`），以及具有独立构建影响的 include 顺序块 `includes::unity_before_ucrt`。普通 include guard 既不是可调用宏也不承载独立契约，因此不单独计数。原有 173 行的原始字节和物理行号保持不变；Doxygen 契约全部追加在原 guard 之后。

该头由四个分别构建的 Unity 测试翻译单元包含；因此每个翻译单元拥有自己的 `static` 夹具状态，外部链接的 `setUp`、`tearDown` 则由各自可执行文件的 Unity runner 调用。不同测试只使用其场景需要的 builder 子集；`build` 提供“补前驱、core 验证、parser state-map 构建”的组合路径，不是每个测试都会调用的步骤。

## Unity 配置与失败控制流

cleanup 目标在 `tests/cmake/ssa-cleanup-tests.cmake:17` 定义 `UNITY_INCLUDE_CONFIG_H`；ownership、liveness、oracle 目标分别在 `tests/cmake/ssa-tests.cmake:267`、`:297`、`:465` 定义它。`tests/unity_config.h:4` 没有 Unity 覆盖宏。项目未为这些目标定义 `UNITY_TEST_PROTECT`、`UNITY_TEST_ABORT` 或 `UNITY_EXCLUDE_SETJMP_H` 覆盖。

当前框架实际路径为 `RUN_TEST` → `UnityDefaultTestRun` → 第一个 `TEST_PROTECT` 保护下的 `setUp` 与测试体 → `TEST_ASSERT_*` → `UNITY_TEST_ASSERT` → `UnityFail` → `UNITY_FAIL_AND_BAIL` → `TEST_ABORT`。当 `UNITY_EXCLUDE_SETJMP_H` 未定义时，Unity 源码把 `TEST_PROTECT` 映射为 `setjmp(Unity.AbortFrame)`，把 `TEST_ABORT` 映射为 `longjmp(Unity.AbortFrame, 1)`；这些目标确实走默认分支，因为项目配置为空，CMake 只要求包含配置头。断言失败时，`longjmp` 离开测试及 helper 栈帧；`UnityDefaultTestRun` 随后建立第二个受保护调用并运行 `tearDown`。这是非局部跳转，不是 C `return`，也不执行栈展开。

夹具 `tearDown` 只释放 `function`。`checkpoint` 返回借用自当前 `function.stateMap` 的 entry 指针；重建或替换 map、释放 function 后指针失效。`diagnostic` 是跨用例复用的 static 值，`setUp` 不清零它，所以测试应在提供对应结果的 API 调用后读取诊断。`blocks(0)` 是 no-op，不标记入口；要形成带入口的 CFG，调用数须大于 0，且该 helper 的 ID 断言要求从 `setUp` 的空 function 开始。`predecessors` 向 function 的前驱存储追加且不先清空；再次调用会重扫同一 successor 图、重复追加源块 ID 序列，并把各块 range 改指向本次新片段；旧片段仍留在数组前缀但不再由块 range 引用。它应只在 successor 图完整后调用一次。`assert_only_root` 在同步材料化过程中借用 function/map，并在正常路径释放局部 target；若成功材料化后的断言失败，Unity longjmp 会跳过局部 `Free`。当前用例没有触发这个分支，因此记录失败路径边界，不另登记 BUG。

## 已复现的 include 顺序缺陷

原头文件第 4 行先包含 Unity，第 7 行才包含 `<stdlib.h>`。在已观察到的 Windows SDK 10.0.26100.0、Clang 19.1.5、C11 配置中，Unity 的 C11 分支从 `unity_internals.h:82` 引入 `<stdnoreturn.h>`，并在 `:83` 将 `UNITY_NORETURN` 定义为 `noreturn`。该宏随后会展开到 UCRT 的 `__declspec(noreturn)` 声明内。修改前 R1 native 构建实际复现 5 条编译错误，首个失败翻译单元是 `tests/parser/test_ssa_state_map_ownership.c`；测试没有启动。这是本 fixture 唯一的 BUG 单元。

Root 的 R2 验证在同一个 E 根构建配置的全局 `CMAKE_C_FLAGS` 原值后追加 Unity 支持的 `/DUNITY_NORETURN=_Noreturn`。native build 退出码为 0，四个相关测试目标均实际编译；这个 flag 只绕开该构建中的头文件污染，未修正源码顺序，也未改变 Unity 默认 `setjmp`/`longjmp` 失败路径。R1 诊断：`E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/state-map-fixture-unity-noreturn-baseline-diagnosis-r1.json`，SHA256 `0b5fd89044123d11458c3dcab3afbf2e982bca436c099b9a183f9e572959f998`。

## 修改前基线与验证范围

R2 基线汇总为 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/parser-conditional-cleanup-preedit-native-r2-summary.json`，SHA256 `fd0b2a1e766535900c34ce813e811056680176d3c88f591f2dca895c27cb3c72`。cleanup runner 的 24 项中 23 项通过；`test_elaboration_owner_analysis_oom_is_transactional` 在 `tests/parser/test_ssa_conditional_cleanup.c:659` 失败，输出 `Expected TRUE Was FALSE`，直接测试进程与 CTest 结果一致。本批记录这一基线，不把失败归因到 fixture，也不声称 cleanup suite 全通过。其余三个相关 suite 合计 46/46 通过（ownership 15、liveness 9、oracle resume 22）。

producer 未构建或运行测试；Root 已完成下节注释后验证。本次仅补注释和台账，不改变测试或产品行为。逐单元决策及 evidence 锚点见 `docs/code-review/coverage/tests_parser_ssa_state_map_fixture.tsv`。

## 最终验收证据

最终头文件 SHA-256 `8c52fcfc80e3100280dbbed588fe133462022cd855975acd75778fad69dbcc8a`。Root 修正了 value 的 oracle 调用行、实际 `ZrCore_ExecIr_FunctionAddBlock` 标识、CMake 创建目标与宏配置位置，以及台账 helper/Unity 调用方完整路径。原 173 行程序字节未变；1,071 个非注释 token、标志和原行号一致，合并 proof 为 `cleanup-fixture-final-comment-equivalence-r2.json` SHA-256 `27d13d0dd54ec413d749168ff1f1361f7dbac3316548189375e17112563ed390`。本叶 13 项为 12 commented / 1 BUG / 0 TODO，无未审查项；默认 include 顺序 BUG 未修复。

同一实际 checkout、E 根 Debug 构建目录及 `/DUNITY_NORETURN=_Noreturn` 配置实际重编四个目标：build 0，9.093 s；cleanup direct 1、CTest 8，仍为 23 PASS / 1 FAIL，同一 line 659 OOM 断言失败；其余三个 CTest 目标 46/46 PASS、退出 0。逐例 path、line、name、outcome、message 与修改前 R2 完全相同。CTest output-on-failure 将 cleanup 的 24 例重复打印两遍，已验证两半相同并比较全部 48 条记录，未将它们计成 48 个测试。比较证据 `cleanup-fixture-final-native-r2-comparison.json` SHA-256 `b4f320d6bab3bc832629183d77cc551aefc4120f57ed4c0164ed6c786a384e13`；结果报告 `cleanup-fixture-final-native-r2-results.json` SHA-256 `f9f251dc181b4512517fdc9006cb2e57aa050e726e016fd0cf9c4a09b4601fce`。

3,216 项保守源码输入中，仅本批四个 EOF 注释文件哈希变化，另外 3,212 项相同；本轮 3,216 输入及 18 冻结依赖前后稳定，四个子进程全部等待退出。构建含既有 warning，不宣称无警告。两个精确叶表 `check-batch` 均退出 0；59 个 canonical key 跨其它叶表无重复，557 个 caller/evidence 锚点有效，并记录共享夹具在五个相关原程序文件的 811 处标识引用；这些出现数不是调用计数。证据 `cleanup-fixture-caller-unit-audit-r2.json` SHA-256 `c6c3e1b544c2e412a78432e68a12b8e32b24835746e1f3a7984fb76697253841`。这不是全仓 coverage 验收，也不授予真实资源析构、生产 parser pipeline、全部 OOM 分配或默认 Windows 构建通过信用。

产物复用与必要失败日志保留，本批释放 0 B；既有自动清理被策略拒绝，未重试，产物生命周期尚未关闭。
