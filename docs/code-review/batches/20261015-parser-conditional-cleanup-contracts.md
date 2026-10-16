---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_state_maps.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - docs/core-runtime/execution-state-maps.md
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_state_map_liveness.c
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/parser/ssa_state_map_fixture.h
  - tests/parser/ssa_owner_fault_allocator.c
  - tests/cmake/ssa-cleanup-tests.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_state_maps.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - tests/parser/test_ssa_conditional_cleanup.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_state_map_liveness.c
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/cmake/ssa-cleanup-tests.cmake
  - tests/acceptance/ssa-conditional-cleanup.md
doc_type: code-review-batch
---

# Parser conditional cleanup contract review — 2026-10-15

## Scope and source identity

This batch covers 46 canonical units: two production C functions, the cleanup declaration in the shared state-map header, and 43 units from tests/parser/test_ssa_conditional_cleanup.c (two structs and 41 functions: 16 helpers/callbacks, 24 Unity test cases, and main). The header's BuildStateMaps declaration, shared fixture globals and helpers, Core verifier/materialization units, CMake setup, and other test modules are referenced as evidence but are not counted here. No behavior, signature, test assertion, build setting, or non-comment program token was intentionally changed. Doxygen was appended at EOF to preserve existing executable source line coordinates. The public C implementation relies on the declaration's complete contract and is recorded no-comment; simple scalar wrappers, the DROP counter, and main are also no-comment with per-unit rationale in the leaf ledger.

The authorized pre-edit SHA-256 values were:

| Path | Before SHA-256 | After SHA-256 |
| --- | --- | --- |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c | 2876b616741f5f8831fa7c33c8e526d1f2653c2e0698e6f915ff3d8c48b91fb5 | c2d04e393cd107d1cdfbeae5c88e1ddca6afb5696287435d00b15b6894af2003 |
| zr_vm_parser/include/zr_vm_parser/exec_ir_state_maps.h | 0f86c5fb2cd57d2edabf04217b753eeab51d3215035fff6496bd1e348aed45a0 | 575062cc0116969d183b0c6311fc6b4ff07d15488680188ac1e77377a98af43a |
| tests/parser/test_ssa_conditional_cleanup.c | 955688deb305cffd147c7339e12cd6e14228ca2e932dabfe4b40527e1d17b9c9 | 7b30c852246cbbca6ac066ba1efc804b49529a78d53129c87cd3dd3aa557372b |

The formal ledger is docs/code-review/coverage/zr_vm_parser_conditional_cleanup.tsv and has 46 data rows. It records the three cleanup-lowering call sites in this repository (the test helper at line 101 and direct sealed/OOM tests at lines 633 and 651); no production caller was found. tests/cmake/ssa-cleanup-tests.cmake:19 registers the single ssa_conditional_cleanup CTest target. Its main runs 24 Unity cases; these are not 24 separate CTest processes. The historical acceptance document is context only and is not runtime evidence for this batch.

## Production contract and failure behavior

ZrParser_ExecIr_ElaborateCleanupDrops requires a non-null, unsealed function and first verifies its current IR with STRUCTURE|EFFECT before cloning. It rewrites ordinary DROP instructions only in cleanup blocks, checks the cloned graph and owner-state obligations, increments generation only if an opcode changed, and rebuilds maps before replacing the entire source function with the candidate. A successful no-op still clone-replaces the object and rebuilds maps; a failed path frees candidate analysis and pools without publishing them, leaving the source object untouched. An input's existing non-null state map must validate against the current IR during that pre-clone source check. The pass does not clear an old map before this check, so callers mutating IR while retaining a stale map can be rejected before candidate elaboration. On success, borrowed pointers into the prior function pools and state map are invalidated. These details are documented on the public declaration; the shared BuildStateMaps declaration remains outside this batch.

The static evidence for the stale-map preflight is ZrCore_ExecIr_VerifyFunction at zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909-910, which validates a non-null state map, and ZrCore_ExecIr_ValidateStateMap in zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556 and following. Its owner/liveness checks reject a map whose conditional cleanup summary no longer matches ordinary DROP semantics. By contrast, ZrParser_ExecIr_BuildStateMaps explicitly clears the map on its verification view before constructing a replacement (zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:372-376). The public comment states only the verified current-map precondition; it does not treat this test setup issue as a production API defect.

## Test scope and known failing case

The 24 Unity cases cover diamond cleanup across state-map phases, owner witnesses and root selection, resume transactionality and allocation-fault injection, invoke exception behavior, repeated loop cleanup, guarded-DROP validation, ordinary double-drop execution, clone pool independence, sealed-function rejection, shared-owner behavior, and owner-state buffer alias rejection. The callback/userData buffers are borrowed only for synchronous oracle calls. Tests which hold result or materialized-state allocations explicitly free them on normal assertion flow; Unity early assertion exit can bypass local OracleResultFree or MaterializedStateFree. Fixture tearDown frees the global function only and must not be described as cleaning every local allocation.

