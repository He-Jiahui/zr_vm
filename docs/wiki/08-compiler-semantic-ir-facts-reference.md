---
related_code:
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/place.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_canonical_type_place_cfg.c
plan_sources:
  - user: 2026-09-10 持续扩充 ZrVm Wiki 的语言实现与 C API 说明
  - docs/parser-and-semantics/index.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/aot/04-semir-and-c-backend.md
doc_type: implementation-reference
---

# Semantic IR、CFG 与数据流事实参考

**状态：`current`。** 本页描述编译器在 AST 之后、字节码和 AOT lowering 之前使用的
语义中间层。它不是 ZR 源语言可直接书写的语法，也不是稳定的第三方二进制 ABI；它是
当前 parser/compiler 公共头文件暴露、并由编译器和测试共同约束的实现契约。

配合阅读：[编译器流水线](08-compiler-pipeline.md)、[语义与执行模型](02-language/semantics-implementation.md)、
[类型与所有权](02-language/types-ownership.md)、[Parser/Compiler 深度 C API](05-interop/parser-compiler-c-api.md)。

## 1. 为什么需要这一层

AST 回答“文本如何组成”；执行层需要回答“一个值从哪里来、是否可读、何时销毁、分支合流后
是否仍初始化、借用是否存活”。这些问题不能可靠地由 token 或格式化后的类型名重新推断。

Semantic IR（下文简称 SemIR）把已绑定的程序表示为四组彼此关联的内容：

```text
canonical TypeId / SymbolId
        +
Place graph ----------- CFG blocks and edges
        |                     |
        +---- SemIR instructions ----+---- source ranges
                                      |
                                      +---- flow facts / diagnostics
```

其中 `TypeId`、`SymbolId`、`PlaceId`、`ValueId`、`LoanId`、`RegionId` 都是本次编译上下文
中的 identity，不是字符串、地址，也不承诺跨 state 或跨一次重新编译稳定。后端、诊断和
语言服务消费这些 identity，而不应当退回 AST 名称匹配。

## 2. 生成顺序与不变量

一段可正常编译的 function，概念上经历以下顺序：

1. parser 生成 AST，并给每个可诊断节点保留 `SZrFileRange`。
2. declaration binding 建立 symbol，canonical type 系统给表达式和声明分配 `TZrTypeId`。
3. compiler 建立 `SZrSemanticIrFunction`，登记参数/局部的 Place、值、region 与 cleanup scope。
4. 每个 bound expression 或 statement 发射一条或多条 `SZrSemanticIrInstruction`。
5. 把指令连续区间绑定至 CFG block，并给 block 设置 terminator。
6. `ZrParser_SemanticIr_Validate` 检查结构不变量，再运行 `ZrParser_SemanticFlow_Analyze`。
7. flow diagnostics 回写 compiler diagnostic 管线；通过验证的 SemIR 再供 ExecBC、artifact 和
   AOT projection 使用。

`ZrParser_Compiler_PreSemanticIr` 暴露的是 compiler 当前持有的 pre-semantic IR；
`ZrParser_Compiler_ValidatePreSemanticIr` 是面向编译流程的验证入口。普通宿主不应修改其
内部数组，也不应把它当作可持久化格式。需要写出可交换产物时，使用 writer/artifact API，见
[Artifact Writer、Loader 与元数据投影 API](05-interop/artifact-writer-loader-api.md)。

### 必须保持的关系

| 关系 | 约束 |
| --- | --- |
| 指令和 value | 产生结果的指令填写 `resultValueId`；每个值有 TypeId、定义指令和 source range。 |
| 指令和 Place | 对局部、字段、索引、解引用等可寻址位置，使用 `placeId`，不能只存显示名称。 |
| 指令和 operand | operand 位于 `valueOperands` 的连续切片，由 `operandStart` 和 `operandCount` 描述。 |
| block 和指令 | `ZrParser_SemanticIr_BindBlockRange` 将连续的 instruction range 与 block/terminator 绑定。 |
| loan 和 region | 借用必须指向 source Place，且带 region、origin、last use 和创建值。 |
| cleanup | 会产生资源清理语义的路径必须带 cleanup scope；异常、return、break/continue 也必须经过统一 cleanup。 |
| source map | 每条可诊断指令都带 source range，不能在后端自行猜测源码位置。 |

