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