The forged-initialized-witness case builds diamond(ZR_FALSE) and uses a false initial condition to select the undefined arm. It only proves rejection when INITIALIZED is forged without payload, then restores UNINITIALIZED and observes zero drops. It neither checks acceptance on the true arm with a valid payload nor asserts a particular diagnostic code.

The owner-analysis OOM test is a confirmed test-setup BUG, not a production behavior fix in this batch. diamond(ZR_FALSE) has already elaborated the graph and built its guarded-DROP map; the test then changes only the opcode back to ordinary DROP and leaves that old map attached. ElaborateCleanupDrops pre-verifies the source, and Core verifies a present state map before cloning. The earliest static mismatch is the poll's BEFORE_EFFECT map: ordinary DROP becomes a semantic use that makes the owner semantically live, while the old guarded-DROP map cannot express that conditional owner mask. StateMapOwnerAt can return OWNER_STATE_COUNT and owners_valid rejects the map before candidate clone or pass OwnerAnalysis once an injected allocation ordinal is not hit by preverification. R2's observed failure is TEST_ASSERT_TRUE(injected) at tests/parser/test_ssa_conditional_cleanup.c:659 (Expected TRUE Was FALSE); the test log does not record a diagnostic code, exact entry, or ordinal. This rejection path is static source-path inference, not a claimed runtime diagnostic observation. The batch records this one BUG under the existing test unit and does not edit its setup or count a new coverage unit. It makes no exact successful-allocation-count claim and grants no clone, candidate-liveness, candidate-owner-analysis, or state-map OOM sweep credit.

## Pre-edit native baseline and remaining verification

The authorized native R2 baseline used the existing Debug build directory and CMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /DUNITY_NORETURN=_Noreturn. Configure and build succeeded (build exit 0, 278.281 s). The direct cleanup executable exited 1 and cleanup CTest exited 8 because of the single line-659 case: 23/24 cases passed. Related CTest suites for oracle resume, state-map liveness, and state-map ownership passed 3/3 targets, representing 46/46 Unity cases. Evidence is parser-conditional-cleanup-preedit-native-r2-summary.json (SHA-256 fd0b2a1e766535900c34ce813e811056680176d3c88f591f2dca895c27cb3c72) and parser-conditional-cleanup-preedit-native-r2-results.json (SHA-256 6b448627e8b20fed397f9209a3179be1e4c32dcc9bc775a049f5715fae14733b).

Root 已完成注释后验证，实际结果与下节证据一致。R2 的 Unity override 仅用于同配置前后对照，没有修改源码或掩盖默认构建失败。

## 最终验收证据

四源共 8,200 个非注释 token 的 kind、spelling、StartOfLine、LeadingSpace 完全相同，原程序 token 物理行号不变；本叶三源为 7,129 个。证据 `cleanup-fixture-final-comment-equivalence-r2.json` SHA-256 `27d13d0dd54ec413d749168ff1f1361f7dbac3316548189375e17112563ed390`。独审发现的 Unity 非局部跳转、MOVE 原值与目的值、diamond 间接 lowering 及前三个 checkpoint 的 owner-state 断言范围已纠正；没有改动断言或程序语句。diagnostic 存储须与 function 及其池、map 独立，事务保证以前述有效、独占输入为前提。共享 H 的 BuildStateMaps 声明仍待下一批，inventory 保持 pending 并记录已完成的 cleanup 接口。

同一实际 checkout、E 根 Debug 构建目录及 `/DUNITY_NORETURN=_Noreturn` 配置实际重编四个目标：build 0，9.093 s；cleanup direct 1、CTest 8，仍为 23 PASS / 1 FAIL，同一 line 659 OOM 断言失败；其余三个 CTest 目标 46/46 PASS、退出 0。逐例 path、line、name、outcome、message 与修改前 R2 完全相同。CTest output-on-failure 将 cleanup 的 24 例重复打印两遍，已验证两半相同并比较全部 48 条记录，未将它们计成 48 个测试。比较证据 `cleanup-fixture-final-native-r2-comparison.json` SHA-256 `b4f320d6bab3bc832629183d77cc551aefc4120f57ed4c0164ed6c786a384e13`；结果报告 `cleanup-fixture-final-native-r2-results.json` SHA-256 `f9f251dc181b4512517fdc9006cb2e57aa050e726e016fd0cf9c4a09b4601fce`。

3,216 项保守源码输入中，仅本批四个 EOF 注释文件哈希变化，另外 3,212 项相同；本轮 3,216 输入及 18 冻结依赖前后稳定，四个子进程全部等待退出。构建含既有 warning，不宣称无警告。两个精确叶表 `check-batch` 均退出 0；59 个 canonical key 跨其它叶表无重复，557 个 caller/evidence 锚点有效，并记录共享夹具在五个相关原程序文件的 811 处标识引用；这些出现数不是调用计数。证据 `cleanup-fixture-caller-unit-audit-r2.json` SHA-256 `c6c3e1b544c2e412a78432e68a12b8e32b24835746e1f3a7984fb76697253841`。这不是全仓 coverage 验收，也不授予真实资源析构、生产 parser pipeline、全部 OOM 分配或默认 Windows 构建通过信用。

