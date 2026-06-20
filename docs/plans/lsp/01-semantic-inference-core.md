---
doc_type: plan-detail
related_code:
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_core/include/zr_vm_core/ownership.h
---

# 01 · 语义推断核心：表达式 / 数值 / 引用 / 所有权 / 控制流

目标：把今天「表达式级、零散」的推断，升级为「以控制流图（CFG）为骨架、数据流为引擎、语义事实为产物」的统一语义层，让编译器、LSP、debug、REPL 都消费同一份事实。

设计原则：
1. **一份事实，多处消费**。所有推断结果落到 `semantic_facts.h` 的六类事实里，新增 CFG/dataflow 也只是「事实的生产者」。
2. **可局部、可降级**。每个分析既能整模块跑，也能限定到单函数 / 单作用域跑（供 LSP）；任何一步失败都产出「未知」而不是崩溃或全盘放弃。
3. **不改运行时表面**。所有权运行时（`ownership.h`）不动，只增强编译期推断与诊断。

## 状态与产出记录

| 阶段 | 状态 | 完成时间 | 完成项目 | 验证 |
| --- | --- | --- | --- | --- |
| 阶段 1 / CFG | 进行中 | 2026-06-20 16:17 +08:00 | 新增 `cfg.h` / `type_inference/cfg.c`，建立 entry/statement/exit block graph，先覆盖顺序语句与 `return`/`throw`/`break`/`continue` 终结后的不可达传播；不可达 block 会向 `SZrSemanticReachabilityFact` 写入 cause 与 causeNode。新增 `tests/parser/test_cfg_reachability.c`，并把 `zr_vm_cfg_reachability_test` 注册到 parser semantic test 区和 language_pipeline 聚合清单。 | TDD RED 先因 CFG 类型/API 缺失编译失败；修复后 Windows MSVC `zr_vm_cfg_reachability_test` 2 PASS，`zr_vm_semantic_facts_test` 4 PASS，`zr_vm_expression_fact_emission_test` 28 PASS。最新 WSL 扫描仍有无关 stuck `find` 任务，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 16:33 +08:00 | `ZR_AST_IF_EXPRESSION` statement 进入 CFG：建立条件 statement block、then/else 分支体和 join block；bool literal 条件剪掉不可达分支，并为被剪枝分支内语句写入 `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`，causeNode 指向条件表达式。 | TDD RED 先失败为 `if (true) {A} else {B}` 的 else 语句无不可达 fact；修复后 Windows MSVC `zr_vm_cfg_reachability_test` 3 PASS，`zr_vm_semantic_facts_test` 4 PASS，`zr_vm_expression_fact_emission_test` 28 PASS。最新 WSL 扫描仍有无关 stuck `find` 与 `cc1` 任务，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 16:39 +08:00 | `ZR_AST_WHILE_LOOP` statement 进入 CFG：建立条件 statement block、循环体子图、back edge 和 join block；`while(false)` 循环体不连入 entry 可达路径，并为循环体内语句写入 `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE`，causeNode 指向 while 条件表达式。 | TDD RED 先失败为 `while(false) {A}` 的循环体语句无不可达 fact；修复后 Windows MSVC `zr_vm_cfg_reachability_test` 4 PASS，`zr_vm_semantic_facts_test` 4 PASS，`zr_vm_expression_fact_emission_test` 28 PASS。最新 WSL 扫描仍有无关 stuck `find` 与 `cc1` 任务，未声明 WSL GREEN。 |
| 阶段 1 / Dataflow | 进行中 | 2026-06-20 16:57 +08:00 | 新增 `dataflow.h/.c` 通用引擎骨架，支持 forward/backward 工作队列、per-block in/out state、entry 初始化、join/transfer 回调和 unreachable block 过滤；CFG 同步补上 `return`/`throw` 到 exit 的退出边，供后向分析从 exit 找到函数退出语句。 | TDD RED 先因 `dataflow.h` 缺失编译失败；补实现后后向测试先失败为 unreachable statement 被遍历，修复 entry-reachability 过滤后 Windows MSVC `zr_vm_dataflow_engine_test` 2 PASS。CFG 退出边测试先失败为 successorCount `0`，修复后 `zr_vm_cfg_reachability_test` 5 PASS；`zr_vm_semantic_facts_test` 4 PASS、`zr_vm_expression_fact_emission_test` 28 PASS。最新 WSL 扫描仍有无关 stuck `find` / AOT `cc1` 任务，未声明 WSL GREEN。 |
| 阶段 1 / Semantic Query | 进行中 | 2026-06-20 17:23 +08:00 | 新增 `semantic_query.h` / `semantic/semantic_query.c` 公共查询骨架，支持 module/node scope、`TypeAt` 类型复制、`ReferencesOf` 按 symbolId 收集 borrowed reference fact 指针、`DefinitionOf` 按 reference fact 的 symbolId/declarationRange 找 declaration、`FactsAt` 聚合六类 facts、`Diagnostics` 返回当前空列表骨架。 | TDD RED 先因 `semantic_query.h` 缺失编译失败；补实现后 `ReferencesOf` 复用输出数组的边界用例先失败为旧结果污染下一次查询，修复后 Windows MSVC `zr_vm_semantic_query_test` 7 PASS，覆盖最窄 expression type、事实聚合、definition 解析、references-of scope 过滤、复用输出清空、node scope 过滤和空 diagnostics；相邻 `zr_vm_reference_fact_emission_test` 5 PASS、`zr_vm_semantic_facts_test` 4 PASS。WSL focused retry 在 CMake regenerate/build 阶段 180 秒超时且未产出测试结果，已终止本次自启动 CMake/Ninja 进程；后续扫描仍有无关 AOT/core 编译/链接任务，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 17:46 +08:00 | `ZR_AST_FOR_LOOP` statement 进入 CFG：建立可选 init、condition statement block、循环体子图、可选 step、back edge 和 join block；`for(...; false; ...)` 循环体不连入 entry 可达路径，并为循环体内语句写入 `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE`，causeNode 指向 for 条件表达式。 | TDD RED 先失败为 `for(false) {A}` 的循环体语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 6 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 当前被无关 dirty-tree `compiler_quickening.c` 的 `compiler_quickening_opcode_is_false_branch` 重定义错误阻断在 `zr_vm_parser_shared` 编译阶段，未声明普通 target GREEN；WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 17:53 +08:00 | `ZR_AST_FOREACH_LOOP` statement 进入 CFG：建立 foreach entry/iteration block、循环体子图、back edge 和 join block；循环体会进入 CFG，因此体内 `break`/`continue`/`return`/`throw` 后续语句可以写入对应 reachability fact。 | TDD RED 先失败为 `foreach { break; A }` 的 `break` 后语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 7 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 仍被无关 dirty-tree `compiler_quickening.c` 的 `compiler_quickening_opcode_is_false_branch` 重定义错误阻断，未声明普通 target GREEN；WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 18:03 +08:00 | `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` statement 进入第一版 CFG：建立 try statement block、try body 子图和 join block；try body 会进入 CFG，因此体内 `return`/`throw`/`break`/`continue` 后续语句可以写入对应 reachability fact。此切片尚未建模 catch/finally 异常边。 | TDD RED 先失败为 `try { return; A }` 的 `return` 后语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 8 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 仍被无关 dirty-tree `compiler_quickening.c` 的 `compiler_quickening_opcode_is_false_branch` 重定义错误阻断，未声明普通 target GREEN；WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 18:09 +08:00 | `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` 的 catch clauses 进入第一版 CFG：从 try statement block 进入 catch body 子图，并在可 fallthrough 时接回 join；catch body 内 `return`/`throw`/`break`/`continue` 后续语句可以写入对应 reachability fact。此切片尚未建模精确异常匹配和 finally 执行边。 | TDD RED 先失败为 `try { A } catch { return; B }` 的 catch body 内 `return` 后语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 9 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 仍被无关 dirty-tree `compiler_quickening.c` 的 `compiler_quickening_opcode_is_false_branch` 重定义错误阻断，未声明普通 target GREEN；WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 18:14 +08:00 | `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` 的 finally block 进入第一版 CFG：从 try/catch join 进入 finally body 子图，并在可 fallthrough 时接回 final join；finally body 内 `return`/`throw`/`break`/`continue` 后续语句可以写入对应 reachability fact。此切片尚未建模所有 abrupt-completion 下 finally 必执行的精确边。 | TDD RED 先失败为 `try { A } finally { return; B }` 的 finally body 内 `return` 后语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 10 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 仍被无关 dirty-tree `compiler_quickening.c` 的 `compiler_quickening_opcode_is_false_branch` 重定义错误阻断，未声明普通 target GREEN；WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 18:20 +08:00 | `ZR_AST_SWITCH_EXPRESSION` statement 进入第一版 CFG：建立 selector block、case candidate chain、case body 子图和 join block；case body 内 `return`/`throw`/`break`/`continue` 后续语句可以写入对应 reachability fact。此切片尚未做 case 值匹配、default 剪枝或 union 穷尽性证明。 | TDD RED 先失败为 `switch { case { return; A } }` 的 case body 内 `return` 后语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 11 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 仍被无关 dirty-tree `compiler_quickening.c` 的 `compiler_quickening_opcode_is_false_branch` 重定义错误阻断，未声明普通 target GREEN；WSL 未补跑，未声明 WSL GREEN。`cfg.c` 已到 936 行，下一步继续 CFG 扩展前需要先拆分构图模块边界。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 18:26 +08:00 | 完成当前 CFG 构图文件拆分：`cfg.c` 保留共享 block helpers、straight-line/loop 构图、top-level build 和 reachability fact emission；新增私有 `cfg_internal.h` 与 `cfg_control_flow.c` 承载 switch 和 try/catch/finally 构图，避免继续向接近 1000 行的单文件堆叠控制结构逻辑。 | 拆分后手工聚焦 Windows MSVC CFG harness 11 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS；`cfg.c` 从 936 行降到 726 行，`cfg_control_flow.c` 为 211 行。普通 Windows MSVC CMake target 和 WSL 仍未声明 GREEN，原因同上。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 18:36 +08:00 | `ZR_AST_SWITCH_EXPRESSION` 补入 bool default 剪枝：当 selector 是 bool literal 且 cases 覆盖 `true` 与 `false` 两个 bool literal 时，default 分支不连入可达路径，但仍构建 default block 并写入 `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`，causeNode 指向 switch selector。 | TDD RED 先失败为 `switch(true) { case true; case false; default; }` 的 default 无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 12 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 与 WSL 仍未声明 GREEN，原因同上。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 19:14 +08:00 | `ZR_AST_WHILE_LOOP`、`ZR_AST_FOR_LOOP`、`ZR_AST_FOREACH_LOOP` 补入基础 loop-control target：body 内 `break` 会连接到当前 loop join；`continue` 在 while/foreach 中回到 loop header/iteration block，在传统 for 中连接到 step-entry join（无 step 时回 condition）。同时新增 `cfg_loops.c` 承载 while/for/foreach 构图，`cfg.c` 从再次超过 900 行回落到共享 helper、if/straight-line、top-level build 和 fact emission。 | TDD RED 依次失败为 while break、while continue、for break/continue、foreach break/continue 的 terminator successorCount 为 0；修复后手工聚焦 Windows MSVC CFG harness 18 PASS。拆分 loop 构图后同一 harness 仍 18 PASS；手工聚焦 Windows MSVC dataflow harness 2 PASS；普通 Windows MSVC CMake target 180 秒无结果并已停止自启动进程；WSL 旧 build 目录缺少新 target，独立 focused WSL build 300 秒超时且有无关 AOT 临时任务，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 19:31 +08:00 | `ZR_AST_SWITCH_EXPRESSION` 补入 bool case matching：当 selector 是 bool literal 且 case value 是相反 bool literal 时，该 case 不连入可达路径，但仍构建 case block/body 并写入 `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`，causeNode 指向 switch selector。 | TDD RED 先失败为 `switch(true) { case false { A } }` 的 case 无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 19 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 与 WSL 未声明 GREEN，沿用上一条记录的构建边界。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 19:50 +08:00 | `ZR_AST_SWITCH_EXPRESSION` 补入 integer case/default matching：当 selector 与 case value 都是 integer literal 时，非匹配 case 不连入可达路径，匹配 case 会消耗常量 selector，使后续 default 不连入可达路径；若匹配 case 以 `return` 等终结且无 default，switch 不再保留未命中 fallthrough。 | TDD RED 先失败为 integer 非匹配 case/default 无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 21 PASS。随后新增匹配 integer case `return` 后 switch 之后语句不可达覆盖，RED 失败为该语句无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 22 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 与 WSL 未声明 GREEN，沿用上一条记录的构建边界。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 20:05 +08:00 | `ZR_AST_SWITCH_EXPRESSION` 补入 string/char case/default matching：当 selector 与 case value 都是 string literal 或 char literal 时，非匹配 case 不连入可达路径，匹配 case 会消耗常量 selector，使后续 default / 未命中路径不连入可达路径。string literal 使用 `ZrCore_String_Equal` 比较，带解析错误或缺失 value 的 string/char literal 不参与常量匹配。 | TDD RED 先失败为 string 非匹配 case/default 无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 24 PASS。随后新增 char 非匹配 case/default 覆盖，RED 同样失败为无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 26 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。普通 Windows MSVC CMake target 与 WSL 未声明 GREEN，沿用上一条记录的构建边界。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 20:12 +08:00 | `ZR_AST_SWITCH_EXPRESSION` 补入 float case/default matching：当 selector 与 case value 都是 float literal 时，使用 literal 解析后的 `TZrDouble` 值做精确比较；非匹配 case 不连入可达路径，匹配 case 会消耗常量 selector，使后续 default / 未命中路径不连入可达路径。 | TDD RED 先失败为 float 非匹配 case/default 无不可达 fact；修复后手工聚焦 Windows MSVC CFG harness 28 PASS，手工聚焦 Windows MSVC dataflow harness 2 PASS。`test_cfg_reachability.c` 当前 957 行，仍是单一 CFG reachability 用例集；本切片不做人工拆分，下一次继续增加 switch 矩阵时应优先抽出 switch literal reachability 测试边界。普通 Windows MSVC CMake target 与 WSL 未声明 GREEN，沿用上一条记录的构建边界。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 20:39 +08:00 | `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` 补入 return/throw abrupt-completion 到 finally entry 的可达性边：新增 `tests/parser/test_cfg_finally_abrupt.c`，验证 `try { return; } finally { A }` 的 finally body 不再被标为不可达，同时 `try { return; } finally { A } B` 仍保持 `B` 不可达。实现上只扫描 try/catch 受保护范围内的 `return`/`throw` terminator；break/continue 穿过 finally 的目标重写继续留待后续。同步确认 switch-local break 不是当前编译器已支持语义：`SZrBreakContinueStatement` 只有 `isBreak/expr`，编译器 break/continue 只读取 `loopLabelStack`，switch 编译路径没有 switch break label。 | TDD RED 先失败为 finally body 仍有不可达 fact（`Expected NULL`）；修复后手工聚焦 Windows MSVC finally-abrupt harness 2 PASS，既有手工聚焦 Windows MSVC CFG reachability harness 28 PASS、dataflow harness 2 PASS；正式 CMake target `zr_vm_cfg_finally_abrupt_test` 在 `build\agent-msvc-tests` Debug 构建并运行通过 2 PASS。WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 20:51 +08:00 | `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` 补入 break/continue abrupt-completion 到 finally entry 的可达性边：`tests/parser/test_cfg_finally_abrupt.c` 新增 `try { break; } finally { A }` 与 `try { continue; } finally { A }`，验证 finally body 不再被标为不可达；实现上将 try/catch 受保护范围扫描扩展到 `ZR_AST_BREAK_CONTINUE_STATEMENT` terminator。此切片只补可达性边，break/continue 经 finally 后再跳往原 loop target 的精确目标重写仍未实现。 | TDD RED 先后失败为 break/continue 形态的 finally body 仍有不可达 fact（`Expected NULL`）；修复后 Windows MSVC CMake targets 在 `build\agent-msvc-tests` Debug 通过：`zr_vm_cfg_finally_abrupt_test` 4 PASS、`zr_vm_cfg_reachability_test` 28 PASS、`zr_vm_dataflow_engine_test` 2 PASS。WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 21:14 +08:00 | `ZR_AST_IF_EXPRESSION`、`ZR_AST_WHILE_LOOP`、`ZR_AST_FOR_LOOP` 的常量布尔条件折叠扩展到非字面量 unary/logical 形态：新增 `tests/parser/test_cfg_constant_conditions.c` 与 `zr_vm_cfg_constant_conditions_test`，覆盖 `if (!false)`、`while (!true)`、`if (true && false)`、`while (false || false)`、`if (false && flag)`、`if (true || flag)`；`cfg_node_bool_constant` 现在是 `cfg.c` / `cfg_loops.c` 共享 helper，统一支持 bool literal、unary `!`、两侧均可折叠时的 logical `&&`/`||`，以及 `false && unknown` / `true || unknown` 这类短路决定项。 | TDD RED 先让 unary 用例失败为 `Expected Non-NULL`；补 unary `!` 后 Windows MSVC CMake focused targets 通过 constant-conditions 2 PASS、CFG reachability 28 PASS、finally-abrupt 4 PASS、dataflow 2 PASS。随后 logical `&&`/`||` 双常量用例先失败为 `Expected Non-NULL`；补 logical folding 后 constant-conditions 4 PASS。最后短路右侧未知用例先失败为 `Expected Non-NULL`；补 decisive-operand folding 后 `build\agent-msvc-tests` Debug 通过：`zr_vm_cfg_constant_conditions_test` 6 PASS、`zr_vm_cfg_reachability_test` 28 PASS、`zr_vm_cfg_finally_abrupt_test` 4 PASS、`zr_vm_dataflow_engine_test` 2 PASS。WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 21:21 +08:00 | `ZR_AST_SWITCH_EXPRESSION` 的常量匹配复用非字面量布尔折叠：新增 `tests/parser/test_cfg_switch_constants.c` 与 `zr_vm_cfg_switch_constants_test`，覆盖 `switch(!false) case true`、`switch(true) case !false`、`switch(false && flag) case true`；`cfg_control_flow.c` 的 switch constant extraction 现在先通过共享 `cfg_node_bool_constant` 提取 folded bool，再处理 integer/string/char/float literal。 | TDD RED 先让新增 switch-constants 3 个用例均失败为 `Expected Non-NULL`；补复用 bool folder 后 Windows MSVC CMake targets 在 `build\agent-msvc-tests` Debug 通过：`zr_vm_cfg_switch_constants_test` 3 PASS、`zr_vm_cfg_constant_conditions_test` 6 PASS、`zr_vm_cfg_reachability_test` 28 PASS、`zr_vm_cfg_finally_abrupt_test` 4 PASS、`zr_vm_dataflow_engine_test` 2 PASS。WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 21:28 +08:00 | `cfg_node_bool_constant` 补入整数 literal binary comparison 常量折叠：`ZR_AST_IF_EXPRESSION`、`ZR_AST_WHILE_LOOP`、`ZR_AST_FOR_LOOP` 的条件现在可识别 `==`、`!=`、`<`、`>`、`<=`、`>=` 两侧都是 integer literal 的恒真/恒假结果；新增 `if (1 == 2) { A } else { B }` 与 `while (1 < 0) { A }` 覆盖，分别验证 then/body 不再连入可达路径并写入 `CONSTANT_BRANCH` / `CONDITION_FALSE` reachability fact。 | TDD RED 先让 integer comparison 用例失败为 `Expected Non-NULL`；补整数 literal comparison folding 后 Windows MSVC CMake targets 在 `build\agent-msvc-tests` Debug 通过：`zr_vm_cfg_constant_conditions_test` 8 PASS、`zr_vm_cfg_switch_constants_test` 3 PASS、`zr_vm_cfg_reachability_test` 28 PASS、`zr_vm_cfg_finally_abrupt_test` 4 PASS、`zr_vm_dataflow_engine_test` 2 PASS。WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG | 进行中 | 2026-06-20 21:36 +08:00 | `cfg_node_bool_constant` 补入 string/char/float literal binary comparison 常量折叠：string 支持 `==`/`!=`，char 与 float 支持 `==`、`!=`、`<`、`>`、`<=`、`>=`；新增 `if ("red" == "blue") { A } else { B }`、`while ('a' != 'a') { A }`、`if (1.5 >= 2.5) { A } else { B }` 覆盖，验证 then/body 不再连入可达路径并写入对应 reachability fact。string 使用 `ZrCore_String_Equal`，string/char literal 有 parse error 或缺失 value 时不参与折叠。 | TDD RED 先让 string/char/float 三个新增用例失败为 `Expected Non-NULL`；补 literal comparison helpers 后 Windows MSVC CMake targets 在 `build\agent-msvc-tests` Debug 通过：`zr_vm_cfg_constant_conditions_test` 11 PASS、`zr_vm_cfg_switch_constants_test` 3 PASS、`zr_vm_cfg_reachability_test` 28 PASS、`zr_vm_cfg_finally_abrupt_test` 4 PASS、`zr_vm_dataflow_engine_test` 2 PASS。WSL 未补跑，未声明 WSL GREEN。 |
| 阶段 1 / CFG + Dataflow + Query | 待完成 | - | 下一步补齐 break/continue 经 finally 后再到原 loop target 的精确目标重写、精确 catch 异常匹配/过滤边、更广泛常量条件折叠（数值区间、符号值等）、switch/union 穷尽分支、definite assignment / reaching defs 等具体 dataflow 分析，以及编译器/LSP 诊断消费路径。switch-local break 已审计为当前语言/编译器不支持：除非后续先在 parser/compiler 中引入 switch break label 语义，否则 CFG 不单独实现。`semantic_query.h` 目前只是稳定查询面骨架，后续需接局部重算、真实结构化诊断来源和更完整的 overload/member ranking。 | - |