## 3. CFG：控制流的可查询骨架

编译器的源码 `if` 语句在条件产生可追溯的 SemIR `ValueId` 时，由
`compiler_semantic_cfg.c` 直接记录条件 `BRANCH`、有序的 `TRUE_BRANCH` / `FALSE_BRANCH`
边，以及 then、else 和 join 各自连续的 SemIR 指令区间。没有 `else` 时仍有一条空的假
分支通向 join。编译完成后，尾块连接到出口的 `RETURN`；这条路径不从 ExecBC 跳转偏移反推。
当前只接受简单字面量/标识符初始化、简单赋值、块和嵌套 `if` 组成的分支体。
没有启用源码 CFG 的代码仍使用旧的入口/出口两块占位图；包含目前尚未建模的
call/short-circuit/loop/return/throw/cleanup/suspend 路径的 `if` 不会发布不完整的条件菱形。

这只是 SSA 01.02 的源码条件分支阶段：循环、短路、异常/cleanup/suspend 和任意计算
条件尚未全部接入，不能由这一阶段推断那些路径已有精确的源码 CFG 或 phi。
当前 ExecIR builder 对普通源码中尚未补齐结果值的 `PLACE_BASE` 仍会拒绝构建，
本阶段的 SemIR CFG 测试不代表端到端 SSA 构建通过。

`SZrParserCfg` 是 function 级的控制流图。公开 block、edge 和 terminator 枚举的含义如下。

### Block kind

| 枚举 | 用途 |
| --- | --- |
| `ZR_PARSER_CFG_BLOCK_ENTRY` | function 的唯一入口。 |
| `ZR_PARSER_CFG_BLOCK_STATEMENT` | 普通语句或表达式序列。 |
| `ZR_PARSER_CFG_BLOCK_JOIN` | `if`、match、短路逻辑等多条路径的合流点。 |
| `ZR_PARSER_CFG_BLOCK_CLEANUP` | `using`、owner/drop、finally 等清理路径。 |
| `ZR_PARSER_CFG_BLOCK_SUSPENSION` | `yield`/async suspension 的边界。 |
| `ZR_PARSER_CFG_BLOCK_EXIT` | function 的正常或异常退出汇聚点。 |

### Edge kind

| 枚举 | 出现位置 |
| --- | --- |
| `NORMAL` | 顺序执行。 |
| `TRUE` / `FALSE` | 条件分支。 |
| `SWITCH_CASE` / `SWITCH_DEFAULT` | `match`、`switch` 和模式分派。 |
| `EXCEPTION` | 可抛出操作到 handler/cleanup 的边。 |
| `CLEANUP` | resource drop、finally 或 scope close。 |
| `RETURN` | return 从当前 block 通向退出。 |
| `SUSPEND` / `RESUME` | generator/async 的暂停和恢复。 |

### Terminator kind

`NONE` 表示 block 尚未封口；其余 `BRANCH`、`SWITCH`、`RETURN`、`THROW`、`BREAK`、
`CONTINUE`、`SUSPEND`、`CLEANUP_DISPATCH` 与 `EXIT` 表示该 block 不能再隐式落入另一段
不相关的指令。由 builder 写入后，consumer 以 edge/terminator 为准，而不是通过最后一条
AST statement 推断控制流。

### CFG C API

```c
SZrParserCfg cfg;

ZrParser_Cfg_Init(state, &cfg);
/* builder/编译器追加 block、连接 edge，并绑定 source AST node */
ZrParser_Cfg_BuildWithSemanticContext(state, &cfg, root, semanticContext);
ZrParser_Cfg_EmitReachabilityFacts(semanticContext, &cfg);
/* 读取时只用 BlockEdgeAt / BlockSuccessorIdAt，不假定数组布局。 */
ZrParser_Cfg_Free(state, &cfg);
```

API 允许工具构建和检查 CFG，但它接收的是 parser state、AST 和 semantic context；独立工具
不应从未绑定的 AST 构造“看似正确”的 CFG 后直接交给 runtime。

## 4. `SZrSemanticIrFunction` 的数据模型

该结构拥有以下数组。其内存 owner 是 `state`，初始化和释放必须成对调用
`ZrParser_SemanticIrFunction_Init` / `ZrParser_SemanticIrFunction_Free`。