产物复用与必要失败日志保留，本批释放 0 B；既有自动清理被策略拒绝，未重试，产物生命周期尚未关闭。

最终 R2 在独审确认 NULL target 是无输出检查、空 function 指的是 NULL 参数后重跑：四源严格等价仍为 8,200 token；只改变这两处说明及相应台账，实际重编与逐例结果再次同基线。R1 中间验证证据保留，没有将它冒称为本最终源码的重编结果。

提交前空白检查发现 cleanup C 的 EOF 文档后有一个多余空行（退出 2）。仅删除最后一个 LF 字节后，四源 R3 严格复核仍为 8,200 token，kind、spelling、StartOfLine、LeadingSpace 及原程序物理行号全部不变；其余三源哈希未变。最终源码严格证据 `cleanup-fixture-final-comment-equivalence-r3.json` SHA-256 `18a95f83c74b326ad3a2e45c10cfea6e9f7e7db02b7bf1a5b02832901859921d`；单字节修正和失败门禁记录分别为 `cleanup-fixture-eof-repair-r3.json`、`parser-conditional-cleanup-stage-r1-failure.json`。本次不重复运行原生测试：实际构建、逐例基线对比仍明确使用 R2，R3 只为文档结束后空白提供最终衔接证据。

## 2026-10-16：既有 helper 文档标签与借用方向补全（零新增单元）

本次仅修订 `tests/parser/test_ssa_conditional_cleanup.c` 的七个既有文档行：为 `terminal`、`diamond`、`stop_at`、`request_at`、`invoke`、`loop_call` 的原有返回说明补上六个 `@return`；将 `request_at` 的 `target` 标为 `@param[in]`，明确该 helper 只保存调用方对象的地址，后续 `ZrCore_ExecIr_MaterializeState` 才把所指对象作为可写输出借用。原有返回值、对象所有权、回调和失败路径契约保持成立。完整调用核查仍为 terminal 14 处、diamond 14 处、stop_at 10 处、request_at 2 处，以及 invoke:427、loop_call:527 两个函数指针入口；六个定义均为本测试翻译单元内的 static helper。原 46-key 叶表和原程序行号保持原样，本次不增加 canonical key、测试、缺陷标签或审查计数，历史已提交台账累计仍为 1,769，不能据此声称全仓覆盖。

最终源码 SHA-256 为 `f10954b3b24e83f15e317970c9907543c3b18bf87114a92e21692d7d44d074c7`。Clang 原始 token 检查实际通过：6,503 个非注释 token 的 kind、spelling、StartOfLine、LeadingSpace 和原始行列坐标完全一致；七次精确替换可重建完整源码，LF 和 1,540 行保持原样。证据 `conditional-cleanup-returns-final-comment-equivalence-r1.json` SHA-256 `2fffeb46085c7cce928ecf612b81b464538cd4ac7cf2c991d0968c40c7238ad3`，完整字节与调用台账复核为 `conditional-cleanup-returns-final-caller-unit-audit-r1.json` SHA-256 `e8c1381e31efc5233b2c80ea340956eea9ad33c6ae809216a348445783898f24`。

修改前、修改后均在同一实际 checkout、同一 E 根 Debug 目录和既有 `/DUNITY_NORETURN=_Noreturn` 配置构建 `zr_vm_ssa_conditional_cleanup_test`，两次 build 均退出 1：`lld-link: error: undefined symbol: ZrCore_Exception_Throw`，引用来自 canonical_type_index.c 的原生数组 helper 路径。修改后实际重编本测试 C 文件，随后链接失败。两次均未运行 CTest 或旧可执行文件，没有本轮运行时 PASS、逐例等价或成功构建信用；上述 line 659 的历史失败仍只是历史记录。修改后构建区间及实际结束时 3,225 项保守输入和 21 项冻结依赖稳定；前后基线之间另有原生 math 测试、LSP hygiene 测试头及 LSP declaration binding 三处其他会话改动，逐项哈希保存在比较报告，不声称完整依赖隔离。前基线 `conditional-returns-preedit-native-r2-summary.json` SHA-256 `88d427526bfd642ea2d56df2082b19552e4d660fae46be3a247df95fd0818229`；后结果摘要 `conditional-cleanup-returns-final-native-r1-summary.json` SHA-256 `db51bedc077e471bde91bd28dbf90d2d56555081e84cbf84bf41f7c74ad6bf02`；比较报告 `conditional-cleanup-returns-final-native-r1-comparison.json` SHA-256 `3e0519e4b46848b4f7a1b3b98f5c814be5afd898607f2b03b0e19d2d4f8d622d`。当前原生验证受该链接失败限制，依赖修复后仍需新鲜构建与测试；本轮只记录，不加 stub、绕过链接或修改程序行为。

本批复用既有构建、缓存及审查产物，新增释放空间为 0 B；保留必要失败证据。此前自动清理被策略拒绝，未重试，产物生命周期仍开放。