---

## 1. 控制流图（CFG）—— 所有流分析的地基

### 1.1 现状

不可达只在表达式级（短路、常量分支），`SZrSemanticReachabilityFact` 的语句级原因（AFTER_RETURN/BREAK/CONTINUE）虽有枚举但缺真正的 CFG 支撑。

### 1.2 设计

在 `zr_vm_parser/src/zr_vm_parser/type_inference/` 下新增 `cfg.c` / `cfg.h`，对每个函数体构建基本块图：

```
SZrCfgBlock {
    TZrUInt32   id;
    SZrAstNode  **statements;   // 块内语句
    TZrUInt32   *successors;    // 后继块 id（条件分支两条边）
    TZrUInt32   *predecessors;  // 前驱块 id
    TZrUInt32    predecessorCount;
    EZrCfgEdgeKind edgeKind;    // NORMAL / TRUE_BRANCH / FALSE_BRANCH / LOOP_BACK / EXCEPTION
    TZrBool      visited;
}
SZrCfg { SZrCfgBlock *blocks; entry; exit; }
```

构建时机：在 `compile_statement.c` 完成函数体 AST 后、生成字节码前，跑一遍 `ZrParser_Cfg_Build(funcBody)`。语句到块的映射：顺序语句进同一块；`if/while/for/foreach/switch/try` 在 `compile_statement_flow.c` 已识别的控制点处切块并连边；`return/throw/break/continue` 终结当前块并连到对应目标（exit / loop header / loop exit / catch）。

