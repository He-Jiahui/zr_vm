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

完整论证见 [`01-design-principles.md`](01-design-principles.md)。四条贯穿全计划的不变量：

- **不变量 A（确定性）**：进入 AOT/typed 路径的每个 SemIR 槽都有**单一静态 C 类型**
  （`TZrInt64` / `TZrFloat64` / 某个 `struct ZrLayout_*` / 某个 GC 引用指针）。
  类型不确定的表达式**不得**进入 typed 路径，只能留在 dynamic（tagged-union）路径并显式 deopt。
- **不变量 B（纯降级）**：typed 路径的任何 SemIR 指令都必须能映射到**不含 VM 调用**的 C 片段
  （算术/赋值/字段/比较/分支）。需要 VM 运行时的操作（GC 分配、动态派发、所有权状态机）
  以**显式 runtime 调用点**出现，且这些点是 ABI 契约的一部分，而非“顺手调一下”。
- **不变量 C（单一真相）**：值的形状只由类型 metadata（`SZrTypeLayout` / 原型）描述一次，
  解释器、AOT C、GC 扫描、反射全部读同一份 metadata，禁止在多处各自硬编码偏移。
- **不变量 D（环境隔离）**：typed AOT 函数体内**不得**出现解释器执行环境的装配与维护
  （`SZrCallInfo` 帧、`slotBase`/`stackTop`、`programCounter`/`observation`/`debugHook` 逐指令发布），
  也**不得**对 typed 值做 register/SZrValue 双写。解释器状态只在**函数边界** marshaling 一次，
  函数体内只有寄存器（C 局部）+ 纯 C 元素操作。完整规则见 [`07`](07-codegen-register-model-and-environment-isolation.md)。

## 4. 阶段与文档索引

| 序号 | 文档                                                                                                              | 主题                                                                                                                           | 状态    |
| ---- | ----------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ | ------- |
| 00   | [`00-current-state.md`](00-current-state.md)                                                                       | 现状基线与差距清单（事实坐标）                                                                                                 | 📋 规划 |
| 01   | [`01-design-principles.md`](01-design-principles.md)                                                               | 设计纲领、不变量、与 il2cpp/hybridclr 对标                                                                                     | 📋 规划 |
| 02   | [`02-typed-value-and-layout.md`](02-typed-value-and-layout.md)                                                     | 值表示去 tag 化 + 栈/struct 连续布局 + metadata                                                                                | 📋 规划 |
| 03   | [`03-instruction-set-refactor.md`](03-instruction-set-refactor.md)                                                 | 指令集改造：含义明确化、typed 化、消除不确定性                                                                                 | 📋 规划 |
| 04   | [`04-semir-and-c-backend.md`](04-semir-and-c-backend.md)                                                           | SemIR 统一中间层 + 纯 C 后端降级规则                                                                                           | 📋 规划 |
| 05   | [`05-ownership-gc-and-bridge.md`](05-ownership-gc-and-bridge.md)                                                   | 所有权/GC 在生成 C 中的表达 + 解释器与 AOT 桥                                                                                  | 📋 规划 |
| 06   | [`06-implementation-blueprint.md`](06-implementation-blueprint.md)                                                 | 分阶段路线图、里程碑、测试矩阵、验收标准                                                                                       | 📋 规划 |
| 07   | [`07-codegen-register-model-and-environment-isolation.md`](07-codegen-register-model-and-environment-isolation.md) | 寄存器模型 + AOT/解释器环境隔离：删除 prologue 观测机制、消除 register/SZrValue 双写、MethodInfo 取代胖 frame、边界 marshaling | 🚧 进行中 |
| 08   | [`08-generic-sharing.md`](08-generic-sharing.md)                                                                   | 泛型共享：值类型单态化 + 引用类型共享 + 泛型字典(RGCTX 等价) + 实例化收集 + hybrid 动态实例化兜底                              | 📋 规划 |
| 09   | [`09-memory-management.md`](09-memory-management.md)                                                               | 内存管理(AOT 视角)：GC 引用 descriptor、精确栈根 + safepoint + 移动 GC 根更新、写屏障与编译期消除、值类型分配/装箱边界化       | 📋 规划 |
| 10   | [`10-reflection.md`](10-reflection.md)                                                                             | 反射：三级元数据(None/RuntimeMapping/Description)、invoker thunk(复用 07 边界)、token/泛型/offset 反射、保留注解驱动裁剪       | 📋 规划 |
| 11   | [`11-metadata.md`](11-metadata.md)                                                                                 | 元数据：zrp 两段式(数据元数据 + 代码注册表)、运行期 token lazy 解析、token↔cTypeId↔ZrLayout 三向表、版本校验、元数据策略     | 📋 规划 |
| 12   | [`12-code-stripping.md`](12-code-stripping.md)                                                                     | 代码裁剪：mark-and-sweep 可达性、裁剪驱动函数/类型/元数据生成、注解 + manifest 保留、泛型实例可达性、trim 警告、符号剥离       | 📋 规划 |

## 5. 与现有模块的关系

- 本计划是 [`docs/core-runtime/inline-type-layout-and-byte-stack.md`](../../core-runtime/inline-type-layout-and-byte-stack.md)
  的**上层规划**：那篇文档实现了 typed layout + byte-offset 栈原语；本计划规划如何让
  **指令集与代码生成全面切换到这套原语之上**，并最终取代旧固定槽 ABI。
- 与 [`docs/plans/using/`](../using/index.md) 的所有权/metadata token 工作是**互补**关系：
  using 计划提供了所有权种类与 metadata token 模型，本计划负责把它们**降级到生成的 C 代码**里
  （见 `05`），并保证 typed 路径下所有权检查能被编译期证明消除。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 里程碑/切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-06-23 03:24:18 +08:00 · M1.5 / 07-S5 static one-arg bool logical-not
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool thunk 模块新增 `!flag` 一参 recognizer，覆盖
  `LOGICAL_NOT_BOOL/FUNCTION_RETURN` 形态；thunk 发出 `return (TZrBool)!zr_aot_arg0;`；
  bool one-arg direct-call route 复用 scalar-local proof 并继续直接发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA)`。bool smoke 新增
  `invert(false)` 运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` /
  typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 logical-not
  recognizer；smoke 证明旧生成物仍走 `zr_aot_bool_not_exec` / `SZrTypeValue` frame path；
  补 recognizer、can-emit gate 与 exact writer 后 typed call contracts 4/0、bool typed
  direct-call smoke 5/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool 5/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric contracts/smoke 1/0、global contracts 7/0、
  global smoke 9/0、logical 4/0、power contracts 2/0、power smoke 1/0、frame setup 1/0、
  return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-one-arg-logical-not-typed-thunk.md`。
  备注：这是窄 bool 一参 `!` 覆盖；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 03:04:46 +08:00 · M1.5 / 07-S5 static two-arg bool logical-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool thunk 模块新增 `left || right` 二参 recognizer，
  覆盖单条 `LOGICAL_OR/FUNCTION_RETURN` 与当前七指令短路形态；thunk 发出
  `return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);`；bool direct-call route 复用二参
  scalar-local proof 并继续直接发 `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`。
  bool smoke 新增 `either(false, true)` 运行结果 42，并继续拒绝 `CallStaticDirect` /
  `CallStackValue` / typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 OR
  recognizer；首版 build 暴露不存在 `JUMP_IF_BOOL_TRUE`，补真实 `JUMP_IF_BOOL_FALSE + JUMP`
  形态并保留精确 AND/OR writer 后 typed call contracts 4/0、bool typed direct-call smoke 4/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 4/0、
  u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  generic numeric contracts/smoke 1/0、global contracts 7/0、global smoke 9/0、logical 4/0、
  power contracts 2/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-bool-two-arg-logical-or-typed-thunk.md`。
  备注：这是窄 bool 二参 `||` 覆盖；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 02:48:44 +08:00 · M1.5 / 07-S5 static two-arg bool logical-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool thunk 模块新增 `left && right` 二参 recognizer，
  覆盖单条 `LOGICAL_AND/FUNCTION_RETURN` 与当前六指令短路形态；thunk 发出
  `return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);`；bool direct-call route 证明
  destination、`functionSlot + 1u`、`functionSlot + 2u` 都是已写入 bool scalar locals，
  并直接发 `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`。bool smoke
  新增 `both(true, false)` 运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` /
  typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 bool two-arg
  can-emit/writer；GREEN 前 smoke 暴露短路 lowering 形态，补 recognizer 后 typed call
  contracts 4/0、bool typed direct-call smoke 3/0。测试结果：较宽 WSL GCC focused AOT 组
  继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool 3/0、u64 14/0、f64 11/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric contracts/smoke 1/0、
  global contracts 7/0、global smoke 9/0、logical 4/0、power contracts 2/0、power smoke 1/0、
  frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-bool-two-arg-logical-and-typed-thunk.md`。
  备注：这是窄 bool 二参 `&&` 覆盖；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-22 19:29:45 +08:00 · M1.5 / 07-S5 static one-arg u64 bitwise-xor-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 thunk 模块新增 `arg0 ^ K` recognizer wrapper，复用
  bitwise-constant helper 覆盖 `SET_STACK/GET_CONSTANT/TO_UINT/BITWISE_XOR/FUNCTION_RETURN`
  形态；thunk 发出 `return (TZrUInt64)(zr_aot_arg0 ^ (TZrUInt64)%llu);`；u64 smoke
  新增 `toggle(63)` 运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` /
  typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 XOR 常量 recognizer；
  GREEN 为 typed call contracts 4/0、u64 typed direct-call smoke 14/0。测试结果：较宽 WSL
  GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 2/0、u64 14/0、f64 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-bitwise-xor-const-typed-thunk.md`。
  备注：u64 一参 AND/OR/XOR constant 小组已覆盖；07-S5 完整验收仍未完成。

- 2026-06-22 19:19:44 +08:00 · M1.5 / 07-S5 static one-arg u64 bitwise-or-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 thunk 模块新增 `arg0 | K` recognizer wrapper，复用
  bitwise-constant helper 覆盖 `SET_STACK/GET_CONSTANT/TO_UINT/BITWISE_OR/FUNCTION_RETURN`
  形态；thunk 发出 `return (TZrUInt64)(zr_aot_arg0 | (TZrUInt64)%llu);`；u64 smoke
  新增 `flags(32)` 运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` /
  typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 OR 常量 recognizer；
  GREEN 为 typed call contracts 4/0、u64 typed direct-call smoke 13/0。测试结果：较宽 WSL
  GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 2/0、u64 13/0、f64 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-bitwise-or-const-typed-thunk.md`。
  备注：仍是窄 u64 一参 `| K` 覆盖；07-S5 完整验收仍未完成。

- 2026-06-22 19:06:21 +08:00 · M1.5 / 07-S5 static one-arg u64 bitwise-and-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 thunk 模块新增 `arg0 & K` recognizer，覆盖当前
  `SET_STACK/GET_CONSTANT/TO_UINT/BITWISE_AND/FUNCTION_RETURN` 形态；thunk 发出
  `return (TZrUInt64)(zr_aot_arg0 & (TZrUInt64)%llu);`；u64 smoke 新增 `mask(47)`
  运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` / typed destination
  sync。RED/GREEN：RED1 为 typed call contract 缺少 recognizer；RED2 为 smoke 证明
  需覆盖 `TO_UINT` 常量转换形态；GREEN 为 typed call contracts 4/0、u64 typed
  direct-call smoke 12/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool 2/0、u64 12/0、f64 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-bitwise-and-const-typed-thunk.md`。
  备注：仍是窄 u64 一参 `& K` 覆盖；07-S5 完整验收仍未完成。

- 2026-06-22 18:49:12 +08:00 · M1.5 / 07-S5 static two-arg u64 bitwise-xor
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 thunk shape 模块新增 `arg0 ^ arg1` recognizer，并把
  AND/OR/XOR 共用形态收敛到私有 bitwise helper；thunk 发出
  `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);`；u64 smoke 新增 `toggle(63, 21)`
  运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` / typed destination
  sync。RED/GREEN：RED 为 typed call contract 缺少 u64 `^` 返回表达式；GREEN 为
  typed call contracts 4/0、u64 typed direct-call smoke 11/0。测试结果：较宽 WSL GCC
  focused AOT 组继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 2/0、u64 11/0、f64 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-bitwise-xor-typed-thunk.md`。
  备注：u64 二参 add/subtract/multiply/AND/OR/XOR 窄直调已覆盖；07-S5 完整验收仍未完成。

- 2026-06-22 18:41:03 +08:00 · M1.5 / 07-S5 static two-arg u64 bitwise-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 thunk shape 模块新增 `arg0 | arg1` recognizer，覆盖
  `BITWISE_OR` + `FUNCTION_RETURN` 的二参 unsigned callee，thunk 发出
  `return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);`；u64 smoke 新增
  `combine(40, 2)` 运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` /
  typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 u64 `|`
  返回表达式；GREEN 为 typed call contracts 4/0、u64 typed direct-call smoke 10/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 2/0、u64 10/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、power smoke 1/0、
  frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-bitwise-or-typed-thunk.md`。
  备注：仍是窄 u64 二参 `|` 覆盖；07-S5 完整验收仍未完成。

- 2026-06-22 18:31:51 +08:00 · M1.5 / 07-S5 static two-arg u64 bitwise-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 thunk shape 模块新增 `arg0 & arg1` recognizer，覆盖
  `BITWISE_AND` + `FUNCTION_RETURN` 的二参 unsigned callee，thunk 发出
  `return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);`；u64 smoke 新增
  `mask(47, 58)` 运行结果 42，并继续拒绝 `CallStaticDirect` / `CallStackValue` /
  typed destination sync。RED/GREEN：RED 为 typed call contract 缺少 u64 `&`
  返回表达式；GREEN 为 typed call contracts 4/0、u64 typed direct-call smoke 9/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 2/0、u64 9/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、power smoke 1/0、
  frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-bitwise-and-typed-thunk.md`。
  备注：仍是窄 u64 二参 `&` 覆盖；07-S5 完整验收仍未完成。

- 2026-06-22 18:17:42 +08:00 · M1.5 / 07-S5 typed call contract split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `tests/parser/test_aot_c_typed_call_contracts.c` 与
  `zr_vm_aot_c_typed_call_contracts_test`，把 i64/bool/u64/f64 typed thunk contract
  从 aggregate call contracts 中拆出；原 `test_aot_c_call_contracts.c` 现在只保留
  dynamic/generic/static/meta 调用边界契约，并从 923 physical / 866 non-empty lines
  降到 386 physical / 348 non-empty lines，新 typed call contract 文件为
  635 physical / 597 non-empty lines。RED/GREEN：RED 为新目标引用尚不存在的测试源文件；
  GREEN 为 call contracts 4/0、typed call contracts 4/0、u64 typed direct-call smoke
  8/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 2/0、u64 8/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、power smoke 1/0、
  frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-typed-call-contract-split.md`。
  备注：支撑拆分不新增 typed thunk 行为；07-S5 完整验收仍未完成。

- 2026-06-22 16:56:34 +08:00 · M1.5 / 07-S5 typed u64 thunk shape split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_u64_thunk_shapes.{h,c}`，把 u64 二参
  add/multiply/subtract return 形态识别从主 u64 thunk 文件拆出；主文件从
  908 physical / 820 non-empty lines 收敛到 569 physical / 509 non-empty lines，
  新 shape 源文件为 356 physical / 326 non-empty lines。RED/GREEN：RED 为
  call contracts 读取新 shape 文件时得到 NULL；GREEN 为 focused target build/relink
  后 call contracts 8/0、u64 typed direct-call smoke 8/0。测试结果：较宽 WSL GCC
  focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 8/0、f64 smoke 11/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、
  logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-typed-u64-thunk-shape-split.md`。
  备注：支撑拆分不新增 typed thunk 行为；07-S5 完整验收仍未完成。

- 2026-06-22 16:39:43 +08:00 · M1.5 / 07-S5 static two-arg u64 multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增二参 u64
  multiply recognizer，识别 `func product(left: uint, right: uint): uint { return left * right; }`
  的 direct multiply、`MUL_SIGNED_LOAD_STACK`、窄 `TO_INT`、parameter-copy + `TO_INT`
  形态；thunk 发出
  `return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1);`。调用侧复用 u64 two-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)` 并沿用
  destination sync elision。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_u64_arg0_arg1_multiply_return(`；GREEN 为 focused
  target build/relink 后 call contracts 8/0、u64 typed direct-call smoke 8/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 8/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-multiply-typed-thunk.md`。
  备注：仅覆盖 u64 二参 multiply-return；07-S5 完整验收仍未完成。

- 2026-06-22 16:29:39 +08:00 · M1.5 / 07-S5 static one-arg u64 multiply-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增一参 u64
  multiply-constant recognizer，识别 `func scale(value: uint): uint { return value * 21; }`
  的 const-op 或 `GET_CONSTANT` / optional parameter-copy + `TO_INT` + `MUL_*`
  + `FUNCTION_RETURN` 形态；thunk 发出
  `return (TZrUInt64)(zr_aot_arg0 * (TZrUInt64)K);`。调用侧复用 u64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_u64_arg0_multiply_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、u64 typed direct-call smoke 7/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 7/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-multiply-const-typed-thunk.md`。
  备注：仅覆盖 u64 一参 multiply-constant；07-S5 完整验收仍未完成。

- 2026-06-22 16:18:35 +08:00 · M1.5 / 07-S5 u64 typed direct-call smoke support
  split 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：新增 `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`，
  把 u64 typed direct-call smoke 的项目写入、binary/AOT C 生成、生成物 needle scan、
  shared-library 编译与 AOT runtime 执行抽成 reusable case runner；具体 u64 smoke
  收敛为 6 个 case 定义与 `main()`，从 909 physical / 842 non-empty lines 降到
  151 physical / 138 non-empty lines，支撑头文件为 211 physical / 191 non-empty lines。
  RED/GREEN：本支撑切片无新增行为 RED；GREEN 为 target rebuild 后 u64 typed direct-call smoke 6/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 6/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-typed-direct-call-u64-smoke-support-split.md`。
  备注：支撑重构不新增 typed thunk 行为；07-S5 完整验收仍未完成。

- 2026-06-22 16:06:28 +08:00 · M1.5 / 07-S5 static one-arg u64 subtract-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增一参 u64
  subtract-constant recognizer，识别 `func dec(value: uint): uint { return value - 8; }`
  的 const-op 或 `GET_CONSTANT` / optional parameter-copy + `TO_INT` + `SUB_*`
  + `FUNCTION_RETURN` 形态；thunk 仅接受 argument 在左、非负常量在右，发出
  `return (TZrUInt64)(zr_aot_arg0 - (TZrUInt64)K);`。调用侧复用 u64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_u64_arg0_subtract_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、u64 typed direct-call smoke 6/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 6/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-subtract-const-typed-thunk.md`。
  备注：仅覆盖 u64 一参 subtract-constant；07-S5 完整验收仍未完成。

