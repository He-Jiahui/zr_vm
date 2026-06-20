---
doc_type: plan-index
plan_sources:
  - user: 2026-06-20 参照 C# struct 栈上连续布局 / il2cpp 风格 typed lowering / AOT 必须降级为纯 C 元素操作（= / memcpy / +-*/）而非调用 VM 行为 / metadata 驱动消除指令不确定性 / 解释器与纯 C 双执行路径
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_numeric.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_conversion.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - docs/core-runtime/inline-type-layout-and-byte-stack.md
references:
  - lua/hybridclr/libil2cpp/hybridclr/interpreter/Instruction.h
  - lua/hybridclr/libil2cpp/hybridclr/metadata/ClassFieldLayoutCalculator.h
  - lua/hybridclr/libil2cpp/codegen/il2cpp-codegen.h
---

# AOT 计划：面向 il2cpp 风格纯 C 降级的指令集与数据模型改造

本计划目录规划 zr_vm 从“tagged-union 解释器 + 半降级 AOT”演进为
**“metadata 驱动的类型确定指令集 + 可降级为纯 C 元素操作的统一中间层”**，
使同一份编译产物既能在解释器上运行，又能被 il2cpp 风格地翻译为不依赖虚拟机行为的纯 C 代码。

## 1. 目标（North Star）

参照 `lua/il2cpp` 与 `lua/hybridclr` 的实现架构，达成以下硬性目标：

1. **值类型 C# 化**：`struct` 作为值类型，在栈上**连续字节布局**，赋值即 `=` 或 `memcpy`，
   而非调用 `ZrCore_*_Assign` 之类的 VM 函数。参考 il2cpp 的 `ClassLayoutInfo` 与
   blittable 判定（`lua/hybridclr/.../ClassFieldLayoutCalculator.h`）。
2. **AOT 不退化为指令调用**：生成的 C 代码必须是**真实的 C 语言元素操作**——
   运算用 `+ - * / %`、赋值用 `=` / `memcpy`、字段访问用 `.`/`->`/偏移，
   **不允许**退化为 `ZrCore_Stack_GetValue` + `ZR_VALUE_FAST_SET` + `arith` 函数这类
   “在 C 里重新搭一个解释器”的形态。
3. **指令含义明确化**：每条指令在编译期就具备**确定的操作数类型与语义**，
   消除“运行时根据 tag 决定行为”的不确定性；全局类型解析完整，不允许不明确的声明。
4. **metadata 驱动存储**：值的形状、字段偏移、对齐、GC/所有权位置由**类型 metadata** 描述，
   指令本身只携带“做什么 + 在哪个 typed 槽上做”，不携带运行时类型分支。
5. **双执行路径**：解释器与纯 C 后端共享同一套 **typed SemIR**（见 `04`），
   两者是同一语义的两种降级目标；纯 C 路径减轻对解释器的依赖，
   解释器路径作为 deopt / 动态回退（对标 hybridclr 的 hybrid 模型）。

## 2. 现状一句话总结

详见 [`00-current-state.md`](00-current-state.md)。要点：

- 指令集（`zr_instruction_conf.h`，251 条）已大量做了**类型特化变体**
  （`ADD_SIGNED` / `ADD_FLOAT` / `..._LOAD_CONST`），方向正确，但仍以
  `SZrTypeValue`（tagged union，含 type tag + 所有权控制块 + GC 标记，~60–80B/槽）为统一容器。
- AOT C 后端（`backend_aot_c_lowering_*.c`）**已能写出真实 C 算术**，
  但写入/读出仍走 `ZrCore_Stack_GetValue` + `ZR_VALUE_FAST_SET`，
  即“运算降级了、存储没降级”——这是本计划要消灭的**半降级**状态。
- 已存在 C# 化的雏形：`SZrTypeLayout` / `SZrTypeLayoutField`（`type_layout.h`）、
  `SZrFunctionFrameSlotLayout`（`function.h`，inline struct 字节槽）、
  `SZrSemIrInstruction` typed IR（`function.h:368-377`，含
  `FIELD_ADDR/LOAD_VALUE/STORE_VALUE/COPY_VALUE/CALL_TYPED`）。
  这些是本计划的**地基**，但仍与旧固定槽 ABI（`functionBase + slot`）并行存在、尚未取代它。

## 3. 设计纲领（不变量）

完整论证见 [`01-design-principles.md`](01-design-principles.md)。三条贯穿全计划的不变量：

- **不变量 A（确定性）**：进入 AOT/typed 路径的每个 SemIR 槽都有**单一静态 C 类型**
  （`TZrInt64` / `TZrFloat64` / 某个 `struct ZrLayout_*` / 某个 GC 引用指针）。
  类型不确定的表达式**不得**进入 typed 路径，只能留在 dynamic（tagged-union）路径并显式 deopt。
- **不变量 B（纯降级）**：typed 路径的任何 SemIR 指令都必须能映射到**不含 VM 调用**的 C 片段
  （算术/赋值/字段/比较/分支）。需要 VM 运行时的操作（GC 分配、动态派发、所有权状态机）
  以**显式 runtime 调用点**出现，且这些点是 ABI 契约的一部分，而非“顺手调一下”。
- **不变量 C（单一真相）**：值的形状只由类型 metadata（`SZrTypeLayout` / 原型）描述一次，
  解释器、AOT C、GC 扫描、反射全部读同一份 metadata，禁止在多处各自硬编码偏移。

## 4. 阶段与文档索引

| 序号 | 文档 | 主题 | 状态 |
|------|------|------|------|
| 00 | [`00-current-state.md`](00-current-state.md) | 现状基线与差距清单（事实坐标） | 📋 规划 |
| 01 | [`01-design-principles.md`](01-design-principles.md) | 设计纲领、不变量、与 il2cpp/hybridclr 对标 | 📋 规划 |
| 02 | [`02-typed-value-and-layout.md`](02-typed-value-and-layout.md) | 值表示去 tag 化 + 栈/struct 连续布局 + metadata | 📋 规划 |
| 03 | [`03-instruction-set-refactor.md`](03-instruction-set-refactor.md) | 指令集改造：含义明确化、typed 化、消除不确定性 | 📋 规划 |
| 04 | [`04-semir-and-c-backend.md`](04-semir-and-c-backend.md) | SemIR 统一中间层 + 纯 C 后端降级规则 | 📋 规划 |
| 05 | [`05-ownership-gc-and-bridge.md`](05-ownership-gc-and-bridge.md) | 所有权/GC 在生成 C 中的表达 + 解释器与 AOT 桥 | 📋 规划 |
| 06 | [`06-implementation-blueprint.md`](06-implementation-blueprint.md) | 分阶段路线图、里程碑、测试矩阵、验收标准 | 📋 规划 |

## 5. 与现有模块的关系

- 本计划是 [`docs/core-runtime/inline-type-layout-and-byte-stack.md`](../../core-runtime/inline-type-layout-and-byte-stack.md)
  的**上层规划**：那篇文档实现了 typed layout + byte-offset 栈原语；本计划规划如何让
  **指令集与代码生成全面切换到这套原语之上**，并最终取代旧固定槽 ABI。
- 与 [`docs/plans/using/`](../using/index.md) 的所有权/metadata token 工作是**互补**关系：
  using 计划提供了所有权种类与 metadata token 模型，本计划负责把它们**降级到生成的 C 代码**里
  （见 `05`），并保证 typed 路径下所有权检查能被编译期证明消除。

##状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 里程碑/切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-06-20 21:35:01 +08:00 · M2 / 04-S3 signed branch local-written-before
  no-refresh 切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.{h,c}` 新增同一 basic block 内 `i64` local-written-before
  保守证明，只记录当前分支指令之前已经写入的 SemIR destination、立即数常量、数值转换 destination
  与可传播 stack-copy destination；`backend_aot_c_lowering_control.c` 的 signed slot/slot
  分支入口现在接收 `execInstructionIndex`，仅当左右操作数都在当前块内已写入 `sN` local 时，
  保留 frame-slot 边界/type tag 校验但直接发出 `if (zr_aot_sL op zr_aot_sR)`，不再把
  `zr_aot_left/right->...nativeInt64` 重新刷新到 `sN`。`JUMP_IF_NOT_EQUAL_SIGNED_CONST`
  同样接收当前指令索引，但在 focused `product == 42` 形状下没有同块写入证明，因此保守回退到
  `zr_aot_left_scalar != zr_aot_right_literal`，避免把跨块或旧帧值误当作权威 local · RED/GREEN：
  先加入 source-contract 对 `backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot,
  execInstructionIndex)` / `rightSlot` 的要求后 RED，报缺少 written-before helper；实现同块写入证明、
  分支签名透传和 no-refresh 模板后 source contracts GREEN。generated-product focused 断言同步要求
  `zr_aot_s2 <= zr_aot_s4` slot/slot 分支没有 `zr_aot_s2 = zr_aot_left->...nativeInt64`
  / `zr_aot_s4 = zr_aot_right->...nativeInt64`，并明确 const 分支当前回退到 left-scalar · 测试结果：
  AOT C source contracts 19/0，exit=0；`backend_aot_c_scalar_locals.c`、
  `backend_aot_c_lowering_control.c`、`backend_aot_c_function_body.c` focused 对象编译通过并同步到
  临时 patched parser archive；AOT C typed scalar focused binary 1/0，exit=0 · 备注：
  本切片只证明同块、当前指令之前的 `i64` local 写入；不声明跨 basic-block/join local 可用性，
  也不完成 resume/deopt local-state 重建、GC 根登记、系统性 C-local/frame-slot 权威存储、
  broader branch 与 typed/dynamic bridge。

- 2026-06-20 21:14:55 +08:00 · M2 / 04-S3 scalar destination-local fallback
  mirror 切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_binary.c` 的 i64/u64/f64 binary fallback 路径现在在结果
  `zr_aot_*_result` 已计算后，若 destination slot 有对应 scalar-local coverage，
  同步写入 `zr_aot_sN` / `zr_aot_uN` / `zr_aot_fN`。`backend_aot_c_scalar_semir.c`
  的 i64/u64 compare fallback 路径同样在 bool result 已计算后同步 `zr_aot_bN`。
  `backend_aot_c_scalar_bitwise.c` 的 i64/u64 bitwise、bit-not、shift fallback
  路径也补齐 destination-local mirror。该切片不改变 operand 读取策略，仍保留必要
  frame-slot 验证/读取，只保证“已有结果写 frame 前先同步 proven destination local” · RED/GREEN：
  source-contract 加入 `zr_aot_s%u = zr_aot_s_result;`、`zr_aot_u%u = zr_aot_u_result;`、
  `zr_aot_f%u = zr_aot_f_result;`、`zr_aot_b%u = zr_aot_s_result;` 与
  `zr_aot_b%u = zr_aot_u_result;` 等模板要求后保持 GREEN · 测试结果：
  AOT C source contracts 19/0，exit=0；`backend_aot_c_scalar_binary.c`、
  `backend_aot_c_scalar_semir.c`、`backend_aot_c_scalar_bitwise.c` focused 对象编译通过并同步到
  临时 patched parser archive；AOT C typed scalar focused binary 1/0，exit=0 · 备注：
  本切片为后续减少 frame→local refresh 打地基，但仍不声明 C locals 已成为全路径权威存储；
  resume/deopt local-state 重建、GC 根登记、broader branch 与 typed/dynamic bridge 仍未完成。

