---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_state_maps.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - docs/core-runtime/execution-state-maps.md
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/ssa_state_map_verifier_cases.inc
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_state_maps.h
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/ssa_state_map_verifier_cases.inc
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/ssa_state_map_verifier_cases.inc
  - tests/cmake/ssa-tests.cmake
doc_type: code-review-batch
---

# Parser ExecIR State Map Builder 契约审查 — 2026-10-15

## 范围与源码身份

本批新增 21 个审查单元：Parser 实现中的 13 个函数和一个私有结果枚举、公开头文件中的 `ZrParser_ExecIr_BuildStateMaps` 声明，以及五个主测试函数和 include 片段中的一个测试函数。主测试文件其余 39 个定义仍待审；include 片段另两个测试也仍待审。本批只在四个授权源码路径的 EOF 追加 Doxygen，保持所有原程序行、token、断言和执行顺序不变。`ZrCore_ExecIr_StateMapLivenessBuild`、owner analysis、验证器、map free/materialization 等 Core 单元由其各自模块审查，不计入本批。

| 路径 | 编辑前 SHA-256 | 当前 SHA-256 |
| --- | --- | --- |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c` | `c0ff46cd4769be0f5bb045413bac831001cc61c2046243f31da9e407813146e4` | `FD19FDCFC5CAC0DD18E83400B230D2D62F03FF4F01FA550981EDE0B49E2AAF5F` |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_state_maps.h` | `575062cc0116969d183b0c6311fc6b4ff07d15488680188ac1e77377a98af43a` | `84BCF3B5774E0206CADC09B8E1F7C3FBCF28F9DB1A3B1A2040983BCD2FE267E8` |
| `tests/parser/test_ssa_state_maps.c` | `2f35b6f9d3f9ea2b8868d668aeda8bd2109f16f0e5d37030f7af0968368fedaf` | `8EE23C541F806F0FB07F4FE396803BF362FC2E6FC32FACE5486B5656A0F006F1` |
| `tests/parser/ssa_state_map_verifier_cases.inc` | `476a390bcc5bd30a9638f55684c7a937ea224d4f3671fd258712c2a0e654dc65` | `8A2E61573FB7F6D0639EF9EFD40F3894371EED4C448F9D41858D585B97C0D77A` |
| `docs/code-review/coverage/zr_vm_parser_state_map_builder.tsv` | 新文件 | `896C3283F8A9626C7AA2EFCBE59C784436FE2514D82083196F15D8B98349CDCA` |
| `docs/code-review/batches/20261015-parser-state-map-builder-contracts.md` | 新文件 | 最终字节由独立暂存审查和串行提交计划固定，本文不自含哈希 |