- 2026-06-22 15:54:25 +08:00 · M1.5 / 07-S5 static one-arg f64 negate typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 negate recognizer，
  识别 `func negate(value: float): float { return -value; }` 的 `NEG_FLOAT`
  + `FUNCTION_RETURN` callee，并接受当前参数 copy 前缀；thunk 发出
  `return (TZrFloat64)(-zr_aot_arg0);`。调用侧复用 f64 one-arg direct-call writer
  与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_negate_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 11/0。
  测试结果：较宽 WSL GCC focused AOT 组重复执行后通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-negate-typed-thunk.md`。
  备注：仅覆盖 f64 一参 negate；07-S5 完整验收仍未完成。

- 2026-06-22 15:43:22 +08:00 · M1.5 / 07-S5 static one-arg f64 modulo-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 modulo-constant recognizer，
  识别 `func remainder(value: float): float { return value % 50.0; }` 的 `GET_CONSTANT` + `MOD_FLOAT`
  + `FUNCTION_RETURN` callee，并接受当前参数 copy 前缀；thunk 仅接受 argument 在左、
  非零常量在右，发出
  `return (TZrFloat64)fmod(zr_aot_arg0, (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)` 并沿用 destination sync elision；
  f64 smoke 支撑层补上 Unix `-lm` 链接以解析生成 C 的 `fmod`。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_modulo_constant_return(`；
  初次 GREEN 尝试暴露 f64 smoke 支撑层缺少 `-lm`；补齐后 focused target build/relink 通过
  call contracts 8/0、f64 typed direct-call smoke 10/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 10/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-modulo-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参非零 modulo-constant；07-S5 完整验收仍未完成。

- 2026-06-22 15:30:30 +08:00 · M1.5 / 07-S5 static one-arg f64 divide-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 divide-constant recognizer，
  识别 `func halve(value: float): float { return value / 2.0; }` 的 `GET_CONSTANT` + `DIV_FLOAT`
  + `FUNCTION_RETURN` callee，并接受当前参数 copy 前缀；thunk 仅接受 argument 在左、
  非零常量在右，发出
  `return (TZrFloat64)(zr_aot_arg0 / (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_divide_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 9/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 9/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-divide-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参非零 divide-constant；07-S5 完整验收仍未完成。

- 2026-06-22 15:18:48 +08:00 · M1.5 / 07-S5 static one-arg f64 multiply-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 multiply-constant recognizer，
  识别 `func scale(value: float): float { return value * 21.0; }` 的 `GET_CONSTANT` + `MUL_FLOAT`
  + `FUNCTION_RETURN` callee，并接受当前参数 copy 前缀；thunk 发出
  `return (TZrFloat64)(zr_aot_arg0 * (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_multiply_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 8/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 8/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-multiply-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参 multiply-constant；07-S5 完整验收仍未完成。

- 2026-06-22 15:09:08 +08:00 · M1.5 / 07-S5 static one-arg f64 subtract-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 subtract-constant recognizer，
  识别 `func dec(value: float): float { return value - 8.0; }` 的 `GET_CONSTANT` + `SUB_FLOAT`
  + `FUNCTION_RETURN` callee，并接受当前参数 copy 前缀；thunk 发出
  `return (TZrFloat64)(zr_aot_arg0 - (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_subtract_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 7/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 7/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-subtract-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参 subtract-constant；07-S5 完整验收仍未完成。

- 2026-06-22 14:53:25 +08:00 · M1.5 / 07-S5 static one-arg f64 add-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 add-constant recognizer，
  识别 `func inc(value: float): float { return value + 37.0; }` 的 `GET_CONSTANT` + `ADD_FLOAT`
  + `FUNCTION_RETURN` callee，并接受当前参数 copy 前缀；thunk 发出
  `return (TZrFloat64)(zr_aot_arg0 + (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_add_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 6/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 6/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-add-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参 add-constant；07-S5 完整验收仍未完成。

- 2026-06-22 14:46:01 +08:00 · M1.5 / 07-S5 f64 typed direct-call smoke support split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`，把 f64 typed
  direct-call smoke 的项目写入、binary/AOT C 生成、生成物 needle scan、shared-library 编译与
  AOT runtime 执行抽成 reusable case runner；具体 f64 smoke 收敛为 5 个 case 定义与 `main()`。
  RED/GREEN：行为保持重构，无新增 RED 行为契约；重构后 focused f64 typed direct-call smoke 5/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 5/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-typed-direct-call-f64-smoke-support-split.md`。备注：
  支撑重构不新增 typed thunk 行为；07-S5 完整验收仍未完成。

- 2026-06-22 14:38:18 +08:00 · M1.5 / 07-S5 static two-arg f64 multiply typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增二参 f64 multiply-return recognizer，
  识别 `func product(left: float, right: float): float { return left * right; }` 的
  `MUL_FLOAT` + `FUNCTION_RETURN` callee，并复用二参 thunk writer 发出
  `return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1);`；调用侧复用 f64 two-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_arg1_multiply_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 5/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 5/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-two-arg-multiply-typed-thunk.md`。备注：
  仅覆盖 f64 二参 multiply-return；f64 除法/取模/常量表达式、更多参数、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:30:18 +08:00 · M1.5 / 07-S5 static two-arg f64 subtract typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增二参 f64 subtract-return recognizer，
  识别 `func diff(left: float, right: float): float { return left - right; }` 的
  `SUB_FLOAT` + `FUNCTION_RETURN` callee，并复用二参 thunk writer 发出
  `return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1);`；调用侧复用 f64 two-arg
  direct-call writer 与 route proof，继续输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_arg1_subtract_return(`；
  首个 GREEN 命令被超时截断，第二次 focused target build/relink 后 call contracts 8/0、
  f64 typed direct-call smoke 4/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 8/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、
  u64 smoke 5/0、f64 smoke 4/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、power smoke 1/0、
  frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-f64-two-arg-subtract-typed-thunk.md`。备注：
  仅覆盖 f64 二参 subtract-return；f64 乘除/常量表达式、更多参数、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:19:22 +08:00 · M1.5 / 07-S5 static two-arg f64 add typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增二参 f64 add-return recognizer/writer，
  识别 `func sum(left: float, right: float): float { return left + right; }` 的
  `ADD_FLOAT` + `FUNCTION_RETURN` callee，并发出
  `TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *, TZrFloat64, TZrFloat64)`；
  call lowering 新增 f64 two-arg direct-call writer，输出
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)`；typed direct-call
  router 证明 destination 与两个 call-window argument 均为已写入 f64 scalar local 后才直调，
  并沿用 destination sync elision。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function)`；GREEN 为
  focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 3/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 3/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-two-arg-add-typed-thunk.md`。备注：
  仅覆盖 f64 二参 add-return；f64 非加法表达式/更多参数、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:06:21 +08:00 · M1.5 / 07-S5 static one-arg f64 identity typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 identity-return recognizer/writer，
  识别 `func pass(value: float): float { return value; }` 并发出
  `TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *, TZrFloat64)`；call lowering 新增
  f64 one-arg direct-call writer，输出 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`；
  typed direct-call router 证明 destination 与 argument 均为已写入 f64 scalar local 后才直调，
  并沿用 destination sync elision。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function)`；GREEN 为
  focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 2/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 2/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-typed-thunk.md`。备注：
  仅覆盖 f64 一参 identity-return；f64 表达式/多参、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 13:53:07 +08:00 · M1.5 / 07-S5 static no-arg f64 constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_f64_thunks.{h,c}`，识别 zero-arg float/double
  constant-return callee，并发出 `TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *)`；
  emitter 接入 f64 typed thunk 声明/定义；call lowering 新增
  `backend_aot_write_c_static_direct_f64_no_arg_function_call()`，生成
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state)`；typed direct-call router 在普通 static-call
  和 no-arg 专用 helper 中都检查 f64 no-arg route，并沿用 scalar-local destination sync
  elision。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function)`；中间 f64
  smoke 暴露 no-arg helper 缺 f64 分支而回退到 `CallStaticDirect`；补 route 后 GREEN 为
  call contracts 8/0、f64 typed direct-call smoke 1/0。测试结果：较宽 WSL GCC focused
  AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 1/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、
  logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-f64-no-arg-typed-thunk.md`。备注：
  仅覆盖 f64 no-arg constant-return；f64 参数/表达式、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 13:31:10 +08:00 · M1.5 / 07-S5 static two-arg u64 subtract typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_u64_thunks.c` 新增二参 u64 subtract-return recognizer/writer，
  识别 `func diff(left: uint, right: uint): uint { return left - right; }` 的 typed callee，
  并发出 `return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1);`；调用侧复用 u64 two-arg
  direct-call writer 与 `backend_aot_c_typed_direct_calls.c` route gate，继续证明 destination、
  first argument 和 second argument 均为已写入 u64 scalar local 后直调
  `zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)`，并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_u64_arg0_arg1_subtract_return(`；GREEN 为 focused target build/relink 后
  call contracts 7/0、u64 typed direct-call smoke 5/0。测试结果：较宽 WSL GCC focused
  AOT 组继续通过 source 19/0、call contracts 7/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-subtract-typed-thunk.md`。
  备注：仅覆盖 u64 二参 subtract-return；更广 u64 表达式、多参非加/减法、f64、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 13:15:44 +08:00 · M1.5 / 07-S5 static two-arg u64 add typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_u64_thunks.c` 新增二参 u64 add-return recognizer/writer，
  识别 `func sum(left: uint, right: uint): uint { return left + right; }` 的 typed callee，
  并发出
  `return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);`；`backend_aot_c_lowering_calls.c`
  新增 u64 two-arg direct-call writer，`backend_aot_c_typed_direct_calls.c` 证明 destination、
  first argument 和 second argument 均为已写入 u64 scalar local 后直调
  `zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)`，并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function)`；GREEN 为
  focused target build/relink 后 call contracts 7/0、u64 typed direct-call smoke 4/0。测试结果：
  较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 7/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 4/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、
  logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-typed-thunk.md`。备注：仅覆盖
  u64 二参 add-return；更广 u64 表达式、多参非加法、f64、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 12:47:02 +08:00 · M1.5 / 07-S5 static one-arg u64 add-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_u64_thunks.c` 识别一参 `uint` 非负常量加法返回，覆盖当前
  `uint + 1` 生成的参数 copy、`GET_CONSTANT`、`TO_INT`、`ADD_SIGNED`/`ADD_SIGNED_PLAIN_DEST`、
  `FUNCTION_RETURN` 序列，并发出
  `return (TZrUInt64)(zr_aot_arg0 + (TZrUInt64)%llu);`；一参 u64 direct-call route
  继续输出 `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)`，并沿用 scalar-local
  destination sync elision。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_u64_arg0_add_constant_return(`；中间 u64 smoke 暴露当前 SemIR 使用
  `TO_INT` + signed add opcode，修正 recognizer 后 GREEN 为 call contracts 7/0、u64 typed direct-call
  smoke 3/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 7/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 3/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  备注：标准 target relink 受并行 `type_inference` 未解析符号影响，u64 smoke 使用同一对象手动链接后运行；
  仅覆盖一参 u64 非负常量加法返回，u64 多参/更广表达式、f64、inline struct、in/out、
  deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 11:49:26 +08:00 · M1.5 / 07-S5 static one-arg u64 identity typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_u64_thunks.c` 识别 `func pass(value: uint): uint { return value; }`
  并发出 `TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *, TZrUInt64)`；`backend_aot_c_lowering_calls.c`
  新增 u64 one-arg direct-call writer；`backend_aot_c_typed_direct_calls.c` 证明 call-window 参数
  是已写入 u64 scalar local 后走直接 C 调用；`backend_aot_c_scalar_stack_copy.c` 修正参数暂存 copy
  的 source-local static type fallback，避免从未同步 frame slot 复制 0。RED/GREEN：RED 为 call contracts
  缺少 `backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function)`；中间 u64 smoke
  暴露结果 5 而非 42 的参数暂存同步问题；修正后 GREEN 为 source contracts 19/0、call contracts 7/0、
  u64 typed direct-call smoke 2/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 2/0、arithmetic 5/0、bitwise 6/0、call smoke 5/0、shared 8/0、
  value-type 1/0、power 2/0 + 1/0、source 19/0、generic numeric 1/0 + 1/0、global 9/0、
  logical 4/0 + 4/0、typed scalar 1/0、return 1/0、frame setup 1/0。备注：仅覆盖一参 u64
  identity-return；u64 表达式返回/多参、f64、inline struct、in/out、deopt/dynamic bridge 与
  07-S5 完整验收仍未完成。

- 2026-06-22 11:20:28 +08:00 · M1.5 / 07-S5 static no-arg u64 constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_u64_thunks.{h,c}`，识别零参数 unsigned integer
  常量返回 callee，并发出 `TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *)`；`backend_aot_c_emitter.c`
  接入 u64 typed thunk 声明与定义；`backend_aot_c_lowering_calls.c` 新增 u64 no-arg direct-call
  writer；`backend_aot_c_typed_direct_calls.c` 在 static no-arg router 中优先检查 u64 route，避免
  `uint` 返回误走 i64/fallback。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function)`；中间暴露并修正
  `uint` surface 的 `UINT32/U32` metadata、非负 signed literal 常量和 no-arg router 顺序问题；GREEN
  为 call contracts 7/0、u64 typed direct-call smoke 1/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 1/0、arithmetic 5/0、bitwise 6/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0 + 1/0、source 19/0、generic numeric 1/0 + 1/0、
  global 9/0、logical 4/0 + 4/0、typed scalar 1/0、return 1/0、frame setup 1/0。备注：仅覆盖
  u64 no-arg constant-return；u64 参数/表达式返回、f64、inline struct、in/out、deopt/dynamic bridge
  仍待后续；08-12 仍按依赖未开始。

- 2026-06-22 10:56:26 +08:00 · M1.5 / 07-S5 static one-arg bool identity typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_bool_thunks.c` 新增一参 bool identity return recognizer，
  识别 `func pass(flag: bool): bool { return flag; }` 并发出
  `TZrBool zr_aot_typed_bool_fn_N(struct SZrState *, TZrBool)`；`backend_aot_c_lowering_calls.c`
  新增 bool one-arg direct-call writer；`backend_aot_c_typed_direct_calls.c` 证明调用窗口参数槽
  `functionSlot + 1` 是已写入 bool C local 后选择该 typed route，并继续按 scalar-local consumer
  证明跳过不必要的 destination value-slot 同步。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function)`；GREEN 为 call
  contracts 6/0、bool typed direct-call smoke 2/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  typed direct-call 5/0、arithmetic 5/0、bitwise 6/0、call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0 + 1/0、source 19/0、generic numeric 1/0 + 1/0、global 9/0、logical 4/0 + 4/0、
  typed scalar 1/0、return 1/0、frame setup 1/0。备注：仅覆盖 bool 一参 identity-return；
  bool 非 identity 表达式、多参 bool、u64/f64、inline struct、in/out、deopt/dynamic bridge 仍待后续；
  08-12 仍按依赖未开始。

