---
doc_type: plan-detail
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_position.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - tests/CMakeLists.txt
---

# 05 · 实施蓝图

把 01-04 的设计落成有依赖顺序、可独立验收的阶段。每阶段都「能单独合入、能单独测」。

---

## 1. 模块依赖图

```
semantic_query.h (01 §7)  <-- 所有前端的统一入口
        ^
        |  produces facts
  +-----+-----------------------------+
  | CFG(01 §1) -> dataflow(01 §2)      |
  |   -> definite-assign/refs(01 §3)   |
  |   -> ownership(01 §4)              |
  |   -> numeric/logical(01 §5,6)      |
  +-----------------------------------+
        |                 |                |
        v                 v                v
 diagnostics(02)     LSP(03)          debug+REPL(04)
```

- 02/03/04 都消费 01 的 `semantic_query.h`；02 的诊断扩展是 03/04 错误展示的前提。
- 03 的「UTF-16 行列修复」**不依赖** 01，可最先独立交付。

---

## 2. 分阶段计划

### 阶段 0：独立速赢（无依赖，最先做）
- **03 §1**：UTF-16 位置编解码层，替换 `lsp_interface_position.c` 全部转换；扩 `test_lsp_position_mapping.c`。
- **03 §2** 部分：越界防护（content[index] 边界检查）。
- 交付物：行列 bug 修复 + 不崩防护。可单独合入。

### 阶段 1：语义地基
- **01 §1** CFG 构建 + 语句级不可达。
- **01 §2** 通用 dataflow 引擎。
- **01 §7** `semantic_query.h` 骨架（先包现有推断，后续填新事实）。
- 交付物：不可达代码诊断 + 统一查询面雏形。

### 阶段 2：流分析铺开
- **01 §3** definite assignment + reaching defs（强化引用）。
- **01 §4** 所有权流追踪（接已有 escape 诊断）。
- **01 §5/§6** 数值区间 + 逻辑事实。
- **02 §1/§2** 诊断结构扩展（related/fixes/descriptorId）+ 关联位置。
- 交付物：未初始化/use-after-move/越界/恒真条件诊断，带「哪里-为什么-怎么改」。

### 阶段 3：前端体验
- **03 §3** 局部语义推断 + 声明级增量解析。
- **03 §4** hover/completion/semanticTokens 质量。
- **02 §3/§4** 类型不匹配具体化 + 诊断码注册表 + 本地化槽。
- 交付物：大文件流畅、诊断可一键修复、IDE 富 hover。

### 阶段 4：调试与交互
- **04 §1** debug 求值统一到 `semantic_query.h` + 条件断点设置期校验。
- **04 §2** REPL 常驻全局态（先过渡、再增量声明注入）。
- **04 §1.5** DAP 适配层（中期）。
- 交付物：调试器/REPL 与编辑器能力一致、REPL 真交互。

---

## 3. 新增/改动文件清单（预估）

新增：
- `zr_vm_parser/src/zr_vm_parser/type_inference/cfg.{c,h}`
- `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.{c,h}`
- `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assign.c`
- `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c`
- `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_numeric.c`
- `zr_vm_parser/include/zr_vm_parser/semantic_query.h` + 实现
- `zr_vm_parser/include/zr_vm_parser/diagnostic_registry.h` + 实现
- `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_position_codec.{c,h}`

改动：
- `diagnostic_builder.h/.c`：扩展结构 + 新 builder。
- `compiler_diagnostics.c`：替换模板化类型错误。
- `lsp_interface_position.c`：转调 codec。
- `lsp_interface.c` / `semantic_analyzer*.c`：诊断序列化补 related/fix；查询接 `semantic_query.h`。
- `incremental_parser.c`：声明级增量。
- `debug_eval.c` / `debug_semantic_facts.c`：接统一推断。
- `repl.c`：常驻全局态。
- `tests/CMakeLists.txt` 等：注册新测试。

---

## 4. 测试策略

- **parser 单测**（`tests/parser/`）：CFG 不可达、definite-assign、所有权流、数值区间，每个分析独立用例 + 「分析失败产出 Unknown 不崩」用例。
- **LSP 测试**（`tests/language_server/`）：position 往返/边界（扩 `test_lsp_position_mapping.c`）、局部推断、诊断 related/fix 字段、hover/completion 内容。
- **debug 测试**（`tests/debug/`）：统一求值后的类型/诊断、条件断点设置期校验、watchpoint。
- **REPL 测试**：跨提交引用变量与运行期值、失败回滚。
- **fuzz**：position codec + 增量编辑，断言不崩 + 往返幂等。
- **回归**：现有验收（`tests/acceptance/`）与 reference fixtures 全绿。