编辑租约为 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/state-map-builder-source-lease-r1.json`，SHA-256 `e44a70d962014b06a512444f33c178c9e6a2c5702752d24b2bf5771fb4bfa537`，固定 HEAD `2b36a00dd449acf7fc07e63513f333fdabc5960d`。机器可读逐单元结论在 `coverage/zr_vm_parser_state_map_builder.tsv`，共 21 行；简单容量、枚举和查找 helper 按其局部表达登记为 `no-comment`，其余复杂契约在声明或 EOF Doxygen 说明。本批没有新增 BUG/TODO，也没有修改行为或测试。

## 构建与调用路径

`ZrParser_ExecIr_BuildStateMaps` 由三处生产代码直接调用：`exec_ir_build.c:500` 的 build wrapper、`exec_ir_pass_manager.c:586` 的 pass pipeline 和 `exec_ir_cleanup_drops.c:75` 的 cleanup lowering；测试与共享 fixture 另有调用。build wrapper 由同文件 `build_impl` 的 `:881` 调用，`build_impl` 另从 `:591` 和 `:893` 进入；pass manager 调用位于 `ZrParser_ExecIr_RunPassPipeline`（`:486`）。全仓文本检索还覆盖 runtime-object、AOTIR、cleanup、DCE/liveness、deopt、oracle、state-map ownership/liveness、pass-manager 与 verifier case。实现中没有函数指针或回调注册路径。共享 fixture 的调用属于 fixture 审查范围，本批只记录关联入口，不复制它的审查单元。

`tests/cmake/ssa-tests.cmake:397` 创建 `zr_vm_ssa_state_maps_test`，`:422` 注册 CTest `ssa_state_maps`。主文件在 `:1351` 包含 verifier case 片段，并于 `:1357` 注册本批所选的 include 测试。主文件所选五个 Unity case 分别由 `:1389`–`:1393` 注册。修改前授权原生基线实际构建 exit 0（7.641 秒），direct 与 CTest 均执行 42 个 Unity case、42/42 通过；该证据来自 Root 固定的 pre-edit summary `state-map-builder-preedit-native-r1-summary.json`（SHA-256 `91b8d3d48560f614dd08879efdad9774d891d2fb8bf115e421ecc286f14906cf`）。注释后实际验证已由 Root 完成，见末节精确证据。

## 接口与候选生命周期

接口要求 function 未 sealed，Core 结构及池可供同步验证和分析，调用期间由调用方独占访问；实现没有锁。可选 diagnostic 在入口先被清零，所以它不得与 function、旧 map 或其池重叠。旧 map 可在内容因 IR 改变而语义过期时重建，但其对象和各个 pool 必须仍可安全、分别释放，且不得与 function 本体或任何 function pool 别名。验证通过浅拷贝清空旧 map 的方式只用于校验当前 IR；它不放宽输入对象所有权约束。

构造过程先执行 Core verifier、liveness 与 owner analysis，再在局部候选中追加可达边界的各个 phase。候选继承当前 function token、signature hash 和 contract generation；该过程只发布新的 `stateMap`，不修改 function 其他 pools 或递增 generation。诊断错误、分析失败、检查点失败或最终 map 对象分配失败都会释放临时候选并保留原 map。检查点内部按 live pool、root pool、owner-state pool、entry 的顺序追加；中间 append 失败可以让尚未发布的 candidate 留有部分数据，唯一生产调用者在失败分支统一释放整个 candidate，不存在 helper 级局部回滚保证。

成功提交时旧 map 及其 entries、value/root/owner-state pools 被分别释放，旧 map 的借用指针随即失效；function 的其余池保持原样。调用方必须重新查询 map，并在本次同步调用期间禁止并发读取或替换。Root 已维护的模块文档 `docs/core-runtime/execution-state-maps.md` 另记录已验证的旧 map 前置约束；本批只链接该契约，不编辑该文件。

`zr_state_map_add_checkpoint` 只把 live 且满足对应 owner mask 的 managed values 纳入 roots；borrowed value 在其仍 live 且边界含 SUSPEND 时拒绝。deopt resume 复用先要求 deoptId 仅出现一次，再要求该记录 sourceId 匹配且 resumeId 非零；重复 resume 使用会被拒绝。边界测试具体断言及未覆盖范围均按测试函数记录，没有从代表 fixture 推断全部 phase、全部 owner states、OOM 路径或完整 deopt payload 行为。

## 定向测试范围

五个主文件测试覆盖：单一 deopt 状态在 BEFORE/AFTER entry 中复用 resumeId；重复 deopt identity 被拒绝；一个 ALLOC 和 DEBUG_POLL fixture 的 entry 元数据、`liveValues`/`rootValues` 数量各为 1、`valuePool` 中的 live ID 为 1，且 dead borrowed ID 2 不在 `valuePool`；ownership/nullability 两种无效枚举；以及无效 block range 触发 Core verifier 失败时旧 map 指针和 entry count 保持不变。该 liveness 用例没有检查 `rootPool` 中具体的 value ID。include case 修改 DEBUG_POLL sourceId 后检查旧内容语义过期时的失败保留、修复 block range 后按新 source 查询 entry，并由完整 Core Verify 接受 map。该 include 测试不比较成功前后的 map 指针，因此不以 pointer inequality 作为信用。

这些测试的断言使用 Unity；失败时 longjmp 退出当前 case，teardown 与 main 继续运行。若断言早于函数末尾，局部 `FreeFunction` 调用会被跳过，文档没有把这种路径描述为已完成局部清理。selected cases 是现有 42-case 目标中的六个，不是新增测试，也不扩大到目标内其余 36 个 case。

## 验证状态

修改前原生构建和 `ssa_state_maps` 的 42/42 结果保留为 baseline。注释后复用同一实际 checkout 和 E 根构建目录、相同 Clang-cl Debug 与 Unity noreturn override：目标构建 exit 0（10.062 秒），CTest exit 0（1.063 秒），direct exit 0（0.265 秒）。CTest 与 direct 各 42/42 通过，全部用例的绝对源码路径、行号、名称、结果及消息与 baseline 逐项一致。42 例运行信用不代表本批为目标内其他待审函数完成台账。

四个源码文件只在 EOF 追加 Doxygen，原始文件字节前缀和原程序物理行保持不变；Clang raw lexer 消费完整输出，12,265 个有效 token 的 kind、拼写、StartOfLine 与 LeadingSpace 标记全部相同，原 token 行号逐项相同。Core 验证和 lower-level 实现未修改。3,216 个保守编译输入中只有本批四源 raw hash 变化，其余 3,212 项与 baseline 相同；当前 23 个冻结源及全部输入在实际运行窗口稳定。三个原生子进程均已等待退出，没有源码副本或新增测试。

本叶 21 行 `check-batch` 退出 0，当前全部 coverage 叶中精确 key 无重复；Root 全仓检索 19 个函数名并核对三处生产调用、测试 include/Unity 注册及私有 helper 路径，保留 176 处原始文本命中以区分定义、注释和真实调用。未见函数指针注册；公开 ABI 不据此排除仓外消费者。本批无新 BUG/TODO。

证据根为 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`：


- 严格 token/行号：`state-map-builder-final-comment-equivalence-r1.json`，SHA-256 `a7c165ffc5b8313a0c060f66994c5a2dbd19ecea7d9fbe0b6b095c69b9472981`。
- 实际原生 summary：`state-map-builder-final-native-r1-summary.json`，SHA-256 `e3d76b5fd5df0bc9911cdb2b0e34961b3f98bd532d56919634263a8a72ca4f9d`。
- 前后 case 比较：`state-map-builder-final-native-r1-comparison.json`，SHA-256 `910128663fab995d4e8ed380b78fb7f82a316c00722cdcfe0a03147cb2cdf51d`。
- 逐单元/调用审计：`state-map-builder-final-caller-unit-audit-r1.json`，SHA-256 `ffd4b09db5099d18e610c503124b8bfd00e0aeaae73035e36fb5518941d5886c`。
- 台账检查：`state-map-builder-final-check-batch-r1.json`，SHA-256 `2a942ecdd5e4ed1e6525798d769033ab832ff4874b0546105b364fb9c75a913f`。

该配置的 Unity noreturn override 沿用已登记验证前提；默认配置先前的 SDK include 顺序失败未在此任务修复，不把当前通过外推为默认配置或全测试集通过。仅实际断言覆盖的状态/资源边界获信用。