- 2026-06-22 10:44:07 +08:00 · M1.5 / 07-S5 static no-arg bool typed thunk direct-call
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_bool_thunks.{h,c}`，识别零参数、返回 bool 常量的 typed
  callee 并发出 `TZrBool zr_aot_typed_bool_fn_N(struct SZrState *)`；`backend_aot_c_emitter.c`
  接入 bool typed thunk 声明与定义；`backend_aot_c_lowering_calls.c` 新增 bool no-arg direct-call
  writer；`backend_aot_c_typed_direct_calls.{h,c}` 承接 bool no-arg 与既有 i64 no/one/two-arg
  static typed direct-call 路由选择，避免继续扩大 `backend_aot_c_function_body.c`。RED/GREEN：
  RED 为 call contracts 缺少 `backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function)`；
  GREEN 为 call contracts 6/0、bool typed direct-call smoke 1/0、既有 typed direct-call smoke 5/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 arithmetic 5/0、bitwise 6/0、call smoke 5/0、shared 8/0、
  value-type 1/0、power 2/0 + 1/0、source 19/0、generic numeric 1/0 + 1/0、global 9/0、
  logical 4/0 + 4/0、typed scalar 1/0、return 1/0、frame setup 1/0。备注：仅覆盖 bool no-arg
  constant-return direct thunk；bool 参数、u64/f64、inline struct、in/out、deopt/dynamic bridge 仍待后续；
  08-12 仍按依赖未开始。

- 2026-06-22 10:11:42 +08:00 · M1.5 / 07-S5 typed i64 thunk definition writer helper
  consolidation 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_i64_thunks.c` 新增 no-arg、one-arg、two-arg i64 thunk definition
  writer helpers，统一现有 direct-return typed i64 thunk 定义模板；生成 C 表面保持不变，source contract
  锁定 helper 名称。RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_write_i64_no_arg_thunk_definition(`；
  GREEN 为 call contracts 5/0、typed direct-call smoke 5/0、arithmetic smoke 5/0、bitwise smoke 6/0。测试结果：
  较宽 WSL GCC focused AOT 组继续通过 shared/call/value-type/power/source/generic numeric/global/logical/typed
  scalar/return/frame setup 相关二进制。备注：支撑重构，不新增 thunk 行为，不关闭 07-S5；08-12 未开始。

- 2026-06-22 10:00:14 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-xor-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 现在识别 `return arg0 ^ signed-constant`；共享 bitwise constant
  recognizer 按 opcode 追加 `BITWISE_XOR`，并继续接受参数转存前缀后的
  `GET_CONSTANT` + bitwise op + `FUNCTION_RETURN` 形态。生成 C 直接返回
  `(TZrInt64)(zr_aot_arg0 ^ (TZrInt64)K)`；bitwise smoke 新增
  `flip(value:int): int { return value ^ 6; }`，运行结果为 42，且拒绝
  `CallStaticDirect` / `CallStackValue` fallback · RED/GREEN：RED 为 call
  contracts 缺少 bitwise-xor-constant typed thunk source contract；GREEN 为 call
  contracts 5/0、typed direct-call bitwise smoke 6/0；broader WSL GCC focused
  group 也通过 typed direct-call smoke 5/0、typed direct-call arithmetic smoke
  5/0、call smoke 5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source
  19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar
  1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-xor-const-typed-thunk.md` ·
  备注：此切片不覆盖除法/取模/shift 等需要失败通道的 direct thunk，也不关闭 07-S5；
  08-12 仍按依赖未开始。

- 2026-06-22 09:52:35 +08:00 · M1.5 / 07-S5 typed direct-call bitwise
  smoke support split · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：新增
  `tests/parser/aot_c_typed_direct_call_bitwise_smoke_support.h`，抽出 bitwise
  shared-library smoke 的项目路径、binary/hash/blob、AOT C 生成、生成物断言、
  Unix shared-library 编译和执行 helper；主 smoke 只保留 two-arg OR/XOR、
  one-arg NOT、one-arg AND-constant、one-arg OR-constant 五个 case 和 `main()`。
  主文件从 753 physical / 697 non-empty lines 降到 162 physical /
  150 non-empty lines，support header 为 197 physical / 178 non-empty lines ·
  RED/GREEN：无新增行为 RED；这是测试支撑拆分 · 测试结果：WSL GCC focused
  typed direct-call bitwise smoke 5/0 · 产出：
  `tests/parser/aot_c_typed_direct_call_bitwise_smoke_support.h`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-direct-call-bitwise-smoke-support-split.md` ·
  备注：此切片不改变 production AOT 行为、不新增 typed thunk coverage，不关闭
  07-S5；08-12 仍按依赖未开始。

- 2026-06-22 09:42:53 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-or-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 现在识别 `return arg0 | signed-constant`；bitwise constant recognizer
  按 opcode 支持 `BITWISE_AND` / `BITWISE_OR`，并继续接受参数转存前缀后的
  `GET_CONSTANT` + bitwise op + `FUNCTION_RETURN` 形态。生成 C 直接返回
  `(TZrInt64)(zr_aot_arg0 | (TZrInt64)K)`，调用侧继续复用 one-arg direct-call
  gate 和 destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-or-const-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 bitwise-or-constant typed thunk source
  contract；GREEN 为 call contracts 5/0、typed direct-call bitwise smoke 5/0；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、typed
  direct-call arithmetic smoke 5/0、call smoke 5/0、shared 8/0、value-type
  1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：
  只覆盖 one-arg i64 bitwise-or constant 返回，不关闭 07-S5；bitwise smoke
  已到 753 physical lines，继续扩展前应拆 helper；08-12 仍依赖后续 M6/M7 未开始。

- 2026-06-22 09:34:08 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-and-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 现在识别 `return arg0 & signed-constant`，包括编译器插入可证明
  `GET_STACK` / `SET_STACK` 参数转存前缀后的 `GET_CONSTANT` + `BITWISE_AND` +
  `FUNCTION_RETURN` 形态；生成 C 直接返回
  `(TZrInt64)(zr_aot_arg0 & (TZrInt64)K)`，调用侧继续复用 one-arg direct-call
  gate 和 destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-and-const-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 bitwise-and-constant typed thunk source
  contract，随后 bitwise smoke 暴露参数转存前缀未识别；补前缀识别后 GREEN 为
  call contracts 5/0、typed direct-call bitwise smoke 4/0；broader WSL GCC
  focused group 也通过 typed direct-call smoke 5/0、typed direct-call arithmetic
  smoke 5/0、call smoke 5/0、shared 8/0、value-type 1/0、power 2/0+1/0、
  source 19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、
  typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：只覆盖 one-arg i64
  bitwise-and constant 返回，不关闭 07-S5；08-12 仍依赖后续 M6/M7 未开始。

- 2026-06-22 09:21:19 +08:00 · M1.5 / 07-S5 typed i64 thunk recognizer
  helper consolidation 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：typed i64 thunk 模块现在用共享
  `backend_aot_c_try_get_i64_arg0_unary_return()` 校验 one-arg i64 unary-return
  形态，用共享 `backend_aot_c_try_get_i64_arg0_arg1_binary_return()` 校验 two-arg
  i64 简单 binary-return 形态；`NEG_SIGNED` / `BITWISE_NOT` 与 `SUB_SIGNED` /
  `SUB_SIGNED_PLAIN_DEST` / `BITWISE_AND` / `BITWISE_OR` / `BITWISE_XOR` 复用这些
  helper，`ADD` / `MUL` 的 LOAD_STACK 特例仍独立保留 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-typed-i64-thunk-recognizer-helper-consolidation.md` ·
  验证：RED 为 call contracts 缺少 shared helper source contract；GREEN 为 call
  contracts 5/0、typed direct-call arithmetic smoke 5/0、typed direct-call bitwise
  smoke 3/0；broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、
  call smoke 5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、
  generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、
  return 1/0、frame setup 1/0 · 备注：支撑重构不新增行为形态、不关闭 07-S5；
  `backend_aot_c_typed_i64_thunks.c` 从 762 physical / 682 non-empty lines
  降至 670 physical / 600 non-empty lines，08-12 仍依赖后续 M6/M7 未开始。

- 2026-06-22 09:09:06 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-not typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed thunk 识别
  `BITWISE_NOT` + `FUNCTION_RETURN` 的 `return ~arg0` 形态；thunk 模块现在为
  `invert(value:int): int { return ~value; }` 生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(~zr_aot_arg0)`。调用侧复用 one-arg i64 direct-call gate 和
  scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-not-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 bitwise-not typed thunk source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke 3/0；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、typed
  direct-call arithmetic smoke 5/0、call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  备注：只覆盖 one-arg i64 bitwise-not 返回；一般返回、inline struct、in/out 写回、
  deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 08:59:28 +08:00 · M1.5 / 07-S5 static two-arg i64
  bitwise-xor typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别
  `BITWISE_XOR` + `FUNCTION_RETURN` 的 `return arg0 ^ arg1` 形态；thunk 模块现在为
  `toggle(left:int, right:int): int { return left ^ right; }` 生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1)`。调用侧复用 two-arg i64 direct-call
  gate 和 scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-bitwise-xor-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 bitwise-xor typed thunk source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke 2/0；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、typed
  direct-call arithmetic smoke 5/0、call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  备注：只覆盖 two-arg i64 bitwise-xor 返回；一般返回、inline struct、in/out 写回、
  deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 08:51:23 +08:00 · M1.5 / 07-S5 static two-arg i64
  bitwise-or typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别
  `BITWISE_OR` + `FUNCTION_RETURN` 的 `return arg0 | arg1` 形态；thunk 模块现在为
  `join(left:int, right:int): int { return left | right; }` 生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 | zr_aot_arg1)`。调用侧复用 two-arg i64 direct-call
  gate 和 scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-bitwise-or-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 bitwise-or typed thunk source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke 1/0；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、typed
  direct-call arithmetic smoke 5/0、call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  备注：只覆盖 two-arg i64 bitwise-or 返回；一般返回、inline struct、in/out 写回、
  deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 08:41:44 +08:00 · M1.5 / 07-S5 typed direct-call arithmetic
  smoke support split 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：将 typed direct-call arithmetic
  smoke 的通用 shared-library 测试 helper 抽到
  `aot_c_typed_direct_call_arithmetic_smoke_support.h`，主 smoke 降到 753
  physical / 697 non-empty lines，支撑头文件为 116 physical / 93 non-empty lines ·
  产出：
  `tests/acceptance/2026-06-21-aot-m1-5-typed-direct-call-arithmetic-smoke-support-split.md` ·
  验证：支撑性拆分无新增行为 RED；GREEN 为 typed direct-call arithmetic
  shared-library smoke 5/0 · 备注：仅为测试文件治理，不扩大 typed thunk 覆盖面，
  不声明 07-S5 完成。

- 2026-06-22 08:34:57 +08:00 · M1.5 / 07-S5 static two-arg i64
  bitwise-and typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed
  thunk 识别 `BITWISE_AND` + `FUNCTION_RETURN` 的 `return arg0 & arg1`
  形态；thunk 模块现在为
  `mask(left:int, right:int): int { return left & right; }` 生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 & zr_aot_arg1)`。调用侧复用 two-arg i64 direct-call
  gate 和 scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-bitwise-and-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 bitwise-and typed thunk source contract；
  GREEN 为 call contracts 5/0、typed direct-call arithmetic shared-library smoke
  5/0；broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 备注：只覆盖 two-arg i64 bitwise-and 返回；一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 08:20:53 +08:00 · M1.5 / 07-S5 static one-arg i64
  negate typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed thunk 识别
  `NEG_SIGNED` + `FUNCTION_RETURN` 的 `return -arg0` 形态；thunk 模块现在为
  `negate(value:int): int { return -value; }` 生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(-zr_aot_arg0)`。调用侧复用 one-arg i64 direct-call gate 和
  scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-negate-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 negate typed thunk source contract；GREEN 为
  call contracts 5/0、typed direct-call arithmetic shared-library smoke 4/0；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 备注：只覆盖 one-arg i64 unary negation 返回；一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 08:05:24 +08:00 · M1.5 / 07-S5 static one-arg i64
  subtract-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 识别从 add/multiply constant 扩展到 `return arg0 - signed-constant`；
  thunk 模块现在为 `decBy(value:int): int { return value - 8; }` 这类
  `SUB_SIGNED_CONST` / `SUB_SIGNED_CONST_PLAIN_DEST` /
  `SUB_SIGNED_LOAD_STACK_CONST` + `FUNCTION_RETURN` 形态生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 - (TZrInt64)K)`。调用侧复用 one-arg i64 direct-call
  gate 和 scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-subtract-const-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 subtract-constant typed thunk source contract；
  GREEN 为 call contracts 5/0、typed direct-call arithmetic shared-library smoke
  3/0；broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 备注：只覆盖 one-arg i64 减 signed constant 返回；一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 07:54:45 +08:00 · M1.5 / 07-S5 typed i64 thunk module split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：typed i64 thunk recognizers、forward declarations
  与 thunk definitions 从 `backend_aot_c_emitter.c` 拆入
  `backend_aot_c_typed_i64_thunks.{h,c}`，主 emitter 降到 421 行，新模块
  为 419 行；call contracts 的 source needle 改为约束新模块，保留所有已落地
  i64 typed thunk 形态 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-typed-i64-thunk-module-split.md` ·
  验证：首次 WSL focused 验证因 unrelated semantic 构建超时中断；重跑后 call
  contracts 5/0、typed direct-call arithmetic shared-library smoke 2/0 通过；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 备注：仅为模块边界与大文件治理，不声明 07-S5 完成；一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 07:37:35 +08:00 · M1.5 / 07-S5 static one-arg i64
  multiply-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 识别从 identity / add-constant 扩展到 `return arg0 * signed-constant`；
  emitter 现在为 `scale(value:int): int { return value * 21; }` 这类
  `MUL_SIGNED_CONST` / `MUL_SIGNED_CONST_PLAIN_DEST` /
  `MUL_SIGNED_LOAD_STACK_CONST` + `FUNCTION_RETURN` 形态生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 * (TZrInt64)K)`。调用侧复用 one-arg i64 direct-call
  gate 和 scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-multiply-const-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 multiply-constant typed thunk source contract；
  GREEN 为 call contracts 5/0、typed direct-call arithmetic shared-library smoke
  2/0；broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 备注：只覆盖 one-arg i64 乘 signed constant 返回；一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 07:28:09 +08:00 · M1.5 / 07-S5 static two-arg i64 multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别从加/减法扩展到
  `return arg0 * arg1`；emitter 现在为 `product(left:int, right:int): int`
  这类 `MUL_SIGNED` / `MUL_SIGNED_PLAIN_DEST` / `MUL_SIGNED_LOAD_STACK`
  + `FUNCTION_RETURN` 形态生成 `zr_aot_typed_i64_fn_N(state, zr_aot_arg0,
  zr_aot_arg1)`，直接返回 `(TZrInt64)(zr_aot_arg0 * zr_aot_arg1)`。调用侧复用
  two-arg i64 direct-call gate 和 scalar-local-only destination stack-sync
  elision；新增独立 arithmetic smoke target，避免继续扩张既有 typed direct-call
  smoke 文件 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-multiply-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 multiply typed thunk source contract；GREEN
  为 call contracts 5/0、typed direct-call arithmetic shared-library smoke 1/0；
  broader WSL GCC focused group 也通过 typed direct-call smoke 5/0、call smoke
  5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 备注：只覆盖 two-arg i64 乘法返回；一般多参数 ABI、一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 07:13:54 +08:00 · M1.5 / 07-S5 static two-arg i64 subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别从加法扩展到
  `return arg0 - arg1`；emitter 现在为 `diff(left:int, right:int): int`
  这类 `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` + `FUNCTION_RETURN` 形态生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 - zr_aot_arg1)`。调用侧复用 two-arg i64 direct-call
  gate 和 scalar-local-only destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-subtract-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 subtract typed thunk source contract；GREEN
  为 call contracts 5/0、typed direct-call shared-library smoke 5/0；broader
  WSL GCC focused group 也通过 call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  备注：只覆盖 two-arg i64 减法返回；一般表达式返回、inline struct、in/out 写回、
  deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 07:04:09 +08:00 · M1.5 / 07-S5 static two-arg i64 typed
  thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别新增
  `return arg0 + arg1`；emitter 现在为 `sum(left:int, right:int): int`
  这类 `ADD_SIGNED` / `ADD_SIGNED_LOAD_STACK` + `FUNCTION_RETURN` 形态生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`。调用侧在证明
  `functionSlot + 1` 与 `functionSlot + 2` 均为已写入 i64 scalar local 后，
  发出 `zr_aot_static_i64_two_arg_direct_call`，并沿用 scalar-local-only
  destination stack-sync elision · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 two-arg typed thunk source contract；GREEN
  为 call contracts 5/0、typed direct-call shared-library smoke 4/0；broader
  WSL GCC focused group 也通过 call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  备注：只覆盖 two-arg i64 参数加法返回；一般多参数 ABI、一般返回、inline struct、
  in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 06:38:57 +08:00 · M1.5 / 07-S5 static one-arg i64
  arg+constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed thunk 识别从
  identity return 扩展到 `return arg0 + signed-constant`；emitter 现在识别
  SemIR 标量路径的 `ADD_SIGNED_LOAD_STACK_CONST` + `FUNCTION_RETURN`，并为
  `inc(value:int): int { return value + 1; }` 生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)` 的直接 C thunk。调用侧复用既有
  one-arg i64 typed direct-call route，scalar-local-only 结果仍通过上一切片的
  stack-sync elision 保持无 typed-destination `SZrTypeValue` 写 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-add-const-typed-thunk.md` ·
  验证：RED 为 call contracts 缺少 arg0+constant thunk source shape，随后 typed
  direct-call smoke 暴露 SemIR `ADD_SIGNED_LOAD_STACK_CONST` 形态未生成 thunk；
  GREEN 为 call contracts 5/0、typed direct-call shared-library smoke 3/0；broader
  WSL GCC focused group also passed call smoke 5/0、shared 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  备注：只覆盖 one-arg i64 参数加 signed constant 返回；multi-arg、一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍未完成。

- 2026-06-22 06:21:30 +08:00 · M1.5 / 07-S5 static i64 typed direct-call
  stack-sync elision 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：no-arg / one-arg typed direct-call writers receive
  `syncStackSlot`, and function-body routing now calls
  `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(...)` so scalar-only
  direct typed calls assign `zr_aot_sN` without materializing
  `SZrTypeValue *zr_aot_typed_destination` or emitting typed-destination
  `ZR_VALUE_FAST_SET`; frame-backed consumers keep the old sync fallback · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-typed-direct-call-stack-sync-elision.md` ·
  验证：RED 为 call contracts 缺少 `TZrBool syncStackSlot`；GREEN 为 call contracts
  5/0、typed direct-call shared-library smoke 2/0；broader WSL GCC focused group
  also passed call smoke 5/0、shared 8/0、value-type 1/0、power 2/0+1/0、
  source 19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、
  typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：只覆盖已证明 scalar-local-only
  的 no-arg/one-arg i64 typed direct-call destination sync elision；multi-arg,
  general typed returns, inline struct, in/out writeback, and deopt/dynamic
  bridge remain later 07-S5 work.

- 2026-06-22 05:56:00 +08:00 · M1.5 / 07-S5 static one-arg i64 typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：generated C now emits typed one-i64-parameter
  identity-return thunks (`zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`) and routes
  eligible static one-arg i64 calls directly to them after proving the call argument
  slot is an initialized i64 scalar local. The direct path assigns `zr_aot_sN` from
  the C argument call and keeps a temporary destination stack-slot sync for remaining
  frame-backed consumers · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-typed-thunk.md` · 验证：
  RED 为 call contracts 缺少 one-arg typed thunk predicate/writer/source shape，typed
  direct smoke 缺少 generated one-arg thunk/direct call；GREEN 为 call contracts 5/0、
  typed direct-call shared-library smoke 2/0；broader WSL GCC focused group also
  passed call 5/0、call smoke 5/0、shared 8/0、value-type 1/0、power 2/0+1/0、
  source 19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、
  return 1/0、frame setup 1/0 · 备注：one-arg identity return only; multi-arg,
  non-identity/general returns, inline struct, in/out writeback, deopt/dynamic bridge,
  and stack-sync removal remain later 07-S5 work.