---

## 5. 验收标准（DoD）

1. 含中文/emoji 的 `.zr` 文件，hover/definition/diagnostic 位置精确（UTF-16）。
2. `return` 后代码、常量分支、穷尽 union 后 default 报不可达，带 cause。
3. 未初始化使用、use-after-move、借用逃逸均报错，带 related 位置 + 修复建议。
4. 类型不匹配诊断给出「期望 vs 实得 + 原因 + 可行 cast」。
5. 任意畸形/超大输入下 LSP 不崩；推断失败降级为 Unknown。
6. 大文件编辑单函数时延迟显著低于全量。
7. debug 条件断点/表达式求值的类型与诊断和编辑器一致。
8. REPL 中先定义变量、后续表达式可引用其运行期值。

---

## 6. 风险与缓解

| 风险 | 缓解 |
| --- | --- |
| REPL 常驻全局态需编译器支持「追加声明」，改动深 | 先做「运行期值快照重放」过渡，验证收益后再上增量注入 |
| dataflow 在病态输入上发散/慢 | 迭代次数与节点数上限 + 超限降级部分结果 |
| 诊断结构扩展破坏现有序列化 | 字段只增不改，旧路径默认空 related/fix |
| `location.h` column 单位若为字节，反向映射也需改 | 阶段 0 先核对 column 语义并统一到 codec |
| DAP/AOT 调试范围大 | 列为中后期，先保证 interp/binary 路径 |

---

## 状态与产出记录