- 不可达检测：从 entry 做可达性传播，`predecessorCount == 0` 且非 entry 的块 -> 整块不可达。对块内首语句发 `ZR_SEMANTIC_REACHABILITY_*` 事实 + 一条 warning 诊断（带「因为上游 return/throw/穷尽分支」的 cause）。
- 借鉴 `lua/cpython/Python/flowgraph.c` 的 `remove_unreachable`：标记访问 + 重算前驱，O(V+E)。

### 1.3 验收

- `return; x = 1;` 报 `x = 1` 不可达，cause 指向 return。
- `if (true) {A} else {B}` 报 B 不可达，cause = 常量条件。
- switch/union 穷尽后 default 不可达可识别（与 04 union 计划对齐）。

---

## 2. 数据流框架 —— 在 CFG 上跑定点迭代

新增 `dataflow.c` / `dataflow.h`，提供通用前向/后向数据流引擎（半格 + 工作队列定点迭代），借鉴 `lua/rust/compiler/rustc_mir_dataflow/src/framework/mod.rs`：

```
SZrDataflowAnalysis {
    EZrDataflowDirection direction;          // FORWARD / BACKWARD
    void (*initEntry)(SZrDataflowState*);    // 入口初值
    void (*transferStatement)(ctx, stmt, state); // 语句迁移函数
    TZrBool (*join)(dst, src);               // 半格 join，返回是否变化
}
```