| 成员 | 元素 | 目的 |
| --- | --- | --- |
| `places` | Place graph | 将 local、field、index、deref 等可重叠位置规范化。 |
| `cfg` | `SZrParserCfg` | 表达可达性、分支、异常和 suspension。 |
| `locals` | `SZrSemanticIrLocal` | symbol 到 base Place、TypeId、parameter 标记。 |
| `values` | `SZrSemanticIrValue` | SSA 风格的值 identity 与定义位置。 |
| `instructions` | `SZrSemanticIrInstruction` | 行为、operand slice、控制和 ownership effect。 |
| `valueOperands` | `TZrValueId` | 指令引用值的紧凑连续存储。 |
| `regions` | `SZrSemanticIrRegion` | lexical/lifetime region 及 escape upper bound。 |
| `cleanupScopes` | `SZrSemanticIrCleanupScope` | 资源关闭和展开边界。 |
| `loanFacts` | `SZrSemanticIrLoanFact` | shared/mutable/two-phase borrow 的生命周期。 |
| `escapeFacts` | `SZrSemanticEscapeFact` | return、closure capture、heap/native capture 等 escape。 |
| `contiguousViewFacts` | `SZrSemanticContiguousViewFact` | 数组/owner/pinned/view 的连续内存视图。 |
| `boundsFacts` | `SZrSemanticBoundsFact` | 索引的常量证明或运行时检查需求。 |
| `sourceMap` | `SZrSemanticIrSourceMapEntry` | instruction 到 source range 的稳定映射。 |

## 5. 指令格式与 opcode 家族

每条 `SZrSemanticIrInstruction` 都带 `id`、`opcode`、Type/Place/Value/Symbol identity、
`targetBlockId`、loan/region/cleanup 信息、ownership operation、operand slice 和 source range。
不是每个字段都对每个 opcode 有意义；consumer 必须按 opcode 解释，不能把零值当成可用 ID。

### 值与 Place

| opcode | 语义 |
| --- | --- |
| `CONSTANT` / `CONVERT` | 物化常量或执行已解析的类型转换。 |
| `PLACE_BASE` / `PLACE_PROJECT` | 建立 local/parameter 基址或 field/index/deref 投影。 |
| `LOAD` / `STORE` / `INITIALIZE` | 读取已有值、写入已有 storage、首次初始化。 |
| `MOVE` / `COPY` / `DROP` | 把 availability、copy/drop effect 显式化。 |
| `DEREFERENCE` | 对已验证可解引用的 place/value 投影。 |

`STORE` 与 `INITIALIZE` 有意分开：前者要求目标已经具有可写的存储语义，后者用于 definite
assignment 的起点。`MOVE` 后的源 Place 不再可当作 `AVAILABLE`；它的后续读取由 flow analyzer
报告 use-after-move 或 maybe-moved，而不是由后端悄悄 copy。

### 借用、资源和 escape

| opcode / operation | 作用 |
| --- | --- |
| `BORROW_SHARED` | 创建共享 loan，可与其它共享 loan 并存。 |
| `BORROW_MUT` | 创建立即生效的可变 loan。 |
| `RESERVE_BORROW_MUT` / `ACTIVATE_LOAN` | two-phase mutable borrow 的保留与激活。 |
| `REBORROW` / `END_LOAN` | 从已有借用继续派生或在最后使用后结束。 |
| `SCOPE_ENTER` / `SCOPE_EXIT` / `CLEANUP` | 建立、离开和执行 cleanup scope。 |
| `OWN_CONSTRUCT` | 建立显式 owner/resource 语义。 |
| `ownershipOperation` | `UNIQUE`、`SHARE`、`DEGRADE`、`WAKE`、`INTO_GC_BOX`、`RETURN_TO_GC` 等过渡标签。 |

语言中的 `share`、`degrade`、`wake`、`intoGc`、`drop` 语法会在 parser/semantic 层被识别并
投影成 ownership effect；并不是一段任意 native 函数调用。详细的源语言限制见
[表达式、构造与调用形状参考](02-language/expression-construction-reference.md)。

### 调用、构造和成员