- 2026-06-22 05:42:18 +08:00 · M1.5 / 07-S5 static no-arg i64 typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：generated C now emits static typed no-arg constant-i64
  thunks (`zr_aot_typed_i64_fn_N`) and routes eligible static no-arg i64
  calls directly to them, assigning `zr_aot_sN` and syncing the destination stack
  slot for remaining frame-backed consumers · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-no-arg-typed-thunk.md` · 验证：
  RED 为 call contracts 缺少 typed thunk predicate/writer/source shape，typed
  direct smoke 缺少 generated thunk/direct call；GREEN 为 call contracts 5/0、
  typed direct-call shared-library smoke 1/0、aggregate shared-library smoke 8/0 ·
  备注：仅覆盖 static no-arg constant i64 typed-to-typed 调用；typed 参数 ABI、一般返回、
  inline struct、in/out 写回、deopt/dynamic bridge 仍未完成。

- 2026-06-22 04:43:15 +08:00 · M1.5 / 07-S5 AOT MethodInfo table publication /
  module descriptor 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：AOT ABI v3 adds `ZrAotCompiledModule.methodInfos` and
  `methodInfoCount`; generated C emits `zr_aot_method_infos[]` from every
  `zr_aot_method_info_N`; the generated module descriptor publishes the table and count,
  allowing shared-library consumers to inspect non-null `module->methodInfos[0]->signature`
  descriptors · 产出：ABI/emitter/test updates and
  `tests/acceptance/2026-06-21-aot-m1-5-method-info-table-publication.md` · 验证：
  RED 为 shared-library smoke compile failure because the module descriptor lacked
  `methodInfos` / `methodInfoCount`; GREEN 为 frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0；broader WSL GCC group 通过 call contracts 4/0、call smoke 5/0、
  shared 8/0、value-type 1/0、power 2/0+1/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：仅发布
  MethodInfo/signature table；direct typed C call/return、in/out writeback、deopt/dynamic
  bridge 仍未完成。

- 2026-06-22 04:31:28 +08:00 · M1.5 / 07-S5 AOT MethodInfo native
  signature descriptor 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：AOT ABI now exposes `SZrAotSignatureType` /
  `SZrAotSignature`, generated C emits per-function signature constants from typed
  return/parameter metadata, and `SZrAotMethodInfo.signature` points to
  `&zr_aot_signature_N` instead of `ZR_NULL`. The value typed-call shared-library smoke
  locks a one-parameter callee signature descriptor for later typed-to-typed native
  routing · 产出：ABI/emitter/test updates and
  `tests/acceptance/2026-06-21-aot-m1-5-method-info-signature-descriptor.md` · 验证：
  RED 为 frame setup contracts 缺少 signature ABI/constant shape；GREEN 为 frame setup
  1/0、typed scalar 1/0、call shared-library smoke 5/0；broader WSL GCC group 通过 call
  4/0、call smoke 5/0、shared 8/0、value-type 1/0、power 2/0+1/0、source 19/0、
  generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return
  1/0、frame setup 1/0 · 备注：签名描述底座已完成，typed-to-typed 直调/直返、
  in/out 写回、deopt/dynamic bridge 仍属后续 07-S5 工作。

- 2026-06-22 04:08:25 +08:00 · M1.5 / 07-S5 TO_BOOL bool scalar-local
  declaration follow-up 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：conversion scalar-local kind proof now maps `TO_BOOL` to
  `ZR_AOT_SCALAR_LOCAL_KIND_BOOL`, making the generic conversion bool sync branch
  reachable for bool-local destinations · 产出：scalar-local proof update, source contracts,
  and `tests/acceptance/2026-06-21-aot-m1-5-to-bool-conversion-local-declaration.md` ·
  验证：RED 为 source contracts 缺少 `ZR_INSTRUCTION_OP_TO_BOOL` bool-kind mapping；GREEN
  为 source contracts 19/0、shared smoke 8/0；broader WSL GCC group 通过 call 4/0、call
  smoke 5/0、value-type 1/0、power 2/0+1/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：
  `backend_aot_c_scalar_locals.c` 已超大，本切片只做证明映射修正；后续应优先抽出
  scalar-local proof/result-skip 模块。

- 2026-06-22 04:01:40 +08:00 · M1.5 / 07-S5 generic primitive conversion
  bool/i64/u64/f64 scalar-local sync boundary helper 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：generic
  `TO_BOOL/TO_INT/TO_UINT/TO_FLOAT` emitters now receive function IR and emit
  bool/i64/u64/f64 `zr_aot_convert_generic_sync_*_local_boundary` markers plus
  `SyncBoolLocal()` / `SyncSignedIntLocal()` / `SyncUnsignedIntLocal()` /
  `SyncFloatLocal()` guards after `ZrLibrary_AotRuntime_ConvertGenericTo*()` when the
  destination is scalar-local · 产出：generic conversion lowering/header/function-body
  routing, source contracts, and
  `tests/acceptance/2026-06-21-aot-m1-5-generic-conversion-local-sync-boundary-helper.md` ·
  验证：RED 为 source contracts 缺少 generic conversion scalar-local include/sync markers；
  GREEN 为 source contracts 19/0、shared smoke 8/0；broader WSL GCC group 通过 call 4/0、
  call smoke 5/0、value-type 1/0、power 2/0+1/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：08-12 仍未开始，
  07-S5 继续推进剩余 typed-to-typed/in-out/deopt/dynamic boundary。

- 2026-06-22 03:50:11 +08:00 · M1.5 / 07-S5 generic power i64/u64/f64
  scalar-local sync boundary helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：generic `POW` emitter now receives function IR and
  emits i64/u64/f64 `zr_aot_generic_power_sync_*_local_boundary` markers plus
  `SyncSignedIntLocal()` / `SyncUnsignedIntLocal()` / `SyncFloatLocal()` guards after
  `ZrLibrary_AotRuntime_GenericPower()` when the destination is scalar-local · 产出：
  generic power lowering/header/function-body routing, power contracts, and
  `tests/acceptance/2026-06-21-aot-m1-5-generic-power-local-sync-boundary-helper.md` ·
  验证：RED 为 power contracts 缺少 generic power scalar-local include/sync markers；GREEN
  为 power contracts 2/0、power smoke 1/0；broader WSL GCC group 通过 call 4/0、call smoke
  5/0、shared 8/0、value-type 1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 备注：08-12 仍未开始，
  07-S5 继续推进剩余 typed-to-typed/in-out/deopt/dynamic boundary。

- 2026-06-22 03:40:06 +08:00 · M1.5 / 07-S5 generic numeric i64/u64/f64
  scalar-local sync boundary helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：generic numeric `ADD/SUB/MUL/DIV/MOD/NEG`
  emitters now receive function IR and use shared post-helper sync logic to emit
  i64/u64/f64 `zr_aot_generic_numeric_sync_*_local_boundary` markers plus
  `SyncSignedIntLocal()` / `SyncUnsignedIntLocal()` / `SyncFloatLocal()` guards when the
  destination is scalar-local · 产出：generic numeric lowering/header/function-body routing,
  generic numeric/source contracts, and
  `tests/acceptance/2026-06-21-aot-m1-5-generic-numeric-local-sync-boundary-helper.md` ·
  验证：RED 为 generic numeric contracts 缺少 `functionIr` 与 sync markers；GREEN 为
  generic numeric contracts 1/0、generic numeric smoke 1/0、source contracts 19/0；broader
  WSL GCC 组通过 call/shared/value-type/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup
  binaries · 备注：local-sync 覆盖为 source-contract/codegen 级，现有 smoke 仍验证 float
  `MOD` helper boundary 的共享库编译路径。07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 03:23:56 +08:00 · M1.5 / 07-S5 CallStackValue u64/f64 scalar-local sync
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic/direct `CallStackValue` call lowering now receives the current function IR and emits
  i64/bool/u64/f64 scalar-local restoration after `ZrLibrary_AotRuntime_CallStackValue(...)` via
  direct/dynamic `zr_aot_direct*_function_call_sync_*_local_boundary` markers and
  `SyncSignedIntLocal()` / `SyncBoolLocal()` / `SyncUnsignedIntLocal()` / `SyncFloatLocal()`
  helper guards; the old `zr_aot_direct_call_result` payload-read template remains rejected ·
  产出：call lowering/header/body dispatch, call contracts, call shared-library smoke, and
  `tests/acceptance/2026-06-21-aot-m1-5-call-stack-value-u64-f64-local-sync.md` · 验证：RED 为
  call contracts 缺少 direct/dynamic u64/f64 `CallStackValue` sync；typed `fn()` runtime smoke
  尝试揭示 unresolved callable return 仍为 `object` 的前端边界并未纳入本切片；GREEN 为 call
  contracts 4/0、call shared-library smoke 5/0；broader WSL GCC 组通过
  call/shared/value-type/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup
  binaries · 备注：u64/f64 direct/dynamic coverage is contract-level, executable smoke covers the
  current StackValue local-assignment path. 07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 03:03:12 +08:00 · M1.5 / 07-S5 static direct-call u64/f64 scalar-local sync
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  static direct call 后的 u64/f64 scalar-local 恢复现在经 `SyncUnsignedIntLocal()` /
  `SyncFloatLocal()` helper 边界完成，generated C 只保留
  `zr_aot_direct_static_function_call_sync_u64_local_boundary` /
  `zr_aot_direct_static_function_call_sync_f64_local_boundary` marker 和 helper guard，
  不再展开 `zr_aot_direct_call_result` native payload 回读；scalar-local analysis 现在识别 non-tail
  call result writes，并仅对“source 是 call-result destination”的 stack copy 执行目标标量类型回传，
  防止 inline-struct/value-type 槽被 broad copy propagation 误标量化 · 产出：call contracts/smoke、
  source contracts、call lowering、scalar-local evidence、
  `tests/acceptance/2026-06-21-aot-m1-5-static-direct-call-u64-f64-local-sync.md` · 验证：
  RED 为 call contracts 缺少 u64/f64 direct-call sync 与 shared-library smoke 缺少 u64 marker；
  GREEN 为 call contracts 4/0、call shared-library smoke 4/0；broader WSL GCC 组通过
  call/shared/value-type/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup binaries ·
  备注：`backend_aot_c_scalar_locals.c` 已超大，后续应抽 result-skip/liveness proof 模块。07-S5
  仍部分完成，08-12 未开始。

- 2026-06-22 02:28:40 +08:00 · M1.5 / 07-S5 typed power scalar-local result-skip 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  typed `POW_SIGNED` / `POW_UNSIGNED` / `POW_FLOAT` proven scalar-local paths now emit
  `zr_aot_arith_exec_*_power_scalar_local` and write local result variables directly; unproven paths keep
  frame/value fallback. Immediate constant scalar-local declarations are limited to constants that feed
  typed power operands, after a regression showed all-constant declaration broke call/value-slot boundary
  behavior · 产出：power contracts/smoke、typed power lowering、function-body/emitter signature updates、
  scalar-local evidence、`tests/acceptance/2026-06-21-aot-m1-5-typed-power-scalar-local-only.md` ·
  验证：RED 为 power contracts 缺少 scalar-local helper/include；GREEN 为 power contracts 2/0、
  power smoke 1/0；focused 回归组 call shared-library smoke 3/0、typed scalar 1/0、
  power contracts 2/0、power smoke 1/0 通过；broader focused group 通过 call/shared/power/source/
  generic numeric/global/logical/typed-scalar/return/frame-setup binaries · 备注：07-S5 仍部分完成，
  08-12 未开始。

- 2026-06-22 01:51:29 +08:00 · M1.5 / 07-S5 string equality bool-result scalar-local path 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  string `==` / `!=` 的 bool destination 可跳过 value-slot 时，generated C 生成
  `zr_aot_string_logical_bool_scalar_local` 并直接写 `zr_aot_bN`；未证明路径仍保留
  `zr_aot_string_logical_equal/not_equal` fallback 与 `ZR_VALUE_FAST_SET` · 产出：
  logical contracts 更新、generic logical lowering 和 scalar-local evidence 更新、
  `tests/acceptance/2026-06-21-aot-m1-5-string-equality-bool-local-only.md` ·
  验证：RED 为 logical contracts 缺少 string bool scalar-local helper；GREEN 为 logical contracts 4/0、
  logical shared-library smoke 4/0、source contracts 19/0、aggregate shared-library smoke 8/0；
  广域聚焦组拆分执行通过 call/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup
  合约与 smoke；generated string-equality fixture 含 local-only marker 并直接写 bool local ·
  备注：限定本次触及文件的 diff 检查通过；全仓仍有既有 LSP 文档 EOF 空行提示。07-S5 仍部分完成，
  08-12 未开始。

- 2026-06-22 01:42:03 +08:00 · M1.5 / 07-S5 bool binary logical scalar-local opcode path 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `LOGICAL_AND` / `LOGICAL_OR` opcode lowering 对已证明的 bool scalar-local 源和可跳过 value-slot
  的 bool destination 直接生成 `zr_aot_bool_binary_scalar_local`，写 `zr_aot_bN`，未证明路径保留
  现有 `zr_aot_bool_logical_and/or` fallback、tag check 和 `ZR_VALUE_FAST_SET` · 产出：
  logical contracts 更新、generic logical lowering 和 scalar-local evidence 更新、
  `tests/acceptance/2026-06-21-aot-m1-5-bool-binary-logical-local-only.md` ·
  验证：RED 为 logical contracts 缺少 bool binary scalar-local helper；GREEN 为 logical contracts 4/0、
  logical shared-library smoke 4/0、source contracts 19/0、aggregate shared-library smoke 8/0；
  广域聚焦组拆分执行通过 call/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup
  合约与 smoke；generator 静态检查含 scalar-local marker/proof/call-site instructionIndex，
  fallback 仍保留 · 备注：源码 `&&`/`||` 通常先降为短路分支，本切片锁定 opcode 直降生成器形态；
  07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 01:24:18 +08:00 · M1.5 / 07-S5 typed bool logical scalar-local direct path 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  typed bool `==` / `!=` / `!` 在 destination 可跳过 value-slot 且源 bool local 已写入时，
  generated C 直接写 `zr_aot_bN` 并保留 `zr_aot_bool_compare_scalar_local` /
  `zr_aot_bool_not_scalar_local` marker，不再展开 destination/source `SZrTypeValue` lookup、
  bool tag check 或 `ZR_VALUE_FAST_SET`；未证明路径继续走 `zr_aot_bool_*_exec` fallback，
  并用 declared bool destination 约束防止 generic conversion fallback 误用 bool local · 产出：
  source/logical smoke 合约更新、typed logical lowering 和 scalar-local evidence 更新、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-bool-logical-local-only.md` ·
  验证：RED 为 source contracts 缺少 bool scalar-local marker；GREEN 为 source contracts 19/0、
  logical shared-library smoke 4/0、aggregate shared-library smoke 8/0、call contracts 4/0、
  call shared-library smoke 3/0、power contracts 2/0、power smoke 1/0、
  generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical contracts 4/0、
  typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；generated bool-logical fixture
  含 scalar-local markers 且无 bool exec markers，generic-conversion fixture 保留 fallback 且无
  `zr_aot_b6` 误用 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:59:24 +08:00 · M1.5 / 07-S5 generic logical bool local sync boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic `LOGICAL_NOT` / `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL` 后的 bool scalar-local 恢复现在经
  `ZrLibrary_AotRuntime_SyncBoolLocal()` helper 边界完成，generated C 只保留
  `zr_aot_generic_logical_sync_bool_local_boundary` marker 和 helper guard，不再展开
  `zr_aot_bool_sync` slot lookup、bool tag 检查和 payload 回读模板 · 产出：
  logical contracts/smoke 合约更新、generic logical lowering 更新、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-logical-local-sync-boundary-helper.md` ·
  验证：RED 为 logical contracts 缺少 generic logical sync boundary marker；GREEN 为 logical contracts 4/0、
  logical shared-library smoke 4/0、call contracts 4/0、call shared-library smoke 3/0、
  aggregate shared-library smoke 8/0、power contracts 2/0、power smoke 1/0、source contracts 19/0、
  generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；generated generic-equality fixture 含 helper marker/call 且
  `zr_aot_bool_sync` 无命中 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:48:45 +08:00 · M1.5 / 07-S5 COPY_STACK scalar-local sync boundary helpers + sync module split 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `COPY_STACK` helper 后的 bool/i64/u64/f64 scalar-local 恢复现在经
  `ZrLibrary_AotRuntime_SyncBoolLocal()` / `SyncSignedIntLocal()` / `SyncUnsignedIntLocal()` /
  `SyncFloatLocal()` helper 边界完成，generated C 只保留
  `zr_aot_direct_stack_copy_sync_*_local_boundary` marker 和 helper guard，不再展开
  `zr_aot_direct_stack_copy_sync_destination` slot lookup、tag 检查和 payload 回读模板；
  local-sync helper 从 `aot_runtime_values.c` 拆入 `aot_runtime_sync.c`，保留 type mismatch no-op 语义 ·
  产出：source/call/shared-library smoke 合约更新、AOT runtime helper ABI/实现、value lowering 更新、
  `tests/acceptance/2026-06-21-aot-m1-5-copy-stack-local-sync-boundary-helpers.md` ·
  验证：RED 为 source contracts 缺少新 sync runtime/helper-only 形态；GREEN 为 source contracts 19/0、
  aggregate shared-library smoke 8/0、call contracts 4/0、call shared-library smoke 3/0、
  power contracts 2/0、power smoke 1/0、generic numeric contracts/smoke 1/0+1/0、
  global smoke 9/0、logical smoke 4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；
  generated numeric-arithmetic fixture 含 stack-copy sync helper marker/call 且
  `zr_aot_direct_stack_copy_sync_destination` 无命中 · 备注：`aot_runtime_values.c` 降至 877 行，
  新增 `aot_runtime_sync.c` 109 行；07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:33:42 +08:00 · M1.5 / 07-S5 static direct call scalar-local sync boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  static direct call 后的 i64/bool scalar-local 恢复现在经
  `ZrLibrary_AotRuntime_SyncSignedIntLocal()` / `ZrLibrary_AotRuntime_SyncBoolLocal()` helper 边界完成，
  generated C 只保留 `zr_aot_direct_static_function_call_sync_*_local_boundary` marker 和 helper guard，
  不再展开 `zr_aot_direct_call_result` slot lookup、tag 检查和 native payload 回读模板；bool helper 保留旧语义，
  source 非 bool 时 no-op · 产出：call/source/shared-library smoke 合约更新、AOT runtime helper ABI/实现、
  call lowering 更新、`tests/acceptance/2026-06-21-aot-m1-5-static-direct-call-local-sync-boundary-helper.md` ·
  验证：RED 为 call contracts 缺少 i64 sync boundary marker；GREEN 为 call contracts 4/0、
  call shared-library smoke 3/0、aggregate shared-library smoke 8/0、power contracts 2/0、power smoke 1/0、
  source contracts 19/0、generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical smoke 4/0、
  typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；generated numeric-arithmetic fixture 含
  `SyncSignedIntLocal` marker/call 且 `zr_aot_direct_call_result` 无命中 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:10:45 +08:00 · M1.5 / 07-S5 generic POW boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic `POW` lowering 现在只保留 `zr_aot_generic_power_boundary` marker 与
  `ZrLibrary_AotRuntime_GenericPower()` helper guard；runtime helper 集中处理 frame/slot 校验、
  `ZR_META_POW` 查询、no-meta/null 目标写回，以及 meta function 存在时的 unsupported failure；
  generated C 不再展开 `SZrMeta` locals、direct meta lookup/reset/debug-error 模板 · 产出：
  `tests/parser/test_aot_c_power_contracts.c`、`tests/parser/test_aot_c_power_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_power.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-power-boundary-helper.md` ·
  验证：RED 为 power contracts 缺少 `zr_aot_generic_power_boundary`；GREEN 为 power contracts 2/0、
  power shared-library smoke 1/0、source contracts 19/0、generic numeric contracts 1/0、
  generic numeric shared-library smoke 1/0、shared-library smoke 8/0、call shared-library smoke 3/0、
  global shared-library smoke 9/0、logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；generated power fixture 含 helper marker/call，旧 generated meta lookup/reset/debug
  模板无命中 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 23:57:30 +08:00 · M1.5 / 07-S5 generic numeric boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic numeric arithmetic lowering (`ADD`/`SUB`/`MUL`/`DIV`/`MOD`/`NEG`) 现在只保留
  boundary marker 与 `ZrLibrary_AotRuntime_GenericNumeric*()` helper guard；runtime helper 集中处理
  frame/slot 校验、numeric primitive tag 分支、float/signed/unsigned 运算、`divide by zero` /
  `modulo by zero` 失败、generic float `MOD` 的 `fmod(leftFloat, rightFloat)` 和 unsupported
  primitive failure，generated C 不再展开 direct `SZrTypeValue` locals、tag branches、zero guards 或
  arithmetic expression 模板 · 产出：`tests/parser/test_aot_c_source_contracts.c`、
  `tests/parser/test_aot_c_generic_numeric_contracts.c`、
  `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-numeric-boundary-helpers.md` ·
  验证：RED 为 source contracts 缺少 `zr_aot_arith_exec_generic_numeric_binary_boundary`；GREEN 为
  source contracts 19/0、generic numeric contracts 1/0、generic numeric shared-library smoke 1/0、
  shared-library smoke 8/0、call shared-library smoke 3/0、global shared-library smoke 9/0、
  logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；
  generated generic numeric mod fixture 含 helper marker/call，旧 direct `fmod(zr_aot_left_float, ...)`
  与 generated tag/zero-guard 模板无命中 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 23:39:10 +08:00 · M1.5 / 07-S5 GET_SUB_FUNCTION native closure boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  zero-capture `GET_SUB_FUNCTION` C fast path 现在只生成
  `zr_aot_value_get_sub_function_native_closure_boundary` marker 与
  `ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state, &frame, destinationSlot, childFunctionIndex, callableFlatIndex, zr_aot_fn_N)`
  helper guard；runtime helper 负责 frame/function/destination 校验、child metadata 查找、destination
  ownership release、native closure 分配初始化、generated thunk 绑定、AOT shim metadata 绑定和 native
  closure value flags，captured/unresolved/missing child materialization 路径仍保持显式 unsupported failure ·
  产出：`tests/parser/test_aot_c_constant_contracts.c`、
  `tests/parser/test_aot_c_global_shared_library_smoke.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-get-sub-function-native-closure-boundary-helper.md` ·
  验证：RED 为 `zr_vm_aot_c_constant_contracts_test` 缺少
  `zr_aot_value_get_sub_function_native_closure_boundary` marker；GREEN 为 constant contracts 4/0、
  source contracts 19/0、shared-library smoke 8/0、call shared-library smoke 3/0、global shared-library
  smoke 9/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；生成的
  `aot_c_get_sub_function_native_closure_smoke.c` 含 helper marker/call 和 `zr_aot_fn_1` thunk，
  旧 `zr_aot_value_exec_get_sub_function_native_closure` / direct `ZrCore_ClosureNative_New` /
  `nativeFunction = zr_aot_fn` 模板无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF warning ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 23:17:41 +08:00 · M1.5 / 07-S5 generic logical boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `LOGICAL_NOT` / `JUMP_IF` / `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL` generic primitive
  logical lowering 现在只保留 `zr_aot_generic_logical_*` marker、`ZR_AOT_C_GUARD(...)`
  和必要的 bool scalar-local 回读同步；primitive truthiness/equality 的
  null/bool/signed/unsigned/float 标签判断与 unsupported primitive failure 集中到
  `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy/LogicalNot/LogicalEqual/LogicalNotEqual()`
  dedicated runtime helpers，避免复用 broader `LogicalNot` / `IsTruthy` / `LogicalEqual`
  动态语义 helper · 产出：`tests/parser/test_aot_c_logical_contracts.c`、
  `tests/parser/test_aot_c_logical_shared_library_smoke.c`、`zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-logical-boundary-helpers.md` ·
  验证：RED 为 `zr_vm_aot_c_logical_contracts_test` 缺少新 GenericPrimitive helper 调用；
  GREEN 为 logical contracts 4/0、logical shared-library smoke 4/0、source contracts 19/0、
  shared-library smoke 8/0、call shared-library smoke 3/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；刷新后的 generic truthiness/equality generated C 含
  `ZrLibrary_AotRuntime_GenericPrimitive*` helper calls，旧 direct primitive truthiness/equality
  模板和 broader helper 调用无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF warning ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 22:52:02 +08:00 · M1.5 / 07-S5 generic primitive conversion boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `TO_BOOL` / `TO_INT` / `TO_UINT` / `TO_FLOAT` generic primitive conversion lowering 现在只保留
  `zr_aot_convert_generic_to_*` marker 与 `ZrLibrary_AotRuntime_ConvertGenericTo*()` helper guard，
  不再展开 generated `SZrTypeValue *zr_aot_destination/source` locals、`ZR_VALUE_IS_TYPE_*`
  分支、`ZR_VALUE_FAST_SET(...)` 或 generated unsupported-conversion block；runtime 侧新增独立
  `ConvertGenericTo*` helper，避免误用带 meta hook 语义的 `ZrLibrary_AotRuntime_To*()` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-generic-conversion-boundary-helpers.md` ·
  RED/GREEN：source contract 先因旧 direct conversion 模板缺 helper 失败；首次复用 `ToBool()`
  后 shared-library smoke 触发崩溃，`gdb` 定位到 `ToBool()` 进入 meta-conversion 路径，与旧
  primitive conversion 语义不等价；改为专用 `ConvertGenericTo*()` helpers 后 source contracts 19/0、
  aggregate shared-library smoke 8/0、call shared-library smoke 3/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；generic-conversion generated fixture 含 `ConvertGenericToBool` helper call，
  旧 direct conversion 模板和 `To*()` helper 复用无命中 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 22:24:19 +08:00 · M1.5 / 07-S5 publish exports boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  export-tail publication 现在只生成 `zr_aot_publish_exports_boundary` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PublishModuleExports(state, &frame));`，
  不再展开 `zr_aot_publish_exports_direct`、直接 `ZrCore_Value_Copy(...)`、closure capture publication、
  `ZrCore_Module_AddPubExport(state, frame.module, ...)` 或 `moduleExecuted` 直写模板；runtime helper 复用
  `aot_runtime_materialize_exports()` 并在 `frame.recordHandle` 已由 07-S3 elide 时按 `frame.function`
  回查 loaded-module record · 产出：`tests/acceptance/2026-06-21-aot-m1-5-publish-exports-boundary-helper.md` ·
  RED/GREEN：return contract 先因缺 `zr_aot_publish_exports_boundary` 失败；补 lowering 与 runtime
  record fallback 后 return contracts 1/0、source contracts 19/0、aggregate shared-library smoke 8/0、
  call shared-library smoke 3/0、frame setup contracts 1/0、typed scalar 1/0；generated shared/call `.c`
  fixtures 含 publish helper marker/call 且旧 direct export publication 模板无命中 · 备注：07-S5 仍部分完成，
  08-12 未开始。
- 2026-06-21 21:57:47 +08:00 · M1.5 / 07-S5 COPY_STACK boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `SET_STACK` fallback C lowering 现在通过 `ZrLibrary_AotRuntime_CopyStack()` helper 执行 frame-slot 校验、
  inline-struct copy、object-to-inline copy、value-slot copy 与 materialized stack-value assignment，生成器只保留
  `zr_aot_value_exec_copy_stack` marker、helper guard、以及必要的 scalar-local sync，不再展开
  `FindFrameSlotLayout` / `CopyFrameSlotInline` / `CopyObjectValueToFrameSlotInline` / `AssignMaterializedStackValue`
  模板；scalar-local sync 改为从 helper-owned destination slot 重新读取 `zr_aot_direct_stack_copy_sync_destination` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-copy-stack-boundary-helper.md` · RED/GREEN：
  source contract 先因缺 `zr_aot_value_exec_copy_stack` helper-only marker 失败；补 runtime helper 与 lowering 后
  source contracts 19/0、aggregate shared-library smoke 8/0、typed scalar 1/0、call shared-library smoke 3/0；
  generated shared/call `.c` fixtures 含 `ZrLibrary_AotRuntime_CopyStack(...)` 且旧 direct stack-copy / inline-struct
  copy / materialized-copy 模板无命中；CTest 过滤仅匹配 `aot_c_typed_scalar` 并通过 1/1；`git diff --check`
  退出 0，仅有既有 LF/CRLF warning · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 21:40:05 +08:00 · M1.5 / 07-S5 RESET_STACK_NULL/RESET_STACK_NULL2 boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `RESET_STACK_NULL` / `RESET_STACK_NULL2` fallback C lowering 现在通过新增
  `ZrLibrary_AotRuntime_ResetStackNull/ResetStackNull2()` helpers 执行 frame-slot 校验与 null reset，
  生成器只保留 marker 与 helper guard，不再展开 reset fallback 的 `SZrTypeValue *` locals 或 direct
  `ZrCore_Value_ResetAsNull(...)` 模板；scalar-local skip 路径保持 marker-only；runtime helper 拆入
  `aot_runtime_values.c`，未继续扩张 7k+ 行 `aot_runtime.c` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null-boundary-helpers.md` · RED/GREEN：
  source contract 先因 helper source 缺失失败；补 helper 与 lowering 后 source contracts 19/0、aggregate
  shared-library smoke 8/0、typed scalar 1/0、call shared-library smoke 3/0；generated `.c` fixtures 含
  `ResetStackNull*` helper calls 且旧 reset2 local template 无命中；CTest 过滤仅匹配 `aot_c_typed_scalar`
  并通过 1/1；`git diff --check` 退出 0，仅有既有 LF/CRLF warning · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 21:19:49 +08:00 · M1.5 / 07-S5 closure value boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `GET_CLOSURE` / `SET_CLOSURE` / `GETUPVAL` / `SETUPVAL` C lowering 现在通过
  `ZrLibrary_AotRuntime_GetClosureValue/SetClosureValue()` helpers 执行 active closure capture
  解析、frame-slot 校验、copy/barrier 与失败上报，生成器只保留 marker 与 helper guard，不再展开
  current-closure lookup、native/VM closure decode、capture read/write 或 setter barrier 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-closure-value-boundary-helpers.md` · RED/GREEN：
  constant contract 先因缺 `GetClosureValue` helper guard 失败；改 lowering 后 constant contracts 4/0、
  global shared-library smoke 8/0；更宽 focused WSL 组通过 source contracts 19/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；
  generated closure fixture 含四个 helper calls 且旧 direct closure/capture/barrier 模板无命中；
  `ctest -R 'aot_c_constant|aot_c_global'` exit 0 但本 build 无注册匹配；`git diff --check` exit 0，
  仅有既存 LF/CRLF warning · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 21:09:08 +08:00 · M1.5 / 07-S5 CREATE_OBJECT/CREATE_ARRAY boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `CREATE_OBJECT` / `CREATE_ARRAY` C lowering 现在通过 `ZrLibrary_AotRuntime_CreateObject/CreateArray()` helpers 执行
  frame/destination/object-allocation/result-write 校验与失败上报，生成器只保留 marker 与 helper guard，不再展开
  destination `SZrTypeValue` lookup、direct object/array allocation、ownership release、raw-object/null result 写入或
  array type-tag 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-object-array-creation-boundary-helpers.md` · RED/GREEN：
  global contract 先因缺 `zr_aot_value_exec_create_object` marker 失败；改 lowering 后 global contracts 7/0、
  global shared-library smoke 8/0；更宽 focused WSL 组通过 source contracts 19/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；
  generated object-array fixture 含两个 helper calls 且旧 direct object/array creation 模板无命中；`ctest -R 'aot_c_global'`
  exit 0 但本 build 无注册匹配；`git diff --check` exit 0，仅有既存 LF/CRLF warning ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 20:54:14 +08:00 · M1.5 / 07-S5 TO_STRING boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `TO_STRING` C lowering 现在通过 `ZrLibrary_AotRuntime_ToString()` helper 执行
  frame/slot/value/conversion/frame-refresh/string-result 校验与失败上报，生成器只保留 marker 与 helper guard，不再展开
  source/destination `SZrTypeValue` lookup、direct `ZrCore_Value_ConvertToString`、frame refresh 或 raw-object/null result
  写入模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-to-string-boundary-helper.md` · RED/GREEN：
  global contract 先因缺 `ToString` helper guard 失败；改 lowering 后 global contracts 6/0、
  global shared-library smoke 7/0；更宽 focused WSL 组通过 source contracts 19/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；
  generated to-string fixture 含 helper call 且旧 direct string conversion 模板无命中；`ctest -R 'aot_c_global'`
  exit 0 但本 build 无注册匹配；`git diff --check` exit 0，仅有既存 LF/CRLF warning ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 20:42:05 +08:00 · M1.5 / 07-S5 TO_OBJECT/TO_STRUCT boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `TO_OBJECT` / `TO_STRUCT` C lowering 现在通过 `ZrLibrary_AotRuntime_ToObject/ToStruct()` helper 执行
  frame/slot/value/type-name/core conversion 校验与失败上报，生成器只保留 marker 与 helper guard，不再展开
  destination/source `SZrTypeValue` lookup、type-name constant lookup 或 direct `ZrCore_Execution_ToObject/ToStruct`
  模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-object-struct-boundary-helpers.md` · RED/GREEN：
  global contract 先因缺 `ToObject` helper guard 失败；改 lowering 后 global contracts 6/0、
  global shared-library smoke 7/0；更宽 focused WSL 组通过 source contracts 19/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；
  generated object/struct fixture 含两个 helper calls 且旧 direct core conversion 模板无命中；
  `ctest -R 'aot_c_global'` 无注册匹配测试；`git diff --check` 退出 0，仅有既有 LF/CRLF 提示 ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 20:22:27 +08:00 · M1.5 / 07-S5 TYPEOF boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `TYPEOF` C lowering 现在通过 `ZrLibrary_AotRuntime_TypeOf()` helper 执行
  frame/slot/value/reflection 校验与失败上报，生成器只保留 marker 与 helper guard，不再展开
  destination/source `SZrTypeValue` lookup 或 direct `ZrCore_Reflection_TypeOfValue` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-typeof-boundary-helper.md` · RED/GREEN：
  global contract 先因缺 `TypeOf` helper guard 失败；改 lowering 后 global contracts 6/0、
  global shared-library smoke 7/0；更宽 focused WSL 组通过 source contracts 19/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts
  1/0；generated typeof fixture 含 helper call 且旧 direct reflection 模板无命中；
  `ctest -R 'aot_c_global'` 无注册匹配测试；`git diff --check` 退出 0，仅有既有 LF/CRLF 提示 ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 20:13:10 +08:00 · M1.5 / 07-S5 GET_GLOBAL boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `GET_GLOBAL` C lowering 现在通过 `ZrLibrary_AotRuntime_GetGlobal()` helper 执行
  frame/slot/value/global-object 校验与 copy/null fallback，生成器只保留 marker 与 helper guard，
  不再展开 `SZrTypeValue` destination lookup、`zr_aot_global_object`、global type check 或 direct
  `ZrCore_Value_Copy/ResetAsNull` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-get-global-boundary-helper.md` · RED/GREEN：
  global contract 先因缺 `zr_aot_value_exec_get_global` marker 失败；改 lowering 后
  global contracts 6/0、global shared-library smoke 7/0；更宽 focused WSL 组通过 source
  contracts 19/0、aggregate shared-library smoke 8/0、return contracts 1/0、value SemIR
  contracts 4/0、control contracts 1/0；generated get-global fixture 含 helper call 且旧 direct
  value-copy 模板无命中；`ctest -R 'aot_c_global'` 无注册匹配测试；`git diff --check`
  退出 0，仅有既有 LF/CRLF 提示 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 19:55:10 +08:00 · M1.5 / 07-S5 iterator boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  iterator C lowering 现在通过 `ZrLibrary_AotRuntime_IterInit/IterMoveNext/IterCurrent()`
  与新增 `IterMoveNextJumpIfFalse()` helper 执行 frame/slot/value/bool-branch 校验和 core
  iterator 语义，生成器只保留 marker、helper guard 和 branch goto，不再展开 `SZrTypeValue`
  slot lookup、cached iterator fast path 或 direct `ZrCore_Object_Iter*` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-iterator-boundary-helpers.md` · RED/GREEN：
  iterator contract 先因缺 `IterMoveNextJumpIfFalse` helper 失败；补 helper 与 lowering 后
  iterator contracts 1/0、iterator shared-library smoke 1/0；更宽 focused WSL 组通过 source
  contracts 19/0、aggregate shared-library smoke 8/0、return contracts 1/0、value SemIR
  contracts 4/0、control contracts 1/0；generated iterator fixture 含全部 iterator helper
  calls 且旧 direct fast/core 展开模板无命中；`ctest -R 'aot_c_iterator'` 无注册匹配测试 ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 19:35:05 +08:00 · M1.5 / 07-S5 super-array integer boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  super-array integer C lowering 现在通过 `ZrLibrary_AotRuntime_SuperArray*()` helper 执行
  frame/slot/value/constant 校验和 core super-array 语义，生成器只保留 marker 与 helper guard，
  不再展开 `SZrTypeValue` slot lookup、fast-path applicability、constant extraction 或 direct
  `ZrCore_Object_SuperArray*` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-super-array-boundary-helpers.md` · RED/GREEN：
  super-array contract 先因缺 helper guard 失败；改 lowering 后 super-array contracts 1/0、
  super-array shared-library smoke 1/0；更宽 focused WSL 组通过 source contracts 19/0、
  aggregate shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、
  control contracts 1/0；generated super-array fixture 含全部 `SuperArray*` helper calls
  且旧 direct fast/core 展开模板无命中；`ctest -R 'aot_c_super_array'` 无注册匹配测试 ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 19:14:29 +08:00 · M1.5 / 07-S5 scope lifecycle boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `MARK_TO_BE_CLOSED` / `CLOSE_SCOPE` C lowering 现在通过
  `ZrLibrary_AotRuntime_MarkToBeClosed/CloseScope()` helper 执行 slot 校验、to-be-closed
  登记与 close-scope closure/stack 维护，生成器只保留 scope marker 与 helper guard，不再展开
  closure/stack direct-core 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-scope-boundary-helpers.md` · RED/GREEN：
  scope contract 先因缺 helper guard 失败；改 lowering 后 scope contracts 1/0、scope
  shared-library smoke 1/0；更宽 focused WSL 组通过 source contracts 19/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control
  contracts 1/0；generated scope fixture 含两个 helper 且旧 closure/stack 展开模板无命中；
  `ctest -R 'aot_c_scope'` 无注册匹配测试；`git diff --check` 退出 0，仅有既有 LF/CRLF
  提示 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 18:50:41 +08:00 · M1.5 / 07-S5 ownership boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  所有 C ownership lowering 现在通过 `ZrLibrary_AotRuntime_Own*()` helper 执行 frame/slot/value
  校验与 core ownership 调用，生成器只保留 marker 与 helper guard，不再展开 direct
  ownership-core 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-ownership-boundary-helpers.md` · RED/GREEN：
  ownership contract 先要求 helper-only ownership 契约并暴露旧 direct-core emitter；
  generator 改写后 PASS · 测试结果：WSL focused 组通过 source contracts 19/0、
  ownership contracts 1/0、ownership shared-library smoke 1/0、aggregate shared-library smoke 8/0、
  return contracts 1/0、value SemIR contracts 4/0；generated ownership C 含 9 个 `Own*`
  helper 且旧 ownership-core 展开模板无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF
  提示 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 18:29:21 +08:00 · M1.5 / 07-S5 OWN_RETURN_LOAN boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `OWN_RETURN_LOAN` lowering 现在通过 `ZrLibrary_AotRuntime_OwnReturnLoan()` helper 执行
  frame/slot/value 校验与 `ZrCore_Ownership_ReturnLoanValue` 调用，生成器只保留 marker 与
  helper guard；其他 ownership lowering 保持既有 direct-core 形态 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-own-return-loan-boundary-helper.md` · RED/GREEN：
  ownership contract 先要求 helper-only return-loan 契约并暴露缺少 runtime helper；helper 与
  generator 改写后 PASS · 测试结果：WSL focused 组通过 source contracts 19/0、
  ownership contracts 1/0、ownership shared-library smoke 1/0、aggregate shared-library smoke 8/0、
  return contracts 1/0、value SemIR contracts 4/0；generated ownership C 含 `OwnReturnLoan`
  helper 且旧 return-loan core 展开模板无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF
  提示 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 18:14:46 +08:00 · M1.5 / 07-S5 frame-backed direct-return boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  frame-backed `FUNCTION_RETURN` lowering 现在通过 `ZrLibrary_AotRuntime_Return()` helper 执行
  source/caller value 取址、exception-handler discard、functionTop stack guard、return escape、
  closure close、inline constructor receiver copy-back、constructor result-copy skip、caller result copy
  与最终 stackTop reset，生成器只保留 marker 与 cleanup-exit helper call；export tail return
  仍先 direct publication，scalar i64 local return 仍走 `ReturnI64()` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-direct-return-boundary-helper.md` · RED/GREEN：
  return contract 先要求 helper-owned return 语义并禁止旧 generated result/caller 展开；
  helper 与 generator 改写后 PASS · 测试结果：WSL focused 组通过 source contracts 19/0、
  aggregate shared-library smoke 8/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0；generated frame-backed direct-return C 含 `Return(state, &frame, 0, ZR_FALSE)`
  且旧 direct-return 展开模板无命中 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 17:51:42 +08:00 · M1.5 / 07-S5 pending-control boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `SET_PENDING_RETURN/BREAK/CONTINUE` lowering 现在通过
  `ZrLibrary_AotRuntime_SetPendingReturn/Break/Continue()` helper 执行 pending-control 设置、
  outer-finally resume、target jump、cleanup、frame refresh 与 resume-index 计算，生成器只保留
  marker、helper guard 和 resume dispatch；旧 generated pending-value local 与
  `execution_set_pending_control` / `execution_resume_pending_via_outer_finally` /
  `execution_jump_to_instruction_offset` 展开模板消失 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-pending-control-boundary-helper.md` · RED/GREEN：
  source contract 与 aggregate shared-library smoke 先切到 helper-only pending-control 断言；
  生成器改写后 PASS · 测试结果：WSL focused 组通过 source contracts 19/0、
  aggregate shared-library smoke 8/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0；generated pending-control C 含三条 `SetPending*` helper
  与 resume-dispatch guard，旧展开模板无命中 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 17:26:50 +08:00 · M1.5 / 07-S5 END_FINALLY boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  END_FINALLY lowering 现在通过 `ZrLibrary_AotRuntime_EndFinally()` helper 执行 handler pop、
  pending exception unwind、pending return/break/continue resume、pending return-value copy、
  frame refresh 与 resume-index 计算，生成器只保留 helper guard 和 resume dispatch；
  未捕获 pending exception 传播也在 helper 内保留，替代后的未用内部 exception-resume helper
  已删除 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-end-finally-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 helper-only END_FINALLY boundary 并禁止旧 generated switch/local
  模板；runtime source contract 同步锁住未捕获异常传播；control shared-library smoke
  同步断言后 PASS · 测试结果：WSL focused 组通过 source contracts 19/0、return
  contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、
  call contracts 4/0、call shared-library smoke 3/0、control contracts 1/0、
  control shared-library smoke 1/0、global contracts 6/0、global shared-library smoke 7/0；
  generated control smoke C 含 END_FINALLY helper 与 resume-dispatch guard，旧 generated
  pending-control switch/local 模板消失；CTest 过滤仅匹配已注册 `aot_c_typed_scalar`
  并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 16:59:30 +08:00 · M1.5 / 07-S5 THROW boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  THROW lowering 现在通过 `ZrLibrary_AotRuntime_Throw()` helper 执行 payload 校验、
  exception normalize、unwind/frame refresh 与 resume-index 计算，生成器只保留 helper
  guard 和 resume dispatch；未捕获异常传播也在 helper 内保留 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-throw-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 helper-only THROW boundary 并禁止旧 generated 模板；
  control shared-library smoke 同步断言后 PASS · 测试结果：WSL focused 组通过 source
  contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0；CTest 过滤仅匹配已注册 `aot_c_typed_scalar`
  并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 16:44:43 +08:00 · M1.5 / 07-S5 CATCH boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  CATCH lowering 现在通过 `ZrLibrary_AotRuntime_Catch()` helper 执行 destination
  slot 校验、current-exception copy/clear、null reset 与 pending-control cleanup，
  生成器只发一条 helper guard，不再展开 exception-copy/reset 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-catch-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 helper-only CATCH boundary 并禁止旧 generated 模板；
  control shared-library smoke 同步断言后 PASS · 测试结果：WSL focused 组通过 source
  contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0；CTest 过滤仅匹配已注册 `aot_c_typed_scalar`
  并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 16:30:20 +08:00 · M1.5 / 07-S5 END_TRY boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  END_TRY lowering 现在通过 `ZrLibrary_AotRuntime_EndTry()` helper 执行 handler lookup、
  finally phase/handler pop 与 frame/call-info 同步，生成器只发一条 helper guard，不再展开
  handler-state/finally-phase 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-end-try-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 helper-only END_TRY boundary 并禁止旧 generated 模板；
  control shared-library smoke 同步断言后 PASS · 测试结果：WSL focused 组通过 source
  contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 16:20:39 +08:00 · M1.5 / 07-S5 TRY boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  TRY lowering 现在通过 `ZrLibrary_AotRuntime_Try()` helper 执行 handler push 与
  frame/call-info 同步，生成器只发一条 helper guard，不再展开 `SZrCallInfo` 恢复、
  `execution_push_exception_handler()` 或 TRY-specific `RunError` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-try-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 helper-only TRY boundary 并禁止旧 generated 模板；
  control shared-library smoke 同步断言后 PASS · 测试结果：WSL focused 组通过 source
  contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0；CTest 过滤仅匹配已注册 `aot_c_typed_scalar`
  并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 16:08:08 +08:00 · M1.5 / 07-S5 unsupported instruction boundary
  helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：unsupported instruction 现在通过既有
  `ZrLibrary_AotRuntime_ReportUnsupportedInstruction()` 报告，并由生成代码用
  `ZR_AOT_C_RETURN(...)` 进入 cleanup exit；生成器不再展开 instruction/opcode locals、
  hand-written unsupported-instruction `RunError` 或 `ZR_AOT_C_FAIL()` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-instruction-boundary-helper.md` ·
  RED/GREEN：source contracts 先要求 helper cleanup-exit shape 并禁止旧 inline failure
  模板；shared-library smoke 的 unsupported instruction boundary 子项同步断言后 PASS ·
  测试结果：WSL focused 组通过 source contracts 19/0、return contracts 1/0、frame setup
  contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、call
  shared-library smoke 3/0、global contracts 6/0、global shared-library smoke 7/0；
  CTest 过滤仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1 · 备注：当前完整
  `zr_vm_aot_c_shared_library_smoke_test` 在 dirty checkout 中仍有两个非本片执行类失败；
  unsupported instruction boundary 子项已 PASS；07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 15:41:20 +08:00 · M1.5 / 07-S5 unsupported dynamic value access boundary
  helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：unsupported dynamic member/index access 现在通过
  `ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess()` 边界 helper 执行当前 frame slot
  校验和 unsupported failure，生成器只发 helper guard，不再展开 opcode local、
  primary/secondary `SZrTypeValue *` locals、operand-index local 或 dynamic-specific
  `RunError` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-dynamic-value-access-boundary-helper.md` ·
  RED/GREEN：global contracts 先要求 helper-only dynamic value access boundary 并禁止旧
  inline failure 模板；global shared-library smoke 同步从旧 generated-C 文本切到 helper
  guard 后 GREEN · 测试结果：WSL focused 组通过 source contracts 19/0、return contracts
  1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call
  contracts 4/0、call shared-library smoke 3/0、global contracts 6/0、global
  shared-library smoke 7/0；CTest 过滤仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 15:25:37 +08:00 · M1.5 / 07-S5 unsupported meta value access boundary
  helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：unsupported meta value access 现在通过
  `ZrLibrary_AotRuntime_UnsupportedMetaValueAccess()` 边界 helper 执行当前 frame slot
  校验和 unsupported failure，生成器只发 helper guard，不再展开 opcode local、
  primary/secondary `SZrTypeValue *` locals、member/cache local 或 meta-specific
  `RunError` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-meta-value-access-boundary-helper.md` ·
  RED/GREEN：global contracts 先要求 helper-only meta value access boundary 并禁止旧
  inline failure 模板；global shared-library smoke 同步从旧 generated-C 文本切到 helper
  guard 后 GREEN · 测试结果：WSL focused 组通过 source contracts 19/0、return contracts
  1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call
  contracts 4/0、call shared-library smoke 3/0、global contracts 6/0、global
  shared-library smoke 7/0；CTest 过滤仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 15:06:55 +08:00 · M1.5 / 07-S5 unsupported meta call boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  unsupported meta-call 现在通过 `ZrLibrary_AotRuntime_UnsupportedMetaCall()` 边界 helper
  执行当前 slot 校验和 unsupported failure，生成器只发 helper guard，不再展开 argument/receiver/
  destination `SZrTypeValue *` 模板；同时补齐 stack-copy scalar kind 继承证明，恢复 focused
  typed-scalar frame-free · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-meta-call-boundary-helper.md` ·
  RED/GREEN：call contracts 先要求 helper-only meta-call boundary 并禁止旧模板；broader validation
  先暴露 f64 stack-copy proof 导致的 typed-scalar frame setup 回归，补证明后 GREEN · 测试结果：
  WSL focused 组通过 source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke
  3/0；CTest 过滤仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成，08-12
  未开始。