引擎 `ZrParser_Dataflow_Run(cfg, analysis)`：工作队列驱动，块状态变化则把后继（前向）重新入队，直到收敛。下面三个分析都建立在它之上。

---

## 3. Definite assignment（确定赋值）+ 引用推断

### 3.1 目标

- 报「使用未初始化变量」。
- 强化引用关系：每个 `SZrSemanticReferenceFact` 都能回答「这个 READ 来自哪个 DECLARATION / 上一次 WRITE」。

### 3.2 设计

前向数据流，domain = 每个局部 symbol 的状态格 `{UNINIT, MAYBE_INIT, INIT}`：
- DECLARATION 无初值 -> UNINIT；有初值 / WRITE -> INIT。
- join：两路都 INIT -> INIT，否则 MAYBE_INIT。
- READ 一个 UNINIT/MAYBE_INIT 的 symbol -> 诊断（cause 指向声明，suggestion 给「在使用前赋值」并带 fix-it 草案，见 02 篇）。

引用推断增强：在迁移时维护「reaching definitions」，把每个 READ 的 `SZrSemanticReferenceFact` 关联到产生它的 WRITE/DECLARATION 的 symbol/location，供 LSP go-to-definition / find-references / rename 复用（03 篇）。

借鉴：Rust `MaybeUninitializedPlaces` / `EverInitializedPlaces`（`lua/rust/compiler/rustc_borrowck/src/dataflow.rs`）。