| opcode | 语义 |
| --- | --- |
| `CALL_TYPED` | 已经解析 contract 的静态调用。 |
| `CALL_VIRTUAL` | interface/prototype dispatch。 |
| `CALL_DYNAMIC` | 只能在保留动态语义的 call site 使用。 |
| `CALL_META` | property/meta protocol 调用。 |
| `VALUE_CONSTRUCT` / `AGGREGATE_CONSTRUCT` | `init`、struct/tuple 等值构造。 |
| `FIELD_INITIALIZE` / `UNION_CONSTRUCT` | 成员初始化和 union variant 构造。 |
| `GC_NEW` | 需要受 GC 管理的 `new` 路径。 |
| `PROPERTY_GET` / `PROPERTY_SET` / `PROPERTY_REF_GET` | accessor/property 的读取、赋值和 ref 获取。 |

### 控制流、模式与迭代

`BRANCH`、`SWITCH`、`RETURN`、`THROW` 与 CFG terminator 对应；`DESTRUCTURE_EVALUATE`、
`SHAPE_VALIDATE`、`DESTRUCTURE_PROJECT`、`DESTRUCTURE_LEAF_ASSIGN`、`DESTRUCTURE_LEAF_BIND`、
`DESTRUCTURE_REST` 记录模式解构不能省略的顺序；`YIELD_VALUE`、`YIELD_SUSPEND`、
`YIELD_RESUME`、`ITERATOR_COMPLETE` 把可暂停迭代器的状态转换显式化。

## 6. 流分析结果如何读

`SZrSemanticFlowResult` 是一次分析的输出，拥有 block facts、diagnostics、instruction-level
loan liveness 与 loan region facts。先 `Init`，后 `Analyze`，最后 `Free`；在一次分析期间，
其中返回的指针均为借用视图。

### Place state

每个可跟踪 Place 在 block entry/exit 都有：

| 维度 | 枚举值 | 含义 |
| --- | --- | --- |
| initialization | `UNINITIALIZED`、`INITIALIZED`、`MAYBE_INITIALIZED` | 是否在所有可达路径上已写入。 |
| availability | `AVAILABLE`、`MOVED`、`MAYBE_MOVED`、`DROPPED` | 是否还能被读取或移动。 |
| borrowing | shared loan 列表 + mutable loan | 当前重叠范围内的借用占用。 |
| escape | `LOCAL`、`FUNCTION`、`CALLER`、`HEAP_STATIC`、`UNKNOWN` | 值可能逃逸到的边界。 |

合流 block 不会假装某一路径从未发生：一条路径未初始化、另一条已初始化时，结果保留为
`MAYBE_INITIALIZED`；一条路径移动、另一条未移动时，结果保留为 `MAYBE_MOVED`。这使诊断
可以同时给出使用点、声明点、loan origin 和 last-use range。

### Diagnostics

`EZrSemanticFlowDiagnosticKind` 包含：

- `UNINITIALIZED`、`MAYBE_UNINITIALIZED`；
- `USE_AFTER_MOVE`、`MAYBE_MOVED`、`USE_AFTER_DROP`；
- `LOAN_CONFLICT`；
- `ESCAPE_VIOLATION`。

`SZrSemanticFlowDiagnostic` 额外保存相关 Place、overlap、loan origin/last-use、escape fact 和
源/目标 range。诊断渲染层应使用这些信息生成 related locations，而不是只输出“borrow error”。
完整的 message/fix 生命周期见[诊断生命周期与修复契约](06-reference/diagnostic-lifecycle-reference.md)。

### 连续 view 与 bounds proof

`SZrSemanticContiguousViewFact` 描述一个 view 对应的 source Place、起点/长度 value、region、
loan 和 source kind（array、owner、native pinned 或另一个 view）。`SZrSemanticBoundsFact` 记录
索引是否拥有常量范围证明，或必须保留 runtime check；`checkElided` 只能由该 fact 支持。

因此 AOT backend 不能仅因索引表达式“看起来是常量”就删掉检查，也不能把来自 native 的裸地址
误当成连续数组 view。native/pinned 边界还必须遵守 [C 值与 GC 生命周期](05-interop/c-api-value-lifecycle.md)。

## 7. 公共 C API 使用模式

以下示例适合 compiler 扩展、测试或内部工具；它展示对象的所有权和调用顺序，不代表宿主需要
手工重建完整 compiler IR。