- 2026-06-21 14:41:47 +08:00 · M1.5 / 07-S5 call stack value boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic/dynamic stack-value function call 现在通过 `ZrLibrary_AotRuntime_CallStackValue()`
  边界 helper 执行当前 call-base/destination anchor、`CallAndRestoreAnchor`、结果槽复制和
  caller frame 恢复；生成器只发 helper guard，不再展开旧 stack-anchor call 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-call-stack-value-boundary-helper.md` ·
  RED/GREEN：call contracts 中 dynamic/generic 用例先要求 helper-only call boundary 并禁止旧模板，
  补 runtime helper 与生成器改写后 GREEN · 测试结果：WSL focused 组通过 source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR
  contracts 4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 14:25:45 +08:00 · M1.5 / 07-S5 static direct call boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  static direct-call 现在通过 `ZrLibrary_AotRuntime_CallStaticDirect()` 边界 helper 执行当前
  VM call-frame 准备、VALUE 参数源槽物化、callee thunk 调用、`PostCall` 和 caller frame 恢复；
  生成器只发 helper guard，不再展开旧 static direct-call prepared-call 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-direct-call-boundary-helper.md` ·
  RED/GREEN：call contracts 先要求 helper-only static direct-call boundary 并禁止旧模板，
  补 runtime helper 与生成器改写后 GREEN · 测试结果：WSL focused 组通过 source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR
  contracts 4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 14:12:31 +08:00 · M1.5 / 07-S5 call inline struct boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  value SemIR typed inline-struct call 现在通过 `ZrLibrary_AotRuntime_CallInlineStruct()`
  边界 helper 执行当前 VM call-frame 准备、VALUE 参数源槽物化、callee thunk 调用、`PostCall`
  和 caller frame 恢复；生成器只发 helper guard，不再把 `SZrCallInfo`、callable
  `SZrTypeValue`、`PreCallPrepared...` 和 `PostCall` 模板展开到 generated C · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-call-inline-struct-boundary-helper.md` ·
  RED/GREEN：value SemIR/source contracts 先要求 helper-only typed-call boundary 并禁止旧
  prepared-call 模板，补 runtime helper 与生成器改写后 GREEN · 测试结果：WSL focused 组通过
  source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤
  仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 13:52:19 +08:00 · M1.5 / 07-S5 inline struct return boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  typed inline-struct return 现在通过 `ZrLibrary_AotRuntime_ReturnInlineStruct()` 边界 helper
  设置 return source/skip-drop；typed-call 和 static direct-call 在 prepared pre-call 前物化 VALUE
  参数源槽；scalar stack-copy 快速路径加入 source written-before/parameter/static-source eligibility
  guard；direct stack-copy fallback 对 inline-struct 同 layout 源/目标走
  `ZrCore_Function_CopyFrameSlotInline()` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-inline-struct-return-boundary-helper.md` ·
  RED/GREEN：shared-library value typed-call smoke 先暴露误把 struct/object 临时槽当 i64 读取，
  随后暴露 inline-struct fallback 误走 object conversion；补 guard、argument materialization 与
  inline-frame copy fallback 后 GREEN · 测试结果：broader focused WSL 组通过 source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR
  contracts 4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成，08-12 未开始。