---

## 4. 所有权推断 —— 从「泛型约束检查」升级为「流式所有权追踪」

### 4.1 现状

所有权只在泛型约束层检查；`%unique/%shared/%weak/%borrow/%loan` 的运行时控制块 `SZrOwnershipControl`（`ownership.h`）成熟，但编译期没有 move/borrow 的流追踪。`TZrLifetimeRegionId` 已定义未用足。

### 4.2 设计

前向数据流，domain = 每个所有权值的状态：`{OWNED, MOVED, BORROWED(n), RELEASED}`，并按 `TZrLifetimeRegionId` 标注借用的有效区间。

迁移规则（产出 `SZrSemanticOwnershipFact`）：
- `%unique` 值被赋给别处 / 传入按值参数 -> MOVED；之后再 READ -> 报「use after move」，cause 指向 move 点。
- `%borrow` / `%loan` 产生借用，记录 region；离开 region 后使用借用 -> 报「borrow escapes / outlives source」。已有诊断 `BuildBorrowEscape` / `BuildLoanEscape` / `BuildOwnerToPlainEscape`（`diagnostic_builder.h:225-233`）可直接挂接。
- `%weak` upgrade 在可能已释放处 -> `BuildWeakUpgrade` 提示。
- 与 `%unique` 字段、`using` 确定性析构（using 计划）对齐：region 退出即视为 RELEASED。