```c
SZrSemanticIrFunction function;
SZrSemanticFlowResult flow;

ZrParser_SemanticIrFunction_Init(state, &function, symbolId, callableTypeId);
ZrParser_SemanticFlowResult_Init(state, &flow);

/* AddLocal / AddValue / AddRegion / AddLoanEx / Emit 由绑定后的 lowering 填充。 */
if (!ZrParser_SemanticIr_Validate(&function) ||
    !ZrParser_SemanticFlow_Analyze(state, &function, &function.cfg, &flow)) {
    /* compiler 的状态与 diagnostic 管线保存失败原因。 */
}

/* BlockFacts、PlaceState、LoanIsLiveAt 仅在 flow 存活期间读取。 */
ZrParser_SemanticFlowResult_Free(state, &flow);
ZrParser_SemanticIrFunction_Free(state, &function);
```

常用查询如下：

| API | 适合的问题 |
| --- | --- |
| `ZrParser_SemanticIr_InstructionAt` / `Value` / `Loan` | 用 ID 找 IR 实体。 |
| `ZrParser_SemanticIr_OpcodeName` | trace、golden test 或诊断显示 opcode。 |
| `ZrParser_SemanticIr_FormatGolden` | 生成稳定的测试可读 IR 文本。 |
| `ZrParser_SemanticFlow_BlockFacts` | 读取某 block entry/exit facts。 |
| `ZrParser_SemanticFlow_PlaceState` | 查询特定 Place 的 entry 或 exit state。 |
| `ZrParser_SemanticFlow_BuildInitializedPlaceBitmap` | 以位图投影一组 Place 的初始化状态。 |
| `ZrParser_SemanticFlow_LoanIsLiveAt` / `LoanIsActiveAt` | 查询 loan 在指令前/后是否 live 或 active。 |
| `ZrParser_SemanticFlow_LoanRegion` | 读取 two-phase loan 的 first-live、activation、last-use 范围。 |
| `ZrParser_SemanticFlow_EscapeDiagnostic` | 按 escape fact 关联已有诊断。 |

## 8. 后端与工具必须遵守的边界

### ExecBC 与解释器

ExecBC 可以 quicken 已验证的热点，但不能改变 `MOVE`、cleanup、property 或 dynamic call 的可见
行为。guard miss 必须走 checked path；不能以 cache 命中假设取代 semantic facts。

### AOT C/LLVM

AOT 使用 TypeLayout、Place、CFG、loan/cleanup 和 bounds facts 来决定 frame layout、GC root、
直接调用与保留检查。它只能在 fact 表明安全时省略检查。关于 generated frame、deopt 和 module
registration，请见[AOT Lowering、运行时 Helper 与注册参考](10-aot-lowering-registration-reference.md)。

### LSP 和增量查询

LSP 的 hover、definition、reference 和 diagnostic 读取已发布的 semantic snapshot。它不得在
request 期间绕过 semantic producer、只按 identifier 文本搜索，否则会在 generic instance、shadowing、
property accessor 与 module revision 下给出错误答案。详见[Language Server 协议与嵌入式接口](04-tools/language-server-protocol-reference.md)。

## 9. 常见错误

| 错误做法 | 正确做法 |
| --- | --- |
| 用字符串比较类型决定是否能直接调用。 | 使用 canonical TypeId、call binding 和 SemIR call opcode。 |
| 把 `STORE` 视为首次初始化。 | 由 `INITIALIZE` 建立 definite-assignment 起点。 |
| 只记录 borrow 起点，不记录 last-use。 | 添加 loan fact 并通过 flow liveness 查询其有效期。 |
| 直接删除数组边界检查。 | 只有对应 `SZrSemanticBoundsFact` 允许 `checkElided` 时才删除。 |
| 把 `return` 视为跳过 drop。 | 通过 CFG `RETURN`/`CLEANUP` path 和 cleanup scope 展开。 |
| 让工具保存 `InstructionAt` 指针跨下一次编译。 | 保存可持久化的诊断/产物或在同一 snapshot/revision 内重新查询。 |

## 10. 验证入口

优先运行 `tests/parser/test_pre_semantic_ir.c`、`test_semantic_facts.c` 和
`test_semir_pipeline.c`。扩展一条语言功能时，至少要覆盖：正向 IR 形状、分支合流、异常或
cleanup 路径、borrow/escape 负例，以及解释器/AOT 两条消费路径中的一条可执行证据。