- 2026-06-21 12:20:30 +08:00 · M1.5 / 07-S5 return boundary module split
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `ZrLibrary_AotRuntime_ReturnI64()` 已从 `aot_runtime.c` 拆入
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c`；新增私有
  `aot_runtime_internal.h`，只暴露 runtime state opaque 类型、`aot_runtime_get_state_from_global()` 和
  `aot_runtime_fail()` 给边界模板复用，`aot_runtime.c` 不再包含 `ReturnI64` 实现体 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-return-boundary-module-split.md` · RED/GREEN：
  return contract 先因缺新内部头/return 模块失败，补模块拆分和窄接口后 GREEN · 测试结果：
  source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；
  CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成；
  此切片只收束 i64 return boundary 模块边界，typed→typed 直返、入参/inout/deopt 边界和
  value-frame fallback 仍待后续 07-S5+。
- 2026-06-21 12:08:43 +08:00 · M1.5 / 07-S5 i64 return boundary runtime helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `ZrLibrary_AotRuntime_ReturnI64(state, value)`，把 focused i64 native→VM 返回打包从
  generated C 函数体移到 runtime 边界模板；`zr_aot_direct_return_i64_local` 现在只调用
  `ReturnI64(state, zr_aot_s%u)` 后 `ZR_AOT_C_RETURN(1)`，不再内联 `SZrCallInfo`、caller
  `SZrTypeValue`、closure/ownership/stackTop 维护 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-return-boundary-helper.md` · RED/GREEN：
  return source contract 先要求 runtime helper，typed-scalar generated-product 禁止内联 return
  VM 维护并要求 `ReturnI64`，补 helper 与 emitter 后 GREEN · 测试结果：source contracts 19/0、
  return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成；typed→typed 直返、入参/inout/deopt
  边界和 value-frame fallback 仍待后续 07-S5+，08-12 未开始。
- 2026-06-21 11:59:37 +08:00 · M1.5 / 07-S4 descriptor-free pure scalar empty entry setup
  切片 · 状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  descriptor-free zero-byte pure scalar generated C 入口现在不再输出
  `zr_aot_generated_frame_setup` block 或 function-scope `zr_aot_call_info`；frame setup 在
  `!includeStackFrameSetup` 时 emit 前返回，direct-return i64 local 模板在返回打包边界内局部读取
  `state->callInfoList` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-empty-pure-scalar-entry-setup.md` · RED/GREEN：
  typed-scalar generated-product 先禁止 `/* zr_aot_generated_frame_setup */` 并失败，补 empty
  entry setup 与 return-boundary callInfo local 后 GREEN · 测试结果：source contracts 19/0、
  return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S4 仍部分完成；后续继续处理返回/boundary/value-frame
  fallback，08-12 未开始。
- 2026-06-21 11:38:18 +08:00 · M1.5 / 07-S4 descriptor-free pure scalar minimal prologue
  切片 · 状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  descriptor-free 且 zero-byte-frame 的 focused pure scalar generated C frame setup 现在只保留
  function-scope `zr_aot_call_info`，随后直接进入 scalar locals；不再发完整 module context
  resolution、stack frame setup locals、stack/GC check、null-fill loop 或 public/generated
  `ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted` marker。runtime 改用私有
  `aot_runtime_mark_record_executed()` 在 record-entry/full context/shim 边界保留 `executedVia`
  标记 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-pure-scalar-minimal-prologue.md` ·
  RED/GREEN：source contract 先要求 `includeStackFrameSetup` gating，generated-product 禁止
  full context/stack setup；第一版省略 context 后 typed-scalar 暴露 `executedVia` 回归，补临时
  generated marker 后 GREEN，最终契约继续禁止 public/generated marker 并迁移到 runtime 边界后
  GREEN · 测试结果：source contracts 19/0、return contracts 1/0、frame setup contracts
  1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S4 仍部分完成；return/boundary/value-frame fallback 仍待后续 07-S5+ 收敛，08-12 未开始。
- 2026-06-21 11:21:56 +08:00 · M1.5 / 07-S4 frame descriptor elision 切片 ·
  状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `backend_aot_c_frame_descriptor.*`，集中证明 generated C function body 是否可省略
  `ZrAotGeneratedFrame frame`；function body 只在证明失败时声明 frame descriptor，并把
  `includeFrameDescriptor` 传入 frame setup；frame setup 只在该标志为真时生成
  `frame.function`、`frame.callInfo`、`frame.slotBase`、`frame.generatedFrameSlotCount`。
  focused pure scalar 生成物现在无 `ZrAotGeneratedFrame frame` 且无任何 `frame.*` 引用；
  `.registerFrameBytes = 0u,` 与 `value SemIR lowering frameByteSize=0` 保持不变 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-descriptor-elision.md` · RED/GREEN：
  frame-setup source contract 先因缺 `includeFrameDescriptor` 失败，typed-scalar
  generated-product 同步新增 frame declaration/assignment 禁止断言；补保守 descriptor
  proof、conditional setup、scalar SemIR frame-free probe 与 stack-copy local-only predicate 后
  GREEN · 测试结果：source contracts 19/0、return contracts 1/0、frame setup contracts
  1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S4 仍部分完成；exports、cleanup、exceptions、inline value frames、未知 opcode 或
  frame-backed fallback 仍保留 descriptor path。08-12 未开始。
- 2026-06-21 10:56:36 +08:00 · M1.5 / 07-S4 frame byte-slot prologue elision 切片 ·
  状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  frame setup 新增 inline-struct-only `registerFrameBytes` helper；当 computed byte-frame size
  为 0 时不再生成 `zr_aot_frame_byte_size` / `zr_aot_frame_byte_slot_count` locals 或
  `zr_aot_frame_byte_size = (TZrSize)0u;`。focused pure scalar 生成物的 byte-slot prologue
  已收窄为零；value SemIR generated comment 同步改为 inline-struct-only `frameByteSize=0`；
  剩余 `frame.*` 字段仍为 `callInfo`、`function`、`generatedFrameSlotCount`、`slotBase` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-byte-slot-prologue-elision.md` ·
  RED/GREEN：frame-setup source contract 先暴露 helper drift 并要求 byte-slot prologue 条件发射，
  typed-scalar generated-product 同步新增旧 byte-frame local/zero assignment 禁止断言；
  source-contract 再要求 value SemIR summary 不读 raw `frameLayout->frameByteSize`；补
  inline-struct-only helper、条件发射与 summary 后 GREEN · 测试结果：source contracts 19/0、return
  contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S4 仍部分完成，08-12 未开始。
- 2026-06-21 10:26:35 +08:00 · M1.5 / 07-S3 skip-drop slot cleanup gating 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  function body 新增 `needsSkipDropSlot = needsFrameCleanup`，只在 inline-struct cleanup
  可能发射时生成 `zr_aot_skip_drop_slot`。focused pure scalar 生成物不再含
  `zr_aot_skip_drop_slot` 或 `zr_aot_frame_started`；剩余 `frame.*` 字段仍为 `callInfo`、
  `function`、`generatedFrameSlotCount`、`slotBase` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-skip-drop-slot-cleanup-gating.md` · RED/GREEN：
  frame-setup source contract 先因缺 skip-drop predicate/local gating 失败，typed-scalar
  generated-product 同步新增旧 skip-drop declaration 禁止断言；补条件发射后 GREEN · 测试结果：
  source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；
  CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S3 仍部分完成，
  08-12 未开始。
- 2026-06-21 10:17:48 +08:00 · M1.5 / 07-S3 empty frame cleanup guard elision 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  frame cleanup emitter 公开是否会发射 inline-struct/drop cleanup 的判定；function body 只在
  cleanup 可能发射时生成 `zr_aot_frame_started` declaration/assignment/exit guard。focused pure
  scalar 生成物不再含空 cleanup guard；剩余 `frame.*` 字段仍为 `callInfo`、`function`、
  `generatedFrameSlotCount`、`slotBase` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-empty-frame-cleanup-guard-elision.md` ·
  RED/GREEN：frame-setup source contract 先因缺 cleanup predicate/conditional cleanup emission
  失败，typed-scalar generated-product 先因旧 `zr_aot_frame_started` 字符串失败；补条件发射后
  GREEN · 测试结果：source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S3 仍部分完成，08-12 未开始。
- 2026-06-21 10:09:03 +08:00 · M1.5 / 07-S3 MethodInfo registerFrameBytes narrowing 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  MethodInfo `.registerFrameBytes` 改为只统计 inline-struct frame slots；focused pure scalar
  生成物从 `.registerFrameBytes = 6272u,` 收窄为 `.registerFrameBytes = 0u,`。
  当前 prologue/comment 仍保留旧 `frameByteSize=6272` 与 `frame.callInfo`、`frame.function`、
  `frame.generatedFrameSlotCount`、`frame.slotBase`，后续 07-S3/07-S4 再继续坍塌 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-method-info-register-frame-bytes-narrowing.md` ·
  RED/GREEN：frame-setup source contract 先因缺 MethodInfo byte-frame 计算 helper/inline-struct
  filter 失败，typed-scalar generated-product 先因旧 `6272u` descriptor 失败；补 helper 后
  GREEN · 测试结果：source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S3 仍部分完成，08-12 未开始。
- 2026-06-21 09:46:32 +08:00 · M1.5 / 07-S3 frame export-context gating 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  frame setup 新增 `includeExportContext`，function body 传入 `publishExports`；module/
  function-table/thunk 等 export-only frame 字段只在 root/export publication 可能发生时生成。
  focused typed scalar 生成物剩余 `frame.*` 字段收窄为 `callInfo`、`function`、
  `generatedFrameSlotCount`、`slotBase` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-export-context-gating.md` · RED/GREEN：
  frame-setup contract 先因缺 `includeExportContext` 失败，typed-scalar generated-product
  针对旧 export-context 字段加负断言；条件化 setup 后 GREEN · 测试结果：source contracts 19/0、
  return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配
  已注册的 `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S3 仍部分完成，08-12 未开始。
- 2026-06-21 09:38:15 +08:00 · M1.5 / 07-S3 frame recordHandle setup elision 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 不再发 `frame.recordHandle = zr_aot_context.recordHandle;`；
  focused typed scalar 生成物不再含 `frame.recordHandle` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-record-handle-setup-elision.md` · RED/GREEN：
  frame-setup contract 和 typed-scalar generated-product 先因旧 setup assignment/generated token
  失败，删除后 GREEN · 测试结果：source contracts 19/0、return contracts 1/0、frame setup
  contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 ·
  备注：07-S3 仍部分完成；剩余 `frame.*` 字段为 call/return/export/cleanup 等旧边界路径服务，
  MethodInfo 尚未替代全部 descriptor state。08-12 未开始。
- 2026-06-21 09:23:10 +08:00 · M1.5 / 07-S3 fail macro function-index local 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generated function prologue 现在发 `const TZrUInt32 zr_aot_function_index = Nu;`；
  `ZR_AOT_C_FAIL()` 不再读取 `frame.functionIndex` / `frame.currentInstructionIndex`，
  改为使用 function-local index 和固定 `UINT32_MAX` instruction index；frame setup 不再写
  `frame.functionIndex`。focused typed scalar 生成物不再含 frame-backed function/current/observation
  字段 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-fail-macro-function-index-local.md` · RED/GREEN：
  typed-scalar generated product 先因缺局部 `zr_aot_function_index` 失败；source contract 先因
  fail macro 仍读 frame 字段失败；frame-setup contract 先因仍写 `frame.functionIndex` 失败；
  补局部 index、改 fail macro、删 setup assignment 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `zr_vm_aot_c_return_contracts_test` 1/0；`zr_vm_aot_c_frame_setup_contracts_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：generated C grep 确认无
  `frame.functionIndex`/`frame.currentInstructionIndex`/observation fields；scoped
  `git diff --check` 退出 0，仅报告既有 CRLF/LF 提示 · 备注：07-S3 尚未完成；typed scalar
  prologue 仍保留其他胖 frame 字段，08-12 未开始。
- 2026-06-21 09:15:34 +08:00 · M1.5 / 07-S3 MethodInfo skeleton 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `zr_aot_abi.h` 新增 `SZrAotMethodInfo` ABI 类型；
  `backend_aot_c_emitter.c` 为每个 generated function 发射一个
  `static const SZrAotMethodInfo zr_aot_method_info_%u`。focused typed scalar 生成物现在含
  `zr_aot_method_info_0`，字段为 `.functionIndex = 0u`、`.registerFrameBytes = 6272u`、
  null metadata/gc-root/signature descriptors、`.observationPolicy = 0u`；该描述符目前不在
  hot path 读取 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-method-info-skeleton.md` · RED/GREEN：
  frame-setup source contract 先因缺 MethodInfo ABI/emitter 模板失败；typed-scalar generated
  product 先因缺 `zr_aot_method_info_0` 失败；补 ABI 类型和 emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1；契约 binaries 在本 build 中未注册为 CTest ·
  检查：generated C inspection 确认 MethodInfo 常量已发射且 setup-time observation 字段未回流；
  scoped `git diff --check` 退出 0，仅报告既有 CRLF/LF 提示（全工作树检查在当前大型 dirty
  worktree 中超时） · 备注：07-S3 尚未完成；MethodInfo 尚未替代 typed prologue 的胖 frame，
  `metadataFunction`/`gcRootMap`/`signature` 仍为空，纯标量 `registerFrameBytes` 仍是当前
  6272-byte frame requirement，07-S4 再收窄到 0。08-12 未开始。
- 2026-06-21 09:01:51 +08:00 · M1.5 / 07-S3 frame setup observation elision 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 不再为每个 generated function 默认注入逐指令观测字段初始化：
  删除 `frame.currentInstructionIndex = 0`、`frame.lastObservedInstructionIndex = UINT32_MAX`、
  `frame.lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE`、`frame.observationMask = ...`、
  `frame.publishAllInstructions = ...` 以及 `state->debugHookSignal` / `ZR_DEBUG_HOOK_MASK_LINE`
  的 line-hook override。07-S1 已移除 typed function body 的 `zr_aot_begin_instruction`
  调用后，这些 setup-time 观测字段在当前 typed scalar hot path 中只剩 prologue 税 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-setup-observation-elision.md` ·
  RED/GREEN：`zr_vm_aot_c_frame_setup_contracts_test` 先因
  `frame.currentInstructionIndex = 0;` 仍存在于 frame setup source 失败；typed-scalar
  generated-product 也先因生成 C 仍含该 setup 赋值失败；删除观测初始化后两者 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1（本 build 中 frame setup contract binary 未注册为 CTest）；
  生成 C 检查确认 focused typed scalar 不再出现 setup-time observation/default-debug 初始化 ·
  备注：07-S3 尚未完成；生成文件头部的 `ZR_AOT_C_FAIL` 宏仍保留
  `frame.currentInstructionIndex` 用于失败报告，且 typed scalar prologue 仍有 `frame.slotBase`、
  `state->stackTop`、`generatedFrameSlotCount` 和返回边界 `SZrTypeValue` marshaling。后续仍需
  MethodInfo 发射、fat-frame 字段移除、byte-frame/prologue 收窄和完整 boundary template。
  `backend_aot_c_frame_setup.c` 当前 86 行、`test_aot_c_frame_setup_contracts.c` 222 行；
  `test_aot_c_typed_scalar.c` 当前 1031 行，本切片只追加 6 个 focused generated-product
  negative assertions，后续最小测试拆分边界仍是 typed-scalar generated-C marker/forbidden-token
  断言 helper 或更窄 regression contract。
- 2026-06-21 08:52:58 +08:00 · M1.5 / 07-S2 i64 direct-return boundary local reuse 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_return_i64_local()` 现在复用当前 frame setup 已生成的
  function-scope `zr_aot_call_info`，不再在 direct-return block 内重新声明
  `SZrCallInfo *zr_aot_call_info = frame.callInfo`；该 optimized path 仍由
  `backend_aot_c_scalar_locals_can_direct_return_i64_local()` 保守门控，因此 constructor
  函数不会进入此路径，return block 内也不再发 `frame.function` null guard 或
  `frame.function->functionName` constructor-name 分支。focused typed scalar 生成物保留
  `zr_aot_caller_result_value->value.nativeObject.nativeInt64 = zr_aot_s23/s48` 的边界写回，
  但不再出现 return-local `frame.callInfo`、`frame.function` guard、constructor-name check
  或 `zr_aot_result_slot = frame.slotBase + N` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-direct-return-i64-boundary-local-reuse.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧的
  `/* zr_aot_direct_return_i64_local */\n        SZrCallInfo *zr_aot_call_info = frame.callInfo;`
  失败；补 direct-return emitter 后 GREEN，并新增 return-local 环境符号负断言 · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `zr_vm_aot_c_return_contracts_test` 1/0；CTest `aot_c_typed_scalar` 1/1（本 build 中
  `ctest -N -R 'return|aot_c_return'` 返回 0 个已注册 return-contract CTest）；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；
  prologue/frame setup、完整 boundary marshaling template、non-i64 direct-return forms、
  byte-frame narrowing 和其他未证明 frame writes 仍待收敛。`backend_aot_c_lowering_control.c`
  当前 964 行，仍是既有 control/return emitter 边界；`tests/parser/test_aot_c_typed_scalar.c`
  当前 1025 行，本切片只给现有 focused generated-product fixture 增加负断言，暂不混入测试重组。
  后续最小拆分边界为 07-S2 typed-scalar generated-C marker/forbidden-token 断言 helper 或
  更窄的 scalar-local regression contract。