- 2026-06-20 21:09:10 +08:00 · M2 / 04-S3 primitive constant scalar-local
  mirror 切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_write_c_direct_primitive_constant()` 现在接收 `functionIr`，并在立即数
  bool/i64/u64/f64 常量写入 `SZrTypeValue` frame slot 后，若目标 slot 已声明对应
  scalar-local coverage，则同步写入 `zr_aot_bN` / `zr_aot_sN` / `zr_aot_uN` /
  `zr_aot_fN`。`backend_aot_c_lowering_values.c` 新增对
  `backend_aot_c_scalar_locals_has_*_slot()` 的常量目的槽查询；`backend_aot_c_function_body.c`
  的 `GET_CONSTANT` 立即数路径把 `functionIr` 传入常量发射器。focused typed scalar
  生成物现在锁定 `zr_aot_s0 = (TZrInt64)21;`、`zr_aot_s1 = (TZrInt64)2;`、
  `zr_aot_s4 = (TZrInt64)40;`、`zr_aot_s6 = (TZrInt64)9;`、`zr_aot_f19 = (TZrFloat64)1.5;`
  与 `zr_aot_f20 = (TZrFloat64)2;` 等立即数 local mirror · RED/GREEN：
  初始 generated-product 断言假设 `uint` literal 直接写 `u6/u7`，focused typed scalar
  RED 于缺少 `zr_aot_u6 = (TZrUInt64)9;`；检查生成物确认当前前端仍先把 `uint`
  字面量落为 signed `INT64` 常量，再由后续 typed unsigned 路径维护 `uN`，因此把本切片约束收窄为
  signed/float 立即数 local mirror 后 GREEN · 测试结果：AOT C source contracts 19/0，
  exit=0；`backend_aot_c_lowering_values.c` 与 `backend_aot_c_function_body.c` focused
  对象编译通过并已同步到临时 patched parser archive；AOT C typed scalar focused binary
  1/0，exit=0 · 备注：本切片仍不让 C locals 成为系统性权威存储，也不声明完整
  CTest/CMake 通过；unsigned literal 的前端常量类型归一、resume/deopt local-state 重建、
  GC 根登记、broader branch、typed/dynamic bridge 仍未完成。

- 2026-06-20 21:01:57 +08:00 · M2 / 04-S3 unsigned compare bool-local
  branch generated-product lock 与 stack-copy propagation source-contract hardening 切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：检查 focused
  `if (unsignedInverted <= unsignedRight)` 生成物后确认当前没有独立的 unsigned/f64
  branch 降级入口；前端先生成 `zr_aot_scalar_exec_u64_compare`，把结果写入
  `TZrBool zr_aot_b23`，随后复用上一切片的 `zr_aot_jump_if_bool_false_scalar_local`
  直跳路径。`tests/parser/test_aot_c_typed_scalar.c` 现在锁定
  `TZrBool zr_aot_b23 = ZR_FALSE;`、`zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);`、
  `zr_aot_u_result = zr_aot_b23;` 与 `if (!zr_aot_b23) {`，避免后续把 unsigned
  compare→branch 退回到 bool frame refresh。`tests/parser/test_aot_c_source_contracts.c`
  同步加固 stack-copy destination propagation source-contract，要求源码存在
  `backend_aot_c_scalar_locals_instruction_is_stack_copy(`、`sourceKind = slotKinds[sourceSlot];`
  与按 source kind 记录 destination slot 的调用 · RED/GREEN：本切片以现有生成物的检查结论与
  generated-product/source-contract 加固为主；新增断言后当前实现保持 GREEN。原计划中的
  “独立 unsigned/f64 branch 后端入口”经生成物检查不成立，后续 broader branch 工作应归入
  systematic C-local/frame-slot authoritative storage 与更完整控制流矩阵，而不是新增虚构入口 · 测试结果：
  AOT C source contracts 19/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0 · 备注：
  本切片不声明完整 CTest/CMake 通过；仍未完成 resume/deopt local-state 重建、GC 根登记、
  系统性 C-local/frame-slot 权威存储切换、broader branch 与 typed/dynamic bridge。

- 2026-06-20 20:53:38 +08:00 · M2 / 04-S3 scalar stack-copy destination
  temporary local propagation 切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.c` 现在扫描 `GET_STACK` / `SET_STACK` stack-copy
  字节码，在 source slot 已有 scalar-local kind 时把该 kind 传播到 destination slot。
  这让 `var floatCopy: float = floatLeft;` 产生的中间 copy 槽 `40` 声明
  `TZrFloat64 zr_aot_f40 = 0.0;`，第一段 copy 写入 `zr_aot_f40 = zr_aot_f_value;`，
  第二段 copy 读取 `zr_aot_f_value = zr_aot_f40;` 并写入 `zr_aot_f22`，不再从
  `zr_aot_source->value.nativeObject.nativeDouble` 读取 payload · RED/GREEN：
  source-contract 先 RED 于缺少 `backend_aot_c_scalar_locals_record_stack_copy_destinations(`；
  generated-product 先 RED 于缺少 `TZrFloat64 zr_aot_f40 = 0.0;`。实现 stack-copy
  destination kind 传播、重建 patched parser archive 后到 GREEN · 测试结果：
  AOT C source contracts 19/0，exit=0；`backend_aot_c_scalar_locals.c` focused
  对象编译通过；AOT C typed scalar focused binary 1/0，exit=0；generated product
  marker scan 命中 `zr_aot_f40` 链路且无 `ZrCore_Stack_GetValue(` /
  `ZR_VALUE_FAST_SET(` / f64 stack-copy source-frame payload read · 备注：
  本切片继续通过直接 WSL gcc/gold 与临时 patched 静态库验证，不声明完整
  CTest/CMake 通过；仍未完成 resume/deopt local-state 重建、GC 根登记、系统性
  C-local/frame-slot 权威存储切换、 broader branch 与 typed/dynamic bridge。

- 2026-06-20 20:43:27 +08:00 · M2 / 04-S3 scalar stack-copy f64 generated-product
  coverage 切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `tests/parser/test_aot_c_typed_scalar.c` 的 focused typed scalar 源码新增
  `var floatCopy: float = floatLeft;`，触发 executable f64 scalar stack-copy
  生成物；测试现在锁定 `zr_aot_scalar_stack_copy_f64`、f64 local 声明
  `TZrFloat64 zr_aot_f22 = 0.0;`、source-local 读取 `zr_aot_f_value = zr_aot_f19;`
  与 destination-local 镜像 `zr_aot_f22 = zr_aot_f_value;` · RED/GREEN：
  先只加入 `zr_aot_scalar_stack_copy_f64` 生成物断言时 focused typed scalar RED，
  证明原输入没有覆盖 f64 copy；加入最小 f64 复制语句并补精确断言后到 GREEN · 测试结果：
  AOT C typed scalar focused binary 1/0，exit=0，生成 shared library 继续与解释器结果一致；
  generated product forbidden-token scan 仍无 `ZrCore_Stack_GetValue(` / `ZR_VALUE_FAST_SET(` · 备注：
  本切片补齐 f64 stack-copy 的 executable generated-product 覆盖，但当前生成物中
  `dstSlot=40 -> srcSlot=19` 只证明 source-local 读取，`dstSlot=22 -> srcSlot=40`
  只证明 destination-local 镜像；临时槽 `40` 尚未声明为 f64 C local，完整连续
  f-local copy 链路仍是后续 04-S3 系统性 local-state 工作。

- 2026-06-20 20:38:15 +08:00 · M2 / 04-S3 scalar stack-copy i64/u64 generated-product
  coverage 加固切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  在 `tests/parser/test_aot_c_typed_scalar.c` 中把已实现的 i64/u64 scalar stack-copy
  source-local 与 destination-local 形态固化为 executable generated-product 断言：
  `zr_aot_u_value = zr_aot_u12;` / `zr_aot_u9 = zr_aot_u_value;`、
  `zr_aot_u_value = zr_aot_u33;` / `zr_aot_u21 = zr_aot_u_value;`、
  `zr_aot_s_value = zr_aot_s16;` / `zr_aot_s13 = zr_aot_s_value;`、
  `zr_aot_s_value = zr_aot_s19;` / `zr_aot_s18 = zr_aot_s_value;`，并禁止
  stack-copy payload 退回 `zr_aot_source->value.nativeObject.nativeUInt64/Int64`
  读取 · RED/GREEN：RED 为测试覆盖缺口：上一切片虽然 source-contract 覆盖了
  bool/i64/u64/f64 source-local 模板，但 executable typed scalar 生成物只显式锁定
  bool `branchFlag` copy；加断言后 focused typed scalar binary 到 GREEN · 测试结果：
  AOT C typed scalar focused binary 1/0，exit=0；继续通过临时 patched
  `libzr_vm_parser_stack_copy_source_local.a` 链接，不声明完整 CTest/CMake 通过 · 备注：
  本切片只加固当前生成物中已有的 i64/u64 stack-copy source/destination local 覆盖；
  当前 focused typed scalar 程序尚未产生 f64 scalar stack-copy generated-product marker，
  f64 仍由 source-contract 模板覆盖，后续需单独补 executable f64 copy 用例。

- 2026-06-20 20:31:38 +08:00 · M2 / 04-S3 scalar stack-copy source-local mirror 切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_stack_copy.c` 的 bool/i64/u64/f64 scalar stack-copy 直接路径
  现在同时查询 source 与 destination scalar-local 覆盖；source proven 时先从
  `zr_aot_bN` / `zr_aot_sN` / `zr_aot_uN` / `zr_aot_fN` 读取 copy payload，
  destination proven 时继续把结果同步到 destination local。focused generated product
  锁定 `branchFlag` copy 产生 `zr_aot_b_value = zr_aot_b7;`，随后
  `zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);` 并由 proven bool branch 直接
  `if (!zr_aot_b5) { ... }` · RED/GREEN：
  source-contract 先 RED 于缺少 `backend_aot_c_scalar_locals_has_*_slot(functionIr, sourceSlot)`
  与 `zr_aot_*_value = zr_aot_*%u;` 模板；generated-product tightening 也要求旧
  `zr_aot_b_value = zr_aot_source->value.nativeObject.nativeBool;` 不再出现在 focused
  bool copy 路径。实现 source-local payload 读取后到 GREEN · 测试结果：
  AOT C source contracts 19/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  `backend_aot_c_scalar_stack_copy.c` focused 对象编译通过；generated typed scalar
  product forbidden-token scan 中 `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(`
  均为 0；marker scan 命中 `zr_aot_b_value = zr_aot_b7;`、
  `zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);`、
  `zr_aot_jump_if_bool_false_scalar_local` 与 `if (!zr_aot_b5) {` · 备注：
  本记录不声明 CTest 或完整 WSL CMake/Ninja 通过；验证仍采用直接 WSL gcc/gold
  对象编译/链接临时 patched 静态库与生成 shared library 的方式。当前切片只覆盖
  focused scalar stack-copy source/destination local mirror；系统性 C-local/frame-slot 镜像、
  resume/deopt 入口 local-state 重建、GC 根登记、更多 branch 形态、完整 03-S3 block
  形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 20:21:40 +08:00 · M2 / 04-S3 bool branch scalar-local no-refresh 切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  在上一切片已让 scalar stack-copy 维护 destination `bN` 的基础上，
  `backend_aot_write_c_direct_jump_if_bool_false()` 的 proven bool scalar-local 路径现在发射
  `zr_aot_jump_if_bool_false_scalar_local` 并直接使用 `if (!zr_aot_bN) { goto ...; }`，
  不再在该 proven 路径里从 `frame.slotBase[condition].value` 重新读取 bool。
  未证明 bool scalar-local 的 fallback 仍保留 frame-slot bool 验证与
  `zr_aot_condition_bool` 临时谓词 · RED/GREEN：
  source contract 先 RED 于缺少 `zr_aot_jump_if_bool_false_scalar_local`，并把旧
  `zr_aot_b%u = (TZrBool)(zr_aot_condition->value.nativeObject.nativeBool != 0u);`
  列为控制流源码 forbidden marker；实现 proven-local 直跳后到 GREEN。
  generated-product focused binary 同步要求 `branchFlag` 先由 stack-copy 产生
  `zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);`，随后分支含
  `zr_aot_jump_if_bool_false_scalar_local` 和 `if (!zr_aot_b5) {`，且不再含旧
  `zr_aot_b5 = (TZrBool)(zr_aot_condition->value.nativeObject.nativeBool != 0u);` · 测试结果：
  AOT C source contracts 19/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  `backend_aot_c_lowering_control.c` 与 `backend_aot_c_scalar_stack_copy.c` focused
  对象编译通过；generated typed scalar product forbidden-token scan 中
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` 均为 0；marker scan 命中
  `zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);`、
  `zr_aot_jump_if_bool_false_scalar_local` 与 `if (!zr_aot_b5) {` · 备注：
  本记录不声明 CTest 或完整 WSL CMake/Ninja 通过；验证仍采用直接 WSL gcc/gold
  对象编译/链接临时 patched 静态库与生成 shared library 的方式。当前切片只覆盖
  focused straight-line proven bool-local branch；resume/deopt 入口的 C-local 重建、
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block
  形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 20:11:41 +08:00 · M2 / 04-S3 scalar stack-copy destination-local mirror 切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_stack_copy.c` 现在在 bool/i64/u64/f64 scalar stack-copy
  直接路径写回 frame slot 后，若 destination slot 有对应 scalar-local 覆盖，同步写入
  `zr_aot_bN` / `zr_aot_sN` / `zr_aot_uN` / `zr_aot_fN`。focused generated product
  现在锁定 `branchFlag` 的 bool copy 先产生
  `zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);`，为后续 `JUMP_IF_BOOL_FALSE`
  proven-local 直跳提供本地谓词输入。`tests/parser/test_aot_c_source_contracts.c`
  新增 scalar stack-copy destination-local mirror source contract，使 source-contract
  计数从 18 增至 19 · RED/GREEN：
  tightening generated-product/source-contract 后，旧 stack-copy 只写 `frame.slotBase[5]`
  而不维护 declared `zr_aot_b5`，不满足 C-local/frame-slot 镜像契约；实现
  destination-local mirror 后到 GREEN，并保留旧 `if (!zr_aot_condition_bool) {`
  forbidden check · 测试结果：
  AOT C typed scalar focused binary 1/0，exit=0；AOT C source contracts 19/0，exit=0；
  `backend_aot_c_scalar_stack_copy.c` 与 `backend_aot_c_lowering_control.c` focused
  对象编译通过；generated typed scalar product forbidden-token scan 中
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` 均为 0；marker scan 命中
  `zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);` 与 `if (!zr_aot_b5) {` · 备注：
  本记录不声明 CTest 或完整 WSL CMake/Ninja 通过；验证仍采用直接 WSL gcc/gold
  对象编译/链接临时 patched 静态库与生成 shared library 的方式。当前切片只关闭
  focused scalar stack-copy destination-local 镜像；系统性 C-local/frame-slot 镜像、
  GC 根登记、更多 branch 形态、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 19:41:43 +08:00 · M2 / 04-S3 unsigned u64 shift executable direct-sync 切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  在 `tests/parser/test_aot_c_typed_scalar.c` 追加 `var unsignedShifted: uint = unsignedSum << right;`
  与 `var unsignedShiftedBack: uint = unsignedShifted >> right;`，并把
  `<int> unsignedShiftedBack` 纳入返回值，要求生成物含 `zr_aot_scalar_exec_u64_shift`、
  `zr_aot_u13 = zr_aot_u8 << zr_aot_s1;` 与 `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;`。
  `compile_expression.c` 现在对 typed shift 保留左右操作数原始类型，不再按 unsigned
  结果类型把 shift-count 转成 `uint`；当任一有效操作数为 unsigned 时选择
  `BITWISE_SHIFT_LEFT` / `BITWISE_SHIFT_RIGHT`，使解释器与 AOT 共享 unsigned shift 语义。
  `tests/core/test_execution_unsigned_bitwise.c` 同步增加 unsigned bitwise shift left/right
  lower-layer 覆盖 · RED/GREEN：
  typed scalar golden 加入 unsigned shift 后先 RED 于解释器 `SHIFT_LEFT_INT` 的 signed-int
  断言；修正编译器 typed unsigned shift opcode 选择并同步当前 `compile_expression.c`
  对象到临时 parser 库后到 GREEN。因新增两个 unsigned 临时值，generated-product
  断言从旧 `s17/f17/s29/u29/f29/b25/b12/u31` 槽位同步到当前
  `u13/u14/s19/f19/f20/s31/u31/f31/f32/b27/b14/u33` 形态 · 测试结果：
  AOT C typed scalar focused binary 1/0，exit=0；unsigned bitwise core focused binary
  7/0，exit=0；AOT C source contracts 18/0，exit=0；`compile_expression.c`、
  `compiler_semir.c`、`backend_aot_c_scalar_bitwise.c` 与 `execution_dispatch.c`
  focused 对象编译通过；generated typed scalar product forbidden-token scan 中
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` 均为 0；marker scan 命中
  `zr_aot_scalar_exec_u64_shift`、`zr_aot_u13 = zr_aot_u8 << zr_aot_s1;`、
  `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;`、`zr_aot_scalar_exec_u64_bit_not` 与
  `zr_aot_u33 = ~zr_aot_u7;`；`git diff --check` 通过，仅报告既有 LF->CRLF 提示 ·
  备注：
  本记录不声明 CTest 或完整 WSL CMake/Ninja 通过；验证仍采用直接 WSL gcc/gold
  对象编译/链接临时 patched 静态库与生成 shared library 的方式。当前切片关闭 executable
  unsigned `u64` shift generated-product 覆盖；系统性 C-local/frame-slot 镜像、GC 根登记、
  更多 branch 形态、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 19:25:01 +08:00 · M2 / 04-S3 unsigned u64 bit-not executable direct-sync 切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  在 `tests/parser/test_aot_c_typed_scalar.c` 恢复执行型 `u64` bit-not golden，
  追加 `var unsignedInverted: uint = ~unsignedRight;` 并通过分支使用该值，要求生成物含
  `zr_aot_scalar_exec_u64_bit_not` 与 `zr_aot_u31 = ~zr_aot_u7;`，同时继续拒绝
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(`。验证中发现临时 parser 静态库若未同步
  当前 `backend_aot_c_scalar_*` 与 `compiler_semir.c` 对象，bitwise/bit-not 会回落到旧
  `zr_aot_bitwise_exec_unary` / `zr_aot_bitwise_exec_binary` 路径；同步当前对象后，SemIR
  typed bitwise 静态类型能命中 `u64` scalar writer，生成物进入直接 `uN` 表达式 · RED/GREEN：
  加入 `u64` bit-not generated-product marker 后，旧验证库先 RED 于旧通用 bitwise 输出和
  forbidden token；同步当前 scalar AOT backend 与 `compiler_semir.c` 后，typed scalar
  generated-product 到 GREEN，并因新增 `unsignedInverted` 临时值将后续 conversion/f64
  断言从 `s28/u28/f28` 与 `f29` 更新为当前 `s29/u29/f29` 与 `f30` · 测试结果：
  AOT C typed scalar focused binary 1/0，exit=0；unsigned bitwise core focused binary
  5/0，exit=0；AOT C source contracts 18/0，exit=0；`backend_aot_c_scalar_bitwise.c`、
  `compiler_semir.c` 与 `execution_dispatch.c` focused 对象编译通过；generated typed scalar product
  forbidden-token scan 中 `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` 均为 0；
  marker scan 命中 `zr_aot_scalar_exec_u64_bit_not`、`zr_aot_u31 = ~zr_aot_u7;`、
  `zr_aot_scalar_exec_u64_bitwise` 与 `zr_aot_u12 = zr_aot_u8 & zr_aot_u7;`；
  `git diff --check` 通过，仅报告既有 LF->CRLF 提示 · 备注：
  本记录不声明 CTest 或完整 WSL CMake/Ninja 通过；验证仍采用直接 WSL gcc/gold
  对象编译/链接临时 patched 静态库与生成 shared library 的方式。当前切片只关闭 executable
  unsigned `u64` bit-not golden；u64 shift 生成物执行覆盖、系统性 C-local/frame-slot 镜像、
  GC 根登记、更多 branch 形态、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 17:29:47 +08:00 · M2 / 04-S3 unsigned u64 binary bitwise executable direct-sync 恢复切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  采用 support-first 回归路径先修复解释器通用 unsigned bitwise 结果类型；`BITWISE_AND`、
  `BITWISE_OR`、`BITWISE_XOR` 和 `BITWISE_NOT` 在 unsigned operand 上现在保留
  `ZR_VALUE_TYPE_UINT64`，解除 typed scalar golden 加入 unsigned bitwise 后的下层阻塞。
  新增 `tests/core/test_execution_unsigned_bitwise.c` 并注册
  `zr_vm_execution_unsigned_bitwise_test`，覆盖 unsigned `&` / `|` / `^` / `~`
  解释器结果类型和 payload。恢复 `tests/parser/test_aot_c_typed_scalar.c` 中
  `var unsignedMasked: uint = unsignedSum & unsignedRight;` 执行型 golden，并要求生成物含
  `zr_aot_scalar_exec_u64_bitwise`、直接从 `frame.slotBase[8]` / `frame.slotBase[7]`
  同步到 `zr_aot_u8` / `zr_aot_u7`、以及 `zr_aot_u12 = zr_aot_u8 & zr_aot_u7;`。
  因新增 unsigned mask 临时值，typed scalar generated-product 的后续槽位断言同步更新为当前
  `b25`、`b12`、`f17/f18/f29`、`s10/s14`、`s13/s14/s17/s18` 与
  `s28/u28/f28` 形态 · RED/GREEN：
  新 lower-layer test 先在旧解释器上 RED，4 个 unsigned bitwise 用例均观察到结果 type 为
  `INT64` 而非期望 `UINT64`；修复解释器 bitwise 结果写入后该 test 到 GREEN。typed scalar
  golden 恢复 `unsignedMasked` 后先因旧生成物槽位断言 RED；同步当前 generated-product
  marker 后到 GREEN · 测试结果：
  `tests/core/test_execution_unsigned_bitwise.c` focused binary 4/0，exit=0；
  AOT C typed scalar focused binary 1/0，exit=0；AOT C source contracts 18/0，exit=0；
  `execution_dispatch.c` WSL gcc focused 对象编译 exit=0（仅既有 warning）；
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译 exit=0 · 备注：
  本记录不声明 CTest 或完整 WSL CMake/Ninja 通过；验证仍采用直接 WSL gcc/gold
  对象编译/链接。typed scalar 运行使用临时 patched `libzr_vm_core.a`，因为静态构建目录未完整重建。
  本切片只关闭 executable unsigned `u64` binary bitwise golden；u64 bit-not/shift 生成物执行覆盖、
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 16:58:41 +08:00 · M2 / 04-S3 unsigned u64 shift operand direct-sync 模板切片 · 状态：
  源码模板子切片完成、生成物执行覆盖受 unsigned bitwise/shift 执行路径限制、04-S3 部分完成、M2 部分完成 ·
  完成项目：
  `backend_aot_write_c_scalar_u64_shift()` 现在接收 `SZrAotExecIrFunction`，并在 left 有
  `u64` scalar-local、shift-count 有 signed `i64` scalar-local、destination 有 `u64`
  scalar-local 覆盖时，直接从 frame slot 同步到 `zr_aot_uLeft` / `zr_aot_sShift`，
  用 `zr_aot_sShift` 做 shift-count 范围检查，再发射 `zr_aot_uDst = zr_aot_uLeft << zr_aot_sShift`
  或 `zr_aot_uDst = zr_aot_uLeft >> zr_aot_sShift`。未证明 operand-local 的 fallback
  仍保留 `zr_aot_u_left` / `zr_aot_s_shift` 临时变量模板 · RED/GREEN：
  source-contract 先要求 `backend_aot_write_c_scalar_u64_shift(FILE *file,`、
  `zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;`、
  `zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;`、
  `zr_aot_u%u = zr_aot_u%u << zr_aot_s%u;` 与
  `zr_aot_u_result = zr_aot_u%u << zr_aot_s%u;`，RED 于缺少 u64 shift direct-local
  表达式。改写 u64 shift 模板并把 functionIr 传入后到 GREEN · 测试结果：
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译通过；AOT C source contracts
  18/0，exit=0；AOT C typed scalar focused binary 回归 1/0，exit=0 · 备注：
  本记录不声明 CTest 通过，也不声明 generated-product 覆盖完成；验证仍采用直接 WSL gcc/gold
  对象编译/链接既有静态库与源码契约检查。u64 shift 生成物执行覆盖、系统性
  C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 16:53:34 +08:00 · M2 / 04-S3 unsigned u64 bit-not source direct-sync 模板切片 · 状态：
  源码模板子切片完成、生成物执行覆盖受解释器 unsigned bitwise 断言阻塞、04-S3 部分完成、M2 部分完成 ·
  完成项目：
  `backend_aot_write_c_scalar_u64_bit_not()` 现在接收 `SZrAotExecIrFunction`，并在 source
  有 `u64` scalar-local 覆盖时，直接从
  `frame.slotBase[source].value.value.nativeObject.nativeUInt64` 同步到 `zr_aot_uSource`，
  随后发射 `zr_aot_uDst = ~zr_aot_uSource` 或 `zr_aot_u_result = ~zr_aot_uSource`。
  未证明 source-local 的 fallback 仍保留 `zr_aot_u_source` 临时变量模板 · RED/GREEN：
  source-contract 先要求 `backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot)`、
  `zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;`、
  `zr_aot_u%u = ~zr_aot_u%u;` 与 `zr_aot_u_result = ~zr_aot_u%u;`，RED 于缺少
  u64 source-local 查询。改写 u64 bit-not 模板并把 functionIr 传入后到 GREEN ·
  测试结果：
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译通过；AOT C source contracts
  18/0，exit=0 · 备注：
  本记录不声明 CTest 通过，也不声明 generated-product 覆盖完成；验证仍采用直接 WSL gcc/gold
  对象编译/链接既有静态库与源码契约检查。u64 bit-not 生成物执行覆盖、u64 shift、
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 16:50:01 +08:00 · M2 / 04-S3 unsigned u64 binary bitwise source direct-sync 模板切片 · 状态：
  源码模板子切片完成、生成物执行覆盖受解释器 unsigned bitwise 断言阻塞、04-S3 部分完成、M2 部分完成 ·
  完成项目：
  `backend_aot_write_c_scalar_u64_bitwise()` 现在接收 `SZrAotExecIrFunction`，并在 destination /
  left / right 都有 `u64` scalar-local 覆盖时，直接从
  `frame.slotBase[left].value.value.nativeObject.nativeUInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeUInt64` 同步到 `zr_aot_uLeft` /
  `zr_aot_uRight`，随后发射 `zr_aot_uDst = zr_aot_uLeft op zr_aot_uRight` 并通过
  `zr_aot_u_result = zr_aot_uDst` 镜像回 frame slot。未证明 scalar-local 的 fallback
  仍保留 `zr_aot_u_left` / `zr_aot_u_right` 临时变量模板 · RED/GREEN：
  source-contract 先要求 `backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)`、
  `backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot)`、
  `backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot)`、
  `zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;`、
  `zr_aot_u%u = zr_aot_u%u %s zr_aot_u%u;` 与 `zr_aot_u_result = zr_aot_u%u;`，
  RED 于缺少 u64 scalar-local 查询。改写 u64 bitwise 模板并把 functionIr 传入后到 GREEN ·
  测试结果：
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译通过；AOT C source contracts
  18/0，exit=0；尝试把 `var unsignedMasked: uint = unsignedSum & unsignedRight;` 加入
  执行型 typed scalar golden 时，解释器在 `ZrCore_Execute` 的 unsigned bitwise operand
  断言处 abort，探针已撤回，未纳入本切片验收 · 备注：
  本记录不声明 CTest 通过，也不声明 generated-product 覆盖完成；验证仍采用直接 WSL gcc/gold
  对象编译/链接既有静态库与源码契约检查。u64 bitwise 生成物执行覆盖、u64 shift/bit-not、
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 16:40:37 +08:00 · M2 / 04-S3 signed i64 bit-not source frame-slot-to-sN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed signed `i64` bit-not 在 source operand 有 `i64` scalar-local 覆盖时，
  不再先把 frame-slot 输入写入 `zr_aot_s_source` 再同步到 `zr_aot_sN`；该 proven-local
  路径现在直接从 `frame.slotBase[source].value.value.nativeObject.nativeInt64` 同步到
  `zr_aot_sSource`，随后发射 `zr_aot_sDst = ~zr_aot_sSource` 或
  `zr_aot_s_result = ~zr_aot_sSource`。未证明 source-local 的 fallback 仍保留
  `zr_aot_s_source` 临时变量模板 · RED/GREEN：
  先在 generated-product 断言中拒绝 `inverted = ~right` path 的旧
  `zr_aot_s1 = zr_aot_s_source;` 同步；source-contract RED 于命中该旧同步模板。
  改写 signed i64 bit-not scalar-local 模板后到 GREEN · 测试结果：
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译通过；
  AOT C source contracts 18/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  generated typed scalar product marker scan 命中 `zr_aot_scalar_exec_i64_bit_not`、
  `zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_s16 = ~zr_aot_s1;`，旧 `zr_aot_s1 = zr_aot_s_source;` 为 0；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused signed `i64` bit-not source 的 frame-slot-to-`sN` direct sync；
  u64 bitwise/shift/bit-not、系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 16:33:22 +08:00 · M2 / 04-S3 signed i64 shift operand frame-slot-to-sN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed signed `i64` shift 在 left / shift-count operand 都有 `i64` scalar-local
  覆盖时，不再先把 frame-slot 输入写入 `zr_aot_s_left` / `zr_aot_s_shift` 再同步到
  `zr_aot_sN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeInt64` 同步到 `zr_aot_sLeft` /
  `zr_aot_sShift`，随后用 `zr_aot_sShift` 做 shift-count 范围检查并发射
  `zr_aot_sDst = ... zr_aot_sLeft ... zr_aot_sShift`。未证明 scalar-local 的 fallback
  仍保留 `zr_aot_s_left` / `zr_aot_s_shift` 临时变量模板 · RED/GREEN：
  先在 generated-product 断言中要求 `shifted = toggled << right` /
  `shiftedBack = shifted >> right` path 出现
  `zr_aot_s12 = frame.slotBase[12].value.value.nativeObject.nativeInt64;`、
  `zr_aot_s13 = frame.slotBase[13].value.value.nativeObject.nativeInt64;`、
  `if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {`，并拒绝旧
  `zr_aot_s12 = zr_aot_s_left;` / `zr_aot_s13 = zr_aot_s_left;` /
  `zr_aot_s1 = zr_aot_s_shift;` 同步；source-contract RED 于缺少 direct
  shift-count `sN` range-check 模板。改写 signed i64 shift scalar-local 模板后到 GREEN ·
  测试结果：
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译通过；
  AOT C source contracts 18/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  generated typed scalar product marker scan 命中
  `zr_aot_s12 = frame.slotBase[12].value.value.nativeObject.nativeInt64;`、
  `zr_aot_s13 = frame.slotBase[13].value.value.nativeObject.nativeInt64;`、
  `if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {`、
  `zr_aot_s16 = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);` 与
  `zr_aot_s17 = zr_aot_s13 >> zr_aot_s1;`，旧 `zr_aot_s12 = zr_aot_s_left;` /
  `zr_aot_s13 = zr_aot_s_left;` / `zr_aot_s1 = zr_aot_s_shift;` 均为 0；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused signed `i64` shift operand 的 frame-slot-to-`sN` direct sync；
  u64 bitwise/shift、bit-not source direct-sync、系统性 C-local/frame-slot 镜像、GC 根登记、
  更多 branch 形态、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge
  仍未完成，不能声明 M2 完成。

- 2026-06-20 16:19:22 +08:00 · M2 / 04-S3 signed i64 binary bitwise frame-slot-to-sN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed signed `i64` binary bitwise 在 destination / left / right 都有 `i64`
  scalar-local 覆盖时，不再先把 frame-slot 输入写入 `zr_aot_s_left` / `zr_aot_s_right`
  再同步到 `zr_aot_sN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeInt64` 同步到 `zr_aot_sLeft` /
  `zr_aot_sRight`，随后发射 `zr_aot_sDst = zr_aot_sLeft op zr_aot_sRight` 并继续镜像回
  frame slot。未证明 scalar-local 的 fallback 仍保留 `zr_aot_s_left` /
  `zr_aot_s_right` 临时变量模板；shift 与 bit-not 的 direct-sync 收敛仍留给后续切片 ·
  RED/GREEN：
  先在 generated-product 断言中要求 `masked = maskBase & left` path 出现
  `zr_aot_s9 = frame.slotBase[9].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_s13 = zr_aot_s9 & zr_aot_s0;`，并拒绝旧
  `zr_aot_s9 = zr_aot_s_left;` / `zr_aot_s0 = zr_aot_s_right;` 同步；source-contract
  RED 于缺少 direct frame-slot-to-`sN` bitwise 模板。改写 signed i64 bitwise
  scalar-local 模板并移除源契约中旧二元位运算临时同步必需项后到 GREEN ·
  测试结果：
  `backend_aot_c_scalar_bitwise.c` WSL gcc focused 对象编译通过；
  AOT C source contracts 18/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  generated typed scalar product marker scan 命中
  `zr_aot_s9 = frame.slotBase[9].value.value.nativeObject.nativeInt64;`、
  `zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_s13 = zr_aot_s9 & zr_aot_s0;`，旧
  `zr_aot_s9 = zr_aot_s_left;` / `zr_aot_s0 = zr_aot_s_right;` 均为 0；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused signed `i64` binary bitwise 的 frame-slot-to-`sN` direct sync；
  u64 bitwise、shift/bit-not operand direct-sync、系统性 C-local/frame-slot 镜像、GC 根登记、
  更多 branch 形态、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge
  仍未完成，不能声明 M2 完成。

- 2026-06-20 16:04:22 +08:00 · M2 / 04-S3 unsigned u64 scalar compare frame-slot-to-uN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed unsigned `u64` scalar compare 在 left/right 都有 `u64` scalar-local
  覆盖时，不再先把 frame-slot 输入写入 `zr_aot_u_left` / `zr_aot_u_right` 再同步到
  `zr_aot_uN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeUInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeUInt64` 同步到 `zr_aot_uLeft` /
  `zr_aot_uRight`，随后按 destination 是否有 bool local 发射
  `zr_aot_bDst = (TZrBool)(zr_aot_uLeft op zr_aot_uRight)` 或
  `zr_aot_u_result = (TZrBool)(zr_aot_uLeft op zr_aot_uRight)`。未证明 operand-local 的
  fallback 仍保留 `zr_aot_u_left` / `zr_aot_u_right` 临时变量模板 · RED/GREEN：
  先在 generated-product 断言中要求 `unsignedSum > unsignedRight` compare 出现
  `zr_aot_u8 = frame.slotBase[8].value.value.nativeObject.nativeUInt64;`，并拒绝旧
  `zr_aot_u8 = zr_aot_u_left;` / `zr_aot_u7 = zr_aot_u_right;` 同步；source-contract
  RED 于缺少 direct frame-slot-to-`uN` 模板。改写 unsigned compare scalar-local 模板后到 GREEN ·
  测试结果：
  AOT C source contracts 18/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  `backend_aot_c_scalar_semir.c` `-Wformat -Werror=format-extra-args` focused 编译检查通过；
  generated typed scalar product marker scan 命中
  `zr_aot_u8 = frame.slotBase[8].value.value.nativeObject.nativeUInt64;`、
  `zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;` 与
  `zr_aot_b11 = (TZrBool)(zr_aot_u8 > zr_aot_u7);`，旧
  `zr_aot_u8 = zr_aot_u_left;` / `zr_aot_u7 = zr_aot_u_right;` 均为 0；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0；
  `git diff --check` 通过，仅报告既有 LF->CRLF 提示 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused unsigned `u64` scalar compare 的 frame-slot-to-`uN` direct sync；
  bitwise compare-like paths、系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 15:51:53 +08:00 · M2 / 04-S3 signed i64 scalar compare frame-slot-to-sN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed signed `i64` scalar compare 在 destination bool / left `i64` / right `i64`
  都有 scalar-local 覆盖时，不再先把 frame-slot 输入写入 `zr_aot_s_left` /
  `zr_aot_s_right` 再同步到 `zr_aot_sN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeInt64` 同步到 `zr_aot_sLeft` /
  `zr_aot_sRight`，随后发射 `zr_aot_bDst = (TZrBool)(zr_aot_sLeft op zr_aot_sRight)`。
  未证明 scalar-local 的 fallback 仍保留 `zr_aot_s_left` / `zr_aot_s_right` 临时变量模板 ·
  RED/GREEN：
  先在 generated-product 断言中要求 `product > threshold` compare 出现
  `zr_aot_s2 = frame.slotBase[2].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_s4 = frame.slotBase[4].value.value.nativeObject.nativeInt64;`，并拒绝旧
  `zr_aot_s2 = zr_aot_s_left;` / `zr_aot_s4 = zr_aot_s_right;` 同步；source-contract
  RED 于缺少 direct frame-slot-to-`sN` 模板。改写 signed compare scalar-local 模板后到 GREEN ·
  测试结果：
  AOT C source contracts 18/0，exit=0；AOT C typed scalar focused binary 1/0，exit=0；
  `backend_aot_c_scalar_semir.c` `-Wformat -Werror=format-extra-args` focused 编译检查通过；
  generated typed scalar product marker scan 命中
  `zr_aot_s2 = frame.slotBase[2].value.value.nativeObject.nativeInt64;`、
  `zr_aot_s4 = frame.slotBase[4].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_b24 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`，旧
  `zr_aot_s2 = zr_aot_s_left;` / `zr_aot_s4 = zr_aot_s_right;` 均为 0；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0；
  `git diff --check` 通过，仅报告既有 LF->CRLF 提示 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused signed `i64` scalar compare 的 frame-slot-to-`sN` direct sync；
  unsigned compare、bitwise compare-like paths、系统性 C-local/frame-slot 镜像、GC 根登记、
  更多 branch 形态、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 15:34:30 +08:00 · M2 / 04-S3 scalar binary lowering module split 维护切片 · 状态：
  子切片完成、04-S3 可维护性推进、M2 部分完成 · 完成项目：
  将 `backend_aot_c_scalar_semir.c` 中的 signed `i64` / unsigned `u64` / `f64`
  binary arithmetic lowering、binary operand decode、direct frame-slot-to-`sN` /
  `uN` / `fN` sync、divide/modulo guards 与 plain scalar result writers 提取到
  `backend_aot_c_scalar_binary.{h,c}`；`backend_aot_c_scalar_semir.c` 现在保留为
  conversion -> bitwise -> binary -> unsigned compare -> signed compare 的小调度器与
  compare lowering 表面，文件行数从约 1099 行降到 416 行，避免继续在 oversized
  dispatcher 中堆叠 04-S3 逻辑。`tests/parser/test_aot_c_source_contracts.c` 同步读取
  dispatcher 与 binary module，锁定 binary arithmetic 后续应落在新模块；`docs/parser-and-semantics/csharp-value-type-semir-aot.md`
  同步补入新模块文件清单和边界说明 · RED/GREEN：
  本切片是验证后结构性重构，不新增行为 RED；保留前一轮 `i64` / `u64` / `f64`
  generated-product guardrails，并在 source-contract 中补锁新模块边界后验证到 GREEN ·
  测试结果：
  WSL gcc focused 编译 `backend_aot_c_scalar_binary.c` 与拆分后的
  `backend_aot_c_scalar_semir.c` 通过；AOT C typed scalar focused binary 1/0，exit=0；
  AOT C source contracts 18/0，exit=0；generated typed scalar product marker scan 仍命中
  `zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;`、
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`、
  `zr_aot_u6 = frame.slotBase[6].value.value.nativeObject.nativeUInt64;`、
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`、
  `zr_aot_f16 = frame.slotBase[16].value.value.nativeObject.nativeDouble;` 与
  `zr_aot_f28 = zr_aot_f16 * zr_aot_f17;`；旧
  `zr_aot_s0 = zr_aot_s_left;` / `zr_aot_u6 = zr_aot_u_left;` /
  `zr_aot_f16 = zr_aot_f_left;` 均为 0；`ZrCore_Stack_GetValue(` 与
  `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0；`git diff --check` 通过，仅报告既有
  LF->CRLF 提示 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片不改变 binary arithmetic 行为，只关闭继续扩张 oversized dispatcher 的维护风险。
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 15:07:27 +08:00 · M2 / 04-S3 f64 binary frame-slot-to-fN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed `f64` binary arithmetic 在 destination / left / right 都有 `f64`
  scalar-local 覆盖时，不再先把 left/right frame-slot 输入写入 `zr_aot_f_left` /
  `zr_aot_f_right` 再同步到 `zr_aot_fN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeDouble` 与
  `frame.slotBase[right].value.value.nativeObject.nativeDouble` 同步到 `zr_aot_fLeft` /
  `zr_aot_fRight`，随后发射 `zr_aot_fDst = zr_aot_fLeft op zr_aot_fRight` 或
  `fmod(zr_aot_fLeft, zr_aot_fRight)` 并继续镜像回 frame slot。未证明 scalar-local 的
  fallback 仍保留 `zr_aot_f_left` / `zr_aot_f_right` 临时变量模板 · RED/GREEN：
  先加入 generated-product 断言，要求首个 `floatLeft * floatRight` lowering 出现
  `zr_aot_f16 = frame.slotBase[16].value.value.nativeObject.nativeDouble;` 与
  `zr_aot_f17 = frame.slotBase[17].value.value.nativeObject.nativeDouble;`，并拒绝旧
  `zr_aot_f16 = zr_aot_f_left;` 同步；旧模板下 focused typed scalar 失败于缺少 direct
  left-slot sync。改写 `f64` binary scalar-local 模板并同步源码契约 needle 后到 GREEN ·
  测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过；generated typed scalar
  product marker scan 命中 `zr_aot_f16 = frame.slotBase[16].value.value.nativeObject.nativeDouble;`、
  `zr_aot_f17 = frame.slotBase[17].value.value.nativeObject.nativeDouble;` 与
  `zr_aot_f28 = zr_aot_f16 * zr_aot_f17;`，旧 `zr_aot_f16 = zr_aot_f_left;` 未命中；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused `f64` binary arithmetic 的 frame-slot-to-`fN` direct sync；
  `backend_aot_c_scalar_semir.c` 已接近 1100 行，后续若继续扩大责任应优先拆分。
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 14:51:09 +08:00 · M2 / 04-S3 unsigned u64 binary frame-slot-to-uN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed unsigned `u64` binary arithmetic 在 destination / left / right 都有 `u64`
  scalar-local 覆盖时，不再先把 left/right frame-slot 输入写入 `zr_aot_u_left` /
  `zr_aot_u_right` 再同步到 `zr_aot_uN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeUInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeUInt64` 同步到 `zr_aot_uLeft` /
  `zr_aot_uRight`，随后发射 `zr_aot_uDst = zr_aot_uLeft op zr_aot_uRight` 并继续镜像回
  frame slot。未证明 scalar-local 的 fallback 仍保留 `zr_aot_u_left` /
  `zr_aot_u_right` 临时变量模板 · RED/GREEN：
  先加入 generated-product 断言，要求首个 `unsignedLeft + unsignedRight` lowering 出现
  `zr_aot_u6 = frame.slotBase[6].value.value.nativeObject.nativeUInt64;` 与
  `zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;`，并拒绝旧
  `zr_aot_u6 = zr_aot_u_left;` 同步；旧模板下 focused typed scalar 失败。改写
  unsigned `u64` binary scalar-local 模板并同步源码契约 needle 后到 GREEN · 测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过；generated typed scalar
  product marker scan 命中 `zr_aot_u6 = frame.slotBase[6].value.value.nativeObject.nativeUInt64;`、
  `zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;` 与
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`，旧 `zr_aot_u6 = zr_aot_u_left;` 未命中；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused unsigned `u64` binary arithmetic 的 frame-slot-to-`uN` direct sync；
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 14:39:25 +08:00 · M2 / 04-S3 signed i64 binary frame-slot-to-sN direct sync 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  SemIR-backed signed `i64` binary arithmetic 在 destination / left / right 都有 `i64`
  scalar-local 覆盖时，不再先把 left/right frame-slot 输入写入 `zr_aot_s_left` /
  `zr_aot_s_right` 再同步到 `zr_aot_sN`；该 proven-local 路径现在直接从
  `frame.slotBase[left].value.value.nativeObject.nativeInt64` 与
  `frame.slotBase[right].value.value.nativeObject.nativeInt64` 同步到 `zr_aot_sLeft` /
  `zr_aot_sRight`，随后发射 `zr_aot_sDst = zr_aot_sLeft op zr_aot_sRight` 并继续镜像回
  frame slot。未证明 scalar-local 的 fallback 仍保留 `zr_aot_s_left` /
  `zr_aot_s_right` 临时变量模板 · RED/GREEN：
  先加入 generated-product 断言，要求首个 `left * right` lowering 出现
  `zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;`，并拒绝旧
  `zr_aot_s0 = zr_aot_s_left;` 同步；旧模板下 focused typed scalar 失败。改写
  signed `i64` binary scalar-local 模板并同步源码契约 needle 后到 GREEN · 测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过；generated typed scalar
  product marker scan 命中 `zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;`、
  `zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;` 与
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`，旧 `zr_aot_s0 = zr_aot_s_left;` 未命中；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc/gold 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused signed `i64` binary arithmetic 的 frame-slot-to-`sN` direct sync；
  系统性 C-local/frame-slot 镜像、GC 根登记、更多 branch 形态、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 14:11:57 +08:00 · M2 / 04-S2-04-S3 signed slot-branch direct sN sync 第一切片 · 状态：
  子切片完成、04-S2 部分完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  fused signed slot/slot branch 在 left/right operand 都有 `i64` scalar-local 覆盖时，
  不再先写 `zr_aot_left_scalar` / `zr_aot_right_scalar` 再同步到 `zr_aot_sN`，而是直接从
  `zr_aot_left->value.nativeObject.nativeInt64` 与 `zr_aot_right->value.nativeObject.nativeInt64`
  同步到 `zr_aot_sLeft` / `zr_aot_sRight`，再发射 `if (zr_aot_sLeft op zr_aot_sRight) { goto ...; }`。
  未证明 scalar-local 的 fallback 仍保留原 `zr_aot_left_scalar` / `zr_aot_right_scalar` 临时变量模板 ·
  RED/GREEN：
  先加入 `zr_aot_s2 = zr_aot_left->value.nativeObject.nativeInt64;`、
  `zr_aot_s4 = zr_aot_right->value.nativeObject.nativeInt64;` 以及旧
  `zr_aot_s2 = zr_aot_left_scalar;` / `zr_aot_s4 = zr_aot_right_scalar;`
  不得出现的 generated-product 断言；旧模板下 focused typed scalar 失败于缺少 direct right-slot sync。
  改写 signed slot/slot branch scalar-local 模板并同步源码契约 needle 后到 GREEN · 测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_lowering_control.c`
  focused 编译检查通过；generated typed scalar product marker scan 命中
  `zr_aot_s2 = zr_aot_left->value.nativeObject.nativeInt64;` 与
  `zr_aot_s4 = zr_aot_right->value.nativeObject.nativeInt64;`，旧
  `zr_aot_s2 = zr_aot_left_scalar;` / `zr_aot_s4 = zr_aot_right_scalar;` 未命中；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused fused signed slot/slot branch 的 direct `sN` 同步；
  更多 branch 形态、系统性 C-local/frame-slot 镜像、GC 根登记、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 13:53:39 +08:00 · M2 / 04-S2-04-S3 bool branch condition-local predicate 第一切片 · 状态：
  子切片完成、04-S2 部分完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `JUMP_IF_BOOL_FALSE` 的 AOT C lowering 现在接收 `functionIr`，并在 condition slot
  有 `bool` scalar-local 覆盖时，把 frame-slot bool 同步到 `zr_aot_bN`，再发射
  `if (!zr_aot_bN) { goto ...; }`；fallback 仍保留 `zr_aot_condition_bool` 临时谓词用于
  未证明 bool scalar-local 的路径。`backend_aot_c_emitter.h` 与
  `backend_aot_c_function_body.c` 同步接线 `SZrAotExecIrFunction` 参数；
  focused generated-product 锁定 `branchFlag` 条件分支使用 `zr_aot_b5` 谓词 · RED/GREEN：
  改动前生成物扫描显示 bool false 分支仍命中 `if (!zr_aot_condition_bool) {`，
  缺少 `if (!zr_aot_b5) {`；加入 generated-product 断言后该形态被视为失败。
  接入 bool branch condition-local 模板后到 GREEN · 测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_lowering_control.c`
  与 `backend_aot_c_function_body.c` focused 编译检查通过；generated typed scalar product
  marker scan 命中 `zr_aot_b5 = (TZrBool)(zr_aot_condition->value.nativeObject.nativeBool != 0u);`
  与 `if (!zr_aot_b5) {`，旧 `if (!zr_aot_condition_bool) {` 未命中；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused `JUMP_IF_BOOL_FALSE` condition-local predicate；更多 branch 形态、
  系统性 C-local/frame-slot 镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 13:30:16 +08:00 · M2 / 04-S2-04-S3 fused signed const branch left-local predicate 第一切片 · 状态：
  子切片完成、04-S2 部分完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_lowering_control.c` 的 `JUMP_IF_NOT_EQUAL_SIGNED_CONST` lowering 现在接收
  `functionIr`，并在 left operand 有 `i64` scalar-local 覆盖时，把 frame-slot 输入同步到
  `zr_aot_sN`，再发射 `if (zr_aot_sN != zr_aot_right_literal) { goto ...; }`；
  fallback 仍保留原 `zr_aot_left_scalar` 模板用于未证明 scalar-local 的路径。
  `backend_aot_c_emitter.h` 与 `backend_aot_c_function_body.c` 同步接线
  `SZrAotExecIrFunction` 参数；focused generated-product 用 `if (product == 42)` 覆盖
  quickening 产生的 `JUMP_IF_NOT_EQUAL_SIGNED_CONST` · RED/GREEN：
  先加入 `zr_aot_s2 = zr_aot_left->value.nativeObject.nativeInt64;`、
  `if (zr_aot_s2 != zr_aot_right_literal) {` 以及旧
  `if (zr_aot_left_scalar != zr_aot_right_literal) {` 不得出现的 generated-product 断言；
  旧 const branch 路径仍使用 helper predicate 时失败。接入 const branch left-local 模板后到 GREEN ·
  测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；generated typed scalar product marker scan
  命中 `zr_aot_s2 = zr_aot_left->value.nativeObject.nativeInt64;` 与
  `if (zr_aot_s2 != zr_aot_right_literal) {`，旧
  `if (zr_aot_left_scalar != zr_aot_right_literal) {` 未命中；
  `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` forbidden-token scan 均为 0 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 focused `JUMP_IF_NOT_EQUAL_SIGNED_CONST` left-local predicate；更多 branch 形态、
  系统性 C-local/frame-slot 镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 12:52:21 +08:00 · M2 / 04-S3 typed conversion destination-local 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.c` 现在会扫描 `SZrFunction::instructionsList`，
  将 focused typed numeric conversion opcode 的 destination slot 映射为对应的
  `i64` / `u64` / `f64` scalar local kind；因此同一个转换临时槽可以声明
  `zr_aot_s27`、`zr_aot_u27`、`zr_aot_f27` 三类本地镜像。既有
  `backend_aot_c_scalar_conversion.c` destination-local 模板随之生效，
  generated typed scalar product 对 `<float> product`、`<int> floatLeft`、
  `<int> unsignedSum`、`<uint> product`、`<uint> floatLeft` 发射
  `zr_aot_f27 = ...`、`zr_aot_s27 = ...`、`zr_aot_u27 = ...`，
  再同步到既有 result/frame slot · RED/GREEN：
  先加入 `TZrInt64 zr_aot_s27 = (TZrInt64)0;`、
  `TZrUInt64 zr_aot_u27 = (TZrUInt64)0u;`、`TZrFloat64 zr_aot_f27 = 0.0;`
  以及 `zr_aot_f27` / `zr_aot_s27` / `zr_aot_u27` conversion expression
  generated-product 断言后，focused typed scalar 在旧 locals 对象覆盖链接下失败；
  扫描 conversion opcode destination slot 并合并到 scalar-local bitmask 后到 GREEN · 测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_scalar_locals.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过；generated typed scalar product
  forbidden-token scan 中 `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` 均为 0，
  并命中 `TZrInt64 zr_aot_s27 = (TZrInt64)0;`、
  `TZrUInt64 zr_aot_u27 = (TZrUInt64)0u;`、`TZrFloat64 zr_aot_f27 = 0.0;`、
  `zr_aot_f27 = (TZrFloat64)zr_aot_s2;`、`zr_aot_s27 = (TZrInt64)zr_aot_f16;`、
  `zr_aot_s27 = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u8 - zr_aot_limit);`、
  `zr_aot_u27 = (TZrUInt64)zr_aot_s2;`、`zr_aot_u27 = (TZrUInt64)zr_aot_f16;` · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成物检查。
  为避免继续扩大已超过 1600 行的 `tests/parser/test_aot_c_source_contracts.c`，本切片使用
  focused generated-product RED/GREEN 覆盖行为，source-contract 套件保持回归运行。本切片只关闭
  focused numeric conversion destination-local 覆盖；更多 branch 形态、系统性的 C-local/frame-slot 镜像、
  GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 12:28:28 +08:00 · M2 / 04-S3 scalar local 多类型槽声明与 signed i64 shift/bit-not destination-local 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.c` 将每个 frame slot 的 scalar local kind 从互斥 enum 改为 bitmask，
  `typedLocalBindings` 与 SemIR destination static-C-type metadata 现在可以共同记录同一 slot 的
  `bN` / `sN` / `uN` / `fN` 镜像声明；生成物可以同时声明复用槽上的
  `zr_aot_s16` / `zr_aot_f16` 与 `zr_aot_s17` / `zr_aot_f17`。
  signed `i64` shift / bit-not 在 SemIR destination 证明临时目标槽为 `i64` 时，
  现在能写入 `zr_aot_s16 = ...`、`zr_aot_s17 = ...`、`zr_aot_s16 = ~...`，
  再镜像回既有 frame slot；source contracts 锁定 scalar-local kind bitmask OR 与
  `(kind & expectedKind) == expectedKind` 查询语义；typed scalar generated-product 断言同步更新
  unsigned compare bool-destination marker 为 `zr_aot_b11 = ...` · RED/GREEN：
  先加入 `TZrInt64 zr_aot_s16 = (TZrInt64)0;`、`TZrInt64 zr_aot_s17 = (TZrInt64)0;`、
  `zr_aot_s16 = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);`、
  `zr_aot_s17 = zr_aot_s13 >> zr_aot_s1;`、`zr_aot_s16 = ~zr_aot_s1;`
  generated-product 断言后，focused typed scalar 在对象覆盖链接下失败；
  改为多类型槽声明并让 signed shift/bit-not 选择 `sN` destination 后到 GREEN · 测试结果：
  AOT C typed scalar 1/0；AOT C source contracts 18/0；`backend_aot_c_scalar_locals.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过；generated typed scalar product
  forbidden-token scan 中 `ZrCore_Stack_GetValue(` 与 `ZR_VALUE_FAST_SET(` 均为 0，
  并命中 `TZrInt64 zr_aot_s16 = (TZrInt64)0;`、`TZrInt64 zr_aot_s17 = (TZrInt64)0;`、
  `zr_aot_s16 = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);`、
  `zr_aot_s17 = zr_aot_s13 >> zr_aot_s1;`、`zr_aot_s16 = ~zr_aot_s1;`、
  `zr_aot_b11 = (TZrBool)(zr_aot_u8 > zr_aot_u7);` · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成物检查。
  本切片只覆盖 multi-kind scalar local declaration 与第一批 signed `i64` shift/bit-not destination locals；
  conversion destination-local、更多 branch 形态、C-local/frame-slot 系统性镜像、GC 根登记、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 12:11:13 +08:00 · M2 / 04-S3 typed signed branch 使用 C 局部 operand 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_lowering_control.c` 的 fused signed branch lowering 现在接收 `functionIr`，
  对 left/right 执行 `backend_aot_c_scalar_locals_has_i64_slot` 查询；两侧 operand 均有
  `i64` scalar local 声明时，会先从 frame slot 验证并同步到 `zr_aot_sN`，再发射
  `if (zr_aot_sLeft op zr_aot_sRight) { goto ...; }`。`backend_aot_c_emitter.h`
  为 signed branch helper 暴露 `SZrAotExecIrFunction` 前置声明与参数，
  `backend_aot_c_function_body.c` 将当前 `functionIr` 传入三类非 const signed branch helper；
  `tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `if (zr_aot_s2 <= zr_aot_s4) {`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 signed branch 对 `i64` operand local 查询与
  `sN` branch predicate 模板的依赖 · RED/GREEN：
  先加入 `if (zr_aot_s2 <= zr_aot_s4) {` generated-product 断言后，
  focused typed scalar 测试失败；接入 signed branch operand-local 模板后到 GREEN，
  生成物 forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + signed branch operands + u64 compare operands
  + TO_UINT/TO_UINT_SIGNED/TO_UINT_FLOAT/TO_FLOAT/TO_INT_FLOAT/TO_INT_UNSIGNED local source
  + u64/f64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `if (zr_aot_s2 <= zr_aot_s4) {` 且旧 `if (zr_aot_left_scalar <= zr_aot_right_scalar) {`
  为 0；AOT C source contracts 18/0；`backend_aot_c_lowering_control.c` 与
  `backend_aot_c_function_body.c` `-Wformat -Werror=format-extra-args`
  focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 fused signed branch 的第一批 `sN` operand 表达式；
  完整 shift/bit-not/conversion destination-local 覆盖、更多 branch 形态、
  C-local/frame-slot 系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 11:49:16 +08:00 · M2 / 04-S3 typed u64 compare 使用 C 局部 operand 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_semir.c` 的 unsigned `u64` compare lowering 接收 `functionIr`，
  将 operand-local 与 destination-local eligibility 拆开：left/right 均有 `u64` scalar local
  声明时，会把 frame-slot 输入同步到 `zr_aot_uN`，并从 `zr_aot_uLeft` / `zr_aot_uRight`
  发射比较表达式；destination 也有 `bool` scalar local 时才写 `zr_aot_bDst`，
  否则保持 `zr_aot_u_result` 作为 bool 结果镜像变量；`tests/parser/test_aot_c_typed_scalar.c`
  断言生成物包含 `zr_aot_u_result = (TZrBool)(zr_aot_u8 > zr_aot_u7);`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 unsigned compare 对 `u64` operand local 查询与
  `uN` operand 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_u_result = (TZrBool)(zr_aot_u8 > zr_aot_u7);` generated-product 断言后，
  focused typed scalar 测试失败；接入 unsigned `u64` compare operand-local 模板后到 GREEN，
  生成物 forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + u64 compare operands
  + TO_UINT/TO_UINT_SIGNED/TO_UINT_FLOAT/TO_FLOAT/TO_INT_FLOAT/TO_INT_UNSIGNED local source
  + u64/f64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_u_result = (TZrBool)(zr_aot_u8 > zr_aot_u7);`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_semir.c` `-Wformat -Werror=format-extra-args`
  focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 unsigned `u64` compare 的第一批 `uN` operand 表达式；
  完整 compare destination-local 覆盖、full bit-not/shift destination-local 覆盖、branch
  还没有切换到 C 局部主存储，C-local/frame-slot 系统性镜像、GC 根登记、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 11:42:37 +08:00 · M2 / 04-S3 typed TO_UINT_SIGNED/TO_UINT_FLOAT 使用 C 局部 source 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_conversion.c` 的 focused `TO_UINT_SIGNED` / `TO_UINT_FLOAT` lowering
  在 source 有 `i64` / `f64` scalar local 声明时，会先把 frame-slot source 同步到
  `zr_aot_sN` / `zr_aot_fN`，再从该 C 局部发射 `TZrUInt64` 转换表达式；destination
  也有 `u64` scalar local 时才写 `zr_aot_uDst`，否则保持 `zr_aot_u_result` 作为结果镜像变量；
  `tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_u_result = (TZrUInt64)zr_aot_s2;` 与
  `zr_aot_u_result = (TZrUInt64)zr_aot_f16;`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 scalar conversion 对 `i64`/`f64`
  source local 查询与 `sN/fN -> u64` 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_u_result = (TZrUInt64)zr_aot_s2;` /
  `zr_aot_u_result = (TZrUInt64)zr_aot_f16;` generated-product 断言后，
  focused typed scalar 测试失败；接入 `TO_UINT_SIGNED` / `TO_UINT_FLOAT` source-local 模板后到 GREEN，
  生成物 forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + TO_UINT/TO_UINT_SIGNED/TO_UINT_FLOAT/TO_FLOAT/TO_INT_FLOAT/TO_INT_UNSIGNED
  local source + u64/f64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；
  marker scan 命中 `zr_aot_u_result = (TZrUInt64)zr_aot_s2;` 与
  `zr_aot_u_result = (TZrUInt64)zr_aot_f16;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_conversion.c` `-Wformat -Werror=format-extra-args`
  focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 `TO_UINT_SIGNED` / `TO_UINT_FLOAT` 的第一批
  `sN/fN` source 表达式；完整 conversion destination-local 覆盖、full bit-not/shift
  destination-local 覆盖、u64 compare、branch 还没有切换到 C 局部主存储，C-local/frame-slot
  系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 11:31:34 +08:00 · M2 / 04-S3 typed TO_UINT 使用 C 局部 source 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_conversion.c` 的 focused `TO_UINT` lowering 接收 `functionIr`，
  在 source 有 `u64` scalar local 声明时，会先把 frame-slot source 同步到 `zr_aot_uN`，
  再从该 C 局部写 `zr_aot_u_result`；destination 也有 `u64` scalar local 且与 source
  不同槽时才写 `zr_aot_uDst`，否则保持 `zr_aot_u_result` 作为结果镜像变量；
  `tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含 `zr_aot_u_result = zr_aot_u6;`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 scalar conversion 对 `u64` source/destination
  local 查询与 `uN -> u64` 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_u_result = zr_aot_u6;` generated-product 断言后，
  focused typed scalar 测试失败；接入 `TO_UINT` source-local 模板后到 GREEN，
  生成物 forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + TO_UINT/TO_FLOAT/TO_INT_FLOAT/TO_INT_UNSIGNED local source
  + u64/f64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_u_result = zr_aot_u6;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_conversion.c` `-Wformat -Werror=format-extra-args`
  focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 `TO_UINT` unsigned-source 分支的第一批 `uN`
  source 表达式；`TO_UINT_SIGNED`、`TO_UINT_FLOAT`、完整 conversion destination-local 覆盖、
  full bit-not/shift destination-local 覆盖、u64 compare、branch 还没有切换到 C 局部主存储，
  C-local/frame-slot 系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 11:25:15 +08:00 · M2 / 04-S3 typed TO_INT_UNSIGNED 使用 C 局部 source 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_conversion.c` 的 focused `TO_INT_UNSIGNED` lowering 在 source 有
  `u64` scalar local 声明时，会先把 frame-slot source 同步到 `zr_aot_uN`，再从该 C 局部执行
  `u64 -> i64` 的范围折返转换；destination 也有 `i64` scalar local 时才写 `zr_aot_sDst`，
  否则保持 `zr_aot_s_result` 作为结果镜像变量；`tests/parser/test_aot_c_typed_scalar.c`
  断言生成物包含 `zr_aot_s_result = (TZrInt64)zr_aot_u8;`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 scalar conversion 对 `u64` source local 查询、
  `i64` destination local 查询与 `uN -> i64` 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_s_result = (TZrInt64)zr_aot_u8;` generated-product 断言后，
  focused typed scalar 测试失败；接入 `TO_INT_UNSIGNED` source-local 模板后到 GREEN，
  生成物 forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + TO_FLOAT/TO_INT_FLOAT/TO_INT_UNSIGNED local source
  + u64/f64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_s_result = (TZrInt64)zr_aot_u8;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_conversion.c` `-Wformat -Werror=format-extra-args`
  focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 `TO_INT_UNSIGNED` 的第一批 `uN` source 表达式；
  完整 conversion destination-local 覆盖、full bit-not/shift destination-local 覆盖、u64 compare、
  branch 还没有切换到 C 局部主存储，C-local/frame-slot 系统性镜像、GC 根登记、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 11:08:57 +08:00 · M2 / 04-S3 typed TO_INT_FLOAT 使用 C 局部 source 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_conversion.c` 的 focused `TO_INT_FLOAT` lowering 在 source 有
  `f64` scalar local 声明时，会先把 frame-slot source 同步到 `zr_aot_fN`，再从该 C 局部发射
  `TZrInt64` 转换表达式；destination 也有 `i64` scalar local 时才写 `zr_aot_sDst`，
  否则保持 `zr_aot_s_result` 作为结果镜像变量；`tests/parser/test_aot_c_typed_scalar.c`
  断言生成物包含 `zr_aot_s_result = (TZrInt64)zr_aot_f16;`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 scalar conversion 对 `f64` source local 查询与
  `fN -> i64` 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_s_result = (TZrInt64)zr_aot_f16;` generated-product 断言后，
  focused typed scalar 测试失败；接入 `TO_INT_FLOAT` source-local 模板后到 GREEN，
  生成物 forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + TO_FLOAT/TO_INT_FLOAT local source + u64/f64 local arithmetic 1/0；
  generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_s_result = (TZrInt64)zr_aot_f16;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_conversion.c` `-Wformat -Werror=format-extra-args`
  focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 `TO_INT_FLOAT` 的第一批 `fN` source 表达式；
  完整 conversion destination-local 覆盖、full bit-not/shift destination-local 覆盖、u64 compare、
  branch 还没有切换到 C 局部主存储，C-local/frame-slot 系统性镜像、GC 根登记、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 11:01:45 +08:00 · M2 / 04-S3 typed TO_FLOAT 使用 C 局部 source 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_conversion.{h,c}` 的 scalar conversion helper 接收 `functionIr`，
  引入 `backend_aot_c_scalar_locals_has_i64_slot` / `has_u64_slot` / `has_f64_slot` 查询；
  focused `TO_FLOAT` / `TO_FLOAT_SIGNED` / `TO_FLOAT_UNSIGNED` 在 source 有 scalar local 声明时，
  会先把 frame-slot source 同步到 `zr_aot_sN` 或 `zr_aot_uN`，再从该 C 局部发射
  `TZrFloat64` 转换表达式；destination 也有 `f64` scalar local 时才写 `zr_aot_fDst`，
  否则保持 `zr_aot_f_result` 作为结果镜像变量；`backend_aot_c_scalar_semir.c` 调用
  conversion helper 时传入 `functionIr`；`tests/parser/test_aot_c_typed_scalar.c`
  断言生成物包含 `zr_aot_f_result = (TZrFloat64)zr_aot_s2;`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 scalar conversion 对 scalar-local 查询与
  `sN/uN -> f64` 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_f_result = (TZrFloat64)zr_aot_s2;` generated-product 断言后，
  focused typed scalar 测试失败；接入 `TO_FLOAT` source-local 模板后到 GREEN，生成物
  forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + TO_FLOAT local source + u64/f64 local arithmetic 1/0；
  generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_f_result = (TZrFloat64)zr_aot_s2;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_conversion.c` 与 `backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 `TO_FLOAT` 的第一批 `sN/uN` source 表达式；
  完整 conversion destination-local 覆盖、full bit-not/shift destination-local 覆盖、u64 compare、
  branch 还没有切换到 C 局部主存储，C-local/frame-slot 系统性镜像、GC 根登记、
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 10:53:38 +08:00 · M2 / 04-S3 typed i64 bit-not 使用 C 局部 source 第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_bitwise.c` 的 signed `i64` bit-not lowering 将 source-local 与
  destination-local eligibility 拆开：source 有 `i64` scalar local 声明时，会先把 frame-slot source
  同步到 `zr_aot_sSource`，并从 `~zr_aot_sSource` 发射 bit-not 表达式；destination 也有 `i64`
  scalar local 时才写 `zr_aot_sDst`，否则保持 `zr_aot_s_result` 作为结果镜像变量，避免临时槽复用
  与 source-level typed local 声明冲突；`tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_s_result = ~zr_aot_s1;`；`tests/parser/test_aot_c_source_contracts.c`
  锁定 signed bit-not 对 scalar-local source 查询与 `sN` source 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_s_result = ~zr_aot_s1;` generated-product 断言后，focused typed scalar 测试失败；
  接入 signed `i64` bit-not 的 source-local 模板后到 GREEN，生成物 forbidden-token 计数仍为 0，
  AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/bit-not/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift operands
  + signed i64 bit-not source + u64/f64 local arithmetic 1/0；generated typed scalar product
  forbidden-token scan 0；marker scan 命中 `zr_aot_s_result = ~zr_aot_s1;`；
  AOT C source contracts 18/0；`backend_aot_c_scalar_bitwise.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 signed `i64` bit-not 的第一批 `sN` source 表达式；
  完整 bit-not/shift destination-local 覆盖、u64 compare、conversion、branch 还没有切换到
  C 局部主存储，C-local/frame-slot 系统性镜像、GC 根登记、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 10:47:38 +08:00 · M2 / 04-S3 typed i64 shift 使用 C 局部操作数第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_bitwise.c` 的 signed `i64` shift lowering 接收 `functionIr`，
  将 operand-local 与 destination-local eligibility 拆开：left / shift count 均有 `i64`
  scalar local 声明时，会把 frame-slot 输入同步到 `zr_aot_sN`，并从 `zr_aot_sLeft` /
  `zr_aot_sShift` 发射移位表达式；destination 也有 `i64` scalar local 时才写
  `zr_aot_sDst`，否则保持 `zr_aot_s_result` 作为结果镜像变量，避免因临时槽复用而强行声明错误
  local；顺手移除了 signed bit-not fallback 中重复写入 `zr_aot_s_result` 的无效发射；
  `tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_s_result = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 signed shift 对 scalar-local 查询与 `sN`
  operand 模板的依赖 · RED/GREEN：
  先加入 `zr_aot_s_result = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);`
  generated-product 断言后，focused typed scalar 测试失败；接入 signed `i64` shift 的
  operand-local 模板后到 GREEN，生成物 forbidden-token 计数仍为 0，AOT shared library
  结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare/bitwise + signed i64 shift local operands
  + u64/f64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_s_result = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);`；
  AOT C source contracts 18/0；`backend_aot_c_scalar_bitwise.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过；scoped `git diff --check`
  通过，仅保留已知 LF-to-CRLF warning · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 signed `i64` shift 的第一批 `sN` operand
  表达式；bit-not、完整 shift destination-local 覆盖、u64 compare、conversion、branch
  还没有切换到 C 局部主存储，C-local/frame-slot 系统性镜像、GC 根登记、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 10:30:13 +08:00 · M2 / 04-S3 typed i64 位运算使用 C 局部第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_bitwise.{h,c}` 接收 `functionIr`，引入
  `backend_aot_c_scalar_locals_has_i64_slot` 判定；SemIR-backed signed `i64` binary bitwise
  在确认 destination / left / right 均有 `i64` scalar local 声明时，会先把 frame-slot 输入同步到
  `zr_aot_sN`，再发射 `zr_aot_sDst = zr_aot_sLeft &|^ zr_aot_sRight`，并用该 C 局部结果镜像回
  既有 frame slot；`backend_aot_c_scalar_semir.c` 调用 bitwise helper 时传入 `functionIr`；
  `tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_s13 = zr_aot_s9 & zr_aot_s0;`；`tests/parser/test_aot_c_source_contracts.c`
  锁定 scalar bitwise 对 scalar locals 的依赖与 `sN` bitwise 模板 · RED/GREEN：
  先加入 `zr_aot_s13 = zr_aot_s9 & zr_aot_s0;` generated-product 断言后，focused typed scalar
  测试失败；接入 signed `i64` bitwise 的 `sN` 模板后到 GREEN，生成物 forbidden-token 计数仍为
  0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare + u64/f64 local arithmetic + i64 local bitwise
  1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_s13 = zr_aot_s9 & zr_aot_s0;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_bitwise.c` 与 `backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 signed `i64` binary bitwise 的第一批 `sN`
  表达式；bit-not、shift、u64 compare、conversion、branch 还没有切换到 C 局部主存储，
  C-local/frame-slot 系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 10:23:20 +08:00 · M2 / 04-S3 typed f64 二元算术使用 C 局部第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.{h,c}` 新增
  `backend_aot_c_scalar_locals_has_f64_slot`；`backend_aot_c_scalar_semir.c` 的 SemIR-backed
  `f64` 二元算术在确认 destination / left / right 均有 `f64` scalar local 声明时，会先把
  frame-slot 输入同步到 `zr_aot_fN` 局部，再发射
  `zr_aot_fDst = zr_aot_fLeft op zr_aot_fRight` 或 `fmod(zr_aot_fLeft, zr_aot_fRight)`，并用该
  C 局部结果镜像回既有 frame slot；`tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_f28 = zr_aot_f16 * zr_aot_f17;`；`tests/parser/test_aot_c_source_contracts.c`
  锁定 `f64` local 查询与 `fN` 模板字符串 · RED/GREEN：
  先加入 `zr_aot_f28 = zr_aot_f16 * zr_aot_f17;` generated-product 断言后，focused typed
  scalar 测试失败；接入 `f64` scalar-local 查询和 `fN` 模板后到 GREEN，生成物 forbidden-token
  计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare + u64 local arithmetic + f64 local arithmetic
  1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_f28 = zr_aot_f16 * zr_aot_f17;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_locals.c` 与 `backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 `f64` 二元算术的第一批 `fN` 表达式；`u64`
  compare、bitwise、conversion、branch 还没有切换到 C 局部主存储，C-local/frame-slot
  系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 10:16:26 +08:00 · M2 / 04-S3 typed u64 二元算术使用 C 局部第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.{h,c}` 新增
  `backend_aot_c_scalar_locals_has_u64_slot`，复用共享 scalar-local kind 查询；
  `backend_aot_c_scalar_semir.c` 的 SemIR-backed unsigned `u64` 二元算术在确认 destination /
  left / right 均有 `u64` scalar local 声明时，会先把 frame-slot 输入同步到 `zr_aot_uN`
  局部，再发射 `zr_aot_uDst = zr_aot_uLeft op zr_aot_uRight` 或 literal 右操作数形式，并用该
  C 局部结果镜像回既有 frame slot；`tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 `u64` local 查询与 `uN` 模板字符串 ·
  RED/GREEN：先加入 `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;` generated-product 断言后，
  focused typed scalar 测试失败；接入 `u64` scalar-local 查询和 `uN` 模板后到 GREEN，生成物
  forbidden-token 计数仍为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar locals + signed i64 local arithmetic/compare + u64 local arithmetic 1/0；generated typed scalar
  product forbidden-token scan 0；marker scan 命中
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_locals.c` 与 `backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 unsigned `u64` 二元算术的第一批 `uN` 表达式；
  `u64` compare、`f64`、bitwise、conversion、branch 还没有切换到 C 局部主存储，
  C-local/frame-slot 系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 10:08:13 +08:00 · M2 / 04-S3 typed signed i64 比较使用 bool C 局部第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.{h,c}` 新增
  `backend_aot_c_scalar_locals_has_bool_slot`，并把 scalar-local kind 查询收敛到共享 helper；
  `backend_aot_c_scalar_semir.c` 的 SemIR-backed signed `i64` 比较在确认 destination 为 bool
  local、left/right 为 `i64` local 时，会先把 frame-slot 输入同步到 `zr_aot_sN`，再发射
  `zr_aot_bDst = (TZrBool)(zr_aot_sLeft op zr_aot_sRight)` 或 literal 右操作数形式，并将
  bool 局部结果镜像回既有 frame slot；`tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_b24 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`；
  `tests/parser/test_aot_c_source_contracts.c` 锁定 bool-slot 查询与 `bN` compare 模板 ·
  RED/GREEN：先加入 generated-product 断言要求 signed `i64` compare 写入 bool C 局部后，
  focused typed scalar 测试失败；实现后生成物实际 compare destination 是 SemIR 临时 bool 槽
  `b24`，断言收敛到该稳定生成物标记并到 GREEN，生成物 forbidden-token 计数仍为 0，AOT
  shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar locals + signed i64 local arithmetic + signed i64 local compare 1/0；generated typed scalar
  product forbidden-token scan 0；marker scan 命中
  `zr_aot_b24 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_locals.c` 与 `backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 signed `i64` compare 的第一批 `bN/sN` 表达式；
  `u64`、`f64`、bitwise、conversion、branch 还没有切换到 C 局部主存储，C-local/frame-slot
  系统性镜像、GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 09:49:19 +08:00 · M2 / 04-S3 typed signed i64 二元算术使用 C 局部第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_semir.c` 的 SemIR-backed signed `i64` 二元算术在确认
  destination / left / right 均有 `i64` scalar local 声明时，会先把 frame-slot 输入同步到
  `zr_aot_sN` 局部，再发射 `zr_aot_sDst = zr_aot_sLeft op zr_aot_sRight` 或 literal 右操作数
  形式，并用该 C 局部结果镜像回既有 frame slot；`backend_aot_c_scalar_locals.{h,c}`
  新增 `backend_aot_c_scalar_locals_has_i64_slot` 查询，复用 declaration collection 规则以避免
  scalar lowering 与局部声明判定分叉；`tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`；`tests/parser/test_aot_c_source_contracts.c`
  锁定 scalar SemIR 对 scalar locals 的依赖与 `sN` 模板字符串 · RED/GREEN：
  先加入 `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;` generated-product 断言后，focused typed scalar
  测试失败；接入 i64 scalar-local 查询和 `sN` 模板后到 GREEN，生成物 forbidden-token 计数仍为
  0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar locals + signed i64 local arithmetic 1/0；generated typed scalar product forbidden-token scan 0；
  marker scan 命中 `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_locals.c` 与 `backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只覆盖 signed `i64` 二元算术的第一批 `sN` 表达式，
  仍保留 frame-slot 作为 ABI/镜像存储；signed compare、u64、f64、bitwise、conversion、branch
  还没有切换到 C 局部主存储，GC 根登记、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 09:41:43 +08:00 · M2 / 04-S3 typed scalar C 局部声明骨架第一切片 · 状态：
  子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  新增 `backend_aot_c_scalar_locals.{h,c}`，在 generated frame setup 后、value SemIR 与
  bytecode dispatch 发射前，根据 `typedLocalBindings` 与 SemIR destination static C type
  收集 bool / signed `i64` / unsigned `u64` / `f64` 标量槽，发射
  `zr_aot_scalar_locals_begin` / `zr_aot_scalar_locals_end` 包围的
  `TZrBool zr_aot_bN`、`TZrInt64 zr_aot_sN`、`TZrUInt64 zr_aot_uN`、
  `TZrFloat64 zr_aot_fN` C 局部声明；typed local metadata 优先于临时 SemIR destination
  类型，避免声明局部槽被同槽临时结果误判为冲突；`backend_aot_c_function_body.c`
  接入该声明骨架；`tests/parser/test_aot_c_typed_scalar.c` 断言生成物包含具体的
  `s0` / `u6` / `f16` / `b5` 声明；`tests/parser/test_aot_c_source_contracts.c`
  追加 scalar-local 声明模块、调度位置和禁止旧 stack/value fallback 的源码契约 · RED/GREEN：
  先加入 generated-product 局部声明断言后，focused typed scalar 测试因缺少
  `zr_aot_scalar_locals_begin` 失败；补局部声明模块后到 GREEN；随后将断言收紧到
  `TZrUInt64 zr_aot_u6 = (TZrUInt64)0u;` 暴露 SemIR destination 冲突覆盖 typed local
  元数据的问题，修正为 typed local 优先后到 GREEN，生成物 forbidden-token 计数仍为 0，
  AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  + scalar-local declarations 1/0；generated typed scalar product forbidden-token scan 0；
  marker scan 命中 `zr_aot_scalar_locals_begin` / `end` 与
  `TZrInt64 zr_aot_s0`、`TZrUInt64 zr_aot_u6`、`TZrFloat64 zr_aot_f16`、
  `TZrBool zr_aot_b5`；AOT C source contracts 18/0；
  `backend_aot_c_scalar_locals.c` 与 `backend_aot_c_function_body.c`
  `-Wformat -Werror=format-extra-args` focused 编译检查通过 · 备注：
  本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只建立 04-S3 的 scalar C 局部声明骨架，尚未把算术/比较/
  位/转换模板真正改写为 `sN = ...`、未镜像 C 局部与 frame slot、未实现 GC 根登记；
  完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，
  不能声明 M2 完成。

- 2026-06-20 09:21:52 +08:00 · M2 / 04-S2 typed branch 直接 frame-slot 降级第一切片 · 状态：
  子切片完成、04-S2 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_lowering_control.c` 中 typed bool branch 与 fused signed branch 发射从旧
  `ZrCore_Stack_GetValue(` / `ZrCore_Function_MakeFrameSlotPlace(` 读取改为直接
  `&frame.slotBase[slot].value`，并保留 frame-slot bound、bool/signed-int tag 检查和 C `goto`
  分支；`tests/parser/test_aot_c_typed_scalar.c` 在 typed scalar golden 中加入
  `if (product > threshold)` 和 typed bool local `branchFlag` 分支，断言生成物包含
  `zr_aot_jump_if_signed_compare` 与 `zr_aot_jump_if_bool_false`；`tests/parser/test_aot_c_source_contracts.c`
  增加 branch lowering 的直接 frame-slot 读取与旧 branch-specific `Stack_GetValue`/typed-place
  fallback 禁用契约 · RED/GREEN：加入 typed `if` 后，生成物先在
  `zr_aot_jump_if_signed_compare` / `zr_aot_jump_if_bool_false` 命中
  `ZrCore_Stack_GetValue(` 并触发 focused forbidden-token 失败；改为 direct frame-slot 读取后到
  GREEN，生成物 forbidden-token 计数为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy/branch
  1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_jump_if_signed_compare`、`zr_aot_jump_if_bool_false` 和 direct
  `frame.slotBase[...]` 读取；AOT C source contracts 17/0；
  `backend_aot_c_lowering_control.c` `-Wformat -Werror=format-extra-args` focused 编译检查通过 ·
  备注：本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只消除 typed branch helper 的旧 stack/value 读取 fallback；
  完整 04-S3 的 C 局部槽声明、完整 03-S3 block 形态、非数值/泛型转换、deopt 执行与
  typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 09:09:41 +08:00 · M2 / 04-S2-S3 typed scalar local copy 直接 frame-slot 降级第一切片 · 状态：
  子切片完成、04-S2 部分完成、04-S3 部分推进、M2 部分完成 · 完成项目：
  新增 `backend_aot_c_scalar_stack_copy.{h,c}`，在 AOT 函数体处理 `GET_STACK` /
  `SET_STACK` 时，若 destination 或 source 槽能由 typed local metadata 证明为 bool、
  signed `i64`、unsigned `u64` 或 `f64`，则优先生成直接 frame-slot scalar copy，
  读写 `nativeBool` / `nativeInt64` / `nativeUInt64` / `nativeDouble` 并清理 destination
  ownership/native flags，不再进入旧 `backend_aot_write_c_direct_stack_copy`；
  `backend_aot_c_function_body.c` 在 value SemIR copy 尝试失败后、旧 stack-copy fallback
  之前插入该 focused scalar copy；`tests/parser/test_aot_c_typed_scalar.c` 将 bitwise/shift
  golden 从 standalone 表达式升级为 `var masked/joined/toggled/shifted/...` 局部链，
  并把这些局部纳入最终返回值，同时断言生成物包含 `zr_aot_scalar_stack_copy_i64` ·
  RED/GREEN：升级为 typed bitwise 局部链后，生成物先暴露旧
  `zr_aot_direct_stack_copy`，禁用 token 扫描命中 `ZrCore_Stack_GetValue(`；
  补 focused scalar stack-copy helper 并在函数体 fallback 前调度后到 GREEN，
  生成物 forbidden-token 计数为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion + i64 bitwise/shift/local-copy
  1/0；generated typed scalar product forbidden-token scan 0；marker scan 命中
  `zr_aot_scalar_stack_copy_i64`、`i64_bit_not`、`i64_bitwise`、`i64_shift` 以及既有
  `i64` / `u64` / `f64` / conversion 标记；AOT C source contracts 17/0；SemIR typed opcode
  guardrails 1/0；`backend_aot_c_scalar_stack_copy.c` 与
  `backend_aot_c_function_body.c` `-Wformat -Werror=format-extra-args` focused 编译检查通过 ·
  备注：本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。本切片只消除 typed scalar local copy 的旧 stack-copy fallback；
  还不是完整 04-S3 的 C 局部槽声明与 GC 根登记，typed branch、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 08:54:57 +08:00 · M2 / 04-S1-S2 typed i64 位运算/移位纯 C 降级第一切片 · 状态：
  子切片完成、04-S1 部分完成、04-S2 部分完成、M2 部分完成 · 完成项目：
  `EZrSemIrOpcode` 追加 `BIT_NOT` / `BIT_AND` / `BIT_OR` / `BIT_XOR` / `SHL` / `SHR`；
  `compiler_semir.c` 将 typed bitwise/shift 字节码映射到这些 SemIR 行，并用 typed local
  static C type hint 区分 signed `i64` 与 unsigned `u64`；`backend_aot_exec_ir.c`
  追加 opcode 名称；新增 `backend_aot_c_scalar_bitwise.{h,c}` 并从
  `backend_aot_c_scalar_semir.c` 优先调度，生成物直接读取/写入
  `frame.slotBase[slot].value.nativeObject.nativeInt64` / `nativeUInt64`，带 frame-slot
  bound、signed/unsigned tag 和 shift-count 范围检查，覆盖 `~`、`&`、`|`、`^`、`<<`、`>>`
  的第一批 focused scalar C 发射标记；`tests/parser/test_semir_typed_opcode_guardrails.c`
  追加 bitwise/shift SemIR guardrail；`tests/parser/test_aot_c_typed_scalar.c`
  追加 standalone typed `int` 位运算/移位 golden 表达式与 marker 断言 ·
  RED/GREEN：先改 SemIR guardrail 后编译失败，原因是新 SemIR opcode 尚未声明；
  补 opcode 与 SemIR 映射后 guardrail GREEN；加入 AOT bitwise golden 后先暴露
  `var masked: int = ...` 这类 scalar local copy 仍会触发旧
  `zr_aot_direct_stack_copy` 的 `ZrCore_Stack_GetValue(`，因此本切片将 golden 收敛为
  standalone bitwise 表达式，只验收 bitwise/shift 操作本身；补 scalar bitwise helper 后
  生成物 forbidden-token 计数为 0，AOT shared library 结果与解释器一致 · 测试结果：
  SemIR typed opcode guardrails 1/0；AOT C typed scalar i64/u64/f64
  arithmetic/comparison/conversion + i64 bitwise/shift 1/0；generated typed scalar
  product forbidden-token scan 0；marker scan 命中 `i64_bit_not` / `i64_bitwise` /
  `i64_shift` 以及既有 `i64` / `u64` / `f64` / conversion 标记；AOT C source contracts
  17/0；`compiler_semir.c`、`backend_aot_c_scalar_semir.c`、
  `backend_aot_c_scalar_bitwise.c` `-Wformat -Werror=format-extra-args` focused 编译检查通过 ·
  备注：本记录不声明 CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成
  shared library 的方式。scalar local copy、typed branch、完整 03-S3 block 形态、
  非数值/泛型转换、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 08:25:04 +08:00 · M2 / 04-S2 typed 数值转换纯 C 降级第一切片 · 状态：
  子切片完成、04-S2 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_conversion.{h,c}` 追加 focused typed scalar conversion 发射，
  覆盖 `TO_INT` / `TO_INT_FLOAT` / `TO_INT_UNSIGNED`、`TO_UINT` /
  `TO_UINT_FLOAT` / `TO_UINT_SIGNED`、`TO_FLOAT` / `TO_FLOAT_SIGNED` /
  `TO_FLOAT_UNSIGNED`；生成物直接读取 frame slot 的 `nativeInt64` /
  `nativeUInt64` / `nativeDouble` 并写回 `nativeInt64` / `nativeUInt64` /
  `nativeDouble`，不再走旧 typed conversion helper 的 `ZrCore_Stack_GetValue(` +
  `ZR_VALUE_FAST_SET(`；`tests/parser/test_aot_c_typed_scalar.c` 追加
  `<float> product`、`<float> unsignedSum`、`<int> floatLeft`、`<int> unsignedSum`
  并断言生成物包含 `zr_aot_scalar_exec_to_i64`、`zr_aot_scalar_exec_to_u64`、
  `zr_aot_scalar_exec_to_f64` 标记 · RED/GREEN：加入 typed numeric conversion
  输入后先暴露旧 `zr_aot_convert_*` lowering 仍会让生成物出现
  `ZrCore_Stack_GetValue(` / `ZR_VALUE_FAST_SET(`；补 direct conversion helper 后到
  GREEN，生成物 forbidden-token 计数为 0，AOT shared library 结果与解释器一致 ·
  测试结果：AOT C typed scalar i64/u64/f64 arithmetic/comparison/conversion 1/0；
  generated typed scalar product forbidden-token scan 0；marker scan 命中 `to_i64` /
  `to_u64` / `to_f64`；AOT C source contracts 17/0；
  `backend_aot_c_scalar_semir.c` 与 `backend_aot_c_scalar_conversion.c`
  `-Wformat -Werror=format-extra-args` 编译检查通过；
  scoped `git diff --check` 通过（仅提示既有 LF→CRLF 警告）· 备注：本记录不声明
  CTest 通过；验证仍采用直接 WSL gcc 对象编译/链接既有静态库与生成 shared library
  的方式。当前 M2 已覆盖 signed `i64`、unsigned `u64`、`f64` 的第一批算术/比较/数值转换；
  scalar C 局部、位运算、typed branch、完整 03-S3 block 形态、非数值/泛型转换、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 08:16:47 +08:00 · M2 / 04-S1-S2 typed u64 标量算术/比较纯 C 降级追加切片 · 状态：
  子切片完成、04-S1 部分完成、04-S2 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_semir.{h,c}` 与 `backend_aot_c_scalar_conversion.{h,c}`
  追加 SemIR-backed unsigned `u64`
  `ADD/SUB/MUL/DIV/MOD` 识别与 C `+ - * / %` 表达式发射，追加 unsigned
  `EQ/NE/LT/LE/GT/GE` 识别与 C 比较表达式发射；生成物直接读取
  `frame.slotBase[slot].value.nativeObject.nativeUInt64`，带 frame-slot bound、unsigned-int
  tag 与除零/取模零检查，并通过直接 `nativeUInt64` / bool value 字段写回 destination；
  为 typed `uint` 局部初始化所需的 `TO_UINT` / `TO_UINT_SIGNED` / `TO_UINT_FLOAT`
  增加 focused 直接写 `nativeUInt64` 路径；`tests/parser/test_aot_c_typed_scalar.c`
  在同一 typed scalar benchmark 中加入 `uint` 局部、`unsignedLeft + unsignedRight`
  和 `unsignedSum > unsignedRight`，并断言生成物包含 `zr_aot_scalar_exec_to_u64`、
  `zr_aot_scalar_exec_u64_binary` 与 `zr_aot_scalar_exec_u64_compare` 标记 · RED/GREEN：
  加入 typed `uint` 输入后先暴露旧 generic `TO_UINT` lowering 仍会让生成物出现
  `ZrCore_Stack_GetValue(` / `ZR_VALUE_FAST_SET(`；补 direct `TO_UINT` 与 SemIR-backed
  `u64` binary/compare helper 后到 GREEN，生成物 forbidden-token 计数为 0，AOT shared
  library 结果与解释器一致 · 测试结果：AOT C typed scalar i64/u64/f64
  arithmetic/comparison 1/0；generated typed scalar product forbidden-token scan 0；
  marker scan 命中 `i64_binary` / `i64_compare` / `to_u64` / `u64_binary` /
  `u64_compare` / `f64_binary`；AOT C source contracts 17/0；
  `backend_aot_c_scalar_semir.c` `-Wformat -Werror=format-extra-args` 编译检查通过；
  scoped `git diff --check` 通过 · 备注：本记录不声明 CTest 通过；验证仍采用直接 WSL
  gcc 对象编译/链接既有静态库与生成 shared library 的方式。当前 M2 覆盖 signed `i64`
  二元算术/比较、unsigned `u64` 二元算术/比较、focused `TO_UINT` 初始化和 `f64`
  二元算术第一批后端映射；scalar C 局部、位运算、完整数值转换、typed branch、
  完整 03-S3 block 形态、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 07:03:37 +08:00 · M2 / 04-S1-S2 typed f64 标量算术纯 C 降级第一切片 · 状态：
  子切片完成、04-S1 部分完成、04-S2 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_semir.{h,c}` 追加 SemIR-backed `f64` `ADD/SUB/MUL/DIV/MOD`
  识别与 C `+ - * /` / `fmod` 表达式发射；生成物直接读取
  `frame.slotBase[slot].value.nativeObject.nativeDouble`，带 frame-slot bound、float tag 与除零检查，
  并通过直接 double value 字段写回 destination；`tests/parser/test_aot_c_typed_scalar.c`
  在同一 typed scalar benchmark 中加入 `floatLeft * floatRight` 表达式，并断言生成物包含
  `zr_aot_scalar_exec_f64_binary` 标记 · RED/GREEN：先加入浮点表达式确认旧 float arithmetic
  lowering 仍会让生成物出现 forbidden runtime token `ZrCore_Stack_GetValue(` 并失败；补 SemIR
  scalar f64 helper 与直接 double 写值后到 GREEN，生成物 `ZrCore_Stack_GetValue(` /
  `ZR_VALUE_FAST_SET(` 计数为 0，AOT shared library 结果与解释器一致 · 测试结果：
  AOT C typed scalar i64/f64 arithmetic/comparison 1/0；generated typed scalar product
  forbidden-token scan 0；AOT C source contracts 17/0；`backend_aot_c_scalar_semir.c`
  `-Wformat -Werror=format-extra-args` 编译检查通过 · 备注：本记录不声明 CTest 通过；
  验证仍采用直接 WSL gcc 链接既有静态库与生成 shared library 的方式。当前 M2 覆盖 signed
  `i64` 二元算术/比较与 `f64` 二元算术第一条后端映射；scalar C 局部、unsigned 算术、位运算、
  转换、typed branch、完整 03-S3 block 形态、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明
  M2 完成。unsigned 算术 golden 输入初探先撞到解释器侧 unsigned operand 断言，未纳入本切片验收。

- 2026-06-20 06:46:36 +08:00 · M2 / 04-S1-S2 typed i64 标量比较纯 C 降级追加切片 · 状态：
  子切片完成、04-S1 部分完成、04-S2 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_semir.{h,c}` 追加 SemIR-backed signed `i64`
  `EQ/NE/LT/LE/GT/GE` 识别与 C `== != < <= > >=` 表达式发射；生成物直接读取
  `frame.slotBase[slot].value.nativeObject.nativeInt64`，带 frame-slot bound 与 signed-int tag
  检查，并通过直接 bool value 字段写回 destination；`tests/parser/test_aot_c_typed_scalar.c`
  在同一 typed scalar benchmark 中加入 `product > threshold`，并断言生成物包含
  `zr_aot_scalar_exec_i64_compare` 标记 · RED/GREEN：先加入比较用例确认旧 comparison
  lowering 仍会让生成物出现 forbidden runtime token `ZrCore_Stack_GetValue(` 并失败；
  补 SemIR scalar compare helper 与直接 bool 写值后到 GREEN，生成物
  `ZrCore_Stack_GetValue(` / `ZR_VALUE_FAST_SET(` 计数为 0，AOT shared library 结果与解释器一致 ·
  测试结果：AOT C typed scalar arithmetic/comparison 1/0；generated typed scalar product
  forbidden-token scan 0；AOT C source contracts 17/0 · 备注：本记录不声明 CTest 通过；
  验证仍采用直接 WSL gcc 链接既有静态库与生成 shared library 的方式。当前 M2 覆盖 signed
  `i64` 二元算术与比较的第一条后端映射；scalar C 局部、无符号/浮点、位运算、转换、
  typed branch、完整 03-S3 block 形态、deopt 执行与 typed/dynamic bridge 仍未完成，不能声明
  M2 完成。

- 2026-06-20 06:36:48 +08:00 · M2 / 04-S1-S2 typed i64 标量纯 C 降级第一切片 · 状态：
  子切片完成、04-S1 部分完成、04-S2 部分完成、M2 部分完成 · 完成项目：新增
  `tests/parser/test_aot_c_typed_scalar.c` 与 `aot_c_typed_scalar` CTest 注册；
  新增 `backend_aot_c_scalar_semir.{h,c}`，在 AOT 函数体发射时优先识别 SemIR
  typed `i64` `ADD/SUB/MUL/DIV/MOD` 并生成 C `+ - * / %` 表达式；生成物在读写 typed
  scalar benchmark 的 frame slot 时直接访问 `frame.slotBase[slot].value` 与
  `nativeObject.nativeInt64`，并带 slot bound、signed-int tag、除零/取模零检查；frame reset、
  primitive constant、`RESET_STACK_NULL`、`RESET_STACK_NULL2` 与 direct return 的同一路径
  去除了 `ZrCore_Stack_GetValue(` / `ZR_VALUE_FAST_SET(` · RED/GREEN：先确认新 typed scalar
  生成物仍含 forbidden runtime token `ZrCore_Stack_GetValue(` 时测试失败；补 SemIR scalar
  helper 与直接 frame-slot 写值后到 GREEN，生成物 forbidden token 计数为 0，AOT shared library
  结果与解释器一致 · 测试结果：AOT C typed scalar 1/0；AOT C source contracts 17/0；
  AOT C frame setup contracts 1/0；AOT C guardrail contracts 4/0；SemIR pipeline 10/0；
  SemIR typed opcode guardrails 1/0；SemIR static C types 1/0；SemIR dynamic arithmetic
  deopt 1/0；SemIR dynamic index deopt 1/0；SemIR type conflict deopt 1/0 · 备注：
  WSL CMake/Ninja 仍在 `VerifyGlobs.cmake` 图验证阶段卡住，因此本切片 focused 验证继续采用
  直接 WSL gcc 链接既有静态库与生成 shared library 的方式；本记录不声明 CTest 通过。
  本切片只覆盖 signed `i64` 二元算术的第一条后端映射与 typed scalar benchmark 的禁用 token
  清理；scalar C 局部、比较、无符号/浮点、位运算、转换、typed branch、完整 03-S3 block 形态、
  deopt 执行与 typed/dynamic bridge 仍未完成，不能声明 M2 完成。

- 2026-06-20 05:39:31 +08:00 · M1 / 03-S2c 静态类型冲突 deopt 边界 · 状态：
  子切片完成、03-S2 部分完成、M1 部分完成 · 完成项目：新增
  `tests/parser/test_semir_type_conflict_deopt.c` 与 `semir_type_conflict_deopt`
  CTest 注册；`compiler_semir.c` 在 typed scalar SemIR 映射前检查 destination /
  operand 槽是否存在冲突 typed-local 静态 C 类型标注；发现同一槽有不同 static C type
  证据时不再发 typed `ADD/SUB/...` 行，而是发 `ZR_SEMIR_OPCODE_DYN_ARITHMETIC`
  dynamic runtime 行并生成 deopt entry，同时保留 destination 与 operand 槽 · RED/GREEN：
  先确认冲突 typed scalar 仍保持 typed `ADD`、没有 dynamic deopt 行时新测试失败
  `Expected Non-NULL`；补冲突检查后到 GREEN · 测试结果：SemIR type conflict deopt 1/0；
  SemIR typed opcode guardrails 1/0；SemIR static C types 1/0；
  SemIR dynamic arithmetic deopt 1/0；SemIR dynamic index deopt 1/0 · 备注：
  WSL CMake/Ninja 仍在 `VerifyGlobs.cmake` 图验证阶段卡住，因此本切片 focused
  验证继续采用当前 `compiler_semir.c` 直接链接既有静态 WSL 库的方式；本记录不声明 CTest 通过。
  本切片只证明一个具体冲突触发 deopt 的 guardrail，完整 def/use 类型流分析、block splitting、
  deopt 执行与 M2 纯 C 标量降级仍未完成，不能声明 M1 完成。

- 2026-06-20 05:29:37 +08:00 · M1 / 03-S4d dynamic 索引访问 deopt 边界 · 状态：
  子切片完成、03-S4 部分完成、M1 部分完成 · 完成项目：`EZrSemIrOpcode`
  新增 `ZR_SEMIR_OPCODE_DYN_INDEX_GET` / `ZR_SEMIR_OPCODE_DYN_INDEX_SET`；
  `compiler_semir.c` 将通用 `GET_BY_INDEX` 映射为 dynamic `DYN_INDEX_GET`，
  将通用 `SET_BY_INDEX` 映射为 dynamic `DYN_INDEX_SET`；两类 dynamic index
  行标记为 `DYNAMIC_RUNTIME` effect，生成 `deoptId` / `semIrDeoptTable`
  回退点，并保留 destination/value、receiver 与 index operand；intermediate writer
  输出 `DYN_INDEX_GET` / `DYN_INDEX_SET` 名称；新增
  `tests/parser/test_semir_dynamic_index_deopt.c` 与 `semir_dynamic_index_deopt`
  CTest 注册；新增
  `tests/acceptance/2026-06-20-aot-m1-semir-dynamic-index-deopt.md` · RED/GREEN：
  先确认缺少 `ZR_SEMIR_OPCODE_DYN_INDEX_GET` / `ZR_SEMIR_OPCODE_DYN_INDEX_SET`
  时新测试编译失败；补 opcode、writer 名称与 fallback 映射后到 GREEN · 测试结果：
  SemIR dynamic index deopt 1/0；SemIR dynamic iterator deopt 1/0；
  SemIR dynamic call deopt 1/0；SemIR dynamic member deopt 1/0；
  SemIR dynamic arithmetic deopt 1/0；SemIR typed opcode guardrails 1/0；
  SemIR static C types 1/0；SemIR pipeline 10/0；intermediate writer syntax check 通过 · 备注：
  WSL CMake/Ninja 仍在 `VerifyGlobs.cmake` 图验证阶段卡住，因此本切片 focused
  验证继续采用当前 `compiler_semir.c` 直接链接既有静态 WSL 库的方式；本记录不声明 CTest 通过。
  03-S4 的 typed array bounds / address lowering 与剩余复杂指令拆解仍未完成；
  03-S2 broader 类型流冲突 deopt 仍未完成，M1 不能声明完成，M2 纯 C 标量降级尚未开始。

- 2026-06-20 05:16:55 +08:00 · M1 / 03-S4c dynamic 迭代 deopt 边界 · 状态：
  子切片完成、03-S4 部分完成、M1 部分完成 · 完成项目：`compiler_semir.c`
  将通用 `ITER_INIT` 映射为 `ZR_SEMIR_OPCODE_DYN_ITER_INIT`，将
  `ITER_MOVE_NEXT` 映射为 `ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT`；两类 dynamic
  iterator 行标记为 `DYNAMIC_RUNTIME` effect，生成 `deoptId` / `semIrDeoptTable`
  回退点，并保留 result 与 iterator/source operand；新增
  `tests/parser/test_semir_dynamic_iter_deopt.c` 与 `semir_dynamic_iter_deopt`
  CTest 注册；新增 `tests/acceptance/2026-06-20-aot-m1-semir-dynamic-iter-deopt.md`
  · RED/GREEN：先确认通用 `ITER_INIT` / `ITER_MOVE_NEXT` 没有 SemIR 边界时新测试失败
  `Expected Non-NULL`；补 fallback 映射后到 GREEN · 测试结果：SemIR dynamic iterator deopt 1/0；
  SemIR dynamic call deopt 1/0；SemIR dynamic member deopt 1/0；SemIR pipeline 10/0 · 备注：
  WSL CMake/Ninja 仍在 `VerifyGlobs.cmake` 图验证阶段卡住，因此本切片 focused
  验证继续采用当前 `compiler_semir.c` 直接链接既有静态 WSL 库的方式；本记录不声明 CTest 通过。
  03-S4 的 arrays 与剩余复杂指令拆解仍未完成；03-S2 broader 类型流冲突 deopt
  仍未完成，M1 不能声明完成，M2 纯 C 标量降级尚未开始。

- 2026-06-20 05:09:00 +08:00 · M1 / 03-S4b dynamic 调用 deopt 边界 · 状态：
  子切片完成、03-S4 部分完成、M1 部分完成 · 完成项目：`compiler_semir.c`
  在 value-type `CALL_TYPED` 不适用时将通用 `FUNCTION_CALL` 映射为
  `ZR_SEMIR_OPCODE_DYN_CALL`，将 `FUNCTION_TAIL_CALL` 映射为
  `ZR_SEMIR_OPCODE_DYN_TAIL_CALL`；两类 dynamic call 行标记为 `DYNAMIC_RUNTIME`
  effect，生成 `deoptId` / `semIrDeoptTable` 回退点，并保留 result、callee、
  argument-count operand；新增 `tests/parser/test_semir_dynamic_call_deopt.c`
  与 `semir_dynamic_call_deopt` CTest 注册；新增
  `tests/acceptance/2026-06-20-aot-m1-semir-dynamic-call-deopt.md` · RED/GREEN：
  先确认通用 `FUNCTION_CALL` / `FUNCTION_TAIL_CALL` 没有 SemIR 边界时新测试失败
  `Expected Non-NULL`；补 fallback 映射后到 GREEN · 测试结果：SemIR dynamic call deopt 1/0；
  SemIR dynamic member deopt 1/0；SemIR dynamic arithmetic deopt 1/0；
  SemIR typed opcode guardrails 1/0；SemIR static C types 1/0；SemIR pipeline 10/0 · 备注：
  新 call-focused WSL CMake/Ninja 构建目录在 `VerifyGlobs.cmake` 图验证阶段卡住，
  因此本切片 focused 验证采用当前 `compiler_semir.c` 直接链接既有静态 WSL 库的方式；
  本记录不声明 CTest 通过。03-S4 的 arrays、iterators 与更多复杂指令拆解仍未完成；
  03-S2 broader 类型流冲突 deopt 仍未完成，M1 不能声明完成，M2 纯 C 标量降级尚未开始。

- 2026-06-20 04:49:44 +08:00 · M1 / 03-S4a dynamic 成员访问 deopt 边界 · 状态：
  子切片完成、03-S4 部分完成、M1 部分完成 · 完成项目：`compiler_semir.c`
  将通用 `GET_MEMBER` / `SET_MEMBER` 映射为 `ZR_SEMIR_OPCODE_META_GET` /
  `ZR_SEMIR_OPCODE_META_SET`，并标记为 `DYNAMIC_RUNTIME` effect；两类 dynamic
  成员访问行生成 `deoptId` / `semIrDeoptTable` 回退点，并保留 destination/value、
  receiver 与 member-entry operand；新增 `tests/parser/test_semir_dynamic_member_deopt.c`
  与 `semir_dynamic_member_deopt` CTest 入口；新增
  `tests/acceptance/2026-06-20-aot-m1-semir-dynamic-member-deopt.md` · RED/GREEN：
  先确认 `GET_MEMBER` / `SET_MEMBER` 没有 SemIR 边界时新测试失败
  `Expected Non-NULL`；补映射后到 GREEN · 测试结果：SemIR dynamic member deopt 1/0；
  SemIR dynamic arithmetic deopt 1/0；SemIR typed opcode guardrails 1/0；
  SemIR static C types 1/0；SemIR pipeline 10/0；CTest 过滤
  `semir_dynamic_member_deopt|semir_dynamic_arithmetic_deopt|semir_typed_opcode_guardrails|semir_static_c_types`
  通过 4/4 · 备注：本切片关闭 03-S4 的 dynamic member 子项，但不是完整复杂指令拆解；
  typed struct `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT` 已由既有 SemIR pipeline 覆盖，
  calls、arrays、iterators 与更多 runtime boundary 仍未完成；M1 不能声明完成，M2 纯 C
  标量降级尚未开始。

- 2026-06-20 04:29:06 +08:00 · M1 / 03-S2b dynamic 算术 deopt 边界 · 状态：
  子切片完成、03-S2 部分完成、M1 部分完成 · 完成项目：新增
  `ZR_SEMIR_OPCODE_DYN_ARITHMETIC`；`compiler_semir.c` 将通用 `ADD/SUB/MUL/DIV/MOD`
  以及 `LOGICAL_EQUAL/LOGICAL_NOT_EQUAL` 映射为 `DYNAMIC_RUNTIME` effect 的 SemIR 行，
  并生成 `deoptId` / `semIrDeoptTable` 回退点；dynamic 行保留 destination/operand 槽位，
  type table 保持 `ZR_STATIC_C_TYPE_DYNAMIC`；intermediate writer 输出 `DYN_ARITHMETIC`；
  新增 `tests/parser/test_semir_dynamic_arithmetic_deopt.c` 与
  `semir_dynamic_arithmetic_deopt` CTest 入口；新增
  `tests/acceptance/2026-06-20-aot-m1-semir-dynamic-arithmetic-deopt.md` · RED/GREEN：
  先用 focused 编译检查确认缺少 `ZR_SEMIR_OPCODE_DYN_ARITHMETIC` 时测试失败；
  补 opcode 与映射后到 GREEN · 测试结果：SemIR dynamic arithmetic deopt 1/0；
  SemIR typed opcode guardrails 1/0；SemIR static C types 1/0；SemIR pipeline 10/0；
  CTest 过滤 `semir_dynamic_arithmetic_deopt|semir_typed_opcode_guardrails|semir_static_c_types`
  通过 3/3 · 备注：因 `build/codex-wsl-gcc-debug` 当时被另一构建进程占用，本切片使用
  `build/codex-aot-m1-deopt-wsl-gcc-debug` 静态 WSL 构建目录验证；03-S2 的 broader
  类型流冲突 deopt 分析仍未完成，03-S4 复杂指令拆解仍未完成，M1 不能声明完成；M2 纯 C
  标量降级尚未开始。

- 2026-06-20 03:36:05 +08:00 · M1 / 03-S1 typed 标量 opcode 护栏 · 状态：
  切片完成、M1 部分完成 · 完成项目：`EZrSemIrOpcode` 新增 typed 标量算术/比较族
  `ADD/SUB/MUL/DIV/MOD/EQ/NE/LT/LE/GT/GE`；`compiler_semir.c` 将现有
  `*_SIGNED` / `*_UNSIGNED` / `*_FLOAT` 特化数值与比较字节码映射为 typed SemIR；
  标量 SemIR 行显式携带结果静态 C 类型，临时槽也能回填到 `I64/U64/F64/BOOL`
  type table entry；intermediate writer 输出新 opcode 名；新增
  `tests/parser/test_semir_typed_opcode_guardrails.c` 与
  `semir_typed_opcode_guardrails` CTest 入口；新增
  `tests/acceptance/2026-06-20-aot-m1-semir-typed-opcode-guardrails.md` · RED/GREEN：
  先确认缺少 typed 标量 SemIR opcode 时新测试构建失败；补 opcode/映射后再暴露
  算术临时槽仍为 dynamic typeTableIndex；补显式 static C type 回填后到 GREEN · 测试结果：
  SemIR typed opcode guardrails 1/0；SemIR pipeline 10/0；SemIR static C types 1/0；
  CTest 过滤 `semir_typed_opcode_guardrails|semir_static_c_types` 通过 2/2 · 备注：
  本记录关闭 03-S1；它只建立 M2 纯 C 标量降级的 SemIR 输入，还未消除 AOT C 产物中的
  `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` fallback；M1 的 03-S2 冲突触发 deopt 与
  03-S4 broader 复杂指令拆解仍未完成。

- 2026-06-20 03:02:15 +08:00 · M1 / 03-S2a SemIR 静态 C 类型标注地基 · 状态：
  子切片完成、03-S2 部分完成、M1 部分完成 · 完成项目：新增 `EZrStaticCType`
  静态 C 类型枚举；`SZrFunctionTypedTypeRef` 与 `SZrIoFunctionTypedTypeRef` 追加
  `staticCType` / `staticCTypeId`；`compiler_semir.c` 在 SemIR type table 建表时标注
  bool、整数/浮点标量、GC ref、inline struct、native pointer/data；`.zro` writer/io/runtime
  copy 路径通过 `ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES` 保留这些标注；新增
  `tests/parser/test_semir_static_c_types.c` 与 `semir_static_c_types` CTest 入口；新增
  `tests/acceptance/2026-06-20-aot-m1-semir-static-c-types.md` · RED/GREEN：
  先确认缺少 `staticCType` / `staticCTypeId` 字段和 `ZR_STATIC_C_TYPE_*` 枚举时
  `zr_vm_semir_static_c_types_test` 构建失败；补齐编译期标注、binary writer/reader、
  runtime copy 后到 GREEN · 测试结果：SemIR static C types 1/0；SemIR pipeline 10/0；
  CTest 过滤 `semir_static_c_types` 通过 1/1 · 备注：本记录只关闭 03-S2 的
  静态 C 类型标注与 binary roundtrip 地基；计划要求的“冲突触发 deopt”仍未完成，
  typed 块通用算术 opcode 拒绝护栏也仍未完成，因此不能声明 M1 完成。

- 2026-06-20 02:25:49 +08:00 · M1 / 02-S2 生成 C layout 声明与静态断言 · 状态：
  切片完成、M1 部分完成 · 完成项目：新增
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.{h,c}`，
  AOT C 后端会从 `SZrAotFunctionTable` 的 inline struct frame slot 出发，经
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout` 与
  `ZrCore_Function_VisitPrototypeFrameFieldLayouts` 复用 canonical runtime metadata，生成
  `ZrLayout_<typeLayoutId>`、padding/field 成员、`sizeof` / `_Alignof` / `offsetof`
  静态断言；生成文件新增 `ZR_AOT_C_LAYOUT_STRUCT` 对齐宏以匹配 metadata 的
  `byteAlign`；`backend_aot_c_emitter.c` 接入 `<stddef.h>` 和 layout 声明发射；
  新增 `tests/parser/test_aot_c_type_layout_contracts.c`，并注册
  `aot_c_type_layout_contracts` CTest 入口；新增
  `tests/acceptance/2026-06-20-aot-m1-type-layout-declarations.md` · RED/GREEN：
  先确认缺少新 backend helper 文件时源码契约测试失败；随后实际值类型生成文件手动 C 编译暴露
  `ZrLayout_0 align drift` 静态断言失败；补齐 layout 对齐宏后到 GREEN · 测试结果：
  type layout metadata contracts 4/0、AOT C type layout contracts 1/0、guardrail 4/0、
  scalar golden 1/0；CTest 过滤
  `type_layout_metadata_contracts|aot_c_type_layout_contracts|aot_c_guardrail_contracts|aot_c_golden_scalar_smoke`
  通过 4/4；手动编译值类型 smoke 已生成的 `main.c` 通过，证明 `sizeof/_Alignof/offsetof`
  断言成立 · 备注：本记录只覆盖 02-S2 的布局声明与断言；现有
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 仍在完整执行前因生成物含
  `unsupported AOT value SemIR field` 失败，属于后续 struct/value SemIR 纯降级切片，
  不能据此声称 M3 struct 语义完成；M1 的 `semIrTypeTable` 静态 C 类型标注与 typed 块 opcode
  护栏仍未完成。

- 2026-06-20 01:57:09 +08:00 · M1 / 02-S1 类型布局 metadata · 状态：切片完成、M1 部分完成 · 完成项目：
  `SZrTypeLayout` 新增 `blittable`、`cTypeId`、`gcFieldOffsets`、`ownershipFieldOffsets`，
  新增 `SZrTypeLayoutMetadata` 与 `ZrCore_TypeLayout_InitStructWithMetadata`，默认
  `InitStruct` 保持 neutral metadata；`ZrCore_TypeLayout_CanRawCopy` 改由 `blittable`
  判定；新增 `tests/core/test_type_layout_metadata_contracts.c` 与
  `tests/acceptance/2026-06-20-aot-m1-type-layout-metadata.md`，并注册
  `type_layout_metadata_contracts` CTest 入口 · RED/GREEN：先确认 metadata 类型/API/字段缺失导致
  `zr_vm_type_layout_metadata_contracts_test` 构建失败，再补齐到 GREEN · 测试结果：
  type layout metadata contracts 4/0；CTest 过滤套件中 `type_layout_metadata_contracts`
  通过 · 备注：本记录只覆盖 02-S1；`semIrTypeTable` 静态 C 类型标注、typed 块 opcode
  护栏仍未完成。

- 2026-06-20 01:36:50 +08:00 · M0 基线与护栏 · 状态：完成 · 完成项目：
  新增 AOT C forbidden-token/白名单护栏契约、scalar golden 对拍脚手架、CTest 入口与
  `tests/acceptance/2026-06-20-aot-m0-guardrails.md`；记录现有半降级 baseline
  （既有 shared-library smoke 仍含 `ZrCore_Stack_GetValue(`=10 / `ZR_VALUE_FAST_SET(`=3，
  numeric arithmetic 仍含 `ZrCore_Stack_GetValue(`=144 / `ZR_VALUE_FAST_SET(`=13）；
  新 M0 scalar golden 产物可完成解释器/AOT 对拍，但仍含
  `ZrCore_Stack_GetValue(`=10 / `ZR_VALUE_FAST_SET(`=4 · RED/GREEN：
  先确认 token detector、runtime call classifier、interpreter golden helper 的缺失链接失败，
  再补齐到 GREEN · 测试结果：guardrail 4/0、source contracts 17/0、scalar golden 1/0；
  CTest 过滤 `aot_c_guardrail_contracts|aot_c_golden_scalar_smoke|aot_c_source_contracts`
  通过 2/2 · 备注：无生产行为改动；M1 之前仅建立基线与防回退门槛，M2 仍需消除既有宽 AOT
  产物中的 VM stack/value fallback。

- 2026-06-20 · 建立 `docs/plans/aot/` 计划骨架（index + 00–06）· 规划阶段，无代码改动 · 基于对
  `zr_instruction_conf.h` / `value.h` / `function.h`（SemIR）/ `backend_aot_c_*` 现状的探查。