借鉴：Rust borrowck 的 `explain_borrow` 给「为什么冲突 + 如何修」分层解释（`lua/rust/compiler/rustc_borrowck/src/diagnostics/explain_borrow.rs`）。

---

## 5. 数值推断（区间 + 溢出）

### 5.1 现状

只有字面量范围（`SZrSemanticNumericFact`）。

### 5.2 设计

前向数据流，domain = 每个数值 symbol 的区间 `[lo, hi]`（带「是否可能溢出目标类型」标记）：
- 字面量 -> 单点区间；`a + b` -> 区间相加（带目标整型上下界饱和检查）。
- 比较/分支细化：`if (x < 10)` 在 true 分支把 x 区间收窄到 `[lo, 9]`。
- 越界索引、明显溢出、恒真/恒假比较（喂给 Logical fact，驱动常量分支不可达）-> 诊断。

范围适度即可（区间格而非完整 SMT），目标是覆盖「数组常量越界」「无符号下溢」「恒真条件」这类高频问题。借鉴 javac 在 AST 节点上挂精确区间/位置的做法（`lua/jdk/.../javac/util/JCDiagnostic.java`）。

---

## 6. 逻辑语义推断

在数值/引用基础上产出 `SZrSemanticLogicalFact`：
- 恒真/恒假条件（`if (x != x)`、`if (1)`）-> 提示 + 驱动 CFG 常量分支不可达。
- 冗余条件 / 不可能同时成立的 `&&`（区间不相交）。
- 短路右侧恒不求值（已有 SHORT_CIRCUIT，纳入统一框架）。