- 2026-06-21 08:41:11 +08:00 · M1.5 / 07-S2 RESET_STACK_NULL2 stateful pair proof 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `RESET_STACK_NULL2` 现在通过
  `backend_aot_c_scalar_locals_reset2_can_skip_value_slots()` 做双槽状态化证明，
  以 live bitmask 分别追踪 first/second slot，从当前 block 后缀进入 successor graph；
  两个 reset target 可在不同指令被覆盖/kill，只要任一路径在 frame/source read 前结束即可跳过
  frame-backed reset2。`backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local()`
  先确认 opcode 是实际 scalar-local consumer，再查询读槽，避免把 `GET_CONSTANT` 目标槽误判为 read ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null2-stateful-pair-skip.md` ·
  RED/GREEN：typed-scalar generated-product 先因缺少
  `/* zr_aot_reset_stack_null2_scalar_local_skip slots=3,4 */` 失败；补状态化 pair proof
  后 GREEN，并新增 `slots=6,7` marker-only/negative 断言。生成 C 现在包含 `slots=3,4`、
  `slots=5,6`、`slots=6,7`、`slots=9,10`、`slots=15,16` reset2 skip marker，
  且不再出现 `zr_aot_value_exec_reset_stack_null2` · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；prologue/frame setup、边界 local restoration、
  generic float copy/type checks、完整 typed boundary marshaling 和其他未证明的 frame writes
  仍待收敛。`backend_aot_c_scalar_locals.c` 当前 2278 行，本切片继续扩展同一
  scalar-local liveness/reset proof 边界；暂不拆分以避免将大量 private predicate 过早公开，
  后续最小拆分边界为独立 scalar-local liveness/result/reset proof 模块。
- 2026-06-21 08:20:48 +08:00 · M1.5 / 07-S2 RESET_STACK_NULL2 scalar-local skip 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `RESET_STACK_NULL2` 现在在两个目标槽都通过
  `backend_aot_c_scalar_locals_reset_can_skip_value_slot()` 时不再发 frame-backed reset2
  block，而是发 `/* zr_aot_reset_stack_null2_scalar_local_skip slots=A,B */`。
  focused typed scalar 中分支两侧的 `slots=5,6` reset2 被证明为 local-only dead reset：
  slot 5/6 均会在后续路径被覆盖，且没有先发生 frame/source read，因此旧的
  `SZrTypeValue *zr_aot_first/second`、`frame.slotBase[5/6].value` 与双
  `ZrCore_Value_ResetAsNull` 被删除 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null2-scalar-local-skip.md` ·
  RED/GREEN：初始 RED 锁定 `slots=3,4`，该子切片当时的 single-slot 独立证明尚未携带
  paired live state，顺序覆盖时会保守失败；有效 RED 改为 `slots=5,6` 并先因缺少
  `/* zr_aot_reset_stack_null2_scalar_local_skip slots=5,6 */` 失败，补 reset2
  双槽 routing 和 marker-only emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 检查确认两处 `slots=5,6` skip marker，
  且无旧 focused `frame.slotBase[5/6]` reset2 block；`zr_vm_aot_c_source_contracts_test`
  19/0；CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；仍有其他 reset pairs、prologue/frame setup、
  边界 local restoration、generic float copy/type checks 和完整 typed boundary
  marshaling 待收敛。
- 2026-06-21 08:11:47 +08:00 · M1.5 / 07-S2 RESET_STACK_NULL scalar-local skip 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  single-slot `RESET_STACK_NULL` 现在先调用
  `backend_aot_c_scalar_locals_reset_can_skip_value_slot()`；当目标槽有 scalar C local
  覆盖、函数无 exception handlers、槽未导出，且从当前 block 后缀到 successor graph
  的所有可达路径都在任何 frame/source read 前覆盖或 kill 该槽时，生成器只发
  `/* zr_aot_reset_stack_null_scalar_local_skip slot=N */`。focused typed scalar
  中分支两侧的 `slot 21` reset 现在都被证明为 local-only dead reset，不再构造
  `SZrTypeValue *zr_aot_destination`，不再检查 `frame.slotBase`，也不再调用
  `ZrCore_Value_ResetAsNull(&frame.slotBase[21].value)` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null-scalar-local-skip.md` ·
  RED/GREEN：focused generated-product 先因缺少
  `/* zr_aot_reset_stack_null_scalar_local_skip slot=21 */` 失败；补 function-body
  routing、reset dead-slot successor 扫描和 marker-only emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 检查确认两处 `slot=21` skip marker，
  且无旧 `frame.slotBase[21].value` single-slot reset；`zr_vm_aot_c_source_contracts_test`
  19/0；CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；`RESET_STACK_NULL2`、其他 reset shapes、
  prologue/frame setup、边界 local restoration、generic float copy/type checks 和完整
  typed boundary marshaling 仍待收敛。
- 2026-06-21 07:51:00 +08:00 · M1.5 / 07-S2 i64 direct-return local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的两个 i64 return sites (`slot 23` 与 `slot 48`) 现在使用
  `zr_aot_direct_return_i64_local`，直接把 `zr_aot_s23` / `zr_aot_s48` 写入 caller result
  `SZrTypeValue`；`backend_aot_write_c_direct_return_i64_local()` 不再委托 frame-backed
  direct return，也不再生成 `zr_aot_result_slot = frame.slotBase + N` /
  `zr_aot_result_value` 读取路径。新增
  `backend_aot_c_scalar_locals_can_direct_return_i64_local()` 集中校验 i64 local direct-return
  的保守条件，并让 i64 result-skip proof 在可由 local direct return 消费的
  `FUNCTION_RETURN` 边界停止；focused generated C 中 return 前的 `slot 23/48`
  result materialization 被删除，返回块直接设置 `ZR_VALUE_TYPE_INT64` 与
  `nativeInt64 = zr_aot_sN` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-direct-return-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因缺少
  `/* zr_aot_direct_return_i64_local */` 失败；补 local-return emitter、return 边界
  result-skip 证明和 generated-product 断言后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 检查确认存在
  `nativeInt64 = zr_aot_s23` / `nativeInt64 = zr_aot_s48`，且不存在
  `zr_aot_result_slot = frame.slotBase + 23/48` 与 return 前 result frame materialization；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；
  reset-stack-null frame writes、prologue/frame setup、边界 local restoration、
  generic float copy/type checks 和完整 typed boundary marshaling 仍待收敛；本轮 WSL
  shared parser rebuild 还暴露并最小修复了既有 untracked `cfg_throw_profile.c`
  缺少 `cfg_node_throw_kind_mask()` 前置声明的 C11 编译阻塞。
- 2026-06-21 07:31:11 +08:00 · M1.5 / 07-S2 f64 primitive constant local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 f64 primitive constants (`slot 19 = 1.5`、`slot 20 = 2.0`)
  现在先发独立 `zr_aot_scalar_constant_f64_local` block，并在后续 f64 binary 可直接消费
  `zr_aot_f19/f20` 时跳过 `zr_aot_value_exec_primitive_constant` frame write；
  生成 C 中 focused f64 constants 只剩 `zr_aot_f19 = (TZrFloat64)1.5;` 与
  `zr_aot_f20 = (TZrFloat64)2;`，不再写回 `frame.slotBase[19/20].value` double payload。
  新增 `backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot()`，复用同一套可达
  consumer/result-skip 证明 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-constant-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因缺少 `zr_aot_scalar_constant_f64_local`
  standalone block 失败；补 f64 constant local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认 f64 constants 后直接进入
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 primitive constants、generic float copy/type checks、
  direct return/result frame fallback removal、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1800 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 07:24:00 +08:00 · M1.5 / 07-S2 i64 primitive constant cross-block local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的前两个 i64 primitive constants (`slot 0 = 21`、`slot 1 = 2`)
  现在只发 `zr_aot_scalar_constant_i64_local` + `zr_aot_s0 = (TZrInt64)21;` /
  `zr_aot_s1 = (TZrInt64)2;`，不再紧跟 `zr_aot_value_exec_primitive_constant`
  或写回 `frame.slotBase[0/1].value` integer payload。primitive-constant skip proof
  复用 result-skip 可达 consumer 扫描，只有后续可达路径均能读/kill scalar local 时才跳过
  frame result；`FUNCTION_RETURN` 被保守视为 frame-based direct-return 边界，返回槽仍保持
  materialized，避免直接返回读取未写 frame 的结果 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-constant-cross-block-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 `slot 0` constant-local 后仍跟
  primitive-constant frame write 失败；补 primitive constant local-only proof 并收紧 direct-return
  boundary 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认
  slots 0/1 只剩 scalar-local assignment，direct-return slots 23/48 仍在 return 前 materialize；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他
  primitive constants、generic float copy/type checks、direct return/result frame fallback removal、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1800 行，后续最小拆分边界为
  scalar result-skip/liveness proof 模块。
- 2026-06-21 06:54:52 +08:00 · M1.5 / 07-S2 numeric conversion result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 typed numeric conversion results (`TO_INT_FLOAT dstSlot=31 srcSlot=19`、
  `TO_INT_UNSIGNED dstSlot=31 srcSlot=8`、`TO_UINT_SIGNED dstSlot=31 srcSlot=2`、
  `TO_UINT_FLOAT dstSlot=31 srcSlot=19`) 现在只发 scalar-local conversion：
  `zr_aot_s31 = (TZrInt64)zr_aot_f19;`、带 `zr_aot_limit` guard 的 u64->i64 写入、
  `zr_aot_u31 = (TZrUInt64)zr_aot_s2;`、`zr_aot_u31 = (TZrUInt64)zr_aot_f19;`，
  不再构造 `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` / `zr_aot_u_result`
  或写回 `frame.slotBase[31].value` integer payload。i64/u64 conversion emitters 复用
  source written-before + matching result-skip proof，只有 source 已证明为 scalar local、目标槽有
  matching local 且后续可达 consumer 均可读/kill local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-numeric-conversion-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 `TO_INT_FLOAT` destination/result
  物化失败；补 numeric conversion result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认四个 focused conversion blocks 只剩
  direct local casts/guard；`zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar
  result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1800 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 06:47:40 +08:00 · M1.5 / 07-S2 f64 conversion result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 f64 conversion results (`TO_FLOAT_SIGNED dstSlot=31 srcSlot=2` 与
  `TO_FLOAT_UNSIGNED dstSlot=31 srcSlot=8`) 现在只发
  `zr_aot_f31 = (TZrFloat64)zr_aot_s2;` 与
  `zr_aot_f31 = (TZrFloat64)zr_aot_u8;`，不再构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_f_result` 或写回
  `frame.slotBase[31].value` double payload。f64 conversion emitter 复用 signed/u64
  source written-before + f64 result-skip proof，只有 source 已证明为 scalar local、目标槽有
  f64 local 且后续可达 consumer 均可读/kill f64 local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-conversion-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 f64 conversion destination/result
  物化失败；补 f64 conversion result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认两个 focused f64 conversion blocks 只剩
  `zr_aot_f31 = (TZrFloat64)zr_aot_s2;` 与 `zr_aot_f31 = (TZrFloat64)zr_aot_u8;`；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar
  conversion/result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1800 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 06:37:26 +08:00 · M1.5 / 07-S2 f64 binary result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 f64 binary result (`dstSlot=32 leftSlot=19 rightSlot=20`) 现在只发
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`，不再构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_f_result` 或写回
  `frame.slotBase[32].value` double payload。f64 binary emitter 复用 f64 source
  written-before + f64 result-skip proof，只有左右源已证明为 f64 locals、目标槽有
  f64 local 且后续可达 consumer 均可读 f64 local/kill 时才跳过 frame result；
  proof 新增识别 f64 binary、f64-to-int/uint conversion 与 f64 stack-copy consumers，
  未知 frame-dependent read 仍保守拒绝 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-binary-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 f64 binary destination/result
  物化失败；补 f64 result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认 focused f64 binary block 只剩
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 scalar result materialization、primitive constant frame writes、
  generic float copy/type checks、direct return/result frame fallbacks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1800 行，后续最小拆分边界为
  scalar result-skip/liveness proof 模块。
- 2026-06-21 06:23:48 +08:00 · M1.5 / 07-S2 i64 compare bool result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 signed i64 compare bool results (`dstSlot=27` 与 `dstSlot=7`,
  `leftSlot=2 rightSlot=4`) 现在只发
  `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);` 与
  `zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`，不再构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` 或写回 bool frame payload。
  i64 compare emitter 复用 signed source written-before + bool result-skip proof，只有左右源
  已证明为 signed locals、目标槽有 bool local 且后续可达 consumer 均可读 bool local/kill
  时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-compare-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 compare bool destination/result
  物化失败；补 result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认两个 focused compare block 只剩 bool local
  assignment；`zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar
  result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1100 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 06:06:05 +08:00 · M1.5 / 07-S2 i64 bit-not result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 signed i64 bit-not (`dstSlot=19 sourceSlot=1`) 现在只发
  `zr_aot_s19 = ~zr_aot_s1;`，不再为 `dstSlot=19` 构造 `SZrTypeValue *zr_aot_destination`、
  `zr_aot_s_result`、source frame type-check/reload 或写回 `frame.slotBase[19].value`。
  i64 bit-not emitter 复用 signed i64 written-before + result-skip proof，只有 source
  已证明为 written scalar local、目标槽有 signed local 且后续可达 consumer 均可读 signed local
  时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-bit-not-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 bit-not destination/result
  物化失败；补 i64 bit-not result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认只剩
  `zr_aot_s19 = ~zr_aot_s1;`；`zr_vm_aot_c_source_contracts_test` 目标验证 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 scalar result materialization、primitive constant frame writes、
  generic float copy/type checks、direct return/result frame fallbacks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c`
  已超过 1100 行，后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 05:57:20 +08:00 · M1.5 / 07-S2 i64 shift result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的首个 signed i64 shift (`dstSlot=19 leftSlot=15 rightSlot=1`) 现在保留
  shift-count range guard 后只发 `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);`，
  不再为 `dstSlot=19` 构造 `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result`
  或写回 `frame.slotBase[19].value`。i64 shift emitter 复用 signed i64 result-skip proof，
  只有左右源均已证明为 written scalar locals、目标槽有 signed local 且后续可达 consumer
  均可读 signed local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-shift-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 shift destination/result
  物化失败；补 i64 shift result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认保留 range guard 且只剩
  `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);`；`zr_vm_aot_c_source_contracts_test`
  目标验证 19/0；CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar result materialization、primitive
  constant frame writes、generic float copy/type checks、direct return/result frame fallbacks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1100 行，后续最小拆分边界为
  scalar result-skip/liveness proof 模块。
- 2026-06-21 05:52:10 +08:00 · M1.5 / 07-S2 i64 bitwise result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的首个 signed i64 bitwise (`dstSlot=16 leftSlot=12 rightSlot=0`) 现在只发
  `zr_aot_s16 = zr_aot_s12 & zr_aot_s0;`，不再为 `dstSlot=16` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` 或写回 `frame.slotBase[16].value`。
  i64 bitwise emitter 复用 signed i64 result-skip proof，只有左右源均已证明为 written scalar locals
  且后续可达 consumer 均可读 signed local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-bitwise-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 bitwise destination/result
  物化失败；补 i64 bitwise result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认只剩
  `zr_aot_s16 = zr_aot_s12 & zr_aot_s0;`；`zr_vm_aot_c_source_contracts_test` 目标重建并执行 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 scalar result materialization、primitive constant frame writes、
  generic float copy/type checks、direct return/result frame fallbacks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c`
  已超过 1100 行，后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 05:43:39 +08:00 · M1.5 / 07-S2 i64 binary result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的首个 signed i64 binary (`dstSlot=2 leftSlot=0 rightSlot=1`) 现在只发
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`，不再为 `dstSlot=2` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` 或写回 `frame.slotBase[2].value`。
  i64 result-skip proof 现在识别 signed local consumers、typed signed conversions、direct signed branches、
  scalar stack-copy consumers 和 unconditional `JUMP`，未知 slot mentions 仍保守拒绝 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-binary-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 binary destination/result 物化失败；
  补 i64 result-skip proof 和 i64 binary result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认只剩
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`；`zr_vm_aot_c_source_contracts_test` 目标重建并执行 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示。
  备注：07-S2 尚未完成；其他 scalar
  result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1100 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。
- 2026-06-21 05:18:36 +08:00 · M1.5 / 07-S2 u64 compare bool result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 `unsignedSum > unsignedRight` 与 `unsignedInverted <= unsignedRight`
  compare bool result blocks 现在只发 `zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);` 与
  `zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);`，不再为 `dstSlot=14/23` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_u_result` 或写回 `frame.slotBase[14/23].value`。
  bool result-skip proof 会沿 successor graph 证明后续可达读取都是 bool local consumers
  或 reset-stack-null kill，未知 frame-dependent reads 继续保守拒绝 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-u64-compare-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 u64 compare destination/result
  物化失败；补 bool result-skip proof 和 compare result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar result materialization、更多 primitive constant
  frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1100 行，后续最小拆分边界为 result-skip/liveness
  证明模块。
- 2026-06-21 04:55:24 +08:00 · M1.5 / 07-S2 u64 bit-not result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 `unsignedInverted` 结果块现在只发
  `zr_aot_u33 = ~zr_aot_u7;`，不再为 `dstSlot=33` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_u_result` 或写回 `frame.slotBase[33].value`。
  `backend_aot_write_c_scalar_u64_bit_not()` 复用 u64 result-skip proof，只有所有可达后续读取
  都是已支持的 local consumers 时才允许省掉 frame result ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-bit-not-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 u64 bit-not destination/result
  物化失败；补 bit-not result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 result materialization、更多 primitive constant
  frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛。
- 2026-06-21 04:48:06 +08:00 · M1.5 / 07-S2 u64 shift result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 `unsignedShifted` / `unsignedShiftedBack` 结果块现在只保留 range
  guard 和 `zr_aot_u13 = zr_aot_u8 << zr_aot_s1;` /
  `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;`，不再为 `dstSlot=13/14` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_u_result` 或写回 `frame.slotBase[13/14].value`。
  `backend_aot_write_c_scalar_u64_shift()` 复用 u64 result-skip proof，只有所有可达后续读取
  都是已支持的 local consumers 时才允许省掉 frame result ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-shift-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 u64 shift destination/result
  物化失败；补 shift result local-only path 并保留 range guard 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 result materialization、更多 primitive constant
  frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛。