| 阶段 | 状态 | 完成时间 | 完成项目 | 验证 |
| --- | --- | --- | --- | --- |
| 阶段 2 / Numeric While Multi-Target Assignment Zero-Or-More Range Join | 已完成 | 2026-06-23 03:08 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补 loop 内跨目标赋值合流：unknown-condition while 的 focused helper 现在可顺序 replay 多个 simple numeric assignments，并把每个目标的 loop-body replay range 与进入循环前 range 合流。`low=5, high=20; while (flag) { low = 1; high = low + 10; } high + low;` 返回 `12..25`。该切片不声明 self-dependent RHS、真正 loop fixed point、for/foreach、unsupported statements/assignments 或 general CFG-wide dataflow。 | RED：新增 LSP assignment query 先返回 `min=25 max=25`；parser 对应 RED 覆盖在共享 checkout 中曾被无关 AOT 编译错误打断。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 11 PASS、LSP numeric branch assignment query 11 PASS、LSP numeric branch refinement query 16 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric While Assignment Zero-Or-More Range Join | 已完成 | 2026-06-23 02:42 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补跨块赋值合流：parser `ZR_AST_WHILE_LOOP` 和 LSP while typecheck 现在共用 `type_inference_loop_assignment_join.c`，对 unknown-condition while 中唯一 simple integer assignment 做进入前 range 与循环体 RHS range 的 zero-or-more join。`narrowed = 5` 后 `while (flag) { narrowed = 10; } narrowed + 1` 返回 `6..11`。该切片不声明 loop fixed point、self-dependent RHS、for/foreach、多目标循环或 general CFG-wide dataflow。 | RED：WSL gcc parser 先失败为 `Expected TRUE Was FALSE`，LSP assignment query 返回 `min=6 max=6`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 10 PASS、LSP numeric branch assignment query 10 PASS、LSP numeric branch refinement query 16 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP logical query 3 PASS、CFG reachability 29 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Nested Branch Assignment Dataflow | 已完成 | 2026-06-23 01:00 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补跨块赋值合流：simple branch assignment replay 现在能处理 branch block 内 bounded nested simple `if/else`，先在临时 branch scope 中 join 嵌套分支写入，再把当前 plan target binding 存回外层 branch result。`narrowed = 1/2` 与 `narrowed = 10/20` 的嵌套 if/else 后，`narrowed + 1` 在 parser 与 LSP local semantic query 中返回 `2..21`。general CFG-wide numeric dataflow、loop/fixed-point、任意嵌套控制流、unsupported assignments 与任意 symbolic range-set/hole-set 仍不声明完成。 | RED：WSL gcc parser 先失败为 `Expected 2 Was 1`，LSP assignment query 返回 `min=1 max=1`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 9 PASS、LSP numeric branch assignment query 9 PASS、LSP numeric branch refinement query 16 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Segment-Preserving Branch Assignment Join | 已完成 | 2026-06-23 00:42 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补跨块赋值合流：simple branch assignment join 现在会保留两侧分支写入产生的不连续 segment payload，`narrowed = 1/10` 后的 `narrowed + 1` 在 parser 与 LSP local semantic query 中返回 envelope `2..11` 与 segments `2..2` / `11..11`。LSP assignment query 覆盖已从 branch refinement 文件拆到独立 assignment 目标。general CFG-wide numeric dataflow、loop/fixed-point、跨嵌套控制流与任意 symbolic range-set/hole-set 仍不声明完成。 | RED：WSL gcc parser 先失败为 `Expected 2 Was 0`，LSP assignment query 返回 `exprSegments=0` / `numeric segments=0`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 8 PASS、LSP numeric branch assignment query 8 PASS、LSP numeric branch refinement query 16 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Multi-Target Branch Assignment Dataflow | 已完成 | 2026-06-23 00:09 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补跨块赋值合流：simple if/else 分支块现在允许多个 simple numeric assignment target，并在临时 branch scope 中按顺序重放 target 写入，让同一分支内的后续 target RHS 可读取前置 target range。`if (flag) { low = 1; high = low + 1; } else { low = 10; high = low + 10; } high + low;` 现在返回 `3..30`。general CFG-wide numeric dataflow、loop/fixed-point、跨嵌套控制流与 segment-preserving assignment join 仍不声明完成。 | RED：WSL gcc parser 新 multi-target 用例失败为 `Expected 3 Was 0`，LSP query 只返回 `0..0`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 7 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 23 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric RHS-Dependent Sequential Branch Assignment Range Join | 已完成 | 2026-06-22 23:44 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补最小跨块赋值合流：simple if/else 分支块允许同一 target 的顺序 simple assignments，并在临时 branch scope 中重放前置写入，让 RHS 依赖前置写的后一条 assignment 可读取分支内 range。`if (flag) { narrowed = 1; narrowed = narrowed + 1; } else { narrowed = 10; narrowed = narrowed + 1; } narrowed + 1;` 现在返回 `3..12`。多 target/多变量 branch dataflow、general CFG-wide numeric dataflow、loop/fixed-point 与 segment-preserving assignment join 仍不声明完成。 | RED：WSL gcc parser 新 RHS-dependent 用例失败为 `Expected 3 Was 1`，LSP query 只返回 `1..1`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 6 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 22 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Nonterminal Multi-Statement Branch Assignment Range Join | 已完成 | 2026-06-22 23:21 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补最小跨块赋值合流：simple if/else 分支块允许后置普通表达式，只要块内只有一个同名数值 simple assignment，就把两侧 RHS range 合并。`if (flag) { narrowed = 1; flag; } else { narrowed = 10; flag; } narrowed + 1;` 现在返回 `2..11`。多次写、RHS 依赖前置写、general CFG-wide numeric dataflow、loop/fixed-point 与 segment-preserving assignment join 仍不声明完成。 | RED：WSL gcc parser 新 nonterminal multi-statement 用例失败为 `Expected 2 Was 1`，LSP query 只返回 `1..1`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 5 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 21 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Multi-Statement Terminal Branch Assignment Range Join | 已完成 | 2026-06-22 23:04 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补最小跨块赋值合流：simple if/else 分支块允许前置普通表达式，只要块末尾是同名数值 simple assignment，就把两侧 RHS range 合并。`if (flag) { flag; narrowed = 1; } else { flag; narrowed = 10; } narrowed + 1;` 现在返回 `2..11`。assignment 不在末尾、多次写、RHS 依赖前置写、general CFG-wide numeric dataflow、loop/fixed-point 与 segment-preserving assignment join 仍不声明完成。 | RED：WSL gcc parser 新 multi-statement 用例失败为 `Expected 2 Was 1`，LSP query 只返回 `1..1`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 4 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 20 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Else-Only Branch Assignment Range Join | 已完成 | 2026-06-22 22:44 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补最小跨块赋值合流：explicit else-only simple assignment 会把 else RHS range 与 true path 保留的进入前 binding range 合并。`var narrowed: int = 5; if (flag) { flag; } else { narrowed = 10; } return narrowed + 1;` 现在返回 `6..11`。general CFG-wide numeric dataflow、multi-statement、loop/fixed-point 与 segment-preserving assignment join 仍不声明完成。 | RED：WSL gcc parser 新 else-only 用例失败为 `Expected 11 Was 6`，LSP query 只返回 `6..6`。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 3 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 19 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric One-Sided Branch Assignment Range Join | 已完成 | 2026-06-22 22:26 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向继续补最小跨块赋值合流：simple if true-branch 单侧赋值会把 then RHS range 与 false path 保留的进入前 binding range 合并；显式类型注解的数值变量声明现在也保留 initializer 单点范围，供 parser/LSP 后续查询使用。`var narrowed: int = 5; if (flag) { narrowed = 10; } return narrowed + 1;` 现在返回 `6..11`。general CFG-wide numeric dataflow、multi-statement、else-only、loop/fixed-point 与 segment-preserving assignment join 仍不声明完成。 | RED：WSL gcc parser 新 if/then 用例失败为 `Expected TRUE Was FALSE`，LSP query 无 range。GREEN：WSL gcc、WSL clang、Windows MSVC focused/adjacent 目标通过：numeric branch assignment 2 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 18 PASS、LSP logical query 3 PASS、LSP reachability query 9 PASS、LSP semantic query diagnostics 11 PASS；不声明全仓库 GREEN。 |
| 阶段 2 / Definite Assignment Declaration Related Information | 已完成 | 2026-06-22 21:36 +08:00 | 按阶段 2 的 definite-assignment 与 02 §1/§2 诊断 related 方向补一层最小闭环：结构化诊断拥有 `relatedInformation` 数组，未初始化/可能未初始化 read 会把 declarationRange 作为 `Variable declaration is here` 关联位置，LSP 诊断转换保留并发布该信息；LSP semantic query diagnostics 在 AST 可用时先跑 CFG-backed definite assignment，让 branch join 场景能发布 `possibly_uninitialized_read`。此切片只覆盖 DA declaration relatedInformation；fix-it、descriptorId、诊断码注册表、所有权 related 与 compiler 外部/二进制诊断通道仍不声明完成。 | RED：parser/LSP relatedInformation 断言先失败，LSP branch fixture 初始无 query diagnostic；GREEN：WSL gcc、WSL clang、Windows MSVC focused 目标通过：semantic query 14 PASS、LSP semantic query diagnostics 11 PASS、LSP local semantic query 19 PASS、numeric branch assignment 1 PASS、LSP numeric branch semantic query 17 PASS。compiler semantic query diagnostics 本轮因 typed metadata export 阶段 invalid `exportedVar->name` segfault 未作为 GREEN 证据；不声明全仓库 GREEN。 |
| 阶段 2 / Numeric Cross-Block Branch Assignment Range Join | 已完成 | 2026-06-22 21:03 +08:00 | 按阶段 2 的“数值区间 + 逻辑事实”方向补一层最小跨块赋值合流：parser 和 LSP 共用 `type_inference_branch_assignment_join.c`，仅对 simple if/else 中两侧同名数值赋值做 range envelope join，并把 join 后 binding 暴露给后续表达式推断。新增 parser/LSP focused 覆盖，`narrowed = 1/10` 后的 `narrowed + 1` 变为 `2..11`。general CFG-wide numeric dataflow、loop fixed point、多语句分支和 one-sided branch 仍不声明完成。 | RED：WSL gcc parser 新用例先失败为 `Expected 2 Was 1`，LSP 新用例没有 range fact。GREEN：WSL gcc、WSL clang、Windows MSVC focused 目标通过：numeric branch assignment 1 PASS、numeric range 10 PASS、numeric branch refinement 15 PASS、numeric branch segment sets 3 PASS、semantic query 14 PASS、compiler semantic query diagnostics 16 PASS、LSP local semantic query 19 PASS、LSP numeric semantic query 4 PASS、LSP numeric branch semantic query 17 PASS、LSP semantic query diagnostics 10 PASS；不声明全仓库 GREEN。 |