---

## 7. 统一语义查询 API（被所有前端复用）

新增 `semantic_query.h`，把上述事实包成稳定查询面（供 LSP 03 / debug REPL 04）：

```
ZrParser_SemanticQuery_TypeAt(ctx, pos) -> InferredType
ZrParser_SemanticQuery_ReferencesOf(ctx, symbolId) -> Reference[]
ZrParser_SemanticQuery_DefinitionOf(ctx, pos) -> location
ZrParser_SemanticQuery_FactsAt(ctx, pos) -> {numeric, logical, ownership, reachability}
ZrParser_SemanticQuery_Diagnostics(ctx, scope) -> StructuredDiagnostic[]
```

要求：每个查询都接受一个「分析范围」（整模块 / 单函数 / 单作用域），并在内部对应「局部跑分析」（03 篇增量），失败返回空/Unknown 不抛错。

---

## 8. 落地顺序（本篇内部）

1. CFG 构建 + 不可达（先把语句级不可达做对，收益最直接）。
2. 通用 dataflow 引擎。
3. definite assignment + reaching defs（顺带强化引用，LSP 立即受益）。
4. 所有权流追踪（接已有 escape 诊断）。
5. 数值区间 + 逻辑事实。
6. `semantic_query.h` 收口。

每步都要：补 `tests/parser/` 单测 + `tests/language_server/` 对应查询测试，且保证「分析失败 -> 产出 Unknown，不崩」。
