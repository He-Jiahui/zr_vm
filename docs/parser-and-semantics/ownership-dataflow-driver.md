---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_owner_sets.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_observations.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_regions.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics_owner_set_cases.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/code-review/coverage/zr_vm_parser_dataflow_ownership_driver.tsv
tests:
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_compiler_return_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/acceptance/comment-review-20261001.md
doc_type: module-detail
---

# 所有权 dataflow 驱动

## 职责与调用方式

`ZrParser_SemanticFacts_ResolveControlFlowOwnership(context, root)` 为 compiler 与 LSP 的语义 query 汇总 CFG 所有权事实。调用前须完成 reference-fact 生产；同步解析期间，引用事实索引与顺序、符号身份、root AST 必须稳定。调用者持有 context 和 AST，分析将区域绑定及 move/release/违规事实写回 context。bool 表示分析及发布流程是否完成；true 仍可能有语义违规，false 也可能留下先前的区域改写或部分事实。

compiler 从 `compiler.c` 的 late-check 阶段经 `compiler_semantic_query_diagnostics.c` 进入，在 definite-assignment/reaching-definition 分析之后调用本驱动。成功解析出的 Unique ERROR 可设置 compiler `hasError`；其他 ownership facts 由语义 query 投射。LSP 的 `semantic_analyzer_query_diagnostics.c` 直接调用驱动，再物化其余 query diagnostics。头文件声明另由 Context 模块审查；本模块台账记录 C 定义与实现契约。

## 状态域、顺序与生命周期

每个符号槽保留可达路径上可能的 OWNED/MOVED/BORROWED/RELEASED 状态、调用级 owner-set ID 和借用的最早 AST cause。definite-assignment 另行负责未初始化检查，入口 OWNED 并不代表局部变量已经初始化。join 合并状态、owner 来源和 cause，只要任何部分变化就通知 solver 继续传播。

transfer 先处理语句读取时旧值是否仍可用，再建立写入后的值生命周期。两阶段次序避免赋值重置掩盖同一语句中的无效读取；新写入须清除旧值的 move/release 见证。alias 的具体 owner 只能来自已识别的绑定，UNKNOWN 与已知 EMPTY 必须区分。

符号映射、owner-set pool 和五个 observations 数组仅属于一次 resolver 调用；每个所选脚本/可调用/block 边界另持有临时 CFG/result。通用 solver 同步调用 init/join/transfer，`userData` 借用解析器栈上 analysis；回调不得保留 solver 缓冲地址。观测按稳定的 reference-fact 索引跨重复访问累计，最后投射到 context；context facts 及其借用 AST cause 的有效期须覆盖后续 query。释放临时缓冲不撤销已写入 context 的事实。

## 已证实的静态失败链

`BUG:` 对合法条件重绑定的 loan alias，OwnerSetSingleton/Union 的单次原始列表分配失败可返回 UNKNOWN。其余分析步骤成功时，驱动无法给 UNKNOWN 提供 owner-release cause，后续 read 缺少违规观测，`loan_escape` 诊断因此遗漏。支持的合法形状和期望见 `test_ownership_diagnostics_owner_set_cases.h` 的 Loaned Alias Owner Set 夹具；正常基线该用例通过。这是合法生产路径上的静态失败链，尚未运行 allocator 故障注入。

`BUG:` 合法 Unique value-pass consume 后读取，在五个观测缓冲区任一原始分配单次失败、其他前后步骤成功时，驱动在启动 CFG 前清理部分缓冲并返回 false。compiler diagnostics helper 随即返回，未扫描违规或设置 `hasError`；`compiler.c` 忽略这个返回，仍可组装并返回函数。没有失败时，该 Unique ERROR 会走 compiler 硬拒绝。原始分配失败链和合法夹具见 observations 台账及 `test_ownership_diagnostics.c`。缺陷是调用方失败处理；本次注释未改变行为，也未把它表述为已运行故障注入。

## 待核实契约

- `TODO:` LSP 忽略驱动 false 后继续物化查询；从现有调用入口注入观测分配失败，核实“分析成功但缺少/部分 ownership diagnostics”是否符合 best-effort 契约。此疑问独立于已证实的 compiler 失败链。
- `TODO:` lambda AST 有 block body，当前递归 dispatch 没有独立 lambda case。沿 parser 构造、reference-fact 范围归属及闭包 CFG 边界核实其读取是否被并入外围语句，确定是否需要独立 CFG；未据缺少 case 单独断言运行时缺陷。

## 审查与验证范围

专用台账覆盖26单元：3类型、14普通函数、3回调、1递归原型和5契约块。先枚举全部可检索直接调用及3个注册点，再跟踪通用 solver 的同步间接 dispatch 和 compiler/LSP 消费。两项无需注释判断也保留具体理由。反向台账仅更新该源文件的数字位置，原有单元、决策及其他证据保持不变；旧私有草稿保留原始哈希，由独立迁移清单供后续消费者使用。

修改前后使用 GCC、Clang 的四个既有 solver/compiler/LSP 可执行文件，以及 MSVC shared-only CLI/hello_world 检查。具体终端结果、用例身份比较及并发源码变化限制记录在当日 acceptance；这里不把基线失败称为全仓绿色，也不把源码哈希/模块提交当作跨仓库捕获封存。