- 2026-06-21 04:42:16 +08:00 · M1.5 / 07-S2 u64 bitwise result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 `unsignedMasked` 结果块现在只发
  `zr_aot_u12 = zr_aot_u8 & zr_aot_u7;`，不再为 `dstSlot=12` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_u_result` 或写回 `frame.slotBase[12].value`。
  `backend_aot_write_c_scalar_u64_bitwise()` 复用 u64 result-skip proof，只有所有可达后续读取
  都是已支持的 local consumers 时才允许省掉 frame result ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-bitwise-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 u64 bitwise destination/result
  物化失败；补 bitwise result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 result materialization、更多 primitive constant
  frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛。
- 2026-06-21 04:33:00 +08:00 · M1.5 / 07-S2 u64 binary result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 `unsignedSum` 结果块现在只发
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`，不再为 `dstSlot=8` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_u_result` 或写回 `frame.slotBase[8].value`。
  `backend_aot_c_scalar_locals` 增加 u64 result-skip proof，从当前 block 后缀沿 successor graph
  追踪 live value，只有所有可达后续读取都是已支持的 local consumers 时才允许省掉 frame result ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-binary-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 u64 binary destination/result
  物化失败；补 result local-only path 和可达消费者证明后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 result materialization、更多 primitive constant
  frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛。
- 2026-06-21 04:10:01 +08:00 · M1.5 / 07-S2 u64 constant/conversion local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 `uint` 初始化链路现在生成 `zr_aot_u6 = (TZrUInt64)zr_aot_s6;`、
  `zr_aot_u7 = (TZrUInt64)4;` 和 `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`，不再为 slot 6/7
  的 `TO_UINT` 与 u64 binary source 发 frame-backed source type-check/reload。signed
  immediate 写入在目标槽有 u64 scalar local 覆盖时也记录 u64 written proof，使后续
  local-only conversion/binary 可以证明 source 已写 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-u64-constant-conversion-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 依次失败在缺少 direct u6 cast、
  slot 7 `TO_UINT` source frame check、slot 6/7 u64 binary source frame check；补
  conversion fast path、signed-immediate u64 written proof、u64 binary written-source
  复用后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；
  result materialization、更多 primitive constant frame writes、direct return/result frame
  fallbacks、generic float copy/type checks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛。
- 2026-06-21 03:46:02 +08:00 · M1.5 / 07-S2 scalar stack-copy local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  bool/i64/u64 stack-copy 与 f64 一起在 source/destination 都有 scalar local 时只发
  C-local assignment；focused typed scalar 的 `zr_aot_b5 = (TZrBool)(zr_aot_b7 != 0u)`、
  `zr_aot_u9 = zr_aot_u12`、`zr_aot_u21 = zr_aot_u33`、`zr_aot_s13 = zr_aot_s16`
  和 `zr_aot_s18 = zr_aot_s19` 不再伴随 source/destination `SZrTypeValue` 指针、
  source tag-check 或 destination frame payload 写 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-scalar-stack-copy-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因缺少直接 bool local copy 失败，
  实现 bool/i64/u64 local-only fast path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1 · 备注：07-S2 尚未完成；
  result materialization、primitive constant frame writes、direct return/result frame fallbacks、
  generic float copy/type checks 和边界 local restoration 仍待收敛。
- 2026-06-21 03:40:27 +08:00 · M1.5 / 07-S2 f64 stack-copy local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  f64 stack-copy 在 source/destination 都有 scalar local 时只发 C-local assignment，
  focused typed scalar 的 `zr_aot_f40 = zr_aot_f19` 与 `zr_aot_f22 = zr_aot_f40`
  不再伴随 source/destination `SZrTypeValue` 指针、source float tag-check、
  destination ownership release 或 frame payload 写 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-stack-copy-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因缺少直接 f64 local copy 失败，
  实现 local-only fast path 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 ·
  备注：07-S2 尚未完成；非 f64 stack-copy frame writes、result materialization、
  return/result frame fallbacks 和边界 local restoration 仍待收敛。
- 2026-06-21 03:25:22 +08:00 · M1.5 / 07-S2 f64 source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_scalar_locals_f64_written_before()` 公开同一套 must-write 证明；
  f64 binary 与 float-source numeric conversion 在 source 已写入时跳过 source frame
  type-check/reload。focused typed scalar 中 `zr_aot_f32 = zr_aot_f19 * zr_aot_f20`、
  `zr_aot_s31 = (TZrInt64)zr_aot_f19` 和 `zr_aot_u31 = (TZrUInt64)zr_aot_f19`
  都直接使用 C local · 产出：`tests/acceptance/2026-06-21-aot-m1-5-f64-source-local.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `frame.slotBase[19/20]` f64 binary
  source type-check 失败，补 f64 written-before wrapper 和 binary/conversion emitter 后 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1 · 备注：此切片不标记 07-S2 完成；destination
  frame write、generic float copy/type checks、return/result materialization 和边界 local
  restoration 仍待收敛。
- 2026-06-21 03:05:55 +08:00 · M1.5 / 07-S2 unsigned shift source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  u64 shift lowering 对 unsigned left source 查询
  `backend_aot_c_scalar_locals_u64_written_before()`，对 signed shift count 查询
  `backend_aot_c_scalar_locals_i64_written_before()`；两者已写入时跳过 source frame
  type-check 和 frame reload，保留已有 `zr_aot_sN` shift-count range guard。focused
  typed scalar 中 `zr_aot_u13 = zr_aot_u8 << zr_aot_s1` 与
  `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1` 直接使用 C local · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsigned-shift-source-local.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `frame.slotBase[8/1]` unsigned shift
  source type-check 失败，补 u64 shift emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1；`git diff --check` 仅报告既有 LF/CRLF 提示 ·
  备注：此切片不标记 07-S2 完成；float source、destination frame write、return/result
  materialization 和边界 local restoration 仍待收敛。
- 2026-06-21 03:00:10 +08:00 · M1.5 / 07-S2 unsigned conversion source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `TO_FLOAT_UNSIGNED` 与 `TO_INT_UNSIGNED` 在 `backend_aot_c_scalar_locals_u64_written_before()`
  证明 source 已写入时，不再生成 `frame.slotBase[8]` source 赋值、unsigned source
  type-check 或 reload 到 `zr_aot_u8`。focused typed scalar 的 unsigned-to-float 和
  unsigned-to-signed wrap 计算直接使用 `zr_aot_u8` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsigned-conversion-source-local.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `frame.slotBase[8]` unsigned conversion
  source assignment/type-check 失败，补 conversion emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1；`git diff --check` 仅报告既有 LF/CRLF 提示 ·
  备注：此切片不标记 07-S2 完成；float-source conversion、unsigned shift、float binary
  source、destination frame write 和 result materialization 仍待收敛。
- 2026-06-21 02:51:35 +08:00 · M1.5 / 07-S2 unsigned source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  公开 `backend_aot_c_scalar_locals_u64_written_before()`；unsigned compare、u64 bitwise
  binary 和 u64 bit-not 在 source 已证明写入时跳过 source frame type-check 与 frame reload。
  focused typed scalar 中 `zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7)`、
  `zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7)` 和
  `zr_aot_u33 = ~zr_aot_u7` 都直接使用 C local · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsigned-source-local.md` · RED/GREEN：
  typed-scalar generated-product 先因旧 `frame.slotBase[21/7]` unsigned compare source
  type-check 失败；compare 改完后暴露 `frame.slotBase[8/7]` unsigned bitwise source fallback，
  补 u64 bitwise/bit-not 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`ctest -R 'aot_c_typed_scalar'` 1/1；
  `git diff --check` 仅报告既有 LF/CRLF 提示 · 备注：此切片不标记 07-S2 完成；
  unsigned shift、unsigned/float conversion source、float source、destination frame write 和
  result materialization 仍待收敛。
- 2026-06-21 02:35:27 +08:00 · M1.5 / 07-S2 signed conversion source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_try_write_c_scalar_conversion()` 传入当前 exec instruction index；
  `TO_FLOAT_SIGNED` 与 `TO_UINT_SIGNED` 在 signed source 已由
  `backend_aot_c_scalar_locals_i64_written_before()` 证明写入时，不再生成
  `frame.slotBase[2]` source 赋值、signed source type-check 或 reload 到 `zr_aot_s2`。
  focused typed scalar 保持 `zr_aot_f31 = (TZrFloat64)zr_aot_s2` 与
  `zr_aot_u31 = (TZrUInt64)zr_aot_s2` 的直接 cast · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-signed-conversion-source-local.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `frame.slotBase[2]` signed conversion
  source assignment/type-check 失败，补 conversion emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1；`git diff --check` 仅报告既有 LF/CRLF 提示 ·
  备注：此切片不标记 07-S2 完成；float/unsigned source conversion、conversion destination
  frame write、result `SZrTypeValue` materialization 仍待收敛。
- 2026-06-21 02:23:15 +08:00 · M1.5 / 07-S2 signed compare source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_scalar_i64_compare()` 采用 per-source
  `backend_aot_c_scalar_locals_i64_written_before()` 结果；已证明写入的 signed i64 compare
  源不再生成 `frame.slotBase[..].value.type` 校验，也不再从 frame reload 到 `zr_aot_sN`。
  focused typed scalar 中 `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4)` 直接复用
  `zr_aot_s2/zr_aot_s4` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-signed-compare-source-local.md` · RED/GREEN：
  typed-scalar generated-product 先因旧 `frame.slotBase[2/4]` signed compare source
  type-check 失败，补 compare emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1 · 备注：此切片不标记 07-S2 完成；compare
  destination 仍走当前 `SZrTypeValue` result write，unsigned/float/conversion/return 等路径仍待收敛。
- 2026-06-21 02:10:32 +08:00 · M1.5 / 07-S2 cross-block signed source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_scalar_locals_i64_written_before()` 从“仅当前 basic block 内写入”扩展为基于
  ExecIR successor 表的保守 must-write 数据流证明；只有所有可达前驱都已写入同类 C local 时，
  后续 block 才跳过 source frame type-check/reload。focused typed scalar 中跨 block 的
  `slot 2/3` signed binary 不再生成 `frame.slotBase[2/3].value.type` 校验和连续回读；
  constant signed branch 在 `zr_aot_s2` 已证明为 live i64 local 时直接发
  `if (zr_aot_s2 != zr_aot_right_literal) { ... }`，不再构造 `SZrTypeValue *zr_aot_left` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-cross-block-signed-source-local.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧跨块 `frame.slotBase[2/3]` source
  type-check 失败；补 dataflow 后又暴露 constant branch 仍构造 `SZrTypeValue` left source；
  删除该 fallback 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`ctest -R 'aot_c_typed_scalar'` 1/1 ·
  备注：此切片不标记 07-S2 完成；destination frame writes、`SZrTypeValue` result
  materialization、unsigned/float/conversion/return 等路径仍待寄存器单写收敛。
- 2026-06-21 01:50:07 +08:00 · M1.5 / 07-S2 signed branch source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_signed_branch()` 在 left/right 均已写入 i64 C local 时提前发
  纯 C local branch，不再构造 `SZrTypeValue *zr_aot_left/right`，也不再读
  `zr_aot_left->type`/`zr_aot_right->type`。focused typed scalar 生成物中的第一条 signed
  branch 现在只有 `if (zr_aot_s2 <= zr_aot_s4) { goto ...; }` · RED/GREEN：
  typed-scalar generated-product 先因旧 signed branch source tag-check 失败，补 control
  emitter 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`ctest -R 'aot_c_typed_scalar'` 1/1 ·
  备注：此切片不标记 07-S2 完成；constant branch、跨块 source-local 证明、primitive/result
  frame writes 与 `SZrTypeValue` 写仍待清理。
- 2026-06-21 01:40:20 +08:00 · M1.5 / 07-S2 signed shift source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_scalar_i64_shift()` 复用 signed bitwise 的 per-source written-local
  规则；已写入 C local 的 shift 左源不再发 source frame-slot type 校验，也不再从 frame reload。
  focused typed scalar 生成物中 `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1)`
  和 `zr_aot_s20 = zr_aot_s16 >> zr_aot_s1` 已直接复用 `zr_aot_s15`/`zr_aot_s16`；`slot 1`
  shift count 仍因当前证明限于基本块内而保留 frame fallback 与范围检查 · RED/GREEN：
  typed-scalar generated-product 先因旧 `frame.slotBase[15/1]` signed shift type-check 失败，
  补 shift emitter 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`ctest -R 'aot_c_typed_scalar'` 1/1 ·
  备注：此切片不标记 07-S2 完成；后续仍需跨块 source-local/dominance 证明、shift count/
  branch/conversion consumers、primitive/result frame writes 与 `SZrTypeValue` 写清理。
- 2026-06-21 01:31:55 +08:00 · M1.5 / 07-S2 signed bitwise source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_scalar_i64_bitwise()` 对 signed i64 bitwise 源槽逐个查询
  `backend_aot_c_scalar_locals_i64_written_before()`；已写入 C local 的源不再发 source
  frame-slot type 校验，也不再从 `frame.slotBase[..].value` 重载。focused typed scalar 生成物中
  `zr_aot_s16 = zr_aot_s12 & zr_aot_s0` 已直接复用 `zr_aot_s12`，`slot 0` 因当前
  written-before 证明仍是块内保守分析而继续走 frame fallback · RED/GREEN：typed-scalar
  generated-product 先因旧的 `frame.slotBase[12/0]` signed bitwise type-check 失败，补
  scalar bitwise emitter 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`ctest -R 'aot_c_typed_scalar'` 1/1 ·
  备注：此切片不标记 07-S2 完成；后续仍需跨块 source-local/dominance 证明、shift/branch/
  conversion consumers、primitive/result frame writes 与 `SZrTypeValue` 写清理。
- 2026-06-21 00:38:52 +08:00 · M1.5 / 07-S2 signed primitive constant scalar-local
  first-read 切片 · 状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成 · 完成项目：
  focused typed scalar 生成物现在要求 `left=21` / `right=2` 先出现
  `zr_aot_scalar_constant_i64_local` C 局部赋值块；`backend_aot_write_c_direct_primitive_constant()`
  对有 `i64` scalar-local 覆盖的 signed primitive constant 先发寄存器赋值，只有 skip predicate
  确认 value slot 可省略时才提前返回，否则保留现有 frame 写以维持后续未迁移路径可执行；
  `backend_aot_write_c_scalar_i64_binary()` 在 left/right 均为已写入的 i64 C local 时不再发
  `frame.slotBase[0/1].value.type` 源操作数类型校验，第一条乘法直接使用
  `zr_aot_s0 * zr_aot_s1` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-signed-constant-first-read.md` · RED/GREEN：
  先让 `zr_vm_aot_c_typed_scalar_test` 因缺少
  `/* zr_aot_scalar_constant_i64_local */\n        zr_aot_s0 = (TZrInt64)21;` 失败；补发
  signed constant scalar-local block 与 i64 binary written-local source fast path 后到 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1 · 备注：此切片不标记 07-S2 完成；后续仍需删除 typed
  函数体中的 primitive constant frame write、结果 frame 写、后续 bitwise/shift/branch/conversion 的
  source frame type 校验与 `SZrTypeValue` 写。
- 2026-06-21 00:02:31 +08:00 · M1.5 / 07-S1 typed 函数体 begin-instruction 环境隔离切片 ·
  状态：07-S1 完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_function_body.c` 不再在逐指令 typed C 函数体内调用
  `backend_aot_write_c_begin_instruction()`；focused typed scalar 生成物新增
  `zr_aot_begin_instruction` 禁止断言；aggregate AOT source-contract 将 function-body
  源码中的 `backend_aot_write_c_begin_instruction(` 设为禁止形态。`backend_aot_c_lowering_control.c`
  内 helper 本体暂保留，作为历史 direct-frame/control 边界代码，后续 07-S3/07-S7 再处理
  MethodInfo/观测策略与反退化门槛 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-environment-isolation.md` · RED/GREEN：
  先让 `zr_vm_aot_c_source_contracts_test` 因 function body 仍含
  `backend_aot_write_c_begin_instruction(` 失败；移除发射调用后 source-contract 与 generated-product
  均为 GREEN · 测试结果：手动 source-contract RED 19 tests / 1 failure；GREEN 后
  `zr_vm_aot_c_source_contracts_test` 19/0；隔离 WSL GCC/Ninja build 中
  `zr_vm_aot_c_typed_scalar_test` 1/0；同一 build 直接运行 `zr_vm_aot_c_source_contracts_test`
  19/0；`ctest -R 'aot_c_(source_contracts|typed_scalar)'` 仅匹配已注册的 `aot_c_typed_scalar`
  并通过 1/1 · 备注：source-contract binary 当前未注册为 CTest；构建日志仍有既有 unrelated warning；
  07-S2 的 `CONST`/标量单寄存器写与零 `SZrValue` 写尚未完成。
- 2026-06-20 21:44:51 +08:00 · M2 / 04-S3 bool branch local-written-before
  no-refresh hardening 切片 · 状态：子切片完成、04-S3 部分完成、M2 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals.c` 将同块 written-before 检查抽成按 scalar kind 复用的私有 helper，
  新增 `backend_aot_c_scalar_locals_bool_written_before()`；`JUMP_IF_BOOL_FALSE` 的 proven-local
  直接分支现在接收 `execInstructionIndex`，并且只在 condition slot 的 `bN` local 已于当前 basic block
  当前指令之前写入时，才发出 `zr_aot_jump_if_bool_false_scalar_local` / `if (!zr_aot_bN)`。
  旧的“只要 slot 有 bool local 声明就直接分支”资格判断被移除，避免和 signed branch 相同的 stale-local
  风险 · RED/GREEN：source-contract 先要求
  `backend_aot_c_scalar_locals_bool_written_before(functionIr, conditionSlot, execInstructionIndex)` 并禁止
  `useScalarCondition = backend_aot_c_scalar_locals_has_bool_slot(functionIr, conditionSlot);`，RED 于缺少
  bool written-before helper；实现共享 kind written-before、bool branch 签名透传后 GREEN · 测试结果：
  AOT C source contracts 19/0，exit=0；`backend_aot_c_scalar_locals.c`、
  `backend_aot_c_lowering_control.c`、`backend_aot_c_function_body.c` focused 对象编译通过并同步到
  临时 patched parser archive；AOT C typed scalar focused binary 1/0，exit=0 · 备注：
  focused `branchFlag` 与 unsigned compare→bool false 分支仍保持 no-refresh 生成物形态；
  本切片不声明跨 basic-block/join bool local 可用性，也不完成 resume/deopt local-state 重建、
  GC 根登记、系统性 C-local/frame-slot 权威存储、broader branch 与 typed/dynamic bridge。
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
  先加入 source-contract 对 `backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex)` / `rightSlot` 的要求后 RED，报缺少 written-before helper；实现同块写入证明、
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
- 2026-06-20 · 新增 [`07-codegen-register-model-and-environment-isolation.md`](07-codegen-register-model-and-environment-isolation.md)

  + 引入不变量 D（环境隔离）· 规划阶段，无代码改动 · 触发：用户反馈 `var left:int=21;` 生成 23 行
    （`zr_aot_begin_instruction` 注入 callInfo/slotBase/stackTop/programCounter/debugHook 全套解释器观测，
    且对 21 做 register/SZrValue 双写）。07 把这些逐条列为删除/改写目标，确立 il2cpp 风格寄存器文件模型
    （`zr_aot_sN` = 裸 C 局部，性能交给 C 编译器 `-O2`）、`SZrAotMethodInfo` 取代胖 `frame`、
    SZrValue 仅在 4 类 VM↔native 边界（入参/返回/inout 写回/deopt）构建、GC 对象直用 object gc 接口 +
    帧根表。落地切片 07-S1..S7 立为**独立先行里程碑 M1.5（环境隔离与去双写）**，排在 M1 之后、M2 之前
    （M2 硬前置，见 06），新增性能门槛（typed 标量基准 AOT ≥ 解释器，目标 ≥3×）与
    反退化 CI（环境零注入 / 零双写 grep + SZrValue 运行期计数=0）。
- 2026-06-20 · 新增 08–12（泛型共享 / 内存管理 / 反射 / 元数据 / 代码裁剪）· 规划阶段，无代码改动 ·
  触发：用户要求参照 `lua/hybridclr`(含 libil2cpp) / `lua/mono` / `lua/roslyn` / `lua/runtime`(illink + NativeAOT)
  完善这五块。经 4 路并行探查（三参考实现机制 + zr_vm 现状缺口）后定稿。两项架构决策（用户确认）：
  (1) 泛型共享 = **il2cpp 式**（值类型/const 单态化 + 引用类型共享 + 泛型字典 RGCTX 等价 + 未收集实例 deopt 解释器兜底，hybrid）；
  (2) 反射/元数据/裁剪 = **默认最小 + 注解保留**（对标 NativeAOT/illink mark-and-sweep）。
  关键设计：09 指出 zr_vm 含 EVACUATE/COMPACT 即移动式 GC → AOT 裸指针寄存器需精确可更新栈根 + safepoint
  （排除 il2cpp Boehm 保守扫法）；10 的 invoker 复用 07§6 边界 marshaling；11 采 zrp 两段式（数据元数据 + 代码注册表）
  并立 token↔cTypeId↔ZrLayout 三向表落地不变量 C；12 的可达性分析是 08/10/11 的共同上游。
  里程碑：06 新增 M6（泛型共享）/ M7（内存硬化 + 反射 + 元数据 + 裁剪），依赖图延伸至 M7，测试矩阵补 5 维。
