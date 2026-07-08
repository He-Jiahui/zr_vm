---
doc_type: plan-index
plan_sources:
  - user: 2026-06-20 参照 C# struct 栈上连续布局 / il2cpp 风格 typed lowering / AOT 必须降级为纯 C 元素操作（= / memcpy / +-*/）而非调用 VM 行为 / metadata 驱动消除指令不确定性 / 解释器与纯 C 双执行路径
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/bridge.h
  - zr_vm_core/src/zr_vm_core/bridge.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/object/object_internal.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_generic_params.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.h
  - zr_vm_core/src/zr_vm_core/reflection_token_resolve.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c
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
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_iterators.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_access_boundaries.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bool_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_numeric_invokers.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_numeric_invokers.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_numeric_three_arg_invokers.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_numeric_three_arg_invokers.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_thunks.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_publication.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_publication.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_super_array.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_exports.h
  - zr_vm_library/src/zr_vm_library/project/project_exports.c
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.h
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.h
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.h
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c
  - zr_vm_language_server_extension/schemas/zrp.schema.json
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/parser/test_aot_c_constant_contracts.c
  - tests/parser/test_aot_c_control_contracts.c
  - tests/parser/test_aot_c_control_shared_library_smoke.c
  - tests/parser/test_aot_c_guardrail_contracts.c
  - tests/parser/test_aot_c_global_contracts.c
  - tests/parser/test_aot_c_generic_call_typed.c
  - tests/parser/test_aot_c_code_stripping.c
  - tests/parser/test_aot_c_type_layout_contracts.c
  - tests/parser/test_aot_c_zrp_metadata_pruning.c
  - tests/parser/test_aot_c_zrp_metadata_publication.c
  - tests/parser/test_aot_c_zrp_metadata_export_token_remap.c
  - tests/parser/test_aot_c_zrp_metadata_size_deltas.c
  - tests/module/test_metadata_runtime_binding_compatibility.c
  - tests/module/test_aot_runtime_typed_direct_call_compatibility.c
  - tests/module/test_reflection_token_resolve.c
  - tests/module/test_metadata_runtime_query.c
  - tests/module/test_metadata_runtime_method_binding.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_aot_writer_options.c
  - tests/cli/test_cli_zrp_metadata_dump.c
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
  - tests/parser/test_aot_c_shared_library_smoke.c
  - tests/parser/test_aot_c_descriptor_diagnostics.c
  - tests/library/test_project_manifest_normalization.c
  - tests/parser/test_aot_c_call_contracts.c
  - tests/parser/test_aot_c_value_semir_contracts.c
  - tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c
  - tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c
  - tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c
  - tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c
  - tests/parser/test_aot_c_global_shared_library_smoke.c
  - tests/parser/test_aot_c_super_array_contracts.c
  - tests/parser/test_aot_c_super_array_shared_library_smoke.c
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
| 08   | [`08-generic-sharing.md`](08-generic-sharing.md)                                                                   | 泛型共享：值类型单态化 + 引用类型共享 + 泛型字典(RGCTX 等价) + 实例化收集 + hybrid 动态实例化兜底                              | 🚧 进行中 |
| 09   | [`09-memory-management.md`](09-memory-management.md)                                                               | 内存管理(AOT 视角)：GC 引用 descriptor、精确栈根 + safepoint + 移动 GC 根更新、写屏障与编译期消除、值类型分配/装箱边界化       | ✅ 完成 |
| 10   | [`10-reflection.md`](10-reflection.md)                                                                             | 反射：三级元数据(None/RuntimeMapping/Description)、invoker thunk(复用 07 边界)、token/泛型/offset 反射、保留注解驱动裁剪       | 🚧 进行中 |
| 11   | [`11-metadata.md`](11-metadata.md)                                                                                 | 元数据：zrp 两段式(数据元数据 + 代码注册表)、运行期 token lazy 解析、token↔cTypeId↔ZrLayout 三向表、版本校验、元数据策略     | 🚧 进行中 |
| 12   | [`12-code-stripping.md`](12-code-stripping.md)                                                                     | 代码裁剪：mark-and-sweep 可达性、裁剪驱动函数/类型/元数据生成、注解 + manifest 保留、泛型实例可达性、trim 警告、符号剥离       | 🚧 进行中 |

## 5. 与现有模块的关系

- 本计划是 [`docs/core-runtime/inline-type-layout-and-byte-stack.md`](../../core-runtime/inline-type-layout-and-byte-stack.md)
  的**上层规划**：那篇文档实现了 typed layout + byte-offset 栈原语；本计划规划如何让
  **指令集与代码生成全面切换到这套原语之上**，并最终取代旧固定槽 ABI。
- 与 [`docs/plans/using/`](../using/index.md) 的所有权/metadata token 工作是**互补**关系：
  using 计划提供了所有权种类与 metadata token 模型，本计划负责把它们**降级到生成的 C 代码**里
  （见 `05`），并保证 typed 路径下所有权检查能被编译期证明消除。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 里程碑/切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-07-06 07:35:26 +08:00 · 07-S6 GC reference register stage acceptance ·
  状态：07-S6 计划验收完成（覆盖当前生成的 conversion reference-local 路径）；07-S7 与 08~12
  继续进行中。完成项目：`SZrRawObject *` reference-local 结构体字段、独立 `LOCAL_ADDRESS`
  root map/root frame、写入后 safepoint GC debt 窗口和 truthiness 前 raw-object 消费路径已串通；
  ordinary VM-stack roots 与 local-address roots 保持两个 frame/base。GREEN：WSL GCC generated-C
  smoke/contracts 为 8/0、9/0、24/0、4/0、1/0；WSL Clang 同组 8/0、9/0、24/0、4/0、1/0；
  MSVC Debug build 通过，Unix-only smoke expected ignored 且 0 failures，contracts 24/0、4/0、1/0。
  AOT GC root-frame runtime matrix 在 WSL GCC、WSL Clang、MSVC Debug 均为 6/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-gc-reference-register-stage-acceptance.md`。
  备注：更长 soak/stress 与更多 object-allocation 形态作为后续强化项；下一阶段进入 07-S7 反退化 CI、
  SZrValue 计数和性能门槛。

- 2026-07-06 07:29:17 +08:00 · 07-S6 reference-local safepoint GC pressure ·
  状态：conversion reference-local 写入后 safepoint/GC 压力子切片完成；07~12 总目标继续进行中。
  完成项目：`TO_STRING` / `TO_OBJECT` reference-local 写回 `zr_aot_ref_locals.oN` 后立即发射
  `zr_aot_gc_safepoint_reference_local` marker 与 `ZrCore_Gc_SafePoint(state)`；logical-not/jump-if
  dynamic string/object smoke 断言 marker 顺序，并用 pending generational GC debt 要求 `gcLastStepWork > 0`。
  RED：WSL GCC logical-not 8/2、source contracts 24/1。GREEN：WSL GCC focused direct run 通过
  8/0、9/0、24/0、4/0、1/0；WSL Clang 同组 build/direct run 通过 8/0、9/0、24/0、4/0、1/0；
  MSVC Debug 同组 build 通过，两个 Unix-only smoke expected ignored 且 0 failures，contracts
  24/0、4/0、1/0。生成 C 检查确认 safepoint 位于 `zr_aot_ref_locals.o2` 写入之后。
  产出：`tests/acceptance/2026-07-06-aot-07-s6-reference-local-safepoint-gc-pressure.md`。
  备注：更长 GC stress、exports/frame cleanup、in/out writeback、性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 06:54:43 +08:00 · 07-S6 reference-local LOCAL_ADDRESS root frame ·
  状态：conversion reference-local 的 local-address root-map/root-frame 子切片完成；07~12 总目标继续进行中。
  完成项目：生成 `zr_aot_ref_root_slots_%u` / `zr_aot_ref_root_map_%u`，每个 reference-local root 使用
  `offsetof(SZrAotReferenceLocals_<flatIndex>, oN)` 和 `ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS`；
  生成函数在 `zr_aot_ref_locals` 后推送独立 `zr_aot_ref_gc_root_frame`，base 为
  `(TZrStackValuePointer)(void *)&zr_aot_ref_locals`，并在 ordinary VM-stack root cleanup 前弹出。
  RED：WSL GCC logical-not 8/2、jump-if 9/2、source contracts 24/1。GREEN：WSL GCC focused build 通过并运行
  8/0、9/0、24/0、4/0、1/0；WSL Clang 同组 build 通过并运行 8/0、9/0、24/0、4/0、1/0；
  MSVC Debug 同组 build 通过，两个 Unix-only smoke expected ignored 且 0 failures，contracts 4/0、24/0、1/0。
  产出：`tests/acceptance/2026-07-06-aot-07-s6-reference-local-address-root-frame.md`。
  备注：`methodInfo.gcRootMap` 仍只绑定 VM stack byte roots；`zr_aot_ref_root_map_%u` 只供独立
  local-address frame 使用。GC 压力/root 正确性、exports/frame cleanup、in/out writeback、性能计数和完整
  07-S6/07~12 仍待后续。

- 2026-07-06 06:19:50 +08:00 · 07-S6 reference-local struct frame preparation ·
  状态：conversion reference-local 结构体帧准备完成；07~12 总目标继续进行中。完成项目：
  为含 `TO_STRING` / `TO_OBJECT` destination 的生成函数发射
  `SZrAotReferenceLocals_<flatIndex>` typedef 和函数体内 `zr_aot_ref_locals` 实例；
  conversion boundary 返回后的 raw-object 写回与 immediate string/object truthiness 统一改用
  `zr_aot_ref_locals.oN`，不再使用裸 `zr_aot_oN` 变量。顶层 emitter 在 function forward decls 后发射这些
  结构体，为后续 `offsetof(..., oN)` local-address root map 做准备。RED：WSL GCC logical-not 8/2、
  jump-if 9/2、logical contracts 4/1、source contracts 24/1。GREEN：WSL GCC focused build 通过并运行
  8/0、9/0、4/0、24/0；WSL Clang 同组 build 通过并运行 8/0、9/0、4/0、24/0；MSVC Debug 同组 build 通过，
  两个 Unix-only smoke expected ignored 且 0 failures，logical/source contracts 4/0 与 24/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-reference-local-struct-frame-prep.md`。
  备注：`LOCAL_ADDRESS` root map 生成、单独 local-address root frame/ABI 扩展、GC 压力/root 正确性、
  exports/frame cleanup、in/out writeback、性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 05:54:47 +08:00 · 07-S6 conversion reference-local reuse ·
  状态：`TO_STRING` / `TO_OBJECT` conversion result 的 `SZrRawObject *zr_aot_oN` 局部声明与 immediate
  truthiness 复用完成；07~12 总目标继续进行中。完成项目：新增 `backend_aot_c_reference_locals.c/.h`，
  在生成函数 prologue 后声明每个 conversion destination 的 `zr_aot_oN`；`TO_STRING` / `TO_OBJECT`
  runtime boundary 返回后校验 destination slot 并填充该 raw-object local；string/object `LOGICAL_NOT`
  与 `JUMP_IF` fast path 直接用 `zr_aot_oN`，不再于 truthiness 阶段回读 `frame.slotBase + N`。RED：
  WSL GCC logical-not smoke 8/2、jump-if smoke 9/2。GREEN：WSL GCC logical-not 8/0、jump-if 9/0、
  logical contracts 4/0、source contracts 24/0；WSL Clang logical-not 8/0、jump-if 9/0；MSVC Debug 构建通过，
  两个 Unix-only smoke 运行体 expected ignored 且 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-conversion-reference-local-reuse.md`。
  备注：root-mapped 持久引用寄存器、LOCAL_ADDRESS root map 发射、GC 压力/root 正确性更广覆盖、exports/frame cleanup、
  in/out writeback、性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 05:24:51 +08:00 · 07-S6 string-slot truthiness raw-object local ·
  状态：`TO_STRING` 后 immediate string truthiness 本地化完成；07~12 总目标继续进行中。完成项目：
  generated C 的 `zr_aot_generic_logical_not_string_slot_local` 与 `zr_aot_generic_jump_if_string_slot_local`
  现在从 `SZrTypeValue` 边界 slot 抽出 `SZrRawObject *zr_aot_string_slot_object`，再把该 raw object 传给
  `ZR_CAST_STRING()` 和字符串长度判断，不再直接 cast `zr_aot_string_slot_value->value.object`。RED：WSL GCC
  logical-not smoke 8/1。GREEN：WSL GCC 8/0 与 9/0；WSL Clang 8/0 与 9/0；MSVC Debug 构建通过，两个
  Unix-only smoke 运行体 expected ignored 且 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-string-slot-truthiness-raw-object-local.md`。
  备注：持久引用寄存器声明、LOCAL_ADDRESS root map 发射、GC 压力/root 正确性更广覆盖、exports/frame cleanup、
  in/out writeback、性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 05:15:29 +08:00 · 07-S6 object-slot truthiness raw-object local ·
  状态：`TO_OBJECT` 后 immediate object truthiness 本地化完成；07~12 总目标继续进行中。完成项目：
  generated C 的 `zr_aot_generic_logical_not_object_slot_local` 与 `zr_aot_generic_jump_if_object_slot_local`
  现在从 `SZrTypeValue` 边界 slot 抽出 `SZrRawObject *zr_aot_object_slot_object`，再以 raw-object 指针非空
  计算 truthiness，不再发旧的 `zr_aot_object_slot_truthy = ZR_TRUE` tag 分支。RED：WSL GCC logical-not smoke
  8/1、jump-if smoke 9/1。GREEN：WSL GCC 8/0 与 9/0；WSL Clang 8/0 与 9/0；MSVC Debug 构建通过，两个
  Unix-only smoke 运行体 expected ignored 且 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-object-slot-truthiness-raw-object-local.md`。
  备注：持久引用寄存器声明、LOCAL_ADDRESS root map 发射、GC 压力/root 正确性更广覆盖、exports/frame cleanup、
  in/out writeback、性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 04:56:47 +08:00 · 07-S6 / 09-S2 LOCAL_ADDRESS root runtime support ·
  状态：local-address AOT root 的 GC 运行期支持完成；07~12 总目标继续进行中。完成项目：AOT root-frame
  mark/rewrite visitor 现在除 `FRAME_BYTE_OFFSET` 栈值根外，还支持
  `ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS`，把 registered frame base/offset 解析为 `SZrRawObject **`
  C local root slot，并在 minor GC 中标记和重写 forwarded raw-object pointer。RED：WSL GCC
  `zr_vm_aot_gc_root_frame_test` 6 tests / 1 failure。GREEN：WSL GCC 6/0、WSL Clang 6/0、MSVC Debug 6/0。
  产出：`tests/acceptance/2026-07-06-aot-09-s2-local-address-root-runtime-support.md`。
  备注：生成器的 LOCAL_ADDRESS root map 发射、GC 压力/root 正确性更广覆盖、exports/frame cleanup、in/out writeback、
  性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 04:45:41 +08:00 · 07-S6 GC-root-only frame setup retention ·
  状态：GC root frame setup 保留门禁完成；07~12 总目标继续进行中。完成项目：
  `backend_aot_write_c_frame_setup()` 的 `includeStackFrameSetup` 现在由
  `includeFrameDescriptor || frameByteSize > 0u || includeGcRootFrame` 决定，避免后续 register/local-address root
  只有 root map、没有 byte-frame/descriptor 时被 zero-byte 入口提前返回。RED：WSL GCC frame setup contracts 1/1。
  GREEN：WSL GCC frame setup contracts 1/0、value-type smoke 5/0；WSL Clang frame setup contracts 1/0、value-type
  smoke 5/0；MSVC Debug frame setup contracts 1/0、value-type smoke 5 tests / 0 failures / 1 expected Unix-only ignore。
  产出：`tests/acceptance/2026-07-06-aot-07-s6-gc-root-only-frame-setup-retention.md`。
  备注：LOCAL_ADDRESS root map 发射、GC 压力/root 正确性、exports/frame cleanup 更广覆盖、in/out writeback、性能计数和
  完整 07-S6/07~12 仍待后续。

- 2026-07-06 04:34:25 +08:00 · 07-S6 POD no-drop frame cleanup elision ·
  状态：静态 no-drop POD inline-struct frame cleanup 省略完成；07~12 总目标继续进行中。完成项目：
  frame cleanup 生成器现在通过 `backend_aot_c_frame_cleanup_would_emit_for_function(state, functionIr)` 和
  `backend_aot_write_c_frame_cleanup(file, state, functionIr)` 解析函数原型 frame type layout，并跳过 resolved
  `dropKind == NONE` 的 inline-struct frame slot；无法解析 layout 时仍保守保留 cleanup。POD value-type generated C
  不再发出 frame-start/skip-drop 变量、`zr_aot_value_frame_drop` 或 `ZrCore_TypeLayout_DropInline`，ref/drop layout
  仍保留 cleanup。RED：WSL GCC source contracts 24/1、frame setup contracts 1/1、value-type smoke 5/1。GREEN：
  WSL GCC/Clang source contracts 均 24/0、frame setup contracts 均 1/0、value-type smoke 均 5/0；MSVC Debug source
  contracts 24/0、frame setup contracts 1/0、value-type smoke 5 tests / 0 failures / 1 expected Unix-only ignore。
  生成物检查确认 `pod_struct.c` 没有 frame cleanup helper/drop 调用，`ref_struct.c` cleanup 保留。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-pod-no-drop-frame-cleanup-elision.md`。
  备注：GC 压力/root 正确性、exports/frame cleanup 更广覆盖、in/out writeback、性能计数和完整 07-S6/07~12
  仍待后续。

- 2026-07-06 04:21:03 +08:00 · 07-S6 GC root frame static map push ·
  状态：GC root frame push 静态 root map 绑定完成；07~12 总目标继续进行中。完成项目：
  `backend_aot_write_c_frame_setup()` 在已证明 `needsGcRootFrame` 的生成函数里，直接把
  `&zr_aot_gc_root_map_%u` 传给 `ZrCore_Gc_AotRootFramePush(...)`，并发出
  `zr_aot_gc_root_frame_static_map_push` marker；不再生成 `zr_aot_context.methodInfo->gcRootMap`
  运行期分支。ref struct generated-C smoke 要求静态 root map push 且禁止 methodInfo lookup，POD struct
  继续要求无 root map / push / pop。RED：WSL GCC frame setup contracts 1/1，value-type smoke 5/1。
  GREEN：WSL GCC/Clang frame setup contracts 均 1/0，value-type smoke 均 5/0；MSVC Debug frame setup contracts
  1/0，value-type smoke 5 tests / 0 failures / 1 expected Unix-only ignore。生成物检查确认 ref struct 直接引用
  `&zr_aot_gc_root_map_0` / `&zr_aot_gc_root_map_1`，POD 无 root frame 代码。产出：
  `tests/acceptance/2026-07-06-aot-07-s6-gc-root-frame-static-map-push.md`。
  备注：exports/frame cleanup、in/out writeback、性能计数和完整 07-S6/07~12 仍待后续。

- 2026-07-06 04:11:00 +08:00 · 07-S5 full-AOT i64/f64 scalar typed direct-call matrix guardrail ·
  状态：i64/f64 full-AOT scalar typed-direct 矩阵验收护栏完成；07~12 总目标继续进行中。完成项目：i64 5 个
  smoke case 与 f64 19 个 smoke case 全部切到 `requireFullAot`，要求对应 `zr_aot_static_i64/f64_*_direct_call_full_aot`
  marker，并禁止 `CanUseTypedDirectCall`、`DeoptTypedDirectCall`、`SyncSignedIntLocal` / `SyncFloatLocal`。
  生产生成器未改动；当前 full-AOT scalar typed-direct 实现已覆盖这些路径，本切片把 i64/f64 覆盖提升为完整回归门。
  RED：不适用，本护栏扩展直接 GREEN。GREEN：WSL GCC/Clang i64 smoke 均 5/0；WSL GCC/Clang f64 smoke 均 19/0；
  MSVC Debug i64/f64 smoke 分别 5/19 expected Unix-only ignores / 0 failures。生成物检查确认 i64 two-arg 与 f64
  three-arg C 代码含 full-AOT marker 和直接 thunk assignment，未见 metadata guard/deopt/sync helper。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-i64-f64-typed-direct-call-matrix-guardrail.md`。
  备注：in/out、GC roots/exports/frame cleanup、性能计数和完整 07-S5 仍待后续。

- 2026-07-06 04:01:42 +08:00 · 07-S5 call-result runtime helper sync compact guard ·
  状态：CallStackValue / CallStaticDirect / CallDynamicDeoptBridge result-sync guard 压缩完成；07~12 总目标继续进行中。
  完成项目：call boundary 生成器在 typed scalar destination 需要同步时发出
  `zr_aot_call_result_sync_compact`，并把 `CallStackValue(...)`、`CallStaticDirect(...)` 或
  `CallDynamicDeoptBridge(...)` 与对应 `Sync*Local(...)` 合并为一个 `ZR_AOT_C_GUARD(... && ...)`；无 typed
  result sync 的 call boundary 保持旧单 guard。RED：WSL GCC call contracts 8 tests / 3 failures，缺少 compact
  marker 且仍有独立 sync guard。GREEN：WSL GCC/Clang call contracts 8/0、source contracts 24/0、call smoke
  5/0、dynamic deopt bridge smoke 7/0；MSVC Debug call contracts 8/0、source contracts 24/0、call smoke 5
  expected Unix-only ignores / 0 failures、dynamic smoke 7 expected Unix-only ignores / 0 failures。生成物检查确认
  static direct 与 dynamic deopt bridge C 代码含 compact marker 和单 `Call* && SyncSignedIntLocal` guard。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-call-result-sync-compact-guard.md`。
  备注：in/out、GC roots/exports/frame cleanup、性能计数和完整 07-S5 仍待后续。

- 2026-07-06 03:31:13 +08:00 · 07-S5 typed direct-call deopt sync compact guard ·
  状态：non-full-AOT typed direct-call fallback guard 压缩完成；07~12 总目标继续进行中。
  完成项目：i64/u64/f64/bool metadata guarded typed direct-call 的 deopt 分支现在用
  `zr_aot_static_typed_direct_call_deopt_sync_compact` 标记，并把 `DeoptTypedDirectCall(...)`
  与对应 `Sync*Local(...)` 合并为一个 `ZR_AOT_C_GUARD(... && ...)`，保留 metadata drift
  fallback 与 result local 回读。RED：WSL GCC call contracts 8 tests / 4 failures，缺少 compact
  marker 且仍有独立 sync guard。GREEN：WSL GCC/Clang call contracts 8/0、typed-call contracts
  4/0、i64 typed-direct smoke 5/0、f64 typed-direct smoke 19/0、source contracts 24/0；
  MSVC Debug call contracts 8/0、typed-call contracts 4/0、i64/f64 smoke 分别 5/19 expected
  Unix-only ignores / 0 failures、source contracts 24/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-typed-direct-call-deopt-sync-compact-guard.md`。
  备注：in/out、GC roots/exports/frame cleanup、性能计数和完整 07-S5 仍待后续。

- 2026-07-06 03:11:05 +08:00 · 07-S5 dynamic value-access compact deopt bridge ·
  状态：动态 value-access deopt bridge guard 压缩完成；07~12 总目标继续进行中。完成项目：
  `backend_aot_c_value_access_boundaries.c` 对 member/index dynamic value access 先发
  `zr_aot_value_dynamic_deopt_bridge_compact`，再把 SemIR deopt 验证和实际 runtime helper 调用合并为一个
  `ZR_AOT_C_GUARD(ValidateDynamicDeoptBridge(...) && Get/Set...)`，减少 generated C 的重复 guard
  模板。RED：WSL GCC dynamic deopt bridge smoke 7/1，缺少 compact marker。GREEN：WSL GCC/Clang
  dynamic deopt bridge smoke 均 7/0；WSL GCC/Clang global contracts 均 9/0；MSVC Debug
  dynamic smoke 7 tests / 0 failures / 7 expected Unix-only ignores；MSVC Debug global contracts 9/0。
  生成物检查确认 member/index deopt bridge generated C 含 compact marker 和单 guard 表达式。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-dynamic-value-access-compact-deopt-bridge.md`。
  备注：in/out、GC roots/exports/frame cleanup、性能计数和完整 07-S5 仍待后续。

- 2026-07-06 02:57:38 +08:00 · 07-S5 full-AOT scalar typed direct-call matrix guardrail ·
  状态：bool/u64 full-AOT scalar typed direct-call 矩阵验收护栏完成；07~12 总目标继续进行中。
  完成项目：bool typed-direct 28 个 case 与 u64 typed-direct 25 个 case 全部切到 `requireFullAot`，
  要求 `*_direct_call_full_aot` marker，并禁止 metadata guard/deopt 与 `Sync*Local`。生产生成器未改动；
  此前 full-AOT scalar typed-direct 实现已覆盖这些路径，本切片把代表覆盖提升为完整回归门。
  RED：不适用，本护栏扩展直接 GREEN。GREEN：WSL GCC bool/u64 smoke 28/0、25/0；WSL Clang
  bool/u64 smoke 28/0、25/0；MSVC Debug bool/u64 smoke 分别 28/25 expected Unix-only ignores /
  0 failures；MSVC Debug typed-call contracts 4/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-scalar-typed-direct-call-matrix-guardrail.md`。
  备注：in/out、动态边界、GC roots/exports/frame cleanup、性能计数和完整 07-S5 仍待后续。

- 2026-07-06 02:45:48 +08:00 · 07-S5 full-AOT inline struct typed-call boundaries ·
  状态：full-AOT inline-struct typed-call metadata guard/deopt 消除完成；07~12 总目标继续进行中。
  完成项目：`requireFullAot` 的 generic shared 与 ordinary static inline-struct `CALL_TYPED` callsite
  现在直接生成
  `zr_aot_value_exec_call_typed_inline_struct_full_aot_direct` 和固定 thunk 的
  `ZrLibrary_AotRuntime_CallInlineStruct(state, ...)`，不再包 `CanUseTypedDirectCall`、
  inline-struct dynamic deopt 或 drift error。RED：WSL GCC generic call typed 7/1，缺少 full-AOT
  inline-struct direct marker；WSL GCC call shared-library smoke 5/1，普通 static inline-struct call
  smoke 缺少同一 marker。GREEN：WSL GCC/Clang generic call typed 均 7/0；WSL GCC/Clang call
  shared-library smoke 均 5/0；WSL GCC/Clang typed-call contracts 均 4/0；WSL GCC value SemIR
  contracts 4/0、source contracts 24/0；MSVC Debug generic call typed 7 tests / 0 failures /
  3 expected Unix-only ignores；MSVC Debug call shared-library smoke 5 tests / 0 failures /
  5 expected Unix-only ignores；MSVC Debug value SemIR contracts 4/0、source contracts 24/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-inline-struct-typed-call-boundary.md`。
  备注：普通 non-full-AOT inline-struct drift fallback 保留；in/out、动态边界和完整 07-S5 仍待后续。

- 2026-07-06 02:21:09 +08:00 · 07-S5 full-AOT mixed bool-return typed direct-call boundary ·
  状态：full-AOT mixed scalar bool-return typed direct-call metadata guard/result sync 消除完成；07~12
  总目标继续进行中。完成项目：full-AOT typed direct-call fast path 覆盖 i64/u64/f64 two-arg
  参数返回 bool 的 route，按参数类型分别生成 `zr_aot_static_i64/u64/f64_bool_two_arg_direct_call_full_aot`
  和 `zr_aot_bD = zr_aot_typed_bool_fn_N(zr_aot_s/u/fA, zr_aot_s/u/fB);`，避免 metadata
  guard/deopt、`SyncBoolLocal` 与 typed-destination 写入。RED：WSL GCC bool typed-direct smoke 28/1，
  缺少 i64 bool full-AOT marker。GREEN：WSL GCC bool smoke 28/0；WSL Clang bool smoke 28/0；
  WSL GCC/Clang typed-call contracts 4/0；MSVC Debug typed-call contracts 4/0；MSVC Debug bool smoke
  28 expected ignored / 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-mixed-bool-return-typed-direct-call-boundary.md`。
  备注：scalar typed-to-typed direct-call guard 消除已覆盖当前 no/one/two/three/mixed bool-return
  代表路径；完整 07-S5 仍需 inline structs、in/out、动态边界等后续项。

- 2026-07-06 02:07:36 +08:00 · 07-S5 full-AOT two/three-arg typed direct-call boundary ·
  状态：full-AOT two/three-arg typed direct-call metadata guard/result sync 消除完成；07~12 总目标继续进行中。
  完成项目：regular typed direct-call dispatcher 在同类型 bool/i64/u64/f64 two/three-arg can-write
  证明成立时，直接生成 `zr_aot_static_*_two_arg_direct_call_full_aot` /
  `zr_aot_static_*_three_arg_direct_call_full_aot` 和 scalar thunk assignment；需要 `state` 的 thunk
  仍保留 `state` 实参。u64 smoke 覆盖 two/three-arg 的 state-free 与 stateful 四个代表形态，并拒绝
  metadata guard/deopt、`SyncUnsignedIntLocal` 与 typed-destination 写入。RED：WSL GCC u64 typed-direct
  smoke 25/1，缺少 two-arg full-AOT marker。GREEN：WSL GCC u64 smoke 25/0；WSL Clang u64 smoke
  25/0；WSL GCC/Clang typed-call contracts 4/0；MSVC Debug typed-call contracts 4/0；MSVC Debug
  u64 smoke 25 expected ignored / 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-two-three-arg-typed-direct-call-boundary.md`。
  备注：mixed-argument bool-return route 仍待后续 full-AOT 收口。

- 2026-07-06 01:46:11 +08:00 · 07-S5 full-AOT one-arg typed direct-call boundary ·
  状态：full-AOT 一参 typed direct-call metadata guard/result sync 消除完成；07~12 总目标继续进行中。
  完成项目：regular typed direct-call dispatcher 接收 `requireFullAot`；full-AOT 下 bool/i64/u64/f64
  可证明一参 typed call 直接生成 `zr_aot_static_*_one_arg_direct_call_full_aot` 和 scalar thunk
  assignment，避免 `CanUseTypedDirectCall`、`DeoptTypedDirectCall` 以及回退 `Sync*Local`。u64
  identity one-arg smoke 切到 full-AOT，并拒绝 metadata guard/deopt、`SyncUnsignedIntLocal` 与
  typed-destination 写入；非 full-AOT u64 smoke cases 补齐默认断言字段。RED：WSL GCC u64 typed-direct
  smoke 25/1，缺少 one-arg full-AOT marker。GREEN：WSL GCC u64 smoke 25/0；WSL Clang u64 smoke
  25/0；WSL GCC/Clang typed-call contracts 4/0；MSVC Debug typed-call contracts 4/0；MSVC Debug
  u64 smoke 25 expected ignored / 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-one-arg-typed-direct-call-boundary.md`。
  备注：two/three-arg full-AOT typed direct-call guard 消除仍待后续。

- 2026-07-06 01:28:16 +08:00 · 07-S5 full-AOT no-arg typed direct-call result boundary ·
  状态：full-AOT 无参 typed direct-call result 边界同步消除完成；07~12 总目标继续进行中。完成项目：
  no-arg typed direct-call dispatcher 从函数体接收 `requireFullAot`；full-AOT 下 bool/i64/u64/f64
  可证明无参 typed call 直接写 `zr_aot_static_*_no_arg_direct_call_full_aot` 和 scalar thunk
  assignment，避免 `CanUseTypedDirectCall`、`DeoptTypedDirectCall` 以及回退 `Sync*Local`。静态 numeric
  call smoke 切到 full-AOT，锁定 u64/f64 call result 后续 scalar stack-copy 和 arithmetic 仍不经
  `SZrTypeValue`。RED：WSL GCC call smoke 5/1，缺少 full-AOT direct-call marker。GREEN：WSL GCC
  call smoke 5/0；WSL Clang call smoke 5/0；WSL GCC/Clang typed-call contracts 4/0；MSVC Debug call
  smoke 5 expected ignored / 0 failures；MSVC Debug typed-call contracts 4/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s5-full-aot-no-arg-typed-direct-call-boundary.md`。
  备注：非 full-AOT metadata guard/deopt fallback 保留，用于动态元数据漂移。

- 2026-07-06 01:13:37 +08:00 · 07-S3/S4 generic equality zero-frame body guardrail ·
  状态：pure primitive generic equality zero-frame body 回归门完成；07~12 总目标继续进行中。完成项目：
  bool/i64/u64+f64/mixed primitive equality 四个本地标量 shared-library smoke 现在显式断言
  `.registerFrameBytes = 0u`、`frameByteSize=0`，且没有 generated frame setup、`ZrAotGeneratedFrame`、
  `frame.slotBase`、stack/GC check 或 stack reset frame setup。审计将 call-result equality 与旧
  numeric arithmetic direct-call sync 边界保留给 07-S5/更广 value-copy migration。验证：WSL GCC
  `zr_vm_aot_c_generic_bool_equality_local_smoke_test` 5/0；WSL Clang 同目标 5/0；MSVC Debug 同目标
  5 expected ignored / 0 failures。产出：
  `tests/acceptance/2026-07-06-aot-07-s3-s4-generic-equality-zero-frame-guardrail.md`。

- 2026-07-06 00:58:36 +08:00 · 07-S3/S4 generic numeric unsigned constants zero-frame body local ·
  状态：proven unsigned/generic numeric scalar-local 常量与相关 mixed numeric bodies 的 zero-frame
  body gate 完成；07~12 总目标继续进行中。完成项目：unsigned `GET_CONSTANT` 在 proven u64
  scalar-local destination 上进入 frame descriptor local-only 判定；generic u64 `SUB`/`MUL`/`DIV`/`MOD`
  进入 u64 local consumer proof；typed unsigned `ADD_UNSIGNED`/`SUB_UNSIGNED`/`MUL_UNSIGNED`/
  `DIV_UNSIGNED`/`MOD_UNSIGNED` 纳入 frame descriptor 与 scalar-local read gates。覆盖 u64
  `ADD`/`SUB`/`MUL`/`DIV`/`MOD`、u64 result-stack-copy `MUL`、mixed i64/u64
  `ADD`/`SUB`/`MUL`/`DIV`/`MOD` 与 result-stack-copy `MUL`、mixed u64/f64
  `ADD`/`SUB`/`MUL`/`DIV`/`MOD`。RED：新增 zero-frame generated-C guard 后暴露 unsigned constant
  未进入 local-only frame descriptor，以及 generic u64 consumer 只覆盖 ADD/NEG。GREEN：
  WSL GCC generic numeric smoke 50/0；WSL Clang generic numeric smoke 50/0；WSL GCC frame setup
  contracts 1/0；WSL Clang frame setup contracts 1/0；MSVC Debug generic numeric smoke
  50 expected ignored / 0 failures；MSVC Debug frame setup contracts 1/0。生成物扫描确认
  zero-register-frame generated files 中 frame setup marker 为 0，u64/mixed generated files 中 primitive
  constant fallback 为 0。产出：
  `tests/acceptance/2026-07-06-aot-07-s3-s4-generic-numeric-unsigned-constant-zero-frame-local.md`。
  备注：dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、
  byte-frame narrowing 剩余形态、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-06 00:14:47 +08:00 · 07-S3/S4 generic numeric zero-frame body local ·
  状态：proven generic numeric scalar-local body 的 frame setup elision 完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_frame_descriptor.c` 现在对 generic numeric
  `ADD`/`ADD_STRING`/`SUB`/`MUL`/`DIV`/`MOD`/`NEG` 镜像 lowering 的 same-kind、mixed i64/u64、
  mixed f64+integer 与 unary `NEG` 标量证明；当 `.registerFrameBytes = 0u` 且 value SemIR
  `frameByteSize=0` 时，covered bodies 不再输出 generated frame setup、`ZrAotGeneratedFrame frame = {0};`、
  stack/GC check 或 `frame.slotBase` 绑定。RED：zero-frame guard 首次加入后 WSL GCC generic numeric
  smoke 50 个用例中 4 个失败，失败点均为仍存在 `/* zr_aot_generated_frame_setup */`。GREEN：
  WSL GCC generic numeric smoke 50/0；WSL Clang generic numeric smoke 50/0；WSL GCC frame setup
  contracts 1/0；MSVC Debug generic numeric smoke 50 expected ignored / 0 failures；MSVC Debug
  frame setup contracts 1/0。产出：
  `tests/acceptance/2026-07-06-aot-07-s3-s4-generic-numeric-zero-frame-body-local.md`。
  备注：该切片只关闭 proven generic numeric scalar-local zero-frame body gate；dynamic/unproven
  operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing 剩余形态、
  性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 23:54:29 +08:00 · 07-S2/S4 generic numeric result stack-copy guarded DIV/MOD matrix coverage ·
  状态：straight-line copied generic numeric result feeding guarded `DIV`/`MOD` 的 f64/i64 左/右操作数矩阵覆盖完成；
  07~12 总目标继续进行中。完成项目：新增 f64 `ADD -> SET_STACK -> MOD`、
  f64 `ADD -> SET_STACK -> MOD(right copied result)`、i64 `ADD -> SET_STACK -> DIV` 与
  i64 `ADD -> SET_STACK -> DIV(right copied result)` shared-library smoke。该覆盖未触发 production RED；
  本切片未修改生产代码。生成 C 输出 f64/i64 stack-copy local、后续 f64 MOD local 与 i64 DIV local，
  并在 copied result 位于左/右操作数时都保留对应零检查；避开 targeted `CopyStack`、
  `GenericNumericDiv`/`GenericNumericMod`、sync boundary、generic numeric binary boundary 与 copied
  slot value materialization。MSVC 初次复核暴露测试辅助函数签名把 `EZrInstructionCode` 泄漏到
  Windows 路径；已将新 guarded assertion helpers 限定在 Unix 路径并补齐 Windows expected ignores。
  验证：WSL GCC generic numeric smoke 50/0；WSL Clang generic numeric smoke 50/0；MSVC Debug
  generic numeric shared-library smoke 0 failures / 50 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-result-stack-copy-div-mod-matrix-local.md`。
  备注：该切片只关闭 f64/i64 result-copy feeding guarded `DIV`/`MOD` 的 straight-line 矩阵；
  dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、
  性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 23:41:38 +08:00 · 07-S2/S4 generic numeric result stack-copy right DIV/MOD guard coverage ·
  状态：straight-line copied generic numeric result feeding right-hand guarded `DIV`/`MOD` 覆盖完成；07~12 总目标继续进行中。
  完成项目：新增 f64 `ADD -> SET_STACK -> DIV(right copied result)` 与 i64
  `ADD -> SET_STACK -> MOD(right copied result)` shared-library smoke。该覆盖未触发 production RED；
  首次 WSL GCC 命令在 120s 预算内超时且未产出测试结果，延长超时后 GREEN；本切片未修改生产代码。
  生成 C 输出 `zr_aot_scalar_stack_copy_f64` / `zr_aot_scalar_stack_copy_i64`、后续 f64 DIV local 与
  i64 MOD local，并对 copied result 右操作数保留零检查；避开 targeted `CopyStack`、
  `GenericNumericDiv`/`GenericNumericMod`、sync boundary、generic numeric binary boundary 与 copied slot
  value materialization。验证：WSL GCC generic numeric smoke 46/0；WSL Clang generic numeric smoke 46/0；
  MSVC Debug generic numeric shared-library smoke 0 failures / 46 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-result-stack-copy-right-div-mod-guard-local.md`。
  备注：该切片只关闭 f64/i64 result-copy feeding right-hand guarded `DIV`/`MOD` 的窄形态；
  dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、
  性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 23:31:33 +08:00 · 07-S2/S4 generic numeric result stack-copy DIV/MOD guard coverage ·
  状态：straight-line copied generic numeric result feeding later guarded `DIV`/`MOD` 覆盖完成；07~12 总目标继续进行中。
  完成项目：新增 f64 `ADD -> SET_STACK -> DIV` 与 i64 `ADD -> SET_STACK -> MOD`
  shared-library smoke。该覆盖首跑即 GREEN，不作为新的 production RED；本切片未修改生产代码。
  生成 C 输出 f64/i64 ADD local、`zr_aot_scalar_stack_copy_f64` / `zr_aot_scalar_stack_copy_i64`、后续
  f64 DIV local 与 i64 MOD local，并保留 `DIV`/`MOD` 零检查；避开 targeted `CopyStack`、
  `GenericNumericDiv`/`GenericNumericMod`、sync boundary、generic numeric binary boundary 与 copied slot
  value materialization。验证：WSL GCC generic numeric smoke 44/0；WSL Clang generic numeric smoke 44/0；
  MSVC Debug generic numeric shared-library smoke 0 failures / 44 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-result-stack-copy-div-mod-guard-local.md`。
  备注：该切片只关闭 f64/i64 result-copy feeding later guarded `DIV`/`MOD` 的窄形态；
  dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、
  性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 23:19:38 +08:00 · 07-S2/S4 generic numeric mixed u64/f64 result stack-copy MUL coverage ·
  状态：straight-line copied mixed u64/f64 result feeding later f64 `MUL` 覆盖完成；07~12 总目标继续进行中。
  完成项目：新增 `GET_CONSTANT uint -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> MUL -> RETURN`
  shared-library smoke。该覆盖首跑即 GREEN，不作为新的 production RED；上一条 mixed-f64 result proof 已能证明
  u64/f64 promoted result copies。生成 C 输出 mixed f64 ADD local、`zr_aot_scalar_stack_copy_f64`、后续
  f64 MUL local，并避开 targeted `CopyStack`、`GenericNumericMul`、`SyncFloatLocal` 与 copied slot value
  materialization。验证：WSL GCC generic numeric smoke 42/0；WSL Clang generic numeric smoke 42/0；
  MSVC Debug generic numeric shared-library smoke 0 failures / 42 expected ignores；上一条 production GREEN 的
  generic numeric contracts、source contracts、power/logical/equality 相邻矩阵已覆盖且通过。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-u64-f64-result-stack-copy-mul-local.md`。
  备注：该切片只关闭 mixed u64/f64 result-copy feeding later `MUL` 的窄形态；dynamic/unproven operands、
  更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame
  typed bodies 仍待后续。

- 2026-07-05 23:12:16 +08:00 · 07-S2/S4 generic numeric mixed i64/f64 result stack-copy MUL local ·
  状态：straight-line copied mixed i64/f64 result feeding later f64 `MUL` 完成；07~12 总目标继续进行中。
  完成项目：新增 `GET_CONSTANT int -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> MUL -> RETURN`
  shared-library smoke。RED 显示 copied mixed-f64 result 的 stack-copy 已本地化，但后续 `MUL` 仍回退到
  `GenericNumericMul`。GREEN 后 f64 before-instruction value-kind proof 可结合 integer proof，把一侧 f64
  另一侧 i64/u64 的 generic numeric binary result 追溯为 f64。生成 C 输出 mixed f64 ADD local、
  `zr_aot_scalar_stack_copy_f64`、后续 f64 MUL local，并避开 targeted `CopyStack`、`GenericNumericMul`、
  `SyncFloatLocal` 与 copied slot value materialization。验证：WSL GCC generic numeric smoke 41/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归通过；WSL Clang
  同组通过；MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 41、1、8、5、4、1、1
  expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-f64-result-stack-copy-mul-local.md`。
  备注：该切片只关闭 mixed i64/f64 result-copy feeding later `MUL` 的窄形态；dynamic/unproven operands、
  更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame
  typed bodies 仍待后续。

- 2026-07-05 22:45:37 +08:00 · 07-S2/S4 generic numeric mixed i64/u64 result stack-copy MUL coverage ·
  状态：straight-line copied mixed i64/u64 signed-result feeding later mixed `MUL` 覆盖完成；07~12 总目标继续进行中。
  完成项目：新增 `GET_CONSTANT int -> GET_CONSTANT uint -> ADD -> SET_STACK -> GET_CONSTANT uint -> MUL -> RETURN`
  shared-library smoke。该覆盖首跑即 GREEN，不作为新的 production RED；integer value-kind proof 已能证明
  mixed i64/u64 signed result copies。生成 C 输出 mixed i64/u64 ADD local、`zr_aot_scalar_stack_copy_i64`、
  mixed i64/u64 MUL local，并避开 targeted `CopyStack`、`GenericNumericMul`、`SyncSignedIntLocal`、误判的
  `TZrFloat64 zr_aot_f4` 与 copied slot value materialization。验证：WSL GCC generic numeric smoke 40/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归通过；WSL Clang
  同组通过；MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 40、1、8、5、4、1、1
  expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-u64-result-stack-copy-mul-local.md`。
  备注：该切片只关闭 mixed i64/u64 result-copy feeding later `MUL` 的窄形态；dynamic/unproven operands、
  更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame
  typed bodies 仍待后续。

- 2026-07-05 22:25:01 +08:00 · 07-S2/S4 generic numeric u64 result stack-copy MUL coverage ·
  状态：straight-line copied u64 generic numeric result feeding later `MUL` 覆盖完成；07~12 总目标继续进行中。
  完成项目：新增 `GET_CONSTANT uint -> GET_CONSTANT uint -> ADD -> SET_STACK -> GET_CONSTANT uint -> MUL -> RETURN`
  shared-library smoke。该覆盖首跑即 GREEN，不作为新的 production RED；上一条 i64 RED/GREEN 的 integer
  value-kind proof 已能证明 u64 generic result copies。生成 C 输出 u64 ADD local、`zr_aot_scalar_stack_copy_u64`、
  u64 MUL local，并避开 targeted `CopyStack`、`GenericNumericMul`、`SyncUnsignedIntLocal`、误判的
  `TZrFloat64 zr_aot_f4` 与 copied slot value materialization。验证：WSL GCC generic numeric smoke 39/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归通过；WSL Clang
  同组通过；MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 39、1、8、5、4、1、1
  expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-u64-result-stack-copy-mul-local.md`。
  备注：该切片只关闭 u64 result-copy feeding later `MUL` 的窄形态；dynamic/unproven operands、更广
  value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame
  typed bodies 仍待后续。

- 2026-07-05 22:05:29 +08:00 · 07-S2/S4 generic numeric i64 result stack-copy MUL local ·
  状态：straight-line copied i64 generic numeric result feeding later `MUL` 完成；07~12 总目标继续进行中。
  完成项目：新增 `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> SET_STACK -> GET_CONSTANT int -> MUL -> RETURN`
  shared-library smoke。RED 显示 copied i64 result 未被 proof 识别，生成 C 仍回退到
  `GenericNumericAdd` / `CopyStack` / `GenericNumericMul`，且 copied slot 被误判为 f64。GREEN 后
  scalar-local proof 可回溯 integer generic results 与 stack copies，stack-copy destination recording 能在
  generic destination pass 前恢复 sourceKind，并用 explicit i64 generic numeric consumer branch 防止 f64
  误分类。生成 C 输出 i64 ADD local、`zr_aot_scalar_stack_copy_i64`、i64 MUL local，并避开 targeted
  `CopyStack`、`GenericNumericMul`、`SyncSignedIntLocal` 与 copied slot value materialization。验证：
  WSL GCC generic numeric smoke 38/0、generic numeric contracts 1/0、source contracts 24/0 及
  power/logical/equality 相邻回归通过；WSL Clang 同组通过；MSVC Debug 构建 focused targets 并通过
  contracts，Unix-only smokes 为 0 failures / 38、1、8、5、4、1、1 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-i64-result-stack-copy-mul-local.md`。
  备注：该切片只关闭 i64 result-copy feeding later `MUL` 的窄形态；dynamic/unproven operands、更广
  value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame
  typed bodies 仍待后续。

- 2026-07-05 21:30:48 +08:00 · 07-S2/S4 generic numeric f64 chained result stack-copy MUL coverage ·
  状态：straight-line f64 chained result-copy 覆盖完成；07~12 总目标继续进行中。完成项目：
  `GET_CONSTANT float -> GET_CONSTANT float -> DIV -> SET_STACK -> SET_STACK -> GET_CONSTANT float -> MUL -> RETURN`
  新增 executable smoke，确认既有 f64 stack-copy chain proof 已输出两段
  `zr_aot_scalar_stack_copy_f64` local copy 和后续 `zr_aot_generic_numeric_f64_mul_scalar_local` direct expression，
  并避开 targeted `CopyStack`、`GenericNumericMul`、`SyncFloatLocal` 与 copied slots 4/6 value-slot materialization。
  该覆盖首跑即 GREEN，不作为 production RED；本切片未修改生产代码。验证：WSL GCC generic numeric smoke 37/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归通过；WSL Clang 同组通过；
  MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 37、1、8、5、4、1、1
  expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-f64-chained-result-stack-copy-local.md`。
  备注：该切片只关闭链式 f64 result-copy feeding later `MUL` 的窄形态；dynamic/unproven operands、更广
  value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies
  仍待后续。

- 2026-07-05 21:04:58 +08:00 · 07-S2/S4 generic numeric mixed u64+f64 coverage + mixed i64/u64 live proof ·
  状态：proven `GET_CONSTANT uint -> GET_CONSTANT float -> ADD/SUB/MUL/DIV/MOD -> RETURN` 常量形态覆盖完成，且
  mixed i64/u64 live-consumer proof 缺口完成；07~12 总目标继续进行中。完成项目：新增 mixed u64/f64 五个 executable
  smoke，确认既有 mixed-f64 scalar-local lowering 已输出 `zr_aot_scalar_constant_u64_local` /
  `zr_aot_scalar_constant_f64_local`、`zr_aot_generic_numeric_mixed_f64_*_scalar_local`、direct f64 expression、
  DIV/MOD zero guard，并避开 targeted generic numeric runtime boundary 与 `SyncFloatLocal`。该 u64/f64 覆盖首跑即
  GREEN，不作为 production RED。实际 RED 来自 scalar-local source contract：mixed i64/u64 u64 consumer branch 未对
  `DIV/MOD` 使用 explicit generic numeric u64 proof。修复后 `backend_aot_c_scalar_locals.c` 对 i64/u64 local consumers
  统一检查 same-kind、mixed i64/u64、mixed-f64 proof across ADD/SUB/MUL/DIV/MOD。GREEN 通过 WSL GCC generic numeric
  smoke 36/0、generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组
  通过；MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 36、1、8、5、4、1、1
  expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-u64-f64-coverage-and-mixed-i64-u64-live-proof.md`。
  备注：proven mixed u64/f64 constant coverage 与 mixed i64/u64 live proof 已收口；dynamic/unproven operands、更广
  value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies
  仍待后续。

- 2026-07-05 20:20:17 +08:00 · 07-S2/S4 generic numeric mixed i64+u64 DIV/MOD local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT uint -> DIV/MOD -> RETURN` 的 proven mixed i64/u64 除法/取模折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `DIV` / `MOD` 现在在一侧为 signed i64 scalar local、另一侧为
  unsigned u64 scalar local 且 destination 可保持 i64 local 时输出
  `zr_aot_generic_numeric_mixed_i64_u64_div_scalar_local` /
  `zr_aot_generic_numeric_mixed_i64_u64_mod_scalar_local`、转换后右操作数零检查、`"divide by zero"` /
  `"modulo by zero"`、`zr_aot_s2 = zr_aot_s0 / (TZrInt64)zr_aot_u1;` /
  `zr_aot_s2 = zr_aot_s0 % (TZrInt64)zr_aot_u1;`；该结果类型沿用 runtime 的 mixed integer 语义：非双 unsigned
  且非 float 时返回 signed i64。未证明的 dynamic/generic operands 继续走
  `ZrLibrary_AotRuntime_GenericNumericDiv` / `GenericNumericMod` fallback。RED/GREEN：RED 新增 mixed i64+u64
  DIV/MOD smoke 后，generic numeric smoke 31 项中两个新增用例失败于缺少 mixed i64/u64 local marker、zero guard 和
  signed-result direct expressions。GREEN 通过 WSL GCC generic numeric smoke 31/0、generic numeric contracts 1/0、
  source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets
  并通过 contracts，Unix-only smokes 为 0 failures / 31、1、8、5、4、1、1 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-u64-div-mod-local.md`。
  备注：mixed i64/u64 scalar-local generic ADD/SUB/MUL/DIV/MOD 已收口；mixed u64/f64、dynamic/unproven operands、
  更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies
  仍待后续。

- 2026-07-05 19:49:11 +08:00 · 07-S2/S4 generic numeric mixed i64+u64 SUB/MUL local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT uint -> SUB/MUL -> RETURN` 的 proven mixed i64/u64 减法/乘法折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `SUB` / `MUL` 现在在一侧为 signed i64 scalar local、另一侧为
  unsigned u64 scalar local 且 destination 可保持 i64 local 时输出
  `zr_aot_generic_numeric_mixed_i64_u64_sub_scalar_local` /
  `zr_aot_generic_numeric_mixed_i64_u64_mul_scalar_local` 和
  `zr_aot_s2 = zr_aot_s0 - (TZrInt64)zr_aot_u1;` /
  `zr_aot_s2 = zr_aot_s0 * (TZrInt64)zr_aot_u1;`；该结果类型沿用 runtime 的 mixed integer 语义：非双 unsigned
  且非 float 时返回 signed i64。未证明的 dynamic/generic operands 继续走
  `ZrLibrary_AotRuntime_GenericNumericSub` / `GenericNumericMul` fallback。RED/GREEN：RED 新增 mixed i64+u64
  SUB/MUL smoke 后，generic numeric smoke 29 项中两个新增用例失败于缺少 mixed i64/u64 local marker 和
  signed-result direct expressions。GREEN 通过 WSL GCC generic numeric smoke 29/0、generic numeric contracts 1/0、
  source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets
  并通过 contracts，Unix-only smokes 为 0 failures / 29、1、8、5、4、1、1 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-u64-sub-mul-local.md`。
  备注：mixed i64/u64 scalar-local generic ADD/SUB/MUL 已收口；mixed i64/u64 DIV/MOD、mixed u64/f64、
  dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和
  完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 19:11:42 +08:00 · 07-S2/S4 generic numeric mixed i64+u64 ADD local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT uint -> ADD -> RETURN` 的 proven mixed i64/u64 加法折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `ADD` 现在在一侧为 signed i64 scalar local、另一侧为
  unsigned u64 scalar local 且 destination 可保持 i64 local 时输出
  `zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local` 和
  `zr_aot_s2 = zr_aot_s0 + (TZrInt64)zr_aot_u1;`；该结果类型沿用 runtime 的 mixed integer 语义：非双 unsigned
  且非 float 时返回 signed i64。对应 int/uint 常量保留为 `zr_aot_scalar_constant_i64_local` /
  `zr_aot_scalar_constant_u64_local`，目的 i64 结果不再通过 `SyncSignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericAdd` fallback。RED/GREEN：RED 新增 mixed
  i64+u64 ADD smoke 后，generic numeric smoke 27 项中新增一个用例失败于缺少 mixed i64/u64 local marker 和
  signed-result direct expression。GREEN 通过 WSL GCC generic numeric smoke 27/0、generic numeric contracts 1/0、
  source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets
  并通过 contracts，Unix-only smokes 为 0 failures / 27、1、8、5、4、1、1 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-u64-add-local.md`。
  备注：mixed i64/u64 scalar-local generic ADD 已收口；mixed i64/u64 SUB/MUL/DIV/MOD、mixed u64/f64、
  dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和
  完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 18:34:46 +08:00 · 07-S2/S4 generic numeric mixed i64+f64 DIV/MOD local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT float -> DIV/MOD -> RETURN` 的 proven mixed i64/f64 除法/取模折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `DIV` / `MOD` 现在在一侧为 signed i64 scalar local、另一侧为
  f64 scalar local 且 destination 可保持 f64 local 时输出
  `zr_aot_generic_numeric_mixed_f64_div_scalar_local` / `zr_aot_generic_numeric_mixed_f64_mod_scalar_local`、
  f64 零检查、`"divide by zero"` / `"modulo by zero"`、direct `/` 和 `fmod` 表达式；对应 int/float 常量保留为
  `zr_aot_scalar_constant_i64_local` / `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再通过 `SyncFloatLocal`
  回写 value slot。RED/GREEN：RED 新增 mixed i64+f64 DIV/MOD smoke 后，generic numeric smoke 26 项中新增两个用例失败
  于缺少 mixed f64 DIV/MOD local markers。GREEN 通过 WSL GCC generic numeric smoke 26/0、generic numeric contracts
  1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets
  并通过 contracts，Unix-only smokes 为 0 failures / 26、1、8、5、4、1、1 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-f64-div-mod-local.md`。
  备注：proven mixed i64/f64 scalar-local generic ADD/SUB/MUL/DIV/MOD 已收口；mixed u64/f64、mixed i64/u64 integer
  arithmetic、dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame
  narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 18:08:52 +08:00 · 07-S2/S4 generic numeric mixed i64+f64 SUB/MUL local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT float -> SUB/MUL -> RETURN` 的 proven mixed i64/f64 减法/乘法折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `SUB` / `MUL` 现在在一侧为 signed i64 scalar local、另一侧为
  f64 scalar local 且 destination 可保持 f64 local 时输出
  `zr_aot_generic_numeric_mixed_f64_sub_scalar_local` / `zr_aot_generic_numeric_mixed_f64_mul_scalar_local` 和
  `zr_aot_f2 = (TZrFloat64)zr_aot_s0 - zr_aot_f1;` / `zr_aot_f2 = (TZrFloat64)zr_aot_s0 * zr_aot_f1;`；
  该结果类型沿用 runtime 的 float promotion 语义。对应 int/float 常量保留为
  `zr_aot_scalar_constant_i64_local` / `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再通过 `SyncFloatLocal`
  回写 value slot。未证明的 dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericSub` /
  `GenericNumericMul` fallback。RED/GREEN：RED 新增 mixed i64+f64 SUB/MUL smoke 后，generic numeric smoke 24 项中
  新增两个用例失败于缺少 mixed f64 local markers 和 direct cast 表达式。GREEN 通过 WSL GCC generic numeric smoke
  24/0、generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；
  MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 24、1、8、5、4、1、1 expected
  ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-f64-sub-mul-local.md`。
  备注：proven mixed i64/f64 scalar-local generic ADD/SUB/MUL 已收口；mixed DIV/MOD、mixed u64/f64、mixed i64/u64
  integer arithmetic、dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame
  narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 17:40:01 +08:00 · 07-S2/S4 generic numeric mixed i64+f64 ADD local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT float -> ADD -> RETURN` 的 proven mixed i64/f64 加法折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `ADD` 现在在一侧为 signed i64 scalar local、另一侧为
  f64 scalar local 且 destination 可保持 f64 local 时输出 `zr_aot_generic_numeric_mixed_f64_add_scalar_local` 和
  `zr_aot_f2 = (TZrFloat64)zr_aot_s0 + zr_aot_f1;`；该结果类型沿用 runtime 的 float promotion 语义。对应
  int/float 常量保留为 `zr_aot_scalar_constant_i64_local` / `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再
  通过 `SyncFloatLocal` 回写 value slot。未证明的 dynamic/generic operands 继续走
  `ZrLibrary_AotRuntime_GenericNumericAdd` fallback。
  RED/GREEN：RED 新增 mixed i64+f64 ADD smoke 后，generic numeric smoke 22 项中新增一个用例失败于缺少 mixed
  f64 local marker 和 direct cast-add 表达式。GREEN 通过 WSL GCC generic numeric smoke 22/0、generic numeric
  contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建
  focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 22、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-f64-add-local.md`。
  备注：proven mixed i64/f64 scalar-local generic ADD 已收口；mixed SUB/MUL/DIV/MOD、mixed u64/f64、mixed
  i64/u64 integer arithmetic、dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame
  cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 17:14:13 +08:00 · 07-S2/S4 generic numeric u64 NEG-to-i64 local fold ·
  状态：`GET_CONSTANT uint -> NEG -> RETURN` 的 proven u64 source / i64 result 折叠完成；07~12 总目标继续进行中。
  完成项目：generic numeric `NEG` 现在在 source 已证明为 u64 scalar local 且 destination 可保持 i64 local 时输出
  `zr_aot_generic_numeric_u64_neg_to_i64_scalar_local` 和 `zr_aot_s1 = -(TZrInt64)zr_aot_u0;`；该结果类型沿用
  `GenericNumericNeg` 的 unsigned 输入语义，即 signed i64，不是 u64 wraparound。对应 unsigned int 常量保留为
  `zr_aot_scalar_constant_u64_local`，目的 i64 结果不再通过 `SyncSignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericNeg` fallback。
  RED/GREEN：RED 新增 unsigned u64 NEG-to-i64 smoke 后，generic numeric smoke 21 项中新增一个用例失败于缺少 u64
  NEG-to-i64 local marker 和 direct cast-negate 表达式。GREEN 通过 WSL GCC generic numeric smoke 21/0、generic
  numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug
  构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 21、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-u64-neg-to-i64-local.md`。
  备注：proven u64 scalar-local generic unary NEG signed-result 形态已收口；mixed numeric、dynamic/unproven
  operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍待后续。

- 2026-07-05 16:52:54 +08:00 · 07-S2/S4 generic numeric u64 DIV/MOD local fold ·
  状态：`GET_CONSTANT uint -> GET_CONSTANT uint -> DIV/MOD -> RETURN` 的 proven u64 local 除法/取模折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `DIV` / `MOD` 现在在 destination/left/right 均已证明为 u64
  scalar local 时输出 `zr_aot_generic_numeric_u64_div_scalar_local` / `zr_aot_generic_numeric_u64_mod_scalar_local`、
  `if (zr_aot_u1 == (TZrUInt64)0u)` 零除保护、`"divide by zero"` / `"modulo by zero"` 错误，以及
  `zr_aot_u2 = zr_aot_u0 / zr_aot_u1;` / `zr_aot_u2 = zr_aot_u0 % zr_aot_u1;`；对应 unsigned int 常量保留为
  `zr_aot_scalar_constant_u64_local`，目的 u64 结果不再通过 `SyncUnsignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericDiv` / `GenericNumericMod` fallback。
  RED/GREEN：RED 新增 unsigned u64 DIV/MOD smoke 后，generic numeric smoke 20 项中新增两个用例失败于缺少 u64
  DIV/MOD local marker、zero guard 和 direct 表达式。GREEN 通过 WSL GCC generic numeric smoke 20/0、generic numeric
  contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建
  focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 20、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-u64-div-mod-local.md`。
  备注：proven u64 scalar-local generic ADD/SUB/MUL/DIV/MOD 已收口；mixed numeric、dynamic/unproven operands、
  更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed
  bodies 仍待后续。

- 2026-07-05 16:37:34 +08:00 · 07-S2/S4 generic numeric u64 SUB/MUL local fold ·
  状态：`GET_CONSTANT uint -> GET_CONSTANT uint -> SUB/MUL -> RETURN` 的 proven u64 local 减法/乘法折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `SUB` / `MUL` 现在在 destination/left/right 均已证明为 u64
  scalar local 时输出 `zr_aot_generic_numeric_u64_sub_scalar_local` / `zr_aot_generic_numeric_u64_mul_scalar_local` 和
  `zr_aot_u2 = zr_aot_u0 - zr_aot_u1;` / `zr_aot_u2 = zr_aot_u0 * zr_aot_u1;`；对应 unsigned int 常量保留为
  `zr_aot_scalar_constant_u64_local`，目的 u64 结果不再通过 `SyncUnsignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericSub` / `GenericNumericMul` fallback。
  RED/GREEN：RED 新增 unsigned u64 SUB/MUL smoke 后，generic numeric smoke 18 项中新增两个用例失败于缺少 u64
  SUB/MUL local marker 和 direct 表达式。GREEN 通过 WSL GCC generic numeric smoke 18/0、generic numeric contracts
  1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused
  targets 并通过 contracts，Unix-only smokes 为 0 failures / 18、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-u64-sub-mul-local.md`。
  备注：u64 DIV/MOD、mixed numeric、dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame
  cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 15:55:37 +08:00 · 07-S2/S4 generic numeric u64 ADD local fold ·
  状态：`GET_CONSTANT uint -> GET_CONSTANT uint -> ADD -> RETURN` 的 proven u64 local 加法折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `ADD` 现在在 destination/left/right 均已证明为 u64
  scalar local 时输出 `zr_aot_generic_numeric_u64_add_scalar_local` 和
  `zr_aot_u2 = zr_aot_u0 + zr_aot_u1;`；对应 unsigned int 常量保留为 `zr_aot_scalar_constant_u64_local`，
  目的 u64 结果不再通过 `SyncUnsignedIntLocal` 回写 value slot。未证明的 dynamic/generic operands 继续走
  `ZrLibrary_AotRuntime_GenericNumericAdd` fallback。
  RED/GREEN：RED 新增 unsigned u64 ADD smoke 后，generic numeric smoke 16 项中新增用例失败于缺少 u64 local
  marker 和 direct 表达式；首次实现暴露 f64 result stack-copy ADD 被误判为 u64 的回归，已用 candidateKind guard
  修复。GREEN 通过 WSL GCC generic numeric smoke 16/0、generic numeric contracts 1/0、source contracts 24/0 及
  power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets 并通过 contracts，
  Unix-only smokes 为 0 failures / 16、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-u64-add-local.md`。
  备注：u64 SUB/MUL/DIV/MOD、mixed numeric、dynamic/unproven operands、更广 value-copy migration、GC
  roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 08:46:37 +08:00 · 07-S2/S4 generic numeric i64 DIV/MOD local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT int -> DIV/MOD -> RETURN` 的 proven i64 local 除法/取模折叠完成；
  07~12 总目标继续进行中。完成项目：generic numeric `DIV` / `MOD` 现在在 destination/left/right 均已证明为 i64
  scalar local 时输出 `zr_aot_generic_numeric_i64_div_scalar_local` / `zr_aot_generic_numeric_i64_mod_scalar_local`，
  保留 `"divide by zero"` / `"modulo by zero"` 保护，并生成 `zr_aot_s2 = zr_aot_s0 / zr_aot_s1;` /
  `zr_aot_s2 = zr_aot_s0 % zr_aot_s1;`；目的 i64 结果不再通过 `SyncSignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `GenericNumericDiv` / `GenericNumericMod` fallback。
  RED/GREEN：RED 新增 signed i64 DIV/MOD smoke 后，generic numeric smoke 15 项中新增两个用例失败于缺少 i64
  DIV/MOD local marker、zero guard 和 direct 表达式。GREEN 通过 WSL GCC generic numeric smoke 15/0、generic
  numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug
  构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures / 15、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-i64-div-mod-local.md`。
  备注：u64/mixed numeric、dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、
  byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 08:25:36 +08:00 · 07-S2/S4 generic numeric i64 NEG local fold ·
  状态：`GET_CONSTANT int -> NEG -> RETURN` 的 proven i64 local 一元负号折叠完成；07~12 总目标继续进行中。
  完成项目：generic numeric `NEG` 现在在 source/destination 均已证明为 i64 scalar local 时输出
  `zr_aot_generic_numeric_i64_neg_scalar_local` 和 `zr_aot_s1 = -zr_aot_s0;`；对应 signed int 常量保留为
  `zr_aot_scalar_constant_i64_local`，目的 i64 结果不再通过 `SyncSignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericNeg` fallback。
  RED/GREEN：RED 新增 signed i64 NEG smoke 后，generic numeric smoke 13 项中新增用例失败于缺少 i64 NEG local
  marker 和 direct 表达式。GREEN 通过 WSL GCC generic numeric smoke 13/0、generic numeric contracts 1/0、source
  contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets 并通过
  contracts，Unix-only smokes 为 0 failures / 13、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-i64-neg-local.md`。
  备注：i64 DIV/MOD、u64/mixed numeric、dynamic/unproven operands、更广 value-copy migration、GC
  roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 08:01:28 +08:00 · 07-S2/S4 generic numeric i64 SUB/MUL local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT int -> SUB/MUL -> RETURN` 的 proven i64 local 减法/乘法折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic numeric `SUB` / `MUL` 现在在 destination/left/right 均已证明为 i64 scalar local 时输出
  `zr_aot_generic_numeric_i64_sub_scalar_local` / `zr_aot_generic_numeric_i64_mul_scalar_local` 和
  `zr_aot_s2 = zr_aot_s0 - zr_aot_s1;` / `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`；对应 signed int 常量保留为
  `zr_aot_scalar_constant_i64_local`，目的 i64 结果不再通过 `SyncSignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `GenericNumericSub` / `GenericNumericMul` fallback。
  RED/GREEN：RED 新增 signed i64 SUB/MUL smoke 后，generic numeric smoke 12 项中新增两个用例失败于缺少 i64
  SUB/MUL local marker 和 direct 表达式，generic numeric contract 也先因缺少 i64 SUB marker 失败。GREEN 通过 WSL
  GCC generic numeric smoke 12/0、generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality
  相邻回归；WSL Clang 同组通过；MSVC Debug 构建 focused targets 并通过 contracts，Unix-only smokes 为 0 failures /
  12、1、8、4 expected ignores。产出：
  `tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-i64-sub-mul-local.md`。
  备注：i64 DIV/MOD/NEG、u64/mixed numeric、dynamic/unproven operands、更广 value-copy migration、GC
  roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍待后续。

- 2026-07-05 07:31:18 +08:00 · 07-S2/S4 generic numeric i64 ADD local fold ·
  状态：`GET_CONSTANT int -> GET_CONSTANT int -> ADD -> RETURN` 的 proven i64 local 加法折叠完成；07~12 总目标继续进行中。
  完成项目：generic numeric `ADD` 现在在 destination/left/right 均已证明为 i64 scalar local 时输出
  `zr_aot_generic_numeric_i64_add_scalar_local` 和 `zr_aot_s2 = zr_aot_s0 + zr_aot_s1;`；对应 signed int 常量保留为
  `zr_aot_scalar_constant_i64_local`，目的 i64 结果不再通过 `SyncSignedIntLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericAdd` fallback。
  RED/GREEN：RED 新增 signed i64 ADD smoke 后，generic numeric smoke 10 项中新增用例失败于缺少 i64 ADD local marker，
  生成物仍调用 `GenericNumericAdd(state, &frame, 2, 0, 1)`。GREEN 通过 WSL GCC generic numeric smoke 10/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；
  MSVC Debug focused targets 构建通过，contracts 1/0、24/0、2/0 通过，Unix-only smoke/regression tests 0 failures
  且按预期 ignored；scoped `git diff --check` exit 0，仅既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-i64-add-local.md`。
  备注：仅关闭 proven i64 scalar-local generic ADD；i64 SUB/MUL/DIV/MOD/NEG、u64 generic numeric、mixed numeric、
  dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数
  和完整 zero-frame typed bodies 仍为 07 后续项。

- 2026-07-05 06:45:19 +08:00 · 07-S2/S4 generic numeric f64 NEG local fold ·
  状态：`GET_CONSTANT float -> NEG -> RETURN` 的 proven f64 local 一元负号折叠完成；07~12 总目标继续进行中。
  完成项目：generic numeric `NEG` 现在在 source/destination 均已证明为 f64 scalar local 时输出
  `zr_aot_generic_numeric_f64_neg_scalar_local` 和 `zr_aot_f1 = -zr_aot_f0;`，对应常量保留为
  `zr_aot_scalar_constant_f64_local`，并避免 `GenericNumericNeg`、unary boundary、`SyncFloatLocal`
  与 result value-slot 回写。
  RED/GREEN：RED 新增 NEG smoke 后，generic numeric smoke 8 项中新增用例失败于缺少 f64 NEG local marker，
  生成物仍调用 `GenericNumericNeg(state, &frame, 1, 0)`。GREEN 通过 WSL GCC generic numeric smoke 8/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；
  MSVC Debug focused targets 构建通过，contracts 1/0、24/0、2/0 通过，Unix-only smoke/regression tests
  0 failures 且按预期 ignored；scoped `git diff --check` exit 0，仅既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-f64-neg-local.md`。
  备注：仅关闭 proven f64 scalar-local generic NEG；dynamic/unproven operands、更广 value-copy migration、
  GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍为 07 后续项。

- 2026-07-05 06:05:04 +08:00 · 07-S2/S4 generic numeric f64 result stack-copy ADD local fold ·
  状态：prior generic numeric f64 result 经一次 stack-copy 后再参与 ADD 的本地折叠完成；07~12
  总目标继续进行中。
  完成项目：`DIV slot2 -> SET_STACK slot4 -> ADD slot5(4,3)` 现在能保持在 scalar locals 中，
  生成 C 包含 `zr_aot_f2 = zr_aot_f0 / zr_aot_f1;`、`zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2`、
  `zr_aot_f4 = zr_aot_f2;`、`zr_aot_generic_numeric_f64_add_scalar_local` 和
  `zr_aot_f5 = zr_aot_f4 + zr_aot_f3;`，并避免 `CopyStack`、`GenericNumericAdd`、`SyncFloatLocal`
  与 copied-result value-slot 物化。
  RED/GREEN：RED 新增 result-stack-copy ADD smoke 后，generic numeric smoke 7 项中新增用例失败于缺少
  f64 ADD local marker，生成物仍调用 `GenericNumericAdd(state, &frame, 5, 4, 3)`。GREEN 通过 WSL GCC
  generic numeric smoke 7/0、generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality
  相邻回归；WSL Clang 同组通过；MSVC Debug focused targets 构建通过，contracts 1/0、24/0、2/0 通过，
  Unix-only smoke/regression tests 0 failures 且按预期 ignored；scoped `git diff --check` exit 0，仅既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-f64-result-stack-copy-add-local.md`。
  备注：仅关闭一次 copied f64 generic numeric result 参与后续 ADD 的窄形态；right-side result copy、
  链式 copy、其他 copied arithmetic variants、dynamic/unproven operands、更广 value-copy migration、
  GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍为 07 后续项。

- 2026-07-05 05:17:00 +08:00 · 07-S2/S4 generic numeric f64 stack-copy DIV local fold ·
  状态：`GET_CONSTANT float -> SET_STACK -> GET_CONSTANT float -> DIV -> RETURN` 的 copied-f64 left operand
  除法本地折叠完成；07~12 总目标继续进行中。
  完成项目：stack-copy 目标槽现在能作为 generic numeric f64 operand 参与本地证明，生成 C 包含
  `zr_aot_scalar_stack_copy_f64 dstSlot=3 srcSlot=0`、`zr_aot_f3 = zr_aot_f0;`、
  `zr_aot_generic_numeric_f64_div_scalar_local` 和 `zr_aot_f2 = zr_aot_f3 / zr_aot_f1;`，并避免
  `CopyStack`、`GenericNumericDiv`、`SyncFloatLocal` 与 copied operand 的 value-slot 物化。
  RED/GREEN：RED 新增 stack-copy-left DIV smoke 后，generic numeric smoke 6 项中新增用例失败于缺少
  f64 DIV local marker，生成物仍调用 `GenericNumericDiv(state, &frame, 2, 3, 1)`。GREEN 通过 WSL GCC
  generic numeric smoke 6/0、generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality
  相邻回归；WSL Clang 同组通过；MSVC Debug focused targets 构建通过，contracts 1/0、24/0、2/0 通过，
  Unix-only smoke/regression tests 0 failures 且按预期 ignored；scoped `git diff --check` exit 0，仅既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-f64-stack-copy-div-local.md`。
  备注：仅关闭 left stack-copy copied-f64 generic DIV；right stack-copy、其他 copied arithmetic、dynamic/unproven
  operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-05 04:28:30 +08:00 · 07-S2/S4 generic numeric f64 DIV local fold ·
  状态：`GET_CONSTANT float -> GET_CONSTANT float -> DIV -> RETURN` 的 proven f64 local 除法折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic numeric `DIV` 现在在 destination/left/right 均已证明为 f64 scalar local 时输出
  `zr_aot_generic_numeric_f64_div_scalar_local`、本地除零检查和 `zr_aot_f2 = zr_aot_f0 / zr_aot_f1;`；
  对应常量保留为 `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再通过 `SyncFloatLocal`
  回写 value slot。未证明的 dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericDiv` fallback。
  RED/GREEN：RED 新增 DIV smoke 后，generic numeric smoke 5 项中只有 DIV 失败于缺少 f64 DIV local marker，
  生成物仍调用 `GenericNumericDiv`。GREEN 通过 WSL GCC generic numeric smoke 5/0、generic numeric
  contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；
  MSVC Debug focused smoke/contract/regression targets 构建通过，contracts 1/0、24/0、2/0 通过，
  Unix-only smoke/regression tests 0 failures 且按预期 ignored；scoped `git diff --check` exit 0，仅既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-f64-div-local.md`。
  备注：ADD/SUB/MUL/DIV/MOD 的 proven f64 scalar-local generic numeric 二元折叠已覆盖；dynamic/unproven
  operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-05 03:38:44 +08:00 · 07-S2/S4 generic numeric f64 SUB/MUL local fold ·
  状态：`GET_CONSTANT float -> GET_CONSTANT float -> SUB/MUL -> RETURN` 的 proven f64 local 减法/乘法折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic numeric `SUB` / `MUL` 现在在 destination/left/right 均已证明为 f64 scalar local 时输出
  `zr_aot_generic_numeric_f64_sub_scalar_local` / `zr_aot_generic_numeric_f64_mul_scalar_local` 和对应 `-` / `*`
  C 表达式；对应常量保留为 `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再通过 `SyncFloatLocal`
  回写 value slot。未证明的 dynamic/generic operands 继续走 runtime fallback。
  RED/GREEN：RED 新增 SUB/MUL smoke 后失败于缺少 f64 SUB/MUL local marker，生成物仍调用
  `GenericNumericSub` / `GenericNumericMul`。GREEN 通过 WSL GCC generic numeric smoke 4/0、generic numeric
  contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；
  MSVC Debug focused smoke/contract/regression targets 构建通过，contracts 1/0、24/0、2/0 通过，
  Unix-only smoke/regression tests 0 failures 且按预期 ignored；scoped `git diff --check` exit 0，仅既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-f64-sub-mul-local.md`。
  备注：generic DIV、本地化 dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、
  byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 13:10:12 +08:00 · 07-S2/S4 generic numeric f64 ADD local fold ·
  状态：`GET_CONSTANT float -> GET_CONSTANT float -> ADD -> RETURN` 的 proven f64 local 加法折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic numeric `ADD` 现在在 destination/left/right 均已证明为 f64 scalar local 时输出
  `zr_aot_generic_numeric_f64_add_scalar_local` 和 `zr_aot_f2 = zr_aot_f0 + zr_aot_f1;`；对应常量保留为
  `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再通过 `SyncFloatLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericAdd` fallback。
  RED/GREEN：RED 新增 ADD smoke 后失败于缺少 f64 ADD local marker，生成物仍调用
  `GenericNumericAdd(state, &frame, 2, 0, 1)`。GREEN 通过 WSL GCC generic numeric smoke 2/0、
  generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality 相邻回归；WSL Clang 同组通过；
  MSVC Debug focused smoke/contract/regression targets 构建通过，contracts 1/0、24/0、2/0 通过，
  Unix-only smoke/regression tests 0 failures 且按预期 ignored；scoped `git diff --check` exit 0，仅既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-numeric-f64-add-local.md`。
  备注：generic SUB/MUL/DIV、本地化 dynamic/unproven operands、更广 value-copy migration、
  GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整 zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 12:51:07 +08:00 · 07-S2/S4 generic numeric f64 MOD local fold ·
  状态：`GET_CONSTANT float -> GET_CONSTANT float -> MOD -> RETURN` 的 proven f64 local 取模折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic numeric `MOD` 现在在 destination/left/right 均已证明为 f64 scalar local 时输出
  `zr_aot_generic_numeric_f64_mod_scalar_local`、除零检查和 `fmod(zr_aot_f0, zr_aot_f1)`；对应常量保留为
  `zr_aot_scalar_constant_f64_local`，目的 f64 结果不再通过 `SyncFloatLocal` 回写 value slot。未证明的
  dynamic/generic operands 继续走 `ZrLibrary_AotRuntime_GenericNumericMod` fallback。
  RED/GREEN：RED 先失败于缺少 f64 scalar constant/local MOD marker；补齐常量声明后第二次 RED 暴露 MOD
  exec write 未被 scalar-locals 记录，导致仍生成 `GenericNumericMod` + `SyncFloatLocal`。GREEN 通过 WSL GCC
  generic numeric smoke 1/0、generic numeric contracts 1/0、source contracts 24/0 及 power/logical/equality
  相邻回归；WSL Clang 同组通过；MSVC Debug focused smoke/contract/regression targets 构建通过，contracts
  1/0、24/0、2/0 通过，Unix-only smoke/regression tests 0 failures 且按预期 ignored；scoped
  `git diff --check` exit 0，仅既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-numeric-f64-mod-local.md`。
  备注：更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 12:14:51 +08:00 · 07-S2/S4 generic reset-null direct LOGICAL_NOT local fold ·
  状态：`RESET_STACK_NULL -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` 的 direct reset-null 本地 bool
  折叠完成；07~12 总目标继续进行中。
  完成项目：新增 direct reset-null logical-not consumed-by-local 判定，并把 reset 发射、frame descriptor
  local-only 分类、generic `LOGICAL_NOT` lowering 串到同一窄证明。生成 C 输出
  `zr_aot_reset_stack_null_local_logical_not_skip`、`zr_aot_generic_logical_not_reset_null_local` 和
  `zr_aot_bD = ZR_TRUE;`，不再为该 direct 链路调用 targeted `ResetStackNull`、
  `GenericPrimitiveLogicalNot`、`SyncBoolLocal` 或读取 `frame.slotBase[0].value`；已有 reset-null
  stack-copy logical-not 覆盖保持不变。static numeric call smoke 同步收窄断言：typed direct-call deopt
  fallback sync 合法，stack-copy 边界同步和目的槽 materialization 仍禁止。
  RED/GREEN：RED 先失败于缺少 `zr_aot_reset_stack_null_local_logical_not_skip slot=0`；GREEN 通过
  WSL GCC generic LOGICAL_NOT 8/0、call shared-library 5/0、两个 stack-copy guardrail 1/0+1/0、
  logical contracts 4/0、generic JUMP_IF 9/0、generic equality stack-copy 4/0；WSL Clang generic
  LOGICAL_NOT 8/0、logical contracts 4/0、generic JUMP_IF 9/0、generic equality stack-copy 4/0；
  MSVC Debug 构建 focused targets，generic LOGICAL_NOT 0 failures / 8 expected ignores，logical contracts 4/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-reset-null-direct-logical-not-local.md`。
  备注：更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 11:26:29 +08:00 · 07-S2/S4 generic bool/mixed stack-copy equality local value-copy ·
  状态：bool stack-copy generic equality 本地化完成，并补齐 copied bool 与 numeric operand 的
  mixed primitive local fold；07~12 总目标继续进行中。
  完成项目：stack-copy generic equality consumer 现在把 BOOL 纳入 primitive copied-kind proof；
  函数体发射的 generic equality scalar-read proof 也从 numeric kind 扩展为 bool/i64/u64/f64
  primitive kind。生成 C 可直接发 `zr_aot_scalar_stack_copy_bool dstSlot=5 srcSlot=0`、
  `zr_aot_b5 = (TZrBool)(zr_aot_b0 != 0u);`、same-kind bool compare 或
  `zr_aot_generic_mixed_primitive_compare_scalar_local` / `leftKind=b rightKind=s` /
  `zr_aot_b2 = ZR_FALSE;`，跳过 `CopyStack`、`SZrTypeValue` destination materialization、
  generic equality helper、operand frame reads 和 bool sync。
  RED/GREEN：same-kind bool stack-copy equality guardrail 在现有实现上已 GREEN；bool/numeric RED
  为新增 mixed smoke 后 WSL GCC focused 失败 `Expected Non-NULL`，缺少 direct bool stack-copy
  assignment 与 mixed marker；GREEN 后目标项目无 targeted `GenericPrimitiveLogicalEqual`、
  `SyncBoolLocal`、`CopyStack` 或 `frame.slotBase[0/1/5].value`，运行返回 91。
  测试结果：WSL GCC 通过 stack-copy equality 4/0、generic equality 5/0、generic JUMP_IF 9/0、
  generic LOGICAL_NOT 8/0、logical contracts 4/0；WSL Clang 同组 4/0、5/0、9/0、8/0、4/0；
  Windows MSVC Debug 在 `build-msvc-aot-stack-copy` 通过构建，Unix-only smoke expected ignored
  且 0 failures，logical contracts 4/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-bool-mixed-stack-copy-equality-local.md`。
  备注：更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 11:12:07 +08:00 · 07-S2/S4 generic mixed primitive stack-copy equality local value-copy ·
  状态：`GET_CONSTANT i64 -> SET_STACK -> GET_CONSTANT u64 -> generic LOGICAL_EQUAL ->
  JUMP_IF_BOOL_FALSE` 的 mixed primitive stack-copy value-copy 本地化完成；07~12 总目标继续进行中。
  完成项目：stack-copy 目的槽 consumer 扫描现在允许 copied numeric primitive 与另一个已证明
  numeric primitive generic equality operand 不同 kind 时仍保留 copied scalar kind；函数体发射用
  numeric operand kind proof 识别 mixed primitive equality 已能完全读取 scalar operands，不再强制
  value-slot 写回。生成 C 直接发 `zr_aot_s5 = zr_aot_s0;`、
  `zr_aot_generic_mixed_primitive_compare_scalar_local`、`leftKind=s rightKind=u`、
  `zr_aot_b2 = ZR_FALSE;` 和 bool-local branch，跳过 `CopyStack`、`SZrTypeValue`
  destination materialization、generic equality helper、operand frame reads 和 bool sync。
  RED/GREEN：RED 为新增 mixed stack-copy equality smoke 后 WSL GCC focused 失败 `Expected Non-NULL`，
  缺少 direct stack-copy assignment 与 mixed marker；GREEN 后目标项目无 targeted
  `GenericPrimitiveLogicalEqual`、`SyncBoolLocal`、`CopyStack` 或 `frame.slotBase[0/1/5].value`，
  运行返回 91。
  测试结果：WSL GCC 通过 stack-copy equality 2/0、generic equality 5/0、generic JUMP_IF 9/0、
  generic LOGICAL_NOT 8/0、logical contracts 4/0；WSL Clang 同组 2/0、5/0、9/0、8/0、4/0；
  Windows MSVC Debug 在 `build-msvc-aot-stack-copy` 通过构建，Unix-only smoke expected ignored
  且 0 failures，logical contracts 4/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-mixed-stack-copy-equality-local.md`。
  备注：更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 11:00:12 +08:00 · 07-S2/S4 generic i64 stack-copy equality local value-copy ·
  状态：`GET_CONSTANT i64 -> SET_STACK -> generic LOGICAL_EQUAL -> JUMP_IF_BOOL_FALSE` 的
  stack-copy value-copy 本地化完成；07~12 总目标继续进行中。
  完成项目：stack-copy 目的槽 consumer 扫描现在用 `slotKinds` 保守识别同类 primitive generic
  equality，把 i64 copy destination 声明为 `zr_aot_s*`；函数体发射在后续 equality 已能读取
  scalar operands 时不再强制 value-slot 写回。生成 C 直接发 `zr_aot_s5 = zr_aot_s0;`、
  `zr_aot_generic_i64_compare_scalar_local` 和 bool-local branch，跳过 `CopyStack`、
  `SZrTypeValue` 目的槽 materialization、generic equality helper 和 bool sync。
  RED/GREEN：RED 为新增 stack-copy equality smoke 后 WSL GCC focused 失败 `Expected Non-NULL`，
  缺少 `zr_aot_s5 = zr_aot_s0;`，生成物仍写 `frame.slotBase[5].value`；GREEN 后目标项目无
  targeted `frame.slotBase[0/1/5].value`、`ZrCore_Stack_GetValue(frame.slotBase + 0/1/5)`、
  `CopyStack`、`GenericPrimitiveLogicalEqual` 或 `SyncBoolLocal`，运行返回 17。
  测试结果：WSL GCC 通过新增 stack-copy equality 1/0、generic equality 5/0、generic JUMP_IF
  9/0、generic LOGICAL_NOT 8/0、logical contracts 4/0；WSL Clang 同组 1/0、5/0、9/0、8/0、4/0；
  Windows MSVC Debug 在 `build-msvc-aot-stack-copy` 通过构建，Unix-only smoke expected ignored
  且 0 failures，logical contracts 4/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-i64-stack-copy-equality-local.md`。
  备注：更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 10:22:37 +08:00 · 07-S2/S4 generic mixed primitive equality local fold ·
  状态：当前可证明的 mixed primitive generic equality 本地折叠完成；07~12 总目标继续进行中。
  完成项目：generic logical lowering 在同类 bool/i64/u64/f64 local compare 后新增混合基础标量
  kind 判定，kind 不同时直接发 `zr_aot_bD = ZR_FALSE;` 或 `zr_aot_bD = ZR_TRUE;`，
  生成 `zr_aot_generic_mixed_primitive_compare_scalar_local` marker，跳过
  `GenericPrimitiveLogicalEqual/NotEqual`、bool sync 和 operand value-slot reread。
  scalar-local consumer 证明同步识别 mixed operands，并保留同类 operand 快速路径以避免 call-result
  equality 回归；frame descriptor 标记该形态 local-only。
  RED/GREEN：RED 为 mixed primitive equality smoke 和 logical source contract 缺少 mixed marker、
  direct bool assignment、helper 证明；GREEN 后追加修正同类 call-result equality slot 复用导致的
  direct-call sync 回归。
  测试结果：WSL GCC、WSL Clang 均通过 generic equality 5/0、logical contracts 4/0、generic JUMP_IF
  9/0、generic LOGICAL_NOT 8/0、logical shared-library 6/0、frame setup contracts 1/0、
  control contracts 2/0、control shared-library 2/0；Windows MSVC Debug 通过 generic equality
  0 failures / 5 expected ignores、generic JUMP_IF 0 failures / 9 expected ignores、
  generic LOGICAL_NOT 0 failures / 8 expected ignores、logical shared-library 0 failures /
  6 expected ignores、control shared-library 0 failures / 2 expected ignores，并通过
  logical/frame/control contracts 4/0、1/0、2/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-mixed-primitive-equality-local.md`。
  备注：value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 09:46:53 +08:00 · 07-S2/S4 generic dynamic object slot truthiness local fold ·
  状态：`TO_OBJECT -> JUMP_IF/LOGICAL_NOT` 的相邻 dynamic object slot 本地真值折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic logical lowering 新增相邻 `TO_OBJECT` 同槽证明与 object slot truthiness
  emitter；`JUMP_IF` 生成 `zr_aot_generic_jump_if_object_slot_local`，`LOGICAL_NOT` 生成
  `zr_aot_generic_logical_not_object_slot_local`，直接检查 object slot 为 null false、object true、
  其他类型 fail closed；`TO_OBJECT` 本身仍保留 runtime conversion。
  RED/GREEN：RED 为 dynamic object slot `JUMP_IF`/`LOGICAL_NOT` smoke 缺少 object slot marker、
  object/null 类型读取和直接 bool assignment，源码契约缺少 `TO_OBJECT` 相邻 helper；GREEN 后不再调用
  targeted `GenericPrimitiveIsTruthy` / `GenericPrimitiveLogicalNot` / bool sync。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF 9/0、generic LOGICAL_NOT 8/0、logical contracts
  4/0、logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、control contracts 2/0、
  control shared-library 2/0；Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 9 expected ignores、
  generic LOGICAL_NOT 0 failures / 8 expected ignores、logical shared-library 0 failures / 6 expected ignores、
  generic equality 0 failures / 4 expected ignores、control shared-library 0 failures / 2 expected ignores，
  并通过 logical/frame/control contracts 4/0、1/0、2/0；scoped `git diff --check` exit 0，仅有既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-dynamic-object-slot-truthiness-local.md`。
  备注：value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing、性能计数和完整
  zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 09:19:31 +08:00 · 07-S2/S4 generic dynamic string slot truthiness local fold ·
  状态：`TO_STRING -> JUMP_IF/LOGICAL_NOT` 的相邻 dynamic string slot 本地真值折叠完成；
  07~12 总目标继续进行中。
  完成项目：generic logical lowering 新增相邻 `TO_STRING` 同槽证明与 string slot truthiness emitter；
  `JUMP_IF` 生成 `zr_aot_generic_jump_if_string_slot_local`，`LOGICAL_NOT` 生成
  `zr_aot_generic_logical_not_string_slot_local`，直接检查 string slot type/object 并用
  `ZrCore_String_GetByteLength(...) > 0u` 求真值；`TO_STRING` 本身仍保留 runtime conversion。
  RED/GREEN：RED 为 dynamic string slot `JUMP_IF`/`LOGICAL_NOT` smoke 缺少 string slot marker
  与 length truthiness 读取，源码契约缺少 `TO_STRING` 相邻 helper；GREEN 后不再调用 targeted
  `GenericPrimitiveIsTruthy` / `GenericPrimitiveLogicalNot` / bool sync。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF 8/0、generic LOGICAL_NOT 7/0、logical contracts
  4/0、logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、control contracts 2/0、
  control shared-library 2/0；Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 8 expected ignores、
  generic LOGICAL_NOT 0 failures / 7 expected ignores、logical shared-library 0 failures / 6 expected ignores、
  generic equality 0 failures / 4 expected ignores、control shared-library 0 failures / 2 expected ignores，
  并通过 logical/frame/control contracts 4/0、1/0、2/0；scoped `git diff --check` exit 0，仅有既有
  LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-dynamic-string-slot-truthiness-local.md`。
  备注：一般 object truthiness、value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame
  narrowing、性能计数和完整 zero-frame typed bodies 仍为 07 后续项。

- 2026-07-04 08:47:05 +08:00 · 07-S2/S4 generic reset-null stack-copy truthiness local fold ·
  状态：`RESET_STACK_NULL -> SET_STACK/GET_STACK -> JUMP_IF/LOGICAL_NOT` 的相邻栈复制本地真值折叠完成；
  07~12 总目标继续进行中。
  完成项目：reset-null stack-copy consumed-by-local gate 覆盖 reset 发射、`SET_STACK`/`GET_STACK`
  copy skip、generic `JUMP_IF` false 直跳、generic `LOGICAL_NOT` 直接 `ZR_TRUE` bool 赋值，以及
  frame descriptor local-only 证明；普通 `RESET_STACK_NULL -> LOGICAL_NOT` runtime fallback 仍保留覆盖。
  RED/GREEN：RED 为 stack-copy reset-null `JUMP_IF`/`LOGICAL_NOT` smoke 缺少 stack-copy reset-null marker，
  源码契约缺少 helper；GREEN 后 reset-null 经栈复制后全部在本地折叠。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical contracts
  4/0、logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、control contracts 2/0、
  control shared-library 2/0；Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 7 expected ignores、
  generic LOGICAL_NOT 0 failures / 6 expected ignores、logical shared-library 0 failures / 6 expected ignores、
  generic equality 0 failures / 4 expected ignores、control shared-library 0 failures / 2 expected ignores，
  并通过 logical/frame/control contracts 4/0、1/0、2/0；`git diff --check` exit 0，仅有既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-reset-null-stack-copy-truthiness-local.md`。
  备注：动态 string slot 和一般 object truthiness 仍为 07 后续项。

- 2026-07-04 08:14:28 +08:00 · 07-S2/S4 generic null stack-copy truthiness local fold ·
  状态：`GET_CONSTANT null -> SET_STACK/GET_STACK -> JUMP_IF/LOGICAL_NOT` 的相邻栈复制本地真值折叠完成；
  07~12 总目标继续进行中。
  完成项目：null stack-copy consumed-by-local gate 覆盖常量发射、`SET_STACK`/`GET_STACK` copy skip、
  generic `JUMP_IF` false 直跳、generic `LOGICAL_NOT` 直接 `ZR_TRUE` bool 赋值，以及 frame descriptor local-only
  证明；目标项目禁止 `CopyConstant`、`CopyStack`、primitive truthiness runtime helper、bool sync 和相关 value-slot 访问。
  RED/GREEN：RED 为 stack-copy null `JUMP_IF`/`LOGICAL_NOT` smoke 缺少 stack-copy null marker，
  生成 C 仍经 value-slot/stack-copy/runtime truthiness；GREEN 后 null 经栈复制后全部在本地折叠。
  测试结果：WSL GCC 通过 generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical contracts 4/0、
  logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、control contracts 2/0、
  control shared-library 2/0；Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 7 expected ignores、
  generic LOGICAL_NOT 0 failures / 6 expected ignores、logical shared-library 0 failures / 6 expected ignores、
  generic equality 0 failures / 4 expected ignores、control shared-library 0 failures / 2 expected ignores，
  并通过 logical/frame/control contracts 4/0、1/0、2/0；`git diff --check` 退出码 0，仅报告 LF/CRLF 提示。
  WSL Clang 重跑被当前工作区无关
  type-inference 重复定义阻塞。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-null-stack-copy-truthiness-local.md`。
  备注：reset-null stack-copy、动态 string slot 和一般 object truthiness 仍为 07 后续项。

- 2026-07-04 07:41:29 +08:00 · 07-S2/S4 generic string stack-copy truthiness local fold ·
  状态：`GET_CONSTANT string -> SET_STACK/GET_STACK -> JUMP_IF/LOGICAL_NOT` 的相邻栈复制本地真值折叠完成；
  07~12 总目标继续进行中。
  完成项目：新增 `backend_aot_c_constant_consumers.c` 抽出 null/bool/string 常量消费者证明，并加入
  string stack-copy consumed-by-local gate；生成 C 对 stack-copy string `JUMP_IF` 输出 source-skip/copy-skip
  与 true/false 直接分支 marker，对 stack-copy string `LOGICAL_NOT` 输出 source-skip/copy-skip 与直接 bool
  赋值 marker，禁止目标项目中的 `CopyConstant`、`CopyStack`、primitive truthiness runtime helper 和 bool sync。
  RED/GREEN：RED 为 stack-copy string `JUMP_IF`/`LOGICAL_NOT` smoke 缺少 stack-copy string marker，
  生成 C 仍经 value-slot/stack-copy/runtime truthiness；GREEN 后空串/非空串经栈复制后全部在本地折叠。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical contracts
  4/0、logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、control contracts 2/0、
  control shared-library 2/0；Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 7 expected ignores、
  generic LOGICAL_NOT 0 failures / 6 expected ignores、logical shared-library 0 failures / 6 expected ignores、
  generic equality 0 failures / 4 expected ignores、control shared-library 0 failures / 2 expected ignores，
  并通过 logical/frame/control contracts 4/0、1/0、2/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-string-stack-copy-truthiness-local.md`。
  备注：动态字符串槽和一般 object truthiness 仍为 07 后续项。

- 2026-07-04 06:49:49 +08:00 · 07-S2/S4 generic string-constant truthiness local fold ·
  状态：立即 `GET_CONSTANT string -> JUMP_IF/LOGICAL_NOT` 的本地真值折叠完成；07~12 总目标继续进行中。
  完成项目：字符串常量空串/非空真值由 `backend_aot_c_string_constant_truthy()` 计算，常量发射、
  frame descriptor 和 generic logical lowering 共用 string constant consumed-by-local 判定；生成 C
  对 string `JUMP_IF` 输出 source-skip + true/false 直接分支 marker，对 string `LOGICAL_NOT`
  输出 source-skip + 直接 bool 赋值 marker，禁止目标项目中的 primitive truthiness runtime helper、
  temporary truthy bool、`CopyConstant` 源写入和 bool sync。
  RED/GREEN：RED 为 string `LOGICAL_NOT`/`JUMP_IF` smoke 缺少 string source-skip marker，生成 C 仍调用
  `GenericPrimitiveLogicalNot` / `GenericPrimitiveIsTruthy`；GREEN 后空串跳转、非空串 fallthrough、空串
  `!` 为 true、非空串 `!` 为 false 全部在本地生成。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical shared-library
  6/0、generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、control contracts 2/0；
  Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 7 expected ignores、generic LOGICAL_NOT
  0 failures / 6 expected ignores、logical shared-library 0 failures / 6 expected ignores、generic equality
  0 failures / 4 expected ignores，并通过 logical/frame/control contracts 4/0、1/0、2/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-string-constant-truthiness-local.md`。
  备注：动态字符串槽和一般 object truthiness 仍为 07 后续项。

- 2026-07-04 06:09:04 +08:00 · 07-S2/S4 generic LOGICAL_NOT bool stack-copy source local branch ·
  状态：`GET_CONSTANT bool -> SET_STACK -> LOGICAL_NOT` 的栈复制源本地 bool 路径完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_scalar_locals.c` 在 stack-copy destination consumer 反推中把 generic
  `LOGICAL_NOT` 按真值消费者收窄，避免目标槽被泛型 u64 消费路径误声明为 `zr_aot_u*`，同时后续
  `LOGICAL_NOT` 又读取未声明 `zr_aot_b*`。新增 shared-library smoke 和源码契约，要求 bool stack-copy、
  generic `LOGICAL_NOT`、typed bool branch 全部本地化，并禁止 `CopyStack`、generic runtime helper 和 bool sync。
  RED/GREEN：RED 生成 C 只有 `b0/b1/u5`，`SET_STACK` 走 `CopyStack`，但 `LOGICAL_NOT` 输出
  `!zr_aot_b5`；GREEN 后生成 `TZrBool zr_aot_b5`、`zr_aot_scalar_stack_copy_bool` 和
  `zr_aot_b1 = (TZrBool)(!zr_aot_b5);`，无 `zr_aot_u5`。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF 6/0、generic LOGICAL_NOT 5/0、
  logical shared-library 6/0、generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、
  control contracts 2/0；Windows MSVC Debug 通过 generic JUMP_IF 0 failures / 6 expected ignores、
  generic LOGICAL_NOT 0 failures / 5 expected ignores、logical shared-library 0 failures / 6 expected ignores、
  generic equality 0 failures / 4 expected ignores，并通过 logical/frame/control contracts 4/0、1/0、2/0。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-logical-not-bool-stack-copy-local.md`。

- 2026-07-04 05:23:13 +08:00 · 07-S2/S4 generic LOGICAL_NOT bool-constant local fold ·
  状态：`GET_CONSTANT bool -> LOGICAL_NOT` 的本地常量取反折叠完成；07~12 总目标继续进行中。
  完成项目：新增共享判定 `backend_aot_c_bool_constant_consumed_by_local_logical_not()`，常量发射、frame
  descriptor 和 generic `LOGICAL_NOT` 发射共用同一保守条件；bool 常量相邻消费时跳过源写入并生成
  `zr_aot_bool_constant_local_logical_not_source_skip`、`zr_aot_generic_logical_not_bool_constant_local` 和
  `zr_aot_bD = ZR_FALSE/ZR_TRUE;`。为保留普通 bool source 覆盖，shared-library fixture 改为
  `LOGICAL_NOT_BOOL -> LOGICAL_NOT` 并继续断言 `zr_aot_generic_logical_not_scalar_local`。
  RED/GREEN：RED 为新增 bool-constant `LOGICAL_NOT` shared-library 用例缺少 source-skip marker，契约测试缺少
  helper；GREEN 后生成 C 检查确认无 `zr_aot_scalar_constant_bool_local` /
  `GenericPrimitiveLogicalNot(state, &frame, 1, 0)` / `SyncBoolLocal(state, &frame, 1)` /
  `frame.slotBase[0].value`。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF smoke 6/0、generic LOGICAL_NOT 4/0、
  logical shared-library 6/0、generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、
  control contracts 2/0；Windows MSVC Debug 通过 focused JUMP_IF 0 failures / 6 expected ignores、
  focused LOGICAL_NOT 0 failures / 4 expected ignores、logical shared-library 0 failures / 6 expected ignores，
  并通过 logical/frame/control contracts 4/0、1/0、2/0。产出：
  `tests/acceptance/2026-07-04-aot-07-s2-s4-generic-logical-not-bool-constant-local.md`。

- 2026-07-04 04:51:23 +08:00 · 07-S2/S4 generic JUMP_IF bool-constant direct branch ·
  状态：`GET_CONSTANT bool -> JUMP_IF` 的常量分支折叠完成；07~12 总目标继续进行中。
  完成项目：新增共享判定 `backend_aot_c_bool_constant_consumed_by_local_jump_if()`，常量发射、frame
  descriptor 和 generic `JUMP_IF` 发射共用同一保守条件；bool 常量相邻消费时跳过
  `zr_aot_scalar_constant_bool_local` 写入，false 常量直接生成
  `zr_aot_bool_constant_local_jump_if_source_skip`、`zr_aot_generic_jump_if_bool_constant_false` 和目标
  `goto`，true 常量生成 `zr_aot_generic_jump_if_bool_constant_true` 后直接落下。为保留旧覆盖，
  bool scalar-local fixture 改为 `GET_CONSTANT -> SET_STACK -> JUMP_IF` 并继续断言 bool stack-copy + local branch。
  RED/GREEN：RED 为新增 bool-constant `JUMP_IF` shared-library 用例缺少 source-skip marker；GREEN 后生成 C
  检查确认无 `zr_aot_scalar_constant_bool_local` / `GenericPrimitiveIsTruthy` / `zr_aot_truthy` /
  `frame.slotBase[0].value`。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF smoke 6/0、generic LOGICAL_NOT 3/0、
  logical shared-library 6/0、generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、
  control contracts 2/0；Windows MSVC Debug 通过 focused 编译运行 0 failures / 6 expected ignores，并通过
  logical/frame/control contracts 4/0、1/0、2/0。产出：
  `tests/acceptance/2026-07-04-aot-07-s2-s4-generic-jump-if-bool-constant-local.md`。

- 2026-07-04 04:28:50 +08:00 · 07-S2/S4 generic JUMP_IF reset-null direct false branch ·
  状态：`RESET_STACK_NULL -> JUMP_IF` 的本地 false 分支折叠完成；07~12 总目标继续进行中。
  完成项目：新增共享判定 `backend_aot_c_reset_null_consumed_by_local_jump_if()`，reset 发射、frame descriptor
  和 generic `JUMP_IF` 发射共用同一保守条件；reset-null 相邻消费时跳过 runtime reset helper，直接生成
  `zr_aot_reset_null_local_jump_if_source_skip`、`zr_aot_generic_jump_if_reset_null_false` 和目标 `goto`。
  RED/GREEN：RED 为新增 reset-null `JUMP_IF` shared-library 用例缺少 source-skip marker；GREEN 后生成 C
  检查确认无 `ResetStackNull(state, &frame, 0)` / `GenericPrimitiveIsTruthy` / `zr_aot_truthy`。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF smoke 5/0、generic LOGICAL_NOT 3/0、
  logical shared-library 6/0、generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、
  control contracts 2/0；Windows MSVC Debug 通过 focused 编译运行 0 failures / 5 expected ignores，并通过
  logical/frame/control contracts 4/0、1/0、2/0。产出：
  `tests/acceptance/2026-07-04-aot-07-s2-s4-generic-jump-if-reset-null-local.md`。

- 2026-07-04 04:10:55 +08:00 · 07-S2/S4 generic JUMP_IF null-constant direct false branch ·
  状态：`GET_CONSTANT null -> JUMP_IF` 的本地 false 分支折叠完成；07~12 总目标继续进行中。
  完成项目：新增共享判定 `backend_aot_c_null_constant_consumed_by_local_jump_if()`，常量发射、frame
  descriptor 和 generic `JUMP_IF` 发射共用同一保守条件；null 常量相邻消费时跳过
  `frame.slotBase[0].value` 写入，直接生成 `zr_aot_generic_jump_if_null_constant_false` 和目标 `goto`。
  RED/GREEN：RED 为新增 null-constant `JUMP_IF` shared-library 用例缺少 source-skip marker；GREEN 后生成 C
  检查确认无 `frame.slotBase[0].value` / `GenericPrimitiveIsTruthy` / `zr_aot_truthy` / `ResetAsNull`。
  测试结果：WSL GCC、WSL Clang 均通过 generic JUMP_IF smoke 4/0、generic LOGICAL_NOT 3/0、
  logical shared-library 6/0、generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、
  control contracts 2/0；Windows MSVC Debug 通过 focused 编译运行 0 failures / 4 expected ignores，并通过
  logical/frame/control contracts 4/0、1/0、2/0。产出：
  `tests/acceptance/2026-07-04-aot-07-s2-s4-generic-jump-if-null-constant-local.md`。

- 2026-07-04 03:49:20 +08:00 · 07-S2/S4 generic LOGICAL_NOT null-constant local branch ·
  状态：`GET_CONSTANT null -> LOGICAL_NOT` 的本地 bool 折叠完成；07~12 总目标继续进行中。
  完成项目：新增共享判定 `backend_aot_c_null_constant_consumed_by_local_logical_not()`，常量发射、frame
  descriptor 和 generic `LOGICAL_NOT` 发射共用同一保守条件；null 常量相邻消费时跳过
  `frame.slotBase[0].value` 写入，直接生成 `zr_aot_b1 = ZR_TRUE;` 和本地 bool 分支；同时把 runtime
  fallback 覆盖迁移到 `RESET_STACK_NULL` 源，确保运行时 helper + `SyncBoolLocal` 边界仍被测试覆盖。
  RED/GREEN：RED 为新增 null-constant shared-library 用例缺少本地 marker；GREEN 后生成 C 检查确认
  null-constant 项目无 `frame.slotBase[0].value` / `GenericPrimitiveLogicalNot` / `SyncBoolLocal`，reset-null
  项目仍保留 runtime helper。测试结果：WSL GCC、WSL Clang 均通过 generic LOGICAL_NOT smoke 3/0、
  logical shared-library 6/0、generic JUMP_IF 3/0、generic equality 4/0、logical contracts 4/0；Windows
  MSVC Debug 通过 focused 编译运行 3/0/3 ignored 和 logical contracts 4/0。产出：
  `tests/acceptance/2026-07-04-aot-07-s2-s4-generic-logical-not-null-constant-local.md`。

- 2026-07-04 03:18:03 +08:00 · 07-S2/S4 generic LOGICAL_NOT runtime bool-result local branch reuse ·
  状态：generic `LOGICAL_NOT` runtime fallback 的 bool 结果本地分支复用完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_scalar_locals.c` 对 generic `LOGICAL_NOT` 的 bool 结果登记不再要求源槽已证明为
  bool local，只要目标槽声明为 bool local 即允许 runtime helper 后 `SyncBoolLocal` 写入 `zr_aot_b*`；新增
  null-source shared-library 覆盖，断言生成 C 保留 `GenericPrimitiveLogicalNot` runtime 边界和 bool sync，
  但后续 `JUMP_IF_BOOL_FALSE` 直接读取 `zr_aot_b1`，不再重建 condition pointer 或读取
  `frame.slotBase[1].value`。
  RED/GREEN：RED 为 runtime-source `LOGICAL_NOT` 缺少本地 branch marker；实现后将不受
  `GenericPrimitiveLogicalNot` 支持的字符串源修正为受支持且同样走 runtime fallback 的 null 源；GREEN 后
  focused generic LOGICAL_NOT smoke 2/0。
  测试结果：WSL GCC、WSL Clang 均通过 generic LOGICAL_NOT numeric/null smoke 2/0、logical shared-library
  smoke 6/0、generic JUMP_IF bool/numeric local 3/0、generic bool equality local 4/0；Windows MSVC Debug
  构建并运行 generic LOGICAL_NOT target，2 项 Unix-only shared-library 用例按预期 ignored 且 0 failures；
  生成物 marker 检查通过，`git diff --check` 仅有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-logical-not-runtime-bool-local.md`。
  备注：本切片不声明完整 07-S2/S4 或 07~12 完成；string/object truthiness、value-copy migration、
  GC roots/exports/frame cleanup、完整 zero-frame proof 和性能门槛仍待后续。
- 2026-07-04 02:52:14 +08:00 · 07-S2/S4 generic call-result equality local compare + u64 reset-null thunk ·
  状态：no-arg typed u64/f64 call result 的 generic equality local compare 完成；07~12 总目标继续进行中。
  完成项目：`tests/parser/test_aot_c_generic_bool_equality_local_smoke.c` 新增 call-result equality
  shared-library 覆盖，将 typed u64/f64 equality SemIR 重写为 generic `LOGICAL_EQUAL`/`LOGICAL_NOT_EQUAL`，
  并断言 u64/f64 call result 直接使用 scalar-local compare；`backend_aot_c_typed_u64_thunks.c` 接受
  `GET_CONSTANT` 后的非返回槽 `RESET_STACK_NULL`，使 reset-tail u64 constant-return helper 可发射
  `static TZrUInt64` no-arg thunk。
  RED/GREEN：RED 为新增 `generic_call_result_equality_local_project` 后缺少 u64 reset-tail typed thunk，
  生成物回退到 direct static call/sync；GREEN 后 generic bool equality local focused 4/0。
  测试结果：WSL GCC、WSL Clang 均通过 generic bool equality local 4/0、logical shared-library smoke 6/0、
  typed direct-call u64 25/0、call shared-library smoke 5/0；Windows MSVC Debug 构建并运行 generic bool
  equality local，4 项 Unix-only shared-library 用例按预期 ignored 且 0 failures；生成物 marker 检查通过，
  `git diff --check` 仅有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-04-aot-07-s2-s4-generic-call-result-equality-u64-reset-thunk.md`。
  备注：本切片不声明完整 07-S2/S4 或 07~12 完成；混合 primitive equality、dynamic/string/object truthiness、
  value-copy migration、GC roots/exports/frame cleanup、完整 zero-frame proof 和性能门槛仍待后续。
- 2026-07-04 02:22:10 +08:00 · 07-S2/S4 generic u64/f64 equality local compare ·
  状态：plain generic equality/inequality 的已证明 u64/f64-local 直接比较完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_lowering_generic_logical.c` 将 i64 fast path 泛化到 u64/f64，对左右 operand
  均为已写入同类 scalar local、目标为 bool scalar local 的 generic `LOGICAL_EQUAL`/`LOGICAL_NOT_EQUAL`
  发射 `zr_aot_b* = (TZrBool)((zr_aot_u* ==|!= zr_aot_u*) != 0u);` 或
  `zr_aot_b* = (TZrBool)((zr_aot_f* ==|!= zr_aot_f*) != 0u);`；`backend_aot_c_scalar_locals.c`
  把该形态纳入 u64/f64 local consumer、mention 和 bool-result write 证明；`backend_aot_c_lowering_values.c`
  补 unsigned 常量 local-only 早返回，避免 u64 常量在 local compare-only 场景写 `frame.slotBase`。
  RED/GREEN：RED 为新增 `generic_u64_f64_equality_local_project` 后 WSL GCC focused 先失败
  `Expected Non-NULL`，补 compare 后又因 `frame.slotBase[0].value` 失败 `Expected NULL`；GREEN 后
  generic bool/i64/u64/f64 equality focused 3/0。
  测试结果：WSL GCC、WSL Clang 均通过 generic bool equality local 3/0、logical shared-library smoke 6/0、
  shared-library smoke 13/0、generic LOGICAL_NOT numeric 1/0、generic JUMP_IF bool/numeric 3/0、
  logical contracts 4/0、source contracts 24/0、guardrail contracts 6/0、typed direct-call bool 28/0、
  typed direct-call u64 25/0、typed direct-call f64 19/0；Windows MSVC Debug 构建同组目标，
  logical/source/guardrail contracts 4/0、24/0、6/0 通过，Unix-only shared-library/direct-call 用例按预期
  ignored 且 0 failures。
  产出：`tests/acceptance/2026-07-03-aot-07-s2-s4-generic-u64-f64-equality-local-compare.md`。
  备注：本切片不声明完整 07-S2/S4 或 07~12 完成；混合 primitive equality、dynamic/string/object truthiness、
  value-copy migration、GC roots/exports/frame cleanup、完整 zero-frame proof 和性能门槛仍待后续。
- 2026-07-04 01:37:47 +08:00 · 07-S2/S4 generic i64 equality local compare ·
  状态：plain generic equality/inequality 的已证明 i64-local 直接比较完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_lowering_generic_logical.c` 对左右 operand 均为已写入 i64 scalar local、目标为 bool
  scalar local 的 generic `LOGICAL_EQUAL`/`LOGICAL_NOT_EQUAL` 发射 `zr_aot_b* = (TZrBool)((zr_aot_s* ==|!= zr_aot_s*) != 0u);`；
  `backend_aot_c_scalar_locals.c` 把该 plain generic equality 形态纳入 i64 local consumer、mention 和 bool-result write
  证明，动态/混合 operand 仍走 runtime helper。RED/GREEN：RED 为新增 `generic_i64_equality_local_project`
  后 WSL GCC focused 失败 `Expected Non-NULL`；GREEN 后 generic bool/i64 equality focused 2/0，并更新 logical shared-library
  断言以接受已证明 i64 的 `choose(true) != 4` 直接 local compare。
  测试结果：WSL GCC、WSL Clang 均通过 generic bool equality local 2/0、logical shared-library smoke 6/0、
  shared-library smoke 13/0、generic LOGICAL_NOT numeric 1/0、generic JUMP_IF bool/numeric 3/0、
  logical contracts 4/0、source contracts 24/0、guardrail contracts 6/0、typed direct-call bool 28/0、
  typed direct-call u64 25/0、typed direct-call f64 19/0；Windows MSVC Debug 构建同组目标，
  logical/source/guardrail contracts 4/0、24/0、6/0 通过，Unix-only shared-library/direct-call 用例按预期 ignored
  且 0 failures。
  产出：`tests/acceptance/2026-07-03-aot-07-s2-s4-generic-i64-equality-local-compare.md`。
  备注：本切片不声明完整 07-S2/S4 或 07~12 完成；混合 primitive equality、dynamic/string/object truthiness、
  value-copy migration、GC roots/exports/frame cleanup、完整 zero-frame proof 和性能门槛仍待后续。
- 2026-07-04 01:04:05 +08:00 · 07-S2/S4 generic equality bool-result branch local reuse ·
  状态：generic equality runtime-boundary bool result 后续 branch 的 frame reread 局部优化完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_scalar_locals.c` 现在把目标槽声明为 bool local 的 generic
  `LOGICAL_EQUAL`/`LOGICAL_NOT_EQUAL` 记录为 bool write，即使 operand 仍必须走 runtime equality helper；
  generated C 保留 `GenericPrimitiveLogicalEqual/NotEqual` 与 `SyncBoolLocal` 边界，但后续
  `JUMP_IF_BOOL_FALSE` 改读 `zr_aot_b9`，不再生成 `zr_aot_condition = &frame.slotBase[9].value;`。
  RED/GREEN：RED 为 logical shared-library smoke 新增 `if (!zr_aot_b9)` 断言后失败
  `Expected Non-NULL`；GREEN 后 focused 6/0，并确认无 unused-parameter warning。
  测试结果：WSL GCC、WSL Clang 均通过 logical shared-library smoke 6/0、shared-library smoke 13/0、
  generic LOGICAL_NOT numeric 1/0、generic JUMP_IF bool/numeric 3/0、source contracts 24/0、
  guardrail contracts 6/0、typed direct-call bool 28/0、typed direct-call u64 25/0、typed direct-call f64 19/0；
  Windows MSVC Debug 构建同组目标，source/guardrail contracts 24/0 与 6/0 通过，Unix-only
  shared-library/direct-call 用例按预期 ignored 且 0 failures。
  产出：`tests/acceptance/2026-07-03-aot-07-s2-s4-generic-equality-bool-branch-local.md`。
  备注：本切片不声明完整 07-S2/S4 或 07~12 完成；dynamic equality operand materialization、value-copy migration、
  GC roots/exports/frame cleanup、完整 zero-frame proof 和性能门槛仍待后续。
- 2026-07-04 00:22:33 +08:00 · 07-S2/S4/S5 u64-f64 call-result truthiness direct-call ·
  状态：no-arg typed u64/f64 call result 的 generic truthiness/direct-call 局部优化完成；07~12 总目标继续进行中。
  完成项目：`backend_aot_c_scalar_locals.c` 的 per-exec written-before 以 typed callee 返回 kind 为准，避免 u64/f64
  call result 被 stale 槽 kind 读错；global declaration 仍用 OR 保留槽复用声明；`backend_aot_c_typed_f64_thunk_shapes.c`
  接受 f64 constant-return 后跟 non-return `RESET_STACK_NULL` 的 thunk 形状。RED/GREEN：RED 为 logical shared-library
  smoke 缺少 f64 no-arg direct-call marker，且 u64/f64 truthiness 读错 stale scalar/local sync；GREEN 后 focused 6/0。
  测试结果：WSL GCC、WSL Clang 均通过 logical shared-library smoke 6/0、generic LOGICAL_NOT numeric 1/0、generic JUMP_IF
  bool/numeric 3/0、source contracts 24/0、guardrail contracts 6/0、typed direct-call f64 19/0、typed direct-call u64 25/0；
  Windows MSVC Debug 构建同组目标，source/guardrail contracts 24/0 与 6/0 通过，Unix-only shared-library/direct-call
  用例按预期 ignored 且 0 failures。
  产出：`tests/acceptance/2026-07-03-aot-07-s2-s4-s5-u64-f64-call-result-truthiness-direct-call.md`。
  备注：本切片不声明完整 07-S2/S4/S5 或 07~12 完成；dynamic/string/object truthiness、value-copy migration、
  GC roots/exports/frame cleanup、完整 zero-frame proof 和性能门槛仍待后续。
- 2026-07-03 23:36:13 +08:00 · 11-S7ZTB / 12-S7ZZZU CLI export duplicate bridge guard ·
  状态：CLI writer bridge 的内存态 export declaration 重复 key 拒绝完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`compiler_aot_exports.c` 在 project export declarations 进入 writer manifest options 前拒绝重复
  `kind + target`，失败时释放 scratch storage 并保持 writer options 为空。
  RED/GREEN：RED 为新增 CLI writer-options 负例失败 `Expected FALSE Was TRUE`；GREEN 后 focused direct run 19/0。
  测试结果：WSL GCC、WSL Clang、Windows MSVC Debug 均通过 CLI writer-options 19/0、project manifest normalization 29/0、
  manifest export/runtime view CTest 2/2、source contracts 24/0；代码文件 diff/尾随空白检查通过（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-07-03-aot-11-s7ztb-12-s7zzzu-cli-export-duplicate-bridge-guard.md`。
  备注：本切片只关闭 bridge 层 export declaration duplicate guard；不声明完整 11-S7、12-S7 或 07~12 总目标完成。
- 2026-07-03 23:20:20 +08:00 · 11-S7ZTA / 12-S7ZZZT project export schema uniqueItems parity ·
  状态：`.zrp` schema export declaration 基础重复对象约束完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`zrp.schema.json` 的 `exports` array 增加 `uniqueItems: true`，让完全重复 export declaration
  object 在 schema 层先被拒绝。
  RED/GREEN：RED 为 schema 断言缺少 `uniqueItems` 失败 `AssertionError`；GREEN 后断言通过。
  测试结果：`python -c` schema uniqueItems 断言通过；`python -m json.tool zrp.schema.json` 通过。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zta-12-s7zzzt-project-export-schema-unique-items.md`。
  备注：本切片只关闭 schema 层完全重复 export object parity；不声明完整 11-S7、12-S7 或 07~12 总目标完成。
- 2026-07-03 23:14:39 +08:00 · 11-S7ZSZ / 12-S7ZZZS project manifest export duplicate target guard ·
  状态：项目 manifest `exports` 重复目标拒绝完成；07~12 总目标继续进行中，完整 metadata sweep/pruning、
  完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`library_project_parse_export_declarations()` 拒绝重复 `kind + target`，避免项目声明层的歧义
  export key 流入 CLI writer bridge、generated manifest export table 和 runtime view。
  RED/GREEN：RED 为新增重复 `method`/`Widget.run` manifest fixture 后 WSL GCC 失败 `Expected NULL`；
  GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC focused project direct 29/0；WSL GCC 下游 CTest
  `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports` 2/2 与 source contracts 24/0 通过；
  WSL clang/MSVC Debug 同组 project direct 29/0、下游 CTest 2/2、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsz-12-s7zzzs-project-export-duplicate-target-guard.md`。
  备注：本切片只关闭 project manifest export duplicate kind+target parser guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。
- 2026-07-03 23:00:31 +08:00 · 11-S7ZSY / 12-S7ZZZR manifest export duplicate kind-target guard ·
  状态：generated manifest export table 重复目标拒绝完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_manifest_export_table_build()` 在写入前拒绝重复 `kind + target`，让生成端与
  `ZrCore_MetadataRuntime_ReadManifestExportView()` 的运行时歧义拒绝语义一致。
  RED/GREEN：RED 为新增 duplicate `METHOD + "Factory.make"` manifest export fixture 后 WSL GCC 失败
  `Expected FALSE Was TRUE`；GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports|aot_c_zrp_metadata_pruning`
  均 3/3；同三套直跑 source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsy-12-s7zzzr-manifest-export-duplicate-target-guard.md`。
  备注：本切片只关闭 generated manifest export duplicate kind+target guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。
- 2026-07-03 22:45:34 +08:00 · 11-S7ZSX / 12-S7ZZZQ writer sidecar definition-table validation ·
  状态：writer-level compacted `.zrp` metadata sidecar 文件级校验完成；07~12 总目标继续进行中，完整
  metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_publish_compacted_zrp_metadata()` 写 sidecar 前校验 header 与 definition tables；
  direct writer 路径遇到无效 definition-table blob 时 fail closed，并删除同路径旧 sidecar。
  RED/GREEN：RED 为新增 direct writer invalid definition-table sidecar fixture 后 WSL GCC 失败
  `Expected FALSE Was TRUE`；GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_publication|cli_aot_compacted_metadata_sidecar|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsx-12-s7zzzq-zrp-sidecar-definition-table-validation.md`。
  备注：本切片只关闭 sidecar publication validation/stale cleanup 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。
- 2026-07-03 22:23:50 +08:00 · 11-S7ZSW / 12-S7ZZZP retained signature UNION string-pool sweep/remap ·
  状态：emitted `.zrp` metadata pruning 中 retained signature `UNION` base-name string 保留与重写完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：retained signature string-pool sweep 现在把 `UNION` base-name offset 当作 compacted string root；
  retained signature rewrite 在 copy 后用 compacted string remap 改写该 offset，覆盖 MethodSpec 泛型实参中的 union signature。
  RED/GREEN：RED 为新增 MethodSpec union fixture 后 WSL GCC 失败 `Expected 780 Was 773`；GREEN 后
  WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_typespec_pruning|aot_c_zrp_metadata_module_ref_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 TypeDef pruning 2/0、source contracts 24/0；`git diff --check` 仅有 LF/CRLF 提示，尾随空白扫描干净。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsw-12-s7zzzp-signature-union-string-remap.md`。
  备注：本切片只关闭 retained signature `UNION` base-name string retention/remap 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。
- 2026-07-03 22:04:51 +08:00 · 11-S7ZSV / 12-S7ZZZO ModuleRef retained-row generic context ·
  状态：emitted `.zrp` metadata pruning 中 ModuleRef retained-row generic context 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：ModuleRef retained/count/compact/remap API 现在携带 GenericParam 与 GenericParamConstraint context；
  retained import token record 的 TypeSpec target 可通过泛型约束保活，从而保留并压缩对应 AssemblyRef/ModuleRef row。
  RED/GREEN：RED 为新增 ModuleRef pruning fixture 后 WSL GCC 失败 `Expected 1 Was 0`；GREEN 后
  WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_typespec_pruning|aot_c_zrp_metadata_module_ref_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 TypeDef pruning 2/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsv-12-s7zzzo-module-ref-generic-context.md`。
  备注：本切片只关闭 ModuleRef retained-row generic context 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 21:20:45 +08:00 · 11-S7ZSU / 12-S7ZZZN retained signature MODULE string-pool sweep/remap ·
  状态：emitted `.zrp` metadata pruning 中 retained signature `MODULE` name/version string retention/remap 完成；
  07~12 总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：string-pool remap 现在扫描 retained signature blobs，把 `MODULE(nameStringOffset, versionStringOffset)` 的
  module name/version strings 作为 roots；signature rewrite 会在 copied signature blob 中把两个 payload 改为 compacted
  string-pool offsets。
  RED/GREEN：RED 为 MethodSpec pruning 新增 `MODULE("__entry","1.0.0")` retained signature fixture，WSL GCC 失败
  `Expected 778 Was 764`；GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug direct MethodSpec pruning 7/0、pool pruning 8/0、metadata pruning 22/0、
  TypeDef pruning 2/0、TypeSpec pruning 2/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsu-12-s7zzzn-signature-module-string-remap.md`。
  备注：本切片只关闭 retained signature `MODULE` string-pool sweep/remap 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 20:50:52 +08:00 · 11-S7ZST / 12-S7ZZZM retained signature TYPE_REF string-pool sweep/remap ·
  状态：emitted `.zrp` metadata pruning 中 retained signature `TYPE_REF` name string retention/remap 完成；
  07~12 总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：string-pool remap 现在扫描 retained signature blobs，把 `TYPE_REF(baseType, nameStringOffset)` 的
  name string offset 纳入 compacted string pool roots；retained signature rewrite context 同步携带 string remap，
  在 copy 后把 `TYPE_REF` payload 中的旧 string offset 写回为 compacted offset。string-pool remap 支持按需扩容，
  避免签名内新增 string roots 超出按行表估算容量。本切片不改变 `TYPE_DEF` signature node 的既有 token rewrite 语义。
  RED/GREEN：RED 为新增 MethodSpec 泛型实参 `TYPE_REF("ExternalArg")` fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 776 Was 764`，证明旧逻辑没有保留只被 retained signature 引用的 string slice。GREEN 后 MethodSpec pruning 6/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、pool pruning 8/0、
  pruning 22/0、TypeDef pruning 2/0、TypeSpec pruning 2/0、MethodSpec pruning 6/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zst-12-s7zzzm-signature-typeref-string-remap.md`。
  备注：本切片只关闭 retained signature `TYPE_REF` string-pool sweep/remap 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 20:26:48 +08:00 · 11-S7ZSS / 12-S7ZZZL MethodSpec imported MEMBER_REF recursive retained-token-record guard ·
  状态：emitted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` recursive retained-token-record guard 完成；
  07~12 总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_method_spec_row()` 和 retained token-record pruning 现在会递归检查 imported
  `MEMBER_REF` token record 的 nested `MEMBER_REF` 引用链；若 `METHOD_SPEC.methodToken = MEMBER_REF(1)` 的 token
  record 通过 `MEMBER_REF(2)` 间接指向已裁剪 MethodDef/FieldDef，则 MethodSpec row、`SIGNATURE` token record、
  signature blob 和 dangling imported member records 会一起被裁掉。直接自引用 imported member record 继续合法，递归深度以
  token-record 数量封顶。
  RED/GREEN：RED 为新增 nested imported `MEMBER_REF` 指向 pruned MethodDef 的 fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 504 Was 639`，证明旧逻辑只检查第一层 record。GREEN 后 MethodSpec pruning 5/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 5/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zss-12-s7zzzl-methodspec-memberref-recursive-retained-token-record-guard.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` recursive retained-token-record 完整性缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 19:54:49 +08:00 · 11-S7ZSR / 12-S7ZZZK MethodSpec imported MEMBER_REF retained-token-record guard ·
  状态：emitted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` retained-token-record guard 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_method_spec_row()` 现在不只要求 imported `MEMBER_REF` token record 存在，
  还会验证该 token record 的 member-token 字段仍能通过 retained MethodDef/FieldDef remap；retained token-record
  pruning 对嵌套 `MEMBER_REF` 引用执行相同 retained-record 检查。MethodSpec count/copy、signature blob remap 和
  MethodSpec signature rewrite 已同步携带 TypeDef/GenericParam/constraint context，避免带 pruned target 的 imported
  member record 把 MethodSpec、`SIGNATURE` token record 或 signature blob 留在 compacted `.zrp`。
  RED/GREEN：RED 为新增 imported `MEMBER_REF` record 指向已裁剪 MethodDef 的 fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 600 Was 639`，证明旧逻辑只检查 record 存在性。GREEN 后修正 retained-record 判定并校准期望长度，
  MethodSpec pruning 测试 3/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 3/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsr-12-s7zzzk-methodspec-memberref-retained-token-record-guard.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` retained-token-record 完整性缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 19:29:47 +08:00 · 11-S7ZSQ / 12-S7ZZZJ MethodSpec imported MEMBER_REF token-record guard ·
  状态：emitted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` token-record guard 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_method_spec_row()` 不再仅凭 `MEMBER_REF` table tag 保留
  `METHOD_SPEC.methodToken`，而是要求源 token record 表中存在对应 imported member record；token-record pruning
  对 `token` / `relatedToken` / `ownerToken` / `targetMetadataToken` / `targetSignatureToken` 中的 `MEMBER_REF`
  引用也执行同一存在性 guard。signature remap、MethodSpec count/copy 和 MethodSpec signature rewrite 已同步携带
  token-record context，避免 orphan MethodSpec row、`SIGNATURE` token record 和 signature blob 留在 compacted `.zrp`。
  RED/GREEN：RED 为新增缺失 imported `MEMBER_REF` token record fixture 后 WSL GCC MethodSpec pruning 失败
  `Expected 504 Was 639`，证明旧逻辑保留了孤儿 MethodSpec/signature。GREEN 后新 MethodSpec pruning 测试 2/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 2/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsq-12-s7zzzj-methodspec-memberref-token-record-guard.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` token-record 完整性缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 19:00:53 +08:00 · 11-S7ZSP / 12-S7ZZZI MethodSpec imported MEMBER_REF method-token retention ·
  状态：emitted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` method-token retention 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_method_spec_row()` 现在区分 `METHOD_SPEC.methodToken` 的本地
  `MEMBER_DEF` 和导入 `MEMBER_REF`。本地 `MEMBER_DEF` 仍通过 retained MethodDef remap 校验并压缩 RID；
  导入 `MEMBER_REF` 作为合法 method reference 原样保留，避免 MethodDef pruning 误删跨模块泛型方法实例。
  新增独立 `zr_vm_aot_c_zrp_metadata_methodspec_pruning_test`，避免继续扩大综合 pruning 测试文件。
  RED/GREEN：RED 为临时撤掉 `MEMBER_REF` 分支后新 fixture 失败 `Expected 735 Was 711`，证明旧逻辑在裁掉
  一个本地 MethodDef 时同时误丢 imported MethodSpec row。GREEN 后新 MethodSpec pruning 测试 1/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 1/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsp-12-s7zzzi-methodspec-memberref-retention.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` method-token 保留缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 08:13:00 +08:00 · 11-S7ZSO / 12-S7ZZZH GenericParam TypeDef owner retained-row guard ·
  状态：emitted `.zrp` metadata pruning 中 GenericParam TypeDef-owner retained-row refinement 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_generic_param_owner_token()` 的 `TYPE_DEF` owner 分支不再直接接受任意
  TypeDef token，而是通过 retained TypeDef remap 校验并压缩 RID；GenericParam retention/count/range 与
  GenericParamConstraint remap/count/range 现在贯穿 TypeDef/token-record/constraint context。TypeDef retention
  也不再允许 TypeDef-owned GenericParam 反向作为 owner TypeDef 的独立保活 root，dead TypeDef 的泛型参数会被剪掉。
  RED/GREEN：RED 为新增 pruned TypeDef-owned GenericParam fixture 后 WSL GCC pruning 失败 `Expected Non-NULL`，
  旧逻辑把 TypeDef-owned GenericParam 当成保活依据或在 copy 阶段 fail-closed。GREEN 后 WSL GCC/clang/Windows
  MSVC Debug source contracts 均 24/0，export token remap 均 10/0，pruning 均 22/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zso-12-s7zzzh-genericparam-typedef-owner-retention.md`。
  备注：本切片只关闭 GenericParam TypeDef owner retained-row/remap 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 07:42:47 +08:00 · 11-S7ZSN / 12-S7ZZZG TypeDef retained FieldDef owner-token guard ·
  状态：emitted `.zrp` metadata pruning 中 TypeDef token-record retained-root refinement 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：TypeDef retention 的 token-record root 检查不再使用 legacy member-token remap；`MEMBER_DEF`
  MethodDef roots 必须是 retained MethodDef rows，FieldDef roots 必须先通过 retained FieldDef row 判定，且不能用
  该 FieldDef 反向保活自己的 owner TypeDef。`backend_aot_c_zrp_type_def_row_is_retained()` 现在接收 TypeDef row
  context，signature/string/type-def remap paths 已贯穿该 context，避免 pruned FieldDef owner/target token record
  把 dead TypeDef 留下。
  RED/GREEN：RED 为新增 dead FieldDef owner-TypeDef token-record fixture 后 WSL GCC pruning 失败
  `Expected Non-NULL`，旧逻辑把只被 pruned FieldDef token record 引用的 TypeDef/FieldDef 留在原 blob 中。
  GREEN 后 WSL GCC/clang/Windows MSVC Debug source contracts 均 24/0，export token remap 均 10/0，pruning 均 21/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsn-12-s7zzzg-typedef-fielddef-owner-token-retention.md`。
  备注：本切片只关闭 TypeDef retained FieldDef owner-token root guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 07:22:01 +08:00 · 11-S7ZSM / 12-S7ZZZF Method-only member-token guard ·
  状态：emitted `.zrp` metadata pruning 中 GenericParam/MethodSpec method-only member-token refinement 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_method_spec_row()` 和
  `backend_aot_c_zrp_remap_generic_param_owner_token()` 的 `MEMBER_DEF` method-only 分支现在只接受 retained
  MethodDef row；FieldDef `MEMBER_DEF` token 仍可作为普通 token-record/FieldDef row 被保留和压缩，但不能冒充
  GenericParam owner 或 MethodSpec method。Malformed MethodSpec/GenericParam rows 会被剪掉，MethodSpec signature blob
  不再因 FieldDef method-token 假根进入输出。
  RED/GREEN：RED 为新增 FieldDef-as-method-owner fixture 后 WSL GCC pruning 失败 `Expected TRUE Was FALSE`，
  旧逻辑把 retained FieldDef 当作 MethodSpec methodToken 接受，随后因缺少 retained signature token record 使
  metadata prepare 失败。GREEN 后 WSL GCC/clang/Windows MSVC Debug source contracts 均 24/0，export token remap 均
  10/0，pruning 均 20/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsm-12-s7zzzf-method-only-member-token-retention.md`。
  备注：本切片只关闭 GenericParam/MethodSpec MethodDef-only member-token guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 06:53:16 +08:00 · 11-S7ZSL / 12-S7ZZZE TypeSpec retained FieldDef token-record guard ·
  状态：emitted `.zrp` metadata pruning 中 TypeSpec retained-token refinement 完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：TypeSpec row retention 现在使用 retained token-record wrapper，要求 TypeSpec-rooting records 中的
  `MEMBER_DEF` FieldDef owners/targets 先通过 owner TypeDef retention 下的 FieldDef 存活检查；TypeDef context 已贯穿
  TypeSpec/moduleRef/signature/string-pool remap paths；dead FieldDef-rooted TypeSpecs 与 signature blobs 会被裁剪，
  live FieldDef-rooted TypeSpecs 会压缩到 retained `TYPE_SPEC` RID 并重映射 signature offsets/hashes。
  RED/GREEN：RED 为新增 TypeSpec FieldDef-token-record fixture 后 WSL GCC pruning 失败 `Expected 765 Was 794`，
  旧 legacy member-token remap 保留了只通过 pruned FieldDef 建根的 TypeSpec。GREEN 后 WSL GCC/clang/Windows
  MSVC Debug source contracts 均 24/0，export token remap 均 10/0，pruning 均 19/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsl-12-s7zzze-typespec-fielddef-retention.md`。
  备注：本切片只关闭 TypeSpec retained FieldDef token-record guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 06:24:32 +08:00 · 11-S7ZSK / 12-S7ZZZD FieldDef member-token retained-row guard ·
  状态：emitted `.zrp` metadata pruning 中 FieldDef `MEMBER_DEF` token 保留/压缩保护子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：token-record pruning 与 persistent manifest export member-token remap 现在使用 retained-row aware wrapper；
  FieldDef token remap 先验证 FieldDef row 仍由 retained owner TypeDef 保活，再通过 retained FieldDef 顺序压缩到方法 RID 之后；
  pruned FieldDef token record 会被丢弃，位于 pruned FieldDef 之后的 live FieldDef 会获得正确 compacted `MEMBER_DEF` RID。
  RED/GREEN：RED 为新增 FieldDef-before-live fixture 后 WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 失败
  `Expected 640 Was 736`，旧逻辑保留了 dead FieldDef token record 并按 source field index 压缩；GREEN 后 WSL GCC/clang/Windows
  MSVC Debug pruning 均 18/0，source contracts 均 24/0，export token remap 均 10/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsk-12-s7zzzd-fielddef-token-retention.md`。
  备注：本切片只关闭 FieldDef member-token retained-row guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 05:50:58 +08:00 · 11-S7ZSC / 12-S7ZZT manifest export target-only string retention ·
  状态：persistent manifest export pruning 的 target-string retention refinement 完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`.zrp` string-pool remap 现在把 retained persistent `manifestExports[].targetStringOffset` 作为字符串保活 root；
  manifest export 模块新增 row-retention predicate 复用既有 type/member token compactability 判定，string-pool remap 只保留最终会存活的 export target string；
  prune orchestration 同步为 manifest export target strings 预留 remap 容量，persistent manifest export copy path 继续负责 target/type/member token rewrite。
  RED/GREEN：RED 为新增 target-only string fixture 后 WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 17 项中 1 项失败，
  `backend_aot_c_prepare_embedded_zrp_metadata` 返回 false，因为旧 string-pool remap 没有收集只被 manifest export target 引用的 `"unused"` 字符串。
  GREEN 后 WSL GCC/clang/Windows MSVC Debug zrp metadata pruning 均 17/0，source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsc-12-s7zzt-manifest-export-target-string-retention.md`。
  备注：本切片只关闭 retained manifest export target-only string 的 remap/compaction 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 05:30:18 +08:00 · 10-S5P / 12-S7ZZZC annotation warning source attribution ·
  状态：10-S5/12-S7 的 `requires-unreferenced-code` annotation trim warning 源位置诊断子切片完成；07~12 总目标继续进行中，
  完整 `@dynamically_accessed` dataflow、annotation/promotion policy、未注解反射 warning、完整 trim analyzer 和完整
  metadata sweep/pruning 仍待后续。
  完成项目：visible `trim_warning.annotation[]` marker 现在复用 ExecIR/source-location 推导，在 retained static caller 调用
  `requiresUnreferencedCode` callee 时输出 caller source file、source line/span 与 column/span；reason 文本 marker 继续保持
  quoted/escaped 兼容。source contract 新增对 source attribution helper 和 marker 字段的守护。
  RED/GREEN：RED 为新增 source attribution 断言后 WSL GCC `aot_c_reflection_annotation_preserve` 失败 `Expected Non-NULL`，
  旧输出只有 function/instruction/target/reason，缺少 `sourceFile/sourceLine/sourceColumn`；source contracts 同步失败缺少
  `backend_aot_exec_ir_debug_line_for_instruction` needle。GREEN 后 WSL GCC/clang/Windows MSVC Debug reflection annotation
  preserve 均 12/0，source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-10-s5p-12-s7zzzc-annotation-warning-source-attribution.md`。
  备注：本切片只关闭 annotation warning 的可定位诊断输出；不声明完整 annotation policy、未注解反射诊断、完整 trim analyzer 或
  07~12 总目标完成。

- 2026-07-03 05:13:46 +08:00 · 10-S5O / 12-S7ZZZB callsite `requires-unreferenced-code` annotation warning suppression ·
  状态：10-S5/12-S7 调用方级 trim annotation warning 抑制子切片完成；07~12 总目标继续进行中，完整
  `@dynamically_accessed` dataflow、annotation/promotion policy、未注解反射 warning 和完整 trim analyzer 仍待后续。
  完成项目：retained caller 调用带 `requiresUnreferencedCode` 的 callee 时，如果 caller metadata 带
  `suppressRequiresUnreferencedCodeWarning: true`，generated-C diagnostics 现在把该 warning 计入
  `trim_warnings.annotationSuppressedCount`，不再输出 `trim_warning.annotation[]`；全局
  `suppressAnnotationWarnings` 仍会把所有 annotation warnings 合并进 suppressed count。source contract 也新增
  suppressed counter API 与 suppression metadata key 的守护。
  RED/GREEN：RED 为新增 callsite suppression fixture 后 WSL GCC `aot_c_reflection_annotation_preserve` 失败
  `Expected Non-NULL`，旧输出仍为 `annotationCount = 1`、`annotationSuppressedCount = 0` 且含 visible warning；
  GREEN 后 WSL GCC/clang/Windows MSVC Debug reflection annotation preserve 均 12/0，source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-10-s5o-12-s7zzzb-callsite-annotation-warning-suppression.md`。
  备注：本切片只关闭 `requires-unreferenced-code` 的调用方级 warning 抑制；不声明完整 annotation policy、
  DESCRIPTION promotion、未注解反射诊断或 07~12 总目标完成。

- 2026-07-03 05:04:05 +08:00 · 11-S7ZSJ / 12-S7ZZZA emitted `.zrp` FieldDef-owned orphan metadata sweep/pruning ·
  状态：11-S7/12-S7 owner-TypeDef 驱动的 FieldDef 孤儿 metadata 清扫子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：TypeDef retention 不再因为任意 FieldDef `ownerTypeToken` 引用而保活不可达 TypeDef；FieldDef section
  现在只保留 owner TypeDef 已保活的 rows，并按 retained FieldDef 集合重写 section count、`MEMBER_DEF` token、
  TypeDef `firstFieldDefIndex/fieldDefCount`、field name/signature/constant-pool 引用和 member-token remap sidecar。
  RED/GREEN：RED 为新增 orphan TypeDef-with-FieldDef fixture 后 WSL GCC `aot_c_zrp_metadata_pruning` 失败
  `Expected 576 Was 692`，证明旧 blob 仍保留 dead type/field metadata；GREEN 后 WSL GCC pruning 16/0、pool 8/0、
  typedef 2/0、typespec 2/0，WSL clang 同组 16/0、8/0、2/0、2/0，Windows MSVC Debug pruning 16/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsj-12-s7zzza-fielddef-owner-sweep.md`。
  备注：本切片只关闭 FieldDef owner-TypeDef 孤儿行清扫与相关 pool/range/remap 压缩；不声明完整 11-S7、12-S7
  或 07~12 总目标完成。

- 2026-07-03 04:18:41 +08:00 · 11-S7ZSI / 12-S7ZZZ CLI/project non-publishable `.zrp` sidecar stale cleanup ·
  状态：CLI/project compacted metadata sidecar fail-closed 清理子切片完成；07~12 总目标继续进行中。
  完成项目：CLI AOT 写入路径现在即使当前 embedded blob 不能发布 compacted `.zrp` metadata，也会先解析 generated AOT C
  对应的 sidecar path，并删除同名旧 `.zrp` sidecar；这样普通 `.zro`、无效 definition-table metadata 或后续非 metadata
  重新生成不会遗留上一次成功发布的 stale sidecar。若旧 sidecar 删除失败，写入路径会 fail closed，不继续生成新的 AOT C。
  RED/GREEN：RED 为新增 stale invalid-metadata fixture 后 WSL GCC `cli_aot_compacted_metadata_sidecar` 失败
  `Expected FALSE Was TRUE`，旧 sidecar 仍存在；追加 blocked stale sidecar fixture 后再次 RED
  `Expected FALSE Was TRUE`，证明 cleanup failure 曾被忽略；GREEN 后 WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsi-12-s7zzz-cli-zrp-sidecar-stale-cleanup.md`。
  备注：本切片只关闭 CLI/project sidecar stale artifact 清理；`compiler_aot.c` 已超过大文件阈值，本次只扩展既有
  AOT C artifact orchestration 分支，后续若继续增长 sidecar policy，应优先抽出独立 helper/module。

- 2026-07-03 03:56:32 +08:00 · 11-S6H / 12-S8E CLI full-AOT metadata-drift assertion alignment ·
  状态：CLI project full-AOT 断言与 11-S6H metadata-drift guard/deopt 语义对齐子切片完成；07~12 总目标继续进行中。
  完成项目：`test_cli_compile_emit_aot_c_writes_full_aot_project_c_source` 不再把所有
  `CallInlineStructDynamicDeoptBridge()` 都视为 full-AOT 违规；断言改为继续禁止
  `generic call typed missing AOT instance` / missing-instance marker，同时要求
  `zr_aot_value_exec_call_typed_metadata_guard`、`ZrLibrary_AotRuntime_CanUseTypedDirectCall()` 和
  `typed inline struct direct call metadata drift`，保持 12-S8E METHOD-slot closure 与 11-S6H metadata-drift fallback
  同时可见。
  RED/GREEN：RED 为 `cli_project_incremental` 在 broad bridge anti-needle 上失败 `Expected NULL`；GREEN 后
  WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_project_incremental|cli_aot_compacted_metadata_sidecar|aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed`
  均 4/4。
  产出：`tests/acceptance/2026-07-03-aot-11-s6h-12-s8e-cli-full-aot-metadata-drift-assertion.md`。
  备注：本切片只校正 CLI 项目级测试语义，未修改生产生成器；完整 07~12 目标继续进行。

- 2026-07-03 03:45:20 +08:00 · 11-S7ZSH / 12-S7ZZY CLI/project compacted `.zrp` metadata sidecar path bridge ·
  状态：11-S7/12-S7 CLI/project 自动派生 compacted `.zrp` metadata sidecar path 子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：project 层新增由 AOT C 输出路径派生 `.zrp` sidecar 路径的 helper；CLI AOT 写入路径只在 embedded blob
  同时通过 `.zrp` header 读取和 definition-table validation 时设置 `compactedZrpMetadataOutputPath`，避免普通 `.zro`
  或伪造 header blob 误发布 sidecar；optional AOT C 清理和移除模块清理现在同步删除派生 `.zrp` sidecar。
  RED/GREEN：RED 为新增 CLI sidecar target 后 WSL GCC 链接失败，缺少
  `ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath`；追加 invalid definition-table 覆盖后旧 header-only gate
  失败 `Expected FALSE Was TRUE`。GREEN 后 WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsh-12-s7zzy-cli-zrp-sidecar-path-bridge.md`。
  备注：本切片验证时相邻 `cli_project_incremental` 暴露 full-AOT dynamic bridge broad anti-needle 失败；
  该运行未生成普通项目 sidecar，随后 2026-07-03 03:56:32 +08:00 的 11-S6H/12-S8E 测试语义对齐已收敛该失败。

- 2026-07-03 02:47:37 +08:00 · 11-S7ZSG / 12-S7ZZX writer-level compacted `.zrp` metadata sidecar publication ·
  状态：11-S7/12-S7 writer-level compacted `.zrp` metadata sidecar 发布子切片完成；07~12 总目标继续进行中，
  CLI/project 自动派生 sidecar artifact path、完整 metadata sweep/pruning、完整 trim analyzer、
  annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrAotWriterOptions.compactedZrpMetadataOutputPath` 现在可指定最终 `.zrp` metadata 输出路径；
  `backend_aot_c_publish_compacted_zrp_metadata()` 会在读取最终 header 成功后按字节写出 writer 已准备好的
  compacted embedded metadata blob，写入/关闭失败时清理 partial sidecar 并让 AOT C writer 失败；AOT C writer
  仅在 generated C 文件 `fclose` 成功后发布 sidecar，若 sidecar 发布失败会删除 generated C 并返回 false。
  RED/GREEN：RED 为新增 writer publication 用例后 WSL GCC 编译失败，
  `SZrAotWriterOptions` 缺少 `compactedZrpMetadataOutputPath`；GREEN 后 WSL GCC/clang/MSVC Debug focused CTest
  `aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|aot_c_code_stripping|zrp_metadata_format|cli_aot_writer_options`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsg-12-s7zzx-zrp-compacted-metadata-publication.md`。
  备注：本记录只关闭 writer option 驱动的最终 `.zrp` metadata sidecar 文件发布；不声明 CLI/project 默认路径、
  完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-03 02:04:51 +08:00 · 11-S7ZSF / 12-S7ZZW `.zrp` manifest export unbound declaration row publication ·
  状态：11-S7/12-S7 unbound manifest export declaration 文件级 row 发布子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy、compacted-token file publication 和
  更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`.zrp` manifestExports row validator 现在接受合法 kind 且 `flags/typeToken/memberToken` 全 0 的 unbound
  rows；pruner 会在 string-pool/token remap 阶段保留既有 unbound rows；declaration publication 会把未绑定
  type/method/field exports 追加为持久 rows，target string 复用或追加到 string pool，token 字段保持 0。
  RED/GREEN：RED 为新增 `.zrp` format unbound row 校验后 WSL GCC format 失败 `Expected TRUE Was FALSE`，并新增
  unbound declaration publication pruning 用例后旧 blob 长度 `Expected 806 Was 708`；GREEN 后 WSL GCC/clang/MSVC
  Debug focused CTest `zrp_metadata_format|aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts` 均 8/8。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsf-12-s7zzw-zrp-manifest-export-unbound-declaration-publication.md`。
  备注：本记录只关闭 unbound manifest export declarations 到持久 `.zrp` rows 的发布/校验；不声明完整 11-S7、12-S7
  或 07~12 总目标完成。

- 2026-07-03 01:33:09 +08:00 · 11-S7ZSE / 12-S7ZZV `.zrp` manifest export type declaration row publication ·
  状态：11-S7/12-S7 bound type manifest export declaration 持久 row 发布子切片完成；07~12 总目标继续进行中，
  unbound declaration policy、完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy、
  compacted-token file publication 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：TypeDef pruning 现在产出 source-to-compacted TypeDef token sidecar；manifest export declaration
  publication 使用该 sidecar 将 bound type declarations 追加为持久 `.zrp` `TYPE` rows，并把 source `TYPE_DEF(2)`
  这样的 post-prune token 改写为 compacted `TYPE_DEF(1)`，同时处理 target string-pool 增长和 final definition-table
  validation。
  RED/GREEN：RED 为新增 pruning 用例后 WSL GCC focused 失败，旧 prepared blob 长度 526，缺少 `api.LiveType`
  type row/string；GREEN 后 WSL GCC/clang/MSVC Debug focused CTest
  `aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts` 均 7/7。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zse-12-s7zzv-zrp-manifest-export-type-declaration-publication.md`。
  备注：本记录只关闭 token-bound type declarations 的持久 `.zrp` row 发布；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 00:43:21 +08:00 · 11-S7ZSD / 12-S7ZZU `.zrp` manifest export declaration row publication ·
  状态：11-S7/12-S7 bound method/field manifest export declaration 持久 row 发布子切片完成；07~12 总目标继续进行中，
  type export 持久 row compaction、unbound declaration policy、完整 metadata sweep/pruning、完整 trim analyzer、
  annotation/promotion policy、compacted-token file publication 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_publish_manifest_export_declarations()` 现在把 writer declarations 中带
  `MEMBER_DEF` binding 的 method/field export 追加到 embedded `.zrp` `manifestExports` section；既有 rows 保留，
  target strings 会复用或追加到 string pool，source member token 通过 retained sidecar remap 为 compacted
  `MEMBER_DEF`，最终重新布局并校验 definition tables。prepare path 在 pruning 后统一执行该发布步骤，供 emitter
  后续 size/blob sampling 消费。
  RED/GREEN：RED 为新增 pruning 用例后 MSVC focused 失败，旧 prepared blob 长度 708，缺少 declaration-derived rows
  和 `api.kept`/`api.field` 字符串；GREEN 后 WSL GCC/clang/MSVC Debug focused pruning 12/0，export-token remap 10/0，
  code stripping 10/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsd-12-s7zzu-zrp-manifest-export-declaration-publication.md`。
  备注：本记录只关闭 token-bound method/field declarations 的持久 `.zrp` row 发布；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-02 10:42:54 +08:00 · 11-S7ZSC / 12-S7ZZT `.zrp` manifest export pruning rewrite ·
  状态：11-S7/12-S7 持久 manifest export row 剪裁后重写子切片完成；07~12 总目标继续进行中，
  writer 持久行生成、完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy、compacted-token
  file publication 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：emitted `.zrp` pruning rebuild 现在结构化重写 `manifestExports` section，按 compacted string pool 修正
  `targetStringOffset`，按 retained member-token remap 修正 method/field `MEMBER_DEF`，按 TypeDef remap 修正 type
  export，并对错误 kind/flag/token 组合 fail closed。实现拆到 `backend_aot_c_zrp_metadata_manifest_export.{h,c}`，
  MSVC focused target source list 同步补齐。
  RED/GREEN：RED 为 WSL GCC focused pruning 11/1，旧 raw-copy manifest export rows 未重写 string offset/member token；
  GREEN 后 WSL GCC/clang/MSVC Debug focused pruning 11/0，export-token remap 10/0，code stripping 10/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsc-12-s7zzt-zrp-manifest-export-pruning.md`。
  备注：本记录只关闭已有持久 section rows 的剪裁后重写；不声明完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-02 10:15:47 +08:00 · 11-S7ZSB / 12-S7ZZS `.zrp` manifest export persistent section format ·
  状态：11-S7/12-S7 `.zrp` manifest export 持久 section 格式层支撑子切片完成；07~12 总目标继续进行中，
  持久 rows 的 generator/pruner 发布、完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion policy
  和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`.zrp` metadata v4 将 header 扩展为 224 字节、section count 扩展为 13，并追加
  `manifestExports` section/row；core header read/write/validate、section view、definition-table payload writer、
  CLI dump/diff、AOT size stats、generated marker/delta 输出和 code-stripping registration 期望均同步到该 section。
  RED/GREEN：RED 为 WSL GCC format 测试缺少 manifest export section 类型/字段而编译失败，以及相邻
  code-stripping focused 因旧 12-section stats/registration 断言失败 10/1。GREEN 后 WSL GCC/clang/MSVC Debug
  focused format、CLI dump、size delta、code-stripping 均通过。
  验证：`zr_vm_zrp_metadata_format_test` 13/0、`zr_vm_cli_zrp_metadata_dump_test` 通过、
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 10/0、`zr_vm_aot_c_zrp_metadata_size_deltas_test` 2/0、
  `zr_vm_aot_c_code_stripping_test` 10/0 覆盖 WSL GCC；WSL clang 和 Windows MSVC Debug 同组 focused 验证通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsb-12-s7zzs-zrp-manifest-export-section.md`。
  备注：本记录只关闭持久 `.zrp` manifest export section 的格式层支撑；不声明完整 11-S7、12-S7 或 07~12
  总目标完成。

- 2026-07-02 09:38:04 +08:00 · 11-S7ZSA / 12-S7ZZR manifest export kind/token guard ·
  状态：11-S7/12-S7 manifest export table builder 支撑子切片完成；07~12 总目标继续进行中，
  持久 `.zrp` manifest export section、完整 metadata sweep/pruning、完整 trim analyzer、annotation/promotion
  policy 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：generated-C manifest export table builder 现在校验 export kind 与绑定 token shape：type export 拒绝
  `MEMBER_DEF` binding，method/field export 拒绝 `TYPE_DEF` binding；错误 declaration fail closed，且不会留下
  partially-built manifest export table。
  RED/GREEN：RED 为 WSL GCC focused export-token remap 新增 mismatch guard 后 10/1，旧 builder 接受错误 token kind。
  GREEN 后 WSL GCC/clang/MSVC Debug direct export-token remap 均 10/0。
  验证：WSL GCC direct source contracts 24/0；WSL GCC/clang provider shared-library smoke 均 1/0；MSVC focused build
  通过，仍保留既有 C4267/C4702 warning 噪声。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsa-12-s7zzr-manifest-export-kind-token-guard.md`。
  备注：本记录只关闭 manifest export table builder 的 kind/token shape guard；不声明完整 11-S7、12-S7 或 07~12
  总目标完成。

- 2026-07-02 09:15:26 +08:00 · 11-S7ZR / 12-S7 range-selected provider runtime export publication ·
  状态：11-S7/12-S7 range-selected provider runtime/export publication 支撑子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer、compacted-token file publication 和更完整的 ABI drift/deopt
  闭环仍待后续。
  完成项目：provider shared-library smoke 在同一 root project 中覆盖 exact `$mathLocal@2.1.0/ops/sum` 与
  candidate-selected `$mathRange@2.1.0/ops/sum` 两条 provider import；`mathRange` 从 2.0.5/2.1.0 candidates 中选择
  2.1.0。strict AOT C runtime 导入 candidate-selected alias 后可同时读取普通 `seed` public export 和 attached manifest
  export metadata (`add`/`seed`)。
  RED/GREEN：RED 为 WSL GCC provider smoke 在 `$mathRange@2.1.0/ops/sum` 上 metadata view 已 attach 但 public export
  `seed` 为 NULL；根因是 `PublishModuleExports()` 对缺失 `frame.recordHandle` 的 generated frame 选中了 earlier
  exact alias record。GREEN 后 runtime 优先使用匹配当前 function 的 `runtimeState->activeRecord`，range alias export
  正确发布。
  验证：WSL GCC/clang provider shared-library smoke 均 1/0；WSL GCC/clang/MSVC Debug direct provider version-selection
  4/0、project import resolver 9/0、manifest normalization 28/0、provider runtime 1/0、source contracts 24/0、
  frame setup contracts 1/0；Windows MSVC Debug provider smoke 编译通过且 Unix-only 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zr-range-selected-provider-export-publication.md`。
  备注：本记录只关闭 range-selected provider runtime export publication；不声明完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-02 08:51:27 +08:00 · 11-S7ZQ / 12-S7 legacy dependency AssemblyRef identity canonicalization ·
  状态：11-S7/12-S7 provider/reference metadata support 测试校准子切片完成；07~12 总目标继续进行中，完整
  metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：关闭 11-S7ZL 记录中的 `zr_vm_project_import_canonicalization_test` 35/1 残留；legacy dependency
  import 仍以 canonical `$math@1.2.3/ops/sum` 作为 static import/module effect key，但 AssemblyRef identity
  使用 normalized provider assembly name `math`。测试同步断言不存在 canonical key 命名的 AssemblyRef。生产代码未修改。
  RED/GREEN：RED 为旧测试按 canonical key 查 AssemblyRef 并失败 35/1；临时 guard 证明 effect 已有
  `$math@1.2.3/ops/sum` + `math` identity。GREEN 后 WSL GCC/clang/Windows MSVC Debug direct canonicalization
  均 35/0。
  验证：同三套工具链 direct provider version-selection 4/0、project import resolver 9/0、manifest normalization
  28/0、provider runtime 1/0；provider shared-library smoke 在 WSL GCC/clang 1/0，Windows MSVC Debug 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zq-legacy-dependency-assembly-ref-identity.md`。
  备注：本记录只关闭 stale AssemblyRef identity 断言；不声明完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-02 08:28:32 +08:00 · 11-S6I malformed binding table fail-closed ·
  状态：11-S6 no-crash ABI drift injection 支撑子切片完成；07~12 总目标继续进行中，cross-module token resolve、
  完整 metadata sweep/pruning、完整 trim analyzer 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：metadata runtime binding compatibility 现在拒绝非零 binding count 搭配 NULL binding table，
  返回 `INVALID_ARGUMENT` 并填充 report；typed direct-call guard 因此对 malformed caller binding table deopt/返回 false。
  RED/GREEN：RED 为旧扫描路径把 malformed table 当 compatible，WSL GCC focused metadata runtime test 16/1；
  GREEN 后 WSL GCC/clang/Windows MSVC Debug 上 metadata runtime binding compatibility 均 16/0，typed direct-call
  compatibility 均 4/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s6i-malformed-binding-table-fail-closed.md`。
  备注：本记录只关闭 malformed binding table no-crash fail-closed；不声明完整 11-S6、11-S7 或 12-S7 完成。

- 2026-07-02 08:16:21 +08:00 · 11-S7ZP / 12-S7 provider automatic range-based candidate selection ·
  状态：11-S7/12-S7 provider candidate-set range selection 支撑子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`.zrp` `references.alias` 支持 `candidates[]` 替代 exact `path`；project loader 会 probe
  候选 `.zrp`/`.zrm`，按 declared assembly、可选 exact version、declared `[min, max)` 与 strict semver
  选择最高 in-range provider，随后复用既有 normalized reference/package path，保持 canonical
  `$alias@version/module`。未选中的 candidates 不污染 dependency package 表；无匹配 candidate set fail-closed。
  `zrp.schema.json` 同步新增 `path`/`candidates` 互斥 schema 与 `mathRange` 示例。
  RED/GREEN：RED 为 candidate-only reference 因缺少 required `path` 被旧 loader 拒绝，positive 选择用例失败；
  GREEN 后 version-selection focused test 4/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct version-selection、project import resolver、manifest normalization、
  provider runtime 与 provider shared-library smoke 均通过；schema JSON parse 通过。Clang/MSVC 仍显示既有
  `project.c` const qualifier warning。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zp-provider-range-candidate-selection.md`。
  备注：本记录只关闭 automatic range-based candidate selection；不声明完整 metadata sweep/pruning、full trim analyzer
  或完整 ABI drift/deopt loop 完成。

- 2026-07-02 08:13:22 +08:00 · 11-S7ZO / 12-S7 provider export metadata attach fixture ·
  状态：11-S7/12-S7 provider export metadata attach 端到端覆盖子切片完成；07~12 总目标继续进行中，
  automatic range-based candidate selection、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  ABI drift/deopt 闭环仍待后续。
  完成项目：provider shared-library smoke 的 `.zrp` provider manifest 现在声明 method `add` 与 field `seed`
  exports；fixture 在 AOT C 生成前应用 export declarations，断言 writer options 绑定 `MEMBER_DEF` token，generated C
  发布 manifest export table 和 member-token flags，runtime import 后从 attached provider metadata runtime 查回
  `add`/`seed` manifest export views。
  RED/基线：之前 success fixture 只覆盖 provider 动态库加载与普通 export value publication，未覆盖 export manifest
  metadata attach；本切片以端到端 fixture 补齐，生产路径无需改动。
  GREEN：WSL GCC/clang provider shared-library smoke 均 1/0；Windows MSVC Debug 编译通过并按 Unix-only
  dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zo-provider-export-metadata-attach-fixture.md`。
  备注：本记录只关闭 provider generated shared-library export metadata attach fixture；不声明自动候选选择、
  完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 07:25:55 +08:00 · 11-S7ZN / 12-S7 provider multi-version exact selection + range guard ·
  状态：11-S7/12-S7 provider exact alias/version selection 与 declared strict semver range guard 支撑子切片完成；
  07~12 总目标继续进行中，automatic range-based candidate selection、cross-provider export metadata attach、
  完整 metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：新增 `zr_vm_project_import_provider_version_selection_test`，覆盖同一 root `.zrp` 同时引用
  `zr.math` 2.1.0 与 3.1.0 时，`&mathV2.ops.sum` / `&mathV3.ops.sum` 分别解析到
  `$mathV2@2.1.0/ops/sum` 与 `$mathV3@3.1.0/ops/sum`，并使用各自 `.zrp` provider 的 source/binary/AOT
  library path；`project.c` 现在对 legacy dependencies、`references` `.zrp` 和 `.zrm` 引用执行 strict
  `major.minor.patch` declared range fail-closed 校验。
  RED/GREEN：RED 为负向用例要求 `version = 3.1.0` 且 declared range `[2.0.0, 3.0.0)` 的 provider manifest
  被拒绝，但旧 parser 仍返回 project；GREEN 后同一测试 2/0。
  验证：WSL GCC/clang direct version-selection、project import resolver、manifest normalization、provider runtime
  与 provider shared-library smoke 均通过；Windows MSVC Debug direct version-selection/resolver/manifest/runtime 均通过，
  provider shared-library smoke 按 Unix-only dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zn-provider-version-selection-range-guard.md`。
  备注：本记录只关闭 exact alias/version path selection 与 strict declared range guard；不声明自动候选选择、
  export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 07:01:13 +08:00 · 11-S7ZM / 12-S7 provider AOT dynamic-library success fixture ·
  状态：11-S7/12-S7 standalone provider AOT dynamic-library success 支撑子切片完成；07~12 总目标继续进行中，
  multi-version selection、export metadata attach、完整 metadata sweep/pruning、完整 trim analyzer
  和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：新增 `zr_vm_aot_c_provider_shared_library_smoke_test`，真实生成 `.zrp` provider 的
  `ops/sum.zr`、`.zro`、AOT C 和 `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.so`，并在 strict AOT C 下导入
  canonical `$mathLocal@2.1.0/ops/sum`；断言 provider-local descriptor name 被接受、module cache 仍使用 canonical key、
  executed-via 为 AOT C，且 provider `seed` export 发布为 37。
  RED/基线：上一切片只覆盖 missing-provider diagnostic，未覆盖真实 provider 动态库成功加载；新增 target 首次在陈旧 build tree
  上需要重新 configure。配置后当前实现已通过，说明 11-S7ZL runtime consumption 已支撑成功路径。
  GREEN：WSL GCC/clang direct `zr_vm_aot_c_provider_shared_library_smoke_test` 均 1/0；Windows MSVC Debug target
  编译通过并按 Unix-only dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zm-provider-aot-dynamic-library-success.md`。
  备注：本记录只关闭 `.zrp` provider generated shared-library success fixture；不声明多版本候选选择、
  cross-provider export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 06:41:32 +08:00 · 11-S7ZL / 12-S7 provider AOT runtime load-request consumption ·
  状态：11-S7/12-S7 standalone provider AOT runtime consumption 支撑子切片完成；07~12 总目标继续进行中，
  provider 动态库成功加载端到端、multi-version selection、export metadata attach、完整 metadata sweep/pruning、完整 trim analyzer
  和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：新增 `zr_vm_project_import_aot_provider_runtime_test`，锁定 strict AOT loader 对 canonical
  `$alias@version/module` provider import 的路径消费；`aot_runtime_prepare_record()` 现在调用
  `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()`，用 `.zrp` provider 的 source/binary/library path 加载，
  descriptor validation 使用 provider-local module name，同时对 `.zrm` archive entry fail-closed。
  RED：WSL GCC focused direct test 编译通过但失败于 `lastError == null`，没有报告 provider AOT library path。
  GREEN：补齐 runtime 消费逻辑后 WSL GCC/clang/Windows MSVC Debug direct
  `zr_vm_project_import_aot_provider_runtime_test` 均 1/0；WSL GCC `zr_vm_project_import_resolver_test` 仍 9/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zl-provider-aot-runtime-load-request.md`。
  备注：额外探测的 WSL GCC `zr_vm_project_import_canonicalization_test` 曾在
  `test_project_compile_applies_dependency_import_version_range_to_assembly_ref` 失败 35/1；后续 11-S7ZQ 已确认
  这是 stale AssemblyRef identity 断言并关闭。

- 2026-07-02 06:13:21 +08:00 · 11-S7ZK / 12-S7 provider AOT load request ·
  状态：11-S7/12-S7 standalone provider AOT load-request 支撑子切片完成；07~12 总目标继续进行中，
  provider runtime dynamic loading、multi-version selection、完整 metadata sweep/pruning、完整 trim analyzer
  和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`project.h` 新增 `SZrLibrary_ProjectImportProviderAotLoadRequest` 与
  `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()`；`project_import_provider_location.c` 复用上一切片的
  provider location discovery，为 `.zrp` provider 生成 canonical module key、descriptor-local module name、
  source/binary/intermediate path 和 backend-specific AOT library path，为 `.zrm` provider 保留 archive/entry view
  且不伪造 filesystem library path；`test_project_import_resolver.c` 覆盖 `.zrp` C backend 与 `.zrm` LLVM backend request。
  RED：focused WSL GCC build 在新增测试引用缺失的 load-request struct/API 时编译失败。
  GREEN：补齐 public API/实现后 WSL GCC/clang/Windows MSVC Debug direct
  `zr_vm_project_import_resolver_test` 均 9/0 通过。
  测试：`cmake --build build-wsl-gcc --target zr_vm_project_import_resolver_test -j 2 &&
  ./build-wsl-gcc/bin/zr_vm_project_import_resolver_test`；WSL clang 同目标/同 direct test；Windows MSVC Debug
  `cmake --build build-msvc --target zr_vm_project_import_resolver_test --config Debug -j 2 &&
  .\build-msvc\bin\Debug\zr_vm_project_import_resolver_test.exe`。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zk-provider-aot-load-request.md`。
  备注：本记录只关闭 provider AOT load request planning；不声明 provider runtime dynamic loading、版本候选选择、
  export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 05:58:12 +08:00 · 11-S7ZJ / 12-S7 provider import location discovery ·
  状态：11-S7/12-S7 standalone provider import-path discovery 支撑子切片完成；07~12 总目标继续进行中，
  provider runtime loading、multi-version selection、完整 metadata sweep/pruning、完整 trim analyzer
  和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`project.h` 新增 `SZrLibrary_ProjectImportProviderLocation` 和
  `ZrLibrary_Project_ResolveImportProviderLocation()`；新拆分的
  `project_import_provider_location.c` 复用现有 import module-key resolver、AssemblyRef version-range query
  与 `.zrm` module-entry lookup，把 raw import specifier 统一解析为 canonical `$alias@version/module` key、
  declared assembly/version range，以及 `.zrm` archive entry 或 `.zrp` source/binary/intermediate provider paths。
  RED/GREEN：RED 为 `zr_vm_project_import_resolver_test` 新增 provider location 覆盖后编译失败于缺少 public type/API；
  GREEN 后同一测试覆盖 `.zrp` project reference 与 `.zrm` assembly reference 两条 provider location 路径。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_project_import_resolver_test` 均 9/0；该 focused target
  当前未作为独立 CTest 注册。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zj-provider-import-location-discovery.md`。
  备注：本记录只关闭 provider import-path location discovery；不声明 provider runtime loading、版本候选选择、
  export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 05:34:27 +08:00 · 11-S7ZI / 12-S7 provider manifest export binding gate ·
  状态：11-S7/12-S7 provider import signature manifest-export gate 支撑子切片完成；07~12 总目标继续进行中，
  standalone provider import-path discovery/loading/version selection、完整 metadata sweep/pruning、完整 trim analyzer
  和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`module_import_signature.c` 在 provider typed export candidate 完成 token/signature/hash/blob 校验后，
  通过新拆分的 `module_import_signature_manifest_export.c/.h` 调用
  `ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()`；当 provider module 已 attached
  `SZrAotCodeRegistration.manifestExports` 时，import verifier 会把 caller `MEMBER_REF`、effect target token/hash、
  provider typed export token/hash 和 entry module hash 组成 binding snapshot，再要求 published manifest export
  type/member token 与 provider typed export token 一致。旧 provider 没有 manifest export table 时继续保持旧兼容路径。
  RED/GREEN：RED 为 `tests/module/test_metadata_type_ref_binding.c` 新增 provider manifest export token drift 用例后，
  WSL GCC direct `zr_vm_metadata_type_ref_binding_test` 9 tests / 1 failure，旧 verifier 接受了 manifest table
  发布 `MEMBER_DEF(2)` 但 typed export 仍暴露 `MEMBER_DEF(1)` 的漂移；GREEN 后同一测试 9/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_type_ref_binding_test` 9/0、
  `zr_vm_metadata_runtime_manifest_exports_test` 7/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；
  同三套工具链 CTest
  `metadata_type_ref_binding|metadata_runtime_manifest_exports|metadata_runtime_binding_compatibility` 均 3/3。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zi-provider-manifest-export-binding-gate.md`。
  备注：本记录只关闭 attached provider manifest export table 在真实 import signature verifier 中的 fail-closed gate；
  不声明 standalone provider import-path wiring、provider discovery/loading/version selection、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-02 04:46:47 +08:00 · 11-S7ZH / 12-S7 manifest export binding gate ·
  状态：11-S7/12-S7 generated-C manifest export table binding compatibility 支撑子切片完成；07~12 总目标继续进行中，
  cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、
  完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()`，把
  `ZrCore_MetadataRuntime_ReadManifestExportView()` 的 `kind + target` export 查询结果接到既有 token/signature/module/layout
  兼容性检查；成功路径要求 binding 的 resolved metadata token 与 manifest export 发布的 type/member token 一致。
  缺失 export 返回 `MANIFEST_EXPORT_NOT_FOUND`，export token 不一致返回
  `MANIFEST_EXPORT_TOKEN_MISMATCH`，版本范围漂移继续返回既有 module-version mismatch report。
  RED/GREEN：RED 为 `tests/module/test_metadata_runtime_manifest_exports.c` 新增 binding gate 用例后，WSL GCC focused
  build 因缺少 public API 和两个 manifest-export binding status 编译失败；GREEN 后同一测试扩展为 7/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 7/0、
  `zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports` 与 `metadata_runtime_binding_compatibility` 均通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zh-manifest-export-binding-gate.md`。
  备注：本记录只关闭 attached manifest export view 与 binding compatibility predicate 的本地 gate；不声明 provider
  loading/version binding、import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 04:23:15 +08:00 · 11-S7ZG / 12-S7 manifest export runtime view ·
  状态：11-S7/12-S7 generated-C manifest export table runtime consumption 支撑子切片完成；07~12 总目标继续进行中，
  cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、
  完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：新增 `SZrMetadataRuntimeManifestExportView` 与
  `ZrCore_MetadataRuntime_ReadManifestExportView()`；attached metadata runtime 可按 `kind + target` 从
  generated-C manifest export table 唯一查询 type/method/field export，并返回只读 entry、index、target、
  `typeToken`/`memberToken`。重复 target/kind、缺少 required token 或 token shape 不匹配时 fail closed 并清空输出。
  实现拆入 `metadata_runtime_manifest_exports.c`，避免继续扩大接近阈值的 `metadata_runtime.c`。
  RED/GREEN：RED 为 `tests/module/test_metadata_runtime_manifest_exports.c` 新增 runtime view 查询、歧义和缺 token
  断言后，WSL GCC 构建因缺少 view type 与 API 编译失败；GREEN 后同一测试 4/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 4/0、
  `zr_vm_metadata_runtime_query_test` 25/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链
  CTest `metadata_runtime_manifest_exports`、`metadata_runtime_query`、`metadata_runtime_binding_compatibility`
  分别通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zg-manifest-export-runtime-view.md`。
  备注：本记录只关闭 attached manifest export table 的 runtime lookup/view API；不声明 provider loading/version
  binding、import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 03:57:34 +08:00 · 11-S7ZF / 12-S7 manifest export runtime mirror ·
  状态：11-S7/12-S7 generated-C manifest export table runtime mirror 支撑子切片完成；07~12 总目标继续进行中，
  cross-module provider loading/version binding、standalone provider import-path manifest consumption、完整 metadata
  sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrMetadataRuntime` 现在镜像 attached `SZrAotCodeRegistration.manifestExports/manifestExportCount`；
  `ZrCore_Module_AttachMetadataRuntime()` 在 attach 时显式发布该 pointer/count；新增
  `zr_vm_metadata_runtime_manifest_exports_test` 和 CTest `metadata_runtime_manifest_exports`，锁定 type/method
  manifest export entry 在 metadata runtime 中可读。新增字段追加在 runtime 结构体尾部，避免破坏已有按位置初始化的旧字段顺序。
  RED/GREEN：RED 为 focused metadata runtime manifest export 测试编译失败，缺少 `manifestExports` /
  `manifestExportCount` runtime 字段；一次相邻 query 运行暴露中间插入字段会造成旧结构布局错位，修正为尾部追加后 GREEN。
  测试结果：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 1/0、
  `zr_vm_metadata_runtime_query_test` 25/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 focused
  CTest `metadata_runtime_manifest_exports`、`metadata_runtime_query`、`metadata_runtime_binding_compatibility` 均 3/3。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zf-manifest-export-runtime-mirror.md`。
  备注：本记录只关闭 attached metadata runtime 对 generated-C manifest export table 的 pointer/count mirror；
  不声明跨模块 provider/version 绑定、standalone provider import-path 消费、完整 metadata sweep/pruning 或 full
  trim analyzer 完成。

- 2026-07-02 03:24:57 +08:00 · 11-S7ZE / 12-S7 manifest export table publication ·
  状态：11-S7/12-S7 generated-C manifest export table publication 支撑子切片完成；07~12 总目标继续进行中，
  cross-module provider loading/version binding、standalone provider manifest consumption、完整 metadata
  sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 13u`，新增 `SZrAotManifestExportEntry`
  以及 `manifestExports/manifestExportCount` descriptor/codeRegistration 字段；AOT C writer 生成
  `zr_aot_manifest_exports[]` 并写入 `ZrAotCompiledModule` 与 `SZrAotCodeRegistration`；table builder
  对 method/field export 的 `MEMBER_DEF` source token 应用 retained member-token sidecar remap，对 type export
  保留 `TYPE_DEF` token，并让 runtime descriptor validation 校验 table pointer/count、entry kind/flags/token shape。
  RED/GREEN：RED 为 focused remap test 期待 manifest export table entry 发布 compacted member token，但旧生成侧只有
  comment diagnostics；GREEN 后 method export source `MEMBER_DEF(7)` 发布为 compacted `MEMBER_DEF(2)`，
  writer options 生成物包含 `manifest.exportTableEntries`、static table 和 descriptor/codeRegistration wiring。
  测试结果：WSL GCC direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 9/0、
  `zr_vm_cli_aot_writer_options_test` 18/0、`zr_vm_aot_c_source_contracts_test` 24/0；WSL GCC/clang/Windows MSVC Debug
  focused CTest `aot_c_zrp_metadata_export_token_remap|cli_aot_writer_options` 均 2/2，source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7ze-manifest-export-table-publication.md`。
  备注：本记录只关闭 generated-C descriptor/codeRegistration manifest export table 发布与 compacted member-token 写入；
  不声明跨模块 provider/version 绑定、完整 metadata sweep/pruning、standalone provider manifest consumption 或 full
  trim analyzer 完成。

- 2026-07-02 02:22:47 +08:00 · 11-S7ZD / 12-S7 type export declaration type-token binding ·
  状态：11-S7/12-S7 type export declaration token binding 支撑子切片完成；07~12 总目标继续进行中，持久 export
  manifest/table writer、compacted-token file publication、cross-module provider loading/version binding、完整
  metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrAotManifestExportDeclaration` 新增 writer-visible type-token binding 字段；CLI AOT export bridge
  将 current-module type declaration target 绑定到当前函数 metadata token records 中匹配的 `TYPE_DEF` token；
  generated C 输出 `manifest.export[i].typeToken = 0x...` 诊断，source-contract 固定公开字段和 marker。
  RED/GREEN：RED 为 WSL GCC focused build 编译失败于缺失 `hasTypeTokenBinding` / `typeToken`；GREEN 后新增
  type export declaration token binding 用例通过，并验证 generated-C marker。中间一次 direct run 暴露测试把
  TypeDef token 编码误期望为 `0x01000001`，已修正为项目实际 `0x02000001`。
  测试结果：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 18/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zd-type-export-declaration-type-token-binding.md`。
  备注：本记录只关闭 current-module type export declaration 的 writer-input token binding，不声明 persistent table、
  provider version binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 01:57:22 +08:00 · 11-S7ZC / 12-S7 field export declaration member-token binding ·
  状态：11-S7/12-S7 field export declaration token binding 支撑子切片完成；07~12 总目标继续进行中，持久 export
  manifest/table writer、type export token binding、compacted-token file publication、cross-module provider
  loading/version binding、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：CLI AOT export bridge 将 current-module field declaration target 绑定到当前函数 typed exported variable
  symbol 的 `MEMBER_DEF` token；method/function 绑定路径保持不变，generated C 输出相同的
  `manifest.export[i].memberToken = 0x...` 诊断。
  RED/GREEN：RED 为 WSL GCC direct `zr_vm_cli_aot_writer_options_test` 新增 field binding 用例失败于
  `Expected TRUE Was FALSE`；GREEN 后新增 field export declaration token binding 用例通过，并验证 generated-C marker。
  测试结果：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 17/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zc-field-export-declaration-member-token-binding.md`。
  备注：本记录只关闭 current-module field export declaration 的 writer-input token binding，不声明 persistent table、
  type binding、provider version binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 01:43:16 +08:00 · 11-S7ZB / 12-S7 export declaration member-token binding ·
  状态：11-S7/12-S7 method export declaration token binding 支撑子切片完成；07~12 总目标继续进行中，持久 export
  manifest/table writer、type/field export token binding、compacted-token file publication、cross-module provider
  loading/version binding、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrAotManifestExportDeclaration` 新增 writer-visible member-token binding 字段；CLI AOT export bridge
  将 current-module method declaration target 绑定到当前函数 typed exported function symbol 的 `MEMBER_DEF` token；
  generated C 输出 `manifest.export[i].memberToken = 0x...` 诊断，source-contract 固定公开字段和 marker。
  RED/GREEN：RED 为 WSL GCC `zr_vm_cli_aot_writer_options_test` 编译失败于缺失 `hasMemberTokenBinding` /
  `memberToken`；GREEN 后新增 method export declaration token binding 用例通过，并验证 generated-C marker。
  测试结果：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 16/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zb-export-declaration-member-token-binding.md`。
  备注：本记录只关闭 current-module method export declaration 的 writer-input token binding，不声明 persistent table、
  type/field binding、provider version binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 01:09:04 +08:00 · 11-S7ZA / 12-S7 export declaration writer option bridge ·
  状态：11-S7/12-S7 export declaration writer bridge 支撑子切片完成；07~12 总目标继续进行中，持久 export
  manifest/table writer、export target token binding、compacted-token file publication、cross-module provider
  loading/version binding、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrAotWriterOptions` 新增 manifest export declaration carrier；CLI AOT preserve-root scratch storage
  新增 export declaration 生命周期管理；`compiler_aot_exports.{h,c}` 将 `.zrp` project `exports` 映射为 writer options；
  generated C 输出 `manifest.exports` 与逐项 `manifest.export[i] kind=... target=...` 诊断。
  RED/GREEN：RED 为 WSL GCC `zr_vm_cli_aot_writer_options_test` 编译失败于缺失 writer option fields/enums 和
  scratch storage；GREEN 后 method/type/field export declarations 均进入 writer options 并出现在 generated-C diagnostics。
  测试结果：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 15/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_source_contracts|aot_c_code_stripping` 匹配已注册的 `cli_aot_writer_options` 与
  `aot_c_code_stripping`，均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7za-export-declaration-writer-options.md`。
  备注：本记录只关闭 manifest export declaration 的 writer-visible diagnostics，不声明 persistent table、token binding、
  provider version binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 00:37:35 +08:00 · 11-S7Z / 12-S7 export manifest declaration model ·
  状态：11-S7/12-S7 manifest export declaration 输入模型子切片完成；07~12 总目标继续进行中，持久 export manifest/table writer、
  export target token binding、cross-module provider loading/version binding、完整 metadata sweep/pruning、完整 trim analyzer
  和更完整的 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrLibrary_Project` 新增 `exports` declaration model；`project_exports.{h,c}` 解析 `.zrp`
  `type`/`method`/`field` export targets 并拒绝 invalid kind/target；project load/free 和 `zrp.schema.json`
  完成 schema parity。
  RED/GREEN：RED 为 WSL GCC project manifest normalization 编译失败于缺失 export declaration fields/enums；
  GREEN 后 WSL GCC/clang 和 Windows MSVC Debug direct manifest normalization 均为 28/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7z-zrp-manifest-export-declarations.md`。
  备注：本记录只关闭持久 export table 的 manifest 输入层，不声明 writer、token binding、provider version binding 或完整
  11-S7/12-S7 关闭。

- 2026-07-02 00:11:02 +08:00 · 12-S7ZZQ / 11-S7 runtime export member-token publication ·
  状态：11-S7/12-S7 export token publication 支撑子切片完成；07~12 总目标继续进行中，持久 export manifest/table writer、
  更完整 cross-module provider loading/version binding、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：AOT generated-C 已发布的 `SZrAotMemberTokenRemap` sidecar 现在由 core module metadata runtime attach 消费；
  `ZrCore_Module_AttachMetadataRuntime()` 镜像 `memberTokenRemapCount`，并把 `sourceToken -> targetToken` 写回 loaded entry
  function 的 `typedExportedSymbols`，让跨模块 import signature provider 匹配读取 compacted retained `MEMBER_DEF` token。
  RED/GREEN：RED 新增 runtime export remap 用例后，WSL GCC 构建失败于 `SZrMetadataRuntime` 缺少
  `memberTokenRemapCount`；GREEN 后新增 runtime mirror 字段和 attach-time writeback helper。
  验证：WSL GCC/clang direct `zr_vm_metadata_runtime_query_test` 均 25/0；WSL GCC/clang CTest
  `metadata_runtime_query|metadata_runtime_binding_compatibility|metadata_type_ref_binding` 均 3/3；
  Windows MSVC Debug direct `zr_vm_metadata_runtime_query_test` 25/0，同 metadata CTest 3/3；WSL GCC/clang/MSVC Debug CTest
  `aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics` 均 3/3。
  产出：`tests/acceptance/2026-07-02-aot-12-s7zzq-runtime-export-member-token-publication.md`。
  备注：本记录只关闭 runtime provider typed export table 的 compacted member-token publication；持久 manifest/table writer、
  完整 provider version binding、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 23:36:12 +08:00 · 12-S7ZZP / 11-S7 signature-rooted ModuleRef retention ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：retained signature blob remap 现在可作为 ModuleRef retention root；只被 retained signature blob 内
  `ASSEMBLY_REF` 节点引用、没有 retained `TYPE_REF`/`MEMBER_REF` token-record root 的 ModuleRef row 会保留，其
  name/version string-pool slice 也会进入 string remap，并在 signature rewrite 时发布 compacted `ASSEMBLY_REF` RID。
  RED/GREEN：RED 新增 signature-rooted ModuleRef fixture：retained `FIELD_SIG(ASSEMBLY_REF source RID2)` 没有
  retained import token-record root，旧 pruning 在 ModuleRef row 已被剪掉后重写 signature，WSL GCC pool pruning 失败为
  `Expected TRUE Was FALSE`；GREEN 后 signature remap 先于 ModuleRef count/string remap 构建，ModuleRef pruning 会扫描
  retained signature blob `ASSEMBLY_REF` payload。
  验证：WSL GCC direct zrp metadata pruning 10/0、TypeSpec pruning 2/0、pool pruning 8/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 10/0、2/0、8/0、8/0、24/0，focused metadata
  CTest 4/4；Windows MSVC Debug direct 同 set 10/0、2/0、8/0、8/0、24/0，focused metadata CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzp-signature-rooted-module-ref-retention.md`。
  备注：本记录只关闭 retained signature blob 作为唯一 live root 时的 ModuleRef row/name/version string 保留与 compacted
  AssemblyRef RID 发布；cross-module target/provider binding、真实 export manifest/table rewrite/publication、
  annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 22:51:15 +08:00 · 12-S7ZZO / 11-S7 signature MemberRef token rewrite ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：retained signature blob 的 `MEMBER_REF` signature node 现在不再只跳过内嵌 token，而是复用既有
  MethodDef/FieldDef token-record remap 路径，把 source `MEMBER_DEF` token 写回为 retained metadata 发布的 compacted
  RID，并在 signature validation/hash recomputation 前完成 blob rewrite。
  RED/GREEN：RED 新增 MethodDef compaction fixture：retained MethodDef source RID2 被压缩为 RID1，但 signature blob root
  `MEMBER_REF` 仍保留 source RID2，WSL GCC zrp metadata pruning 10 tests / 1 failure（`Expected 50331649 Was
  50331650`）；GREEN 后 blob 内 token 同步发布 compacted RID1，source contract 锁定 signature MemberRef rewrite helper。
  验证：WSL GCC direct zrp metadata pruning 10/0、TypeSpec pruning 2/0、pool pruning 7/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 10/0、2/0、7/0、8/0、24/0，focused metadata
  CTest 4/4；Windows MSVC Debug direct 同 set 10/0、2/0、7/0、8/0、24/0，focused metadata CTest 4/4；focused
  `git diff --check` 无 whitespace error，仅有仓库 CRLF normalization warning。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzo-signature-member-ref-token-rewrite.md`。
  备注：本记录只关闭 retained signature blob 内 `MEMBER_REF` local member token 随 MethodDef/FieldDef
  pruning/compaction 重写；cross-module target/provider binding、真实 export manifest/table rewrite/publication、
  annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 22:34:08 +08:00 · 12-S7ZZN / 11-S7 signature AssemblyRef token rewrite ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：retained signature blob rewrite context 现在接收 TypeSpec/ModuleRef rows；签名节点遇到 `ASSEMBLY_REF`
  token 时会调用现有 ModuleRef remap helper，并把 compacted `ASSEMBLY_REF` RID 写回 signature blob，再执行签名校验和
  signature hash recomputation。
  RED/GREEN：RED 新增 ModuleRef compaction fixture：source `ASSEMBLY_REF` RID2 被 retained `TYPE_REF` token record
  引用并压缩为 RID1，但 signature blob root `ASSEMBLY_REF` 仍保留 source RID2，WSL GCC pool pruning 7 tests / 1 failure
  （`Expected 67108865 Was 67108866`）；GREEN 后 blob 内 token 同步发布 compacted RID1，source contract 锁定 signature
  ModuleRef rewrite helper。
  验证：WSL GCC direct zrp metadata pruning 9/0、TypeSpec pruning 2/0、pool pruning 7/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 9/0、2/0、7/0、8/0、24/0，focused metadata CTest
  4/4；Windows MSVC Debug direct 同 set 9/0、2/0、7/0、8/0、24/0，focused metadata CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzn-signature-assembly-ref-token-rewrite.md`。
  备注：本记录只关闭 retained signature blob 内 `ASSEMBLY_REF` token 随 ModuleRef pruning/compaction 重写；cross-module
  target/provider binding、真实 export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和
  full trim analyzer 仍未完成。

- 2026-07-01 22:11:32 +08:00 · 12-S7ZZM / 11-S7 GenericParamConstraint TypeSpec root retention ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：TypeSpec retention/count/remap/copy helper 现在接收 GenericParam/GenericParamConstraint 上下文；retained
  `GenericParamConstraint.constraintTypeToken` 即使没有 retained `TYPE_SPEC` token record，也能作为 TypeSpec row
  保留根。对应 TypeSpec row/signature blob 会被保留并压缩，constraint token 同步发布 compacted `TYPE_SPEC` RID。
  RED/GREEN：RED 新增 constraint-rooted TypeSpec fixture：source `TYPE_SPEC` RID2 只被 retained GenericParamConstraint
  引用，旧 pruner 先删除 TypeSpec row，`prepare_embedded_zrp_metadata` 返回 false（`Expected TRUE Was FALSE`）；GREEN 后
  row/token/signature blob 均保留并压缩，source contract 锁定 TypeSpec helper 调用
  `backend_aot_c_zrp_remap_generic_param_constraint_row(&constraintRow, ...)`。
  验证：WSL GCC direct zrp metadata pruning 9/0、TypeSpec pruning 2/0、pool pruning 6/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 9/0、2/0、6/0、8/0、24/0，focused metadata CTest
  4/4；Windows MSVC Debug direct 同 set 9/0、2/0、6/0、8/0、24/0，focused metadata CTest 4/4；`git diff --check` focused
  code files 无 whitespace error，仅有仓库 CRLF normalization warning。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzm-generic-param-constraint-typespec-root-retention.md`。
  备注：本记录只关闭 retained GenericParamConstraint-rooted TypeSpec row/signature retention；cross-module target/provider
  binding、真实 export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer
  仍未完成。

- 2026-07-01 21:41:02 +08:00 · 12-S7ZZL / 11-S7 GenericParamConstraint TypeSpec token remap ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_copy_generic_param_constraints()` 现在在线程化 TypeSpec rows/count 后，对 retained
  `GenericParamConstraint.constraintTypeToken` 执行既有 `backend_aot_c_zrp_remap_type_spec_token()`，因此 TypeSpec 表
  剪枝压缩后约束行不再保留 source `TYPE_SPEC` RID 空洞。
  RED/GREEN：RED 先新增 retained generic method parameter constraint fixture：source `TYPE_SPEC` RID1 被删除、RID2 被保留
  并压缩为 RID1，旧 pruner 输出 constraint token RID2，WSL GCC zrp metadata pruning 8 tests / 1 failure，
  `Expected 117440513 Was 117440514`；GREEN 后同 fixture 通过，并由 source contract 锁定 constraint TypeSpec remap 调用。
  验证：WSL GCC direct zrp metadata pruning 8/0、TypeSpec pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang 同 direct set 8/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug
  direct 同 set 8/0、2/0、6/0、24/0，focused metadata CTest 4/4；`git diff --check` focused code files 无 whitespace
  error，仅有仓库 CRLF normalization warning。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzl-generic-param-constraint-typespec-remap.md`。
  备注：本记录只关闭 retained GenericParamConstraint `TYPE_SPEC` token remap；cross-module target/provider binding、真实 export
  manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 21:14:56 +08:00 · 12-S7ZZK / 11-S7 signature-token orphan rejection ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_remap_retained_signature_token()` 现在拒绝 retained row/token record 引用一个没有 retained
  signature token-record 发布的本地 `SIGNATURE` token；`backend_aot_c_prepare_embedded_zrp_metadata()` 在 pruning 失败时会
  释放已准备的 embedded metadata 输出，避免 false 返回后仍暴露 source blob/length。
  RED/GREEN：RED 先证明 retained MethodSpec row 携带 `SIGNATURE` RID11 但缺少对应 signature token-record 时旧 pruner
  仍返回 true，失败为 `Expected FALSE Was TRUE`；实现 orphan rejection 后又暴露失败清理缺口，`outMetadata.length`
  仍为 source blob 长度 659，失败为 `Expected 0 Was 659`；GREEN 后 orphan signature token 使 pruning fail-closed 且输出清空。
  验证：WSL GCC direct zrp metadata pruning 7/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang 同组 direct 7/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  zrp metadata pruning 7/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzk-signature-token-orphan-rejection.md`。
  备注：本记录只关闭 retained local `SIGNATURE` orphan token 拒绝和 pruning failure output cleanup；cross-module
  target/provider binding、真实 export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full
  trim analyzer 仍未完成。

- 2026-07-01 20:55:12 +08:00 · 12-S7ZZJ / 11-S7 retained SIGNATURE token RID compaction ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_metadata_signature.{h,c}` 新增 retained signature token remap helper，按经过
  member/TypeDef/TypeSpec/ModuleRef pruning 后仍保留的 signature token-record 顺序，把本地 `SIGNATURE` token 压缩为
  RID1..N；token-record copy 同步重写 `token/relatedToken/ownerToken/targetMetadataToken/targetSignatureToken` 中的
  retained signature token，MethodSpec copy 同步重写 row `token`。FieldDef signature、MethodSpec signature、AssemblyRef
  signature 和 TypeRef signature token records 不再保留 source RID 空洞。
  RED/GREEN：RED 为 WSL GCC direct zrp metadata pruning 6 tests 中 2 个失败，旧实现输出 source `SIGNATURE` RID9/RID11，
  新期望为 compacted RID1，失败为 `Expected 134217729 Was 134217737/134217739`；GREEN 后 signature token records 与
  MethodSpec row token 均发布 compacted local RIDs。
  验证：WSL GCC direct zrp metadata pruning 6/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang 同组 direct 6/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  zrp metadata pruning 6/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzj-signature-token-rid-compaction.md`。
  备注：本记录只关闭 retained local `SIGNATURE` token RID compaction；cross-module target/provider binding、真实 export
  manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 20:17:40 +08:00 · 12-S7ZZI / 11-S7 member-token remap retained-count guard ·
  状态：11-S7/12-S7 generated zrp metadata remap sidecar 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_remap_export_member_token()` 与 `backend_aot_c_zrp_member_token_remap_build()` 现在都重新
  计算实际 retained MethodDef count，并在该值与调用方传入的 `retainedMethodDefCount` 不一致时拒绝 direct remap 或
  remap sidecar publication；这避免 FieldDef compacted target token 因过大的 retained MethodDef count 跳过不存在的
  `MEMBER_DEF` RID。row 指针检查也改为基于 `methodCount`/`fieldCount`，避免用不可信 retained count 推断输入有效。
  RED/GREEN：RED 先后覆盖 direct export-token remap 与 sidecar build：实际只保留 1 个 MethodDef 但传入 retained count 2，
  旧代码均返回 true 并会让 FieldDef target 跳到 RID3，WSL GCC 均失败 `Expected FALSE Was TRUE`；GREEN 后 direct remap
  返回 false 且保留原 token，sidecar builder 返回 false 且 metadata remap state 保持清空，source contract 锁定 direct/helper
  与 sidecar 两侧 retained-count guard。
  验证：WSL GCC direct export-token remap 8/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 8/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  8/0、source contracts 24/0、registered remap CTest 1/1。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzi-member-token-remap-sidecar-retained-count-guard.md`。
  备注：本记录只关闭生成侧 export-token remap retained MethodDef count 一致性 guard；cross-module target/provider binding、
  真实 export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 19:57:11 +08:00 · 12-S7ZZH / 11-S7 member-token remap sidecar token-shape guard ·
  状态：11-S7/12-S7 generated zrp metadata remap sidecar 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_member_token_is_member_def()` 现在要求 token 非零、table 为 `MEMBER_DEF` 且 RID 非零；
  `backend_aot_c_zrp_member_token_remap_append()` 在写 entry 前同时校验 source/target token shape，避免 generated C
  在 runtime ABI validation 前发布非 member 或 RID 0 remap。export-token remap focused test 新增非 `MEMBER_DEF`
  source token 与 `MEMBER_DEF` RID 0 source token 两条拒绝路径，source contract 同步锁定 source/target shape guard。
  RED/GREEN：RED 1 为 retained MethodDef 使用 `TYPE_DEF` source token，旧 builder 返回 true，WSL GCC 失败
  `Expected FALSE Was TRUE`；RED 2 为 retained MethodDef 使用 `MEMBER_DEF` RID 0 source token，旧 shape helper 仍接受，
  WSL GCC 再次失败 `Expected FALSE Was TRUE`；GREEN 后两种非法 sidecar build 均返回 false 且 metadata remap state 清空。
  验证：WSL GCC direct export-token remap 6/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 6/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  6/0、source contracts 24/0、registered remap CTest 1/1。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzh-member-token-remap-sidecar-token-shape-guard.md`。
  备注：本记录只关闭生成侧 member-token remap sidecar token-shape guard；cross-module target/provider binding、真实
  export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 19:43:02 +08:00 · 12-S7ZZG / 11-S7 member-token remap sidecar source duplicate guard ·
  状态：11-S7/12-S7 generated zrp metadata remap sidecar 支撑子切片完成；07~12 总目标继续进行中，cross-module provider
  binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_member_token_remap_build()` 生成 retained member-token sidecar 时现在拒绝重复
  `sourceToken`；append helper 在写入前扫描既有 entries，防止 generated C 在 ABI validation 前发布一对多 remap。
  export-token remap focused test 新增 method/field 共用同一 `MEMBER_DEF` source token 的拒绝用例，source contract
  同步锁定 duplicate source 比较。
  RED/GREEN：RED 为同一 source member token 同时来自 retained MethodDef 与 FieldDef，旧 builder 生成重复 source table，
  WSL GCC 失败 `Expected FALSE Was TRUE`；GREEN 后 builder 返回 false，metadata remap 指针/count 保持清空。
  验证：WSL GCC direct export-token remap 4/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 4/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  4/0、source contracts 24/0、registered remap CTest 1/1；`git diff --check` exit 0，仅输出既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzg-member-token-remap-sidecar-source-duplicate-guard.md`。
  备注：本记录只关闭生成侧 member-token remap sidecar source 唯一性；cross-module target/provider binding、真实
  export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 19:27:51 +08:00 · 12-S7ZZF / 11-S7 member-token remap ABI duplicate validation ·
  状态：11-S7/12-S7 runtime ABI validation 支撑子切片完成；07~12 总目标继续进行中，cross-module provider binding、
  真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：root runtime 与 mirrored AOT runtime 现在校验 `SZrAotMemberTokenRemap` table 的唯一性；同一 table 内
  `sourceToken` 不能重复，`targetToken` 也不能重复。descriptor diagnostics 新增 duplicate source 与 duplicate target
  两个手写 bad descriptor 动态库，source contract 同步锁定 root/mirrored runtime duplicate guards 与诊断文本。
  RED/GREEN：RED 为合法 pointer/count 和合法 `MEMBER_DEF` token 形态但 source 或 target 重复的 bad descriptor，旧 runtime
  错误接受并执行，WSL GCC 两个新用例均失败 `Expected FALSE Was TRUE`；GREEN 后 validation 分别报出
  `member token remap duplicate sourceToken index=1 previousIndex=0 sourceToken=0x03000001` 与
  `member token remap duplicate targetToken index=1 previousIndex=0 targetToken=0x03000001`。
  验证：WSL GCC direct descriptor diagnostics 5/0、source contracts 24/0、focused CTest 4/4；WSL Clang direct
  descriptor diagnostics 5/0、source contracts 24/0、focused CTest 4/4；Windows MSVC Debug descriptor diagnostics
  5/0/5 ignored、source contracts 24/0、focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzf-member-token-remap-duplicate-validation.md`。
  备注：本记录只关闭 generated C member-token remap ABI 唯一性校验；cross-module target/provider binding、真实
  export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 19:04:27 +08:00 · 12-S7ZZE / 11-S7 member-token remap ABI entry validation ·
  状态：11-S7/12-S7 runtime ABI validation 支撑子切片完成；07~12 总目标继续进行中，cross-module provider binding、
  真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和更完整的
  runtime ABI drift/deopt coverage 仍未关闭。
  完成项目：root runtime 与 mirrored AOT runtime 现在校验 `SZrAotMemberTokenRemap` 每个 entry 的 token shape；
  `sourceToken/targetToken` 必须是非零 `MEMBER_DEF` token。descriptor diagnostics 新增手写 bad descriptor 动态库，
  source contract 同步锁定 root/mirrored runtime entry validator 与诊断文本。
  RED/GREEN：RED 为合法 pointer/count 的 bad descriptor 发布 `sourceToken=0x02000001`，旧 runtime 错误接受并执行，
  WSL GCC 失败 `Expected FALSE Was TRUE`；GREEN 后 validation 报出
  `member token remap entry invalid index=0 sourceToken=0x02000001 targetToken=0x03000001`。
  验证：WSL GCC direct descriptor diagnostics 3/0、source contracts 24/0、focused CTest 4/4；WSL Clang direct
  descriptor diagnostics 3/0、source contracts 24/0、focused CTest 4/4；Windows MSVC Debug descriptor diagnostics
  3/0/3 ignored、source contracts 24/0、focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zze-member-token-remap-entry-validation.md`。
  备注：本记录只关闭 generated C member-token remap ABI entry 形态校验；cross-module target/provider binding、真实
  export manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 18:43:00 +08:00 · 12-S7ZZD / 11-S7 member-token remap ABI publication ·
  状态：11-S7/12-S7 cross-module token publication 支撑子切片完成；07~12 总目标继续进行中，cross-module
  provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer 和
  runtime ABI drift deopt coverage 仍未关闭。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 12u` 并新增 `SZrAotMemberTokenRemap`；generated AOT C
  把 emitted zrp metadata pruning sidecar 发布为 `zr_aot_member_token_remaps[]`，同时写入
  `ZrAotCompiledModule` 与 `SZrAotCodeRegistration` 的 `memberTokenRemaps/memberTokenRemapCount`。runtime loader
  校验 descriptor/codeRegistration 表指针与数量一致，generated C marker 暴露 remap count 与逐项 source/target token。
  RED/GREEN：RED 为 MethodDef RID2->RID1 compact fixture 要求 generated C 出现 ABI remap 表和两处绑定，旧实现只改写
  method-token table；GREEN 后 source `0x03000002` -> target `0x03000001` 的 remap 表、marker 和 ABI 字段均输出。
  验证：WSL GCC direct code stripping 10/0、source contracts 24/0、export-token remap 3/0，focused CTest 4/4；
  WSL Clang direct source contracts 24/0、focused CTest 4/4；Windows MSVC Debug direct source contracts 24/0、
  focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzd-member-token-remap-abi-publication.md`。
  备注：本记录只关闭 generated C ABI remap publication；cross-module target/provider binding、真实 export
  manifest/table rewrite/publication、annotation policy、完整 metadata sweep 和 full trim analyzer 仍未完成。

- 2026-07-01 18:09:50 +08:00 · 12-S7ZZC / 11-S7 signature blob embedded TypeDef token rewrite ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export
  manifest/table publication/rewrite、cross-module target/provider binding、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime
  ABI drift deopt coverage 仍未关闭。
  完成项目：retained signature blob pool copy 后新增 pruning-time rewrite pass，递归扫描 retained `METHOD_SIG`、`FIELD_SIG`
  与 standalone type-node signature blob，把 direct `TYPE_DEF` node 的 embedded token 改写为 compacted TypeDef RID；该 pass
  复用 TypeDef retained-row/compacted-token 规则，重写后再次结构校验，并让 token record signature hash 从最终 blob bytes 重新计算。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c` 新增 retained FieldDef signature blob，旧实现仍输出
  embedded source RID2，WSL GCC 失败 `Expected 33554433 Was 33554434`；GREEN 后 embedded token 输出 compacted `TYPE_DEF`
  RID1，已有 MethodSpec `MEMBER_REF` method-token rewrite 路径保持可叠加。
  验证：WSL GCC direct TypeDef pruning 2/0；WSL Clang direct TypeDef pruning 2/0；Windows MSVC Debug direct TypeDef pruning 2/0；
  focused CTest `zrp_metadata|aot_c_zrp_metadata|metadata_module_hash` 在 WSL GCC、WSL Clang、Windows MSVC Debug 均为 8/8。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzc-signature-typedef-token-rewrite.md`。
  备注：本记录只关闭 retained signature blob 内 direct TypeDef token 的 compaction rewrite；TypeRef/provider binding、
  cross-module export manifest/table publication、完整 metadata sweep、annotation policy 和 full trim analyzer 仍未完成。

- 2026-07-01 17:44:55 +08:00 · 12-S7ZZB / 11-S7 TypeDef RID compaction ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export
  manifest/table publication/rewrite、cross-module target/provider binding、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime
  ABI drift deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_metadata_type_def.{h,c}` 从 retained prefix 模型升级为 retained row count/compacted token/remap/copy
  helper；token-record count/copy 在 member-token pruning 后、TypeSpec/ModuleRef remap 前重写 direct `TYPE_DEF`
  tokens；MethodDef.ownerTypeToken、FieldDef.ownerTypeToken、GenericParam.ownerToken 和
  GenericParamConstraint.constraintTypeToken 的 direct TypeDef 引用同步重写；string/signature pool collection 改为基于实际 retained
  TypeDef row，而不是保留前 N 行。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c` 构造 source TypeDef RID1 orphan、RID2 live 的
  interior-hole fixture；旧 retained-prefix 模型 identity-exit，失败为 pruned metadata `Expected Non-NULL`。GREEN 后 live TypeDef
  从 source RID2 发布为 compacted `TYPE_DEF` RID1，tokenRecords/MethodDef owner 均指向 compacted RID，orphan type strings 不再进入
  stringPool。
  验证：Windows MSVC Debug direct runs：TypeDef pruning 1/0、direct zrp pruning 6/0、TypeSpec pruning 2/0、pool pruning 6/0、source
  contracts 24/0；WSL GCC 与 WSL Clang 均完成配置、focused target builds 和同五个 direct runs，结果分别为 1/0、6/0、2/0、6/0、24/0；
  `git diff --check` exit 0，仅有既有 LF/CRLF warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzb-typedef-rid-compaction.md`。
  备注：本记录只关闭 TypeDef interior RID compaction 与 direct token/row-owner rewrite；cross-module export manifest/table
  rewrite/publication、cross-module target/provider binding、完整 metadata sweep/pruning、annotation policy 和 full trim analyzer 仍未完成。

- 2026-07-01 17:10:03 +08:00 · 12-S7ZZA / 11-S7 TypeDef trailing orphan sweep ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export manifest/table
  publication/rewrite、cross-module target/provider binding、TypeDef RID compaction、完整 metadata sweep/pruning、完整 trim
  analyzer 和 runtime ABI drift deopt coverage 仍未关闭。
  完成项目：新增 `backend_aot_c_zrp_metadata_type_def.{h,c}`，按 retained token record、retained MethodDef owner、
  FieldDef owner 和 TypeDef-owned GenericParam root 计算 retained TypeDef prefix；pruned header、TypeDef copy、string-pool
  collection 和 signature-pool collection 均使用该 prefix count，尾部无 root 的 TypeDef row 及其 type name/namespace
  strings 不再进入 after-trim metadata；source contracts 与 Windows 手工 target source list 同步新 helper 边界。
  RED/GREEN：RED 为 direct zrp pruning fixture 构造 live TypeDef/MethodDef 加 trailing orphan TypeDef，旧实现 raw-copy
  TypeDef section 和 orphan strings，长度为 580 而非期望 510；GREEN 后 TypeDef count 从 2 收缩到 1，stringPool 只保留
  `LiveType\0Example\0Kept\0`。
  验证：Windows MSVC Debug direct runs：direct zrp pruning 6/0、pool pruning 6/0、TypeSpec pruning 2/0、export-token remap
  3/0、zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest
  6/6；WSL GCC/Clang focused builds 均通过，focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup
  contracts 1/0；`git diff --check` exit 0，仅有既有 LF/CRLF warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zza-typedef-trailing-orphan-sweep.md`。
  备注：本记录只关闭 TypeDef 后缀孤儿行清理；为保持现有 TypeDef RID 稳定，interior TypeDef hole/RID compaction、
  TypeDef token-reference rewrite、cross-module export manifest/table rewrite/publication、cross-module target/provider
  binding、完整 metadata sweep/pruning、annotation policy 和 full trim analyzer 仍未完成。

- 2026-07-01 16:40:57 +08:00 · 12-S7ZZ / 11-S7 ModuleRef orphan sweep ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export manifest/table
  publication/rewrite、cross-module target/provider binding、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime ABI drift
  deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_metadata_module_ref.{h,c}` 新增 ModuleRef retention/count/compacted-token/remap helper；
  pruning layout 和 ModuleRef copy 阶段只保留仍被 retained `TYPE_REF`/`MEMBER_REF` token record 引用的 `ASSEMBLY_REF` row；
  retained ModuleRef row token、token-record AssemblyRef 字段和 name/version string offsets 均重写到 compacted metadata；
  string/signature pool collection 复用同一 ModuleRef-aware retention，移除 self-only orphan AssemblyRef/signature 记录和 orphan
  module identity strings。
  RED/GREEN：RED 为 pool-pruning fixture 构造 orphan/live 两个 ModuleRef rows，旧 pruner raw-copy ModuleRef section 并 identity-exit，
  首先失败为 pruned owned blob `Expected Non-NULL`；GREEN 后 orphan ModuleRef row 和两个 self-only token records 被删除，live
  `ASSEMBLY_REF` 从 RID2 compact 为 RID1。
  验证：Windows MSVC Debug direct tests 通过 `aot_c_zrp_metadata_pool_pruning` 6/0、`aot_c_zrp_metadata_pruning` 5/0、
  `aot_c_zrp_metadata_typespec_pruning` 2/0、`aot_c_zrp_metadata_export_token_remap` 3/0、`aot_c_zrp_metadata_size_deltas` 2/0、
  `aot_c_code_stripping` 10/0、`aot_c_source_contracts` 24/0 和 `aot_c_frame_setup_contracts` 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，source/frame direct runs 各 24/0 和 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zz-module-ref-orphan-sweep.md`。
  备注：本记录只关闭 ModuleRef orphan sweep 与 retained AssemblyRef token RID compaction；不声明 cross-module export
  manifest/table rewrite/publication、cross-module target/provider binding、完整 metadata sweep/pruning、annotation policy 或 full
  trim analyzer 完成。

- 2026-07-01 16:14:53 +08:00 · 12-S7ZY / 11-S7 TypeSpec RID compaction ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export manifest/table
  publication/rewrite、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime ABI drift deopt coverage 仍未关闭。
  完成项目：`backend_aot_c_zrp_metadata_type_spec.{h,c}` 新增 retained TypeSpec compacted-token/remap helper；pruned
  token-record count/copy 在 member-token remap 后继续重写 TypeSpec token 字段，retained TypeSpec row token 也按 compacted row
  RID 发布；signature-pool collection 复用同一 TypeSpec-aware token-record retention，避免 pruned TypeSpec token record 继续保留
  orphan signature bytes。
  RED/GREEN：RED 为 TypeSpec pruning focused test 构造两个 TypeSpec rows，第一条被 pruning、第二条 retained，旧实现仍发布 source
  `0x07000002u`，失败为 `Expected 117440513 Was 117440514`；GREEN 后 retained row/token record 均发布 compacted
  `0x07000001u`。
  验证：Windows MSVC Debug direct tests 通过 `aot_c_zrp_metadata_typespec_pruning` 2/0、`aot_c_zrp_metadata_pruning` 5/0、
  `aot_c_zrp_metadata_pool_pruning` 5/0、`aot_c_zrp_metadata_export_token_remap` 3/0、`aot_c_zrp_metadata_size_deltas` 2/0、
  `aot_c_code_stripping` 10/0、`aot_c_source_contracts` 24/0 和 `aot_c_frame_setup_contracts` 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，source/frame direct runs 各 24/0 和 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zy-typespec-rid-compaction.md`。
  备注：本记录只关闭 retained TypeSpec row/token-record RID compaction；不声明 cross-module export manifest/table
  rewrite/publication、cross-module target/provider binding、完整 metadata sweep/pruning、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 15:57:26 +08:00 · 12-S7ZX / 11-S7 published export member-token remap ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export manifest/table
  publication/rewrite、TypeSpec RID compaction、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime ABI drift deopt
  coverage 仍未关闭。
  完成项目：`.zrp` pruning 结果新增 retained MethodDef/FieldDef `sourceToken -> targetToken` sidecar；
  `backend_aot_c_zrp_metadata_member_token.{h,c}` 负责构建、查询和释放 member-token remap；generated AOT C
  `zr_aot_method_tokens[]` 通过 sidecar 发布 compacted MethodDef token，pruned/missing member token 回退为 `0u`，避免导出
  method 继续写出 stale pre-prune RID。
  RED/GREEN：RED 为 export-token remap focused test 引入缺失的 member-token helper 后 MSVC build 失败，并由 code-stripping
  generated-C 断言锁定旧输出会发布 `0x03000002u`；GREEN 后 helper、sidecar 生命周期、method-token writer remap 和 contracts
  通过。
  验证：Windows MSVC Debug direct tests 通过 `aot_c_zrp_metadata_export_token_remap` 3/0、`aot_c_code_stripping` 10/0、
  `aot_c_source_contracts` 24/0、`aot_c_frame_setup_contracts` 1/0、`aot_c_zrp_metadata_pruning` 5/0、
  `aot_c_zrp_metadata_typespec_pruning` 1/0 和 `aot_c_zrp_metadata_pool_pruning` 5/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，source/frame direct runs 各 24/0 和 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zx-published-export-member-token-remap.md`。
  备注：本记录只关闭 generated AOT C method-token table 的 retained member-token compaction；不声明 cross-module export
  manifest/table rewrite/publication、TypeSpec RID compaction、cross-module target/provider binding、完整 metadata sweep/pruning、
  annotation policy 或 full trim analyzer 完成。

- 2026-07-01 15:30:06 +08:00 · 12-S7ZW / 11-S7 TypeSpec orphan sweep ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export-token
  publication/rewrite、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime ABI drift deopt coverage 仍未关闭。
  完成项目：AOT C zrp metadata pruner 新增 `backend_aot_c_zrp_metadata_type_spec.{h,c}`，以 retained `TYPE_SPEC`
  token record 为 root 统计和复制 TypeSpec rows，丢弃 token-record pruning 后失去 live token record 的 TypeSpec rows，
  并把 retained TypeSpec signature blob offsets 重写到 compacted signatureBlobPool；signature-pool collection 只收集
  retained TypeSpec rows 的 blobs，source contracts 锁定新 helper 边界。
  RED/GREEN：RED 为 `aot_c_zrp_metadata_typespec_pruning` 期望 orphan TypeSpec row/signature blob 消失，但 pruned blob
  仍为 517 bytes；GREEN 后 TypeSpec pruning、direct zrp pruning、pool pruning、source contracts、code stripping、
  zrp size deltas 和 export-token remap focused set 通过。
  验证：Windows MSVC Debug direct tests 通过 `aot_c_zrp_metadata_typespec_pruning` 1/0、`aot_c_zrp_metadata_pruning` 5/0、
  `aot_c_zrp_metadata_pool_pruning` 5/0、`aot_c_source_contracts` 24/0、`aot_c_code_stripping` 10/0、
  `aot_c_zrp_metadata_size_deltas` 2/0 和 `aot_c_zrp_metadata_export_token_remap` 2/0；WSL GCC/Clang 同组 build/direct
  run 通过；focused CTest 三套环境均为 6/6；`git diff --check` exit 0，仅输出既有 line-ending conversion warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zw-typespec-orphan-sweep.md`。
  备注：本记录只关闭 token-record-rooted TypeSpec row/signature-pool orphan sweep；不声明 TypeSpec RID compaction、
  cross-module export-token publication/rewrite、完整 zrp metadata sweep/pruning、annotation-driven policy 或 full trim
  analyzer 完成。

- 2026-07-01 14:51:47 +08:00 · 12-S7ZV / 11-S7 FieldDef default-value constant-pool remap ·
  状态：11-S7/12-S7 emitted zrp metadata pruning 支撑子切片完成；07~12 总目标继续进行中，cross-module export-token
  publication/rewrite、完整 metadata sweep/pruning、完整 trim analyzer 和 runtime ABI drift deopt coverage 仍未关闭。
  完成项目：`.zrp` data metadata 版本提升到 v3，`SZrZrpMetadataFieldDefRow` 新增
  `defaultValueConstantPoolOffset/defaultValueConstantPoolLength` 并由 zrp validator 校验 constant-pool slice；AOT C
  metadata pruner 新增 `backend_aot_c_zrp_metadata_constant_pool.{h,c}`，保留 FieldDef default-value 引用的 constant-pool
  slices、重写 retained FieldDef offsets、丢弃 orphan constant payload，并把 source contracts/CLI version expectations
  同步到新 ABI。
  RED/GREEN：RED 为 focused MSVC build 因 FieldDef row 缺少 default-value constant-pool 字段而编译失败；GREEN 后
  metadata format、pool pruning、direct zrp pruning、source contracts、code stripping、zrp size deltas、CLI dump/version
  和 CLI args focused set 通过。
  验证：Windows MSVC Debug direct tests 通过 `zrp_metadata_format` 12/0、`aot_c_zrp_metadata_pool_pruning` 5/0、
  `aot_c_zrp_metadata_pruning` 5/0、`aot_c_source_contracts` 24/0、`aot_c_code_stripping` 10/0、
  `aot_c_zrp_metadata_size_deltas` 2/0、CLI dump/version 与 CLI args；WSL GCC/Clang 同组 build/direct run 通过；
  focused CTest 三套环境均为 7/7；`git diff --check` exit 0，仅输出既有 line-ending conversion warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zv-field-default-constant-pool-remap.md`。
  备注：本记录只关闭 FieldDef default-value backed constant-pool retained-slice remap；不声明跨模块 target、
  cross-module export-token publication/rewrite、完整 zrp metadata sweep/pruning、annotation-driven policy 或 full trim
  analyzer 完成。

- 2026-07-01 14:05:25 +08:00 · 10-S4Z28 / 11-S4BN / 12-S5 support FieldInfo nested primitive POD storage-width path matrix ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested primitive path storage-width matrix coverage 子切片完成；
  07~12 总目标继续进行中，完整 signature-derived binding、完整 FieldInfo 行为、`@dynamically_accessed` dataflow 和完整
  metadata sweep 仍未关闭。
  完成项目：focused fixture 在同一 `{0,0}` nested primitive path 上覆盖 int8、int16、int64、uint8、uint16、uint64、float32
  raw child read/write，复用 S4Z25/S4Z26/S4Z27 的 object adapter、nested traversal、leaf layout identity guard 和 shared
  primitive POD guard。未新增 public API 或 production code。
  RED/GREEN：coverage GREEN；新增 storage-width matrix 后 `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z28-fieldinfo-nested-primitive-pod-width-matrix.md`。
  备注：本记录不声明 signature-derived binding、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion
  或完整 metadata sweep 完成。

- 2026-07-01 14:00:03 +08:00 · 10-S4Z27 / 11-S4BM / 12-S5 support FieldInfo nested primitive POD representative path matrix ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested primitive path representative matrix coverage 子切片完成；
  07~12 总目标继续进行中，完整 primitive raw child matrix/signature-derived binding、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：focused fixture 在同一 `{0,0}` nested primitive path 上覆盖 bool、uint32、double raw child read/write，
  复用 S4Z25/S4Z26 的 object adapter、nested traversal、leaf layout identity guard 和 shared primitive POD guard。未新增
  public API 或 production code。
  RED/GREEN：coverage GREEN；新增矩阵覆盖后 `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z27-fieldinfo-nested-primitive-pod-path-matrix.md`。
  备注：本记录不声明完整 primitive 宽度矩阵、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion
  或完整 metadata sweep 完成。

- 2026-07-01 13:51:37 +08:00 · 10-S4Z26 / 11-S4BL / 12-S5 support FieldInfo nested primitive POD leaf layout identity guard ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested primitive raw child leaf guard 子切片完成；
  07~12 总目标继续进行中，完整 primitive raw child matrix/signature-derived binding、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：runtime reflection 的 nested primitive path read/write leaf 现在要求
  `SZrTypeLayoutField.typeLayoutIndex == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`，避免把仍指向 registered child layout 的
  inline aggregate child 误解释为 raw primitive POD child。focused fixture 覆盖 leaf `typeLayoutIndex = 42u` 时 read/write
  均拒绝，并验证 rejected write 保留原始 INT32 字节。
  RED/GREEN：RED 为 Windows MSVC Debug focused `reflection_token_resolve` 30 tests / 1 failure，
  `Expected FALSE Was TRUE`；GREEN 后 `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z26-fieldinfo-nested-primitive-pod-leaf-layout-guard.md`。
  备注：本记录不声明完整 primitive 宽度矩阵、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion
  或完整 metadata sweep 完成。

- 2026-07-01 13:37:45 +08:00 · 10-S4Z25 / 11-S4BK / 12-S5 support FieldInfo nested inline primitive POD path read/write ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested inline primitive raw child multi-level path read/write
  first-contract 子切片完成；07~12 总目标继续进行中，完整 primitive raw child matrix/signature-derived binding、
  完整 FieldInfo 行为、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue()` /
  `ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue()` 及 object adapters；runtime reflection 从 FieldInfo
  identity/FieldDef token 恢复 retained owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved field type layout，
  按 `nestedFieldIndices[]` 逐级解析 `SZrTypeLayoutField.typeLayoutIndex`，并在 leaf raw child 上复用
  `reflection_field_value_primitive.{h,c}` shared primitive POD guard 读写代表性 INT32。focused fixture 覆盖合法两级 `{0,0}`
  path、short storage、zero-length path、final child out-of-range、missing intermediate layout、中间 VALUE_SLOT/GC/ownership
  flags、leaf VALUE_SLOT flag、byte-size mismatch 和 write type mismatch 拒绝。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（缺少 nested primitive path object read/write API，C4013 + LNK2019）；
  GREEN 后 `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 30/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z25-fieldinfo-nested-primitive-pod-path-read-write.md`。
  备注：本记录只关闭 retained metadata 下代表性 INT32 nested primitive raw child path read/write 的最小边界；不声明
  完整 primitive 宽度矩阵、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 13:14:27 +08:00 · 10-S4Z24 / 11-S4BJ / 12-S5 support FieldInfo nested inline VALUE_SLOT path write ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested inline field multi-level path write first-contract
  子切片完成；07~12 总目标继续进行中，primitive raw child marshaling、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue()` 与
  `ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue()`；runtime reflection 从 FieldInfo identity/FieldDef token 恢复
  retained owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved field type layout，按 `nestedFieldIndices[]`
  逐级解析 `SZrTypeLayoutField.typeLayoutIndex`，并在 leaf `VALUE_SLOT` child 上写入 `SZrTypeValue`。focused fixture 覆盖
  合法两级 `{0,0}` path、short storage、zero-length path、final child out-of-range、missing intermediate layout 和中间
  GC/ownership flags 拒绝，并验证旧 unique-owned nested value replacement/drop。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（缺少
  `ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue` / LNK2019）；GREEN 后 `reflection_token_resolve` 29/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 29/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z24-fieldinfo-nested-value-slot-path-write.md`。
  备注：本记录只关闭 retained metadata 下 multi-level nested value-slot path write 的最小边界；不声明
  primitive raw child read/write、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 13:00:18 +08:00 · 10-S4Z23 / 11-S4BI / 12-S5 support FieldInfo nested inline VALUE_SLOT path read ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested inline field multi-level path read first-contract
  子切片完成；07~12 总目标继续进行中，nested path write、primitive raw child marshaling、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue()` 与
  `ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue()`；runtime reflection 从 FieldInfo identity/FieldDef token 恢复
  retained owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved field type layout，按 `nestedFieldIndices[]`
  逐级解析 `SZrTypeLayoutField.typeLayoutIndex`，并在 leaf `VALUE_SLOT` child 上复制 `SZrTypeValue`。新增
  `reflection_field_value_nested.{h,c}` 拆出 recursive nested inline layout traversal helper。focused fixture 覆盖合法两级
  `{0,0}` path、zero-length path、final child out-of-range、missing intermediate layout 和中间 child GC/ownership flags 拒绝。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 28/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 28/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z23-fieldinfo-nested-value-slot-path-read.md`。
  备注：本记录只关闭 retained metadata 下 multi-level nested value-slot path read 的最小边界；不声明 path write、
  primitive raw child read/write、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。
  `reflection_field_value.c` 一度超过 1100 行，已把 nested traversal helper 拆出，当前约 935 行；大型反射 token 测试文件
  后续最小拆分边界为独立 FieldInfo inline-storage fixture/test target。

- 2026-07-01 12:40:09 +08:00 · 10-S4Z22 / 11-S4BH / 12-S5 support FieldInfo nested inline VALUE_SLOT write ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested inline field write first-contract 子切片完成；
  07~12 总目标继续进行中，完整 recursive path API、primitive raw child marshaling、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_WriteFieldInfoTokenNestedValue()` 与
  `ZrCore_Reflection_WriteFieldInfoObjectNestedValue()`；runtime reflection 从 FieldInfo identity/FieldDef token 恢复
  retained owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved field type layout，按
  `SZrTypeLayoutField` index 写入 nested `VALUE_SLOT` child，并用 `ZrCore_Value_Copy()` 执行 replacement/drop。新增
  focused fixture 覆盖 short storage、out-of-range index、外层 GC/ownership flags 拒绝，以及合法 nested child write
  把 unique-owned old string 释放到 strong ref 0、destination slot 替换为 plain new string、ownership metadata 归一为空。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 27/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 27/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z22-fieldinfo-nested-value-slot-write.md`。
  备注：本记录只关闭 retained metadata 下 nested value-slot child write 的最小 layout-index 边界；不声明 full recursive
  nested marshaling、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。
  `reflection_field_value.c` 现为约 915 行，仍保持单一 FieldInfo value boundary 责任，本切片暂不拆分；后续最小生产拆分边界为
  nested field layout read/write helper。`tests/module/test_reflection_token_resolve.c` 已是大型单一反射 token 测试文件；
  本次为同一 FieldInfo inline-storage fixture 的窄切片，未拆分以避免阶段切片外结构性 churn，后续最小测试拆分边界应是
  独立 `FieldInfo` inline-storage fixture/test target。

- 2026-07-01 12:24:56 +08:00 · 10-S4Z21 / 11-S4BG / 12-S5 support FieldInfo nested inline VALUE_SLOT read ·
  状态：最小 FieldDef token `FieldInfo` object 的 recursive nested inline field read first-contract 子切片完成；
  07~12 总目标继续进行中，完整 recursive path API、nested write、primitive raw child marshaling、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_ReadFieldInfoTokenNestedValue()` 与
  `ZrCore_Reflection_ReadFieldInfoObjectNestedValue()`；runtime reflection 从 FieldInfo identity/FieldDef token 恢复
  retained owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved field type layout，按
  `SZrTypeLayoutField` index 读取 nested `VALUE_SLOT` child，并用 `ZrCore_Value_Copy()` 返回 `SZrTypeValue`。新增
  focused fixture 覆盖 short storage、out-of-range index、外层 GC/ownership flags 拒绝，以及合法 nested child read
  返回 int `314159`。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 26/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 26/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z21-fieldinfo-nested-value-slot-read.md`。
  备注：本记录只关闭 retained metadata 下 nested value-slot child read 的最小 layout-index 边界；不声明 full recursive
  nested marshaling、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。
  `tests/module/test_reflection_token_resolve.c` 已是大型单一反射 token 测试文件；本次为同一 FieldInfo inline-storage
  fixture 的窄切片，未拆分以避免阶段切片外结构性 churn，后续最小拆分边界应是独立 `FieldInfo` inline-storage fixture/test target。

- 2026-07-01 12:09:54 +08:00 · 10-S4Z20 / 11-S4BF / 12-S5 support FieldInfo inline aggregate replacement/drop borrowed-source write coverage ·
  状态：最小 FieldDef token `FieldInfo` object 的 inline aggregate replacement/drop coverage 子切片完成；
  07~12 总目标继续进行中，完整 nested field 解构/递归 marshaling、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 object-level FieldInfo inline aggregate fixture，构造 `FIELD_COPY/FIELD_DROP` field type layout，
  其 nested field 为 `VALUE_SLOT | GC_VALUE | OWNERSHIP_VALUE` 的 `SZrTypeValue`；destination 先持有 unique-owned
  old string，native-pointer source 提供无 ownership 的 new string。写入后验证 old owner strong ref 从 1 降到 0、
  destination slot 被替换为 new string 且 ownership metadata 归一为空，new source 仍未被持有。生产代码未改动，
  当前行为复用 10-S4Z19 的 `ZrCore_TypeLayout_CopyInline()` 路径和 central `ZrCore_Value_Copy()` replacement/drop 语义。
  RED/GREEN：coverage GREEN；新增覆盖后 Windows MSVC Debug focused `reflection_token_resolve` 直接通过 25/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 25/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z20-fieldinfo-inline-aggregate-replacement-drop-write.md`。
  备注：本记录只补齐 retained metadata 下 field-copy inline aggregate 写入覆盖的 replacement/drop 防退化；
  不声明 recursive field access API、完整 `FieldInfo.SetValue` method surface、trim analyzer、DESCRIPTION promotion
  或完整 metadata sweep 完成。`tests/module/test_reflection_token_resolve.c` 已是大型单一反射 token 测试文件；
  本次为同一 fixture 的 coverage-only 补强，未拆分以避免阶段切片外结构性 churn，后续最小拆分边界应是
  独立 `FieldInfo` inline-storage fixture/test target。

- 2026-07-01 11:52:49 +08:00 · 10-S4Z19 / 11-S4BE / 12-S5 support FieldInfo inline aggregate field-copy borrowed-source write ·
  状态：最小 FieldDef token `FieldInfo` object 的 nested inline field marshaling support 子切片完成；
  07~12 总目标继续进行中，完整 nested field 解构/递归 marshaling、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo object write adapter 现在可对 non-GC/non-ownership inline struct/union 字段复用
  `ZrCore_TypeLayout_CopyInline()`；当 resolved field type layout 可 raw copy 时走 raw-copy 分支，
  当 `copyKind == ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY` 时走逐字段 copy。source 仍必须是非空
  `ZR_VALUE_TYPE_NATIVE_POINTER`，且必须先通过 S4Z17 的 inline aggregate borrowed-view 判定。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 把 inline aggregate non-blittable field-copy
  source 写入期望改为成功后失败 1/24（`Expected TRUE Was FALSE`）；GREEN 后 Windows focused
  `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z19-fieldinfo-inline-aggregate-field-copy-write.md`。
  备注：本记录只关闭 retained metadata 下的 layout-aware borrowed-source write 支撑边界；不声明完整
  `FieldInfo.SetValue` method surface、destination ownership replacement/drop lifecycle、trim analyzer、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:34:10 +08:00 · 10-S4Z18 / 11-S4BD / 12-S5 support FieldInfo inline aggregate borrowed-source write ·
  状态：最小 FieldDef token `FieldInfo` object 的 nested inline field marshaling second-contract 子切片完成；
  07~12 总目标继续进行中，完整 nested field 解构、non-blittable struct semantics、完整 FieldInfo 行为、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo object write adapter 现在可通过 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 resolved
  field type layout，对 non-GC/non-ownership、blittable inline struct/union 字段接受非空
  `ZR_VALUE_TYPE_NATIVE_POINTER` source，并按 field byte size 复制到调用方 inline storage；scalar/null source、
  non-blittable layout 和 GC/ownership flagged 字段仍拒绝。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 在 inline struct fixture 增加 native-pointer source
  写入期望后失败 1/24；GREEN 后 Windows focused `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z18-fieldinfo-inline-aggregate-borrowed-source-write.md`。
  备注：本记录只关闭 blittable aggregate borrowed-source byte copy；不声明完整 `FieldInfo.SetValue` method surface、
  trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:26:41 +08:00 · 10-S4Z17 / 11-S4BC / 12-S5 support FieldInfo inline struct borrowed view ·
  状态：最小 FieldDef token `FieldInfo` object 的 nested inline field marshaling first-contract 子切片完成；
  07~12 总目标继续进行中，完整 nested field 解构/写回、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo object read adapter 现在可通过 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 resolved
  field type layout，对 non-GC/non-ownership inline struct/union 字段返回 borrowed `ZR_VALUE_TYPE_NATIVE_POINTER`
  view，指向调用方 inline storage 中的字段地址；write path 仍拒绝非 primitive/非 VALUE_SLOT 字段。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 新增 inline struct object fixture 后失败 1/24；
  GREEN 后 Windows focused `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z17-fieldinfo-inline-struct-borrowed-view.md`。
  备注：本记录只关闭 borrowed-view read boundary；不声明完整 `FieldInfo.SetValue` method surface、
  trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:15:23 +08:00 · 10-S4Z16 / 11-S4BB / 12-S5 support FieldInfo object primitive POD coverage ·
  状态：最小 FieldDef token `FieldInfo` object 的 primitive POD raw inline read/write 覆盖子切片完成；
  07~12 总目标继续进行中，nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 focused fixture 通过 object-level FieldInfo read/write adapter 读取 int32 raw primitive field、
  拒绝 bool 写入并验证 raw bytes 保持、再写入并读回 int `4096`；生产代码复用既有 object adapter 与 token-driven
  primitive POD path，未新增 API。
  RED/GREEN：coverage GREEN；Windows focused `reflection_token_resolve` 23/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 23/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z16-fieldinfo-object-primitive-pod.md`。
  备注：本记录只补 object-level primitive POD coverage；不声明完整 `FieldInfo.SetValue` method surface、
  trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:06:55 +08:00 · 10-S4Z15 / 11-S4BA / 12-S5 support FieldInfo object value write adapter ·
  状态：最小 FieldDef token `FieldInfo` object 的 object-level inline value write adapter 子切片完成；
  07~12 总目标继续进行中，nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_WriteFieldInfoObjectValue()`，从 `FieldInfo.metadataRuntime` native pointer
  与 `metadataToken` int carrier 取出 same-runtime FieldDef context，再委托既有 token-driven write path；
  focused fixture 覆盖 null/short-storage 负向路径、失败后原值保持和成功写入 inline `VALUE_SLOT` int。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_WriteFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 22/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 22/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z15-fieldinfo-object-value-write.md`。
  备注：本记录只关闭 object-level write adapter；不声明完整 `FieldInfo.SetValue` method surface、
  trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 10:52:48 +08:00 · 10-S4Z14 / 11-S4AZ / 12-S5 support FieldInfo object value read adapter ·
  状态：最小 FieldDef token `FieldInfo` object 的 read-only object-level inline value adapter 子切片完成；
  07~12 总目标继续进行中，object-level write、nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_ReadFieldInfoObjectValue()`，从 `FieldInfo.metadataRuntime` native pointer
  与 `metadataToken` int carrier 取出 same-runtime FieldDef context，再委托既有 token-driven read path；
  focused fixture 覆盖 null/short-storage 负向路径和成功读回 inline `VALUE_SLOT` int。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_ReadFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 21/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 21/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z14-fieldinfo-object-value-read.md`。
  备注：本记录只关闭 read-only object adapter；不声明 `SetValue`、完整 FieldInfo method surface、
  trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 10:36:14 +08:00 · 10-S4Z13 / 11-S4AY / 12-S5 support FieldInfo metadata runtime carrier ·
  状态：最小 FieldDef token `FieldInfo` object 的 `metadataRuntime` same-runtime identity carrier 子切片完成；
  07~12 总目标继续进行中，nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：runtime reflection 现在在 FieldInfo object 上写入 attached `SZrMetadataRuntime *` native pointer，
  与 `metadataToken` 一起为后续 object-level FieldInfo 字段读写提供对象内 token context；不新增 public API 或 metadata ABI。
  RED/GREEN：RED 为 `metadataRuntime` native-pointer 断言后 Windows MSVC Debug focused run 失败 1/20；
  GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z13-fieldinfo-metadata-runtime-carrier.md`。
  备注：本记录只关闭 FieldInfo metadata runtime carrier；不声明完整 FieldInfo method surface、trim analyzer、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 10:21:28 +08:00 · 10-S4Z12 / 11-S4AX / 12-S5 support FieldInfo primitive POD float32 precision guard ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline float32 字段 precision/no-loss
  写入守卫子切片完成；07~12 总目标继续进行中，nested field marshaling、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：runtime reflection raw primitive 写入现在会在 float32 cast 和 `memcpy` 前拒绝不能经
  `TZrFloat32` 无损 round-trip 的 double source，并保证失败写入不改原始 inline storage bytes；测试覆盖
  `1.0 + FLT_EPSILON / 4.0` precision-loss source 和失败后原值保留。
  RED/GREEN：RED 为新增 precision-loss 写入用例后 Windows MSVC Debug focused run 失败 1/20；
  GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z12-fieldinfo-primitive-pod-float32-precision-guard.md`。
  备注：本记录只关闭 primitive POD float32 precision/no-loss guard；不声明 nested struct marshaling、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 10:11:20 +08:00 · 10-S4Z11 / 11-S4AW / 12-S5 support FieldInfo primitive POD float32 NaN guard ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline float32 字段 NaN 写入守卫子切片完成；
  07~12 总目标继续进行中，float32 precision semantics、nested field marshaling、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：runtime reflection raw primitive 写入现在会在 float32 cast 和 `memcpy` 前拒绝 NaN double source，
  并保证失败写入不改原始 inline storage bytes；测试覆盖 NaN source 和失败后原值保留。
  RED/GREEN：RED 为新增 NaN 写入用例后 Windows MSVC Debug focused run 失败 1/19；GREEN 后同一 focused run 19/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 19/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z11-fieldinfo-primitive-pod-float32-nan-guard.md`。
  备注：本记录只关闭 primitive POD float32 NaN guard；不声明 precision policy、nested struct marshaling、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 09:58:56 +08:00 · 10-S4Z10 / 11-S4AV / 12-S5 support FieldInfo primitive POD float32 range guard ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline float32 字段写入范围守卫子切片完成；
  07~12 总目标继续进行中，float32 NaN/precision semantics、nested field marshaling、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：runtime reflection raw primitive 写入现在会在 float32 cast 和 `memcpy` 前拒绝 `|double source| > FLT_MAX`
  的写入，并保证失败写入不改原始 inline storage bytes；测试覆盖正负两个越界方向。
  RED/GREEN：RED 为新增 float32 越界写入用例后 Windows MSVC Debug focused run 失败 1/18；GREEN 后同一 focused run 18/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 18/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z10-fieldinfo-primitive-pod-float32-range-guard.md`。
  备注：本记录只关闭 primitive POD float32 range guard；不声明 NaN/precision policy、nested struct marshaling、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 09:49:10 +08:00 · 10-S4Z9 / 11-S4AU / 12-S5 support FieldInfo primitive POD integer range guard ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline 整数字段写入范围守卫子切片完成；07~12
  总目标继续进行中，float32 narrowing/finite semantics、nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：runtime reflection raw primitive 写入现在按目标 primitive storage width 拒绝 signed/unsigned 越界写入，并保证失败
  写入不改原始 inline storage bytes；测试覆盖 int8 上/下界、uint8 负数/超上界和 int64 unsigned 超 signed max。
  RED/GREEN：RED 为新增越界写入用例后 Windows MSVC Debug focused run 失败 1/17；GREEN 后同一 focused run 17/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 17/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z9-fieldinfo-primitive-pod-integer-range-guard.md`。
  备注：本记录只关闭 primitive POD integer range guard；不声明 float32 narrowing/finite semantics、nested struct marshaling、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 09:35:02 +08:00 · 10-S4Z8 / 11-S4AT / 12-S5 support FieldInfo primitive POD width matrix ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline 字段读写 storage-width 矩阵子切片完成；
  07~12 总目标继续进行中，primitive numeric overflow/range semantics、nested field marshaling、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 focused width matrix 覆盖 int8、int16、int64、uint8、uint16、uint64、float32 raw inline FieldDef storage，
  经同一 `ZrCore_Reflection_ReadFieldInfoTokenValue()` / `WriteFieldInfoTokenValue()` API 读取/写入。生产代码无需改动，
  10-S4Z6 的 generic primitive path 已覆盖这些 storage widths。
  RED/GREEN：coverage GREEN；Windows MSVC Debug focused run 在新增宽度矩阵后通过 16/0，并修正测试 helper 中的
  unreachable-code warning。验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 16/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z8-fieldinfo-primitive-pod-width-matrix.md`。
  备注：本记录只关闭 primitive POD storage-width matrix 覆盖；不声明 numeric overflow/range semantics、nested struct
  marshaling、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 05:06:21 +08:00 · 10-S4Z7 / 11-S4AS / 12-S5 support FieldInfo primitive POD representative matrix ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline 字段读写代表性矩阵子切片完成；
  07~12 总目标继续进行中，全量 primitive 变体穷举、nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 focused matrix 覆盖 bool、uint32、double raw inline FieldDef storage，经同一
  `ZrCore_Reflection_ReadFieldInfoTokenValue()` / `WriteFieldInfoTokenValue()` API 读取/写入；bool case 覆盖 read true、
  reject int write、write false，uint32 case 覆盖 read `0xFEDC1234`、reject bool write、write `0xAABBCCDD`，double case
  覆盖 read `6.25`、reject bool write、write `-12.5`。生产代码无需改动，10-S4Z6 的 generic primitive path 已覆盖该代表矩阵。
  RED/GREEN：coverage GREEN；Windows MSVC Debug focused run 在新增矩阵后直接通过 15/0。验证：WSL GCC/Clang/Windows
  MSVC Debug 均通过 `reflection_token_resolve` 15/0、`metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout`
  17/0；focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z7-fieldinfo-primitive-pod-matrix.md`。
  备注：本记录只关闭代表性 primitive POD matrix 覆盖；不声明 nested struct marshaling、完整 FieldInfo method surface、
  trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 04:45:19 +08:00 · 10-S4Z6 / 11-S4AR / 12-S5 support FieldInfo primitive POD read/write boundary ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven primitive POD raw inline 字段读写边界子切片完成；07~12 总目标继续
  进行中，broader primitive matrix、nested field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `zr_vm_core/src/zr_vm_core/reflection_field_value.c` 作为 FieldInfo token value boundary；read/write API
  共用 FieldDef token owner-field resolver，`VALUE_SLOT` 继续复制 `SZrTypeValue`，raw primitive POD 路径读取
  `FIELD_SIG(PRIMITIVE(...))`、校验 field flags/range/exact byte size，并用 `memcpy` 读写 raw scalar。focused fixture 覆盖
  int32 raw inline storage：读 `-12345`、拒绝 bool 写、写 `2048` 并读回。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 primitive POD read/write 路径后，Windows MSVC Debug 运行
  14 个测试并在新用例 primitive POD read 处失败；GREEN 后同一测试 14/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 14/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z6-fieldinfo-primitive-pod-read-write.md`。
  备注：本记录只关闭 same-runtime FieldDef token 的 primitive POD int32 raw inline boundary；不声明完整 primitive
  variant matrix、nested struct marshaling、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整
  metadata sweep 完成。

- 2026-07-01 04:12:42 +08:00 · 10-S4Z5 / 11-S4AQ / 12-S5 support FieldInfo value-slot write boundary ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven `VALUE_SLOT` 字段值写入边界子切片完成；07~12 总目标继续
  进行中，POD/nested inline field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_WriteFieldInfoTokenValue()`，并让 read/write 共用
  `reflection_resolve_field_value_slot_layout()` 对 FieldDef token、owner layout、field layout id、offset、
  `VALUE_SLOT` flag 和 inline storage range 的校验；写入成功时从调用方 `SZrTypeValue` 复制到 inline storage 字段槽。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 API 调用后 Windows MSVC Debug 构建/链接失败，缺少
  `ZrCore_Reflection_WriteFieldInfoTokenValue`；GREEN 后同一测试 13/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 13/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z5-fieldinfo-value-slot-write.md`。
  备注：本记录只关闭 same-runtime FieldDef token 的 inline `VALUE_SLOT` write boundary；不声明 raw POD/nested
  marshaling、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 03:49:06 +08:00 · 10-S4Z4 / 11-S4AP / 12-S5 support FieldInfo value-slot read boundary ·
  状态：最小 FieldDef token `FieldInfo` 的 token-driven `VALUE_SLOT` 字段值只读边界子切片完成；07~12 总目标继续
  进行中，field write、POD/nested inline field marshaling、完整 FieldInfo 行为、generic reflection object、
  `@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_ReadFieldInfoTokenValue()`，复用 FieldDef token resolver 的 owner layout、
  field layout id 与 offset；实现按 owner `SZrTypeLayoutField` 校验 offset/type-layout/`VALUE_SLOT` flag/inline
  storage range 后，从 inline storage 复制 `SZrTypeValue` 到输出槽。失败路径返回 `ZR_FALSE` 并重置输出为 null。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 API 调用后 WSL GCC 构建/链接失败，缺少
  `ZrCore_Reflection_ReadFieldInfoTokenValue`；GREEN 后同一测试 12/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 12/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z4-fieldinfo-value-slot-read.md`。
  备注：本记录只关闭 same-runtime FieldDef token 的 inline `VALUE_SLOT` read boundary；不声明 field write、
  raw POD/nested marshaling、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep
  完成。

- 2026-07-01 03:17:49 +08:00 · 10-S4Z3 / 11-S4AO / 12-S5 support FieldInfo recursive signature type-node type literal carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 recursive signature type-node object type literal carrier 子切片完成；
  07~12 总目标继续进行中，recursive/signature-derived field type binding、跨模块 provider、字段值读写、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在会在 recursive signature type-node object 具备
  semantic `typeName` 时物化 public `type` type literal object。`baseTypeNodeObject.type`、primitive `childNodeObjects[0].type`、
  direct TypeDef `childNodeObjects[1].type` 和 direct TypeRef `childNodeObjects[2].type` 均暴露 `kind == "type"` 且
  `name/qualifiedName == "int"`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 nested `type` object 断言后 WSL GCC 运行失败 1/11，
  失败点为 `Expected Non-NULL`；GREEN 后同一测试 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z3-fieldinfo-signature-node-type-literal.md`。
  备注：本记录只关闭 same-runtime recursive signature type-node type literal carrier；不声明跨模块 provider、字段 read/write、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 03:04:36 +08:00 · 10-S4Z2 / 11-S4AN / 12-S5 support FieldInfo direct TypeRef child type-node semantic carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature type-node object direct TypeRef child semantic carrier 子切片完成；
  07~12 总目标继续进行中，recursive/signature-derived field type binding、跨模块 provider、字段值读写、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在对递归 direct `TYPE_REF` node 复用 module
  signature token record、TypeRef→target TypeDef layout resolver 和 target TypeDef string-pool name view。
  `fieldTypeSignatureNodeObject.childNodeObjects[2]` 对 `GENERIC_INST(TYPE_DEF(...), int64, TYPE_DEF(...), TYPE_REF(...))`
  暴露 token/layout/name。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 direct TypeRef child `typeToken/typeLayoutId/typeSize/typeName`
  断言后 WSL GCC 运行失败 1/11；GREEN 后同一测试 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z2-fieldinfo-signature-typeref-child-node-semantic.md`。
  备注：本记录只关闭 same-runtime bound TypeRef child semantic carrier；不声明跨模块 provider、字段 read/write、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 02:47:33 +08:00 · 10-S4Y / 11-S4AM / 12-S5 support FieldInfo direct TypeDef child/base type-node semantic carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature type-node object direct TypeDef base/child semantic carrier 子切片完成；
  07~12 总目标继续进行中，recursive/signature-derived field type binding、direct TypeRef generic argument semantic
  token/layout binding、跨模块 provider、字段值读写、完整 FieldInfo 行为、generic reflection object、`@dynamically_accessed`
  dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在带 runtime 上下文递归构造 signature type-node
  object；对 direct `TYPE_DEF` node 复用 signature record matcher、registry-backed token→layout resolver 和 TypeDef
  string-pool name view。`fieldTypeSignatureNodeObject.baseTypeNodeObject` 与 `childNodeObjects[1]` 对
  `GENERIC_INST(TYPE_DEF(...), int64, TYPE_DEF(...))` 均暴露 token/layout/name。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 direct TypeDef base/child `typeToken/typeLayoutId/typeSize/typeName`
  断言后 WSL GCC 运行失败 1/11；GREEN 后同一测试 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4y-fieldinfo-signature-typedef-child-node-semantic.md`。
  备注：本记录只关闭 same-runtime direct TypeDef base/child semantic carrier；不声明 direct TypeRef child token/layout binding、
  字段 read/write、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或跨模块 provider 完成。

- 2026-07-01 02:19:00 +08:00 · 10-S4X / 11-S4AL / 12-S5 support FieldInfo signature primitive child type-node semantic name carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature type-node object primitive child semantic name carrier 子切片完成；07~12
  总目标继续进行中，recursive/signature-derived field type binding、direct TypeDef/TypeRef generic argument semantic
  token/layout binding、跨模块 provider、字段值读写、完整 FieldInfo 行为、generic reflection object、`@dynamically_accessed`
  dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在为递归读取到的 primitive child node 补出
  builtin semantic type name；`fieldTypeSignatureNodeObject.childNodeObjects[0]` 对
  `GENERIC_INST(TYPE_DEF(...), int64)` 的 `PRIMITIVE(INT64)` 子节点暴露 `typeName == "int"`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 primitive child `typeName` 断言后 WSL GCC 运行失败 1/11；
  GREEN 后同一测试 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4x-fieldinfo-signature-primitive-child-node-semantic.md`。
  备注：本记录只关闭 primitive child semantic name carrier；不声明 direct TypeDef/TypeRef child token/layout binding、
  字段 read/write、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或跨模块 provider 完成。

- 2026-07-01 02:00:10 +08:00 · 10-S4W / 11-S4AK / 12-S5 support FieldInfo signature child type-node object list carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature type-node object child-list carrier 子切片完成；07~12 总目标继续进行中，
  recursive/signature-derived field type binding、generic argument semantic binding、跨模块 provider、字段值读写、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在为 signature node object 写出 `childNodeObjects` array；
  `fieldTypeSignatureNodeObject` 对 `GENERIC_INST(TYPE_DEF(...), int64)` 可读取同 blob 的 generic argument
  `PRIMITIVE(INT64)` 子节点并暴露 structural carrier。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 child list 断言后 WSL GCC 运行失败 1/11
  （`childNodeObjects` 缺失）；GREEN 后同一测试 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4w-fieldinfo-signature-child-node-objects.md`。
  备注：本记录只关闭 child type-node structural object list；不声明 child semantic token/layout binding、字段 read/write、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或跨模块 provider 完成。

- 2026-07-01 01:41:50 +08:00 · 10-S4V / 11-S4AJ / 12-S5 support FieldInfo signature base type-node object carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature type-node object base-child carrier 子切片完成；07~12 总目标继续进行中，
  recursive/signature-derived field type binding、generic argument child object list、跨模块 provider、字段值读写、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在向 signature node object builder 传入 validated signature blob；
  `fieldTypeSignatureNodeObject` 对 `GENERIC_INST(TYPE_DEF(...), int64)` 这类节点可读取同 blob 的 base `TYPE_DEF`
  子节点并暴露 `baseTypeNodeObject` structural carrier。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic base object 断言后 WSL GCC 运行失败 1/11
  （`baseTypeNodeObject` 缺失）；GREEN 后同一测试 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4v-fieldinfo-signature-base-type-node-object.md`。
  备注：本记录只关闭 base type-node structural object carrier；不声明 generic argument list、semantic token/layout binding、
  字段 read/write、完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或跨模块 provider 完成。

- 2026-07-01 01:17:21 +08:00 · 10-S4U / 11-S4AI / 12-S5 support FieldInfo signature type-node object carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature type-node object carrier 子切片完成；07~12 总目标继续进行中，
  recursive/signature-derived field type binding、跨模块 provider、字段值读写、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 新增 `fieldTypeSignatureNodeObject`。该 object
  聚合已验证 field type-node 的 node/blob offset/payload/base/child summary，并同步 signature-derived
  token/layout/typeName/matchesLayout；primitive、direct `TYPE_DEF` 与 bound `TYPE_REF` fixtures 均覆盖。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增该 object 字段断言后 WSL GCC 运行失败 3/10
  （字段缺失）；GREEN 后 primitive、TypeDef、TypeRef 三种 node object 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4u-fieldinfo-signature-type-node-object.md`。
  备注：本记录只关闭 top-level signature type-node object carrier；不声明 recursive child nodes、字段 read/write、
  完整 FieldInfo method surface、trim analyzer、DESCRIPTION promotion 或跨模块 provider 完成。

- 2026-07-01 00:55:33 +08:00 · 10-S4T / 11-S4AH / 12-S5 support FieldInfo signature/layout consistency carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 signature-derived type 与 FieldDef layout 一致性 carrier 子切片完成；
  07~12 总目标继续进行中，recursive/signature-derived field type binding、跨模块 provider、字段值读写、完整 FieldInfo
  行为、generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 新增 `fieldTypeSignatureMatchesLayout` bool。primitive
  bool signature 对 int FieldDef layout 为 false；direct `TYPE_DEF` 与 bound `TYPE_REF` 解析到同一 registry layout
  id/pointer 时为 true。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增该 bool 字段断言后 WSL GCC 运行失败 3/10（字段缺失）；
  GREEN 后 primitive false、TypeDef true、TypeRef true 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4t-fieldinfo-signature-layout-consistency.md`。
  备注：本记录只关闭 read-only signature/layout consistency carrier；不声明字段 read/write、完整 FieldInfo method
  surface、trim analyzer、DESCRIPTION promotion 或跨模块 provider 完成。

- 2026-07-01 00:37:26 +08:00 · 10-S4S / 11-S4AG / 12-S5 support FieldInfo bound TypeRef signature carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 current-runtime bound TypeRef signature carrier 子切片完成；07~12 总目标
  继续进行中，recursive/signature-derived field type binding、跨模块 TypeRef/provider 规则、字段值读写、完整 FieldInfo
  行为、generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 新增 `FIELD_SIG(TYPE_REF(object, 23))`、matching module TypeRef token/signature records
  和 target local TypeDef layout binding；builder 保留 public TypeRef token identity，同时复用 TypeRef -> target TypeDef
  layout resolver 写出 layout/size，并读取 target TypeDef row name 物化 signature-derived type object。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureTypeName` 与
  `fieldTypeSignatureType` object 后 WSL GCC 失败；GREEN 后同一测试暴露 TypeRef token/layout/size 和 target TypeDef
  type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4s-fieldinfo-signature-typeref-carrier.md`。
  备注：本记录只关闭 current-runtime attached bound TypeRef signature carrier；不声明 cross-module signature binding、
  字段 read/write、完整 FieldInfo method surface、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-07-01 00:15:36 +08:00 · 10-S4R / 11-S4AF / 12-S5 support FieldInfo direct TypeDef signature type object ·
  状态：最小 FieldDef token `FieldInfo` 的 direct local TypeDef signature type object 子切片完成；07~12 总目标
  继续进行中，recursive/signature-derived field type binding、TypeRef/跨模块规则、字段值读写、完整 FieldInfo 行为、
  generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 继续使用 `FIELD_SIG(TYPE_DEF(object, 17))`，并在既有 token/layout/size carrier
  之上断言 `fieldTypeSignatureTypeName == "int"` 和 `fieldTypeSignatureType.kind/name/qualifiedName == type/int/int`。
  builder 复用 TypeDef layout binding view 读取 zrp string-pool type name，并用同一 type literal object builder
  物化 signature-derived type object。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureType` object 后 WSL GCC 失败；
  GREEN 后同一测试暴露 direct local TypeDef signature type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4r-fieldinfo-signature-typedef-type-object.md`。
  备注：本记录只关闭 current-runtime local TypeDef signature type object；不声明 TypeRef/cross-module signature
  binding、字段 read/write、完整 FieldInfo method surface、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 21:45:03 +08:00 · 10-S4Q / 11-S4AE / 12-S5 support FieldInfo direct TypeDef signature token/layout carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 direct local TypeDef signature token/layout carrier 子切片完成；07~12
  总目标继续进行中，recursive/signature-derived field type binding、TypeRef/跨模块规则、字段值读写、完整 FieldInfo
  行为、generic reflection object、`@dynamically_accessed` dataflow 和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 新增 `FIELD_SIG(TYPE_DEF(object, 17))`；builder 将 validated field type-node
  匹配到 attached local `TYPE_DEF` signature record，并经 runtime type-token layout resolver 写出
  `fieldTypeSignatureTypeToken/fieldTypeSignatureTypeLayoutId/fieldTypeSignatureTypeSize`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureTypeToken` 后 WSL GCC 失败；
  GREEN 后同一测试暴露 direct local TypeDef signature token/layout carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4q-fieldinfo-signature-typedef-carrier.md`。
  备注：本记录只关闭 direct local TypeDef signature carrier；不声明 TypeRef/cross-module signature binding、字段
  read/write、完整 FieldInfo method surface、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 21:20:26 +08:00 · 10-S4P / 11-S4AD / 12-S5 support FieldInfo module reflection object link ·
  状态：最小 FieldDef token `FieldInfo` 的 module reflection object link 子切片完成；07~12 总目标继续进行中，
  完整 signature-derived semantic field type binding、字段值读写、完整 FieldInfo 行为、recursive type-node reflection object、
  generic reflection object、`@dynamically_accessed` dataflow、跨模块规则和完整 metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 现在用真实 `ZrCore_Module_Create()` module 验证 `FieldInfo.module`；builder 在
  attached runtime module 是 `ZR_OBJECT_INTERNAL_TYPE_MODULE` 时复用既有 module reflection builder/cache，并写出
  `module.kind/name/qualifiedName == module/geometry/geometry`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 module reflection object 后 WSL GCC 失败；
  GREEN 后同一测试暴露最小 module reflection object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4p-fieldinfo-module-reflection-link.md`。
  备注：本记录只关闭 FieldInfo -> module identity link；不声明字段 read/write、完整 FieldInfo method surface、
  TypeDef/TypeRef signature binding、跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 21:02:11 +08:00 · 10-S4O / 11-S4AC / 12-S5 support FieldInfo primitive signature type object carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 primitive field signature type object carrier 子切片完成；07~12 总目标继续进行中，
  完整 signature-derived semantic field type binding、字段值读写、完整 FieldInfo 行为、recursive type-node reflection object、
  module full reflection object link、generic reflection object、`@dynamically_accessed` dataflow、跨模块规则和完整
  metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 现在同时锁定 layout/token-derived `type` object 为 `"int"` 与 signature-derived
  `PRIMITIVE(BOOL)` type object；`ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 validated field type-node 为
  `PRIMITIVE` 时写入 public `fieldTypeSignatureType` type literal object，缺失/无效/非 primitive node 保持 null。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 primitive signature type object 后 WSL GCC 失败；
  GREEN 后同一测试暴露 `fieldTypeSignatureType.kind/name/qualifiedName == type/bool/bool`，同时不改变既有
  layout-derived `type`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4o-fieldinfo-field-signature-type-object.md`。
  备注：本记录只关闭 primitive signature type object carrier；不声明 TypeDef/TypeRef signature binding、字段类型一致性校验、
  recursive type-node reflection object、字段 read/write、完整 FieldInfo method surface、module reflection link、
  跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 20:46:10 +08:00 · 10-S4N / 11-S4AB / 12-S5 support FieldInfo primitive signature type carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 primitive field signature type carrier 子切片完成；07~12 总目标继续进行中，
  完整 signature-derived semantic field type binding、字段值读写、完整 FieldInfo 行为、recursive type-node reflection object、
  module full reflection object link、generic reflection object、`@dynamically_accessed` dataflow、跨模块规则和完整
  metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 现在同时锁定 layout/token-derived `typeName == "int"` 与 signature-derived
  `PRIMITIVE(BOOL)`；`ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 validated field type-node 为 `PRIMITIVE`
  时写入 public `fieldTypeSignatureValueType` 与 `fieldTypeSignatureTypeName`，缺失/无效/非 primitive node
  保持 `0` 与空字符串。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 primitive signature value type/name 字段后 WSL GCC 失败；
  GREEN 后同一测试暴露 `ZR_VALUE_TYPE_BOOL` 和 `"bool"`，同时不改变既有 layout-derived `typeName`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4n-fieldinfo-field-signature-primitive-type.md`。
  备注：本记录只关闭 primitive signature type carrier；不声明 TypeDef/TypeRef signature binding、字段类型一致性校验、
  recursive type-node reflection object、字段 read/write、完整 FieldInfo method surface、module reflection link、
  跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 20:32:08 +08:00 · 10-S4M / 11-S4AA / 12-S5 support FieldInfo field signature type-node summary carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 validated field signature type-node summary carrier 子切片完成；07~12 总目标继续进行中，
  signature-derived semantic field type binding、字段值读写、完整 FieldInfo 行为、recursive type-node reflection object、
  module full reflection object link、generic reflection object、`@dynamically_accessed` dataflow、跨模块规则和完整
  metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 的 `FIELD_SIG` blob 现在继续断言 field type node 为 `PRIMITIVE(BOOL)`；
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 validated `FIELD_SIG` header 成功后调用
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`，并把 node/blobOffset/nextBlobOffset/payload/base/child summary
  写入 public `FieldInfo.fieldTypeSignature*` 字段；缺失或无效 view 时保持 `0`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 field signature type-node summary 字段后 WSL GCC 失败；
  GREEN 后同一测试暴露 `PRIMITIVE(BOOL)` node、offset `2`、next offset `7` 和 bool payload。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4m-fieldinfo-field-signature-type-node.md`。
  备注：本记录只关闭最小 FieldInfo field signature type-node summary carrier；不声明 signature-derived field type
  binding、recursive type-node reflection object、字段 read/write、完整 FieldInfo method surface、module reflection link、
  跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 20:13:27 +08:00 · 10-S4L / 11-S4Z / 12-S5 support FieldInfo validated field signature header carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 validated field signature header carrier 子切片完成；07~12 总目标继续进行中，
  signature-derived field type binding、字段值读写、完整 FieldInfo 行为、recursive type-node reflection object、
  module full reflection object link、generic reflection object、`@dynamically_accessed` dataflow、跨模块规则和完整
  metadata sweep 仍未关闭。
  完成项目：FieldInfo fixture 现在在 signature blob pool 的 raw `4/7` 坐标处放入合法 `FIELD_SIG` blob；
  `ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)` 现在复用
  `ZrCore_MetadataRuntime_ReadSignatureView()`，在 FieldDef paired signature record 可解析为 `FIELD_SIG` 时写入 public
  `signatureRootNode` / `signatureFlags` / `fieldTypeBlobOffset` 字段；签名缺失、无效或 root 不是 `FIELD_SIG` 时这些
  validated header 字段保持 `0`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `signatureRootNode` / `signatureFlags` /
  `fieldTypeBlobOffset` 后，WSL GCC 运行失败；GREEN 后 `zr_vm_reflection_token_resolve_test` 8/0。
  测试结果：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4l-fieldinfo-field-signature-header.md`。
  备注：本记录只关闭最小 FieldInfo validated field signature header carrier；不声明 signature-derived field type
  binding、recursive type-node reflection object、字段 read/write、完整 FieldInfo method surface、module reflection link、
  跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 19:54:58 +08:00 · 10-S4K / 11-S4Y / 12-S5 support FieldInfo FieldDef signature blob coordinate carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 raw signature blob coordinate carrier 子切片完成；07~12 总目标继续进行中，
  field signature 语义解析、字段值读写、完整 FieldInfo 行为、module full reflection object link、generic reflection
  object、`@dynamically_accessed` dataflow、跨模块规则和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)` 现在从 resolved FieldDef row
  的 `signatureBlobOffset` / `signatureBlobLength` 写入 public `signatureBlobOffset` / `signatureBlobLength` 字段；
  focused test 用 `4/7` 锁定 raw coordinate carrier，不验证或解析 blob。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `signatureBlobOffset` / `signatureBlobLength`
  后，WSL GCC 运行失败；GREEN 后 `zr_vm_reflection_token_resolve_test` 8/0。
  测试结果：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4k-fieldinfo-fielddef-signature-blob.md`。
  备注：本记录只关闭最小 FieldInfo raw FieldDef signature blob offset/length carrier；不声明 blob slice validation、
  field signature parser、signature-derived field type binding、字段 read/write、完整 FieldInfo method surface、
  module reflection link、跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 19:39:14 +08:00 · 10-S4J / 11-S4X / 12-S5 support FieldInfo FieldDef flags carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 raw metadata flags carrier 子切片完成；07~12 总目标继续进行中，
  flags 位语义、字段值读写、完整 FieldInfo 行为、module full reflection object link、generic reflection object、
  `@dynamically_accessed` dataflow、跨模块规则和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)` 现在从 resolved FieldDef row
  的 `flags` 写入 public `metadataFlags` 字段；focused test 用 `0xA5` 锁定 raw carrier，不解释 flag bit 含义。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `metadataFlags` 后，WSL GCC 运行失败；
  GREEN 后 `zr_vm_reflection_token_resolve_test` 8/0。
  测试结果：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4j-fieldinfo-fielddef-flags.md`。
  备注：本记录只关闭最小 FieldInfo raw FieldDef flags carrier；不声明 flags 位语义、`isStatic`/`isConst`
  映射、字段 read/write、完整 FieldInfo method surface、module reflection link、跨模块 FieldRef/TypeRef、
  trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 19:26:41 +08:00 · 10-S4I / 11-S4W / 12-S5 support FieldInfo moduleName carrier ·
  状态：最小 FieldDef token `FieldInfo` 的 module name carrier 子切片完成；07~12 总目标继续进行中，
  字段值读写、完整 FieldInfo 行为、module full reflection object link、generic reflection object、
  `@dynamically_accessed` dataflow、跨模块规则和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)` 现在从 attached metadata runtime
  的 `module->moduleName` 读取 module name，缺失时回退 `fullPath`，并写入 public `moduleName` 字段；focused test
  用 synthetic module name `geometry` 锁定该载体。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `moduleName` string 后，WSL GCC 运行失败；
  GREEN 后 `zr_vm_reflection_token_resolve_test` 8/0。
  测试结果：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4i-fieldinfo-module-name.md`。
  备注：本记录只关闭最小 FieldInfo moduleName string carrier；不声明字段 read/write、完整 FieldInfo method
  surface、module reflection link、跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 19:11:22 +08:00 · 10-S4H / 11-S4V / 12-S5 support FieldInfo owner object link ·
  状态：最小 FieldDef token `FieldInfo` 的 owner link 子切片完成；07~12 总目标继续进行中，
  字段值读写、完整 FieldInfo 行为、module full reflection object link、generic reflection object、
  `@dynamically_accessed` dataflow、跨模块规则和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)` 现在把 public `owner`
  字段链接到同一 `declaringType` type literal object；focused test 断言两者指针一致。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `owner` object link 后，WSL GCC 运行失败；
  GREEN 后 `zr_vm_reflection_token_resolve_test` 8/0。
  测试结果：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4h-fieldinfo-owner-link.md`。
  备注：本记录只关闭最小 FieldInfo owner identity link；不声明字段 read/write、完整 FieldInfo method surface、
  module reflection link、跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 18:58:39 +08:00 · 10-S4G / 11-S4U / 12-S5 support FieldInfo declaring type object link ·
  状态：最小 FieldDef token `FieldInfo` 的 declaring type carrier 子切片完成；07~12 总目标继续进行中，
  字段值读写、完整 FieldInfo 行为、owner/module full reflection object links、generic reflection object、
  `@dynamically_accessed` dataflow、跨模块规则和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)` 现在从 owner TypeDef row 的
  zrp string pool 写出 `ownerTypeName`、`declaringTypeName`，并构造 nested `declaringType` type literal object；
  focused test 同时锁定既有 field `type` nested object 形态。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 declaring type 字段后，WSL GCC 运行失败；GREEN 后
  `zr_vm_reflection_token_resolve_test` 8/0。
  测试结果：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4g-fieldinfo-declaring-type-object.md`。
  备注：本记录只关闭最小 FieldInfo declaring type name/object carrier；不声明字段 read/write、完整 FieldInfo method
  surface、跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 18:38:42 +08:00 · 10-S4F / 11-S4T / 12-S5 support minimum FieldDef token FieldInfo public object ·
  状态：最小 FieldDef token -> public `FieldInfo` object 子切片完成；07~12 总目标继续进行中，
  字段值读写、完整 FieldInfo 行为、generic reflection object、`@dynamically_accessed` dataflow、跨模块规则和
  完整 metadata sweep 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)`，复用
  FieldDef token resolver carrier、11-S4 FieldDef/TypeDef layout binding views、layout/token/offset 单一真相和
  attached zrp string pool，构造包含 name、qualifiedName、kind、typeName、metadata/owner/type tokens、offset/size、
  layout ids、layout 子对象与 nested type literal 的最小 public `FieldInfo` reflection object。
  RED/GREEN：RED 为 focused reflection token resolver 测试引用缺失 API 导致 WSL GCC 链接失败；GREEN 后
  positive/null/wrong-token paths 通过，reflection token resolve 8/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s4f-fielddef-token-fieldinfo-object.md`。
  备注：本记录只关闭最小 FieldInfo object materialization；不声明字段 read/write、完整 FieldInfo method surface、
  owner/module links、跨模块 FieldRef/TypeRef、trim analyzer 或 DESCRIPTION promotion 完成。

- 2026-06-30 18:17:22 +08:00 · 10-S5N / 11-S4S / 12-S5M runtime bound TypeRef token layout resolver ·
  状态：运行期 attached bound TypeRef -> TypeDef layout resolver 子切片完成；07~12 总目标继续进行中，跨模块 provider
  lookup/version compatibility、dataflow、warning policy、FieldInfo/字段值读写和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 现在接受 attached `TYPE_REF` token record，要求
  `targetMetadataToken` 指向当前 runtime 可解析的 `TYPE_DEF`，复用 TypeDef layout binding view，校验 optional signature、
  module hash 与 layout identity，并把 TypeRef token 命中写入 layout cache。
  RED/GREEN：RED1 为 TypeRef 正向用例返回 NULL；GREEN1 后 bound TypeRef->TypeDef layout/id 与 cache 通过。RED2 为
  module identity mismatch 未拒绝；GREEN2 后 module hash mismatch 返回 NULL 并清空 out layout id，layout identity mismatch
  负向路径保持通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `metadata_runtime_typespec_layout` 17/0、`metadata_runtime_query`
  24/0、`reflection_token_resolve` 7/0、`metadata_type_ref_binding` 8/0；focused CTest 同组在三套环境均为 4/4。
  WSL GCC 初次 combined CTest 出现过 `metadata_type_ref_binding` wrapper transient，直接执行、单项 CTest 和重跑
  combined CTest 均通过。`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5n-runtime-bound-typeref-token-layout-resolver.md`。
  备注：本记录只关闭 attached/current-runtime bound TypeRef layout resolver；不声明跨模块 provider runtime context、
  embedded zrp token-record 独立 scan、`@dynamically_accessed`、warning promotion/suppression、DESCRIPTION 提升或完整
  metadata sweep 完成。

- 2026-06-30 17:56:52 +08:00 · 10-S5M / 12-S5L dynamic-dependency bound TypeRef type token root ·
  状态：10/12 注解保留中当前 embedded zrp TypeRef token-record binding 子切片完成；完整跨模块 provider
  load/compatibility、runtime TypeRef layout resolution、跨模块依赖、数据流和 trim analyzer 仍未关闭。
  完成项目：`dynamicDependencyTypeToken` root collector 现在接受 `TYPE_REF` token，要求 embedded zrp
  `TOKEN_RECORDS` section 中有唯一 matching `SZrMetadataTokenRecord`，且 `record->targetMetadataToken`
  指向当前 blob 的 `TYPE_DEF` row。该目标 TypeDef 的 `typeLayoutId` 复用 type-layout root retention 管道，
  保留 descriptor、registration entry、统计、root marker 和 root-only token table fallback。
  RED/GREEN：RED 为新增 TypeRef fixture 使用 `dynamicDependencyTypeToken = 0x05000001` 后 writer 返回 false；
  GREEN 后 `targetMetadataToken = 0x02000001` 解析到 `typeLayoutId = 2`，function 2 被裁剪但
  `ZrTypeLayout_2` 与 `zr_aot_type_layout_tokens[2] = 0x02000001u` 保留。
  验证：WSL gcc/clang focused code stripping 10/0、source contracts 24/0、type-layout contracts 1/0，
  global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0；WSL gcc/clang focused CTest 2/2；
  Windows MSVC Debug focused code stripping 10/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest 2/2；Windows shared-library smoke binaries returned OK with 10/5/7 ignored and 0 failures；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5m-dynamic-dependency-bound-typeref-token-layout-root.md`。
  备注：仍不声明跨模块 provider 加载/版本兼容、运行期 TypeRef->layout resolver、字段值读写、
  FieldInfo object、数据流、warning policy、DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 17:31:53 +08:00 · 10-S5L / 12-S5K dynamic-dependency field token layout roots ·
  状态：10/12 注解保留中当前模块 FieldDef field-token carrier 子切片完成；完整字段值读写/FieldInfo 物化、
  TypeRef/跨模块 type token、跨模块依赖、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT C writer 现在在 code stripping 前扫描 `dynamicDependencyFieldToken` decorator metadata，
  仅把当前模块 embedded zrp FieldDef token 映射到 owner TypeDef `typeLayoutId` 与 field `typeLayoutId`，
  并复用 type-layout root retention 管道保留 descriptors、registration entries、统计和 root markers。
  新增 `backend_aot_c_type_layout_metadata_roots.{h,c}` 承载 metadata token -> type-layout roots 解析，避免继续膨胀
  `backend_aot_c_type_layouts.c`。
  RED/GREEN：RED 为新增 FieldDef token fixture 期望 `ZrTypeLayout_2` 后失败；GREEN 后 function 2 被裁剪，
  但 owner/field type-layout descriptors、registration entries、统计和 root markers 1/2 保留。
  验证：WSL gcc/clang focused code stripping 9/0、source contracts 24/0、type-layout contracts 1/0，
  global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0；WSL gcc/clang focused CTest 2/2；
  Windows MSVC Debug focused code stripping 9/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest 2/2；Windows shared-library smoke binaries returned OK with 10/5/7 ignored and 0 failures；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5l-dynamic-dependency-field-token-layout-roots.md`。
  备注：仍不声明字段值读写、FieldInfo object、TypeRef/跨模块 type token、数据流、warning policy、
  DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 16:58:17 +08:00 · 10-S5K / 12-S5J dynamic-dependency type token root ·
  状态：10/12 注解保留中当前模块 TypeDef/TypeSpec type-token carrier 子切片完成；完整 FieldDef/field dependency、
  TypeRef/跨模块 type token、跨模块依赖、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT C writer 现在在 code stripping 前扫描 `dynamicDependencyTypeToken` decorator metadata，
  仅把当前模块 embedded zrp TypeDef/TypeSpec token 映射到唯一 non-none `typeLayoutId`，并复用 type-layout root
  retention 管道保留 descriptor、registration entry、统计和 marker。root-only `zr_aot_type_layout_tokens[]`
  现在能从 metadata blob 回填对应 TYPE_DEF/TYPE_SPEC token。
  RED/GREEN：RED 为新增 TypeDef token fixture 期望 `ZrTypeLayout_2` 后失败；GREEN 后 TypeDef/TypeSpec fixtures
  都确认 function 2 被裁剪，但 type-layout descriptor、registration entry、token table、统计和 root marker 保留。
  验证：WSL gcc/clang focused code stripping 8/0、source contracts 24/0、type-layout contracts 1/0，
  global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0；WSL gcc/clang focused CTest 2/2；
  Windows MSVC Debug focused code stripping 8/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest 2/2；Windows shared-library smoke binaries returned OK with 10/5/7 ignored and 0 failures；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5k-dynamic-dependency-type-token-root.md`。
  备注：仍不声明 FieldDef、TypeRef/跨模块 type token、field dependency、数据流、warning policy、
  DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 16:25:31 +08:00 · 10-S5J / 12-S5I dynamic-dependency type-layout root ·
  状态：10/12 注解保留中当前模块 type-layout-id carrier 子切片完成；完整 TypeDef/TypeSpec token dependency、
  field dependency、跨模块依赖、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT C writer 现在在 code stripping 前扫描 `dynamicDependencyTypeLayoutId` decorator metadata，
  收集独立 type-layout root 集合，并让 type-layout stats/declarations/index spaces/GC descriptor table/
  registration table 消费该集合。生成 C 输出 `code_stripping.annotationTypeLayoutRoots` 与
  `code_stripping.annotationTypeLayoutRoot[index]`；测试确认被裁剪函数的 `ZrTypeLayout_2` 仍能作为
  `zr_aot_type_layouts[2]` 保留。
  RED/GREEN：RED 为新增 generated-C fixture 期望 `ZrTypeLayout_2` 后失败；GREEN 后 function 2 被裁剪，
  但 type-layout descriptor、registration entry、统计和 root marker 保留。
  验证：WSL gcc/clang focused code stripping 6/0、source contracts 24/0、type-layout contracts 1/0，
  global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0；WSL gcc/clang focused CTest 2/2；
  Windows MSVC Debug focused code stripping 6/0、source contracts 24/0、type-layout contracts 1/0；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5j-dynamic-dependency-type-layout-root.md`。
  备注：仍不声明 TypeDef/TypeSpec token dependency、field dependency、跨模块 annotation、
  非方法 member token、`@dynamically_accessed` 数据流、warning promotion/per-warning suppression、
  未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 15:31:50 +08:00 · 10-S5I / 12-S5H dynamic-dependency non-exported MethodDef token root ·
  状态：10/12 注解保留中当前模块非导出 method token carrier 子切片完成；完整跨模块依赖、
  field/type dependency、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT reachability annotation root collector 的 `dynamicDependencyMethodToken` resolver 现在按 root module
  `typedExportedSymbols` 中的 typed function symbol + `MEMBER_DEF` token 精确匹配 callable child，再映射到 pre-trim
  flat function index；不再要求该 symbol 是 exported function。按名 dependency 仍保持 exported-name 限制。
  RED/GREEN：RED 为非导出 typed function symbol 承载 `dynamicDependencyMethodToken = 0x03000008` 后收集失败
  `Expected TRUE Was FALSE`，generated-C fixture 无法保留 target；GREEN 后 token 解析到 flat index 2，
  `annotationRoot[0] = 2` 与 `zr_aot_fn_2` 均可见，source contract 锁定 non-exported token helper、
  `symbolKind`/`MEMBER_DEF` gate、matched token count 和 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3，
  reachability 14/0、annotation preserve 11/0、global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0、
  source contracts 23/0；Windows MSVC Debug 同组 CTest 3/3、reachability 14/0、annotation preserve 11/0、
  source contracts 23/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5i-dynamic-dependency-non-exported-method-token-root.md`。
  备注：仍不声明跨模块 annotation、field/type dependency、非方法 member token、`@dynamically_accessed` 数据流、
  warning promotion/per-warning suppression、未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 14:38:53 +08:00 · 10-S5H / 12-S5G dynamic-dependency method-name signature-hash disambiguation ·
  状态：10/12 注解保留中当前模块 exported method name 签名消歧子切片完成；完整跨模块/非导出成员依赖、
  field/type dependency、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT reachability annotation root collector 现在读取可选
  `dynamicDependencyMethodSignatureHash` uint64，并与 `dynamicDependencyMethodName` 一起按 root module
  `typedExportedSymbols` 的 exported function name + `signatureHash` 唯一匹配 callable child，再映射到 pre-trim
  flat function index。未提供 signature hash 时，重复同名 exported function 不再静默绑定第一个匹配项。
  RED/GREEN：RED 为两个同名 exported `target` 方法写入不同 signature hash 后，旧 resolver 保留错误 child，
  reachability 失败为 `Expected 2 Was 1`；GREEN 后 `dynamicDependencyMethodSignatureHash = 0x2222`
  解析到 flat index 2，`annotationRoot[0] = 2` 与 `zr_aot_fn_2` 可见，并新增无 hash 同名歧义拒绝用例。
  source contract 锁定 uint64 metadata helper、signature field、显式 zero hash、name+signature resolver、`symbol->signatureHash`
  match 与 matched-symbol count。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3，
  reachability 13/0、annotation preserve 10/0、global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0、
  source contracts 23/0；Windows MSVC Debug 同组 CTest 3/3、reachability 13/0、annotation preserve 10/0、
  source contracts 23/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5h-dynamic-dependency-method-name-signature-hash-root.md`。
  备注：仍不声明跨模块 annotation、非导出 member token、field/type dependency、`@dynamically_accessed` 数据流、
  warning promotion/per-warning suppression、未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 14:01:25 +08:00 · 10-S5G / 12-S5F dynamic-dependency exported method-name root ·
  状态：10/12 注解保留中当前模块 exported method name carrier 子切片完成；完整跨模块/非导出成员依赖、
  field/type dependency、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT reachability annotation root collector 现在读取 function decorator metadata
  `dynamicDependencyMethodName` string，并只通过 root module `typedExportedSymbols` 中的 exported function
  name 匹配 callable child，再映射到 pre-trim flat function index；解析成功后目标复用 annotation root 去重、
  `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 和 generated-C `code_stripping.annotationRoot[]` 诊断路径。
  RED/GREEN：RED 为 reachability fixture 写入 `dynamicDependencyMethodName = "target"` 后 annotation root count
  仍为 0，generated-C fixture 无法保留 target；GREEN 后 name 解析到 flat index 2，`annotationRoot[0] = 2`
  与 `zr_aot_fn_2` 均可见，source contract 锁定 string metadata helper、method-name resolver、typed exported
  symbol name match 和 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3，
  reachability 10/0、annotation preserve 9/0、global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0、
  source contracts 23/0；Windows MSVC Debug 同组 CTest 3/3、reachability 10/0、annotation preserve 9/0、
  source contracts 23/0，Unix-only smoke 为 0 failures / ignored。
  产出：`tests/acceptance/2026-06-30-aot-10-s5g-dynamic-dependency-method-name-root.md`。
  备注：仍不声明跨模块 annotation、非导出 member token、field/type dependency、重载签名消歧、
  `@dynamically_accessed` 数据流、warning promotion/per-warning suppression、未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 13:22:30 +08:00 · 10-S5F / 12-S5E dynamic-dependency MethodDef token root ·
  状态：10/12 注解保留中当前模块 exported MethodDef token carrier 子切片完成；完整按名/跨模块/非导出成员依赖、
  数据流和 trim analyzer 仍未关闭。
  完成项目：AOT reachability annotation root collector 现在读取 function decorator metadata
  `dynamicDependencyMethodToken`，要求 token table 为 `MEMBER_DEF`，并经 root module `typedExportedSymbols`
  中的 function export token 解析为 callable child flat function index；解析成功后目标复用 annotation root 去重、
  `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 和 generated-C `code_stripping.annotationRoot[]` 诊断路径。
  RED/GREEN：RED 为 reachability fixture 的 token dependency root count 仍为 0，generated-C fixture 无法保留 target；
  GREEN 后 `dynamicDependencyMethodToken = 0x03000007` 解析到 flat index 2，`annotationRoot[0] = 2` 与 `zr_aot_fn_2`
  均可见，source contract 锁定 MEMBER_DEF gate、typed exported symbol lookup 和 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3，
  reachability 9/0、annotation preserve 8/0、global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0、
  source contracts 23/0；Windows MSVC Debug 同组 CTest 3/3、reachability 9/0、annotation preserve 8/0、
  source contracts 23/0，Unix-only smoke 为 0 failures / ignored。
  产出：`tests/acceptance/2026-06-30-aot-10-s5f-dynamic-dependency-method-token-root.md`。
  备注：仍不声明按名成员 dependency、field/type dependency、非导出 member token、跨模块 annotation、
  `@dynamically_accessed` 数据流、warning promotion/per-warning suppression、未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 12:54:26 +08:00 · 12-S7ZU / 10-S5E annotation warning suppression ·
  状态：writer-level annotation warning suppression 子切片完成；完整 attribute/annotation-driven warning policy
  和 trim analyzer 仍未关闭。
  完成项目：AOT writer options 新增 `suppressAnnotationWarnings`；opt-in code stripping 下的
  `requiresUnreferencedCode` annotation warning 仍会被扫描，但可见 count 变为 0，总数转入
  `trim_warnings.annotationSuppressedCount`，逐条 `trim_warning.annotation[]` marker 不再输出；runtime fallback
  warning 统计和 reason mask 保持独立。测试同时补强 `TYPEOF` reflection runtime fallback warning marker 断言。
  RED/GREEN：RED 为 suppressed fixture 缺少 writer option 字段而编译失败；GREEN 后 suppressed marker/count
  形态正确，既有 reflectable、dynamic dependency、reason text、runtime fallback warning 路径保持通过。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang annotation preserve 7/0、global smoke 10/0、call smoke 5/0、dynamic deopt smoke 7/0、source contracts 22/0；
  Windows MSVC Debug 同组 CTest 3/3、annotation preserve 7/0、source contracts 22/0，Unix-only smoke 均 0 failures/ignored；
  追加 source-contract marker 后三平台 source contracts 与 annotation preserve 快速复验均通过。
  产出：`tests/acceptance/2026-06-30-aot-12-s7zu-annotation-warning-suppression.md`。
  备注：不声明 per-warning attribute suppression、warning promotion、`@dynamically_accessed`、按 token/按名
  dynamic dependency、跨模块 annotation、未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 12:38:52 +08:00 · 10-S5D / 12-S5D dynamic-dependency function root ·
  状态：10/12 注解保留中 `@dynamic_dependency` 的函数级首个 carrier 子切片完成；完整数据流/成员依赖/
  跨模块规则仍未关闭。
  完成项目：AOT reachability annotation root collector 现在读取 function decorator metadata
  `dynamicDependencyFunctionIndex`，把存在于 pre-trim function table 的目标 flat function index 去重加入 root
  集合；opt-in code stripping 因此可保留 otherwise-unreachable target，并继续输出
  `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]` 诊断。
  RED/GREEN：RED 为 generated-C fixture 写入 dependency 后仍缺少 `annotationRoot[0] = 2` 且 `zr_aot_fn_2`
  被裁剪；GREEN 后该 target 保留，既有 reflectable、prune、requires-unreferenced warning 和 reason text 路径
  保持通过。
  验证：WSL gcc focused annotation preserve 6/0、reachability 8/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call
  shared-library smoke 5/0、dynamic deopt bridge smoke 7/0、source contracts 22/0；Windows MSVC Debug 同组
  CTest 3/3，Unix-only smoke 为 0 failures / ignored，source contracts 22/0；`git diff --check` 仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-30-aot-10-s5d-dynamic-dependency-function-root.md`。
  备注：仍不声明 `@dynamically_accessed`、按 token/按名成员 dependency、跨模块 annotation、warning 抑制/提升、
  未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 12:22:15 +08:00 · 10-S5C / 12-S5C requires-unreferenced-code reason text marker ·
  状态：10/12 注解 warning reason 文本子切片完成；完整 trim analyzer 仍未关闭。
  完成项目：AOT C opt-in code stripping 的 annotation warning scanner 现在会读取
  `requiresUnreferencedCodeReason` 字符串，并在 retained static caller 调用
  `requiresUnreferencedCode: true` callee 的 `trim_warning.annotation[]` marker 中追加
  quoted/escaped `message="..."`。缺失或非字符串 reason 保持 10-S5B 无 `message=` 的旧格式。
  RED/GREEN：RED 为 reason fixture 缺少 escaped `message` marker；GREEN 后 `uses "name" lookup`
  输出为 `message="uses \"name\" lookup"`，bool-only 与 unannotated 负向路径保持既有结果。
  验证：WSL gcc focused annotation preserve 5/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；WSL gcc/clang source contracts 22/0；
  Windows MSVC Debug 同组 CTest 3/3，Unix-only smoke 为 0 failures / ignored，source contracts 22/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5c-requires-unreferenced-code-reason.md`。
  备注：仍不声明 `@dynamically_accessed`、`@dynamic_dependency`、warning 抑制/提升、跨模块 annotation、
  未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 11:59:59 +08:00 · 10-S5B / 12-S5B requires-unreferenced-code static call warning ·
  状态：10/12 注解 warning 子切片完成；完整 trim analyzer 仍未关闭。
  完成项目：AOT C opt-in code stripping 现在会在 retained caller 静态调用 decorator metadata 含
  `requiresUnreferencedCode: true` 的 callee 时输出 `trim_warnings.annotationCount` 与
  `trim_warning.annotation[] reason=requires-unreferenced-code`；新增 annotation warning 扫描模块，并让 callable
  provenance 支持 `GET_SUB_FUNCTION` slot 解析。
  RED/GREEN：RED 为 requires-unreferenced static-call fixture 缺少 annotation warning marker；GREEN 后标注 callee 输出
  1 条 warning，未标注 callee 为 0。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3，
  Unix-only smoke 为 0 failures / ignored；WSL gcc/clang source contracts 22/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5b-requires-unreferenced-code-warning.md`。
  备注：仍不声明 `@dynamically_accessed`、`@dynamic_dependency`、warning 抑制/提升、跨模块 annotation 或完整
  metadata sweep 完成。

- 2026-06-30 11:25:44 +08:00 · 10-S5A / 12-S5A reflectable annotation function roots ·
  状态：10-S5/12-S5 首个 annotation-root 子切片完成；完整 annotation dataflow、dynamic dependency 和 trim warning 策略仍未关闭。
  完成项目：AOT C code stripping 在裁剪前扫描 function decorator metadata，将 `reflectable: true` 记录为
  reflection annotation root，传入 static callable reachability graph，并用
  `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 保留 otherwise-unreachable function；生成 C 头部新增
  `code_stripping.annotationRoots` 与 `code_stripping.annotationRoot[index]` markers。MSVC shared 测试目标同步把
  backend reachability internal sources 直接编入 `zr_vm_aot_reachability_test`，避免 DLL 非导出内部符号导致链接失败。
  RED/GREEN：RED 为新 `zr_vm_aot_c_reflection_annotation_preserve_test` 缺少 annotation root markers；GREEN 后
  reflectable function 保留、未标注 function 裁剪，reachability reason 单测覆盖 annotation root。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  Windows MSVC Debug 同组 3/3；WSL gcc/clang `zr_vm_aot_c_source_contracts_test` 22/0；`git diff --check` 退出 0
  （仅 LF/CRLF 提示）。产出：`tests/acceptance/2026-06-30-aot-10-s5a-reflectable-annotation-function-root.md`。
  备注：本记录只覆盖函数级 `reflectable: true` metadata root；`@dynamically_accessed`、`@dynamic_dependency`、
  `@requires_unreferenced_code`、类型/成员级 DESCRIPTION 提升、未注解反射 warning 与完整 trim analyzer 仍待后续。

- 2026-06-30 10:49:57 +08:00 · 10-S2Y / 10-S3AC generated Method.Invoke bool three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `bool(bool,bool,bool)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_bool_three_arg_invokers.h/.c`，生成
  `zr_aot_try_invoke_bool_three_arg(...)`，用 MethodInfo signature、三个 `args` 类型和 `functionIndex`
  选择 bool three-arg typed helper；`backend_aot_c_typed_bool_three_arg_thunks.c` 同步识别当前
  `left && middle && right` cleanup-reset 短路 AND 形态；返回值通过 `ZrCore_Value_InitAsBool(...)`
  写入 boxed `outReturn`。
  RED/GREEN：RED 为 frame setup source contract 要求 bool three-arg invoker source 时，失败在
  `reflectionBoolThreeArgInvokersSourceText` 非空断言；GREEN 后 frame setup contracts 1/0、shared-library smoke 13/0，
  并验证 `all_truth(true,true,true)` 返回 `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2y-method-invoke-bool-three-arg-unbox-return-boxing.md`。
  备注：四参数及以上、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 10:13:18 +08:00 · 10-S2X / 10-S3AB generated Method.Invoke f64 three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `float(float,float,float)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：扩展 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，生成
  `zr_aot_try_invoke_f64_three_arg(...)`，用 MethodInfo signature、三个 `args` 类型和 `functionIndex`
  选择既有 f64 three-arg typed helper，再用 `ZrCore_Value_InitAsFloat(...)` 写入 boxed `outReturn`。
  `backend_aot_c_typed_f64_thunks.h` 暴露 three-arg/state-free eligibility predicate；
  `backend_aot_c_reflection_invokers.c` 只保留总调度编排，文件为 948 行；numeric three-arg 子模块为 296 行。
  RED/GREEN：RED 缺 f64 three-arg reflection bucket source contract；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_three_ratio(1.5,2.25,3.25)` 返回 `ZR_VALUE_TYPE_DOUBLE`/7.0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2x-method-invoke-f64-three-arg-unbox-return-boxing.md`。
  备注：bool 三参数桶、四参数及以上、object/inline 返回、numeric widening、完整签名桶、
  typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 09:52:48 +08:00 · 10-S2W / 10-S3AA generated Method.Invoke uint64 three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `uint64(uint64,uint64,uint64)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：扩展 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，生成
  `zr_aot_try_invoke_u64_three_arg(...)`，用 MethodInfo signature、三个 `args` 类型和 `functionIndex`
  选择既有 u64 three-arg typed helper，再用 `ZrCore_Value_InitAsUInt(...)` 写入 boxed `outReturn`。
  `backend_aot_c_typed_u64_three_arg_thunks.h` 提供 three-arg/state-free eligibility predicate；
  `backend_aot_c_reflection_invokers.c` 只保留总调度编排，文件为 944 行；numeric three-arg 子模块为 198 行。
  RED/GREEN：RED 缺 u64 three-arg reflection bucket source contract；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_three_unsigned(50,20,5)` 返回 `ZR_VALUE_TYPE_UINT64`/75。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2w-method-invoke-uint64-three-arg-unbox-return-boxing.md`。
  备注：f64/bool 三参数桶、四参数及以上、object/inline 返回、numeric widening、完整签名桶、
  typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 09:32:55 +08:00 · 10-S2V / 10-S3Z generated Method.Invoke int64 three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `int64(int64,int64,int64)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，生成
  `zr_aot_try_invoke_i64_three_arg(...)`，用 MethodInfo signature、三个 `args` 类型和 `functionIndex`
  选择既有 i64 three-arg typed helper，再用 `ZrCore_Value_InitAsInt(...)` 写入 boxed `outReturn`。
  `backend_aot_c_typed_i64_thunks.h` 暴露 three-arg/state-free eligibility predicate；`backend_aot_c_reflection_invokers.c`
  只保留总调度编排，文件为 940 行；新增 numeric three-arg 子模块 100 行。
  RED/GREEN：RED 缺 numeric three-arg reflection bucket source contract；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_three(10,20,12)` 返回 `ZR_VALUE_TYPE_INT64`/42。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2v-method-invoke-int64-three-arg-unbox-return-boxing.md`。
  备注：u64/f64/bool 三参数桶、四参数及以上、object/inline 返回、numeric widening、完整签名桶、
  typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 09:05:37 +08:00 · 10-S2U / 10-S3Y generated Method.Invoke bool-return numeric comparison two-arg argument unbox + return boxing buckets ·
  状态：generated reflection invoker `bool(int,int)`、`bool(uint,uint)`、`bool(float,float)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_bool_numeric_invokers.h/.c`，生成
  `zr_aot_try_invoke_bool_i64_two_arg(...)`、`zr_aot_try_invoke_bool_u64_two_arg(...)`、
  `zr_aot_try_invoke_bool_f64_two_arg(...)`，用 MethodInfo signature、两个 `args` 类型和 `functionIndex`
  选择既有 signed/unsigned/float comparison typed bool helper，再用 `ZrCore_Value_InitAsBool(...)` 写入 boxed `outReturn`。
  `backend_aot_c_reflection_invokers.c` 只保留总调度编排，文件为 935 行；新增 bool numeric 子模块 240 行。
  RED/GREEN：RED 缺 bool-return numeric reflection bucket source contract；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `less_values(3,7)`、`unsigned_after(9,4)`、`ratio_equal(2.5,2.5)` 均返回
  `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2u-method-invoke-bool-numeric-two-arg-unbox-return-boxing.md`。
  备注：三参数及以上、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 08:35:02 +08:00 · 10-S2T / 10-S3X generated Method.Invoke f64 two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker float(float, float) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_f64_two_arg(...)`，用 MethodInfo signature、两个 `args` 类型和
  `functionIndex` 选择既有 f64 two-arg typed helper；state-free helper 调
  `zr_aot_typed_f64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，stateful divide/modulo helper 保留 `state` 参数，
  再用 `ZrCore_Value_InitAsFloat(...)` 写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，
  但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 f64 two-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `sum_ratio(left: float, right: float): float` 的 reflection invoker 对 1.25/2.5 写出
  `ZR_VALUE_TYPE_DOUBLE`/3.75。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2t-method-invoke-f64-two-arg-unbox-return-boxing.md`。
  备注：bool-return numeric comparison buckets、三参数及以上、object/inline 返回、numeric widening、完整签名桶、
  typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 08:18:43 +08:00 · 10-S2S / 10-S3W generated Method.Invoke bool two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker bool(bool, bool) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_bool_two_arg(...)`，用 MethodInfo signature、两个 `args` 类型和
  `functionIndex` 选择既有 bool two-arg typed helper
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，再用 `ZrCore_Value_InitAsBool(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 bool two-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `same_truth(left: bool, right: bool): bool` 的 reflection invoker 对 true/true 写出
  `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2s-method-invoke-bool-two-arg-unbox-return-boxing.md`。
  备注：bool-return numeric comparison buckets、f64 二参数桶、三参数及以上、object/inline 返回、numeric widening、
  完整签名桶、typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 08:03:24 +08:00 · 10-S2R / 10-S3V generated Method.Invoke uint64 two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker uint64(uint64, uint64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_u64_two_arg(...)`，用 MethodInfo signature、两个 `args` 类型和
  `functionIndex` 选择既有 u64 two-arg typed helper；state-free helper 调
  `zr_aot_typed_u64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，需要错误路径的 divide/modulo helper 调
  `zr_aot_typed_u64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，再用 `ZrCore_Value_InitAsUInt(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 u64 two-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `sum_unsigned(left: uint, right: uint): uint` 的 reflection invoker 对 100/23 写出
  `ZR_VALUE_TYPE_UINT64`/123。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2r-method-invoke-uint64-two-arg-unbox-return-boxing.md`。
  备注：bool/f64 二参数桶、三参数及以上、object/inline 返回、numeric widening、完整签名桶、
  typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 07:45:38 +08:00 · 10-S2Q / 10-S3U generated Method.Invoke int64 two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker int64(int64, int64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_i64_two_arg(...)`，用 MethodInfo signature、两个 `args` 类型和
  `functionIndex` 选择既有 i64 two-arg typed helper；state-free helper 调
  `zr_aot_typed_i64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，需要错误路径的 divide/modulo helper 调
  `zr_aot_typed_i64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，再用 `ZrCore_Value_InitAsInt(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 i64 two-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `sum_values(left: int, right: int): int` 的 reflection invoker 对 20/22 写出
  `ZR_VALUE_TYPE_INT64`/42。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2q-method-invoke-int64-two-arg-unbox-return-boxing.md`。
  备注：u64/bool/f64 二参数桶、三参数及以上、object/inline 返回、numeric widening、完整签名桶、
  typed target ABI carrier、public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 07:21:33 +08:00 · 10-S2P / 10-S3T generated Method.Invoke f64 one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker float(float) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_f64_one_arg(...)`，用 MethodInfo signature、`args[0].type` 和
  `functionIndex` 选择 `zr_aot_typed_f64_fn_<index>(zr_aot_arg0)`，再用 `ZrCore_Value_InitAsFloat(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 f64 one-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `echo_ratio(value: float): float` 的 reflection invoker 对 1.75 写出
  `ZR_VALUE_TYPE_DOUBLE`/1.75。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2p-method-invoke-f64-one-arg-unbox-return-boxing.md`。
  备注：多参数、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 07:05:20 +08:00 · 10-S2O / 10-S3S generated Method.Invoke bool one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker bool(bool) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_bool_one_arg(...)`，用 MethodInfo signature、`args[0].type` 和
  `functionIndex` 选择 `zr_aot_typed_bool_fn_<index>(zr_aot_arg0)`，再用 `ZrCore_Value_InitAsBool(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 bool one-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `echo_truth(value: bool): bool` 的 reflection invoker 对 false 写出
  `ZR_VALUE_TYPE_BOOL`/false。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2o-method-invoke-bool-one-arg-unbox-return-boxing.md`。
  备注：多参数、其他标量参数、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 06:45:51 +08:00 · 10-S2-maint / 10-S3-maint reflection invoker emitter split ·
  状态：generated reflection invoker emitter 拆分完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_invokers.h/.c`，把当前 no-arg scalar return-boxing buckets 和
  i64/u64 one-arg argument-unbox buckets 从 `backend_aot_c_method_metadata.c` 迁出；`backend_aot_c_emitter.c`
  直接包含 invoker 头。`backend_aot_c_method_metadata.c` 从 948 行降到 552 行，新 invoker 文件 397 行。
  RED/GREEN：拆分前以现有 u64 one-arg source/runtime coverage 为基线；拆分后 WSL gcc frame setup contracts 1/0、
  shared-library smoke 13/0，且 CMake glob 重新配置纳入新文件。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2-maint-reflection-invoker-emitter-split.md`。
  备注：该切片只做模块边界整理，不新增签名桶、不改变 generated ABI；后续可继续按 10-S2/10-S3 增量补更多参数/返回桶。

- 2026-06-30 06:25:21 +08:00 · 10-S2N / 10-S3R generated Method.Invoke uint64 one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker uint64(uint64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_u64_one_arg(...)`，用 MethodInfo signature、`args[0].type` 和
  `functionIndex` 选择 `zr_aot_typed_u64_fn_<index>(zr_aot_arg0)`，再用 `ZrCore_Value_InitAsUInt(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 u64 one-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `echo_unsigned(value: uint): uint` 的 reflection invoker 对 101 写出
  `ZR_VALUE_TYPE_UINT64`/101。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2n-method-invoke-uint64-one-arg-unbox-return-boxing.md`。
  备注：多参数、其他标量参数、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续；后续 10-S2-maint 已拆出
  reflection invoker 生成模块。

- 2026-06-30 06:05:32 +08:00 · 10-S2M / 10-S3Q generated Method.Invoke int64 one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker int64(int64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_i64_one_arg(...)`，用 MethodInfo signature、`args[0].type` 和
  `functionIndex` 选择 `zr_aot_typed_i64_fn_<index>(zr_aot_arg0)`，再用 `ZrCore_Value_InitAsInt(...)`
  写入 boxed `outReturn`。unsupported case 仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 i64 one-arg reflection bucket source contract；GREEN 后 source contract 1/0、
  shared-library smoke 13/0，并验证 `echo(value: int): int` 的 reflection invoker 对 99 写出 `ZR_VALUE_TYPE_INT64`/99。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2m-method-invoke-int64-one-arg-unbox-return-boxing.md`。
  备注：多参数、其他标量参数、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 05:44:51 +08:00 · 10-S2L / 10-S3P generated Method.Invoke f64 no-arg return boxing bucket ·
  状态：generated reflection invoker f64/no-arg return boxing 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_f64_no_arg(...)`，用 MethodInfo signature 和 `functionIndex` 选择
  `zr_aot_typed_f64_fn_<index>()`，再用 `ZrCore_Value_InitAsFloat(...)` 写入 boxed `outReturn`。unsupported case
  仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 f64 reflection bucket source contract；GREEN 后 source contract 1/0、shared-library smoke 13/0，
  并验证 `ratio(): float` 的 reflection invoker 写出 `ZR_VALUE_TYPE_DOUBLE`/2.5。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2l-method-invoke-f64-no-arg-return-boxing.md`。
  备注：args unbox、object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 05:26:08 +08:00 · 10-S2K / 10-S3O generated Method.Invoke bool no-arg return boxing bucket ·
  状态：generated reflection invoker bool/no-arg return boxing 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_bool_no_arg(...)`，用 MethodInfo signature 和 `functionIndex` 选择
  `zr_aot_typed_bool_fn_<index>()`，再用 `ZrCore_Value_InitAsBool(...)` 写入 boxed `outReturn`。unsupported case
  仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 bool reflection bucket source contract；GREEN 后 source contract 1/0、shared-library smoke 13/0，
  并验证 `truth(): bool` 的 reflection invoker 写出 `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2k-method-invoke-bool-no-arg-return-boxing.md`。
  备注：args unbox、f64/object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 05:06:13 +08:00 · 10-S2J / 10-S3N generated Method.Invoke uint64 no-arg return boxing bucket ·
  状态：generated reflection invoker uint64/no-arg return boxing 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_u64_no_arg(...)`，用 MethodInfo signature 和 `functionIndex` 选择
  `zr_aot_typed_u64_fn_<index>()`，再用 `ZrCore_Value_InitAsUInt(...)` 写入 boxed `outReturn`。unsupported case
  仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 缺 u64 reflection bucket source contract；GREEN 后 source contract 1/0、shared-library smoke 13/0，
  并验证 `unsigned_answer(): uint` 的 reflection invoker 写出 `ZR_VALUE_TYPE_UINT64`/13。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  `git diff --check` 退出 0，仅有既有 LF/CRLF 规范化警告。
  产出：`tests/acceptance/2026-06-30-aot-10-s2j-method-invoke-uint64-no-arg-return-boxing.md`。
  备注：args unbox、bool/f64/object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 04:45:07 +08:00 · 10-S2I / 10-S3M generated Method.Invoke int64 no-arg return boxing bucket ·
  状态：generated reflection invoker int64/no-arg return boxing 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：生成 C 新增 `zr_aot_try_invoke_i64_no_arg(...)`，用 MethodInfo signature 和 `functionIndex` 选择
  `zr_aot_typed_i64_fn_<index>()`，再用 `ZrCore_Value_InitAsInt(...)` 写入 boxed `outReturn`。unsupported case
  仍回落完整 entry thunk，但不把 entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：初始 RED 缺 `value.h` include；naive target-return 捕获在 shared-library smoke 中失败为
  `Expected 42 Was 1`；修正后的 RED 缺 function-table-fed invoker emitter API；GREEN 后 source contract 1/0、
  shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  `git diff --check` 退出 0，仅有既有 LF/CRLF 规范化警告。
  产出：`tests/acceptance/2026-06-30-aot-10-s2i-method-invoke-int64-no-arg-return-boxing.md`。
  备注：args unbox、bool/u64/f64/object/inline 返回、numeric widening、完整签名桶、typed target ABI carrier、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 04:17:27 +08:00 · 10-S2H / 10-S3L token-driven Method.Invoke void return-slot canonicalization ·
  状态：public counted token-driven `Method.Invoke` void/no-return return-slot canonicalization 子切片完成；完整
  10-S2/10-S3/11-S2 仍未关闭。
  完成项目：counted dispatcher 在 registered invoker 返回后，若 MethodInfo signature 声明无返回值，则把最终
  `outReturn` 规范为 null；void/no-return 方法不再向 public output slot 泄出 invoker 误写值或旧槽值。
  `zr_vm_reflection_method_invoke_test` 新增 no-return signature 下 invoker 写出 int64 的规范化用例。
  RED/GREEN：RED 为 no-return signature 下旧 dispatcher 调用成功但保留 `outReturn.type=INT64`，WSL gcc 失败在
  `Expected 0 Was 5`；GREEN 后调用仍成功、invoker 调用一次，最终 `outReturn.type=NULL`。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection method invoke 5/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query` 均 4/4。
  产出：`tests/acceptance/2026-06-30-aot-10-s2h-method-invoke-void-return-slot.md`。
  备注：返回 box、typed return register 捕获、typed unbox、numeric widening、nullable/ownership/staticCType 兼容、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 04:06:53 +08:00 · 10-S2G / 10-S3K token-driven Method.Invoke required return-slot reset guard ·
  状态：public counted token-driven `Method.Invoke` required return-slot reset 子切片完成；完整 10-S2/10-S3/11-S2
  仍未关闭。
  完成项目：counted dispatcher 在 required return signature 下、调用 registered invoker 前清空 `outReturn`，
  让 return base-type post-guard 只接受 invoker 本次写出的返回值；预填 stale 返回槽不再能让调用误返回成功。
  `zr_vm_reflection_method_invoke_test` 新增 invoker 不写返回值的拒绝用例，`zr_vm_reflection_token_resolve_test`
  的 arity success fixture 改为由 synthetic invoker 显式写入返回。
  RED/GREEN：RED 为 bool return signature 下 `outReturn` 预填 bool、但 invoker 不写返回值时旧 dispatcher 仍返回 true，
  WSL gcc 失败在 `Expected FALSE Was TRUE`；GREEN 后 return slot 先被清空，未写返回值返回 false。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection method invoke 4/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query` 均 4/4。
  产出：`tests/acceptance/2026-06-30-aot-10-s2g-method-invoke-return-slot-reset.md`。
  备注：返回 box、typed return register 捕获、typed unbox、numeric widening、nullable/ownership/staticCType 兼容、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 03:47:49 +08:00 · 10-S2F / 10-S3J token-driven Method.Invoke return base-type guard ·
  状态：public counted token-driven `Method.Invoke` return base-type guard 子切片完成；完整 10-S2/10-S3/11-S2
  仍未关闭。
  完成项目：counted dispatcher 在 registered invoker 写出 `outReturn` 后，若 MethodInfo signature 声明 concrete
  非 null/unknown `returnType->baseType`，要求 `outReturn->type` 匹配；越界 return baseType 拒绝成功结果。
  RED/GREEN：RED 为 bool return signature 下 synthetic invoker 写出 int64 时旧 dispatcher 仍返回 true，WSL gcc
  失败在 `Expected FALSE Was TRUE`；GREEN 后 mismatch 返回 false，写出 bool 后返回 true。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection method invoke 3/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2f-method-invoke-return-base-type-guard.md`。
  备注：返回 box、无副作用预检、typed unbox、numeric widening、nullable/ownership/staticCType 兼容、
  public method reflection object、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 03:37:13 +08:00 · 10-S2E / 10-S3I token-driven Method.Invoke fixed parameter base-type guard ·
  状态：public counted token-driven `Method.Invoke` fixed parameter base-type guard 子切片完成；完整 10-S2/10-S3/11-S2
  仍未关闭。
  完成项目：counted dispatcher 在 token→MethodInfo/function pointer/invoker binding、arity 与 shape 均通过后，
  对 fixed `parameterTypes[i].baseType` 与 `args[i].type` 做 concrete baseType 等值 guard；null/unknown/untyped slot
  与 varargs 额外参数暂不强制，越界 baseType 直接拒绝。`zr_vm_reflection_method_invoke_test` 新增错类型参数拒绝用例。
  RED/GREEN：RED 为 bool 参数签名传入 int64 时旧 dispatcher 仍返回 true 并调用 invoker，WSL gcc 失败在
  `Expected FALSE Was TRUE`；GREEN 后错类型被拒绝，修正为 bool 后正常派发。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection method invoke 2/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2e-method-invoke-parameter-base-type-guard.md`。
  备注：typed unbox、numeric widening、nullable/ownership/staticCType 兼容、返回 box、public method reflection object、
  cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 03:22:37 +08:00 · 10-S2D / 10-S3H token-driven Method.Invoke signature shape guard ·
  状态：public counted token-driven `Method.Invoke` signature shape guard 子切片完成；完整 10-S2/10-S3/11-S2
  仍未关闭。
  完成项目：`ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)` 在 token→MethodInfo/function pointer/invoker
  binding 成功后、调用 registered invoker 前，拒绝 incomplete MethodInfo signature shape：fixed 参数数量大于 0
  时必须有 `parameterTypes`，声明返回值时必须有 `returnType`；新增独立 focused
  `zr_vm_reflection_method_invoke_test` 覆盖 shape 防御边界。
  RED/GREEN：RED 为新 focused 测试要求缺 `parameterTypes` / `returnType` 的 signature 返回 false，但旧 dispatcher
  仍调用 invoker，WSL gcc 失败在 `Expected FALSE Was TRUE`；GREEN 后补齐 shape 才派发。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection method invoke 1/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2d-method-invoke-signature-shape-guard.md`。
  备注：typed argument compatibility、参数 unbox、返回 box、public method reflection object、MethodSpec 专用 code slot、
  cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 03:09:55 +08:00 · 10-S2C / 10-S3G token-driven Method.Invoke signature arity guard ·
  状态：public counted token-driven `Method.Invoke` signature arity guard 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)`，复用 token→MethodInfo/function pointer/invoker
  binding，再用 `methodInfo->signature->parameterCount/hasVarArgs` 在 dispatch 前拦截参数数量不足、过多和非零
  argCount 下的 null `args`；varargs 路径允许 `argCount >= parameterCount`。
  RED/GREEN：RED 为 focused reflection token resolve 测试先引用缺失 counted dispatcher API，WSL gcc 出现 implicit
  declaration 和 undefined reference；GREEN 后 exact arity、mismatch arity、null args 与 varargs 扩展路径通过，当前测试数
  升为 7/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection token resolve 7/0、method binding 2/0、metadata runtime query
  24/0；WSL gcc 与 MSVC Debug CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s2c-method-invoke-signature-arity-guard.md`。
  备注：参数类型 unbox、返回 box、signature type validation、public method reflection object、MethodSpec 专用 code slot、
  cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 02:57:41 +08:00 · 10-S2B / 10-S3F token-driven Method.Invoke dispatcher ·
  状态：public token-driven `Method.Invoke` dispatcher 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_InvokeMethodToken(...)`，通过 `ZrCore_Reflection_ResolveToken()` 找到
  method token 的 MethodInfo、entry thunk 与 invoker binding，再把 state、target、method、self、args、outReturn
  派发给 registered AOT invoker。测试捕获 invoker 实参并覆盖 null/非 method token 拒绝路径。
  RED/GREEN：RED 为 focused reflection token resolve 测试先引用缺失 dispatcher API，WSL gcc 出现 implicit
  declaration 和 undefined reference；GREEN 后 dispatcher path 通过，当前测试数升为 6/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection token resolve 6/0、method binding 2/0、metadata runtime query
  24/0；WSL gcc 与 MSVC Debug CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s2b-token-driven-method-invoke-dispatcher.md`。
  备注：参数 unbox、返回 box、签名校验、public method reflection object、MethodSpec 专用 code slot、cross-module
  token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 02:28:55 +08:00 · 12-S7ZT runtime fallback warning reason-mask aggregate markers ·
  状态：12-S7 runtime fallback diagnostics 子切片完成；完整 12-S7 仍未关闭。
  完成项目：AOT C header 在 runtime fallback warning count/suppressed count 之外输出
  `trim_warnings.runtimeFallbackReasonMask` 与
  `trim_warnings.runtimeFallbackSuppressedReasonMask`，分别聚合可见 warning 和被 suppression 隐藏的
  `ZR_AOT_RUNTIME_FALLBACK_WARNING_*` reason bits。这样被抑制 warning 不再只剩 count，诊断消费端可直接
  判断 dynamic-call 或 dynamic-value-access reason 是否被隐藏。
  RED/GREEN：RED 为 dynamic deopt bridge smoke 要求 visible dynamic-call mask=1、suppressed
  dynamic-call mask=1、visible dynamic-value-access mask=2 后，旧生成器缺 marker，WSL gcc focused
  失败 4/7；GREEN 后 helper 聚合 reason mask，emitter 输出 header marker，逐条 warning、full-AOT
  拒绝和 hybrid fallback 行为保持。
  验证：WSL gcc/clang dynamic deopt bridge smoke 7/0、code stripping 5/0；Windows MSVC Debug
  dynamic deopt bridge smoke 0 failures/7 ignored、code stripping 5/0；focused CTest
  `aot_c_code_stripping` 在 WSL gcc 与 Windows MSVC Debug 均 1/1；`git diff --check` 退出 0。
  产出：`tests/acceptance/2026-06-30-aot-12-s7zt-runtime-fallback-reason-mask-aggregates.md`。
  备注：完整 trim analyzer、reflection data-flow annotation、annotation warning policy、cross-module
  export-token rewrite 和完整 metadata sweep/pruning 仍待后续。

- 2026-06-30 02:05:51 +08:00 · 10-S3E / 11-S2D MethodSpec underlying method binding carrier ·
  状态：public MethodSpec token resolver underlying method binding carrier 子切片完成；完整 10-S2/10-S3/11-S2
  仍未关闭。
  完成项目：`ZrCore_Reflection_ResolveToken()` 对 MethodSpec `SIGNATURE` token 保留 MethodSpec 签名身份、
  generic signature hash 和实参列表，同时用 underlying `methodToken` 消费 11-S2D binding view，在存在 AOT
  registration 时填充 `methodFunctionIndex`、`methodInfo`、`methodFunctionPointer` 和 `methodInvoker`。
  RED/GREEN：RED 为 MethodSpec resolved-token 测试新增 MethodInfo/function pointer/invoker 断言后失败在
  `methodFunctionIndex == 0`；GREEN 后 MethodSpec token 解析同时携带 MethodSpec metadata carrier 与 underlying
  MethodDef AOT 调用载体。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection token resolve 5/0、method binding 2/0、metadata runtime query
  24/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3e-methodspec-underlying-method-binding-carrier.md`。
  备注：public generic method reflection object、`Method.Invoke` marshaling、MethodSpec 专用 code slot、
  cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 01:54:46 +08:00 · 10-S3D / 11-S2D method binding reflection carrier ·
  状态：public token resolver method binding carrier 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：`SZrReflectionResolvedToken` 暴露 `methodFunctionIndex`、`methodInfo`、`methodFunctionPointer`、
  `methodInvoker`；`ZrCore_Reflection_ResolveToken()` 对普通 MethodDef/MethodRef token 先保持 method
  record/signature identity，再在存在 AOT method binding 时消费 11-S2D runtime view 填充 MethodInfo、entry thunk
  与 invoker；无 binding 时仍成功返回 method record 且 binding 字段为空。
  RED/GREEN：RED 为 reflection token resolve 测试要求新增 public method binding 字段后 WSL gcc 编译失败；
  GREEN 后有 binding 与无 binding MethodDef token 解析均通过。
  验证：WSL gcc/clang/MSVC Debug 均通过 reflection token resolve 5/0、method binding 2/0、metadata runtime query
  24/0；WSL gcc 与 MSVC Debug CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s3d-method-binding-reflection-carrier.md`。
  备注：public method reflection object、`Method.Invoke` marshaling、MethodSpec runtime binding、cross-module token
  rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 01:38:26 +08:00 · 11-S2D / 10-S2 method token binding view ·
  状态：runtime 内部 method token→MethodInfo/function pointer/invoker binding view 子切片完成；完整
  10-S2/10-S3/11-S2 仍未关闭。
  完成项目：新增 `SZrMetadataRuntimeMethodBindingView`、
  `ZrCore_MetadataRuntime_ReadMethodBindingView()` 和实现文件 `metadata_runtime_method_binding.c`；该入口从
  attached code registration 的 `methodTokens[]`、`methodInfos[]`、`functionPointers[]` 绑定唯一 local
  `MEMBER_DEF` token 到 function slot、MethodInfo、entry thunk 和 invoker，并对缺失表、重复 token、非 method
  token、stale MethodInfo slot、缺 thunk/invoker 清空输出并返回 false。新增 focused Unity 测试目标和 CTest。
  RED/GREEN：RED 为新增 focused 测试要求缺失 view/API 后 WSL gcc 编译失败；GREEN 后成功路径与负向防御路径通过。
  验证：WSL gcc 通过 method binding 2/0、metadata runtime query 24/0、reflection token resolve 4/0、CTest
  `metadata_runtime_method_binding` 1/1；WSL clang 通过 2/0、24/0、4/0；Windows MSVC Debug 通过 2/0、24/0、4/0
  以及 CTest 1/1。
  产出：`tests/acceptance/2026-06-30-aot-11-s2d-method-token-binding-view.md`。
  备注：public `ResolveToken()` 尚未暴露 MethodInfo/function pointer，`Method.Invoke` marshaling、public method
  reflection object、MethodSpec runtime binding、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 01:05:20 +08:00 · 11-S2B / 10-S2 method token code-registration carrier ·
  状态：11-S2 code-registration method token carrier 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。
  完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 11u`；`SZrAotCodeRegistration` 与 `ZrAotCompiledModule`
  暴露 `methodTokens/methodTokenCount`；`SZrMetadataRuntime` mirror `methodTokenCount`；AOT C 生成
  `zr_aot_method_tokens[]`，按 `functionIndex` 对齐 `methodInfos[]`，root module typed exported function 填充真实
  `MEMBER_DEF` token（smoke 中为 `0x03000001u`），不可靠槽位写 `0u`；runtime descriptor validation 拒绝
  method token table 指针/计数不一致或形态错误。
  RED/GREEN：RED 为 AOT C frame setup source contract 要求 method token ABI/emitter/runtime validation 后缺少
  codeRegistration method token mismatch 文本；GREEN 后 ABI/source contract、metadata runtime mirror、生成共享库和
  descriptor validation 均通过。
  验证：WSL gcc、WSL clang 均通过 metadata runtime query 24/0、AOT C source contracts 22/0、frame setup contracts
  1/0、shared-library smoke 13/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过前三项 24/0、22/0、1/0，
  shared-library smoke 13 项 ignored，descriptor diagnostics 2 项 ignored。
  产出：`tests/acceptance/2026-06-30-aot-11-s2b-method-token-carrier.md`。
  备注：public method reflection object、token-driven `Invoke`、MethodInfo/function pointer/invoker binding、
  cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 00:31:26 +08:00 · 10-S3C / 11-S3 method signature reflection carrier ·
  状态：10-S3 token-driven method signature carrier 子切片完成，并消费 11-S3C signature record resolver 与
  11-S3M MethodSpec signature view；完整 10-S3/11-S3 仍未关闭。
  完成项目：`SZrReflectionResolvedToken` 暴露 `methodSignatureToken`、`methodSignatureRecord` 与
  `methodSignatureHash`；普通 MethodDef/MethodRef token 解析时复用
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()` 填充 paired method signature identity；MethodSpec
  `SIGNATURE` token 则把自身 token/record/hash 作为 method signature identity 暴露。
  RED/GREEN：RED 为 reflection token resolve 测试要求 MethodDef/MethodSpec signature carrier 后得到 WSL gcc
  编译失败；GREEN 后普通方法签名 identity 与 MethodSpec 签名 identity 均通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 metadata runtime query 24/0、reflection token resolve 4/0、
  metadata runtime TypeSpec layout 14/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3c-method-signature-reflection-carrier.md`。
  备注：public method reflection object、token-driven `Invoke`、MethodInfo/function pointer 绑定、名表→token 重写、
  cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 00:17:04 +08:00 · 10-S3B / 11-S5 MethodSpec token resolver carrier ·
  状态：10-S3 MethodSpec token-driven resolver 子切片完成，并消费 11-S5 MethodSpec signature view；完整 10-S3/11-S5
  仍未关闭。
  完成项目：`SZrMetadataRuntimeMethodSpecSignatureView` 暴露 MethodSpec signature record；`SZrReflectionResolvedToken`
  暴露 method token/record；`ZrCore_Reflection_ResolveToken()` 现在可把通过
  `GENERIC_INST(MEMBER_REF methodToken, args...)` 验证的 MethodSpec `SIGNATURE` token 解析为 method-like carrier，
  保留 MethodSpec record、underlying method identity、signature hash、argument count 与 argument-list blob offset。
  RED/GREEN：RED 为 reflection token resolve 与 metadata runtime query 分别要求缺失的 method carrier 字段和
  `methodSpecRecord` 后得到 WSL gcc 编译失败；GREEN 后 MethodSpec `ResolveToken()` 和既有 generic argument view
  同时通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 metadata runtime query 24/0、reflection token resolve 4/0、
  metadata runtime TypeSpec layout 14/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3b-methodspec-token-resolve-carrier.md`。
  备注：public generic method reflection object、MethodSpec runtime binding、runtime generic layout construction、
  cross-module token publication/rewrite 和 full trim analyzer 仍待后续。

- 2026-06-29 23:56:22 +08:00 · 11-S5 / 10-S4E MethodSpec generic argument view and reflection carrier ·
  状态：11-S5 MethodSpec indexed generic argument runtime view 与 10-S4E public reflection carrier 子切片完成；
  完整 10-S4/11-S5 仍未关闭。
  完成项目：新增 `SZrMetadataRuntimeMethodSpecGenericArgumentView`、
  `ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView()`、`SZrReflectionResolvedMethodSpecGenericArgument` 和
  `ZrCore_Reflection_ResolveMethodSpecGenericArgument()`，可从
  `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec signature 按 index 暴露 primitive argument node
  或 direct TypeDef/TypeRef argument token/record，并保留 method token/record 与 signature hash。
  RED/GREEN：RED 为 metadata runtime query 与 reflection token resolve 新增 MethodSpec generic argument fixture
  后缺少类型/API 导致 WSL gcc 编译失败；GREEN 后 primitive argument、TypeRef argument、null/未 attach/wrong
  token/out-of-range 负向路径均通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 metadata runtime query 24/0、reflection token resolve
  4/0、metadata runtime TypeSpec layout 14/0。
  产出：`tests/acceptance/2026-06-29-aot-11-s5-methodspec-generic-argument-view.md`。
  备注：public generic method reflection object、MethodSpec runtime binding、runtime generic layout construction、
  cross-module token publication/rewrite 和 full trim analyzer 仍待后续。

- 2026-06-29 23:34:03 +08:00 · 10-S4D / 11-S5 public GenericParam constraint reflection carrier ·
  状态：10-S4 泛型参数/约束 public carrier 子切片完成，并消费 11-S5 runtime view；完整 10-S4/11-S5 仍未关闭。
  完成项目：新增 `SZrReflectionResolvedGenericParameter`、
  `SZrReflectionResolvedGenericParameterConstraint`、`ZrCore_Reflection_ResolveGenericParameter()` 和
  `ZrCore_Reflection_ResolveGenericParameterConstraint()`，按 TypeDef/MethodDef owner + parameter/constraint
  index 暴露 GenericParam row/name/flags/constraint range、constraint type token/record 与可选 signature blob。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 新增 GenericParam/GenericParamConstraint public carrier
  fixture 后缺少类型/API 导致 WSL gcc 编译失败；GREEN 后反射 carrier 用例通过。
  验证：WSL gcc/clang 均通过 reflection token resolve 3/0；WSL gcc 通过 metadata runtime query 23/0 与
  TypeSpec layout 14/0；Windows MSVC Debug 通过同三项 3/0、23/0、14/0。
  产出：`tests/acceptance/2026-06-29-aot-10-s4d-generic-parameter-reflection-carrier.md`。
  备注：public generic reflection object、MethodSpec runtime binding、runtime generic layout construction、
  cross-module token publication/rewrite 和 full trim analyzer 仍待后续。

- 2026-06-29 23:20:13 +08:00 · 11-S5 generic parameter runtime metadata views ·
  状态：11-S5 泛型参数/约束运行时只读 view 子切片完成；完整 11-S5 仍未关闭，MethodSpec 标准化实例签名、
  与 08 实例化去重键的全链路统一、运行期 generic entity/layout materialization、public generic reflection
  object 和跨模块 generic binding 仍待后续。
  完成项目：新增 `SZrMetadataRuntimeGenericParamView` /
  `SZrMetadataRuntimeGenericParamConstraintView` 与对应 read API；runtime 现在能从 attached zrp
  `GENERIC_PARAMS` / `GENERIC_PARAM_CONSTRAINTS` 读取 TypeDef/MethodDef owner 的参数定义、name/flags、
  constraint range、constraint type record 与可选 validated signature blob，并拒绝未 attach/越界/owner
  range 不匹配路径。
  RED/GREEN：`zr_vm_metadata_runtime_query_test` 新增泛型参数/约束 view 测试后先因缺失类型/API 编译失败；
  新实现转绿后补充 name/flags 与零长度约束 signature blob 断言。
  验证：WSL gcc/clang 均通过 metadata runtime query 23/0；Windows MSVC Debug 通过 metadata runtime
  query 23/0；同一生产变更下 WSL gcc 与 Windows MSVC Debug 均通过 reflection token resolve 2/0 和
  metadata runtime TypeSpec layout 14/0。
  产出：`tests/acceptance/2026-06-29-aot-11-s5-generic-param-runtime-views.md`。
  备注：本记录不声明 MethodSpec 运行期实例绑定、泛型 layout 构建、full trim analyzer、
  cross-module token publication/rewrite 或完整 11-S5 关闭。

- 2026-06-29 14:40:45 +08:00 · 07-S2/S5 bool call-result stack-copy direct-call propagation ·
  状态：07 typed bool direct-call / scalar-local consumer 子切片完成；完整 07 M1.5 仍有更广 byte-frame
  narrowing、GC roots/exports、SZrValue 归零、字段布局 resolver 和性能门槛未关闭；08-12 继续按计划推进。
  完成项目：AOT scalar-local inference now propagates call-result kinds through stack-copy destinations and their
  downstream branch/truthiness consumers, so bool typed call results copied before a bool branch remain eligible for
  typed direct-call lowering instead of falling back to `CallStaticDirect` plus i64 sync.
  RED/GREEN：`zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test` 先在 28 个用例中 25 个失败，
  首个失败是缺少 bool direct-call marker；修复后 WSL gcc/clang 均为 28/0。
  验证：WSL gcc/clang 均通过 typed direct-call bool smoke 28/0、typed call contracts 4/0、call contracts
  8/0、source contracts 22/0、logical shared-library smoke 6/0、call shared-library smoke 5/0；Windows MSVC
  Debug 通过 typed call contracts 4/0、call contracts 8/0、source contracts 22/0，typed bool smoke 为
  0 failures / 28 ignored。
  产出：`tests/acceptance/2026-06-29-aot-07-s2-s5-bool-call-result-stack-copy-direct-call.md`。
  备注：本记录不关闭完整 trim analyzer、cross-module token rewrite、runtime generic layout construction、
  full frame-free proof 或完整 07-S7 performance gate。

- 2026-06-29 14:21:20 +08:00 · 07-S3/S4 / 11-S4G runtime CopyStack registry layout resolver ·
  状态：07 runtime CopyStack inline-struct fallback 与 11 function-level layout resolver 支撑子切片完成；
  07/11 仍未全量关闭，字段布局 resolver、完整 byte-frame narrowing、GC roots/exports、runtime generic
  layout construction、持久 cTypeId→token 索引和跨模块 token publication/rewrite 仍待后续 ·
  完成项目：`ZrLibrary_AotRuntime_CopyStack()` 现在用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, destinationLayout->typeLayoutId)` 校验
  inline-struct stack-copy fallback 的 generated-frame layout，不再通过 prototype layout cache ·
  RED/GREEN：source contract 新增 `aot_runtime_values.c` metadata runtime include/resolver needle 后 RED；
  runtime helper 替换 include 与 resolver 后 GREEN · 验证：WSL gcc/clang 均通过 source contracts 22/0、
  call shared-library smoke 5/0、value-type smoke 5/0；Windows MSVC Debug 通过 source contracts 22/0、
  call smoke 5/0/5 ignored、value-type smoke 5/0/1 ignored。
  产出：`tests/acceptance/2026-06-29-aot-07-s3-s4-runtime-copy-stack-registry-layout.md`。
  备注：本记录不声明字段布局 resolver、object-to-inline copy 语义变更、public reflection object、
  runtime layout construction、cross-module token rewrite 或完整裁剪分析完成。

- 2026-06-29 14:11:34 +08:00 · 07-S3/S4 / 11-S4G runtime inline-struct call/return registry layout resolver ·
  状态：07 runtime inline-struct call/return boundary 与 11 function-level layout resolver 支撑子切片完成；
  07/11 仍未全量关闭，字段布局 resolver、完整 byte-frame narrowing、GC roots/exports、runtime generic
  layout construction、持久 cTypeId→token 索引和跨模块 token publication/rewrite 仍待后续 ·
  完成项目：`ZrLibrary_AotRuntime_CallInlineStruct()`、`CallInlineStructDynamicDeoptBridge()` 与
  `ReturnInlineStruct()` 现在用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, typeLayoutId)` 校验 generated-frame
  inline struct destination/source layout，不再通过 prototype layout cache；return contract 同步当前
  `codeRegistration->functionPointers` frame thunk ABI · RED/GREEN：先修正 return contract 里既有 descriptor
  thunk needle 漂移，再由新增 metadata-runtime resolver needle 得到 RED；runtime helper 替换后 GREEN ·
  验证：WSL gcc/clang 均通过 return contracts 1/0、source contracts 22/0、value SemIR contracts 4/0、
  call shared-library smoke 5/0、value-type smoke 5/0；Windows MSVC Debug 通过 return contracts 1/0、
  source contracts 22/0、value SemIR contracts 4/0、call smoke 5/0/5 ignored、value-type smoke 5/0/1 ignored。
  产出：`tests/acceptance/2026-06-29-aot-07-s3-s4-runtime-inline-struct-call-return-registry-layout.md`。
  备注：本记录不声明字段布局 resolver、public reflection object、runtime layout construction、cross-module token
  rewrite 或完整裁剪分析完成。

- 2026-06-29 13:57:57 +08:00 · 07-S3/S4 / 11-S4G value SemIR registry layout resolver ·
  状态：07 value SemIR copy/field transfer 与 11 function-level layout resolver 支撑子切片完成；07/11
  仍未全量关闭，typed call/return resolver、完整 byte-frame narrowing、GC roots/exports、runtime generic
  layout construction、持久 cTypeId→token 索引和跨模块 token publication/rewrite 仍待后续 ·
  完成项目：generated value SemIR inline `COPY_VALUE` 与 nested inline-struct field load/store 现在使用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, typeLayoutId)`，不再由这些 value SemIR
  helper 输出 `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, ..., state)`。这把 inline
  copy/field transfer consumer 接到 AOT attached code-registration layout registry，与 cleanup/GC/generic
  dictionary/descriptor consumer 使用同一 layout table · RED/GREEN：RED 为 source/value SemIR contracts
  缺少 metadata-runtime resolver；GREEN 后 source/value SemIR contracts 要求 metadata resolver 并禁止旧
  prototype type-layout resolver · 验证：WSL gcc/clang 均通过 source contracts 22/0、value SemIR contracts
  4/0、value-type shared-library smoke 5/0；Windows MSVC Debug 通过 source contracts 22/0、value SemIR
  contracts 4/0、value-type smoke 5/0/1 ignored。生成产物检查确认 value-type generated C 中这些 lookup
  没有旧 `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function`。产出：
  `tests/acceptance/2026-06-29-aot-07-s3-s4-value-semir-registry-layout.md`。
  备注：本记录不声明字段布局 resolver、typed call/return、public reflection object、runtime layout
  construction、cross-module token rewrite 或完整裁剪分析完成。

- 2026-06-29 13:44:33 +08:00 · 07-S3/S4 / 11-S4G frame cleanup registry layout resolver ·
  状态：07 value-frame cleanup 与 11 function-level layout resolver 支撑子切片完成；07/11 仍未全量关闭，
  value SemIR copy/field resolver 迁移、完整 byte-frame narrowing、GC roots/exports、runtime generic layout
  construction、持久 cTypeId→token 索引和跨模块 token publication/rewrite 仍待后续 ·
  完成项目：generated C 现在 include `zr_vm_core/metadata_runtime.h`，并让 value-frame cleanup 的 inline-struct
  drop lookup 使用 `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, typeLayoutId)`，不再由 cleanup
  emitter 输出 `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, ..., state)`。这把 cleanup drop
  consumer 接到 AOT attached code-registration layout registry，与 GC/generic dictionary/descriptor consumer 使用同一
  layout table · RED/GREEN：RED 为 source contract 缺少 metadata-runtime include；GREEN 后 cleanup source contract
  要求 metadata resolver 并禁止 cleanup 旧 resolver · 验证：WSL gcc/clang 均通过 source contracts 22/0、frame setup
  contracts 1/0、value-type shared-library smoke 5/0；Windows MSVC Debug 通过 source contracts 22/0、frame setup
  contracts 1/0、value-type smoke 5/0/1 ignored。生成产物检查确认 cleanup blocks 出现
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, ...)`。产出：
  `tests/acceptance/2026-06-29-aot-07-s3-s4-frame-cleanup-registry-layout.md`。
  备注：本记录不声明 07 frame-free closure、value SemIR copy/field helper 迁移、public reflection object、
  runtime layout construction、cross-module token rewrite 或完整裁剪分析完成。

- 2026-06-29 13:22:39 +08:00 · 10-S4 / 11-S4R-union generated union ownership offset table ·
  状态：10-S4/11-S4 owner-field layout 支撑子切片完成；10-S4/11-S4 仍未全量关闭，public
  `FieldInfo` 对象、字段值读写 marshaling、完整 generic reflection、runtime generic layout construction、
  持久 cTypeId→token 索引和跨模块 token publication/rewrite 仍待后续 ·
  完成项目：AOT C generated `SZrTypeLayout` descriptor 现在可为 union owner payload fields 生成
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并将 `.ownershipFieldOffsets` 指向该表；union active payload
  仍由 `SZrTypeLayoutField.activeTag` 与 tag metadata 表达 · RED/GREEN：RED 为 value-type shared-library
  smoke 新增 `Shared<Box>` union payload fixture 后缺少 generated `ZrOwnershipOffsets_`；GREEN 后
  generated union descriptor 包含 ownership offset table、`.kind = 2u`、`.ownershipFieldCount = 1u`
  和 `.ownershipFieldOffsets = ZrOwnershipOffsets_<id>` · 验证：WSL gcc/clang 均通过 type-layout CTest
  1/1、source contracts 22/0、value-type smoke 5/0；Windows MSVC Debug 通过 type-layout CTest 1/1、
  source contracts 22/0、value-type smoke 5/0/1 ignored。产出：
  `tests/acceptance/2026-06-29-aot-11-s4r-union-ownership-offset-table.md`。
  备注：本记录不声明 public reflection entity、runtime generic layout construction、cross-module token
  publication/rewrite 或完整裁剪分析完成。

- 2026-06-29 13:03:17 +08:00 · 10-S4C / 11-S4L/N FieldDef public reflection carrier ·
  状态：10-S4 字段实体支撑子切片完成；10-S4/11-S4 仍未全量关闭，public `FieldInfo` 对象、
  字段值读写 marshaling、完整 generic reflection、跨模块 token publication/rewrite 和 trim analyzer 仍待后续 ·
  完成项目：`SZrReflectionResolvedToken` FieldDef 结果现在暴露 owner type record/row 与 field type
  token/record；field type token 复用 metadata runtime 的 layoutId→TypeDef/TypeSpec token resolver，保持
  offset/layout/type identity 的单一真相 · RED/GREEN：RED 为 focused reflection token resolver 测试引用缺失
  FieldDef owner/type carrier 字段后编译失败；GREEN 后 owner TypeDef + field-type TypeDef + FieldDef metadata
  路径、TypeSpec generic argument 既有路径和 out-of-range 负向路径通过 · 验证：WSL gcc/clang 与 Windows
  MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。产出：
  `tests/acceptance/2026-06-29-aot-10-s4-fielddef-public-reflection-carrier.md`。
  备注：本记录不声明 public field reflection object、字段访问 marshaling、metadata format 变更或完整裁剪完成。

- 2026-06-29 12:51:49 +08:00 · 10-S4B public TypeSpec generic argument carrier ·
  状态：10-S4 支撑子切片完成；10-S4/11-S3/11-S4 仍未全量关闭，public generic type
  reflection object、`MakeGenericType` runtime construction、recursive generic argument semantic binding、
  TypeSpec/generic runtime materialization、跨模块 token publication/rewrite 和注解驱动保留策略仍待后续 ·
  完成项目：`SZrReflectionResolvedToken` 对 TypeSpec token 现在暴露 generic base token、signature token/hash
  和 argument count；新增 `ZrCore_Reflection_ResolveTypeSpecGenericArgument(...)` 与
  `SZrReflectionResolvedGenericArgument`，可按 index 暴露 primitive signature argument 或 direct TypeRef/TypeDef
  argument token/record · RED/GREEN：RED 为 focused reflection token resolver 测试引用缺失 TypeSpec generic
  carrier 字段和 indexed argument API 后编译失败；GREEN 后 TypeSpec base/argument-count、primitive argument、
  TypeRef argument 和 out-of-range argument 负向路径通过 · 验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。产出：
  `tests/acceptance/2026-06-29-aot-10-s4-public-typespec-generic-argument-reflection.md`。
  备注：本记录不声明泛型反射对象、运行期泛型实例构造、metadata format 变更或完整 trim analyzer 完成。

- 2026-06-29 12:37:45 +08:00 · 10-S3A public reflection token resolver carrier ·
  状态：10-S3 支撑子切片完成；10-S3/10-S4 与 11-S4 仍未全量关闭，名表→token 重写、public
  reflection object materialization、`Invoke` 注册表消费、trim warning/annotation flow、完整
  TypeSpec/generic layout materialization 和跨模块 token publication/rewrite 仍待后续 ·
  完成项目：新增 public `ZrCore_Reflection_ResolveToken(...)` 与 `SZrReflectionResolvedToken` carrier，
  以 `SZrMetadataRuntime` 为入口解析 TypeDef/TypeSpec/TypeRef、FieldDef 与 MethodDef/MethodRef；
  TypeDef/TypeSpec/FieldDef 路径直接复用 11-S4 registry-backed layout/binding view，method token 先返回
  record-only entity · RED/GREEN：RED 为 focused reflection token resolver 测试引用缺失 API/enum/carrier 后
  编译失败；GREEN 后 TypeDef、FieldDef、MethodDef 与 invalid input 路径通过 · 验证：WSL gcc/clang 与
  Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。产出：`tests/acceptance/2026-06-29-aot-10-s3-public-reflection-token-resolve.md`。
  备注：本记录不声明反射对象物化、按名查找裁剪安全、dynamic invoke 或 metadata format 变更完成。

- 2026-06-29 11:56:53 +08:00 · 07-S2/S4 call-result truthiness value-slot sync elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic truthiness consumer scan
  现在把 `JUMP_IF` / `LOGICAL_NOT` 纳入 i64/u64/f64 scalar-local 读取证明，使 no-arg typed i64
  call result 在 `!zero()`、`if (zero())`、`!one()` 链路中只保留 `zr_aot_s*` direct-call local，
  不再额外同步到 `SZrValue` value slot。生成 C 已禁止 `zr_aot_typed_destination`、
  `ZR_VALUE_FAST_SET(zr_aot_typed_destination, ...)` 和
  `zr_aot_static_i64_no_arg_direct_call_sync_stack_slot` · RED/GREEN：RED 由 logical shared
  truthiness smoke 捕获旧 typed call-result sync marker；GREEN 后 WSL GCC logical shared smoke
  6/0、logical contracts 4/0、generic LOGICAL_NOT numeric local smoke 1/0、generic JUMP_IF smoke 3/0、
  call shared smoke 5/0、call contracts 8/0；WSL Clang 同组 6/0、4/0、1/0、3/0、5/0、8/0；
  Windows MSVC Debug logical contracts 4/0、call contracts 8/0，Unix-only logical shared smoke
  0 failures / 6 ignored。产出：
  `tests/acceptance/2026-06-29-aot-07-s2-s4-call-result-truthiness-value-slot-sync-elision.md`。
  备注：不声明 07-S2/S4 全量完成；dynamic/string/object truthiness、value-copy 迁移、
  GC roots/exports/frame cleanup、byte-frame 更广收窄、性能计数仍待后续。

- 2026-06-29 11:29:46 +08:00 · 07-S2/S4 call-result truthiness local propagation ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `LOGICAL_NOT` 现在可从 no-arg
  typed callee 反向恢复 call-result primitive return kind，使 `!zero()` 和 `!one()` 这类 i64
  call result 走 `zr_aot_s*` typed direct-call local，再以
  `zr_aot_bD = (TZrBool)(zr_aot_sS == (TZrInt64)0);` 写 bool destination local，不再调用
  `GenericPrimitiveLogicalNot` / `SyncBoolLocal`。相邻 call shared smoke 更新为已接受的 numeric
  scalar stack-copy local 形态 · RED/GREEN：RED 由扩展后的 logical shared truthiness smoke 捕获旧
  runtime helper path；GREEN 后 WSL GCC logical contracts 4/0、logical shared smoke 6/0、
  generic LOGICAL_NOT numeric local smoke 1/0、generic JUMP_IF smoke 3/0、generic bool equality smoke 1/0、
  control contracts 2/0、frame setup contracts 1/0、call shared smoke 5/0、call contracts 8/0；
  WSL Clang 同组 4/0、6/0、1/0、3/0、1/0、2/0、1/0、5/0、8/0；Windows MSVC Debug
  logical/control/frame/call contracts 4/0、2/0、1/0、8/0，Unix-only shared smokes 全部 0 failures
  且按平台 ignored。产出：
  `tests/acceptance/2026-06-29-aot-07-s2-s4-call-result-truthiness-local-propagation.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string/object truthiness、value-copy 迁移、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-29 10:25:39 +08:00 · 07-S2/S4 generic LOGICAL_NOT numeric-source local branch ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `LOGICAL_NOT` 现在对已证明的
  i64/u64/f64 source local 直接写 bool destination local，生成 zero-predicate logical-not 表达式，
  不再为这些 proven primitive source 调 `GenericPrimitiveLogicalNot` 与 `SyncBoolLocal`。scalar-local
  声明/写入追踪只把 bool/i64/u64/f64 primitive source 纳入该路径，不扩大到 string/object/dynamic。
  RED/GREEN：新增独立 `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`，RED 捕获旧 runtime
  helper path；GREEN 后 WSL GCC logical contracts 4/0、新 smoke 1/0、logical shared smoke 6/0、
  generic JUMP_IF smoke 3/0、generic bool equality smoke 1/0、control contracts 2/0、frame setup contracts 1/0；
  WSL Clang 同组 4/0、1/0、6/0、3/0、1/0、2/0、1/0；Windows MSVC Debug logical/control/frame contracts
  4/0、2/0、1/0，Unix-only新 smoke 为 0 failures / 1 ignored，logical shared smoke 为
  0 failures / 6 ignored，generic JUMP_IF smoke 为 0 failures / 3 ignored，generic bool equality smoke 为
  0 failures / 1 ignored。产出：
  `tests/acceptance/2026-06-29-aot-07-s2-s4-generic-logical-not-numeric-source-local-branch.md`。
  备注：不声明 07-S2/S4 全量完成；call-result truthiness 传播、generic/dynamic/string/object boundaries、
  value-copy 迁移、GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-29 10:00:09 +08:00 · 07-S2/S4 generic JUMP_IF numeric stack-copy local branch ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `JUMP_IF` 的 stack-copy
  condition consumer 现在沿用候选 primitive scalar kind，而不是一律窄成 bool；`JUMP_IF_BOOL_FALSE`
  仍保持 bool-only。`GET_CONSTANT i64 -> SET_STACK -> JUMP_IF` 现在生成 `zr_aot_s5 = zr_aot_s0;`
  和 `if (zr_aot_s5 == (TZrInt64)0) { goto ...; }`，不再走 truthiness helper。RED/GREEN：
  RED 由扩展后的 generic JUMP_IF smoke 捕获旧 frame/helper path；GREEN 后 WSL GCC logical contracts 4/0、
  generic JUMP_IF bool/numeric/stack-copy local smoke 3/0、control contracts 2/0、frame setup contracts 1/0、
  generic bool equality local smoke 1/0、logical shared smoke 6/0；WSL Clang 同组 4/0、3/0、2/0、1/0、1/0、6/0；
  Windows MSVC Debug logical/control/frame contracts 4/0、2/0、1/0，Unix-only generic JUMP_IF smoke 为
  0 failures / 3 ignored，generic bool equality smoke 为 0 failures / 1 ignored，logical shared smoke 为
  0 failures / 6 ignored。产出：
  `tests/acceptance/2026-06-29-aot-07-s2-s4-generic-jump-if-numeric-stack-copy-local-branch.md`。
  备注：不声明 07-S2/S4 全量完成；call-result truthiness 传播、generic/dynamic/string boundaries、
  value-copy 迁移、GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 17:12:40 +08:00 · 07-S2/S4 generic JUMP_IF numeric-source local branch ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `JUMP_IF` lowering 现在在
  bool-source local 分支后继续尝试已证明的 i64/u64/f64 scalar-local 条件；匹配时分别生成
  `if (zr_aot_sS == (TZrInt64)0) { goto ...; }`、`if (zr_aot_uS == (TZrUInt64)0u) { goto ...; }`、
  `if (zr_aot_fS == (TZrFloat64)0.0) { goto ...; }`，否则保留 `GenericPrimitiveIsTruthy`
  fallback。frame descriptor 用同一 bool/i64/u64/f64 condition proof 接受 local-only 分支。
  RED/GREEN：RED 由 logical contracts 和扩展后的手写 IR generic JUMP_IF smoke 捕获旧 runtime path；
  GREEN 后 WSL GCC logical contracts 4/0、generic JUMP_IF bool/numeric local smoke 2/0、
  control contracts 2/0、frame setup contracts 1/0、generic bool equality local smoke 1/0、logical shared smoke 6/0；
  WSL Clang 同组 4/0、2/0、2/0、1/0、1/0、6/0；Windows MSVC Debug logical contracts 4/0、
  control contracts 2/0、frame setup contracts 1/0，Unix-only generic JUMP_IF bool/numeric smoke 为
  0 failures / 2 ignored，generic bool equality smoke 为 0 failures / 1 ignored，logical shared smoke 为
  0 failures / 6 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-generic-jump-if-numeric-source-local-branch.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string truthiness boundaries、value-copy 迁移、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 16:49:34 +08:00 · 07-S2/S4 generic JUMP_IF bool-source local branch ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `JUMP_IF` lowering 现在先尝试
  bool-source scalar-local 分支，只有 condition slot 有严格 bool-value written-before 证明时才生成
  `if (!zr_aot_bS) { goto ...; }`；否则保留 `GenericPrimitiveIsTruthy` runtime fallback。
  frame descriptor、scalar-local 声明和 bool consumer/mention 扫描同步使用该严格 bool condition 证明，
  动态/int/object/string truthiness 仍走 runtime helper。RED/GREEN：RED 由 logical contracts 和新增手写 IR
  generic JUMP_IF bool local smoke 捕获旧 runtime path；GREEN 后 WSL GCC logical contracts 4/0、control contracts 2/0、
  frame setup contracts 1/0、generic JUMP_IF bool local smoke 1/0、generic bool equality local smoke 1/0、
  logical shared smoke 6/0；WSL Clang 同组 4/0、2/0、1/0、1/0、1/0、6/0；Windows MSVC Debug logical contracts 4/0、
  control contracts 2/0、frame setup contracts 1/0，Unix-only generic JUMP_IF bool local smoke 为 0 failures / 1 ignored，
  generic bool equality smoke 为 0 failures / 1 ignored，logical shared smoke 为 0 failures / 6 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-generic-jump-if-bool-source-local-branch.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string truthiness boundaries、value-copy 迁移、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 16:07:34 +08:00 · 07-S2/S4 generic LOGICAL_EQUAL/LOGICAL_NOT_EQUAL bool-source local branch ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL`
  lowering 现在先尝试 bool-source scalar-local 比较，只有 destination bool result 可跳过 value slot、
  两个 source slot 都有严格 bool-value written-before 证明且结果可由 bool branch 本地消费时才生成
  `zr_aot_bD = (TZrBool)((zr_aot_bL ==|!= zr_aot_bR) != 0u);` 与 `if (!zr_aot_bD)`；否则保留
  runtime fallback。frame descriptor、value-slot 强制读取判断、scalar-local consumer 扫描和跨基本块
  bool 写入证明同步使用该严格 bool-source 证明，source-level mixed/dynamic equality 仍走 runtime helper。
  RED/GREEN：RED 由 logical contracts 和新增手写 IR generic bool equality local smoke 捕获旧 runtime path；
  GREEN 后 WSL GCC logical contracts 4/0、frame setup contracts 1/0、generic bool equality local smoke 1/0、
  logical shared smoke 6/0；WSL Clang 同组 4/0、1/0、1/0、6/0；Windows MSVC Debug logical contracts 4/0、
  frame setup contracts 1/0，Unix-only generic bool equality smoke 为 0 failures / 1 ignored，
  logical shared smoke 为 0 failures / 6 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-generic-bool-equality-local-branch.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string boundaries、value-copy 迁移、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 15:17:03 +08:00 · 07-S2/S4 generic LOGICAL_NOT bool-source local branch ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `LOGICAL_NOT` lowering 现在先尝试
  bool-source scalar-local 分支，只有下一条 `JUMP_IF_BOOL_FALSE` 读取同一 destination、bool result 可跳过
  value slot、且 source slot 有严格 bool-value written-before 证明时才生成
  `zr_aot_bD = (TZrBool)(!zr_aot_bS);` 与 `if (!zr_aot_bD)`；否则保留 runtime fallback。
  frame descriptor、value-slot 强制读取判断和 scalar-local consumer 扫描同步使用该严格 bool source 证明，
  int truthiness 的 `!zero()` / `!one()` 仍走 `GenericPrimitiveLogicalNot` / `GenericPrimitiveIsTruthy`。
  RED/GREEN：RED 由新增手写 IR bool-source smoke 捕获旧 runtime path，并由 truthiness negative 断言防止
  int source 被误优化；GREEN 后 WSL GCC logical contracts 4/0、frame setup contracts 1/0、logical shared smoke 6/0；
  WSL Clang 同组 4/0、1/0、6/0；Windows MSVC Debug logical contracts 4/0、frame setup contracts 1/0，
  Unix-only logical shared smoke 为 0 failures / 6 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-generic-logical-not-bool-source-local-branch.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string boundaries、value-copy 迁移、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 13:44:34 +08:00 · 07-S2/S4 generic TO_BOOL scalar-source local-only ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic `TO_BOOL` lowering 现在先尝试
  scalar source local-only 路径，只有 destination bool result 可跳过 value slot 且 source slot 已写入
  bool/i64/u64/f64 local 时才生成 `zr_aot_scalar_exec_to_bool`；否则保留 runtime fallback。
  frame descriptor 将 `TO_BOOL` 纳入本地化证明，scalar-local consumer 扫描也把 `TO_BOOL` 作为四类
  scalar source consumer，使 `<bool> truthSource` 直接从 `zr_aot_s1` 生成 `zr_aot_b4`，不再写
  `frame.slotBase[1].value` 或 `frame.slotBase[4].value`。RED/GREEN：RED 由 generic primitive conversion
  smoke 新增 generated-C 断言捕获旧 `ConvertGenericToBool(state, &frame, 4, 1)` 和 frame slot 写；
  GREEN 后 WSL GCC source contracts 22/0、frame setup contracts 1/0、shared-library smoke 13/0；
  WSL Clang 同组 22/0、1/0、13/0；Windows MSVC Debug source contracts 22/0、frame setup contracts 1/0，
  Unix-only shared smoke 为 0 failures / 13 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-generic-to-bool-scalar-source-local.md`。
  备注：不声明 07-S2/S4 全量完成；value-level stack-copy 全面迁移、generic/dynamic/string boundaries、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 13:14:53 +08:00 · 07-S2/S4 stack-copy scalar declaration consumer narrowing ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：stack-copy scalar local 声明传播先采用
  SemIR destination kind，再按后续实际消费者收窄，不再把 source slot 的全局 bitset 原样传播到 destination。
  bool logical fixture 中 slot 3/5/15 只声明 bool local，不再被 slot 6/16 的整数返回路径污染为 i64；
  真实返回槽 `s6/s16` 仍声明并用于 direct i64 return。新增 operand consumer kind helper 使 generic
  `TO_INT`/`TO_UINT`/`TO_FLOAT` stack-copy source 保留 candidate source kind，避免 conversion result kind
  误收窄破坏 unsigned mixed-literal frame-free 路径。RED/GREEN：RED 由新增 generated-C anti-needle 捕获
  `TZrInt64 zr_aot_s3` 触发；GREEN 后 WSL GCC logical smoke 5/0、source contracts 22/0、shared-library
  smoke 13/0、frame setup contracts 1/0；WSL Clang 同组 5/0、22/0、13/0、1/0；Windows MSVC Debug
  source contracts 22/0、frame setup contracts 1/0，Unix-only logical/shared smoke 为 0 failures / 5 ignored
  与 0 failures / 13 ignored。
  产出：`tests/acceptance/2026-06-28-aot-07-s2-s4-stack-copy-declaration-consumer-narrowing.md`。
  备注：不声明 07-S2/S4 全量完成；value-level stack-copy 全面迁移、generic/dynamic/string boundaries、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 12:33:55 +08:00 · 07-S2/S4 callable stack-copy scalar-sync elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：direct stack-copy 发射器新增
  `skipScalarLocalSync`，当 destination 是下一条调用的 callable slot 时保留 `CopyStack`，
  但跳过 bool/i64/u64/f64 local sync，避免把 closure/callable frame 槽误同步为 scalar local。
  `yes() && no()` 中 slot 7/16 的 callable copy 不再在 direct call 前生成
  `zr_aot_direct_stack_copy_sync_bool_local_boundary`；direct-call fallback 的 call-result sync 保留。
  RED/GREEN：RED 由新增 generated-C 断言捕获 `CopyStack(..., 7/16, 1)` 后紧跟 `SyncBoolLocal`
  触发；GREEN 后 WSL GCC logical smoke 5/0、source contracts 22/0；WSL Clang 同组 5/0、22/0；
  Windows MSVC Debug source contracts 22/0，Unix-only logical smoke 为 0 failures / 5 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-callable-stack-copy-sync-elision.md`。
  备注：不声明 07-S2/S4 全量完成；callable/closure value materialization、
  value-level stack-copy 全面迁移、generic/dynamic/string boundaries、GC roots/exports/frame cleanup、
  byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 12:16:07 +08:00 · 07-S2/S4 bool condition-copy local-only value-slot elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：stack-copy destination 紧邻
  `JUMP_IF_BOOL_FALSE` 时不再强制写 value slot；value-slot 强制边界保留为下一条调用参数和后续真实
  bool value operand。`yes() && no()` 的 `dstSlot=15 srcSlot=16` 现在生成
  `zr_aot_b15 = (TZrBool)(zr_aot_b16 != 0u);`，下一条分支生成 `if (!zr_aot_b15)`，
  复制处不再写 `frame.slotBase[15].value`。RED/GREEN：RED 由新增 generated-C 断言缺少
  bool local-only assignment 触发；GREEN 后 WSL GCC logical smoke 5/0、source contracts 22/0；
  WSL Clang 同组 5/0、22/0；Windows MSVC Debug source contracts 22/0，Unix-only logical smoke 为
  0 failures / 5 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-bool-condition-copy-local-only.md`。
  备注：不声明 07-S2/S4 全量完成；value-level stack-copy 全面迁移、
  generic/dynamic/string boundaries、GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 12:01:38 +08:00 · 07-S2/S4 bool call-result branch local ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：call-result destination 纳入
  scalar-local declaration collection，no-arg typed bool direct call 的 slot 14 可声明并写入
  `zr_aot_b14`，后续短路分支生成 `if (!zr_aot_b14)`，不再读取 `frame.slotBase[14].value`。
  value-level stack-copy fallback 的 `Sync*Local` 现在要求 source/destination 都有相同 scalar kind，
  避免 callable 临时 slot 把 bool/i64 sync 污染到 call-result slot。RED/GREEN：RED 由新增
  generated-C 断言缺少 `TZrBool zr_aot_b14 = ZR_FALSE;` 触发；GREEN 后 WSL GCC logical smoke
  5/0、source contracts 22/0；WSL Clang 同组 5/0、22/0；Windows MSVC Debug source contracts 22/0，
  Unix-only logical smoke 为 0 failures / 5 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-bool-call-result-branch-local.md`。
  备注：不声明 07-S2/S4 全量完成；value-level stack-copy 全面迁移、generic/dynamic/string boundaries、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 11:25:49 +08:00 · 07-S2/S4 bool condition-copy branch local ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：stack-copy write tracking 在 source kind
  无法证明时回退 declared destination kind，匹配 value-level `CopyStack` 后的 `Sync*Local` 边界。
  `yes() && no()` 中 `dstSlot=15 srcSlot=16` 后续 `JUMP_IF_BOOL_FALSE` 现在生成
  `if (!zr_aot_b15)`，不再读取 `frame.slotBase[15].value`；logical smoke 和 source contracts 锁定该路径。
  RED/GREEN：RED 由新增 generated-C 断言缺少 `if (!zr_aot_b15)` 触发；GREEN 后 WSL GCC logical smoke
  5/0、source contracts 22/0；WSL Clang 同组 5/0、22/0；Windows MSVC Debug source contracts 22/0，
  Unix-only logical smoke 为 0 failures / 5 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-bool-condition-copy-branch-local.md`。
  备注：不声明 07-S2/S4 全量完成；value-level stack-copy 全面迁移、generic/dynamic/string boundaries、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 11:05:55 +08:00 · 07-S2/S4 string logical bool sync parentheses ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：generic/string logical bool sync 模板现在发
  `(TZrBool)((%s) != 0u)`，修正 string `!=` 生成 `!zr_aot_equal != 0u` 触发的 Clang
  `logical-not-parentheses` 警告。string equality smoke 要求 `(TZrBool)((!zr_aot_equal) != 0u)` 并禁止旧形态；
  source contracts 锁定新模板。RED/GREEN：RED 由新 generated-C 断言失败触发；GREEN 后 WSL GCC logical smoke
  5/0、source contracts 22/0；WSL Clang 同组 5/0、22/0 且旧 warning 消失；Windows MSVC Debug
  source contracts 22/0，Unix-only logical smoke 为 0 failures / 5 ignored。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-string-logical-bool-sync-parentheses.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string boundaries、GC roots/exports/frame cleanup、
  byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 10:55:19 +08:00 · 07-S2/S4 call-boundary bool frame semantics ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：stack-call argument slot 现在被识别为
  value-frame 读取，bool 常量实参不会被错误 local-only elide；scalar-local write tracking 改为 exact
  current kind 并在 reset/call-result/stack-copy 上更新真实 kind；forced condition-copy 可从上一条 typed
  bool call result 选择 bool scalar source，`dstSlot=15 srcSlot=16` 生成 bool frame write 而不是 stale i64
  copy。RED/GREEN：RED 由 logical shared-library smoke 的 generic equality 错值、bool short-circuit 失败和新增
  generated-C 断言触发；GREEN 后 WSL GCC logical smoke 5/0、shared-library smoke 13/0、source contracts
  21/0、frame setup contracts 1/0；WSL Clang 同组 5/0、13/0、21/0、1/0；Windows MSVC Debug source/frame
  contracts 21/0、1/0，Unix-only logical/shared smoke 为 0 failures / 5 ignored 与 0 failures / 13 ignored。
  产出：`tests/acceptance/2026-06-28-aot-07-s2-s4-call-boundary-bool-frame-semantics.md`。
  备注：不声明 07-S2/S4 全量完成；string equality Clang warning、generic/dynamic/string boundaries、
  GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数仍待后续。

- 2026-06-28 10:06:29 +08:00 · 07-S2/S4 signed branch constant frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 尚未达成；08-12 继续按各自计划推进 · 完成项目：typed signed branch loop 中只作为
  比较 operand 的 `CONST i64=0` 现在通过 scalar-local proof 生成 `zr_aot_s3 = (TZrInt64)0;`
  和 `if (zr_aot_s1 <= zr_aot_s3)`，不再写/读 `frame.slotBase[3].value`。
  `backend_aot_c_scalar_locals` 将 signed branch opcodes 纳入后续标量消费者、i64 read
  和 operand-local proof；shared-library branch smoke 禁止 `zr_aot_value_exec_primitive_constant`
  与 slot 3 value materialization，source contracts 锁定 signed branch scalar consumer 路径。
  RED/GREEN：RED 由新增 generated-C branch 断言失败触发；GREEN 后 WSL GCC shared-library smoke
  13/0、source contracts 21/0、frame setup contracts 1/0；WSL Clang 同组 13/0、21/0、1/0；
  Windows MSVC Debug 编译三组目标并通过 source contracts 21/0、frame setup contracts 1/0，
  shared-library smoke 为 0 failures / 13 ignored Unix-only。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-signed-branch-constant-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；generic/dynamic/string branch、runtime boundary、GC roots/
  exports/frame cleanup、byte-frame 更广收窄、性能计数和完整 typed 函数体零 frame 仍待后续。

- 2026-06-28 09:53:59 +08:00 · 07-S2/S4 conversion temporary result frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：bool->u64/f64 generic conversion 的临时 result slot 被后续
  scalar stack-copy 消费后可保持 local-only，不再写 `frame.slotBase[6].value` /
  `frame.slotBase[7].value`。`result_scan_live_value_block()` 复用任意 scalar overwrite
  判断，在旧值未被读取且同一 slot 被后续 scalar kind 重写时结束旧 result 生命周期。RED/GREEN：
  RED 由新增 slot 6/7 value-slot 禁止断言触发；GREEN 后 WSL gcc shared-library smoke 13/0、
  source contracts 21/0、frame setup contracts 1/0；WSL clang 同组 13/0、21/0、1/0；
  MSVC Debug source contracts 21/0、frame setup contracts 1/0、shared-library smoke
  0 failures / 13 ignored Unix-only。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-conversion-temp-result-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；真实变量 slot、generic boundary、GC roots/exports/
  frame cleanup、byte-frame 更广收窄和性能计数仍待后续切片。

- 2026-06-28 09:38:30 +08:00 · 07-S2/S4 bool generic conversion source frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：typed generic conversion 现在可从 bool scalar local 直接生成
  `<int>`、`<uint>`、`<float>` 转换，不再要求 source bool slot 物化到 `frame.slotBase`。
  `TO_UINT`/`TO_FLOAT` frame descriptor local-only 证明接受 bool-written source，scalar locals
  将 `TO_INT`/`TO_UINT`/`TO_FLOAT` 识别为 bool consumers，并修正 conversion/unary 的
  slot-mention 判断，避免默认未用 operand 把 slot 0 误判为 frame 读。shared-library generic
  conversion smoke 现在要求 bool constant `flag` 只保留 `zr_aot_b0 = ZR_TRUE;`，并验证
  bool->u64/f64 生成 `zr_aot_b0 ? ...` 表达式且不含 `frame.slotBase[0].value`。RED/GREEN：
  RED 先命中 bool->u64/f64 frame-source fallback，随后命中 bool constant slot 0 仍因 operand
  mention 误判而物化；GREEN 后 WSL gcc shared-library smoke 13/0、source contracts 21/0、
  frame setup contracts 1/0；WSL clang 同组 13/0、21/0、1/0；MSVC Debug source contracts
  21/0、frame setup contracts 1/0、shared-library smoke 0 failures / 13 ignored Unix-only。
  产出：`tests/acceptance/2026-06-28-aot-07-s2-s4-bool-generic-conversion-source-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；destination value slot、generic boundary、GC roots/exports/
  frame cleanup、byte-frame 更广收窄和性能计数仍待后续切片。

- 2026-06-28 08:54:52 +08:00 · 07-S2/S4 bool constant frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：bool `GET_CONSTANT` immediate 现在复用 scalar-local
  reachable-consumer proof，可在后续 consumer/return 都本地化时只生成
  `zr_aot_scalar_constant_bool_local` 和 `zr_aot_bN = ZR_TRUE|ZR_FALSE`，并让 frame
  descriptor 判定省略 generated frame。新增 bool constant return shared-library smoke
  覆盖 `var flag: bool = true; return flag;`，要求 generated C 直接 bool return 且不含
  frame setup、`ZrAotGeneratedFrame frame`、`frame.slotBase`、`ZrCore_Stack_GetValue`、
  `ZR_VALUE_FAST_SET` 或 `zr_aot_value_exec_primitive_constant`。另新增 f64 compare
  frame-free 执行护栏，锁定 `zr_aot_scalar_exec_f64_compare`、bool local compare 和
  direct bool return；该用例确认既有实现已满足目标，不包含新的生产修复。同时把 generated typed
  bool NOT 表达式改成显式括号，消除 Clang `!x != 0u` 警告。RED/GREEN：RED 由新增
  bool constant smoke 缺少 local-only bool constant 并仍写 value slot 触发；GREEN 后 WSL
  gcc shared-library smoke 13/0、source contracts 21/0、frame setup contracts 1/0、float
  shared-library smoke 2/0；WSL clang 同组 13/0、21/0、1/0、float smoke 2/0；MSVC Debug
  source contracts 21/0、frame setup contracts 1/0、shared-library smoke 0 failures /
  13 ignored Unix-only、float smoke 0 failures / 2 ignored Unix-only。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-bool-constant-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；dynamic/generic/string 边界、GC roots/exports/
  frame cleanup、byte-frame 更广收窄和性能计数仍待后续切片。

- 2026-06-28 08:12:21 +08:00 · 07-S2/S4 signed load-const/load-stack-const scalar frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：AOT scalar-local 证明补齐 signed fused
  load-const/load-stack-const/load-stack-load-const 算术族的 i64 result/consumer 识别；
  fused consumer 只读取真实 source slot，`GET_CONSTANT` 目的槽在后续 scalar consumer 或
  fused constant materialization slot 场景下声明为 scalar local。新增 load-const scalar
  smoke 构造 `GET_CONSTANT`、`ADD_SIGNED_LOAD_CONST`、`GET_STACK`、
  `SUB_SIGNED_LOAD_STACK_CONST`、`ADD_SIGNED_LOAD_STACK_LOAD_CONST` 与 direct return 链路，
  要求 generated C 只含 scalar locals、literal assignment、scalar stack-copy 和
  `zr_aot_direct_return_i64_local`，不含 frame setup、`ZrAotGeneratedFrame frame`、
  `frame.slotBase`、`ZrCore_Stack_GetValue` 或 `ZR_VALUE_FAST_SET`。RED/GREEN：旧证明缺少
  fused load-const 常量物化槽覆盖；GREEN 后 WSL gcc/clang load-const scalar smoke 1/0、
  source contracts 21/0、frame setup contracts 1/0；MSVC Debug source contracts 21/0、
  frame setup contracts 1/0、load-const scalar smoke 0 failures / 1 ignored Unix-only。
  `git diff --check` 通过，仅 LF/CRLF 提示。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-load-const-stack-const-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；dynamic/generic/string 边界、GC roots/exports/
  frame cleanup、byte-frame 更广收窄和性能计数仍待后续切片。

- 2026-06-28 07:09:58 +08:00 · 07-S2/S4 f64 mod frame-elision execution guardrail ·
  状态：07-S2/S4 验收护栏子切片完成；07/M1.5 继续进行中，完整 typed 函数体零
  `SZrValue`/frame write 仍未关闭 · 完成项目：float modulo shared-library smoke 升级为
  项目级 AOT runtime 执行验收，写入 `.zrp/.zr/.zro`，嵌入 binary input，生成共享库并执行
  `return left % right`，要求返回 double 1.5。generated C 约束锁定
  `zr_aot_scalar_exec_f64_binary`、`fmod(`、`zr_aot_direct_return_f64_local`，并禁止 frame setup、
  `ZrAotGeneratedFrame frame`、`ZrCore_Stack_GetValue`、`ZR_VALUE_FAST_SET` 和
  `ZrLibrary_AotRuntime_ModFloat(state, &frame)`。RED/GREEN：旧生成物曾显示 frame/result
  materialization；当前代码重新生成后直接 GREEN，说明前序 scalar/result/frame-descriptor 工作
  已覆盖 f64 modulo 纯标量 frame-elision，本切片只补产品级执行验收。测试：WSL gcc/clang
  float shared-library smoke 1/0；MSVC Debug 0 failures / 1 ignored Unix-only；`git diff --check`
  通过，仅 LF/CRLF 提示。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-f64-mod-frame-elision-execution.md`。
  备注：不声明 07-S2/S4 全量完成；f64 更广算术/比较、load-const/load-stack const、
  dynamic/generic/string 边界、GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数
  仍待后续切片。

- 2026-06-28 07:01:28 +08:00 · 07-S2/S4 unsigned mixed-literal conversion frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：新增 unsigned const、unsigned-to-signed conversion、unsigned
  mixed-literal shared-library smoke，要求 `uint` 常量流水线和 `uint seed + int literal`
  生成 C 只走 scalar u64/i64 locals、scalar conversion 与 direct return，不生成 frame setup、
  `ZrAotGeneratedFrame frame`、`ZrCore_Stack_GetValue` 或 `ZR_VALUE_FAST_SET`。`TO_INT`
  local-only 转换现在可消费已写入的 u64/i64/f64/bool scalar source，frame descriptor 将
  转换族纳入 local-only 证明；stack-copy descriptor 判定与正文发射器对齐 source static/local
  类型回退，修复 mixed-literal `GET_STACK dst=2 src=0` 因目的类型误判而保守要求 frame。
  RED/GREEN：RED 显示 mixed-literal generated C 仍有 frame setup；诊断定位 descriptor
  reject 于 early stack-copy。GREEN 后 WSL gcc/clang frame setup contracts 1/0、shared-library
  smoke 12/0；MSVC Debug frame setup contracts 1/0、shared-library smoke 0 failures /
  12 ignored Unix-only。`git diff --check` 通过，仅 LF/CRLF 提示。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-unsigned-mixed-literal-conversion-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；f64 const/reset、load-const/load-stack const、
  dynamic/generic/string 边界、GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数
  仍待后续切片。

- 2026-06-28 06:07:24 +08:00 · 07-S2/S4 signed-const scalar reset frame elision ·
  状态：07-S2/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：新增 signed-const scalar shared-library smoke，证明纯 `int`
  右常量算术流水线生成并执行结果 42，同时 generated C 只有 `zr_aot_scalar_exec_i64_binary`、
  `zr_aot_direct_return_i64_local` 与 scalar reset skip 标记，不再生成 frame setup、
  `ZrAotGeneratedFrame frame`、`ZrCore_Stack_GetValue` 或 `ZR_VALUE_FAST_SET`。
  reset liveness 现在按 opcode 家族选择 i64/u64/f64/bool reader，signed-const 通用
  operand/source mention 也统一只看左操作数槽，避免 `operand1[1]` 常量池索引被误判为
  栈槽。RED/GREEN：RED 显示 `ResetStackNull2(state, &frame, 2, 3)` 与
  `ResetStackNull(state, &frame, 4/5)` 仍强制 frame；GREEN 后 WSL gcc/clang frame setup
  contracts 1/0、shared-library smoke 9/0，MSVC Debug frame setup contracts 1/0、
  shared-library smoke 9/0/9 ignored Unix-only。`ctest -R 'aot_c_(frame_setup|shared_library)'`
  未发现注册测试，直接二进制为权威验证；`git diff --check` 通过，仅 LF/CRLF 提示。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-s4-signed-const-scalar-reset-frame-elision.md`。
  备注：不声明 07-S2/S4 全量完成；unsigned/f64 const/reset、load-const/load-stack const、
  dynamic/generic/string 边界、GC roots/exports/frame cleanup、byte-frame 更广收窄和性能计数
  仍待后续切片。

- 2026-06-28 05:40:17 +08:00 · 07-S3/S4 bool logical frame-descriptor elision ·
  状态：07-S3/S4 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：frame descriptor 现在按 bool local proof 识别
  `LOGICAL_AND`/`LOGICAL_OR`/`LOGICAL_EQUAL_BOOL`/`LOGICAL_NOT_EQUAL_BOOL`/
  `LOGICAL_NOT_BOOL`，typed return descriptor-free 判定与 bool/u64/f64 inferred return
  生成器对齐，并取消 typed `LOGICAL_NOT_BOOL` 已可本地消费时对前置 bool stack copy 的
  无条件 value-slot 强制写入。新增 shared-library smoke 证明 bool logical pipeline 生成
  `zr_aot_bool_not_scalar_local`、`zr_aot_bool_compare_scalar_local`、`zr_aot_direct_return_bool_local`，
  且不生成 `ZrAotGeneratedFrame frame`、frame setup、`ZrCore_Stack_GetValue` 或 `ZR_VALUE_FAST_SET`。
  RED/GREEN：RED 覆盖 source contract 缺少 bool logical proof、产品 smoke 仍含 frame setup、
  以及 descriptor-free 后残留 `frame.slotBase` 导致 gcc `frame undeclared`；GREEN 后 WSL
  gcc/clang frame setup contracts 1/0、logical shared-library smoke 5/0，MSVC Debug frame
  setup contracts 1/0、logical smoke 0 failures / 5 ignored Unix-only。`ctest -R
  'aot_c_(frame_setup|logical)'` 未发现注册测试，直接二进制为权威验证；`git diff --check`
  touched files 通过。产出：
  `tests/acceptance/2026-06-28-aot-07-s3-s4-bool-logical-frame-descriptor-elision.md`。
  备注：不声明 07-S3/S4 全量完成；dynamic/generic/string logical、GC roots、exports、
  frame cleanup、inline value layouts、byte-frame 更广收窄和性能计数仍待后续切片。

- 2026-06-28 05:04:55 +08:00 · 07-S2 signed binary scalar-operands consumer alignment ·
  状态：07-S2 子切片完成；07/M1.5 继续进行中，完整 typed 函数体零 `SZrValue`/frame
  write 仍未关闭 · 完成项目：signed i64 binary emitter 对已写入 scalar locals 的左右源槽
  发射 `zr_aot_arith_exec_signed_scalar_operands`，static numeric no-arg typed direct-call
  smoke 中 `ADD_SIGNED` 不再回读未 materialize 的 `frame.slotBase[7/9]`，而是直接消费
  `zr_aot_s7/zr_aot_s9`。RED/GREEN：RED 为 WSL gcc shared-library smoke 5 tests /
  1 failure；GREEN 后 source contracts 21/0、shared-library smoke 5/0、typed scalar 1/0。
  测试结果：WSL gcc/clang direct 同组 21/0、5/0、1/0 + focused CTest 3/3；Windows
  MSVC Debug direct source contracts 21/0、Unix-only smoke/typed scalar ignored + focused
  CTest 3/3。产出：
  `tests/acceptance/2026-06-28-aot-07-s2-signed-binary-scalar-operands.md`。
  备注：本条同时关闭 11-S6H 验证期间发现的既有 scalar no-arg typed direct-call smoke
  runtime blocker，但不声明 11-S6 cross-module token resolve 或完整 ABI drift injection 完成。

- 2026-06-28 04:22:47 +08:00 · 11-S6H inline-struct CALL_TYPED metadata guard/deopt ·
  状态：11-S6 inline-struct typed-call deopt 子切片完成；11 阶段继续进行中，cross-module
  token resolve 与完整 ABI drift injection 仍未关闭 · 完成项目：value SemIR inline-struct
  `CALL_TYPED` ordinary direct call、shared generic METHOD-slot call 和 full-AOT collected shared
  callsite 现在生成 metadata guard；兼容时保留 `CallInlineStruct()`，不兼容时经
  `CallInlineStructDynamicDeoptBridge()` 降级并复制 inline return bytes。RED/GREEN：RED 由
  value SemIR source contract 要求 guard marker 后，WSL gcc 失败 4 tests / 1 failure；GREEN 后
  runtime guard 3/0、value SemIR contracts 4/0、generic CALL_TYPED 7/0。测试结果：WSL gcc direct
  3/0、4/0、7/0 + CTest 2/2；WSL clang direct 3/0、4/0、7/0 + CTest 2/2；Windows
  MSVC Debug direct 3/0、4/0、7/0（3 ignored）+ CTest 2/2。产出：
  `tests/acceptance/2026-06-28-aot-11-s6h-inline-struct-typed-call-deopt.md`。
  备注：本切片不声明 cross-module token resolve 或完整 ABI drift injection 完成；验证期间暴露的
  scalar no-arg typed direct-call runtime failure 已由后续 07-S2 signed binary scalar-operands
  consumer alignment 修复，不计入 11-S6H 验收。

- 2026-06-28 03:43:43 +08:00 · 11-S6G bool typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；11 阶段继续进行中，inline-struct
  typed boundary、跨模块 token resolve 仍未关闭 · 完成项目：bool no/one/two/three-arg typed
  direct-call 与 i64/u64/f64 comparison -> bool direct-call 生成 metadata guard，兼容时保持
  bool direct thunk，不兼容时经 `CallStackValue()` 降级并同步 bool scalar local。RED/GREEN：
  RED 由新 source contract 要求 bool guard marker 后，WSL gcc call contracts 失败
  8 tests / 1 failure；GREEN 后 runtime guard 3/0、call contracts 8/0、typed call contracts 4/0、
  bool typed direct-call smoke 28/0。测试结果：WSL gcc direct 3/0、8/0、4/0、28/0 + CTest
  1/1；WSL clang direct 3/0、8/0、4/0、28/0 + CTest 1/1；Windows MSVC Debug direct 3/0、8/0、
  4/0、0 failures / 28 ignored + CTest 1/1。产出：
  `tests/acceptance/2026-06-28-aot-11-s6g-bool-typed-direct-call-deopt.md`。
  备注：本切片不声明 inline struct writeback、跨模块 token resolve 或完整 ABI drift injection 完成。

- 2026-06-28 03:14:20 +08:00 · 11-S6F f64 typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；11 阶段继续进行中，bool/inline-struct
  typed boundary、跨模块 token resolve 仍未关闭 · 完成项目：f64 no/one/two/three-arg typed
  direct-call 生成 metadata guard，兼容时保持 float direct thunk，不兼容时经 `CallStackValue()`
  降级并同步 f64 scalar local。RED/GREEN：RED 由新 source contract 要求 f64 guard marker 后，
  WSL gcc call contracts 失败 7 tests / 1 failure；GREEN 后 runtime guard 3/0、call contracts
  7/0、f64 typed direct-call smoke 19/0。测试结果：WSL gcc direct 3/0、7/0、19/0 + CTest
  1/1；WSL clang direct 3/0、7/0、19/0 + CTest 1/1；Windows MSVC Debug direct 3/0、7/0、
  0 failures / 19 ignored + CTest 1/1。产出：
  `tests/acceptance/2026-06-28-aot-11-s6f-f64-typed-direct-call-deopt.md`。
  备注：本切片不声明 bool typed boundary、inline struct writeback、跨模块 token resolve
  或完整 ABI drift injection 完成。

- 2026-06-28 02:54:58 +08:00 · 11-S6E u64 typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；11 阶段继续进行中，bool/f64/inline-struct
  typed boundary、跨模块 token resolve 仍未关闭 · 完成项目：u64 no/one/two/three-arg typed
  direct-call 生成 metadata guard，兼容时保持 unsigned direct thunk，不兼容时经
  `CallStackValue()` 降级并同步 u64 scalar local。RED/GREEN：RED 由新 source contract 要求
  u64 guard marker 后，WSL gcc call contracts 失败 6 tests / 1 failure；GREEN 后 runtime guard
  3/0、call contracts 6/0、u64 typed direct-call smoke 25/0。测试结果：WSL gcc direct
  3/0、6/0、25/0 + CTest 1/1；WSL clang direct 3/0、6/0、25/0 + CTest 1/1；Windows MSVC
  Debug direct 3/0、6/0、0 failures / 25 ignored + CTest 1/1。产出：
  `tests/acceptance/2026-06-28-aot-11-s6e-u64-typed-direct-call-deopt.md`。
  备注：本切片不声明 bool/f64 typed boundary、inline struct writeback、跨模块 token resolve
  或完整 ABI drift injection 完成。

- 2026-06-28 02:16:03 +08:00 · 11-S6D i64 typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；11 阶段继续进行中，bool/u64/f64/inline-struct
  typed boundary、跨模块 token resolve 仍未关闭 · 完成项目：runtime 新增 typed direct-call
  caller/callee binding guard 与 deopt helper；i64 no/one/two/three-arg typed direct-call 生成
  metadata guard，兼容时保持 direct thunk，不兼容时经 `CallStackValue()` 降级并同步 i64 scalar local。
  RED/GREEN：RED 由新 runtime guard test 引用缺失 API 触发 WSL gcc 隐式声明/链接失败；GREEN 后
  runtime guard 3/0、call contracts 5/0、i64 typed direct-call smoke 继续 5/0。测试结果：WSL gcc
  direct 3/0、5/0、5/0 + CTest 1/1；WSL clang direct 3/0、5/0、5/0 + CTest 1/1；Windows MSVC
  Debug direct 3/0、5/0、0 failures / 5 ignored + CTest 1/1。产出：
  `tests/acceptance/2026-06-28-aot-11-s6d-i64-typed-direct-call-deopt.md`。
  备注：本切片不声明 bool/u64/f64 typed boundary、inline struct writeback、跨模块 token resolve
  或完整 ABI drift injection 完成。

- 2026-06-28 01:18:38 +08:00 · 11-S6C dynamic AOT module-load binding reject ·
  状态：11-S6 dynamic loader reject 子切片完成；11 阶段继续进行中，typed boundary deopt 与跨模块
  token resolve 仍未关闭 · 完成项目：root AOT runtime loader 在加载 embedded/zro metadata function、
  构建 function table 并 attach metadata runtime 后扫描 `moduleMetadataBindings`，不兼容时拒绝加载并输出
  status/token/hash/layout 诊断。RED/GREEN：RED 由新 loader test 注入 signature hash drift 后旧 loader
  仍成功，失败为 `Expected FALSE Was TRUE`；GREEN 后错误包含 `SIGNATURE_HASH_MISMATCH`，且既有
  shared-library smoke 空 binding 正常路径仍 8/0。测试结果：WSL gcc direct 1/0 + CTest 1/1，
  WSL clang direct 1/0 + CTest 1/1，Windows MSVC Debug 构建通过、direct 1 ignored、CTest 1/1。
  产出：`tests/acceptance/2026-06-28-aot-11-s6c-dynamic-loader-binding-reject.md`。
  备注：本切片不声明 typed deopt、跨模块 token resolve 或更广 no-crash ABI drift injection 完成。

- 2026-06-28 00:56:57 +08:00 · 11-S6B function token binding compatibility scan ·
  状态：11-S6 支撑子切片完成；11 阶段继续进行中，完整 11-S6 dynamic 拒绝/typed deopt 仍未关闭 ·
  完成项目：新增 function-level runtime binding compatibility scan API，按 function 的
  `moduleMetadataBindings` 顺序复用单 binding predicate，返回首个不兼容 binding/ref record/report；
  支持先查 function-local record、再查 module metadata record。RED/GREEN：RED 先由新测试引用缺失
  function scan API 触发 WSL gcc 链接失败；GREEN 后 compatibility test 覆盖 15 个用例。测试结果：
  WSL gcc、WSL clang、Windows MSVC Debug 均通过直接测试 15/0 与 CTest
  `metadata_runtime_binding_compatibility` 1/1。产出：
  `tests/acceptance/2026-06-28-aot-11-s6b-function-binding-compatibility-scan.md`。
  备注：本切片只关闭 function scan/report 支撑层；runtime loader reject、typed boundary deopt 与跨模块
  token resolve 接线仍待后续。

- 2026-06-27 09:04:36 +08:00 · 11-S6A runtime metadata token binding compatibility ·
  状态：11-S6 支撑子切片完成；11 阶段继续进行中，完整 11-S6 dynamic 拒绝/typed deopt 仍未关闭 ·
  完成项目：新增 runtime token binding compatibility status/report/API，统一检查 version range、
  module signature hash、metadata/signature token、signature hash 与 layoutVersion/layoutHash 漂移；
  补充 focused metadata runtime test target/CTest。RED/GREEN：RED 先由新测试引用缺失 status/report/API
  触发 WSL gcc 编译失败；GREEN 后 compatibility test 覆盖 12 类状态分支。测试结果：WSL gcc、
  WSL clang、Windows MSVC Debug 均通过直接测试 12/0 与 CTest
  `metadata_runtime_binding_compatibility` 1/1。产出：
  `tests/acceptance/2026-06-27-aot-11-s6a-runtime-binding-compatibility.md`。
  备注：本切片只关闭可复用 predicate；runtime loader reject、typed boundary deopt 与跨模块 token
  resolve 接线仍待后续。

- 2026-06-27 08:35:30 +08:00 · 11-S7Y / 12-S7ZS zrp metadata version check ·
  状态：11-S7/12-S7 支撑子切片完成；11/12 阶段继续进行中 · 完成项目：
  CLI 新增 `--check-zrp-metadata-version <file>` 只读工具，解析层保存 version-check 路径并拒绝与
  run/compile/debug/output modifiers 混用；dump runner 读取 zrp metadata header 前缀，输出
  `zrp.metadata.versionCheck.status` 以及 actual/expected magic、version、headerBytes、sectionCount。
  当前 header shape 经完整 header 校验后输出 `ok`，版本或头形状不匹配输出 `unsupported` 并失败返回。
  RED/GREEN：RED 先由 CLI args 测试要求 version-check mode/path 后旧 command 结构缺 enum/字段而
  编译失败，随后 metadata dump 测试因缺 version-check API 链接失败；GREEN 后 `cli_args` 与
  `cli_zrp_metadata_dump` 通过。测试结果：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建
  `zr_vm_cli_executable`；同组可执行测试通过，focused CTest `cli_args|cli_zrp_metadata_dump`
  三套环境均为 2/2；WSL gcc 与 Windows MSVC help 输出覆盖新增帮助文本。产出：
  `tests/acceptance/2026-06-27-aot-11-s7y-zrp-metadata-version-check.md`。
  备注：本切片只关闭 standalone zrp metadata header version/shape check；11-S6 运行时 ABI 漂移
  deopt/拒绝、cross-module export-token rewrite、field/default-value backed constant-pool remap 和完整
  metadata sweep/pruning 仍待后续。

- 2026-06-27 08:14:35 +08:00 · 11-S7X / 12-S7ZR zrp metadata diff summary ·
  状态：11-S7/12-S7 支撑子切片完成；11/12 阶段继续进行中 · 完成项目：
  CLI 新增 `--diff-zrp-metadata <before> <after>` 只读工具，解析层保存 before/after 路径并拒绝与
  run/compile/debug/output modifiers 混用；dump runner 读取并校验两个 zrp metadata header，输出
  version/headerBytes/sectionCount before/after 与 12 个 section 的
  `bytes/count before/after/removed` diff summary。RED/GREEN：RED 先由 CLI args 测试要求 diff mode/path
  后旧 command 结构缺 enum/字段而编译失败，随后 diff summary 目标因缺 API 链接失败；GREEN 后
  `cli_args` 与 `cli_zrp_metadata_dump` 通过。测试结果：WSL gcc、WSL clang 与 Windows MSVC Debug
  均构建 `zr_vm_cli_executable`；同组可执行测试通过，focused CTest `cli_args|cli_zrp_metadata_dump`
  三套环境均为 2/2；WSL gcc `zr_vm_cli --help` 覆盖新增帮助文本。产出：
  `tests/acceptance/2026-06-27-aot-11-s7x-zrp-metadata-diff-summary.md`。
  备注：本切片只关闭 standalone section byte/count diff summary；版本检查、cross-module export-token
  rewrite、field/default-value backed constant-pool remap 和完整 metadata sweep/pruning 仍待后续。

- 2026-06-27 07:48:22 +08:00 · 11-S7W / 12-S7ZQ zrp metadata dump summary ·
  状态：11-S7/12-S7 支撑子切片完成；11/12 阶段继续进行中 · 完成项目：
  CLI 新增 `--dump-zrp-metadata <file>` 只读工具，解析层保存 `zrpMetadataPath` 并拒绝与
  run/compile/debug/output modifiers 混用；dump runner 校验 zrp metadata header，输出
  `zrp.metadata.version/headerBytes/sectionCount` 与 12 个 section 的
  `bytes/count/elementSize/offset` summary。RED/GREEN：RED 先由 CLI args 测试要求 dump mode/path
  后旧 command 结构缺 enum/字段而编译失败，随后 dump summary 目标因缺实现文件配置失败；
  GREEN 后 `cli_args` 与 `cli_zrp_metadata_dump` 通过。测试结果：WSL gcc、WSL clang 与 Windows
  MSVC Debug 均构建 `zr_vm_cli_executable`；同组可执行测试通过，focused CTest
  `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。产出：
  `tests/acceptance/2026-06-27-aot-11-s7w-zrp-metadata-dump-summary.md`。
  备注：本切片只关闭 standalone section summary dump；metadata diff、版本检查、cross-module
  export-token rewrite、field/default-value backed constant-pool remap 和完整 metadata sweep/pruning
  仍待后续。

- 2026-06-27 07:20:00 +08:00 · 12-S7ZP / 11-S7 zrp section count delta markers ·
  状态：12-S7/11-S7 支撑子切片完成；11/12 阶段继续进行中 · 完成项目：
  zrp metadata stats 现在从 header section directory 采样每个 section 的 count，并输出
  `aot_size.zrpMetadataSectionCounts.<section>` 与
  `code_stripping.zrpMetadataSectionCounts.<section>Before/After/Removed`，让 generated C
  同时具备 bytes 与 row/count 两个 diff 维度。RED/GREEN：RED 为新增 direct size-delta
  count marker 后，旧 stats 结构缺少 count 字段导致 WSL gcc 编译失败；GREEN 后 size-delta 2/0、
  source contracts 21/0、code stripping 5/0、direct zrp pruning 5/0、pool pruning 4/0、
  export-token remap 2/0。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 同组可执行测试通过，三套 focused CTest
  `aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  均为 5/5。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zp-zrp-section-count-delta-markers.md`。
  备注：完整 trim analyzer、attribute/annotation suppression/promotion、cross-module export-token
  publication/rewrite、field/default-value backed constant-pool remap 和独立 dump/diff 工具仍待后续。

- 2026-06-27 06:51:55 +08:00 · 12-S7ZO / 11-S7 zrp section-level trim delta markers ·
  状态：12-S7/11-S7 支撑子切片完成；11/12 阶段继续进行中 · 完成项目：
  zrp metadata size-delta writer 现在输出 12 个 section 的 before/after/removed marker，
  让 token records、8 类 definition tables、string/signature/constant pools 的裁剪变化可从 generated C
  直接 diff。RED/GREEN：RED 为新增 direct size-delta 测试缺 marker 失败 1/1；GREEN 后
  size-delta 1/0、source contracts 21/0、code stripping 5/0、direct zrp pruning 5/0、
  pool pruning 4/0、export-token remap 2/0。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 同组可执行测试通过，三套 focused CTest
  `aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  均为 5/5。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zo-zrp-section-delta-markers.md`。
  备注：完整 trim analyzer、attribute/annotation suppression/promotion、cross-module export-token
  publication/rewrite、field/default-value backed constant-pool remap 和独立 dump/diff 工具仍待后续。

- 2026-06-27 06:30:32 +08:00 · 12-S7ZN / 11-S7 export member-token remap surface ·
  状态：12-S7/11-S7 支撑子切片完成；12 阶段继续进行中 · 完成项目：
  zrp metadata remap 模块公开 `backend_aot_c_zrp_remap_export_member_token()`，可把
  pruning 前的 exported `MEMBER_DEF` token 映射到裁剪后的 MethodDef/FieldDef compacted RID；
  保留方法旧 token 得到新 RID，字段 token 排在 retained MethodDef 后，已删除方法 token
  返回 false 供后续跨模块 export manifest/table 写回时拒绝或诊断。
  RED/GREEN：RED 为 direct pruning 测试先要求导出方法旧 RID2 在 RID1/RID3 删除后映射到
  compacted RID1，旧实现缺 helper，WSL gcc 链接失败；GREEN 后新增独立
  export-token remap 测试 2/0，direct zrp pruning 5/0、pool pruning 4/0、code stripping 5/0、
  source contracts 21/0。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 同组通过，focused CTest
  `aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  4/4。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zn-export-member-token-remap.md`。
  备注：完整 trim analyzer、attribute/annotation suppression/promotion、cross-module export-token
  publication/rewrite、field/default-value backed constant-pool remap 和 dump/diff 仍待后续。

- 2026-06-27 05:57:45 +08:00 · 12-S7ZM / 11-S7 zrp pool compaction without MethodDef pruning ·
  状态：12-S7/11-S7 支撑子切片完成；12 阶段继续进行中 · 完成项目：
  emitted zrp metadata pruning 的 skip gate 从 MethodDef-count early return 改为 post-remap
  identity check；即使所有 MethodDef 都保留，只要 string/signature/constant pool 需要收缩，
  仍会生成 compacted owned blob。
  RED/GREEN：RED 为 no-MethodDef-prune duplicate string fixture 要求 retained TypeDef/FieldDef
  共享 `Shared` 字符串后，旧实现 `ownedBlob` 为空，pool pruning 失败 1/4；GREEN 后
  pool pruning 4/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0。
  测试结果：WSL gcc/clang 同组通过，focused CTest
  `aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping` 2/2；Windows MSVC Debug 同组通过，
  focused CTest 2/2。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zm-zrp-pool-compaction-without-method-pruning.md`。
  备注：完整 trim analyzer、attribute/annotation suppression/promotion、cross-module/export token、
  field/default-value backed constant-pool remap 和 dump/diff 仍待后续。

- 2026-06-27 05:46:58 +08:00 · 12-S7ZL / 11-S7 zrp string-pool duplicate slice compaction ·
  状态：12-S7/11-S7 支撑子切片完成；12 阶段继续进行中 · 完成项目：
  zrp emitted metadata string-pool remap 现在按内容合并 retained duplicate slices；不同旧 offset
  中相同的 NUL-terminated 字符串只写入一份 emitted payload，同时保留每个旧 offset 的 remap entry，
  让 TypeDef/MethodDef/FieldDef/GenericParam/ModuleRef offset rewrite 仍可解析。
  RED/GREEN：RED 为 direct pool-pruning duplicate string fixture 要求 retained TypeDef/FieldDef
  共享 `Shared` 字符串后，旧 offset-only remap 失败 1/3（Expected 540 Was 547）；GREEN 后
  pool pruning 3/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0。
  测试结果：WSL gcc/clang 同组通过，focused CTest
  `aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping` 2/2；Windows MSVC Debug 同组通过，
  focused CTest 2/2。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zl-zrp-string-pool-duplicate-slice-compaction.md`。
  备注：完整 trim analyzer、attribute/annotation suppression/promotion、cross-module/export token、
  field/default-value backed constant-pool remap 和 dump/diff 仍待后续。

- 2026-06-27 05:31:19 +08:00 · 12-S7ZK trim warning sourceFile quoted escaping ·
  状态：12-S7 子切片完成；12 阶段继续进行中 · 完成项目：runtime fallback trim warning
  comment 的 `sourceFile` 字段现在以 quoted/escaped 形式输出，例如
  `sourceFile="dynamic_deopt_bridge.zr"`；带反斜杠和双引号的路径会写成
  `sourceFile="src\\quoted \"module\".zr"`，避免源文件名中的空格、引号或反斜杠破坏诊断解析。
  RED/GREEN：RED 为 dynamic deopt bridge smoke 要求 quoted/escaped `sourceFile` 后失败 4/7；
  GREEN 后 dynamic deopt bridge smoke 7/0、source contracts 21/0、code stripping 5/0。
  测试结果：WSL gcc/clang 同组通过，CTest `aot_c_code_stripping` 1/1；Windows MSVC Debug
  dynamic deopt bridge smoke 0 failures/7 ignored、source contracts 21/0、code stripping 5/0，
  CTest `aot_c_code_stripping` 1/1；`git diff --check` 退出 0，仅 LF/CRLF 提示。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zk-trim-warning-source-file-escaping.md`。
  备注：完整 trim analyzer、`@requires_unreferenced_code`、reflection data-flow annotation、
  annotation-based suppression/promotion、cross-module/export token 和 dump/diff 仍待后续。

- 2026-06-27 05:19:43 +08:00 · 12-S7ZJ trim warning reasonFlag marker ·
  状态：12-S7 子切片完成；12 阶段继续进行中 · 完成项目：runtime fallback trim warning
  comment 在 textual `reason=...` 外新增 `reasonFlag=<mask>`，数值复用
  `ZR_AOT_RUNTIME_FALLBACK_WARNING_*` suppression mask 映射，使单条 warning 可直接和 reason-mask
  suppression/统计对齐。dynamic-call 输出 `reasonFlag=1`，dynamic-value-access 输出 `reasonFlag=2`。
  RED/GREEN：RED 为 dynamic deopt bridge smoke 要求 `reasonFlag=... reason=...` 后失败 3/6；
  GREEN 后 dynamic deopt bridge smoke 6/0、source contracts 21/0、code stripping 5/0。
  测试结果：WSL gcc/clang 同组通过；Windows MSVC Debug dynamic deopt bridge smoke 0 failures/6 ignored、
  source contracts 21/0、code stripping 5/0，CTest `aot_c_code_stripping` 1/1。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zj-trim-warning-reason-flag.md`。
  备注：完整 trim analyzer、`@requires_unreferenced_code`、reflection data-flow annotation、annotation-based
  suppression/promotion、cross-module/export token 和 dump/diff 仍待后续。

- 2026-06-27 05:07:26 +08:00 · 12-S7ZI / 11-S7 zrp constant-pool orphan sweep ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp metadata pruning 在当前无 constant-pool offset row 字段的 ABI 下，将 constantPool 视为
  无 retained 引用的 orphan payload；pruned header rebuild 现在接受 retained constant-pool byte 数，
  当前 MethodDef pruning 路径写入 0，使 after-trim constantPool section 为空。code stripping
  MethodDef pruning fixture 同步要求 after-trim constantPool 5->0，并把该 5 bytes 计入 pool/metadata
  removed delta。
  RED/GREEN：RED 为 direct pool-pruning fixture 要求孤立 constant pool 清空后失败 1/2（Expected 488 Was 493）；
  GREEN 后 pool pruning 2/0、zrp pruning 5/0、code stripping 5/0。
  测试结果：WSL gcc/clang pool pruning 2/0、direct zrp pruning 5/0、code stripping 5/0、
  source contracts 21/0、frame setup 1/0、typed scalar 1/0、shared-library smoke 8/0，focused CTest 4/4；
  Windows MSVC Debug 同组 pool/code/source/frame 通过，typed scalar 0 failures/1 ignored、shared-library smoke
  0 failures/8 ignored，focused CTest 4/4。WSL clang 仍有既有 generated generic-conversion
  `-Wlogical-not-parentheses` warning。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zi-zrp-constant-pool-orphan-sweep.md`。
  备注：完整 trim analyzer、attribute/annotation promotion/suppression、cross-module/export token、
  dump/diff，以及未来如有 constant literal/default-value row 字段时的 retained constant slice remap 仍待后续。

- 2026-06-27 04:42:55 +08:00 · 12-S7ZH / 11-S7 zrp string-pool sweep/compaction ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp metadata pruning 现在会为 retained TypeDef、retained MethodDef、FieldDef、retained GenericParam
  与 ModuleRef row 引用的 NUL-terminated strings 构建 compacted string pool，并在 blob rebuild 时重写这些 row
  的 string offsets。新增 `backend_aot_c_zrp_metadata_string_pool.{h,c}` 承载 string slice collection、dedupe、
  pool copy 和 offset remap；新增 `backend_aot_c_zrp_metadata_sections.{h,c}` 承载共享 section helper。
  RED/GREEN：RED 为 direct pool-pruning fixture 要求 string pool 40->25、MethodDef name offset 重映射后失败 1/1；
  GREEN 后 pool pruning 1/0、zrp pruning 5/0、code stripping 5/0，并由 source contract 锁定 section/string-pool
  module 边界。测试结果：WSL gcc/clang pool pruning 1/0、direct zrp pruning 5/0、code stripping 5/0、
  source contracts 21/0、frame setup 1/0、typed scalar 1/0、shared-library smoke 8/0，focused CTest 4/4；
  Windows MSVC Debug pool pruning 1/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 4/4。
  WSL clang 仍有既有 generated generic-conversion `-Wlogical-not-parentheses` warning。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zh-zrp-string-pool-compaction.md`。
  备注：完整 trim analyzer、attribute/annotation promotion/suppression、cross-module/export token、constant pool sweep
  和 dump/diff 仍待后续。

- 2026-06-27 03:49:57 +08:00 · 12-S7ZG / 11-S7 zrp MethodSpec signature-pool rewrite/compaction ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp metadata pruning 现在会为 retained signature blob slices 构建 compacted pool，
  重写保留 MethodSpec `GENERIC_INST(MEMBER_REF methodToken, args...)` signature 内的 method token payload，
  并同步更新 token record / MethodSpec 的 signature blob offset 与 stable hash。新增
  `backend_aot_c_zrp_metadata_signature.{h,c}` 承载 slice collection、pool copy、offset remap 和 hash recomputation。
  RED/GREEN：RED 为 MethodSpec-present direct zrp fixture 要求 signature pool 30->15、method token RID 2->1、
  hash 重算后失败 1/5；GREEN 后 zrp pruning 5/0，并由 source contract 锁定 signature module 边界。
  测试结果：WSL gcc/clang direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 1/0、shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 5/0、
  code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。WSL clang 仍有既有
  generated generic-conversion `-Wlogical-not-parentheses` warning。产出：
  `tests/acceptance/2026-06-27-aot-12-s7zg-zrp-methodspec-signature-pool-rewrite.md`。
  备注：完整 trim analyzer、attribute/annotation promotion/suppression、cross-module/export token、非 signature pool sweep
  和 dump/diff 仍待后续。

- 2026-06-26 08:38:24 +08:00 · 12-S7ZF / 11-S7 zrp MethodSpec method-token cascade ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp metadata pruning 现在随 MethodDef table 压缩同步处理 MethodSpec table；
  指向被删除 MethodDef 的 MethodSpec row 会被删除，指向保留 MethodDef 的 `methodToken`
  会重映射到 compacted `MEMBER_DEF` RID。signature blob pool 本切片原样保留，不声明池压缩完成。
  RED/GREEN：RED 为 MethodSpec-present direct zrp fixture 要求 owned pruned blob、MethodSpec count 2->1、
  methodToken RID 2->1 和 signature blob pool 保留后失败 1/5；GREEN 后 zrp pruning 5/0，
  并由 source contract 锁定 MethodSpec remap/count/copy 路径。测试结果：
  WSL gcc/clang direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 5/0、code stripping 5/0、
  source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。产出：
  `tests/acceptance/2026-06-26-aot-12-s7zf-zrp-methodspec-method-token-cascade.md`。
  备注：MethodSpec signature-pool rewrite、pool compaction、annotation promotion/suppression、export token 和 dump/diff 仍待后续。

- 2026-06-26 08:15:19 +08:00 · 12-S7ZE / 11-S7 zrp GenericParamConstraint cascade ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp metadata pruning 现在会随 GenericParam table 压缩同步处理 GenericParamConstraint table；
  指向被删除 MethodDef-owned GenericParam 的 constraints 会被删除，保留 constraints 的 `genericParamIndex`
  会重映射到 compacted GenericParam index，保留 GenericParam 的 constraint range 同步重算。
  RED/GREEN：RED 为 GenericParamConstraint-present direct zrp fixture 要求 owned pruned blob、constraint section count 4->3、
  `genericParamIndex` 2->1 和 constraint range compaction 后失败 1/4；GREEN 后 zrp pruning 4/0，
  并由 source contract 锁定 constraint copy/remap/count/range 路径。测试结果：
  WSL gcc/clang direct zrp pruning 4/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 4/0、code stripping 5/0、
  source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。产出：
  `tests/acceptance/2026-06-26-aot-12-s7ze-zrp-generic-param-constraint-cascade.md`。
  备注：MethodSpec method-token/signature-pool rewrite、pool compaction、annotation promotion/suppression、export token 和 dump/diff 仍待后续。

- 2026-06-26 07:55:51 +08:00 · 12-S7ZD / 11-S7 zrp metadata remap module split ·
  状态：12-S7 与 11-S7 交叉支持性 refinement 完成；11/12 阶段继续进行中 · 完成项目：
  新增 `backend_aot_c_zrp_metadata_remap.{h,c}`，把 emitted zrp pruning 的 MethodDef/FieldDef/GenericParam
  token/range remap helper 从 prune orchestration 中拆出，`backend_aot_c_zrp_metadata_prune.c` 从 982 行降到 549 行。
  RED/GREEN：无行为变化拆分，复用 zrp pruning 3 个 direct fixture；GREEN 后 source contract 21/0 锁定 prune/remap 模块边界。
  测试结果：WSL gcc/clang direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 1/0、shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 3/0、
  code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。产出：
  `tests/acceptance/2026-06-26-aot-12-s7zd-zrp-metadata-remap-module-split.md`。
  备注：为后续 GenericParamConstraint/MethodSpec/pool cascade 继续扩展前收敛模块边界，不改变当前剪枝语义。

- 2026-06-26 07:30:55 +08:00 · 12-S7ZC / 11-S7 zrp GenericParam owner remap ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp MethodDef pruning 现在可处理无 constraints 的 GenericParam rows；TypeDef-owned rows 保留，retained
  MethodDef/FieldDef-owned rows remap owner token，被删除 MethodDef 拥有的 rows 删除，TypeDef/MethodDef generic-param range
  随压缩后的 GenericParam 表重算。RED/GREEN：RED 为 GenericParam-present direct zrp fixture 要求 owned pruned blob 后失败 1/3；
  GREEN 后 zrp pruning 3/0，并由 source contract 锁定 GenericParam remap/count/range/copy 路径。测试结果：
  WSL gcc/clang direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。产出：
  `tests/acceptance/2026-06-26-aot-12-s7zc-zrp-generic-param-owner-remap.md`。
  备注：GenericParamConstraint cascade、MethodSpec signature-pool rewrite、pool compaction、annotation promotion、export token 和 dump/diff 仍待后续。

- 2026-06-26 07:14:57 +08:00 · 12-S7ZB / 11-S7 zrp FieldDef member-token remap ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp MethodDef pruning 现在可处理含 FieldDef rows 的 blob；MethodDef 删除后，FieldDef row token 与 token record
  内的 FieldDef `MEMBER_DEF` 引用会重排到保留 MethodDef 之后，指向已删除 MethodDef 的 token record 继续被移除。
  RED/GREEN：RED 为 FieldDef-present direct zrp fixture 要求 owned pruned blob 和 FieldDef RID 3->2 remap 后失败 1/2；
  GREEN 后 zrp pruning 2/0，并由 source contract 锁定 FieldDef lookup/remap/copy 路径。测试结果：
  WSL gcc/clang direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。产出：
  `tests/acceptance/2026-06-26-aot-12-s7zb-zrp-fielddef-member-token-remap.md`。
  备注：GenericParam/MethodSpec cascade、pool compaction、annotation promotion、export token 和 dump/diff 仍待后续。

- 2026-06-26 06:58:15 +08:00 · 12-S7ZA / 11-S7 zrp token-record MethodDef pruning/remap ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  emitted zrp MethodDef pruning 现在会同步压缩 tokenRecords section，重写保留 MethodDef row 的 `MEMBER_DEF` RID，
  并同步重映射 token record 内指向保留 MethodDef 的 member-token 字段；指向被裁剪 MethodDef 的 token record 被删除。
  含 FieldDef rows 的 blob 暂时保留原始元数据，因为当前 MethodDef/FieldDef 共享 `MEMBER_DEF` token 空间。
  RED/GREEN：RED 为新 direct zrp pruning fixture 要求 owned pruned blob 后失败 1/1（旧 guard 保留原 blob）；
  GREEN 后 zrp pruning 2/0，FieldDef guard 也被锁住。测试结果：WSL gcc/clang direct zrp pruning 2/0、code stripping 5/0、
  source contracts 21/0、frame setup 1/0、typed scalar 1/0、shared-library smoke 8/0，focused CTest 3/3；
  Windows MSVC Debug direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。产出：
  `tests/acceptance/2026-06-26-aot-12-s7za-zrp-token-record-methoddef-pruning.md`。
  备注：FieldDef token remap、GenericParam/MethodSpec cascade、pool compaction、annotation 抑制和 dump/diff 仍待后续。

- 2026-06-26 06:30:39 +08:00 · 12-S7Z / 11-S7 zrp MethodDef metadata pruning ·
  状态：12-S7 与 11-S7 交叉子切片完成；11/12 阶段继续进行中 · 完成项目：
  opt-in code stripping 的 emitted embedded zrp metadata blob 现在会在 reachability filter 后按 stripped function table
  裁剪 MethodDef rows，并让 descriptor length、embeddedModuleBytes、zrp section stats 与 before/after/removed deltas
  读取实际 emitted blob。RED/GREEN：RED 为 code-stripping fixture 要求 retained/removable MethodDef row 剪枝后失败 1/5；
  GREEN 后 MethodDef section 72->36、zrp metadata 446->410、definition-table removed=36。测试结果：
  WSL gcc/clang direct code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、shared-library smoke 8/0，
  focused CTest 2/2；Windows MSVC Debug direct code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 2/2。产出：
  `tests/acceptance/2026-06-26-aot-12-s7z-zrp-methoddef-metadata-pruning.md`。
  备注：完整 trim analyzer、attribute/annotation suppression、GenericParam/MethodSpec/token/pool 跟随剪枝、导出 token 和 dump/diff 仍待后续。

- 2026-06-26 06:00:16 +08:00 · 12-S7Y / 10-S1 / 11-S7 default-min reflection metadata policy ·
  状态：12-S7、10-S1 与 11-S7 交叉子切片完成；10/11/12 阶段继续进行中 · 完成项目：
  AOT C generated MethodInfo 的 reflection metadata level 现在按 writer policy 输出；默认/非裁剪产物保持
  `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，opt-in code stripping 产物输出
  `ZR_AOT_REFLECTION_METADATA_NONE`，文件头新增 `metadata_policy.reflectionLevel` marker。RED/GREEN：
  RED 为 `zr_vm_aot_c_code_stripping_test` 新增 stripped MethodInfo `NONE` 与 policy marker 断言后失败 1/4；
  GREEN 后新增 shared option helper，并把 policy 穿过 C emitter、method metadata emitter 和 byte sampling。
  测试结果：WSL gcc 与 WSL clang 均通过 direct code stripping 4/0、source contracts 21/0、frame setup 1/0、
  typed scalar 1/0、shared-library smoke 8/0，focused CTest 2/2；Windows MSVC Debug 通过 direct code stripping
  4/0、source contracts 21/0、frame setup 1/0、typed scalar 1 test 0 failures/1 ignored、shared-library smoke
  8 tests 0 failures/8 ignored，focused CTest 2/2。产出：
  `tests/acceptance/2026-06-26-aot-12-s7y-default-min-reflection-metadata-policy.md`。
  备注：完整 trim analyzer、attribute/annotation suppression、实际 zrp metadata sweep/pruning、导出 token 和
  dump/diff 工具仍待后续。

- 2026-06-26 05:41:31 +08:00 · 12-S7X release symbol stripping CLI policy ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  `ZrCli_Compiler_ApplyProjectAotWriterOptions()` 把 `.zrp` `aotMode: "full-aot"` 同时映射为
  `requireFullAot = ZR_TRUE` 和 `stripGeneratedSymbols = ZR_TRUE`；缺省/`hybrid` 映射为二者均 false。
  CLI `--emit-aot-c` full-AOT project fixture 生成 C 现在输出
  `/* symbol_stripping.generatedSymbols = 1 */`。RED/GREEN：RED 为 `zr_vm_cli_project_incremental_test`
  新增 full-AOT/hybrid writer option expectations 和 generated-C marker expectation 后失败 3/11；
  GREEN 为 existing project AOT option helper 增加 symbol-stripping policy mapping。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 均通过 direct CLI incremental 11/0、
  generic call typed 7/0、LLVM symbol stripping 2/0，focused CTest
  `cli_project_incremental|aot_c_generic_call_typed|aot_llvm_symbol_stripping` 3/3；Windows generic
  call typed 仍按既有 Unix shared-library guard 3 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7x-release-symbol-stripping-cli-policy.md`。
  备注：本切片未新增单独 `release` manifest 字段；完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning 和默认最小 metadata policy 仍待后续。

- 2026-06-26 05:30:36 +08:00 · 12-S7W LLVM generated-symbol stripping parity ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  LLVM writer 复用 `stripGeneratedSymbols` option；默认保持 `@zr_aot_fn_<flatIndex>`，
  开启后 generated private function definition、function thunk table、entry thunk 和静态 direct-call
  references 改为稳定 `@zr_fn_g<flatIndex>`；`.ll` 头部新增
  `; symbol_stripping.generatedSymbols = 0/1`，公开 `@ZrVm_GetAotCompiledModule` 导出保持不变。
  RED/GREEN：RED 为新增 `zr_vm_aot_llvm_symbol_stripping_test` 后先缺 marker 与 stripped
  `@zr_fn_g0/@zr_fn_g1` 失败 2/2；GREEN 后新增 LLVM function-symbol formatter 并把 strip flag
  穿过 emitter/function body/static direct call/module artifacts。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 均通过 direct LLVM 2/0、code-stripping 4/0、
  generic call typed 7/0，focused CTest
  `aot_llvm_symbol_stripping|aot_c_code_stripping|aot_c_generic_call_typed` 3/3；Windows generic
  call typed 仍按既有 Unix shared-library guard 3 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7w-llvm-generated-symbol-stripping.md`。
  备注：完整 trim analyzer、attribute/annotation 抑制策略、实际 metadata sweep/pruning、
  默认最小 metadata policy 和 release-mode 默认策略/CLI 接线仍待后续。

- 2026-06-26 05:06:33 +08:00 · 12-S7V method metadata generated byte trim delta ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  AOT C 文件头部新增 `code_stripping.methodMetadataGeneratedBytesBefore/After/Removed`
  markers；`backend_aot_write_c_method_infos()` 返回已发射 method signature/info metadata byte total，
  新增 `backend_aot_c_method_metadata_generated_bytes_referenced()` 复用真实 method metadata emitter
  在裁剪前后采样 generated-C byte span。Code-stripping fixture 校验 before >= after、
  removed = before - after、after 等于 `aot_size.methodMetadataBytesTotal`，并区分普通不可达 child
  裁剪 removed > 0 与 export/manifest root 保留 removed = 0。RED/GREEN：RED 为
  `zr_vm_aot_c_code_stripping_test` 缺少 method metadata generated-byte markers 失败 3/4，
  source contract 同步锁定 emitter/header/source plumbing；GREEN 后输出三项 byte-delta markers。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 均通过 direct code-stripping 4/0、
  generic call typed 7/0、source contracts 21/0、frame setup contract 1/0，CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|aot_c_frame_setup_contracts` 2/2。产出：
  `tests/acceptance/2026-06-26-aot-12-s7v-method-metadata-generated-byte-trim-delta.md`。
  备注：真实 metadata pool sweep/pruning、默认最小 metadata policy、release-mode 默认策略/CLI 接线
  和完整 trim analyzer 仍待后续。

- 2026-06-26 04:49:28 +08:00 · 12-S7U release generated-symbol stripping option ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  新增 writer-level `stripGeneratedSymbols` option 与 `symbol_stripping.generatedSymbols`
  generated-C marker；默认模式保持可读泛型 helper/debugName，开启后把 generic monomorphized/shared
  私有 helper 符号和 shared slot `debugName` 剥离为稳定 ID（如 `zr_fn_g1__1`、`zr_fn_g1__shared`、
  `generic#1`），避免在这些 release 私有表面暴露 `Pair`/`Box` 类型名。RED/GREEN：
  RED 为 generic generated-C fixture 使用 `options.stripGeneratedSymbols` 后编译失败，source contract
  同步要求 public option 和 emitter plumbing；GREEN 后 option 经 backend helper 传入 generic emitters。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 均通过 direct code-stripping 4/0、CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3、source contracts 20/0、
  frame setup contract 1/0。产出：`tests/acceptance/2026-06-26-aot-12-s7u-release-generated-symbol-stripping.md`。
  备注：release-mode 默认策略/CLI 接线、完整 trim analyzer、attribute/annotation 抑制和 metadata
  sweep/pruning 仍待后续。

- 2026-06-26 04:21:08 +08:00 · 12-S7T zrp metadata size module split ·
  状态：12-S7 支持性 refinement 完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  新增 `backend_aot_c_zrp_metadata_size.{h,c}`，把 zrp metadata size stats、`aot_size.zrpMetadata*`
  marker 和 `code_stripping.zrpMetadata*Before/After/Removed` marker 写入从主 emitter 拆出；`backend_aot_c_emitter.c`
  从 893 行收回到 763 行，新模块为 116 行。RED/GREEN：RED 为 source contract 要求新模块存在且 emitter
  不再直接读取 zrp header 后 20 个 source contract 中 1 个失败；GREEN 后新模块进入 parser shared 链接，
  行为保持 12-S7S 语义。测试结果：WSL gcc 直接 code-stripping 4/0；WSL gcc/clang/Windows MSVC Debug
  的 `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，source contracts 20/0。
  产出：`tests/acceptance/2026-06-26-aot-12-s7t-zrp-metadata-size-module-split.md`。
  备注：完整 trim analyzer、attribute/annotation suppression、实际 metadata sweep/pruning、默认最小 metadata policy
  和 release 符号剥离仍未关闭；WSL clang 需重新生成 CMake 以接入新 globbed source。

- 2026-06-26 04:04:49 +08:00 · 12-S7S zrp metadata byte trim delta carrier ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  generated AOT C 现在输出 zrp metadata size group 的
  `code_stripping.zrpMetadataBytesBefore/After/Removed`、
  `zrpMetadataTokenRecordBytesBefore/After/Removed`、
  `zrpMetadataDefinitionTableBytesBefore/After/Removed` 和
  `zrpMetadataPoolBytesBefore/After/Removed`。当前 writer 尚未重写 metadata blob，因此
  before=after、removed=0；后续实际 metadata sweep/pruning 可复用该载体。
  RED/GREEN：RED 为 zrp metadata size fixture 要求 code-stripping metadata byte delta markers 后旧生成物缺 marker；
  GREEN 后 total/token-record/definition-table/pool 四组 marker 均输出并保持 removed=0。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_code_stripping_test` 均为 4/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7s-zrp-metadata-byte-trim-delta.md`。
  备注：完整 trim analyzer、attribute/annotation suppression、实际 metadata sweep/pruning、默认最小 metadata policy
  和 release 符号剥离仍未关闭；WSL clang 普通构建入口被无关缺失测试源引用阻塞，已用 fast target 完成聚焦验证。

- 2026-06-26 03:52:50 +08:00 · 12-S7R generated type-layout byte trim delta ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  opt-in code stripping 现在输出 referenced inline type-layout 的 generated-C byte span
  `code_stripping.typeLayoutGeneratedBytesBefore/After/Removed`；采样复用真实 type-layout 发射循环，
  GREEN 后普通 trim fixture 报告 `1072 -> 536`、removed `536`，export/manifest root fixture
  报告 `1072 -> 1072`、removed `0`，且 after 等于 `aot_size.typeLayoutBytesTotal`。
  RED/GREEN：RED 为生成 C 缺少 generated-byte markers，code-stripping 测试 4 个用例中 3 个失败；
  GREEN 后新增 markers、裁剪差值和 after/total 一致性断言均通过。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_code_stripping_test` 均为 4/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7r-generated-type-layout-byte-trim-delta.md`。
  备注：完整 trim analyzer、attribute/annotation suppression、metadata sweep diff、默认最小 metadata policy
  和 release 符号剥离仍未关闭；WSL clang 普通构建入口被无关缺失测试源引用阻塞，已用 fast target 完成聚焦验证。

- 2026-06-26 03:24:43 +08:00 · 12-S7Q runtime fallback warning source file attribution ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  runtime fallback trim warning marker 现在从 `SZrFunction.sourceCodeList` 输出 `sourceFile=<file>`，
  缺失或空 source file 回退为 `<unknown>`。
  RED/GREEN：RED 为动态 deopt warning fixture 要求 `sourceFile=dynamic_deopt_bridge.zr` 后旧生成物缺 marker；
  GREEN 后 dynamic-call 与 dynamic-value-access warning 均输出
  `sourceFile=dynamic_deopt_bridge.zr sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19`，
  suppressed fixture 仍只输出 suppressed count。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 6/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。Windows 动态 deopt shared-library 用例按既有规则 6 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7q-runtime-fallback-warning-source-file.md`。
  备注：完整 trim analyzer、attribute/annotation suppression、pre-trim generated-C type/layout byte span attribution、
  metadata sweep diff、默认最小 metadata policy 和 release 符号剥离仍未关闭。

- 2026-06-26 03:14:26 +08:00 · 12-S7P runtime fallback warning column span ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  `SZrAotExecIrInstruction` 现在携带 `debugColumn/debugColumnEnd`，runtime fallback trim warning marker 输出
  `sourceLine=<start> sourceLineEnd=<end> sourceColumn=<start> sourceColumnEnd=<end>`；
  source-location 推导从 `backend_aot_exec_ir.c` 拆入 `backend_aot_exec_ir_source_location.{h,c}`。
  RED/GREEN：RED 为动态 deopt warning fixture 要求 `sourceColumn=7 sourceColumnEnd=19` 后旧生成物缺 marker；
  GREEN 后 dynamic-call 与 dynamic-value-access warning 均输出 `sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19`，
  suppressed fixture 仍只输出 suppressed count。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 6/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。Windows 动态 deopt shared-library 用例按既有规则 6 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7p-runtime-fallback-warning-column-span.md`。
  备注：为满足大文件边界，`backend_aot_exec_ir.c` 已从 1023 行收回到 906 行；完整 trim analyzer、source file attribution、
  attribute/annotation suppression、pre-trim generated-C type/layout byte span attribution、metadata sweep diff、
  默认最小 metadata policy 和 release 符号剥离仍未关闭。

- 2026-06-26 02:46:07 +08:00 · 12-S7O runtime fallback warning reason-mask suppression ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  `EZrAotRuntimeFallbackWarningFlag` 和
  `SZrAotWriterOptions.suppressRuntimeFallbackWarningReasonMask` 现在支持按 reason 抑制 hybrid
  runtime fallback trim warning；全局 `suppressRuntimeFallbackWarnings` 继续等价 all-reasons mask；
  visible count 与 suppressed count 分离，非匹配 reason 仍输出 compact `trim_warning.runtimeFallback[...]` marker。
  RED/GREEN：RED 为动态 deopt reason-mask fixture 引用新 option/constant 后构建失败；GREEN 后 dynamic-call
  生成物输出 `runtimeFallbackCount = 0`、`runtimeFallbackSuppressedCount = 1` 且无 visible marker，
  dynamic-value-access 在同一 mask 下仍输出 `sourceLine=41 sourceLineEnd=43 reason=dynamic-value-access`。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 6/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。Windows 动态 deopt shared-library 用例按既有规则 6 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7o-runtime-fallback-warning-reason-mask-suppression.md`。
  备注：完整 trim analyzer、file/column source span、attribute/annotation suppression、pre-trim generated-C
  type/layout byte span attribution、metadata sweep diff、默认最小 metadata policy 和 release 符号剥离仍未关闭。

- 2026-06-26 02:26:36 +08:00 · 12-S7N runtime fallback source line span ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  ExecIR instruction 现在携带 `debugLineEnd`，runtime fallback trim warning marker 输出
  `sourceLine=<start> sourceLineEnd=<end>`，line-span 来源优先 execution location，随后 fallback 到 per-instruction
  line list 与 function end/start line。
  RED/GREEN：RED 为动态 deopt warning fixture 要求 `sourceLineEnd=43` 后旧生成物缺 marker；GREEN 后
  dynamic-call 与 dynamic-value-access warning 均输出 `sourceLine=41 sourceLineEnd=43`，suppressed fixture
  仍只输出 suppressed count。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 5/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。Windows 动态 deopt shared-library 用例按既有规则 5 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7n-runtime-fallback-source-line-span.md`。
  备注：完整 trim analyzer、file/column source span、per-warning/attribute-based suppression、pre-trim generated-C
  type/layout byte span attribution、metadata sweep diff、默认最小 metadata policy 和 release 符号剥离仍未关闭。

- 2026-06-26 02:12:56 +08:00 · 12-S7M runtime fallback warning suppression ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  `SZrAotWriterOptions.suppressRuntimeFallbackWarnings` 现在可让 hybrid AOT C 生成物抑制 runtime fallback
  trim warning entry，并用 `trim_warnings.runtimeFallbackSuppressedCount` 保留被抑制数量。
  RED/GREEN：RED 为动态 deopt fixture 引用新 writer option 后编译失败；GREEN 后 suppressed 生成物输出
  `runtimeFallbackCount = 0`、`runtimeFallbackSuppressedCount = 1`，保留 dynamic deopt bridge 调用，且没有
  `trim_warning.runtimeFallback[0]`。
  测试结果：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 5/0；WSL gcc/clang 和
  Windows MSVC Debug 均通过 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  并通过 source contracts 19/0。Windows 动态 deopt shared-library 用例按既有规则 5 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7m-runtime-fallback-warning-suppression.md`。
  备注：完整 trim analyzer、完整 source span、per-warning/attribute-based suppression、pre-trim generated-C
  type/layout byte span attribution、metadata sweep diff、默认最小 metadata policy 和 release 符号剥离仍未关闭。

- 2026-06-26 01:56:33 +08:00 · 12-S7L type-layout payload byte trim delta ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7 仍未关闭 · 完成项目：
  generated AOT C writer 现在输出 `code_stripping.typeLayoutPayloadBytesBefore/After/Removed`，按 distinct
  inline layout 的 `frameSlotLayout.byteSize` 聚合 opt-in code stripping 前后 referenced payload bytes。
  RED/GREEN：RED 为 code-stripping 生成 C fixture 缺 payload byte delta marker；GREEN 后普通裁剪 fixture
  输出 before=16、after=8、removed=8，export root 与 manifest root 输出 before=16、after=16、removed=0。
  测试结果：WSL gcc/clang 和 Windows MSVC Debug 均通过 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，并通过 source contracts 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7l-type-layout-payload-byte-trim-delta.md`。
  备注：完整 trim analyzer、source span/warning 抑制、pre-trim generated-C type/layout byte span attribution、
  metadata sweep diff、默认最小 metadata policy 和 release 符号剥离仍未关闭。

- 2026-06-26 01:40:40 +08:00 · 12-S7K zrp metadata section/table/pool byte statistics ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7/11-S7 仍未关闭 · 完成项目：
  generated AOT C writer 现在在 embedded module size marker 后识别有效 zrp data metadata blob，并输出
  total、token-record、definition-table、pool 与 12 个 section 的 byte attribution marker；非 zrp 或缺失 blob
  保持稳定 0 值统计。RED/GREEN：RED 为 zrp metadata size fixture 缺少 zrp 统计 marker；GREEN 后生成物包含
  `zrpMetadataBytes = 374`、`zrpMetadataTokenRecordBytes = 96`、
  `zrpMetadataDefinitionTableBytes = 52`、`zrpMetadataPoolBytes = 18`，并列出
  `tokenRecords/typeDefs/stringPool/signatureBlobPool/constantPool` 等 section marker。
  测试结果：WSL gcc/clang 和 Windows MSVC Debug 均通过 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，并通过 source contracts 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7k-zrp-metadata-section-byte-statistics.md`。
  备注：完整 trim analyzer、source span/warning 抑制、metadata/type-layout 裁剪前后 diff、默认最小 metadata 策略、
  zrp dump/diff 和 release 符号剥离仍未关闭。

- 2026-06-26 01:14:40 +08:00 · 11-S4R generated ownership offset table emission ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  generated `SZrTypeLayout` descriptor 现在为可安全导出 offset 的 struct owner fields 写出
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并将 `.ownershipFieldOffsets` 指向该表；zero-count、union 和 unsafe/
  unsupported offset 路径保持 `ZR_NULL` 并输出 failure marker。RED/GREEN：RED 为 type-layout source contract
  要求 ownership-offset writer helper 后缺少 `backend_aot_c_type_layout_can_emit_ownership_offsets(`；GREEN 后
  `Unique<string>` 字段的 `Holder` generated C 出现 `/* zr_aot_ownership_offsets layout=0 count=1 */`、
  `ZrOwnershipOffsets_0[]`、`.ownershipFieldCount = 1u` 和 `.ownershipFieldOffsets = ZrOwnershipOffsets_0`。
  测试结果：WSL gcc/clang 均通过 CTest `aot_c_type_layout_contracts` 1/1、source contracts 19/0、value-type smoke
  4/0；Windows MSVC Debug 通过同组 CTest 1/1、source contracts 19/0、value-type smoke 3/0/1 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4r-generated-ownership-offset-table.md`。
  备注：union ownership offset 表、持久 cTypeId→token 索引、runtime generic layout construction、cross-module token table
  和 public reflection entity 仍未关闭。

- 2026-06-26 00:42:14 +08:00 · 11-S4Q generated TypeSpec-backed type layout token population ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  generated C token-table writer 在 TypeDef 未命中后会结构匹配 current-function `TYPE_SPEC` signature blob，
  并为唯一匹配的 generated generic layout 写入真实 TypeSpec token；`Pair<int, int>` generated layout 现在在
  `zr_aot_type_layout_tokens[]` 中得到 `0x07000001u`，缺 metadata、多重匹配、跨模块/unsupported signature
  形态仍保守写 0。RED/GREEN：RED 为 `aot_c_generic_call_typed` 新增 TypeSpec token 断言后旧生成物 table 全为
  `0u`；GREEN 后 generated token slot、shared-library 编译和既有 TypeDef-backed union token smoke 均通过。
  测试结果：WSL gcc/clang CTest `aot_c_type_layout_contracts|aot_c_generic_call_typed` 2/2，通过 source contracts
  19/0 与 value-type smoke 3/0；Windows MSVC Debug 通过同组 CTest 2/2、source contracts 19/0、value-type smoke
  2/0/1 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4q-generated-typespec-type-layout-token-population.md`。
  备注：持久 cTypeId→token 索引、ownership offsets、runtime generic layout construction、cross-module token table
  和 public 泛型反射实体仍未关闭。

- 2026-06-26 00:13:32 +08:00 · 11-S4P generated TypeDef-backed type layout token population ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  generated C 的 `zr_aot_type_layout_tokens[]` 现在对唯一匹配本地 TypeDef metadata 的 named struct/union
  `SZrTypeLayout` 写入真实 `TYPE_DEF` token；token writer 拆到
  `backend_aot_c_type_layout_tokens.c`，type-layout emitter 暴露 generated table resolver，并为 union layout
  发射 runtime descriptor。缺 TypeDef metadata、多重匹配、TypeSpec/generic entries 仍写 0。
  RED/GREEN：RED 为 union `Shape` generated-C smoke 缺 `ZrTypeLayout_` descriptor 和非零 token；GREEN 后
  generated union layout registry、`zr_aot_type_layout_tokens[]`、`0x02000001u` token entry 和无 debug-only
  `typeLayoutToken` 注释断言通过。测试结果：WSL gcc/clang 直接运行通过 metadata TypeSpec layout 14/0、
  AOT type-layout contracts 1/0、source contracts 19/0、frame setup 1/0、shared-library smoke 8/0、value-type
  smoke 3/0；Windows MSVC Debug 通过 metadata 14/0、type-layout contracts 1/0、source contracts 19/0、
  frame setup 1/0，shared/value-type smoke 的 Unix-only 分支按既有规则 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4p-generated-type-layout-token-population.md`。
  备注：TypeSpec/generic token population、持久 cTypeId→token 索引、ownership offsets、runtime layout
  construction 和 public reflection entity 仍未关闭。

- 2026-06-25 23:13:20 +08:00 · 11-S4O code-registration type layout token carrier ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  AOT ABI 10 新增 `typeLayoutTokens/typeLayoutTokenCount` carrier，generated C 发射
  `zr_aot_type_layout_tokens[]` 并连接 module descriptor/codeRegistration；loader 校验 token table shape/count，
  metadata runtime attach 镜像 token count，`ResolveTypeLayoutToken()`/`ResolveCTypeIdToken()` 先读 registration
  token 表，再 fallback 到 zrp scan。RED/GREEN：RED 为 token carrier source/runtime tests 缺字段和 runtime mirror；
  GREEN 后手工非零 token 表消费、非 type token/缺 layout 拒绝、generated table shape 与 descriptor validation 通过。
  测试结果：WSL gcc/clang 直接运行通过 metadata TypeSpec layout 14/0、source contracts 19/0、frame setup 1/0、
  shared-library smoke 8/0、value-type smoke 2/0；Windows MSVC Debug 通过 metadata TypeSpec layout 14/0、
  source contracts 19/0、frame setup 1/0，shared/value-type smoke 的 Unix-only 分支按既有规则 ignored。产出：
  `tests/acceptance/2026-06-25-aot-11-s4o-type-layout-token-carrier.md`。
  备注：generated entries 当前零填充；真实 token 填充/持久 cTypeId→token 索引、TypeSpec/generic layout
  materialization、ownership offsets、runtime layout construction 和 public 泛型反射实体仍未关闭。

- 2026-06-25 22:22:13 +08:00 · 11-S4N cTypeId to TypeDef/TypeSpec token resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  新增 `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)`，在当前 `cTypeId == typeLayoutId`
  不变量下复用 typeLayoutId→token resolver、bounded multi-entry cache 与 TypeDef/TypeSpec binding-view
  registry-backed 校验；null、NONE 和 stale prototype cache 路径返回 0。
  RED/GREEN：RED 为 cTypeId→token focused tests 暴露缺少 public resolver/API；GREEN 后 TypeDef/TypeSpec
  cTypeId 反查、多项 cache 命中和 no-prototype-fallback 通过。测试结果：WSL gcc/clang CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` 4/4；
  Windows MSVC Debug 通过 metadata TypeSpec layout 12/0、zrp metadata format 11/0、metadata query 22/0、
  metadata type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4n-ctype-id-token-resolver.md`。
  备注：持久 cTypeId→token 索引表、TypeSpec/generic layout materialization、ownership offset 表、运行期 layout
  构建和 public 泛型反射实体仍未关闭。

- 2026-06-25 22:13:54 +08:00 · 11-S4M bounded multi-entry type layout cache ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  `SZrMetadataRuntime` 将 TypeDef/TypeSpec token/layout cache 从单项最近命中扩展为 8 项 bounded cache，
  `ResolveTypeTokenLayout()` 按 token 查/写 cache，`ResolveTypeLayoutToken()` 按 layout id 查/写 cache，
  未命中时仍经 TypeDef/TypeSpec binding view 校验 registry-backed layout；同一 runtime 可同时保留 TypeDef 与
  TypeSpec 的正向和反向命中，registry 表项清空后仍可命中已缓存项，满表按 round-robin 覆盖。
  RED/GREEN：RED 为多项 token/layout cache focused tests 暴露旧单项 cache 覆盖前一个 TypeDef/TypeSpec 命中；
  GREEN 后两项多项 cache 用例和既有 TypeSpec layout 组均通过。测试结果：WSL gcc/clang CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` 4/4；
  Windows MSVC Debug 通过 metadata TypeSpec layout 10/0、zrp metadata format 11/0、metadata query 22/0、
  metadata type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4m-multi-entry-type-layout-cache.md`。
  备注：持久 cTypeId→token 索引表、TypeSpec/generic layout materialization、ownership offset 表、运行期 layout
  构建和 public 泛型反射实体仍未关闭。

- 2026-06-25 21:53:56 +08:00 · 11-S4L typeLayoutId to TypeDef/TypeSpec token reverse resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  新增 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)`，从最近一次
  token/layout cache 命中反查 type token，未命中时扫描 zrp `TYPE_DEFS` / `TYPE_SPECS` 并复用 binding view
  确认 registry-backed layout 后返回 TypeDef/TypeSpec token；null、NONE、缺 registry layout 和 stale prototype cache
  路径返回 0。RED/GREEN：RED 为 focused TypeSpec layout 测试新增 layoutId→token coverage 后缺少 API；GREEN 后
  TypeDef/TypeSpec layoutId 反查、cache 命中和 no-prototype-fallback 负向用例通过。测试结果：WSL gcc/clang CTest
  metadata runtime 相关 4/4 通过；Windows MSVC Debug 通过 metadata TypeSpec layout 8/0、zrp metadata format 11/0、
  metadata query 22/0、metadata type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4l-layout-id-token-reverse-cache.md`。
  备注：完整多项 token/cTypeId/layout cache、持久 cTypeId→token 索引、TypeSpec/generic layout materialization、
  ownership offset 表和 public 泛型反射实体仍未关闭。

- 2026-06-25 21:37:38 +08:00 · 11-S4K TypeDef/TypeSpec token layout cache resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/10-S4 仍未关闭 · 完成项目：
  新增 `ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, typeToken, outTypeLayoutId)`，通过 TypeDef/TypeSpec
  binding view 把 type token 解析到 code-registration registry-backed `SZrTypeLayout`，缓存最近一次
  token→layoutId/layout 命中，拒绝 null、非 type token 和缺 registry layout 的路径，并在失败时复位输出 layout id。
  RED/GREEN：RED 为 focused TypeSpec layout 测试新增 public resolver/cache 覆盖后缺少 API；GREEN 后 TypeDef cache、
  TypeSpec cache、非 type token 拒绝和 stale prototype cache 负向用例通过。测试结果：WSL gcc/clang CTest
  metadata runtime 相关 4/4 通过；Windows MSVC Debug 通过 metadata TypeSpec layout 5/0、zrp metadata format 11/0、
  metadata query 22/0、metadata type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4k-type-token-layout-cache.md`。
  备注：完整多项 token/cTypeId/layout cache、cTypeId→token 反查、TypeSpec/generic layout materialization、
  ownership offset 表和 public 泛型反射实体仍未关闭。

- 2026-06-25 21:18:46 +08:00 · 11-S4J TypeSpec layout binding view ·
  状态：11-S4 子切片完成、11 阶段继续进行中；10-S4 获得后续类型实参反射的数据路径支撑，
  但完整 10-S4/11-S4 仍未关闭 · 完成项目：`SZrZrpMetadataTypeSpecRow` 使用 `typeLayoutId` 记录 closed
  TypeSpec 对应的 layout id；新增 `SZrMetadataRuntimeTypeSpecLayoutBindingView` 和
  `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, typeSpecToken, outView)`，从 `TYPE_SPEC`
  token 绑定 type record、paired signature record、zrp `TYPE_SPECS` row、11-S3K generic base-token binding、
  `typeLayoutId/cTypeId/signatureHash` 和 code-registration registry 中的 `SZrTypeLayout`。row 的
  `signatureBlobOffset/signatureBlobLength/signatureHash` 必须与 token/signature record 一致，缺 registry layout
  时不 fallback 到 prototype layout cache。实现上把 TypeDef/TypeSpec/FieldDef layout binding reader 拆入
  `metadata_runtime_layout_binding.c`。RED/GREEN：RED 为 focused TypeSpec layout 测试新增后缺少 row 字段、view type
  和 API；GREEN 后正向 TypeSpec binding 与 stale prototype cache 负向用例通过。测试结果：WSL gcc/clang/MSVC Debug
  均通过 metadata TypeSpec layout 2/0、zrp metadata format 11/0、metadata query 22/0、metadata type-layout 10/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4j-typespec-layout-binding-view.md`。
  备注：完整 TypeSpec/generic layout materialization、ownership offset 表、runtime layout construction、
  token/cTypeId/layout cache，以及 public 泛型反射实体仍未关闭。

- 2026-06-25 20:43:52 +08:00 · 11-S4I FieldDef layout binding view ·
  状态：11-S4 子切片完成、11 阶段继续进行中；10-S4 获得后续 token-driven 字段反射实体的数据路径支撑，
  但完整 10-S4/11-S4 仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeFieldDefLayoutBindingView` 和
  `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(runtime, fieldDefToken, outView)`，从 attached zrp metadata
  的 `FIELD_DEFS` section 读取 FieldDef row，绑定 field token record、owner TypeDef token/record/row、
  FieldDef `byteOffset/typeLayoutId` 和 owner `typeLayoutId`，并要求 owner/field layout 都经
  code-registration layout registry 解析；缺 registry layout 时不 fallback 到 prototype layout cache。RED/GREEN：
  RED 为 focused metadata runtime query 新增 FieldDef binding view 后缺少 view type/API 编译失败；GREEN 后
  正向 binding 与 stale prototype cache 负向用例通过。测试结果：WSL gcc/clang/MSVC Debug 均通过 metadata query
  22/0 和 metadata type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4i-fielddef-layout-binding-view.md`。
  备注：TypeSpec/generic layout materialization、ownership offset 表、runtime layout construction、完整三向缓存，
  以及 public token-driven field reflection entity 仍未关闭。

- 2026-06-25 20:27:41 +08:00 · 11-S4H / 10-S4A reflection registry-backed layout consumer ·
  状态：11-S4 与 10-S4 子切片完成、10/11 阶段继续进行中；完整 10-S4/11-S4 仍未关闭 ·
  完成项目：新增 `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(function, prototype, outTypeLayoutId)`，
  通过函数自身或 prototype-context entry function 的 prototype 实例表映射 `typeLayoutId`，再从 attached
  code-registration layout registry 解析 `SZrTypeLayout`，且不 fallback 到 prototype layout cache；反射 type/member
  layout 消费端在 attached registry 下读取 `SZrTypeLayout`，字段 offset/size 按实例字段序号从
  `SZrTypeLayout.fields[i]` 写回，非 AOT/native/无 registry 匹配时保留旧 fallback。RED/GREEN：RED 为 focused
  metadata runtime type-layout 测试新增 prototype layout resolver 后缺少 API 链接失败；GREEN 后 stale prototype
  cache 不会使 detached 函数成功，反射源码契约锁定 metadata runtime include、resolver 调用、类型/字段 layout
  写入 helper 和实例字段序号消费。测试结果：WSL gcc/clang/MSVC Debug 均通过 metadata type-layout 10/0、
  metadata query 20/0、AOT type-layout contracts 1/0、source contracts 19/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4h-10-s4a-reflection-layout-registry-consumer.md`。
  备注：11-S4 的 TypeSpec/generic layout materialization、ownership offset 表、runtime layout construction 和完整三向缓存仍未关闭；
  10-S4 的泛型参数反射、token-driven 字段实体和类型实参反射仍未关闭。

- 2026-06-25 19:31:46 +08:00 · 11-S4G GC inline-frame metadata-runtime layout resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：AOT functions 绑定 code-registration layout registry，GC mark/rewrite 的 inline-frame
  layout resolver 对 attached registry 函数使用 `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()`，并在
  registry layout 缺失时拒绝 prototype fallback；function reset 清空 attached registry 字段；未绑定 registry 的 VM/interpreter 函数保留旧
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout()` fallback。RED/GREEN：RED 为新增 function-level resolver
  测试缺少 attach/resolve API；第二个 RED 为 GC inline-frame minor rewrite work 计数从 2 降到 1；GREEN 后
  AOT registry lookup 与非 AOT fallback 均通过。测试结果：WSL gcc/clang/MSVC Debug 均通过 metadata
  type-layout 7/0、metadata query 20/0、AOT GC root-frame 5/0、GC 66/0、value-type runtime 14/0、frame setup
  1/0、source contracts 19/0、value-type smoke 2/0、shared-library smoke 8/0、descriptor diagnostics 2/0、
  generic reference sharing 4/0；MSVC ignored 均为既有 Unix-only 分支。产出：
  `tests/acceptance/2026-06-25-aot-11-s4g-gc-inline-frame-runtime-layout-resolver.md`。
  备注：反射 consumer 迁移已由 11-S4H / 10-S4A 后续切片处理；TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表和完整三向缓存仍未关闭。

- 2026-06-25 18:45:50 +08:00 · 11-S4F GC descriptor metadata-runtime resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, typeLayoutId)`，
  通过 `codeRegistration->gcDescriptors[typeLayoutId]` 解析 GC descriptor，并强制同一 id 先经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析到 code-registration layout registry；空 runtime、
  `NONE`、越界、稀疏 descriptor、descriptor id 不匹配或 layout registry 缺失均返回 null，prototype layout
  cache 不作为 fallback。RED/GREEN：RED 为 metadata type-layout focused 测试新增 GC descriptor resolver
  用例后编译失败；GREEN 后返回 matching descriptor，且 stale prototype cache/单独 descriptor 表不能绕过
  runtime layout resolver。测试结果：WSL gcc/clang 均通过 metadata type-layout 5/0、metadata query 20/0、
  AOT GC root-frame 5/0、frame setup 1/0、source contracts 19/0、value-type smoke 2/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0、generic reference sharing 4/0；Windows MSVC Debug
  同组通过，其中 value-type smoke 1 ignored、shared-library smoke 8 ignored、descriptor diagnostics 2 ignored
  均为既有 Unix-only 分支。产出：
  `tests/acceptance/2026-06-25-aot-11-s4f-gc-descriptor-runtime-resolver.md`。
  备注：本记录只关闭 code-registration GC descriptor lookup 的 metadata-runtime 入口；TypeSpec/generic layout
  materialization、runtime layout construction、反射 consumer 迁移、GC inline-frame scanning 迁移和完整三向缓存
  仍未关闭。

- 2026-06-25 18:22:45 +08:00 · 11-S4E generic dictionary metadata-runtime type-layout resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：泛型字典 TYPE_LAYOUT/SIZEOF runtime helper 改为接收 `SZrMetadataRuntime*`，
  经 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 读取 code-registration layout registry；generated C 的
  `ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, slotIndex)` 宏和 shared-reference generic 函数签名同步携带
  `metadataRuntime`，prototype layout cache 不再作为 fallback。RED/GREEN：RED 为 reference-sharing 测试新增
  registry layout 与 stale prototype layout 同 id 不同 size 的断言后失败；GREEN 后 TYPE_LAYOUT/SIZEOF 返回 registry
  layout/size，缺失 registry 时返回 null/false。测试结果：WSL gcc/clang 均通过 reference sharing 4/0、
  generic call typed 6/0、source contracts 19/0、frame setup 1/0、metadata type-layout 3/0、
  metadata query 20/0、shared-library smoke 8/0、value-type shared-library smoke 2/0、descriptor diagnostics 2/0；
  Windows MSVC Debug 同组通过，其中 generic call typed 3 ignored、shared-library smoke 8 ignored、
  value-type shared-library smoke 1 ignored、descriptor diagnostics 2 ignored 均为既有 Unix-only 分支。
  产出：`tests/acceptance/2026-06-25-aot-11-s4e-generic-dictionary-type-layout-runtime-resolver.md`。
  备注：本记录只关闭 08 字典 layout consumer 接入 11 的单一 layout 表；TypeSpec/generic layout materialization、
  runtime layout construction、反射/GC consumer 统一迁移和 full-AOT 泛型闭包仍未关闭。

- 2026-06-25 17:54:28 +08:00 · 11-S4D metadata runtime public type-layout resolver ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`SZrMetadataRuntime` attach 时保存 `typeLayoutCount`，新增
  `ZrCore_MetadataRuntime_ResolveTypeLayout()`，通过 runtime 的 code-registration layout registry
  解析 `typeLayoutId`，拒绝空表、`NONE`、越界、稀疏空项和 `cTypeId` 不匹配，并让 TypeDef binding view
  复用同一入口。RED/GREEN：RED 为新增 `zr_vm_metadata_runtime_type_layout_test` 后编译失败，缺少
  `typeLayoutCount` 和 public resolver；GREEN 后 resolver 返回 registry 中的 matching layout，且不会 fallback
  到 prototype layout cache。测试结果：WSL gcc/clang 均通过 type-layout runtime 3/0、metadata runtime query 20/0、
  frame setup 1/0、source contracts 19/0、shared-library smoke 8/0、value-type shared-library smoke 2/0、
  descriptor diagnostics 2/0；Windows MSVC Debug 通过 type-layout runtime 3/0、metadata runtime query 20/0、
  frame setup 1/0、source contracts 19/0、shared-library smoke 8/0（8 ignored Unix-only 分支）、
  value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、descriptor diagnostics 2/0
  （2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4d-metadata-runtime-public-type-layout-resolver.md`。
  备注：本记录只关闭 public runtime layout registry lookup；TypeSpec/generic layout、ownership offsets、
  反射/泛型/GC 统一强制读取和 metadata policy 仍未关闭。

- 2026-06-25 17:32:12 +08:00 · 11-S4C metadata runtime registry-backed TypeDef layout binding ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 继续暴露 TypeDef token、
  zrp TypeDef row、`typeLayoutId/cTypeId`、layout version/hash，但 `typeLayout` 指针现在只从
  `codeRegistration->typeLayouts[typeLayoutId]` 读取，不再使用 prototype layout cache 作为运行期返回来源。
  RED/GREEN：RED 为 metadata runtime query 新增 registry-backed 断言后失败，期望 code-registration registry
  指针但实际返回 prototype layout；GREEN 后 TypeDef row 绑定到同一 generated/module layout registry。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 20/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、value-type shared-library smoke 2/0、descriptor diagnostics 2/0；Windows MSVC Debug
  通过 metadata runtime query 20/0、frame setup 1/0、source contracts 19/0，shared-library smoke 8/0
  （8 ignored Unix-only 分支）、value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4c-metadata-runtime-registry-backed-layout-binding.md`。
  备注：本记录只关闭 TypeDef binding view 对 code-registration layout registry 的运行期读取；
  TypeSpec/generic layout、ownership offsets、反射/泛型/GC 统一强制读取和 metadata policy 仍未关闭。

- 2026-06-25 17:17:50 +08:00 · 11-S4B code-registration type layout registry ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：ABI 9 在 `SZrAotCodeRegistration` 与 `ZrAotCompiledModule` 暴露
  `typeLayouts/typeLayoutCount`，generated C 发射 `SZrTypeLayoutField`、`SZrTypeLayout` 和
  `zr_aot_type_layouts[]` 稀疏 registry，并让 module descriptor 与 code registration 指向同一表。
  runtime descriptor validation 现在拒绝 registry 指针/计数不一致和空表形态不一致。
  RED/GREEN：RED 为 source contract 要求 `typeLayouts/typeLayoutCount` 后失败，缺少
  `const struct SZrTypeLayout *const *typeLayouts;`；GREEN 后无 layout 模块暴露 null/0，一般值类型模块暴露
  非空 registry 且 `layout->cTypeId` 与 GC descriptor 的 `typeLayoutId` 对齐。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 19/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、value-type shared-library smoke 2/0、descriptor diagnostics 2/0；Windows MSVC Debug
  通过 metadata runtime query 19/0、frame setup 1/0、source contracts 19/0，shared-library smoke 8/0
  （8 ignored Unix-only 分支）、value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4b-code-registration-type-layout-registry.md`。
  备注：本记录只关闭 generated-C code-registration layout registry 的 carrier/emission/validation；
  TypeSpec/generic layout、ownership offsets、反射/泛型/GC 统一强制读取和 metadata policy 仍未关闭。

- 2026-06-25 16:39:14 +08:00 · 11-S4A metadata runtime TypeDef layout binding view ·
  状态：11-S4 子切片完成、11 阶段继续进行中；完整 11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeTypeDefLayoutBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`，把 `TYPE_DEF` token 绑定到现有 type record、
  attached zrp `TypeDefRow.typeLayoutId`、`cTypeId`、layout version/hash，并在已缓存 layout 的 `cTypeId`
  匹配时暴露 `SZrTypeLayout` 指针。
  RED/GREEN：RED 为 metadata runtime query 新增 TypeDef layout binding view 用例后编译失败，缺少
  view 类型与 `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`；GREEN 后空/非法输入拒绝，
  合法 TypeDef row 绑定到 token record、cTypeId 和 matching cached layout。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 19/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 19/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4a-metadata-runtime-typedef-layout-binding-view.md`。
  备注：本记录只关闭 TypeDef-backed token→cTypeId/layout identity view；code-registration layout registry、
  TypeSpec/generic layout、反射/泛型/GC 统一强制读取和 metadata policy 仍未关闭。

- 2026-06-25 16:16:39 +08:00 · 11-S3M metadata runtime MethodSpec signature view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeMethodSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`，读取 `SIGNATURE` token 承载的 MethodSpec
  direct signature record，验证 `GENERIC_INST(MEMBER_REF methodToken, args...)`，并暴露 method token/record
  与 argument count/list offset。
  RED/GREEN：RED 为 metadata runtime query 新增 MethodSpec signature view 用例后编译失败，缺少
  view 类型与 `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`；GREEN 后空/非法输入拒绝，
  合法 MethodSpec signature view 绑定到本地 `MEMBER_DEF` method record。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 18/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 18/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3m-metadata-runtime-methodspec-signature-view.md`。
  备注：本记录只关闭 MethodSpec signature view 与 method record binding；method instantiation、
  generic dictionary、token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 16:04:29 +08:00 · 11-S3L metadata runtime generic TypeSpec argument binding view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeTypeSpecGenericArgumentView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`，按 index 读取 `GENERIC_INST`
  argument type-node，并把直接 `TYPE_REF/TYPE_DEF` argument 绑定到现有 type record 的 token/record。
  RED/GREEN：RED 为 metadata runtime query 新增 generic argument binding view 用例后编译失败，缺少
  view 类型与 `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`；GREEN 后空/非法输入和越界
  argument index 拒绝，`GENERIC_INST(TYPE_REF base, INT64, TYPE_REF arg)` 的 primitive argument 与
  direct `TYPE_REF` argument binding 通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 17/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 17/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3l-metadata-runtime-generic-typespec-argument-binding.md`。
  备注：本记录只关闭 TypeSpec generic indexed argument view 与直接 type-token argument binding；
  nested/recursive argument binding、MethodSpec runtime binding、token→运行期实体物化、layout 三向表和
  metadata policy 仍未关闭。

- 2026-06-25 15:48:39 +08:00 · 11-S3K metadata runtime generic TypeSpec base-token binding view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeTypeSpecGenericBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()`，复用 TypeSpec signature view 并把
  `GENERIC_INST` 的 base `TYPE_REF/TYPE_DEF` node 绑定到现有 type record 的 token/record。
  RED/GREEN：RED 为 metadata runtime query 新增 base-token binding view 用例后编译失败，缺少
  view 类型与 `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()`；GREEN 后空/非法输入拒绝，
  `GENERIC_INST(TYPE_REF, INT64)` 与 `GENERIC_INST(TYPE_DEF, INT64)` 分别绑定到 module `TYPE_REF`
  record 和本地 `TYPE_DEF` record。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 16/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 16/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3k-metadata-runtime-generic-typespec-base-token-binding.md`。
  备注：本记录只关闭 TypeSpec generic base-token binding view；generic argument semantic binding、
  MethodSpec runtime binding、token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 15:34:40 +08:00 · 11-S3J metadata runtime generic TypeSpec signature view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeTypeSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView()`，在 `TYPE_SPEC` token 上暴露 paired
  signature token/hash、validated signature blob、root `GENERIC_INST` node、base type node、
  argument count 和 argument-list blob offset。
  RED/GREEN：RED 为 metadata runtime query 新增 generic TypeSpec signature view 用例后编译失败，缺少
  view 类型与 `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView()`；GREEN 后空/非法输入拒绝，
  `GENERIC_INST(TYPE_REF, INT64)` TypeSpec 签名 view 通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 14/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 14/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3j-metadata-runtime-generic-typespec-signature-view.md`。
  备注：本记录只关闭 TypeSpec generic signature 的只读身份/结构 view；generic semantic binding、
  token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 15:18:58 +08:00 · 11-S3I metadata runtime signature type-node view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeSignatureTypeNodeView` 与
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`，在 validated signature blob slice 上按 blob offset 暴露
  type-node 的 node kind、payload、base type offset、child count、child list offset 与 next blob offset。
  RED/GREEN：RED 为 metadata runtime query 新增 signature type-node view 用例后编译失败，缺少 view 类型与
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`；GREEN 后越界/空参数拒绝、method return/parameter primitive
  node，以及 TypeSpec `GENERIC_INST` base/argument 子节点 view 通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 13/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 13/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3i-metadata-runtime-signature-type-node-view.md`。
  备注：本记录只关闭 signature type-node 只读结构 view；TypeSpec/generic 语义绑定、
  token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 15:03:19 +08:00 · 11-S3H metadata runtime method/field signature header view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `SZrMetadataRuntimeSignatureView` 与
  `ZrCore_MetadataRuntime_ReadSignatureView()`，在 validated signature blob slice 上读取 method/field 顶层
  signature header、计数和内部 blob 偏移。
  RED/GREEN：RED 为 metadata runtime query 新增 method/field signature view 用例后编译失败，缺少 view 类型与
  `ZrCore_MetadataRuntime_ReadSignatureView()`；GREEN 后未 attached runtime 拒绝、method header view、
  field header view 和 return/parameter/field type blob offset 通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 12/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 12/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3h-metadata-runtime-signature-header-view.md`。
  备注：本记录只关闭 method/field signature 顶层 header view；nested type-node AST、TypeSpec/generic 语义绑定、
  token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 14:51:27 +08:00 · 11-S3G metadata runtime validated signature blob view ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增 `ZrCore_MetadataRuntime_GetSignatureBlob()`，把 entity token 的 paired
  `SIGNATURE` record 解析结果连接到 attached zrp metadata 的 signature blob pool slice，并复用既有结构校验。
  RED/GREEN：RED 为 metadata runtime query 新增 validated signature blob view 用例后链接失败，缺少
  `ZrCore_MetadataRuntime_GetSignatureBlob()`；GREEN 后未 attached runtime 拒绝、method signature blob view、
  payload 指针/长度匹配和截断 signature 拒绝通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 11/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 11/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3g-metadata-runtime-signature-blob-view.md`。
  备注：本记录只关闭 validated signature blob view 查询；signature AST/semantic parsing、TypeSpec/generic 语义绑定、
  token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 14:40:17 +08:00 · 11-S3F metadata runtime zrp metadata mmap attach ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`SZrMetadataRuntime` 新增 zrp data metadata buffer/header 载体，并提供
  `ZrCore_MetadataRuntime_AttachZrpMetadata()` 与 `ZrCore_MetadataRuntime_GetZrpSectionView()`；runtime 会先校验
  header/definition tables，再暴露只读 section view。
  RED/GREEN：RED 为 metadata runtime query 新增 zrp metadata mmap attach/view 用例后编译/链接失败，缺少
  runtime zrp metadata 字段和 attach/view API；GREEN 后 header validation、buffer/header 挂载、typeDefs
  section view 和空 runtime 拒绝通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 10/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 10/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3f-metadata-runtime-zrp-mmap-attach.md`。
  备注：本记录只关闭 data metadata mmap attach 与 raw section view 查询；row 语义解析、signature blob
  semantic parsing、token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 14:24:52 +08:00 · 11-S3E metadata runtime field record lazy cache ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`SZrMetadataRuntime` 新增 field record 单项缓存，并提供
  `ZrCore_MetadataRuntime_ResolveFieldRecord()`；字段 token 复用 `MEMBER_DEF` / `MEMBER_REF` record，
  resolver 从 attached metadata function 本地或 module metadata ref 表 lazy 解析字段 record，且 field cache
  独立于 method cache。
  RED/GREEN：RED 为 metadata runtime query 新增 field lazy/cache 用例后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveFieldRecord()`；GREEN 后 local/imported field record、非法 token 拒绝和独立缓存命中通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 9/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 9/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3e-metadata-runtime-field-record-cache.md`。
  备注：本记录只关闭 field token record 的 lazy/cache 查询；FIELD_SIG 解析、method/field 语义区分、
  data metadata mmap、token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 14:10:48 +08:00 · 11-S3D metadata runtime TypeSpec type record cache ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`ZrCore_MetadataRuntime_ResolveTypeRecord()` 现在接受本地 `TYPE_SPEC`
  token，从 attached metadata function 的 token record 表 lazy 解析 TypeSpec record，并复用 type record
  单项缓存覆盖二次命中。
  RED/GREEN：RED 为 metadata runtime query 新增 TypeSpec lazy/cache 用例后返回 null；GREEN 后 local
  TypeSpec record 和缓存命中通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 8/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 8/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3d-metadata-runtime-typespec-record-cache.md`。
  备注：本记录只关闭 TypeSpec token record 的 lazy/cache 查询；field resolve、data metadata mmap、
  signature blob 语义解析、TypeSpec/generic 语义解析、token→运行期实体物化、layout 三向表和 metadata policy
  仍未关闭。

- 2026-06-25 14:02:23 +08:00 · 11-S3C metadata runtime signature record lazy cache ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`SZrMetadataRuntime` 新增 signature record 单项缓存，并提供
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()`；按 entity token 先从 attached metadata function 的本地
  signature record 查询，再从 module metadata ref 表查询；空 runtime、空 token 和 SIGNATURE token 被拒绝，
  二次查询命中缓存。
  RED/GREEN：RED 为 metadata runtime query 新增 signature lazy/cache 用例后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()`；GREEN 后 local/imported signature record、非法 token 拒绝和缓存命中通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 7/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 7/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3c-metadata-runtime-signature-record-cache.md`。
  备注：本记录只关闭 entity token → SIGNATURE record 的 lazy/cache 查询；field resolve、TypeSpec、
  data metadata mmap、signature blob 语义解析、token→运行期实体物化、layout 三向表和 metadata policy
  仍未关闭。

- 2026-06-25 13:52:15 +08:00 · 11-S3B metadata runtime type record lazy cache ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`SZrMetadataRuntime` 新增 type record 单项缓存，并提供
  `ZrCore_MetadataRuntime_ResolveTypeRecord()`；`TYPE_DEF` 从 attached metadata function 的本地
  token record 表 lazy 解析，`TYPE_REF` 从 module metadata ref 表 lazy 解析；空 runtime、空 token 和非
  type token 被拒绝，二次查询命中缓存。
  RED/GREEN：RED 为 metadata runtime query 新增 type lazy/cache 用例后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveTypeRecord()`；GREEN 后 local/imported type record、非法 token 拒绝和缓存命中通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 6/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 6/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3b-metadata-runtime-type-record-cache.md`。
  备注：本记录只关闭 type token record 层 lazy/cache 查询；field resolve、TypeSpec、data metadata mmap、
  token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 13:40:40 +08:00 · 11-S3A metadata runtime method record lazy cache ·
  状态：11-S3 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：`SZrMetadataRuntime` 新增 method record 单项缓存，并提供
  `ZrCore_MetadataRuntime_ResolveMethodRecord()`；`MEMBER_DEF` 从 attached metadata function 的本地
  token record 表 lazy 解析，`MEMBER_REF` 从 module metadata ref 表 lazy 解析；空 runtime、空 token 和非
  method token 被拒绝，二次查询命中缓存。
  RED/GREEN：RED 为 metadata runtime query 新增 lazy/cache 用例后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveMethodRecord()`；GREEN 后 local/imported method record、非法 token 拒绝和缓存命中通过。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 5/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 5/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3a-metadata-runtime-method-record-cache.md`。
  备注：本记录只关闭 method token record 层 lazy/cache 查询；type/field resolve、data metadata mmap、
  token→运行期实体物化、layout 三向表和 metadata policy 仍未关闭。

- 2026-06-25 13:19:32 +08:00 · 11-S2C module metadata runtime registration carrier ·
  状态：11-S2 子切片完成、11 阶段继续进行中；完整 11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：新增最小 `SZrMetadataRuntime` carrier；`SZrObjectModule` 提供
  attach/query；AOT 模块加载时把 module、metadata function、`codeRegistration` 与表计数登记到
  metadata runtime；GC mark/rewrite 维护 metadata runtime 内的 `metadataFunction`。
  RED/GREEN：RED 为 metadata runtime query 缺少 `metadata_runtime.h` 与 attach/query API；
  第二个 RED 为 frame setup source contract 缺少 AOT loader 的
  `ZrCore_Module_AttachMetadataRuntime(record.module, record.moduleFunction, record.codeRegistration)`。
  GREEN 后模块 attach/query、loader attach、失败诊断和 GC 引用维护通过 focused 覆盖。
  测试结果：WSL gcc/clang 均通过 metadata runtime query 4/0、frame setup 1/0、source contracts 19/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过 metadata runtime query 4/0、
  frame setup 1/0、source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s2c-metadata-runtime-registration.md`。
  备注：本记录只关闭 module-load metadata runtime registration carrier；data metadata mmap attach、
  token lazy resolve/cache、token↔layout 三向表和默认最小 metadata 策略仍未关闭。

- 2026-06-25 12:58:28 +08:00 · 11-S2B runtime code registration context carrier ·
  状态：11-S2 子切片完成、11 阶段继续进行中；完整 11-S2/11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：loaded-module record、generated module context 与 generated frame 均携带
  `codeRegistration`；MethodInfo 解析、callable 常量物化、native direct call、meta call 和 static direct call
  现在经 `record->codeRegistration->methodInfos/functionPointers` 消费注册表；generated C frame setup 将
  `zr_aot_context.codeRegistration` 传入 frame，保留旧 `functionThunks` 字段作为兼容视图。
  RED/GREEN：RED 为 frame setup contract 缺少 runtime header/context/frame 的
  `SZrAotCodeRegistration` 载体、runtime source 的 `record->codeRegistration` 消费文本，以及
  generated frame 的 `frame.codeRegistration` 赋值；第二个 RED 暴露记录绑定仍是栈值赋值而非 record 指针语义。
  GREEN 后运行时记录、上下文、生成帧和直接调用入口统一从 code registration carrier 读取。
  测试结果：WSL gcc/clang 均通过 frame setup 1/0、source contracts 19/0、shared-library smoke 8/0、
  descriptor diagnostics 2/0；Windows MSVC Debug 通过 frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s2b-runtime-registration-context-carrier.md`。
  备注：本记录只关闭运行时 record/context/frame 对 code registration 的消费载体；不关闭模块加载登记、
  `SZrMetadataRuntime` token lazy resolve、token↔layout 三向表或完整反射/泛型/GC 统一查询。

- 2026-06-25 12:41:09 +08:00 · 11-S2A generated-C code registration carrier ·
  状态：11-S2 子切片完成、11 阶段继续进行中；完整 11-S2/11-S3/11-S4/11-S5/11-S6/11-S7
  仍未关闭 · 完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 8u`；新增
  `SZrAotCodeRegistration` 和 `ZrAotCompiledModule.codeRegistration`；generated C 发射
  `zr_aot_reflection_invokers[]` 与 `zr_aot_code_registration`，将 function/method/invoker/GC
  descriptor 表汇入计划 §2 的代码注册表 carrier；runtime descriptor validation 会在解引用前拒绝
  缺失/不一致的 code registration 与缺失 invoker 表。
  RED/GREEN：RED 为 shared-library smoke 新增 `module->codeRegistration` 断言后编译失败，
  frame/source 契约缺少 ABI v8 和注册表发射文本；补强 RED 为 descriptor diagnostics 构造
  `codeRegistration = ZR_NULL` 的 ABI v8 模块时旧 runtime 校验前崩溃；GREEN 后 descriptor 暴露的
  注册表与旧模块字段指向同一套表，缺失/不一致注册表会被诊断拒绝。
  测试结果：WSL gcc/clang 均通过 frame setup 1/0、source contracts 19/0、shared-library smoke 8/0、
  descriptor diagnostics 2/0；Windows MSVC Debug 通过 frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s2a-code-registration-carrier.md`。
  备注：本记录只关闭 generated-C 注册表载体与 descriptor-safety validation；不关闭模块加载登记、`SZrMetadataRuntime` token
  lazy resolve、token↔layout 三向表或完整反射/泛型/GC 统一查询。

- 2026-06-25 12:05:39 +08:00 · 10-S2A MethodInfo reflection invoker carrier ·
  状态：10-S2 子切片完成、10 阶段继续进行中；完整 10-S2/10-S3/10-S4/10-S5 仍未完成 ·
  完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 7u`，新增 `FZrAotReflectionInvoker`
  和 `SZrAotMethodInfo.invoker`；AOT C 生成物发射共享 `zr_aot_invoker_entry_thunk`，
  当前按统一 `FZrAotEntryThunk(SZrState *)` 签名分桶，并把每个 generated MethodInfo 的
  `.invoker` 登记到该桶。
  RED/GREEN：RED 为 shared-library smoke 新增 `methodInfos[0]->invoker` 断言后编译失败，
  frame setup 源契约也缺少 invoker ABI 文本；GREEN 后源契约和 descriptor runtime assertion 均通过。
  测试结果：WSL gcc/clang 均通过 focused 组：`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0、`zr_vm_aot_c_return_contracts_test` 1/0；
  Windows MSVC Debug 构建通过，frame setup 1/0、source contracts 19/0、return contracts 1/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支）、descriptor diagnostics 1/0（1 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-10-s2a-reflection-invoker-carrier.md`。
  备注：本记录只关闭 invoker ABI carrier 与 generated-C 登记点；不关闭完整 `Method.Invoke`
  参数解包/返回打包、token 解析、typed-target 分桶、字段/generic DESCRIPTION 元数据或注解裁剪。

- 2026-06-25 11:35:46 +08:00 · 09-S4C new-owner write barrier elimination ·
  状态：09-S4 完成、09 阶段计划切片完成 · 完成项目：AOT C 函数体新增保守线性证明，在 receiver slot
  可追溯为本函数内新建 object/array 且未跨调用、控制流、返回或不明逃逸时，member/member-slot/index/super-array
  写入改发 `ZrLibrary_AotRuntime_*NewOwnerNoWriteBarrier`；core object 层新增 assume-new-owner no-barrier setter，
  通过 `skipWriteBarrier` 候选和 target object `YOUNG_MOVABLE` 运行时复核，只在该证明成立且 owner 仍为 young
  的路径跳过 09-S4B 公共写屏障，不能证明或 owner 已晋升时继续走
  `ZrCore_Gc_WriteBarrier`。RED/GREEN：RED 为 global source contract 找不到 no-barrier writer/API/runtime
  helper；第二个 RED 为 guardrail 拒绝新的 no-barrier runtime boundary；补强 RED 要求 core no-barrier API 出现
  young-owner 运行时确认。GREEN 后 source contract 锁定 writer、runtime helper、core object no-barrier route、
  young-owner guard 与 super-array fallback，guardrail 明确允许四个新边界。测试结果：
  WSL gcc direct global contracts 9/0、guardrail 6/0、global smoke 10/0、root-frame 5/0、constant contracts 5/0、
  super-array contracts 1/0、super-array smoke 1/0；WSL clang direct 同组 9/0、6/0、10/0、5/0、5/0、1/0、1/0；
  Windows MSVC Debug direct 同组 9/0、6/0、global smoke 10/0（10 ignored）、root-frame 5/0、constant contracts 5/0、
  super-array contracts 1/0、super-array smoke 1/0（1 ignored）。产出：
  `tests/acceptance/2026-06-25-aot-09-s4c-new-owner-write-barrier-elision.md`。备注：`LOCAL_ADDRESS` roots、
  长时间 GC 压测、card-table 优化与 pinned-region demotion 仍为扩展项；本记录只关闭 09 计划切片。

- 2026-06-25 10:56:08 +08:00 · 09-S5B FFI native-call pin scope ·
  状态：09-S5 完成、09 阶段继续进行中，09-S4C 编译期写屏障消除仍未关闭 · 完成项目：核心新增
  `SZrGcNativeCallPin` 与 `ZrCore_Gc_NativeCallPinObject/Value/Unpin`；libffi symbol call 在 native invoke
  前 pin self/owner/argument GC values，cleanup 中反向 unpin；callback trampoline 用 `SZrFunctionStackAnchor`
  在 native 回调后重新定位保存的 `stackTop`，避免 GC/栈重分配后误判 call stack corruption；新增
  `zr_vm_ffi_native_call_pin_contract_test` 锁定 pin/unpin source contract。RED/GREEN：RED 为 root-frame
  pin contract 缺少类型/API 编译失败，随后暴露 pin/ignore 顺序问题；完整 FFI path 还暴露 callback 裸
  `stackTop` 对比不可靠，GREEN 后 pin/ignore 保留调用前状态且 callback 使用 stack anchor。测试结果：
  WSL gcc direct root-frame 5/0、FFI native-call pin contract 2/0；WSL clang direct 同组 5/0、2/0；
  Windows MSVC Debug direct 同组 5/0、2/0。产出：
  `tests/acceptance/2026-06-25-aot-09-s5b-ffi-native-call-pin.md`。备注：当前 unpin 清理临时 native pin
  flag 与 ignore root，不做 pinned region demotion；完整 `zr_vm_ffi_test` 相关早期 symbol/callback 路径通过，
  后段 source-extern `pointer<T>` 未限定类型解析失败属于既有基线，未并入本切片。

- 2026-06-25 10:06:27 +08:00 · 09-S5A public boxing/unboxing bridge boundary ·
  状态：09-S5 子切片完成、完整 09-S5 继续进行中 · 完成项目：核心新增
  `ZrCore_Bridge_BoxTyped` / `ZrCore_Bridge_UnboxTyped` 公共桥入口，并保持内部委托既有
  `ZrCore_Execution_ToObject` / `ZrCore_Execution_ToStruct`；AOT runtime 的 `TO_OBJECT` /
  `TO_STRUCT` 边界改走 bridge API，生成 C 继续只在 typed↔dynamic 边界触发装箱/拆箱 runtime helper；
  镜像 AOT runtime 同步迁移。RED/GREEN：RED 为 global source contract 读取不到 bridge.h/c 且 runtime
  仍直接调用 execution conversion；GREEN 后 source contract、guardrail 和 shared-library smoke 都确认
  bridge API 是允许的显式 runtime boundary。测试结果：WSL gcc direct global contracts 8/0、global
  smoke 10/0、guardrail 6/0、value-type smoke 2/0、constant contracts 5/0、root/barrier 4/0；
  WSL clang direct 同组 8/0、10/0、6/0、2/0、5/0、4/0；Windows MSVC Debug direct 同组
  8/0、10/0（10 ignored）、6/0、2/0（1 ignored）、5/0、4/0。产出：
  `tests/acceptance/2026-06-25-aot-09-s5a-boxing-bridge-boundary.md`。备注：FFI pin/unpin 仍留给
  09-S5 后续；本切片不改变装箱对象布局或现有值类型分配策略。

- 2026-06-25 09:44:28 +08:00 · 09-S4B member/index heap-store public write barrier boundary ·
  状态：09-S4 子切片完成、完整 09-S4 继续进行中 · 完成项目：generated-C member/index/super-array 写入仍走
  AOT runtime boundary，但实际 object heap-store 落点已从内部 `ZrCore_Value_Barrier(state, ...)` 收束到
  `ZrCore_Gc_WriteBarrier(state, ownerObject, value)`；AOT runtime 镜像副本同步迁移旧 closure/capture barrier；
  stack inline struct store 仍由 value-type smoke 锁定为无屏障。RED/GREEN：RED 为 global source contract 缺少
  object heap-store 公共 GC write barrier 并看到旧 object value barrier；GREEN 后 source contract、member/index
  smoke、super-array smoke 和 core old-to-young wrapper 语义均通过。测试结果：WSL gcc direct 4/0、8/0、10/0、
  1/0、1/0、2/0、5/0；WSL clang direct 同组 4/0、8/0、10/0、1/0、1/0、2/0、5/0；Windows MSVC Debug direct
  同组 4/0、8/0、10/0（10 ignored）、1/0、1/0（1 ignored）、2/0（1 ignored）、5/0。备注：新分配 owner
  的编译期屏障消除仍留给 09-S4C；`object.c` 只替换既有屏障入口，未新增职责。

- 2026-06-25 09:22:37 +08:00 · 09-S4A public AOT write barrier API + closure heap-owner boundary ·
  状态：09-S4 子切片完成、完整 09-S4 继续进行中 · 完成项目：核心 GC 新增
  `ZrCore_Gc_WriteBarrier(state, ownerObject, value)`，AOT runtime 中闭包/capture heap-owner
  写入边界全部改走这个白名单 API；guardrail 允许 `ZrCore_Gc_SafePoint` 与 `ZrCore_Gc_WriteBarrier`；
  value-type generated-C smoke 明确栈上 inline struct 字段 store 不生成多余写屏障。
  RED/GREEN：RED 为 source contract 找不到 `ZrCore_Gc_WriteBarrier` API 且 `aot_runtime.c` 仍直接调用
  `ZrCore_Value_Barrier(state, ...)`；GREEN 后核心写屏障 API 记录 old-to-young remembered-set，
  AOT runtime source 不再出现旧 value barrier。测试结果：WSL gcc direct root/barrier 4/0、constant
  contracts 5/0、guardrail 6/0、value-type smoke 2/0、global smoke 10/0；WSL clang direct 同组
  4/0、5/0、6/0、2/0、10/0；Windows MSVC Debug direct root/barrier 4/0、constant contracts 5/0、
  guardrail 6/0、value-type smoke 2/0（1 ignored）、global smoke 10/0（10 ignored）。产出：
  `tests/acceptance/2026-06-25-aot-09-s4a-write-barrier-api.md`。备注：member/index direct
  generated-C 屏障和新生代 owner 编译期消除仍留给 09-S4B。

- 2026-06-25 08:53:00 +08:00 · 09-S3A safepoint API + allocation/call/back-edge insertion ·
  状态：09-S3 完成、09 阶段继续进行中；09-S4 写屏障和 09-S5 装箱/FFI pin 仍未完成 ·
  完成项目：核心 GC 新增 `ZrCore_Gc_SafePoint(state)`，AOT C emitter 现在能写出带 marker 的
  `ZrCore_Gc_SafePoint(state);`；generated C 在 object/array 分配后、所有函数调用 lowering 后、
  以及只对 `targetInstructionIndex <= instructionIndex` 的循环/回边 `goto` 前插入 safepoint。
  RED/GREEN：RED 为缺少 safepoint API 与 emitter/source marker；GREEN 后 runtime pending GC debt
  可被 safepoint 推进，source contract 和 generated-C safepoint smoke 均通过。测试结果：WSL gcc
  direct root-frame 3/0、control contracts 2/0、value-type smoke 2/0、iterator contracts 1/0、
  control shared-library smoke 2/0，CTest `aot_gc_root_frame` 1/1；WSL clang direct root-frame 3/0、
  control contracts 2/0、control shared-library smoke 2/0、value-type smoke 2/0，CTest
  `aot_gc_root_frame` 1/1；Windows MSVC Debug direct root-frame 3/0、control contracts 2/0、
  control shared-library smoke 2/0（Unix branch ignored）、value-type smoke 2/0（Unix branch ignored），
  CTest `aot_gc_root_frame` 1/1。产出：
  `tests/acceptance/2026-06-25-aot-09-s3a-safepoint-insertion.md`。备注：`zr_vm_aot_c_logical_contracts_test`
  的 2 个失败已调查为既有过时 scalar-local source-contract 断言，与 09-S3A safepoint 插入无关；
  `backend_aot_c_function_body.c` 暂未拆分，后续宜按 call/branch dispatch 边界处理。

- 2026-06-25 08:14:05 +08:00 · 09-S2B runtime AOT root-frame stack + generated-C push/pop ·
  状态：09-S2 默认 frame-byte-offset 根路径完成；09 阶段继续进行中 · 完成项目：核心 state
  现在维护 AOT root-frame stack，`ZrCore_Gc_AotRootFramePush/Pop/Depth` 登记 `(frame base, root map)`；
  GC mark/rewrite 路径会按 `SZrAotGcRootMap` 的 frame-byte-offset slots 标记并重写 AOT byte-frame
  中的 `SZrTypeValue` 根；AOT runtime generated context 绑定 `SZrAotMethodInfo`，generated C 仅对
  非空 `gcRootMap` 方法生成 prologue push / epilogue pop，POD/blittable 继续不生成 root-frame 调用。
  RED/GREEN：RED 为新增 root-frame 测试缺少 carrier/API 编译失败；GREEN 后 root-stack 平衡和
  minor-GC 保活/重写通过，frame setup 合同与 value-type generated-C smoke 通过。测试结果：WSL gcc
  direct root-frame 2/0、frame setup 1/0、value-type smoke 2/0，CTest `aot_gc_root_frame` 1/1，
  full `zr_vm_gc_test` 66/0；WSL clang direct root-frame 2/0、frame setup 1/0、value-type smoke 2/0，
  CTest `aot_gc_root_frame` 1/1；Windows MSVC Debug direct root-frame 2/0、frame setup 1/0、
  value-type smoke 2/0（Unix shared-library execution branch 1 ignored），CTest `aot_gc_root_frame`
  1/1。产出：`tests/acceptance/2026-06-25-aot-09-s2b-runtime-root-frame-stack.md`。
  备注：`LOCAL_ADDRESS` 根位置、安全点、写屏障、装箱和 FFI pin 仍属后续 09 切片。

- 2026-06-25 07:10:49 +08:00 · 09-S2A AOT GC root map descriptor ABI + generated-C publication ·
  状态：09-S2 子切片完成、09 阶段继续进行中；完整 09-S2 根栈登记/移动更新、09-S3 safepoint、
  09-S4 写屏障、09-S5 装箱/FFI pin 仍未完成 · 完成项目：公共 AOT ABI 新增 root-location kind、
  `SZrAotGcRootSlot` 和 `SZrAotGcRootMap`；generated C 现在为含 GC 字段的 inline-struct frame slots
  发布 `zr_aot_gc_root_slots_<flatIndex>[]` / `zr_aot_gc_root_map_<flatIndex>`，并经
  `SZrAotMethodInfo.gcRootMap` 暴露；POD/blittable struct 继续保持 `gcRootMap = ZR_NULL`。
  RED/GREEN：RED 为新增 value-type smoke 编译生成 C 时缺少 root-map ABI 类型/枚举；GREEN 后
  `ZrVm_GetAotCompiledModule()` 可读取 method info root map，验证 root slots 使用 frame-byte-offset
  定位并绑定到 string-field struct 的 GC descriptor，POD 生成物不发布 root map。测试结果：WSL gcc
  直接 frame setup 1/0、value-type smoke 2/0，相关 CTest 4/4；WSL clang 直接同两目标 1/0、2/0，
  相关 CTest 4/4；Windows MSVC Debug 直接 frame setup 1/0、value-type smoke 2/0（Unix shared-library
  execution 分支 1 ignored），相关 CTest 3/3。产出：
  `tests/acceptance/2026-06-25-aot-09-s2a-gc-root-map-descriptor.md`。备注：本记录只关闭
  root-map ABI 与 generated-C method metadata 发布，不声明 runtime 根栈 push/pop、safepoint 或移动后根重写完成。

- 2026-06-25 06:26:16 +08:00 · 11-S7V / 12-S3F / 12-S4N / 08-S7K manifest generic MethodSpec binding ·
  状态：11-S7、12-S3、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：manifest generic preserve root 的 target 现在可以绑定 current-module typed exported method；
  CLI AOT preserve bridge 扫描 `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 形态签名，
  将 method-spec token、method token 和 instantiation signature hash 写入 writer root；generated C manifest
  诊断输出 MethodSpec identity，full-AOT gate 接受 MethodSpec-bound generic method root。
  RED/GREEN：RED 为新增 `Factory.make<Foo>` 用例引用缺失 MethodSpec root 字段后编译失败；GREEN 后
  method-spec token 为 `0x08000002`，method token 为 `0x03000001`，签名 hash 为 `0x2233445566778899`。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 14/0；WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7v-12-s3f-manifest-generic-methodspec-binding.md`。
  备注：本记录不关闭跨模块泛型目标、annotation roots、泛型方法代码体传递闭包、默认最小 metadata 策略、
  zrp MethodSpec table 持久导出或完整 mark-and-sweep generic closure。

- 2026-06-25 06:03:45 +08:00 · 11-S7U / 12-S3E / 12-S4M / 08-S7J manifest generic synthesized TypeSpec binding ·
  状态：11-S7、12-S3、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：manifest generic preserve root 没有现成 `TYPE_SPEC` 时，CLI AOT preserve bridge 现在可从当前函数
  metadata 中同名 open `TYPE_DEF` 或 `TYPE_REF` record 合成 writer-visible `TYPE_SPEC` / paired `SIGNATURE`
  binding，并继续物化 generic instantiation identity；full-AOT writer gate 可接受这个已绑定且已物化的
  current-module generic root。
  RED/GREEN：RED 为新增 full-AOT `List<Foo>` 用例仅有 open `TYPE_REF(List)` metadata 时
  `hasTypeSpecBinding` 仍为 false；GREEN 后得到 synthesized TypeSpec token `0x07000001`、
  signature token `0x08000002`、open base token `0x05000001` 和 generic instance id `1`。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 13/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7u-12-s3e-manifest-generic-synthesized-typespec.md`。
  备注：本记录不关闭 MethodSpec、跨模块泛型目标、annotation roots、反射动态实例、默认最小 metadata 策略、
  `.zro` metadata 持久化重排或完整 mark-and-sweep generic closure。

- 2026-06-25 05:41:31 +08:00 · 11-S7T / 12-S3D / 12-S4L / 08-S7I current-module TypeDef generic base token ·
  状态：11-S7、12-S3、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：TypeSpec-bound manifest generic preserve root 物化为 generic instantiation identity 时，
  现在可识别 `GENERIC_INST(TYPE_DEF target, args...)` 签名；若当前函数 metadata records 存在同名
  current-module `TYPE_DEF` 签名，base token 使用 open `TYPE_DEF` token，并继续保留 TypeRef open-base
  与 TypeSpec fallback 路径。generated C manifest 诊断可输出 `genericInstance.baseToken = 0x02000001`。
  RED/GREEN：RED 为新增 `TYPE_DEF(List)` + `TYPE_SPEC(List<Foo>)` writer-options 用例无法建立
  TypeSpec binding；GREEN 后该用例绑定成功并报告 `0x02000001` base token。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 12/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7t-12-s3d-generic-instantiation-typedef-base-token.md`。
  备注：本记录不关闭 MethodSpec、缺失 TypeSpec 合成、跨模块泛型目标、annotation roots、
  反射动态实例或完整 mark-and-sweep generic closure。

- 2026-06-25 05:28:38 +08:00 · 11-S7S / 12-S3C / 12-S4K / 08-S7H generic instantiation open base token ·
  状态：11-S7、12-S3、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：TypeSpec-bound manifest generic preserve root 物化为 generic instantiation identity 时，
  若当前函数 metadata records 存在同名 `TYPE_REF` 签名，base token 现在使用 open generic `TYPE_REF`
  token；没有匹配 TypeRef 时保留 closed `TYPE_SPEC` fallback。generated C manifest 诊断可输出
  `genericInstance.baseToken = 0x05000001`。
  RED/GREEN：RED 为新增 `TYPE_REF(List)` + `TYPE_SPEC(List<Foo>)` writer-options 用例仍得到
  `0x07000001`；GREEN 后得到 `0x05000001`，旧 TypeSpec fallback 和 full-AOT generic gate 均保持通过。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 11/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7s-12-s3c-generic-instantiation-open-base-token.md`。
  备注：本记录不关闭 MethodSpec、缺失 TypeSpec 合成、跨模块泛型目标、annotation roots、
  反射动态实例或完整 mark-and-sweep generic closure。

- 2026-06-25 05:08:49 +08:00 · 11-S7R / 12-S8I / 12-S3B / 08-S7G full-AOT generic instantiation closure gate ·
  状态：11-S7、12-S8、12-S3 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：AOT C writer 的 full-AOT manifest generic root 预检从 TypeSpec gate 收紧为
  TypeSpec + generic-instantiation identity gate；TypeSpec/signature token/hash 存在但没有
  `hasGenericInstantiationBinding` 的 root 会被拒绝，避免 TypeSpec-only root 被视为完整泛型闭包。
  RED/GREEN：RED 为 direct writer-options 测试构造 TypeSpec-only generic root 后 writer 仍返回 true；
  GREEN 后该路径返回 false，既有 `List<Foo>` TypeSpec-backed generic instance materialization 和未绑定 TypeSpec gate 继续通过。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 10/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7r-12-s8i-full-aot-generic-instantiation-closure-gate.md`。
  备注：本记录不关闭 MethodSpec、缺失 TypeSpec 合成、跨模块泛型目标、annotation roots、
  反射动态实例或完整 mark-and-sweep generic closure。

- 2026-06-25 04:50:01 +08:00 · 11-S7Q / 12-S3A / 12-S4J / 08-S7F manifest generic TypeSpec-backed instantiation root ·
  状态：11-S7、12-S3、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：已绑定 `TYPE_SPEC` 的 manifest generic preserve root 现在会通过 `SZrGenericInstantiationTable`
  物化为 writer 可见的 generic instantiation identity，并在 generated C manifest 诊断输出
  `genericInstance.baseToken`、`genericInstance.id`、`genericInstance.shareKind`。
  RED/GREEN：RED 为 CLI writer options 测试引用缺失 generic-instantiation fields 后编译失败；GREEN 后
  `List<Foo>` root 绑定为 TypeSpec-backed shared-reference instance id 1。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 9/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7q-12-s3a-manifest-generic-preserve-instantiation-root.md`。
  备注：本记录不关闭 MethodSpec、缺失 TypeSpec 合成、跨模块泛型目标、传递 generic closure、反射动态实例或默认最小 metadata 策略。

- 2026-06-25 04:14:31 +08:00 · 11-S7P / 12-S8H / 08-S7E full-AOT manifest generic TypeSpec closure gate ·
  状态：11-S7、12-S8 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：AOT C writer 在 full-AOT 模式下会拒绝未绑定 `TYPE_SPEC` 的 manifest generic preserve root，
  防止仅有文本 target/arguments 的 root 被视为完整 AOT 闭包；hybrid 模式仍保留清单诊断输出。
  RED/GREEN：RED 为 CLI writer options 新增 full-AOT 未绑定 `List<Foo>` generic preserve 用例后，writer 仍返回 true；
  GREEN 后该路径返回 false，既有 hybrid unbound root 和 bound TypeSpec root 用例保持通过。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 8/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7p-12-s8h-full-aot-generic-preserve-typespec-closure-gate.md`。
  备注：该记录只关闭 manifest generic preserve 的 full-AOT TypeSpec 门禁；MethodSpec、TypeSpec 合成、
  generic instantiation materialization、跨模块 generic target、注解 roots、默认最小 metadata 和完整 mark-and-sweep closure 仍未关闭。

- 2026-06-25 04:00:47 +08:00 · 11-S7O / 12-S4I / 08-S7D manifest generic preserve TypeSpec binding ·
  状态：11-S7、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：manifest generic preserve root 现在可在当前函数 metadata 中匹配已有
  `GENERIC_INST` `TYPE_SPEC` 签名，并把 TypeSpec token、paired signature token 和 signature hash
  写入 writer options 与 generated-C manifest 诊断。
  RED/GREEN：RED 为 CLI writer options 测试引用缺失 token/hash 字段后编译失败；GREEN 后
  `List<Foo>` 绑定到 `TYPE_SPEC` `0x07000001`、`SIGNATURE` `0x08000001` 和 hash `0x123456789abcdef0`。
  测试结果：WSL gcc `zr_vm_cli_aot_writer_options_test` 7/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5；`git diff --check` 仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7o-12-s4i-manifest-generic-preserve-typespec-binding.md`。
  备注：这是当前模块已有 TypeSpec 的窄绑定；MethodSpec、缺失 TypeSpec 合成、真实 generic instantiation roots、
  跨模块 generic target、注解 roots、默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 03:27:16 +08:00 · 11-S7N / 12-S4H / 08-S7C manifest generic preserve writer roots ·
  状态：11-S7、12-S4 与 08-S7 子切片完成；08/11/12 阶段继续进行中 ·
  完成项目：`SZrAotWriterOptions` 新增 manifest generic root carrier；CLI preserve root bridge 将
  `.zrp` `kind: "generic"` 的 target 和 `arguments` 注入 writer options；generated C 文件头输出
  `manifest.genericRoots` 与逐参数清单。
  RED/GREEN：RED 为 CLI writer options 测试引用缺失 generic root fields 后编译失败；GREEN 后
  `List<Foo, Bar.Baz>` 进入 writer options，生成物包含对应 manifest generic root 记录。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 的 `zr_vm_cli_aot_writer_options_test` 均为 6/0；
  三个环境的 CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed` 均为 3/3；
  schema JSON 解析通过，`git diff --check` 仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7n-12-s4h-manifest-generic-preserve-writer-roots.md`。
  备注：这只是 generic preserve 到 writer/generated-C 清单的 bridge；MethodSpec/TypeSpec token binding、
  真实 generic instantiation roots、跨模块 target、注解 roots、默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 03:02:14 +08:00 · 11-S7M / 12-S4G generic preserve argument model ·
  状态：11-S7 与 12-S4 子切片完成；11/12 阶段继续进行中 ·
  完成项目：`.zrp` `preserve` 的 `kind: "generic"` 新增非空 `arguments` 数组，project model
  以 `genericArguments` / `genericArgumentCount` 承载类型实参，并在 parser/schema 中拒绝缺参数、
  空参数、非法参数、非数组参数和非 generic rule 携带参数。
  RED/GREEN：RED 为 manifest normalization 新增 generic argument 断言后缺 project model 字段编译失败；
  继续补无参数/空参数/非 generic rule 携带参数 RED，旧实现会接受；GREEN 后
  `List<Foo, Bar.Baz>` 解析成功，非法形态拒绝。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 的
  `zr_vm_project_manifest_normalization_test` 均为 25/0；schema JSON 解析通过；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 为 2/2；`git diff --check` 仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7m-12-s4g-generic-preserve-argument-model.md`。
  备注：这只是 generic preserve 参数声明模型；generic instantiation root binding、metadata token resolution、
  跨模块 target、注解 roots、默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 02:40:15 +08:00 · 11-S7L / 12-S4F feature switch preserve root gating ·
  状态：11-S7 与 12-S4 子切片完成；11/12 阶段继续进行中 ·
  完成项目：新增 `.zrp` top-level `features` boolean switch map，独立 `project_features` 解析模块，
  并让 CLI AOT preserve root 注入按 rule 的 `feature` / `featureValue` 选择是否执行。
  RED/GREEN：RED 为 manifest normalization 缺 feature switch model、CLI writer options 缺
  feature-conditioned root gating；GREEN 后 `EnableFastAot=true` 保留 `Widget.kept`，
  `EnableFastAot=false` 跳过相同 preserve rule 并让 generated C 裁剪 `zr_aot_fn_2`。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 的
  `zr_vm_project_manifest_normalization_test` 均为 19/0，`zr_vm_cli_aot_writer_options_test` 均为 5/0；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 为 2/2；schema JSON 解析通过；
  `git diff --check` 仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7l-12-s4f-feature-switch-preserve-root-gating.md`。
  备注：feature switch 目前只覆盖已支持的 method/type preserve writer roots；generic preserve、
  metadata token resolution、跨模块 target、注解 roots、默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 02:23:14 +08:00 · 11-S7K / 12-S4E preserve feature condition model ·
  状态：11-S7 与 12-S4 子切片完成；11/12 阶段继续进行中 ·
  完成项目：`.zrp` `preserve` rule model 新增 `feature`、`hasFeatureValue`、`featureValue`，
  parser/schema 接受成对的 safe dotted feature name + boolean expected value，并拒绝半声明。
  RED/GREEN：RED 为 manifest normalization 测试引用缺失 feature fields 后编译失败；GREEN 后
  `featureValue: true/false` 均可解析，缺 `feature` 或缺 `featureValue` 会拒绝 manifest。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 的
  `zr_vm_project_manifest_normalization_test` 均为 17/0；schema JSON 解析通过；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 为 2/2；`git diff --check` 仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7k-12-s4e-preserve-feature-condition-model.md`。
  备注：本切片只实现 feature 条件声明模型；feature 条件求值/构建配置接入、generic preserve、
  metadata token resolution、跨模块 target、注解 roots、默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 02:09:47 +08:00 · 11-S7J / 12-S4D dotted and type-member preserve roots ·
  状态：11-S7 与 12-S4 子切片完成；11/12 阶段继续进行中 ·
  完成项目：`method` preserve 支持完整 dotted callable 名精确匹配；`type` preserve 的
  `members: "methods"` / `"all"` 会扫描 entry function top-level callable bindings，并把 `<type>.`
  前缀下的方法全部注入 writer manifest roots。
  RED/GREEN：RED 为 dotted method 和 type-members 测试 root count 仍为 0；GREEN 后 `Widget.kept`
  解析到 flat index 2，`type Widget methods` 解析到 flat indices 1/2，generated C 保留全部 3 个函数。
  测试结果：WSL gcc、WSL clang、Windows MSVC Debug 的
  `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` CTest 均为 3/3。
  产出：`tests/acceptance/2026-06-25-aot-11-s7j-12-s4d-dotted-type-method-preserve-roots.md`。
  备注：generic preserve、metadata token resolution、跨模块 method target、注解 roots、feature switch、
  默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 01:53:48 +08:00 · 11-S7I / 12-S4C method preserve root binding ·
  状态：11-S7 与 12-S4 子切片完成；11/12 阶段继续进行中 ·
  完成项目：AOT writer 新增 top-level callable name 到 flat function index 的解析 API；
  CLI AOT C helper 新增 preserve root container，并把当前模块 `.zrp` `preserve` 中的 `method`
  target 绑定为 `SZrAotWriterOptions.manifestPreserveFunctionFlatIndices`。opt-in code stripping
  复用既有 `MANIFEST` roots，保留 manifest 指定的 top-level callable child。
  RED/GREEN：RED 为新增 `tests/cli/test_cli_aot_writer_options.c` 后缺失 resolver/helper 类型导致编译失败；
  GREEN 后 `main.kept` method preserve 在 generated C 中保留 `zr_aot_fn_2`，函数裁剪统计为 `3/3/0`。
  测试结果：WSL gcc `cli_args|cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` CTest 4/4；
  WSL clang `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` CTest 3/3；
  Windows MSVC Debug 同组 3/3。
  产出：`tests/acceptance/2026-06-25-aot-11-s7i-12-s4c-preserve-method-root-binding.md`。
  备注：本记录只关闭当前模块 method preserve 到 writer manifest roots 的绑定；type/generic preserve、
  metadata token resolution、跨模块 method target、feature switch、默认最小 metadata 和 dump/diff 工具仍未关闭。

- 2026-06-25 01:13:27 +08:00 · 11-S7H / 08-S7 / 12-S8G CLI AOT C emission entry ·
  状态：11-S7 子切片完成，08-S7/12-S8 CLI full-AOT writer 入口接线完成；11/12 阶段继续进行中 ·
  完成项目：CLI 新增 `--emit-aot-c`；project path resolver 新增 `bin/aot_c/src/<module>.c`
  输出路径并支持依赖包 binary root；incremental manifest 升级 v3 记录 `aot_c` 路径；project compiler
  用 `.zro` binary blob、`inputKind = ZR_AOT_INPUT_KIND_BINARY`、manifest 注入的 `requireFullAot`
  调用 AOT C writer，并把缺失 AOT C 输出纳入 dirty check。
  RED/GREEN：RED 为 CLI args 测试新增 `emitAotC` 断言后编译失败；GREEN 后 `--emit-aot-c` 解析/校验、
  AOT C path resolver、full-AOT project AOT C 输出、manifest v3 增量行为均通过。
  测试结果：WSL gcc `cli_args|cli_project_incremental` CTest 2/2；WSL clang 同组 2/2；
  Windows MSVC Debug 同组 2/2；Windows MSVC CLI `--compile --emit-aot-c --incremental` 删除
  `main.c` 后重新生成 `bin/aot_c/src/main.c`（114478 bytes）；`git diff --check` 退出 0，仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7h-cli-aot-c-emission-entry.md`。
  备注：本记录只关闭 CLI/project AOT C 发射入口；full-AOT 缺失实例诊断、manifest dynamic generic roots、
  reflection preservation、preserve writer roots、默认最小 metadata 策略和 dump/diff 工具仍未关闭。

- 2026-06-25 00:29:49 +08:00 · 11-S7G / 08-S7 / 12-S8 manifest full-AOT writer option bridge ·
  状态：11-S7 子切片完成、08-S7/12-S8 manifest policy 注入完成；11/12 阶段继续进行中 ·
  完成项目：新增 `ZrCli_Compiler_ApplyProjectAotWriterOptions()`，把 `SZrLibrary_Project.aotMode`
  映射到 `SZrAotWriterOptions.requireFullAot`；`full-aot` manifest 置 true，缺省 hybrid 置 false，
  并保持其他 writer option 字段不变。
  RED/GREEN：RED 为 CLI project incremental 测试调用缺失 helper 后链接失败；GREEN 后
  full-AOT 和 hybrid/default 两条 bridge 用例通过，CLI project incremental 测试 10/0。
  测试结果：WSL gcc `zr_vm_cli_project_incremental_test` 10/0；WSL clang 同目标 10/0；
  Windows MSVC Debug `zr_vm_cli_project_incremental_test` 10/0；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-25-aot-11-s7g-zrp-project-manifest-aot-mode-writer-injection.md`。
  备注：本记录只覆盖 manifest policy 到 writer option 的注入点；CLI AOT C 发射入口、
  full-AOT 完整闭合诊断、manifest 动态泛型实例、preserve writer roots、默认最小 metadata 策略和
  dump/diff 工具仍未关闭。

- 2026-06-25 00:08:34 +08:00 · 11-S7F / 08-S7 / 12-S8 zrp project manifest AOT mode parsing ·
  状态：11-S7 子切片完成、08-S7/12-S8 manifest full-AOT 前置解析完成；11/12 阶段继续进行中 ·
  完成项目：`.zrp` project loader 接受 top-level `aotMode` string，缺省 `hybrid`，显式
  `"full-aot"` 写入 `SZrLibrary_Project.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT`；
  非法类型或未知值拒绝 manifest。`zrp.schema.json` 同步声明 `hybrid` / `full-aot` enum。
  实现拆入 `project_aot_options.{h,c}`，避免继续扩大 `project.c`。
  RED/GREEN：RED 为新增 AOT mode 测试后 project model 缺失 `aotMode` 字段和 AOT mode enum 导致编译失败；
  GREEN 后缺省 hybrid、显式 full-AOT 和非法 mode 拒绝通过，manifest normalization 测试 14/0。
  测试结果：WSL gcc `zr_vm_project_manifest_normalization_test` 14/0、`zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 14/0、9/0；schema JSON 解析通过；Windows MSVC 两个 focused 测试分别 14/0、9/0；
  Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-25-aot-11-s7f-zrp-project-manifest-aot-mode.md`。
  备注：本记录只覆盖 manifest declaration parser，不关闭 CLI/compiler 到
  `SZrAotWriterOptions.requireFullAot` 的自动注入、full-AOT 闭合诊断、动态泛型实例保留、
  默认最小 metadata 策略或 dump/diff 工具。

- 2026-06-24 23:36:19 +08:00 · 11-S7E / 12-S4B zrp project manifest preserve rule parsing ·
  状态：11-S7 子切片完成、12-S4 manifest 前置解析桥接完成；11/12 阶段继续进行中 ·
  完成项目：`.zrp` project loader 接受 top-level `preserve` array，支持 `type`、`method`、`generic`
  三类 declaration target 和 optional `members`（`all`/`methods`），并把结果暴露为
  `SZrLibrary_Project.preserveRules` / `preserveRuleCount`。非法 target 形态会拒绝 manifest；
  `zrp.schema.json` 同步声明 preserve schema。实现拆入 `project_preserve.{h,c}`，避免继续扩大 `project.c`。
  RED/GREEN：RED 为新增 preserve 规则测试后 project model 缺失 preserve fields / enum 导致编译失败；
  GREEN 后合法 type/all 与 method/default 两条规则解析通过，非法 target 被拒绝，manifest normalization 测试 12/0。
  测试结果：WSL gcc `zr_vm_project_manifest_normalization_test` 12/0、`zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 12/0、9/0；schema JSON 解析通过；Windows MSVC 两个 focused 测试分别 12/0、9/0；
  Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s7e-zrp-project-manifest-preserve-rule-parsing.md`。
  备注：本记录只覆盖 manifest declaration parser，不关闭 preserve target 到 token/function flat index 的绑定、
  generic 实参、writer option 自动注入、feature switch、AOT mode、默认最小 metadata 策略或 dump/diff 工具。

- 2026-06-24 22:56:42 +08:00 · 11-S7D zrp project manifest legacy declared assembly mapping ·
  状态：11-S7 子切片完成、11 阶段继续进行中；完整 11-S7 仍未完成 ·
  完成项目：旧 `dependencies.$alias` object 现在接受 `assembly` 或 legacy `name` 声明目标 assembly
  identity；声明值使用 assembly-name shape 校验，`assembly`/`name` 冲突拒绝，声明 identity 与目标
  `.zrp` manifest 实际 identity 不一致拒绝。含点段 assembly 通过 alias package key 继续解析
  `&alias` import，同时真实 assembly identity 写入 package/ref 元数据并暴露给
  `ZrLibrary_Project_GetDependencyImportVersionRange()`。
  RED/GREEN：RED 为新增 legacy declared assembly 用例后 10 个用例中 2 个失败：含点段 declared assembly
  被拒绝、declared identity mismatch 被接受；补充 AssemblyRef identity 查询断言后也先失败。GREEN 后
  manifest normalization 测试 10/0，schema JSON 解析通过。
  测试结果：WSL gcc `zr_vm_project_manifest_normalization_test` 10/0、`zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 10/0、9/0；Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s7d-zrp-project-manifest-legacy-declared-assembly-mapping.md`。
  备注：本记录只覆盖 legacy dependency declared assembly compatibility mapping，不关闭 preserve 规则解析、
  AOT mode、默认最小 metadata 策略或 dump/diff 工具。

- 2026-06-24 22:39:26 +08:00 · 11-S7C zrp project manifest legacy identity/schema parity ·
  状态：11-S7 子切片完成、11 阶段继续进行中；完整 11-S7 仍未完成 ·
  完成项目：`.zrp` project loader 对 compatibility mapping 的 top-level `name` 执行 assembly-name shape
  校验，非法旧 `name` 拒绝；top-level `version` 的非 string/null 形态拒绝；缺省 assembly identity 字段继续
  规范化为 version `0.0.0`、culture `neutral`、kind `library`。`zrp.schema.json` 同步收紧
  `manifestVersion`、legacy `name`、`publicKeyToken` 与 `kind`。
  RED/GREEN：RED 为 legacy identity 测试扩展后 7 个用例中 1 个失败：旧 `name: "app render"` 被接受；
  GREEN 后 manifest normalization 测试 8/0，schema JSON 解析通过。
  测试结果：WSL gcc `zr_vm_project_manifest_normalization_test` 8/0、`zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 8/0、9/0；Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s7c-zrp-project-manifest-legacy-identity-schema-parity.md`。
  备注：本记录只覆盖 manifest Layer 1 identity/schema parity，不关闭 preserve 规则解析、AOT mode、
  默认最小 metadata 策略或 dump/diff 工具。

- 2026-06-24 22:26:53 +08:00 · 11-S7B zrp project manifest publicKeyToken normalization ·
  状态：11-S7 子切片完成、11 阶段继续进行中；完整 11-S7 仍未完成 ·
  完成项目：`.zrp` project loader 对 `assembly.publicKeyToken` 做十六进制校验并把 `A-F` 归一化为小写；
  非 hex token 拒绝，`null` 仍表示无 token。
  RED/GREEN：RED 为 publicKeyToken 测试新增后 2 个失败：大写 token 未小写化、非法 token 被接受；
  GREEN 后 manifest normalization 测试 5/0。
  测试结果：WSL gcc `zr_vm_project_manifest_normalization_test` 5/0、`zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 5/0、9/0；Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s7b-zrp-project-manifest-public-key-token-normalization.md`。
  备注：本记录只覆盖 publicKeyToken identity text normalization，不关闭 strong-name 验证、runtime binding 诊断、
  preserve 规则解析、默认最小 metadata 策略或 dump/diff 工具。

- 2026-06-24 22:19:43 +08:00 · 11-S7A zrp project manifest normalization gates ·
  状态：11-S7 子切片完成、11 阶段继续进行中；完整 11-S7 仍未完成 ·
  完成项目：`.zrp` project loader 校验 `manifestVersion`，只接受缺省或 `1`；旧 `dependencies.$alias` 与新
  `references.alias` 指向同一 package / assembly / version range 时去重，冲突时继续拒绝。
  RED/GREEN：RED 为新增 project manifest normalization 测试出现 2 个失败：同值 mixed old/new reference 被拒绝、
  unsupported `manifestVersion: 2` 被接受；GREEN 后 mixed 同值只保留一条 ref，mixed 冲突和 unsupported version 均拒绝。
  测试结果：WSL gcc `zr_vm_project_manifest_normalization_test` 3/0、`zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 3/0、9/0；Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s7a-zrp-project-manifest-normalization-gates.md`。
  备注：本记录只覆盖 `.zrp` manifest loader normalization gate，不关闭 preserve 规则解析、按 symbol/token 保留、
  AOT mode、默认最小 metadata 策略或 dump/diff 工具。

- 2026-06-24 21:56:34 +08:00 · 11-S1J zrp signature blob structural validator ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1 仍未完成 ·
  完成项目：`ZrCore_ZrpMetadata_ValidateSignatureBlob()` 支持对 method signature、field signature 与常用
  type node 做边界安全的结构校验，并拒绝 null/empty blob、截断 payload、未知 node、非法 root、嵌套
  method/field signature 与尾随字节。
  RED/GREEN：RED 为 zrp metadata format 测试要求 signature blob validator 后链接失败；GREEN 后 format
  测试覆盖合法 method/field/generic-inst blob，以及 null、空、尾随字节、截断和未知 node 失败边界。
  测试结果：WSL gcc `zr_vm_zrp_metadata_format_test` 11/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 11/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1j-zrp-signature-blob-structural-validator.md`。
  备注：本记录只覆盖 signature blob 结构校验，不关闭 token/type/string 语义解析、compiler signature pool
  导出、zrp manifest 读写或 dump/diff 工具。

- 2026-06-24 21:46:03 +08:00 · 11-S1I zrp string pool view decoder ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1 仍未完成 ·
  完成项目：`SZrZrpMetadataStringView` 与 `ZrCore_ZrpMetadata_GetString()` 支持从 string pool
  按 offset 解析 NUL-terminated string view，返回不含 NUL 的 byte length，并拒绝 offset 越界、
  缺少终止 NUL 与空输出指针。
  RED/GREEN：RED 为 zrp metadata format 测试要求 string view type/API 后编译失败；GREEN 后 format
  测试覆盖普通字符串、空字符串、pool 尾越界和缺失 NUL 边界。
  测试结果：WSL gcc `zr_vm_zrp_metadata_format_test` 10/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 10/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1i-zrp-string-pool-view-decoder.md`。
  备注：本记录只覆盖 string pool 只读 view 解码，不关闭 UTF-8 语义校验、compiler string pool 导出、
  签名 blob 解析、zrp manifest 读写或 dump/diff 工具。

- 2026-06-24 21:33:41 +08:00 · 11-S1H zrp definition table payload writer ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1 仍未完成 ·
  完成项目：`ZrCore_ZrpMetadata_WriteDefinitionTablePayload()` 支持把完整 row payload 写入
  TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、TypeSpec、MethodSpec、ModuleRef
  section，并拒绝非表 section、非空 row payload 空指针、row count/element size 与 section 目录不一致、
  截断 buffer。
  RED/GREEN：RED 为 zrp metadata format 测试要求 definition-table writer 后链接失败；GREEN 后 format
  测试覆盖 TypeDef/MethodDef 写入/读回、定义表校验与失败边界。
  测试结果：WSL gcc `zr_vm_zrp_metadata_format_test` 9/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 9/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1h-zrp-definition-table-payload-writer.md`。
  备注：本记录只覆盖 zrp definition-table row payload 写入，不关闭 compiler 真实定义表导出、
  字符串/签名解析、zrp manifest 读写或 dump/diff 工具。

- 2026-06-24 21:25:01 +08:00 · 11-S1G zrp pool payload writer ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1 仍未完成 ·
  完成项目：`ZrCore_ZrpMetadata_WritePoolPayload()` 支持把完整 payload 写入 string pool、
  signature blob pool、constant pool 三类 byte pool，并拒绝非 pool section、非空 payload 空指针、
  payload 长度与 section 目录不一致、截断 buffer；0 长度空 pool 可 no-op 写入。
  RED/GREEN：RED 为 zrp metadata format 测试要求 pool writer 后链接失败；GREEN 后 format 测试覆盖三类
  pool 写入/读回与失败边界。
  测试结果：WSL gcc `zr_vm_zrp_metadata_format_test` 8/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 8/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1g-zrp-pool-payload-writer.md`。
  备注：本记录只覆盖 zrp 三类 byte pool payload 写入，不关闭定义表内容导出、字符串/签名解析、
  zrp manifest 读写或 dump/diff 工具。

- 2026-06-24 21:04:24 +08:00 · 12-S7J runtime fallback warning source line ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：runtime fallback trim warning marker 新增 `sourceLine=<debugLine>` 字段，由
  `backend_aot_c_runtime_fallback.c` 从 ExecIR instruction 的 `debugLine` 输出；focused fixture 覆盖
  dynamic call 与 dynamic member/index value-access。
  RED/GREEN：RED 为 dynamic deopt bridge smoke 要求 `sourceLine=41` 后 warning marker 断言失败；GREEN 后
  dynamic deopt bridge smoke 4/0。
  测试结果：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0；`zr_vm_aot_c_generic_call_typed_test` 6/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7j-runtime-fallback-warning-source-line.md`。
  备注：本记录只覆盖已有 ExecIR debug line 到 warning marker 的传播，不关闭完整 trim analyzer、
  source range 诊断、warning 抑制、zrp metadata 裁剪或 release 符号剥离。

- 2026-06-24 20:48:06 +08:00 · 12-S7I runtime fallback diagnostics module split ·
  状态：12-S7I 支持性 refinement 完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：runtime fallback warning 统计/输出与 12-S8A-S8D full-AOT runtime closure 预检从
  `backend_aot_c_emitter.c` 拆入 `backend_aot_c_runtime_fallback.{h,c}`；emitter 收回到 520 行，
  新模块 294 行。
  RED/GREEN：本次为保持行为的支持拆分，未新增 RED；GREEN 复跑 focused 与相关 AOT 回归。
  测试结果：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0；`zr_vm_aot_c_generic_call_typed_test` 6/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_code_stripping_test` 3/0。
  备注：本记录只覆盖 runtime fallback diagnostics 归属整理，不关闭完整 trim analyzer、source span 诊断、
  warning 抑制、zrp metadata 裁剪或 release 符号剥离。

- 2026-06-24 20:28:06 +08:00 · 12-S7I runtime fallback trim warning reason classification ·
  状态：12-S7 子切片 refinement 完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：runtime fallback warning 的 `reason` 字段从单一文本细化为分类枚举，当前可输出
  `dynamic-call`、`dynamic-value-access`、`dynamic-iterator` 与 `reflection`；focused 验收覆盖 dynamic call
  和 dynamic value-access。
  RED/GREEN：RED 为 dynamic value-access hybrid smoke 要求 `reason=dynamic-value-access` 后失败；GREEN 后
  dynamic deopt bridge smoke 4/0。
  测试结果：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7i-runtime-fallback-warning-reason-classification.md`。
  备注：本记录只覆盖 warning reason 分类，不关闭完整 trim analyzer、source span 诊断、warning 抑制、
  zrp metadata 裁剪或 release 符号剥离。

- 2026-06-24 20:17:59 +08:00 · 12-S7I runtime fallback trim warning markers ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：AOT C writer 在 hybrid 生成文件头部输出 `trim_warnings.runtimeFallbackCount`，并为会触发
  runtime fallback 的 dynamic/reflection boundary 输出 `trim_warning.runtimeFallback[index]` marker；当前 focused
  验收覆盖 SemIR dynamic call deopt bridge。
  RED/GREEN：RED 为 dynamic deopt bridge hybrid smoke 要求 warning marker 后失败；GREEN 后 hybrid 仍生成
  dynamic deopt bridge 且 full-AOT 拒绝路径保持通过。
  测试结果：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7i-runtime-fallback-trim-warning-markers.md`。
  备注：本记录只覆盖 runtime fallback warning marker，不关闭完整 trim analyzer、zrp section/table/pool 明细、
  source span 诊断、release 符号剥离或 12-S8 完整闭合。

- 2026-06-24 20:06:57 +08:00 · 12-S7H type-layout trim before/after statistics ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：AOT C writer 在生成文件头部输出
  `code_stripping.typeLayoutsBefore`、`code_stripping.typeLayoutsAfter` 和
  `code_stripping.typeLayoutsRemoved`，统计 function table 中 distinct inline `typeLayoutId` 引用在
  reachability filter 前后的变化。
  RED/GREEN：RED 为 code-stripping 用例要求 type-layout before/after/removed marker 后 3 个用例失败；
  GREEN 后普通裁剪路径为 2→1/removed=1，export root 与 manifest root 保留路径为 2→2/removed=0。
  测试结果：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7h-type-layout-trim-before-after-statistics.md`。
  备注：本记录只覆盖 type-layout 引用数量裁剪前后对比，不关闭 generated layout byte delta、
  zrp section/table/pool 明细、trim warning 或 release 符号剥离。

- 2026-06-24 19:35:15 +08:00 · 12-S7G generated method metadata byte statistics ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：AOT C method-info emitter 为每个 generated `zr_aot_signature_<flatIndex>` +
  `zr_aot_method_info_<flatIndex>` descriptor block 输出 `aot_size.methodMetadataBytes[flatIndex]`，
  并在 method-info 区末尾输出 `aot_size.methodMetadataBytesTotal`。
  RED/GREEN：RED 为 generic call typed 生成 C 用例要求 method metadata byte marker 后失败；GREEN 后
  generic call typed source/binary/full-AOT 路径继续通过。测试结果：`zr_vm_aot_c_generic_call_typed_test` 6/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s7g-method-metadata-byte-statistics.md`。
  备注：本记录只覆盖 generated signature/method-info descriptor metadata，不关闭 zrp section/table/pool 明细、
  trim warning、trim 前后 metadata 对比或 release 符号剥离。

- 2026-06-24 19:24:50 +08:00 · 12-S7F embedded module byte statistic ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：AOT C writer 在生成文件头部输出 `aot_size.embeddedModuleBytes = <bytes>`，
  统计随 module descriptor 嵌入的 `.zro/.zrp` blob 字节数，来源为
  `SZrAotWriterOptions.embeddedModuleBlobLength`。
  RED/GREEN：RED 为 generic call typed binary-AOT 用例要求 embedded module byte marker 后失败；
  GREEN 后生成 C 含该 marker，二进制输入的共享泛型 smoke 仍执行返回 `42`。
  测试结果：`zr_vm_aot_c_generic_call_typed_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7f-embedded-module-byte-statistic.md`。
  备注：本记录只覆盖 embedded module blob carrier 字节统计，不关闭 zrp section/table/pool 明细、
  trim warning、trim 前后 metadata 对比或 release 符号剥离。

- 2026-06-24 19:10:02 +08:00 · 08-S7B / 12-S8E full-AOT generic METHOD slot static closure ·
  状态：08-S7 与 12-S8 子切片完成；08/12 阶段继续进行中，完整 full-AOT 泛型实例闭合仍未完成 ·
  完成项目：full-AOT shared generic `CALL_TYPED` 对已静态收集 callee 直接传 `zr_aot_fn_<callee>` 给
  `ZrLibrary_AotRuntime_CallInlineStruct()`，不再生成 callsite-local METHOD slot dictionary、
  `ZrAot_GenericSlot_Method()` lookup 或 METHOD slot null runtime branch；hybrid 路径保持 missing-instance
  deopt bridge。
  RED/GREEN：RED 为 full-AOT generic call typed 仍含 METHOD slot null branch；GREEN 后该分支被移除，full-AOT
  共享泛型调用仍编译并执行返回 `42`。测试结果：`zr_vm_aot_c_generic_call_typed_test` 6/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s8e-full-aot-generic-method-slot-closure.md`。
  备注：本记录只覆盖已静态收集 generic callsite，不关闭 manifest 动态泛型实例、反射 `MakeGenericType` 或完整收集不全诊断。

- 2026-06-24 18:52:22 +08:00 · 12-S8D full-AOT TYPEOF reflection runtime contract guard ·
  状态：12-S8 子切片完成、12 阶段继续进行中；完整 12-S8 仍未完成 ·
  完成项目：AOT C writer 在 `requireFullAot` 下预检 `TYPEOF` reflection runtime contract，遇到需要
  `ZrLibrary_AotRuntime_TypeOf()` 的未注解反射边界时返回 `ZR_FALSE` 并删除半成品 C 文件。
  RED/GREEN：RED 为 global shared-library smoke 在 full-AOT 下仍成功生成 TYPEOF 产物；GREEN 后默认 hybrid
  TYPEOF runtime boundary 仍生成并编译，full-AOT TYPEOF 被拒绝。测试结果：
  `zr_vm_aot_c_global_shared_library_smoke_test` 10/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s8d-full-aot-typeof-reflection-closure.md`。
  备注：本记录只覆盖未注解 TYPEOF reflection runtime boundary，不关闭 invoker、token 解析、反射注解数据流、泛型实例或完整裁剪闭包诊断。

- 2026-06-24 18:42:16 +08:00 · 12-S8C full-AOT dynamic iterator deopt closure guard ·
  状态：12-S8 子切片完成、12 阶段继续进行中；完整 12-S8 仍未完成 ·
  完成项目：AOT C writer 在 `requireFullAot` 下预检 SemIR/显式 dynamic iterator runtime boundary，
  遇到 `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT` / `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE` 或对应 SemIR
  dynamic iterator 行时返回 `ZR_FALSE` 并删除半成品 C 文件。
  RED/GREEN：RED 为 iterator shared-library smoke 在 full-AOT 下仍成功生成；GREEN 后默认 hybrid iterator
  helper 仍生成并编译，full-AOT 动态迭代被拒绝。测试结果：
  `zr_vm_aot_c_iterator_shared_library_smoke_test` 2/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s8c-full-aot-dynamic-iterator-closure.md`。
  备注：本记录只覆盖 dynamic iterator deopt runtime boundary，不关闭反射、泛型实例或完整裁剪闭包诊断。

- 2026-06-24 18:33:16 +08:00 · 12-S8B full-AOT dynamic value-access deopt closure guard ·
  状态：12-S8 子切片完成、12 阶段继续进行中；完整 12-S8 仍未完成 ·
  完成项目：AOT C writer 在 `requireFullAot` 下预检 SemIR dynamic member/index value access，
  遇到 `META_GET` / `META_SET` / `DYN_INDEX_GET` / `DYN_INDEX_SET` 这类需要 value-access deopt bridge
  的路径时返回 `ZR_FALSE` 并删除半成品 C 文件。
  RED/GREEN：RED 为 dynamic value-access smoke 在 full-AOT 下仍成功生成；GREEN 后默认 hybrid member/index
  deopt bridge 仍生成并编译，full-AOT 动态成员/索引访问被拒绝。测试结果：
  `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s8b-full-aot-dynamic-value-access-closure.md`。
  备注：本记录只覆盖 dynamic member/index value-access deopt bridge，不关闭 dynamic iterator、反射、泛型实例或完整裁剪闭包诊断。

- 2026-06-24 18:22:58 +08:00 · 12-S8A full-AOT dynamic-call deopt closure guard ·
  状态：12-S8 子切片完成、12 阶段继续进行中；完整 12-S8 仍未完成 ·
  完成项目：AOT C writer 在 `requireFullAot` 下预检 SemIR/显式 dynamic call，遇到无法静态解析 callee
  且需要 `CallDynamicDeoptBridge` 的路径时返回 `ZR_FALSE` 并删除半成品 C 文件。
  RED/GREEN：RED 为 dynamic-deopt bridge smoke 在 full-AOT 下仍成功生成；GREEN 后默认 hybrid deopt bridge
  仍生成，full-AOT 动态调用被拒绝。测试结果：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 3/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s8a-full-aot-dynamic-call-closure.md`。
  备注：本记录只覆盖 dynamic call deopt bridge，不关闭 dynamic value access、反射、泛型实例或完整裁剪闭包诊断。

- 2026-06-24 18:06:18 +08:00 · 12-S7E generated type-layout byte total ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：type-layout emitter 累加每个 `aot_size.typeLayoutBytes[typeLayoutId]` span，并在 layout
  声明区末尾输出 `aot_size.typeLayoutBytesTotal = <bytes>`。
  RED/GREEN：RED 为 value-type shared-library smoke 要求 ref/POD 生成物含 type-layout 总量统计后失败；
  GREEN 后两类生成物均含 per-layout 与 total 统计。测试结果：
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7e-type-layout-byte-total.md`。
  备注：这是 generated type-layout 总量统计，不声明 trim 前后对比、metadata 体积统计、trim warning 或符号剥离完成。

- 2026-06-24 17:58:34 +08:00 · 12-S7D generated type-layout byte statistics ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：AOT C type-layout emitter 在每个 `ZrLayout_<typeLayoutId>` 及同组 generated GC descriptor
  block 后输出 `aot_size.typeLayoutBytes[typeLayoutId] = <bytes>`；POD layout 统计 layout block，
  引用字段 layout 统计 layout + descriptor block。
  RED/GREEN：RED 为 value-type shared-library smoke 要求 ref/POD 生成物含 type-layout byte 统计后失败；
  GREEN 后两类生成物均含统计，既有 GC descriptor 行为保持。测试结果：
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7d-type-layout-byte-statistics.md`。
  备注：这是 type-layout/generated-descriptor 体积入口，不声明 metadata 字节统计、trim warning 或符号剥离完成。

- 2026-06-24 17:50:57 +08:00 · 11-S1F zrp pool slice view API ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1、11-S2~11-S7 仍未完成 ·
  完成项目：新增 `SZrZrpMetadataPoolSliceView` 与 `ZrCore_ZrpMetadata_GetPoolSlice()`，
  支持从 string/signature blob/constant 三类 byte pool 中按 offset/length 获取只读 slice；池尾 0 长度
  slice 合法，非池 section 与越界 slice 被拒绝并清空输出。
  RED/GREEN：RED 为 format 测试要求 pool slice API 后编译失败；GREEN 后三类 pool payload、0 长度尾部 slice、
  非池 section 拒绝和越界拒绝均通过。测试结果：`zr_vm_zrp_metadata_format_test` 7/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1f-zrp-pool-slice-view.md`。
  备注：这是池读取入口，不声明池内容生成、字符串解码、签名 blob 解析、代码注册表或 token lazy resolve 完成。

- 2026-06-24 17:46:08 +08:00 · 11-S1E zrp definition-table RID/range validation ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1、11-S2~11-S7 仍未完成 ·
  完成项目：`ZrCore_ZrpMetadata_ValidateDefinitionTables()` 进一步校验 owner RID 与子表 range：
  MethodDef/FieldDef owner 必须落在 TypeDef 表内，GenericParam owner/constraint 必须存在，
  TypeDef 的 method/field/generic-param range 不得越界。
  RED/GREEN：RED 为新增 cross-table range 测试中越界 owner/range/constraint 仍被接受导致运行失败；
  GREEN 后合法 payload 通过，错误 owner RID、越界 method range、越界 generic-param constraint 被拒绝。
  测试结果：`zr_vm_zrp_metadata_format_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1e-zrp-definition-table-range-validation.md`。
  备注：这是格式层 RID/range 护栏，不声明真实表导出、跨模块解析、代码注册表或 token lazy resolve 完成。

- 2026-06-24 17:40:49 +08:00 · 11-S1D zrp definition-table token validation ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1、11-S2~11-S7 仍未完成 ·
  完成项目：新增 `ZrCore_ZrpMetadata_ValidateDefinitionTables()`，按 section view 校验定义表行的
  token/table tag：TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、TypeSpec、
  MethodSpec、ModuleRef 不再只依赖 row 宽度合法，还会拒绝明显错误的 token 表归属。
  RED/GREEN：RED 为 format 测试要求定义表 token 校验 API 后链接失败；GREEN 后合法定义表 payload 通过，
  错误 TypeDef token、MethodDef owner、MethodSpec method token 被拒绝。测试结果：
  `zr_vm_zrp_metadata_format_test` 5/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1d-zrp-definition-table-token-validation.md`。
  备注：这是格式层 token/tag 护栏，不声明跨表 RID 范围、真实表导出、代码注册表或 token lazy resolve 完成。

- 2026-06-24 17:34:08 +08:00 · 11-S1C zrp mmap section view API ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1、11-S2~11-S7 仍未完成 ·
  完成项目：新增 `SZrZrpMetadataSectionView` 与 `ZrCore_ZrpMetadata_GetSectionView()`，
  可从已验证 zrp metadata 只读 buffer 按 section kind 解析 payload 指针、字节长度、count 和 element size；
  空 section 返回合法空 view，截断 buffer 或未知 section kind 被拒绝并清空输出。
  RED/GREEN：RED 为 format 测试要求 section view API 后编译失败；GREEN 后 TypeDef/string-pool payload、
  空 constant-pool view、截断 buffer 拒绝和非法 kind 拒绝均通过。测试结果：
  `zr_vm_zrp_metadata_format_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1c-zrp-section-view.md`。
  备注：这是 mmap section payload 访问入口，不声明真实表内容导出、zrp manifest 文件读写、代码注册表或
  token lazy resolve 完成。

- 2026-06-24 17:28:29 +08:00 · 11-S1B zrp definition table directory ABI ·
  状态：11-S1 子切片完成、11 阶段继续进行中；完整 11-S1、11-S2~11-S7 仍未完成 ·
  完成项目：zrp metadata header 从 4-section 扩展为 version 2 / 208 字节 / 12-section 目录，
  固定覆盖 token records、TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、
  TypeSpec、MethodSpec、ModuleRef、string pool、signature blob pool、constant pool；新增对应紧凑定义表
  row 类型，并让 header 校验按 section kind 拒绝错误 element size / byte length。
  RED/GREEN：RED 为 zrp metadata format 测试要求定义表目录和 row 类型后编译失败；GREEN 后 12-section
  header round-trip 与错误定义表宽度拒绝均通过。测试结果：`zr_vm_zrp_metadata_format_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1b-zrp-definition-table-directory.md`。
  备注：这是定义表目录 ABI，不声明真实表内容导出、池内容物化、zrp manifest 文件读写、代码注册表或 token
  lazy resolve 完成。

- 2026-06-24 17:17:43 +08:00 · 12-S7C retained function body byte total ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：opt-in AOT C code stripping 在已发射函数体后输出
  `code_stripping.functionBodyBytesTotal = <bytes>`，聚合所有保留函数的 generated-C body span；被裁剪函数
  不进入总量。RED/GREEN：RED 为 code-stripping 测试要求总函数字节统计后 3 个用例失败；GREEN 后
  普通裁剪、export root、manifest root 路径均通过。测试结果：`zr_vm_aot_c_code_stripping_test` 3/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s7c-function-body-byte-total.md`。
  备注：这是 retained-function 总量统计，不声明裁剪前估算、类型/layout/metadata 字节统计、trim warning
  或符号剥离完成。

- 2026-06-24 17:10:37 +08:00 · 12-S7B emitted function body byte statistics ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：opt-in AOT C code stripping 在每个已发射函数体后输出
  `code_stripping.functionBodyBytes[flatIndex] = <bytes>`；普通裁剪路径只统计保留的 0/1，
  不统计被移除的 2；export root 与 manifest root 保留路径统计 0/1/2。RED/GREEN：RED 为
  code-stripping 测试要求 body-byte 统计后 3 个用例失败；GREEN 后 3 条路径均通过。
  测试结果：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7b-function-body-byte-statistics.md`。
  备注：这是 generated-C 函数体级字节统计，不声明类型/layout/metadata 字节统计、trim warning 或符号剥离完成。

- 2026-06-24 17:01:12 +08:00 · 12-S7A function stripping statistics ·
  状态：12-S7 子切片完成、12 阶段继续进行中；完整 12-S7、12-S8 仍未完成 ·
  完成项目：opt-in AOT C code stripping 在生成文件头部输出
  `code_stripping.enabled`、`functionsBefore`、`functionsAfter`、`functionsRemoved`；
  普通不可达 child 裁剪时统计 3→2/removed=1，export root 与 manifest root 保留时统计 3→3/removed=0。
  RED/GREEN：RED 为 code stripping 测试要求统计注释后 3 个用例失败；GREEN 后 3 条 opt-in 裁剪路径均通过。
  测试结果：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7a-function-stripping-statistics.md`。
  备注：这是函数级体积统计入口，不声明 trim analyzer、类型/元数据字节统计或 release 符号剥离完成。

- 2026-06-24 16:54:42 +08:00 · 09-S1B AOT GC descriptor module table publication ·
  状态：09-S1 完成、09 阶段继续进行中；09-S2~09-S5 仍未完成 ·
  完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 6u`；新增公共 `SZrAotGcDescriptor`；
  `ZrAotCompiledModule` 发布 `gcDescriptors` / `gcDescriptorCount`；AOT C type-layout emitter 改用公共
  descriptor 类型并生成按 `typeLayoutId` 索引的稀疏 `zr_aot_gc_descriptors[]` 表；无 GC 字段模块保持
  null/0；value-type shared-library smoke 通过 `ZrVm_GetAotCompiledModule()` 读取 string-field struct
  descriptor 并确认 `gcFieldCount==1`。RED/GREEN：RED 为新增 smoke 编译失败，缺少
  `SZrAotGcDescriptor` 和模块 descriptor 字段；GREEN 后 source/type-layout/frame/descriptor/value-type/shared
  相关验证均通过。测试结果：`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_type_layout_contracts_test` 1/0、`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0、`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0。产出：
  `tests/acceptance/2026-06-24-aot-09-s1b-gc-descriptor-module-table.md`。
  备注：本切片关闭 09-S1 的 descriptor 发射、核心扫描和模块 metadata 注册面；AOT 栈根、safepoint、
  写屏障、装箱/FFI pin 与 runtime token/layout hydration 仍按后续切片推进。

- 2026-06-24 16:34:54 +08:00 · 12-S4A manifest preserve function roots ·
  状态：12-S4 子切片完成、12 阶段继续进行中；完整 12-S4、12-S5~12-S8 仍未完成 ·
  完成项目：`SZrAotWriterOptions` 新增已解析 manifest preserve 函数根输入
  `manifestPreserveFunctionFlatIndices` / `manifestPreserveFunctionFlatIndexCount`；
  `backend_aot_compute_static_callable_reachability()` 将这些 flat function index 作为
  `MANIFEST` roots 加入 BFS，并拒绝非法或不存在的 root。opt-in C code stripping 会把 writer options 中的
  manifest roots 传入 graph helper。RED/GREEN：RED 为 focused reachability 测试要求 manifest-root
  helper 形态而旧实现只有 entry/export roots；GREEN 后 manifest root 保留、无效 manifest root 拒绝，
  generated-C 合约证明 manifest 保留的 otherwise-unused 函数继续发射。测试结果：
  `zr_vm_aot_reachability_test` 6/0、`zr_vm_aot_c_code_stripping_test` 3/0、CTest
  `aot_c_code_stripping|aot_reachability` 2/2、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s4a-manifest-preserve-function-roots.md`。
  备注：这是 manifest 规则的后端输入通道；zrp manifest 解析、按 symbol/token 保留、注解 roots、
  feature switch、trim warning 和 full-AOT 闭合诊断仍待后续。

- 2026-06-24 16:20:39 +08:00 · 12-S2E export callable roots ·
  状态：12-S2 子切片完成、12 阶段继续进行中；完整 12-S2、12-S3~12-S8 仍未完成 ·
  完成项目：`backend_aot_compute_static_callable_reachability()` 改为由调用方提供 root/root-reason
  缓冲区，entry flat index 0 仍作为 `ROOT_ENTRY`，同时扫描 entry function 的
  `SZrFunctionTopLevelCallableBinding` 并把导出 callable child 作为 `ROOT_EXPORT`。opt-in C code stripping
  同步分配 root buffers。新增 focused reachability 合约与 generated-C 合约，证明未被 entry bytecode
  引用但作为模块导出的 child 仍会保留并发射。RED/GREEN：RED 为新测试调用 root-buffer 版 helper
  时旧 API/固定 root 实现编译失败；GREEN 后 export root 保留、普通 unused child 仍可裁剪。
  测试结果：`zr_vm_aot_reachability_test` 5/0、`zr_vm_aot_c_code_stripping_test` 2/0、CTest
  `aot_c_code_stripping|aot_reachability` 2/2、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s2e-export-callable-roots.md`。
  备注：manifest roots、默认 writer 裁剪、trim 诊断、体积统计和 full-AOT 闭合诊断仍待后续。

- 2026-06-24 16:10:41 +08:00 · 12-S2D opt-in AOT C code stripping emitter ·
  状态：12-S2 子切片完成、12 阶段继续进行中；完整 12-S2、12-S3~12-S8 仍未完成 ·
  完成项目：`SZrAotWriterOptions` 新增 `enableCodeStripping`，C emitter 在 opt-in 时调用 12-S2C
  静态 callable graph，再用 12-S2A filter 压缩 function table，同时保留 12-S2B `indexSpace`，使 thunk 与
  MethodInfo 表对不可达洞位输出 `ZR_NULL`。新增 `zr_vm_aot_c_code_stripping_test` 和 CTest
  `aot_c_code_stripping`，验证 root + reachable child + unused child 生成 C 时 `zr_aot_fn_2` 不再发射。
  RED/GREEN：RED 为测试编译失败，缺少 `enableCodeStripping`；GREEN 后 opt-in 裁剪保留 0/1、删除 2，
  并保持稀疏表洞位。测试结果：`zr_vm_aot_c_code_stripping_test` 1/0、`zr_vm_aot_reachability_test` 4/0、
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、CTest
  `aot_c_code_stripping|aot_reachability` 2/2。产出：
  `tests/acceptance/2026-06-24-aot-12-s2d-opt-in-code-stripping-emitter.md`。
  备注：默认 writer 路径仍全量生成；export/manifest roots、trim 诊断和体积统计仍待后续。

- 2026-06-24 15:50:18 +08:00 · 12-S2C static callable reachability graph helper ·
  状态：12-S2 子切片完成、12 阶段继续进行中；完整 12-S2、12-S3~12-S8 仍未完成 ·
  完成项目：新增 `backend_aot_reachability_function_graph.{h,c}` 与
  `backend_aot_compute_static_callable_reachability()`，以 entry flat index 0 为根，扫描
  `GET_CONSTANT`、`CREATE_CLOSURE`、`GET_SUB_FUNCTION` 静态 callable materialization，并写入
  `DIRECT_CALL` 边后复用 12-S1A BFS 生成 marks。focused 测试验证 `GET_SUB_FUNCTION` 子函数会被标记、
  未引用函数保持 `UNMARKED`，edge buffer 容量不足会被拒绝。RED/GREEN：RED 为缺少
  `backend_aot_reachability_function_graph.h`；GREEN 后 root+child 标记、edge reason/predecessor 与容量拒绝通过。
  测试结果：`zr_vm_aot_reachability_test` 4/0、CTest `aot_reachability` 1/1、
  `zr_vm_aot_c_source_contracts_test` 19/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s2c-static-callable-reachability-graph.md`。
  备注：默认 AOT C 过滤仍未启用，export/manifest roots、死函数不发 C 与体积统计仍待后续。

- 2026-06-24 15:37:08 +08:00 · 12-S2B sparse AOT thunk/method-info index space ·
  状态：12-S2 子切片完成、12 阶段继续进行中；完整 12-S2、12-S3~12-S8 仍未完成 ·
  完成项目：`SZrAotFunctionTable` 增加 `indexSpace`，构建时记录原始 function index 空间，过滤时只压缩
  可发射 entries；新增 `backend_aot_function_table_index_space()`。C emitter 的 forward decl、
  `zr_aot_function_thunks[]`、`zr_aot_method_infos[]` 和 descriptor count 改为使用原始 `flatIndex` 与
  `functionIndexSpace`，不可达洞位输出 `ZR_NULL`，为后续默认裁剪接入保留运行期索引 ABI。
  RED/GREEN：RED 为新增 index-space 测试链接失败，以及 frame setup contract 要求 sparse emitter 文本失败；
  GREEN 后过滤表保持 indexSpace=4，source contract 命中 `entry->flatIndex`、`functionIndexSpace` 和
  `ZR_NULL` 洞位。测试结果：`zr_vm_aot_reachability_test` 3/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、CTest `aot_reachability` 1/1。产出：
  `tests/acceptance/2026-06-24-aot-12-s2b-sparse-aot-index-space.md`。
  备注：真实 reachability graph 接入、默认过滤、死函数不发 C 和体积统计仍待后续。

- 2026-06-24 15:13:07 +08:00 · 12-S2A function table reachability filter helper ·
  状态：12-S2 子切片完成、12 阶段继续进行中；完整 12-S2、12-S3~12-S8 仍未完成 ·
  完成项目：`backend_aot_function_table.{h,c}` 新增
  `backend_aot_filter_function_table_by_reachability()`，可用 12-S1A 的 reachability mark 对
  `SZrAotFunctionTable` 原地压缩，跳过 `UNMARKED` 函数项，保留可达项的原始 `flatIndex`，并拒绝
  表结构非法或 `flatIndex` 超出 mark 数组的输入。RED/GREEN：RED 为新增过滤测试后缺少
  `backend_aot_filter_function_table_by_reachability` 链接符号；GREEN 后 4 项表压缩为 0/2 两个可达项、
  原编号不重排，mark 数不足输入被拒绝。测试结果：`zr_vm_aot_reachability_test` 3/0、CTest
  `aot_reachability` 1/1、`zr_vm_aot_c_source_contracts_test` 19/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s2a-function-table-reachability-filter.md`。
  备注：默认 AOT C emitter 尚未接入该过滤结果，真实死函数不发 C、体积下降统计、manifest/root 图构建
  与 full-AOT 闭合诊断仍待后续。

- 2026-06-24 15:04:13 +08:00 · 12-S1A AOT reachability state machine + BFS ·
  状态：12-S1 子切片完成、12 阶段进入进行中；完整 12-S1、12-S2~12-S8 仍未完成 ·
  完成项目：新增 `backend_aot_reachability.{h,c}` 和 `zr_vm_aot_reachability_test`，实现
  unmarked/marked_pending/processed 三态、root/direct-call/field/virtual/reflection/generic 等依赖原因、
  调用方缓冲区驱动的 BFS 标记、首次原因/predecessor 记录和非法图拒绝。RED/GREEN：RED 为缺少
  `backend_aot_reachability.h`；GREEN 后 root/edge 传播、manifest root reason 保留、未连接节点不标记、
  queue 容量/edge 越界拒绝均通过。测试结果：`zr_vm_aot_reachability_test` 2/0、CTest 1/1、
  `zr_vm_aot_c_source_contracts_test` 19/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s1a-reachability-engine.md`。
  备注：真实函数体扫描、function table 可达过滤、体积下降与 full-AOT 闭合诊断仍待后续 12 切片。

- 2026-06-24 14:53:23 +08:00 · 11-S1A zrp metadata header/section format ·
  状态：11-S1 子切片完成、11 阶段进入进行中；完整 11-S1、11-S2~11-S7 仍未完成 ·
  完成项目：新增 `zr_vm_core/zrp_metadata.h` / `zrp_metadata.c`，定义固定 80 字节 zrp metadata header、
  四个 section 目录（token records、string pool、signature blob pool、constant pool），并提供
  little-endian header init/read/write/validate API；新增 `zr_vm_zrp_metadata_format_test` 和 CTest 入口。
  RED/GREEN：RED 为新增测试目标配置后缺少 `zr_vm_core/zrp_metadata.h`；GREEN 后 round-trip 与坏 mmap
  view 拒绝用例通过。测试结果：`zr_vm_zrp_metadata_format_test` 2/0、
  `zr_vm_metadata_runtime_query_test` 3/0、`zr_vm_metadata_token_model_test` 21/0；CTest 3/3。产出：
  `tests/acceptance/2026-06-24-aot-11-s1a-zrp-metadata-header.md`。
  备注：实际定义表/池内容导出、zrp manifest 文件读写、代码注册表与 token lazy resolve 仍待后续 11 切片。

- 2026-06-24 14:37:23 +08:00 · 10-S1A MethodInfo reflection metadata level carrier ·
  状态：10-S1 子切片完成、10 阶段进入进行中；完整 10-S1、10-S2~10-S5 仍未完成 ·
  完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 5u`；新增
  `EZrAotReflectionMetadataLevel` 三态；`SZrAotMethodInfo` 增加 `reflectionMetadataLevel` 与保留字节；
  生成 MethodInfo 默认写入 `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，共享库 descriptor 可在运行期读到该级别。
  RED/GREEN：RED 为 MethodInfo 无反射 metadata level carrier；GREEN 后源契约、descriptor runtime assertion、
  ABI mismatch 诊断均覆盖新字段和 ABI 版本。测试结果：
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0。产出：
  `tests/acceptance/2026-06-24-aot-10-s1a-reflection-metadata-level.md`。
  备注：实体可达性分析、未反射类型元数据裁剪、体积下降对比和 `DESCRIPTION` 保留策略仍待后续 10/12。

- 2026-06-24 14:20:45 +08:00 · 09-S1A GC descriptor offset-list emission + metadata scan path ·
  状态：09-S1 子切片完成、09 阶段继续进行中；完整 09-S1、09-S2~09-S5 仍未完成 ·
  完成项目：`ZrCore_TypeLayout_VisitGcValues()` 对带 `gcFieldOffsets` 的非 union struct 优先走
  metadata offset 表；AOT C type-layout emitter 为含引用字段的 inline struct 生成
  `ZrGcOffsets_<id>[]` 与 `ZrGcDescriptor_<id>`，并跳过 `gcFieldCount == 0` 的 blittable/POD struct。
  RED/GREEN：RED 为 AOT 生成物只有 `ZrLayout_*` 和静态断言，核心 GC scan 仍只按字段表遍历；
  GREEN 后 `zr_vm_value_type_runtime_test` 14/0、`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_type_layout_contracts_test` 1/0。产出：
  `tests/acceptance/2026-06-24-aot-09-s1a-gc-descriptor-offsets.md`。
  备注：生成 descriptor 目前是 AOT C 内的静态载体，尚未通过 AOT MethodInfo/模块 metadata 注册给运行期，
  因此不关闭完整 09-S1。

- 2026-06-24 14:03:46 +08:00 · 08-S7A full-AOT generic CALL_TYPED no-deopt switch ·
  状态：08-S7 子切片完成、08 阶段继续进行中；完整 08-S7 和 12-S8 full-AOT 闭合诊断仍未完成 ·
  完成项目：`SZrAotWriterOptions.requireFullAot` 接入 AOT C function body/value SemIR lowering；
  默认 hybrid 模式保留 shared generic `CALL_TYPED` missing-instance interpreter deopt，显式 full-AOT
  模式生成 `zr_aot_generic_call_typed_full_aot_no_deopt`，静态 METHOD slot 为空时直接失败而不调用
  `CallInlineStructDynamicDeoptBridge()`。RED/GREEN：RED 为 full-AOT 选项缺失，所有 shared generic
  callsite 都生成动态兜底；GREEN 后 `zr_vm_aot_c_generic_call_typed_test` 6/0，full-AOT 用例确认生成 C
  不包含 missing-instance deopt marker/bridge/error label，并编译共享库执行返回与解释器一致的 `42`。
  产出：`tests/acceptance/2026-06-24-aot-08-s7a-full-aot-generic-call-typed.md`。
  备注：缺失实例的编译期诊断仍需要 `12` 可达性/manifest/full-AOT 闭合校验支撑，本记录不关闭完整 08-S7。

- 2026-06-24 13:47:41 +08:00 · 08-S6A generic CALL_TYPED missing-instance deopt bridge ·
  状态：08-S6 子切片完成、08 阶段继续进行中；完整 08-S6、08-S7 仍未完成 ·
  完成项目：runtime 增加 `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()`，在 shared generic
  `CALL_TYPED` METHOD slot 无法解析 AOT 入口时，按 deopt id 校验后准备 call window、复制 value
  参数、调回解释器并把 inline struct return 写入 AOT destination；生成 C 在
  `ZrAot_GenericSlot_Method()` 非空时继续走 `CallInlineStruct()`，为空时生成
  `zr_aot_generic_call_typed_missing_instance_deopt` 并调用动态桥。RED/GREEN：RED 为 METHOD slot
  缺失只能失败；GREEN 后 `zr_vm_aot_c_generic_call_typed_test` 5/0，其中新增用例把生成 C 的
  `.staticMethod = zr_aot_fn_*` 改成 `ZR_NULL` 后重新编译共享库，AOT execution 经解释器 fallback
  返回与解释器一致的 `42`。验证：generic call typed 5/0、dynamic deopt bridge smoke 2/0。
  产出：`tests/acceptance/2026-06-24-aot-08-s6a-generic-call-typed-missing-instance-deopt.md`。
  备注：这只覆盖 shared generic METHOD-slot missing-instance fallback；反射构造/运行期动态泛型实例化
  和 full-AOT 缺失实例诊断仍开放。

- 2026-06-24 13:32:21 +08:00 · 08-S5 generic CALL_TYPED monomorphized/shared dispatch ·
  状态：08-S5 验收完成、08 阶段继续进行中；08-S6、08-S7 仍未完成 ·
  完成项目：在 08-S5A 的 METHOD-slot lazy helper、shared METHOD-slot lookup/call 和 monomorphized
  direct marker 基础上，AOT C value SemIR `CALL_TYPED` route 现在拿到 callee typed metadata；
  对 `where T: class` 的源级引用泛型调用生成 callsite-local `ZR_AOT_GENERIC_SLOT_METHOD`
  dictionary，经 `ZrAot_GenericSlot_Method()` 取得 `zr_aot_fn_*`，再传入
  `ZrLibrary_AotRuntime_CallInlineStruct()`；非共享路径继续保持直接 `zr_aot_fn_*` 调用。
  同时修复 typed export metadata 构建时的 current function declaration，上游 generic parameter `T`
  能进入 callee parameter metadata。RED/GREEN：RED 为 08-S5A 后缺少源级泛型 `CALL_TYPED`
  shared callsite route 和执行一致性；GREEN 后 `zr_vm_aot_c_generic_call_typed_test` 3/0，第三个
  用例编译生成 C 共享库并通过 AOT execution 返回与解释器一致的 `42`。验证：
  `zr_vm_aot_c_generic_call_typed_test` 3/0、reference sharing 2/0、monomorphization 1/0、
  source contracts 19/0、frame setup contracts 1/0、method-info signature 1/0、type inference 全量通过、
  CTest 泛型 AOT 三项 3/3。产出：
  `tests/acceptance/2026-06-24-aot-08-s5-generic-call-typed.md`。备注：08-S6 dynamic-instance
  deopt、08-S7 full-AOT missing-instance 仍开放。

- 2026-06-24 12:46:24 +08:00 · 08-S5A generic CALL_TYPED METHOD-slot carrier ·
  状态：08-S5 子切片完成、08 阶段继续进行中；08-S5B、08-S6、08-S7 仍未完成 ·
  完成项目：runtime 增加 `ZrLibrary_AotRuntime_GenericSlot_Method()`，对
  `ZR_AOT_GENERIC_SLOT_METHOD` 做 lazy 解析与 `FZrAotEntryThunk` cache；AOT C generic sharing
  emitter 增加 `ZrAot_GenericSlot_Method` 宏、每共享引用实例的 METHOD slot 与静态 method target，
  并在 `zr_fn_<base>__shared` 内生成从字典 METHOD slot 取入口并调用的形态；AOT C generic
  monomorphization wrapper 增加 direct-call marker，锁定值类型泛型的直接特化入口形态。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_generic_call_typed_test` 后缺少 METHOD slot runtime helper；
  GREEN 后 helper cache、monomorphized direct marker、shared METHOD slot lookup/call 和 generated C
  shared-library compile 均通过。验证：`zr_vm_aot_c_generic_call_typed_test` 2/0、
  `zr_vm_aot_c_generic_reference_sharing_test` 2/0、`zr_vm_aot_c_generic_monomorphization_test` 1/0、
  source contracts 19/0、frame setup contracts 1/0、method-info signature 1/0，CTest 泛型 AOT 三项
  3/3。产出：`tests/acceptance/2026-06-24-aot-08-s5a-generic-call-typed-method-slot.md`。
  备注：本子切片只完成 METHOD-slot carrier 与生成契约；源级泛型 `CALL_TYPED` 双形态实际接入和
  AOT/解释器一致性仍是 08-S5B。

- 2026-06-24 12:14:13 +08:00 · 08-S4 generic reference sharing dictionary ·
  状态：08-S4 验收完成、08 阶段继续进行中；08-S5~08-S7 未开始 ·
  完成项目：AOT ABI 升至 v4，新增泛型字典 slot/cache/dictionary 结构，`SZrAotMethodInfo`
  携带 `genericDictionary`；runtime 增加 TYPE_LAYOUT/SIZEOF lazy slot 解析并缓存；
  AOT C emitter 为引用型闭泛型实例发每实例字典，按泛型基名只发一份 `zr_fn_<base>__shared`，
  同时发 `ZrAot_GenericSlot_*` 访问宏并将当前函数首个共享字典挂入 MethodInfo。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_generic_reference_sharing_test` 后缺少
  `SZrAotGenericDictionary` 与 lazy API；GREEN 后 `Box<RefA>`/`Box<RefB>` 两个实例共享一份
  `zr_fn_box__shared`，字典 lazy helper 直接验证 TYPE_LAYOUT/SIZEOF cache。验证：
  `zr_vm_aot_c_generic_reference_sharing_test` 2/0、CTest `aot_c_generic_reference_sharing` 1/1、
  frame setup contracts 1/0、source contracts 19/0、method-info signature 1/0、generic monomorphization 1/0。
  产出：`tests/acceptance/2026-06-24-aot-08-s4-generic-reference-sharing.md`。备注：08-S5 泛型
  `CALL_TYPED` 单态/共享分派、08-S6 dynamic-instance deopt、08-S7 full-AOT missing-instance 仍开放。

- 2026-06-24 11:38:33 +08:00 · 08-S3 generic monomorphization ·
  状态：08-S3 验收完成、08 阶段继续进行中；08-S4~08-S7 未开始 ·
  完成项目：新增 AOT generic monomorphization emitter，closed value-generic `Pair<int,int>`
  生成 `ZrLayout_<id>` inline layout、`zr_aot_generic_monomorphization_table` marker 与
  `zr_fn_pair__*` 特化 wrapper；closed generic prototype 生成 concrete field layout，typed metadata
  精确匹配 closed layout，core inline receiver copy-back 支持 open/closed layout id 不同但实际 layout
  兼容的构造路径。RED/GREEN：RED 依次暴露缺少 marker/layout、inline typed call failure、
  `COPY_STACK` missing inline layout、运行结果 0；GREEN 后 generated shared library 返回 81。
  验证：`zr_vm_aot_c_generic_monomorphization_test` 1/0、CTest `aot_c_generic_monomorphization` 1/1、
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_typed_scalar_test` 1/0、generic numeric
  contracts/smoke 各 1/0、parser 75/0、type inference 全量通过、CTest `generic_instantiation` /
  `generic_constraints` 2/2。产出：
  `tests/acceptance/2026-06-24-aot-08-s3-generic-monomorphization.md`。备注：08-S4 泛型字典、
  08-S5 泛型 `CALL_TYPED`、08-S6 dynamic-instance deopt、08-S7 full-AOT missing-instance 仍开放。

- 2026-06-24 10:28:48 +08:00 · 08-S2 generic constraints ·
  状态：08-S2 验收完成、08 阶段进行中；08-S3~08-S7 未开始 ·
  完成项目：确认现有约束求解覆盖 named constraints、class/struct、owner、unique/shared/weak
  ownership；新增 `zr_vm_generic_constraints_test` 独立覆盖 `where T: new()`，默认可构造 class
  放行、interface 报 `new() constraint`。RED/GREEN：新目标首次构建前因 CMake 尚未重配而不存在；
  重配后测试直接 GREEN，说明生产约束逻辑已满足验收。验证：`zr_vm_generic_constraints_test` 1/0、
  CTest `generic_constraints` 1/1、`zr_vm_parser_test` 75/0、`zr_vm_type_inference_test` 118/0。产出：
  `tests/acceptance/2026-06-24-aot-08-s2-generic-constraints.md`。备注：本切片是 08-S2 约束验收补齐，
  不改 AOT codegen；08-S3 才进入单态化生成。

- 2026-06-24 10:18:45 +08:00 · 08-S1 generic instantiation table ·
  状态：08-S1 完成、08 阶段进入进行中；08-S2~08-S7 未开始 ·
  完成项目：新增 parser 级 `SZrGenericInstantiationTable` / `SZrGenericInstantiationRecord`，记录
  `baseToken`、类型实参、`shareKind`、`cInstanceId`；按 base token + 实参类型 + resolved
  reference/value shape 去重；按 il2cpp-style 规则判定全部 reference 共享、任一 value 单态化；
  提供默认 `EZrValueType` shape 推断和显式 resolved class/struct shape 入口。RED/GREEN：RED 为
  新测试目标缺少 `zr_vm_parser/generic_instantiation.h`；GREEN 为 `zr_vm_generic_instantiation_test`
  3/0、CTest `generic_instantiation` 1/1、`zr_vm_type_inference_test` 118/0。产出：
  `tests/acceptance/2026-06-24-aot-08-s1-generic-instantiation-table.md`。备注：本切片只完成收集表与
  shareKind 判定；约束求解、codegen 单态化/共享字典、泛型 CALL_TYPED、动态实例 deopt 和 full-AOT
  模式仍开放。

- 2026-06-24 09:41:44 +08:00 · M1.5 / 07-S5 scalar typed direct-call aggregate guardrail ·
  状态：验收护栏子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 generated-product guardrail，用一个 mixed scalar fixture 同时锁定 i64/u64/f64/bool
  二参 typed→typed direct-call 的 state-free signature/call；禁止 typed destination、stack sync、runtime
  call fallback 与 `state` 首参退化；同时按 scalar typed thunk 函数体范围禁止解释器环境符号和 `SZrTypeValue`。
  RED/GREEN：guardrail target 先因仍只链接 core 而缺少 parser include
  构建失败；补齐 parser/core/library 依赖后 `zr_vm_aot_c_guardrail_contracts_test` 6/0。生成物 grep 确认
  i64/u64/f64/bool direct calls 均只传 scalar locals，且无 `SZrTypeValue *zr_aot_typed_destination`、
  `ZR_VALUE_FAST_SET(zr_aot_typed_destination,`、`CallStaticDirect`、`CallStackValue` 或 stateful typed-thunk
  用法；scoped thunk-body 检查确认四类 typed thunk body 都是直接 C return。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-scalar-typed-direct-call-guardrail.md`。
  备注：07-S5 仍部分完成；full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、
  性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 09:26:01 +08:00 · M1.5 / 07-S5 bool state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：bool no/one/two/three-arg typed thunks 生成并调用 `zr_aot_typed_bool_fn_N(void/TZrBool, ...)`，
  不再传 `state`；i64/u64/f64 comparison -> bool typed thunks 生成并调用 `zr_aot_typed_bool_fn_N(TZrInt64/TZrUInt64/TZrFloat64, ...)`，
  也不再传 `state`。RED/GREEN：typed-call contracts RED 1/4 后 GREEN 4/0，bool shared-library smoke 28/0，
  source contracts 19/0；生成物 grep 确认 bool no/two/three-arg 与 i64/u64/f64 comparison bool calls 均为 state-free。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-bool-state-free-typed-direct-call.md`。
  备注：07-S5 仍部分完成；full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、
  性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 09:10:18 +08:00 · M1.5 / 07-S5 f64 state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：f64 no/one-arg pure thunks 生成并调用 `zr_aot_typed_f64_fn_N(void/TZrFloat64)`，不再传
  `state`；纯 f64 二参/三参 add/subtract/multiply 生成并调用 `zr_aot_typed_f64_fn_N(TZrFloat64, ...)`，
  不再传 `state`；f64 divide/modulo 继续使用 stateful ABI 以保留 zero-denominator error path。
  RED/GREEN：typed-call contracts RED 1/4 后 GREEN 4/0，f64 shared-library smoke 19/0，source contracts
  19/0；生成物 grep 确认 two/three-arg add state-free，two/three-arg divide stateful。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-f64-state-free-typed-direct-call.md`。
  备注：07-S5 仍部分完成；bool ABI parity、full typed ABI、inline structs、in/out writeback、
  完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 08:52:05 +08:00 · M1.5 / 07-S5 u64 two/three-arg state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：纯 u64 二参/三参 typed thunks（add/subtract/multiply/bitwise AND/OR/XOR）生成并调用
  `zr_aot_typed_u64_fn_N(TZrUInt64, ...)`，不再传 `state`；u64 divide/modulo 继续使用 stateful ABI
  以保留 divide/modulo-by-zero error path。RED/GREEN：typed-call contracts RED 1/4 后 GREEN 4/0，
  u64 shared-library smoke 25/0，source contracts 19/0；生成物 grep 确认 two/three-arg add state-free，
  two/three-arg divide stateful。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-u64-two-three-arg-state-free-typed-direct-call.md`。
  备注：07-S5 仍部分完成；f64/bool ABI parity、full typed ABI、inline structs、in/out writeback、
  完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 08:26:14 +08:00 · M1.5 / 07-S5 u64 no/one-arg state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：u64 no-arg thunks 生成 `zr_aot_typed_u64_fn_N(void)` 并以空实参调用；u64 one-arg thunks
  生成 `zr_aot_typed_u64_fn_N(TZrUInt64)` 并以单 scalar local 调用，不再传 `state`。同时修复
  signed-const fallback scalar-local 读取/写回，以及 forced stack-copy 保留 u64 call-arg source local。
  RED/GREEN：typed-call contracts RED 1/4 后 GREEN 4/0，source contracts 19/0，u64 shared-library
  smoke 25/0；生成物 grep 确认 no/one-arg state-free，two-arg 参数同步使用 u64 scalar copy。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-u64-no-one-arg-state-free-typed-direct-call.md`。
  备注：07-S5 仍部分完成；u64 two/three-arg state-free ABI、f64/bool ABI parity、full typed ABI、
  inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 07:54:44 +08:00 · M1.5 / 07-S5 i64 three-arg state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：纯 signed i64 三参 typed thunks（add/sub/multiply/bitwise AND/OR/XOR）生成并调用
  `zr_aot_typed_i64_fn_N(TZrInt64, TZrInt64, TZrInt64)`，不再传 `state`；three-arg divide/modulo
  仍保留 stateful ABI，以保护 divide/modulo-by-zero error path。RED/GREEN：typed-call contracts
  RED 1/4 后 GREEN 4/0，i64 three-arg shared-library smoke 8/0；生成物 grep 确认 add/bitwise OR
  state-free，divide stateful。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-i64-three-arg-state-free-typed-direct-call.md`。
  备注：07-S5 仍部分完成；u64/f64/bool ABI parity、full typed ABI、inline structs、in/out writeback、
  完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 07:34:41 +08:00 · M1.5 / 07-S5 i64 no/one-arg state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：i64 no-arg thunks 生成 `zr_aot_typed_i64_fn_N(void)` 并以空实参调用；i64 one-arg thunks
  生成 `zr_aot_typed_i64_fn_N(TZrInt64)` 并以单 scalar local 调用，不再传 `state`。RED/GREEN：
  typed-call contracts RED 1/4 后 GREEN 4/0，arithmetic smoke 7/0，bitwise smoke 6/0，
  typed direct-call shared-library smoke 5/0；生成物 grep 确认 no/one-arg state-free，divide 继续 stateful。
  产出：`tests/acceptance/2026-06-24-aot-m1-5-i64-no-one-arg-state-free-typed-direct-call.md`。
  备注：07-S5 仍部分完成；i64 three-arg state-free ABI、u64/f64/bool ABI parity、full typed ABI、inline structs、
  in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 07:24:50 +08:00 · M1.5 / 07-S5 i64 two-arg state-free typed direct-call ABI ·
  状态：实现子切片完成、07-S5 typed direct-call ABI 收紧部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：纯 signed i64 二参 typed thunks（add/sub/multiply/bitwise AND/OR/XOR）生成并调用
  `zr_aot_typed_i64_fn_N(TZrInt64, TZrInt64)`，不再传 `state`；divide/modulo 仍保留 stateful ABI，
  以保护 divide/modulo-by-zero error path。RED/GREEN：arithmetic smoke 7/0、bitwise smoke 6/0、
  typed-call contracts 4/0、typed direct-call shared-library smoke 5/0；生成物 grep 确认 multiply/bitwise OR
  state-free，divide stateful。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-i64-two-arg-state-free-typed-direct-call.md`。备注：07-S5 仍部分完成；
  no/one/three-arg state-free ABI、u64/f64/bool ABI parity、full typed ABI、inline structs、in/out writeback、
  完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 06:48:00 +08:00 · M1.5 / 07-S5 i64 bitwise binary expression return coverage ·
  状态：覆盖子切片完成、07-S5 expression direct-return scalarization 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `i64_bitwise_and_expr` generated-product 合同，锁定 `return left & right;` 生成 i64
  MethodInfo signature、`hasReturnValue=1` 和 `ReturnI64(state, zr_aot_s2)`；生成物命中
  `zr_aot_scalar_exec_i64_bitwise`，不命中 `zr_aot_bitwise_exec_binary`、
  `SZrTypeValue *zr_aot_destination` 或 `zr_aot_value_exec_primitive_constant`。RED/GREEN：
  覆盖 probe 直接通过，说明既有 binary bitwise scalar-local/result-local proof 已覆盖该形态，生产代码无需改动；
  method-info signature 1/0，CTest `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-i64-bitwise-binary-expression-return.md`。备注：07-S5 仍部分完成；
  full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 06:42:01 +08:00 · M1.5 / 07-S5 i64 bit-not expression return coverage ·
  状态：覆盖子切片完成、07-S5 expression direct-return scalarization 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `i64_bit_not_expr` generated-product 合同，锁定 `return ~value;` 生成 i64 MethodInfo
  signature、`hasReturnValue=1` 和 `ReturnI64(state, zr_aot_s1)`；生成物命中
  `zr_aot_scalar_exec_i64_bit_not`，不命中 `zr_aot_bitwise_exec_unary`、
  `SZrTypeValue *zr_aot_destination` 或 `zr_aot_value_exec_primitive_constant`。RED/GREEN：
  覆盖 probe 直接通过，说明既有 bit-not scalar-local/result-local proof 已覆盖该形态，生产代码无需改动；
  method-info signature 1/0，CTest `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-i64-bit-not-expression-return.md`。备注：07-S5 仍部分完成；
  full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 06:32:09 +08:00 · M1.5 / 07-S5 i64 unary expression return scalarization ·
  状态：子切片完成、07-S5 expression direct-return scalarization 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：script-level signed integer unary direct return（`return -value;`）现在把 `NEG_SIGNED` 识别为
  i64 scalar-local result writer，并在可跳过 value slot 时生成 `zr_aot_scalar_exec_i64_unary` /
  `zr_aot_sN = -zr_aot_sM`；`NEG_SIGNED` 也作为 signed i64 local consumer 参与 source constant
  skip proof。生成 C 命中 i64 signature、`hasReturnValue=1`、`zr_aot_scalar_constant_i64_local` 和
  `ReturnI64(state, zr_aot_s1)`；`SZrTypeValue *zr_aot_destination`、`zr_aot_value_exec_primitive_constant`
  与旧 `zr_aot_arith_exec_signed_unary` 均无命中。RED/GREEN：
  focused method-info signature test 先因 `i64_neg_expr` 仍命中 forbidden signed unary helper 失败 1/1；
  严格 zero-materialization 检查又因 `SZrTypeValue *zr_aot_destination` 失败 1/1；实现后 method-info signature 1/0。
  补充验证通过 return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0；
  CTest `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-i64-unary-expression-return.md`。备注：07-S5 仍部分完成；
  full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 06:04:14 +08:00 · M1.5 / 07-S5 f64 unary expression return scalarization ·
  状态：子切片完成、07-S5 expression direct-return scalarization 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：script-level `float` unary direct return（`return -value;`）现在把 `NEG_FLOAT` 识别为 f64
  scalar-local result writer，并在可跳过 value slot 时生成 `zr_aot_scalar_exec_f64_unary` /
  `zr_aot_fN = -zr_aot_fM`。生成 C 命中 f64 signature、`hasReturnValue=1` 和 `ReturnF64(state, zr_aot_f1)`；
  generic frame-slot return、expression result `SZrTypeValue *zr_aot_destination` 与旧
  `zr_aot_arith_exec_float_unary` 均无命中。RED/GREEN：focused method-info signature test 先因 `f64_neg_expr`
  缺失 return type 失败 1/1；一次 build+run 合并 GREEN 命令在 124s 工具超时，拆分构建后 method-info
  signature 1/0。补充验证通过 return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、
  typed scalar 1/0；CTest `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-f64-unary-expression-return.md`。备注：07-S5 仍部分完成；
  signed unary direct return、full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、
  性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 05:50:48 +08:00 · M1.5 / 07-S5 f64 comparison expression return scalarization ·
  状态：子切片完成、07-S5 expression direct-return scalarization 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：script-level `float` comparison direct return（`return left > right;`）现在把 `LOGICAL_*_FLOAT`
  识别为 f64 source consumer + bool scalar-local result writer。生成 C 命中 bool signature、`hasReturnValue=1`、
  `zr_aot_scalar_exec_f64_compare` 和 `ReturnBool(state, zr_aot_b2)`；generic frame-slot return、
  expression result `SZrTypeValue *zr_aot_destination` 与旧 `zr_aot_compare_exec_float` 均无命中。
  RED/GREEN：focused method-info signature test 先因 `f64_bool_expr` 缺失 return type 失败 1/1；实现后
  method-info signature 1/0。补充验证通过 return contracts 1/0、frame setup contracts 1/0、source contracts
  19/0、typed scalar 1/0；CTest `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-f64-comparison-expression-return.md`。备注：07-S5 仍部分完成；
  full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 05:30:05 +08:00 · M1.5 / 07-S5 expression direct-return scalarization ·
  状态：子切片完成、07-S5 expression direct-return scalarization 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：script-level bool/u64/f64 direct expression returns 现在从 typed expression result opcode
  记录 scalar-local destination coverage，并让 `FUNCTION_RETURN` 使用 kind-based scalar return proof。
  bool/u64/f64 expression cases 生成非空 return signature、`hasReturnValue=1`、对应 `baseType/staticCType`
  和 `ReturnBool` / `ReturnU64` / `ReturnF64`；generic frame-slot return 与 expression result
  `SZrTypeValue *zr_aot_destination` grep 均无命中。RED/GREEN：focused method-info signature test
  先因 direct bool expression 缺失 return type 失败 1/1；实现后 method-info signature 1/0。补充验证通过
  return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0；CTest
  `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-expression-direct-return-scalarization.md`。备注：07-S5 仍部分完成；
  full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 05:06:03 +08:00 · M1.5 / 07-S5 script typed-local return route ·
  状态：子切片完成、07-S5 typed return boundary route 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：script-level bool/u64/f64 typed-local return 现在使用 typed native→VM return helper：
  `ReturnBool` / `ReturnU64` / `ReturnF64`，不再落回 generic frame-slot return。`backend_aot_c_typed_return.c`
  对 bool/u64/f64 route 同时接受 direct callable proof 与 inferred script proof，callable metadata gate
  仍由 scalar-local direct-return proof 保留。RED/GREEN：focused method-info signature test 先因缺失
  `ReturnBool(state, zr_aot_b...)` 失败 1/1；实现后 method-info signature 1/0。补充验证通过 return contracts 1/0、
  frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0；generated bool/u64/f64 grep 命中对应
  typed Return helper，generic frame-slot return grep 无命中；CTest `aot_c_method_info_signature` 1/1 passed。
  产出：`tests/acceptance/2026-06-24-aot-m1-5-script-typed-local-return-route.md`。备注：07-S5 仍部分完成；
  表达式直接返回标量化、full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、性能/SZrValue
  计数和 08-12 仍未完成。

- 2026-06-24 04:38:40 +08:00 · M1.5 / 07-S5 MethodInfo scalar return signatures ·
  状态：子切片完成、07-S5 MethodInfo/signature boundary metadata 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：AOT C emitter 现在除 i64 外，也能为 typed-local bool/u64/f64 script return 推导非空
  MethodInfo signature；通用 scalar return 推断器要求所有 `FUNCTION_RETURN` 都通过同一标量 proof。
  scalar-local proof 抽出共享 `can_return_kind_local`，typed callable direct-return 保留 callable metadata
  gate，script signature inference 使用无 callable gate 的 bool/u64/f64 helper。RED/GREEN：focused
  method-info signature test 先因缺失 return pointer 失败 1/1；实现后 method-info signature 1/0。
  补充验证通过 frame setup contracts 1/0、source contracts 19/0、return contracts 1/0、typed scalar 1/0；
  CTest `aot_c_method_info_signature` 1/1 passed；generated bool/u64/f64 signature grep 分别命中
  `baseType/staticCType=1/9/11`、return pointer 与 `hasReturnValue=1`。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-method-info-scalar-return-signatures.md`。备注：07-S5 仍部分完成；
  full typed ABI、inline structs、in/out writeback、表达式直接返回标量化、完整 07-S5 acceptance、性能/SZrValue
  计数和 08-12 仍未完成。

- 2026-06-24 04:00:05 +08:00 · M1.5 / 07-S5 MethodInfo typed return signature ·
  状态：子切片完成、07-S5 MethodInfo/signature boundary metadata 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：AOT C emitter 现在为缺少 callable return metadata、但所有 `FUNCTION_RETURN` 都能由
  i64 scalar-local direct-return proof 证明的 script-level typed return 生成非空 MethodInfo signature：
  `.returnType = &zr_aot_signature_0_types[0]`、`.hasReturnValue = 1`、i64 `baseType/staticCType`。
  RED/GREEN：typed scalar generated-product 先因缺失 return pointer 失败 1/1；实现后 focused
  typed scalar 1/0。补充验证通过 frame setup contracts 1/0、source contracts 19/0、return contracts
  1/0、typed scalar 1/0；07§9 environment grep 与 SZrValue/double-write grep 对 generated typed scalar
  均无命中，signature grep 命中预期 metadata。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-method-info-typed-return-signature.md`。规模：emitter 550、
  typed scalar 1101、frame setup contract 344。备注：full typed ABI、inline structs、in/out writeback、
  bool/u64/f64 script-level inferred signatures、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 03:34:00 +08:00 · M1.5 / 07-S5 boundary guardrail allowlist hardening ·
  状态：支撑子切片完成、07-S5 acceptance/guardrail 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：AOT C guardrail runtime-call classifier 显式允许当前 07-S5 VM↔native 边界 helper：
  scalar/inline-struct typed return、`Sync*Local`、`CallStackValue`、`CallStaticDirect`、
  `CallInlineStruct`、dynamic/deopt bridge validation，以及 dynamic member/index `Get*` / `Set*`
  helpers；未分类 VM fallback 仍被拒绝，包括 `ZrCore_Stack_GetValue`、`ZR_VALUE_FAST_SET`、
  `ZrLibrary_AotRuntime_Add`。RED/GREEN：guardrail contracts 先因新增 boundary helper 未分类失败
  1/4；实现后 focused guardrail contracts 4/0。补充验证通过 source contracts 19/0、guardrail 4/0、
  global 7/0、return 1/0、call 4/0、typed-call 4/0、dynamic deopt bridge smoke 2/0；`git diff --check`
  退出 0 且仅提示既有 LF/CRLF 规范化警告。规模：guardrail contract 159 / 139。备注：本切片
  只收紧 07-S5 acceptance 护栏，不改变 generated C 行为；07-S5 full typed ABI、inline structs、
  in/out writeback、完整 07-S5 acceptance 和 08-12 仍未完成。

- 2026-06-24 03:24:02 +08:00 · M1.5 / 07-S5 dynamic value-access deopt bridge surfacing ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：dynamic member/index value-access 边界 writer 新增 `deoptId` 入参，
  `backend_aot_c_function_body.c` 从 ExecIR 取对应 SemIR deopt id，generated C 在
  `GetMember` / `SetMember` / `GetMemberSlot` / `SetMemberSlot` / `GetByIndex` /
  `SetByIndex` helper 前输出 `zr_aot_value_dynamic_deopt_bridge deopt=...` marker 并调用
  `ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(...)`。dynamic call bridge 复用同一校验 helper，
  成员/索引实际访问仍走原 runtime helper。RED/GREEN：global contracts 先因缺失 value-access
  deopt bridge 形状失败 1/7，实现后 focused global contracts 7/0、dynamic deopt bridge smoke 2/0。
  补充验证通过 source contracts 19/0、global contracts 7/0、global shared-library smoke 9/0、
  SemIR dynamic member deopt 1/0、SemIR dynamic index deopt 1/0；`git diff --check` 退出 0
  且仅提示既有 LF/CRLF 规范化警告。规模：value-access boundary source 201 / 180，
  dynamic bridge smoke 337 / 288，global contract 716 / 666。仍未完成：07-S5 full typed ABI、
  inline structs、in/out writeback、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 02:53:01 +08:00 · M1.5 / 07-S5 dynamic/deopt bridge surfacing ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：AOT C dynamic call 边界 writer 新增 `deoptId` 入参，generic
  SemIR dynamic call 从 ExecIR 取 `deoptId` 并生成 `zr_aot_dynamic_deopt_bridge deopt=...`
  marker 与 `ZrLibrary_AotRuntime_CallDynamicDeoptBridge(...)`；runtime helper 校验可见
  SemIR deopt table 并委托现有 `CallStackValue`，普通 generic direct-call smoke 仍保持
  `CallStackValue` 路径。新增独立 dynamic deopt bridge shared-library smoke，避免继续压大
  call shared-library smoke。RED/GREEN：call contracts 先因缺失 deopt bridge 形状失败 1/4，
  实现后 focused call contracts 4/0。补充验证通过 call shared-library smoke 5/0、
  dynamic deopt bridge smoke 1/0、source contracts 19/0、global contracts 7/0、SemIR dynamic
  call deopt 1/0；`git diff --check` 对本切片相关 tracked 文件通过。规模：call-boundary source
  233 / 223，call smoke 898，新 bridge smoke 189。仍未完成：07-S5 full typed ABI、
  inline structs、in/out writeback、broader dynamic value access hardening、完整 07-S5 acceptance
  和 08-12。

- 2026-06-24 02:04:55 +08:00 · M1.5 / 07-S5 value-access boundary writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_value_access_boundaries.c`，从 `backend_aot_c_lowering_values.c`
  迁出 unsupported meta/dynamic value-access 边界 writer 和六个动态 member/index runtime boundary writer；
  原 value lowering 文件继续保留 `TO_STRING`、常量/物化、所有权和其他值 lowering。RED/GREEN：
  global contracts 先读取缺失的 value-access boundary 模块并按预期 2/7 `Expected Non-NULL`，
  拆分后 focused global contracts 7/0。补充验证通过 global shared-library smoke 9/0、source contracts 19/0；
  `zr_vm_parser_shared` 构建通过并确认新对象编入 parser shared。规模：value lowering 主文件降至
  839 physical / 755 non-empty lines，新 value-access boundary source 165 / 147，global contract 703 / 653。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  broader dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 01:47:59 +08:00 · M1.5 / 07-S5 call-boundary writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_call_boundaries.c`，从 `backend_aot_c_lowering_calls.c`
  迁出 generic/dynamic `CallStackValue` 边界 writer、static resolved `CallStaticDirect` 边界 writer
  和 i64/bool/u64/f64 scalar-local call-result sync 发射；原 lowering 文件继续保留 unsupported meta call
  与具体 typed direct-call scalar writer。RED/GREEN：合约先读取缺失的 call-boundary 模块并按预期 3/4
  `Expected Non-NULL`，迁出后 focused call contracts 4/0。补充相关 AOT 合约/烟测分组通过 source 19/0、
  call 4/0、typed call 4/0、constant 5/0、global 7/0、logical 4/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared-library 8/0、call smoke 5/0、typed direct-call 5/0、bool 28/0、u64 25/0；
  `zr_vm_parser_shared` 构建通过并确认新对象编入 parser shared。规模：call lowering 主文件降至
  481 physical / 455 non-empty lines，新 call-boundary source 185 / 177，call contract 424 / 386。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 01:30:26 +08:00 · M1.5 / 07-S5 typed-return route split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_return.{h,c}`，从 `backend_aot_c_function_body.c`
  迁出 i64/bool/u64/f64 scalar typed-return route 判定和 typed native-to-VM return writer 调用；
  `FUNCTION_RETURN` 现在委托 `backend_aot_try_write_c_typed_return(...)`，generic/value-SemIR
  fallback 与普通 direct return boundary 仍保留在函数体。RED/GREEN：合约先读取缺失的 typed-return
  route 模块并按预期 `Expected Non-NULL`，迁出后 focused return contracts 1/0。补充相关 AOT
  合约/烟测分组通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、global 7/0、logical 4/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、shared-library 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 28/0、u64 25/0；`zr_vm_parser_shared` 构建通过并确认新 typed-return 对象编入 parser shared。
  规模：function body 2082 physical / 2034 non-empty lines，新 typed-return source 50 / 42，header 14 / 10。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 01:03:26 +08:00 · M1.5 / 07-S5 i64 typed-direct route proof split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_direct_i64_calls.{h,c}`，从 `backend_aot_c_typed_direct_calls.c`
  迁出 i64 no/one/two/three-arg typed-direct can-write proof；顶层 typed-direct 入口删除旧 function-table
  查表 helper 和 i64 静态 proof，仅保留统一调度、result sync 判定与 writer 调用。RED/GREEN：合约先读取缺失的
  i64 typed-direct route 模块并在 typed-call contracts 中按预期 `Expected Non-NULL`，迁出后 focused
  typed-call contracts 4/0、i64 smoke 5/0。补充相关 AOT 合约/烟测串联执行通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、global 7/0、logical 4/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  shared-library 8/0、call smoke 5/0、typed direct-call 5/0、bool 28/0、u64 25/0。规模：typed-direct
  主文件降至 467 physical / 436 non-empty lines，新 i64 route source 152 / 132，header 43 / 40。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  general typed-return ABI、dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 00:50:03 +08:00 · M1.5 / 07-S5 bool typed-direct route proof split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_direct_bool_calls.{h,c}`，从 `backend_aot_c_typed_direct_calls.c`
  迁出 bool no/one/two/three-arg typed-direct can-write proof 与 i64 参数到 bool 结果的 two-arg proof；
  顶层 typed-direct 入口只新增 bool route header include 并继续负责统一调度/writer 调用，保持 u64/f64
  已有拆分模式。RED/GREEN：合约先读取缺失的 bool typed-direct route 模块并在 typed-call contracts 中
  按预期 `Expected Non-NULL`，迁出后 focused typed-call contracts 4/0、bool smoke 28/0。补充相关
  AOT 合约/烟测串联执行通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、global 7/0、
  logical 4/0、frame setup 1/0、return 1/0、value SemIR 4/0、shared-library 8/0、call smoke 5/0、
  typed direct-call 5/0、bool 28/0、u64 25/0。规模：typed-direct 主文件降至 610 physical / 560
  non-empty lines，新 bool route source 189 / 165，header 53 / 50。仍未完成：07-S5 full typed ABI、
  inline structs、in/out writeback、deopt/dynamic bridges、general typed-return ABI、
  dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 00:32:08 +08:00 · M1.5 / 07-S5 bool call lowering writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_lowering_typed_bool_calls.c`，从 `backend_aot_c_lowering_calls.c`
  迁出 bool no/one/two/three-arg typed direct-call lowering writer 与 i64/u64/f64 参数到 bool
  结果的 two-arg direct-call lowering writer；公开 writer 名称不变。RED/GREEN：合约先读取缺失的
  bool lowering 模块并在 typed-call contracts 中按预期 `Expected Non-NULL`，迁出后 focused typed-call
  contracts 4/0、bool smoke 28/0。补充相关 AOT 合约/烟测串联执行通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、global 7/0、logical 4/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared-library 8/0、call smoke 5/0、typed direct-call 5/0、bool 28/0、u64 25/0。
  规模：call lowering 主文件降至 664 physical / 630 non-empty lines，新 bool lowering source 272 / 257。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  general typed-return ABI、dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 00:08:20 +08:00 · M1.5 / 07-S5 bool two-arg thunk writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_bool_two_arg_thunks.{h,c}`，迁出 plain bool/bool
  two-arg equal/not-equal/logical-and/logical-or recognizer、can-emit、forward declaration writer
  与 definition writer；`backend_aot_c_typed_bool_thunks.c` 仅委托二参 bool/bool 写入并继续保留
  no/one-arg、三参委托和 i64/u64/f64 比较 writer。RED/GREEN：合约先读取缺失的 bool two-arg 模块并在
  typed-call contracts 中按预期 `Expected Non-NULL`，迁出后 focused typed-call contracts 4/0、bool smoke 28/0、
  u64 smoke 25/0；删除重复 null guard 后重跑 focused typed-call contracts 4/0 与 bool smoke 28/0。
  补充 AOT 合约/烟测组在重复 guard 清理前全部通过；一次清理后的整组重跑因 f64 generated shared-library
  编译超过工具超时窗口未计入证据。规模：bool thunk 主文件降至 673 physical / 587 non-empty lines，
  新二参 source 298 / 257，header 12 / 8。仍未完成：07-S5 full typed ABI、inline structs、in/out
  writeback、deopt/dynamic bridges、general typed-return ABI、dynamic value access hardening、
  完整 07-S5 acceptance 和 08-12。

- 2026-06-23 23:32:28 +08:00 · M1.5 / 07-S5 u64 three-arg thunk writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_u64_three_arg_thunks.{h,c}`，迁出 u64 three-arg
  can-emit、forward declaration writer 和三参 thunk definition writer；`backend_aot_c_typed_u64_thunks.c`
  仅保留 no/one/two-arg writer 与总入口委托。RED/GREEN：合约先读取缺失三参模块并在 typed-call
  contracts 中按预期 `Expected Non-NULL`，迁出后 focused typed-call contracts 4/0、u64 smoke 25/0、
  bool smoke 28/0。补充 AOT 合约/烟测组全部通过；验证过程中的并行构建超时和旧 target 名停顿均为
  执行方式问题，串行/更正名称后通过。规模：u64 thunk 主文件降至 832 physical / 748 non-empty lines，
  新三参 source 126 / 109，header 12 / 8。仍未完成：07-S5 full typed ABI、inline structs、
  in/out writeback、deopt/dynamic bridges、general typed-return ABI、dynamic value access hardening、
  完整 07-S5 acceptance 和 08-12。

- 2026-06-23 22:51:07 +08:00 · M1.5 / 07-S5 u64 typed-direct route split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_direct_u64_calls.{h,c}`，把 u64 no/one/two/three-arg
  typed-direct can-write proof 与 u64->bool two-arg route proof 从顶层 typed-direct router
  迁出；`backend_aot_c_typed_direct_calls.c` 仅保留调度和 writer 调用。RED/GREEN：合约先读取缺失的
  u64 direct-route 模块并在 typed-call contracts 中按预期 `Expected Non-NULL`，迁出后 focused
  typed-call contracts 4/0、u64 smoke 25/0、bool smoke 28/0。补充 AOT 合约/烟测组全部通过，
  generic numeric smoke 初次执行是 stale 目标，重建后 1/0 通过。规模：typed-direct router 降至
  771 physical / 702 non-empty lines，新增 u64 direct module 189 / 165，header 53 / 50。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  general typed-return ABI、dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-23 22:20:33 +08:00 · M1.5 / 07-S5 typed-call contract file split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：把已达 1000 行边界的 typed-call 合约入口拆成 16 行汇总 main，并将 i64/bool/u64/f64
  四组 source-shape 断言迁入四个独立契约源；新增 cases header 提供用例原型，复用现有 support
  helper，`zr_vm_aot_c_typed_call_contracts_test` 目标名不变。RED/GREEN：先接入 CMake 后构建按预期
  失败于缺少 `test_aot_c_typed_call_i64_contracts.c`；补齐拆分文件和原型头后 focused GREEN 为
  typed call contracts 4/0、bool smoke 28/0。补充验证：contracts 组通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、
  call 5/0、typed direct-call 5/0、i64 three-arg 8/0、bool 28/0、u64 25/0、f64 19/0、
  arithmetic 7/0、bitwise 6/0、global 9/0、logical 4/0、power 1/0、value-type 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-typed-call-contract-file-split.md`。
  备注：本切片不新增 AOT 生成行为，只解除后续 typed-call/ABI 合约扩展的大文件风险；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 21:45:12 +08:00 · M1.5 / 07-S5 static bool three-arg logical-or
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool 三参 typed thunk 识别 `arg0 || arg1 || arg2`，覆盖 compact
  OR 和 12 指令短路 OR 形态，writer 发出三 bool 参数 `TZrBool` thunk 并返回
  `(TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2)`；既有三参 bool direct-call 路径复用
  scalar-local 参数证明，继续拒绝 VM call/value fallback 和三参 sync marker。RED/GREEN：
  typed call contracts 先因缺三参 OR recognizer 失败；bool smoke 新增 `either3(false, false, true)`
  后缺生成 thunk；补实现后 focused GREEN 为 typed call contracts 4/0、bool smoke 28/0。补充验证：
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；
  shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、
  arithmetic 7/0、bitwise 6/0、bool 28/0、u64 25/0、f64 19/0、global 9/0、logical 4/0、
  power 1/0、value-type 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-three-arg-logical-or-typed-thunk.md`。
  备注：typed call contract 已到 1000 行边界，后续扩展应优先拆分；仅完成 bool 三参 `||` 直调，
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 21:11:23 +08:00 · M1.5 / 07-S5 static bool three-arg logical-and
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool 三参 typed thunk 识别三 bool 参数 `arg0 && arg1 && arg2`
  的窄返回形态，writer 生成 `TZrBool` 三参 thunk，caller 生成 `zr_aot_static_bool_three_arg_direct_call`
  并直接赋值 bool scalar local，继续拒绝 VM call/value fallback 和三参 sync marker。补充完成
  `GET_CLOSURE` / `GETUPVAL` callable provenance、可解析 `CREATE_CLOSURE` helper 发射，以及
  `CopyConstant` / `CreateClosure` 在 `frame.recordHandle` 省略时按函数反查 AOT module record。
  RED/GREEN：typed call contracts 先因缺 `backend_aot_c_can_emit_typed_bool_three_arg_thunk(`
  失败；中间 smoke 暴露 closure materialization frame-record fallback 缺口；补实现后 focused GREEN 为
  constant contracts 5/0、frame setup 1/0、typed call contracts 4/0、bool smoke 27/0。补充验证：
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；
  shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、
  arithmetic 7/0、bitwise 6/0、bool 27/0、u64 25/0、f64 19/0、global 9/0、logical 4/0、
  power 1/0、value-type 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-three-arg-logical-and-typed-thunk.md`。
  备注：仅完成 bool 三参 `&&` 直调及必要闭包物化 fallback；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 19:45:07 +08:00 · M1.5 / 07-S5 static u64 three-arg modulo
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 三参 typed thunk 识别有序 `MOD_UNSIGNED -> MOD_UNSIGNED ->
  FUNCTION_RETURN` 的 `arg0 % arg1 % arg2` 形态，can-emit gate 纳入 modulo，writer 发出
  `zr_aot_arg1 == 0u || zr_aot_arg2 == 0u` 防护和 `generated AOT unsigned three-arg modulo by zero`
  运行时错误，正常路径直接返回 `TZrUInt64` C 表达式。u64 shared-library smoke 新增
  `remainder3(92, 50, 43)`，继续拒绝 VM call/value fallback 和三参 sync marker。RED/GREEN：
  typed call contracts 先因缺 `generated AOT unsigned three-arg modulo by zero` 失败；补实现后
  focused GREEN 为 typed call contracts 4/0、u64 smoke 25/0。补充验证：拆批验证通过；contracts
  组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、
  power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library smokes
  通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、arithmetic 7/0、
  bitwise 6/0、bool 26/0、u64 25/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-modulo-typed-thunk.md`。
  备注：仅完成 u64 三参取模直调；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 19:18:23 +08:00 · M1.5 / 07-S5 static u64 three-arg divide
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 三参 typed thunk 识别有序 `DIV_UNSIGNED -> DIV_UNSIGNED ->
  FUNCTION_RETURN` 的 `arg0 / arg1 / arg2` 形态，can-emit gate 纳入 divide，writer 发出
  `zr_aot_arg1 == 0u || zr_aot_arg2 == 0u` 防护和 `generated AOT unsigned three-arg divide by zero`
  运行时错误，正常路径直接返回 `TZrUInt64` C 表达式。u64 shared-library smoke 新增
  `quotient3(168, 2, 2)`，继续拒绝 VM call/value fallback 和三参 sync marker。RED/GREEN：
  typed call contracts 先因缺 `generated AOT unsigned three-arg divide by zero` 失败；补实现后
  focused GREEN 为 typed call contracts 4/0、u64 smoke 24/0。补充验证：首次全链构建+测试命令
  304s 超时且无失败细节，随后拆批验证通过；contracts 组通过 source 19/0、call 4/0、typed call 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、
  i64 three-arg 8/0、arithmetic 7/0、bitwise 6/0、bool 26/0、u64 24/0、f64 19/0、global 9/0、
  logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-divide-typed-thunk.md`。
  备注：仅完成 u64 三参除法直调；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 18:55:27 +08:00 · M1.5 / 07-S5 static i64 three-arg modulo
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64 三参 typed thunk 识别有序 `MOD_SIGNED -> MOD_SIGNED ->
  FUNCTION_RETURN` 的 `arg0 % arg1 % arg2` 形态，can-emit gate 纳入 modulo，writer 发出
  `zr_aot_arg1 == 0 || zr_aot_arg2 == 0` 防护和 `generated AOT signed three-arg modulo by zero`
  运行时错误，正常路径直接返回 `TZrInt64` C 表达式。i64 three-arg shared-library smoke 新增
  `remainder3(92, 50, 43)`，继续拒绝 VM call/value fallback 和三参 sync marker。RED/GREEN：
  typed call contracts 先因缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(` 失败；
  补实现后 focused GREEN 为 typed call contracts 4/0、i64 three-arg smoke 8/0。补充验证：
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；
  shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、
  arithmetic 7/0、bitwise 6/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、
  power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-modulo-typed-thunk.md`。
  备注：仅完成 i64 三参取模直调；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 18:35:09 +08:00 · M1.5 / 07-S5 static i64 three-arg divide
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64 三参 typed thunk 识别有序 `DIV_SIGNED -> DIV_SIGNED ->
  FUNCTION_RETURN` 的 `arg0 / arg1 / arg2` 形态，can-emit gate 纳入 divide，writer 发出
  `zr_aot_arg1 == 0 || zr_aot_arg2 == 0` 防护和 `generated AOT signed three-arg divide by zero`
  运行时错误，正常路径直接返回 `TZrInt64` C 表达式。i64 three-arg shared-library smoke 新增
  `quotient3(64, 4, 2)`，继续拒绝 VM call/value fallback 和三参 sync marker。RED/GREEN：
  typed call contracts 先因缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(` 失败；
  补实现后 focused GREEN 为 typed call contracts 4/0、i64 three-arg smoke 7/0。补充验证：
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；
  shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 7/0、
  arithmetic 7/0、bitwise 6/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、
  power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-divide-typed-thunk.md`。
  备注：仅完成 i64 三参除法直调；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 18:12:47 +08:00 · M1.5 / 07-S5 i64 no/one-arg thunk shape split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：将 i64 no-arg constant-return 与 one-arg identity/negate/bitwise-not/
  bitwise-const/add-const/subtract-const/multiply-const recognizer 从
  `backend_aot_c_typed_i64_thunks.c` 迁入 `backend_aot_c_typed_i64_thunk_shapes.{h,c}`，
  writer 继续只负责 can-emit 与 thunk definition 发射。RED/GREEN：typed call
  contract 先因 shape 文件缺 no-arg recognizer 失败；迁移后 first smoke 暴露缺
  `backend_aot_c_get_constant_value()` 声明导致隐式声明/崩溃，补声明来源后 focused GREEN
  为 typed call contracts 4/0、typed direct-call 5/0、i64 three-arg 6/0、arithmetic 7/0、
  bitwise 6/0。补充验证：contracts 组通过 source 19/0、call 4/0、typed call 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、
  call 5/0、typed direct-call 5/0、i64 three-arg 6/0、arithmetic 7/0、bitwise 6/0、
  bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-i64-no-one-arg-shape-split.md`。备注：
  行为保持的支撑拆分；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 17:43:06 +08:00 · M1.5 / 07-S5 i64 two-arg thunk shape split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：将 i64 两参 typed thunk 的 add/subtract/multiply/divide/modulo/AND/OR/XOR
  recognizer 从 `backend_aot_c_typed_i64_thunks.c` 迁入
  `backend_aot_c_typed_i64_thunk_shapes.{h,c}`，writer 继续只负责 can-emit 与 thunk
  definition 发射。RED/GREEN：typed call contract 先因 shape 文件缺两参 recognizer
  失败；迁移后 focused GREEN 为 typed call contracts 4/0、i64 three-arg smoke 6/0、
  arithmetic smoke 7/0。补充验证：contracts 组通过 source 19/0、call 4/0、typed call 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、
  call 5/0、typed direct-call 5/0、i64 three-arg 6/0、arithmetic 7/0、bool 26/0、
  u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-i64-two-arg-shape-split.md`。备注：
  行为保持的支撑拆分；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 17:11:47 +08:00 · M1.5 / 07-S5 static i64 two-arg modulo typed thunk
  direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：先把 arithmetic shared-library smoke 拆为共享 runner + case 数据，行为保持
  6/0；随后 i64 两参 typed thunk 识别 `MOD_SIGNED -> FUNCTION_RETURN` 的
  `return left % right` 窄形态，生成带 `generated AOT signed modulo by zero`
  防护的 `TZrInt64` 两参 thunk，并让 arithmetic smoke 的 `remainder(left, right)` 走
  `zr_aot_static_i64_two_arg_direct_call`，拒绝 `CallStaticDirect` / `CallStackValue`
  fallback 和 unnecessary typed-destination sync。RED/GREEN：typed call contract 先缺
  modulo recognizer；新增 arithmetic smoke 再缺 generated modulo thunk；focused GREEN 为
  typed call contracts 4/0、arithmetic shared-library smoke 7/0。补充验证：较宽 AOT
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float 1/0；shared-library smokes 拆批后通过 shared 8/0、call 5/0、typed direct-call 5/0、
  arithmetic 7/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-modulo-typed-thunk.md`。
  备注：仅完成 i64 两参 modulo 直调与必要 smoke harness 拆分；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 16:50:50 +08:00 · M1.5 / 07-S5 static i64 two-arg divide typed thunk
  direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：i64 两参 typed thunk 识别 `DIV_SIGNED -> FUNCTION_RETURN` 的
  `return left / right` 窄形态，生成带 `generated AOT signed divide by zero`
  防护的 `TZrInt64` 两参 thunk，并让 arithmetic smoke 的 `ratio(left, right)` 走
  `zr_aot_static_i64_two_arg_direct_call`，拒绝 `CallStaticDirect` / `CallStackValue`
  fallback 和 unnecessary typed-destination sync。RED/GREEN：typed call contract 先缺
  divide recognizer；新增 arithmetic smoke 再缺 generated divide thunk；focused GREEN 为
  typed call contracts 4/0、arithmetic shared-library smoke 6/0。补充验证：较宽 AOT
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float 1/0；shared-library smokes 拆批后通过 shared 8/0、call 5/0、typed direct-call 5/0、
  arithmetic 6/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-divide-typed-thunk.md`。
  备注：仅完成 i64 两参除法直调；i64 modulo parity、general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 16:30:13 +08:00 · M1.5 / 07-S5 i64/bool/u64/f64 no-arg local-constant
  typed direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64/bool/u64/f64 no-arg typed thunk recognizer 现在覆盖
  `var result = constant; return result;` 的本地常量返回形态，return-boundary smokes
  和 static numeric call smoke 均要求 typed thunk 直调并禁止 runtime call fallback；
  i64 no-arg smoke 也收紧为本地常量返回。RED/GREEN：bool return-boundary smoke 先因缺
  typed bool forward declaration 失败；i64 no-arg smoke 收紧后因缺 typed i64 forward
  declaration 失败；focused GREEN 为 i64/bool/u64/f64 typed direct-call shared-library
  smokes 5/0、26/0、23/0、19/0，call shared-library smoke 5/0。补充验证：较宽 AOT 组通过 contracts
  source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；
  shared-library smokes shared 8/0、call 5/0、typed direct-call 5/0、bool 26/0、
  u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-no-arg-local-constant-typed-direct-call.md`。
  备注：仅完成 no-arg local-constant return shape 收紧；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 15:35:25 +08:00 · M1.5 / 07-S5 bool/u64/f64 direct-return
  frame descriptor gates · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_frame_descriptor.c` 的
  `FUNCTION_RETURN` local-only 判定不再只认 i64 direct return，在非 export tail 下同时复用
  `backend_aot_c_scalar_locals_can_direct_return_bool_local()`、
  `backend_aot_c_scalar_locals_can_direct_return_u64_local()` 和
  `backend_aot_c_scalar_locals_can_direct_return_f64_local()`；其余需要 frame 的指令仍按
  既有保守判定保留 frame descriptor。RED/GREEN：frame setup contract 先因缺 bool
  direct-return descriptor gate 失败；补 bool/u64/f64 gates 后 focused GREEN 为 frame setup
  contracts 1/0、bool typed direct-call shared-library smoke 26/0、u64 smoke 23/0、f64 smoke
  19/0。补充验证：较宽 AOT 组通过 source 19/0、call 4/0、typed call 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float 1/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-bool-u64-f64-return-frame-descriptor-gates.md`。
  备注：仅完成 return-frame descriptor gate 对齐；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 15:17:34 +08:00 · M1.5 / 07-S5 static f64 native-to-VM return
  boundary helper · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：新增 `ZrLibrary_AotRuntime_ReturnF64()`，让可证明为
  `float` 返回类型的 f64 local return 直接通过 runtime helper 打包到 caller result；
  新增 `backend_aot_write_c_direct_return_f64_local()` 和 scalar-local proof gate，并在
  `FUNCTION_RETURN` 的 i64/bool/u64 fast path 后接入 f64 fast path。RED/GREEN：return
  contracts 先因缺 f64 writer prototype 失败；新增普通 static call 进入 f64 callee
  的 shared-library smoke 后 focused GREEN 为 return contracts 1/0、f64 typed direct-call
  shared-library smoke 19/0。补充验证：较宽 AOT 组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、logical 4/0、power
  2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0、shared
  8/0、call smoke 5/0、typed direct-call 5/0、bool 26/0、u64 23/0、f64 19/0、
  global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-return-boundary-helper.md`。
  备注：仅完成窄 f64 native-to-VM return packing；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 14:59:10 +08:00 · M1.5 / 07-S5 static bool native-to-VM return
  boundary helper · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：新增 `ZrLibrary_AotRuntime_ReturnBool()`，让可证明为 `bool`
  返回类型的 bool local return 直接通过 runtime helper 打包到 caller result；新增
  `backend_aot_write_c_direct_return_bool_local()` 和 scalar-local proof gate，并在
  `FUNCTION_RETURN` 的 i64 fast path 后、u64 fast path 前接入 bool fast path。
  RED/GREEN：return contracts 先因缺 bool writer prototype 失败；新增普通 static call
  进入 bool callee 的 shared-library smoke 后 focused GREEN 为 return contracts 1/0、
  bool typed direct-call shared-library smoke 26/0。补充验证：较宽 AOT 组通过 source
  19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、global
  7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float contracts 1/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool
  26/0、u64 23/0、f64 18/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-return-boundary-helper.md`。
  备注：仅完成窄 bool native-to-VM return packing；f64/general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 14:26:38 +08:00 · M1.5 / 07-S5 static u64 native-to-VM return
  boundary helper · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：新增 `ZrLibrary_AotRuntime_ReturnU64()`，让可证明为 `uint`
  返回类型的 u64 local return 直接通过 runtime helper 打包到 caller result；新增
  `backend_aot_write_c_direct_return_u64_local()` 和 scalar-local proof gate，并在
  `FUNCTION_RETURN` 的 i64 fast path 后接入 u64 fast path。RED/GREEN：return contracts
  先因缺 u64 writer prototype 失败；初版 proof 过宽导致 int-return u64 smoke 误走 u64
  return，收窄到 callable return type U64 后 focused GREEN 为 return contracts 1/0、
  u64 typed direct-call shared-library smoke 23/0。补充验证（2026-06-23 14:42:02 +08:00）：
  较宽 AOT 组通过 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0、shared 8/0、call smoke 5/0、typed direct-call
  5/0、bool 25/0、u64 23/0、f64 18/0、global 9/0、logical 4/0、power 1/0；u64 smoke
  support 结果断言改为按实际 signed/unsigned value type 读取。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-return-boundary-helper.md`。
  备注：仅完成窄 u64 native-to-VM return packing；bool/f64/general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 13:49:07 +08:00 · M1.5 / 07-S5 dynamic member/index runtime
  boundary helpers · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：AOT 函数体的 `GET_MEMBER`、`GET_MEMBER_SLOT`、
  `GET_BY_INDEX`、`SET_MEMBER`、`SET_MEMBER_SLOT`、`SET_BY_INDEX` 现在生成
  已存在的 runtime helper 边界调用，不再统一落到 unsupported dynamic value access；
  typed-call 合约共享 helper 已抽出到 `aot_c_typed_call_contract_support.h`，主合约文件
  回落到 893 / 872。RED/GREEN：global contracts 先以 7 tests / 1 failure 锁定缺
  direct writer；补实现后 focused GREEN 为 global contracts 7/0、global smoke 9/0、
  typed call contracts 4/0。测试结果：较宽 AOT 组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、logical 4/0、
  power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0；
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 25/0、u64 22/0、f64
  18/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-dynamic-member-index-runtime-boundaries.md`。
  备注：dynamic member/index helper 子项推进完成；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general typed-return ABI、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 13:05:52 +08:00 · M1.5 / 07-S5 static f64 two-arg
  comparison bool typed thunk family · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：在 f64 `<` bool direct-call 基础上补齐两
  f64 参数返回 bool 的比较族，`backend_aot_c_typed_bool_thunks.c` 现在识别并发出
  `<`、`<=`、`==`、`!=`、`>`、`>=` 的 `TZrBool` typed thunk；调用侧复用已有
  `zr_aot_static_f64_bool_two_arg_direct_call`，以 `zr_aot_f*` 参数调用并按需同步 bool
  栈槽。RED/GREEN：先用 `<=` case 得到 contracts 缺 less-equal recognizer、bool
  smoke 21 tests / 1 failure；补齐后 focused GREEN 4/0 + 21/0。再补 `==`、`!=`、
  `>`、`>=` 得到 contracts 缺 equal recognizer、bool smoke 25 tests / 4 failures；
  补实现后 focused GREEN 为 typed call contracts 4/0、bool smoke 25/0。测试结果：
  WSL GCC debug 宽构建通过；合约组 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame
  setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0；smoke 组 900s 重跑通过：
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、bool 25/0、
  u64 22/0、f64 18/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric 1/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-comparison-bool-typed-thunk.md`。
  备注：只覆盖窄 f64 两参 bool 比较族；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、dynamic value access helpers、general typed-return ABI、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 12:45:38 +08:00 · M1.5 / 07-S5 static f64 two-arg less bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool typed thunk 现在识别两 f64 参数
  `LOGICAL_LESS_FLOAT` + `FUNCTION_RETURN` 的 `arg0 < arg1` 窄形态，发出
  `TZrFloat64` 两参 bool thunk；调用侧新增 f64 参数、bool 结果的 static direct-call
  lowering；新增 `backend_aot_c_typed_direct_f64_calls.{h,c}` 拆出 f64 route proof，
  让 `backend_aot_c_typed_direct_calls.c` 回落到 868 / 785 行。RED/GREEN：RED 为
  typed call contracts 缺 f64 bool can-emit gate，bool smoke 新 case 为 `Expected Non-NULL`；
  focused GREEN 为 typed call contracts 4/0、bool smoke 20/0；拆分后 focused GREEN
  还覆盖 f64 smoke 18/0。测试结果：WSL GCC debug 宽构建通过；合约组 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 6/0、bool 20/0、u64 22/0、f64 18/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-less-bool-typed-thunk.md`。
  备注：只覆盖窄 f64 `<` 返回 bool direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general typed-return ABI、dynamic value access
  helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 12:10:24 +08:00 · M1.5 / 07-S5 f64 three-arg shape split
  支撑切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_f64_three_arg_shapes.{h,c}`，把 f64 三参数
  add/subtract/multiply/divide/modulo shape 识别从基础 f64 shape source 中拆出；
  typed call contracts 分别检查基础 f64 shape source 与三参数 f64 shape source。
  RED/GREEN：RED 为 typed call contracts 读取新三参 shape source 时 `Expected Non-NULL`；
  补实现后 focused GREEN 为 typed call contracts 4/0、f64 smoke 18/0。测试结果：
  WSL GCC debug 目标构建通过并自动重新配置新增 glob source；合约组 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 6/0、bool 19/0、u64 22/0、f64 18/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-f64-three-arg-shape-split.md`。
  备注：行为保持的 shape ownership 拆分；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg typed-return ABI、dynamic value access
  helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:48:03 +08:00 · M1.5 / 07-S5 static f64 three-arg modulo
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_modulo_return()`，识别三个 f64 参数
  `arg0 % arg1 % arg2` 的两条有序 `MOD_FLOAT` + `FUNCTION_RETURN` 窄形态；f64
  三参 can-emit gate 纳入 modulo，thunk writer 发出双分母 `0.0` 保护并在正常路径返回
  `return (TZrFloat64)fmod(fmod(zr_aot_arg0, zr_aot_arg1), zr_aot_arg2);`。
  调用侧复用已有 `zr_aot_static_f64_three_arg_direct_call` proof/writer 与
  `nativeDouble` sync path。RED/GREEN：RED 为 typed call contracts 缺 f64 三参 modulo
  writer 文本，新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed
  call contracts 4/0、f64 smoke 18/0。测试结果：WSL GCC debug 目标构建通过；
  合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 22/0、f64 18/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-modulo-typed-thunk.md`。
  备注：只覆盖窄 f64 三参 modulo typed direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg typed-return ABI、dynamic value
  access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:38:58 +08:00 · M1.5 / 07-S5 static f64 three-arg divide
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return()`，识别三个 f64 参数
  `arg0 / arg1 / arg2` 的两条有序 `DIV_FLOAT` + `FUNCTION_RETURN` 窄形态；f64
  三参 can-emit gate 纳入 divide，thunk writer 发出双分母 `0.0` 保护并在正常路径返回
  `return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_f64_three_arg_direct_call` proof/writer 与 `nativeDouble`
  sync path。RED/GREEN：RED 为 typed call contracts 缺 f64 三参 divide writer
  文本，新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed
  call contracts 4/0、f64 smoke 17/0。测试结果：WSL GCC debug 目标构建通过；
  合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 22/0、f64 17/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-divide-typed-thunk.md`。
  备注：只覆盖窄 f64 三参 divide typed direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg typed-return ABI、dynamic value
  access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:29:03 +08:00 · M1.5 / 07-S5 static f64 three-arg subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_subtract_return()`，识别三个 f64 参数
  `arg0 - arg1 - arg2` 的两条有序 `SUB_FLOAT` + `FUNCTION_RETURN` 窄形态；f64
  三参 can-emit gate 纳入 subtract，thunk writer 发出
  `return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_f64_three_arg_direct_call` proof/writer 与 `nativeDouble`
  sync path。RED/GREEN：RED 为 typed call contracts 缺 f64 三参 subtract return
  文本，新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed
  call contracts 4/0、f64 smoke 16/0。测试结果：WSL GCC debug 目标构建通过；
  合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 22/0、f64 16/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-subtract-typed-thunk.md`。
  备注：只覆盖窄 f64 三参 subtract typed direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg typed-return ABI、dynamic value
  access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:17:58 +08:00 · M1.5 / 07-S5 static f64 three-arg multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_multiply_return()`，识别三个 f64 参数
  `arg0 * arg1 * arg2` 的两条 `MUL_FLOAT` + `FUNCTION_RETURN` 窄形态；f64 三参
  can-emit gate 纳入 multiply，thunk writer 发出
  `return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_f64_three_arg_direct_call` proof/writer 与 `nativeDouble`
  sync path。RED/GREEN：RED 为 typed call contracts 缺 f64 三参 multiply return
  文本，新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed
  call contracts 4/0、f64 smoke 15/0。测试结果：WSL GCC debug 目标构建通过；
  合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 22/0、f64 15/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-multiply-typed-thunk.md`。
  备注：只覆盖窄 f64 三参 multiply typed direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg typed-return ABI、dynamic value
  access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:07:26 +08:00 · M1.5 / 07-S5 f64 thunk shape split
  支撑切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_f64_thunk_shapes.{h,c}`，把 f64 shape 识别从
  `backend_aot_c_typed_f64_thunks.c` 抽出；f64 thunk writer 只保留 can-emit、
  forward declarations 和 definition writer；typed call contracts 分别检查 f64 writer 与
  shape source。RED/GREEN：RED 为新 shape source 缺失导致 `Expected Non-NULL`；
  focused GREEN 为 typed call contracts 4/0、f64 smoke 14/0。测试结果：WSL GCC debug
  目标构建通过并自动重新配置新增 glob source；合约组 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0；共享库烟测组
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、
  u64 22/0、f64 14/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：`tests/acceptance/2026-06-23-aot-m1-5-f64-thunk-shape-split.md`。
  备注：行为保持支撑拆分；inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  general multi-arg typed-return ABI、dynamic value access helpers、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 10:54:32 +08:00 · M1.5 / 07-S5 static f64 three-arg add
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunks.c` 新增
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return()`，识别三个 f64 参数
  `arg0 + arg1 + arg2` 的两条 `ADD_FLOAT` + `FUNCTION_RETURN` 窄形态；
  f64 can-emit gate、forward decl 和 thunk definition writer 覆盖三参签名并直接返回
  `return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。
  `backend_aot_c_typed_direct_calls.c` 新增 f64 三参 route proof；
  `backend_aot_c_lowering_calls.c` / `backend_aot_c_emitter.h` 新增
  `zr_aot_static_f64_three_arg_direct_call` writer 与 `nativeDouble` sync path。
  RED/GREEN：RED 为 typed call contracts 缺 f64 三参 can-emit declaration，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  f64 smoke 14/0。测试结果：WSL GCC debug 全量构建通过；合约组 source 19/0、call
  contracts 4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、logical
  4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0；
  共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、
  bool 19/0、u64 22/0、f64 14/0、typed arithmetic 5/0、typed bitwise 6/0、
  value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-add-typed-thunk.md`。
  备注：只覆盖窄 f64 三参 add typed direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg typed-return ABI、dynamic value
  access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 10:30:14 +08:00 · M1.5 / 07-S5 static u64 two-arg modulo
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_modulo_return()`，识别两 unsigned 参数
  `arg0 % arg1` 的 `MOD_UNSIGNED` + `FUNCTION_RETURN` 窄形态；u64 两参 can-emit gate
  纳入 modulo，thunk writer 发出带 `ZR_UNLIKELY(zr_aot_arg1 == 0u)` 的取模除零保护，
  除零时调用 `ZrCore_Debug_RunError(state, "generated AOT unsigned modulo by zero")`
  并 defensive 返回 `(TZrUInt64)0`，正常路径返回
  `return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1);`。调用侧复用已有
  `zr_aot_static_u64_two_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 unsigned modulo guard 文本，新增 smoke
  失败为 `Expected Non-NULL`；补实现并对 generator 源码中的 `%%` 转义修正契约后
  focused GREEN 为 typed call contracts 4/0、u64 smoke 22/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup
  1/0、return 1/0、value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、
  call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 22/0、
  f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-modulo-typed-thunk.md`。
  备注：u64 两参 divide/modulo 与生成除零保护已覆盖；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 10:20:11 +08:00 · M1.5 / 07-S5 static u64 two-arg divide
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_divide_return()`，识别两 unsigned 参数
  `arg0 / arg1` 的 `DIV_UNSIGNED` + `FUNCTION_RETURN` 窄形态；u64 两参 can-emit gate
  纳入 divide，thunk writer 发出带 `ZR_UNLIKELY(zr_aot_arg1 == 0u)` 的除零保护，
  除零时调用 `ZrCore_Debug_RunError(state, "generated AOT unsigned divide by zero")`
  并 defensive 返回 `(TZrUInt64)0`，正常路径返回
  `return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1);`。调用侧复用已有
  `zr_aot_static_u64_two_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：最终 RED 为 typed call contracts 缺 unsigned divide guard 文本，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  u64 smoke 21/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call
  5/0、i64 three-arg 6/0、bool 19/0、u64 21/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-divide-typed-thunk.md`。
  备注：u64 两参 divide 与生成除零保护已覆盖；modulo、inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 10:04:13 +08:00 · M1.5 / 07-S5 static u64 three-arg bitwise-xor
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return()` 和 bitwise-xor
  operand reader，复用三参 u64 binary-return helper；u64 三参 can-emit gate 现在覆盖
  add/multiply/subtract/bitwise-and/bitwise-or/bitwise-xor，thunk writer 发出
  `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_u64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 bitwise-xor 三参 return 文本，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  u64 smoke 20/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call
  5/0、i64 three-arg 6/0、bool 19/0、u64 20/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-bitwise-xor-typed-thunk.md`。
  备注：三参 u64 add/multiply/subtract/bitwise-and/bitwise-or/bitwise-xor 窄形态已覆盖；
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg
  returns、dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 09:55:55 +08:00 · M1.5 / 07-S5 static u64 three-arg bitwise-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return()` 和 bitwise-or
  operand reader，复用三参 u64 binary-return helper；u64 三参 can-emit gate 现在覆盖
  add/multiply/subtract/bitwise-and/bitwise-or，thunk writer 发出
  `return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_u64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 bitwise-or 三参 return 文本，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  u64 smoke 19/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call
  5/0、i64 three-arg 6/0、bool 19/0、u64 19/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-bitwise-or-typed-thunk.md`。
  备注：三参 u64 add/multiply/subtract/bitwise-and/bitwise-or 窄形态已覆盖；u64 三参
  XOR、inline structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg
  returns、dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 09:46:43 +08:00 · M1.5 / 07-S5 static u64 three-arg bitwise-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return()` 和 bitwise-and
  operand reader，复用三参 u64 binary-return helper；u64 三参 can-emit gate 现在覆盖
  add/multiply/subtract/bitwise-and，thunk writer 发出
  `return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_u64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 bitwise-and 三参 return 文本，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  u64 smoke 18/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组第一次整串运行在 124s 超时且无失败输出，拆分后
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、
  u64 18/0、f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-bitwise-and-typed-thunk.md`。
  备注：三参 u64 add/multiply/subtract/bitwise-and 窄形态已覆盖；u64 三参 OR/XOR、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、
  dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 09:33:50 +08:00 · M1.5 / 07-S5 static u64 three-arg subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return()` 和 subtract operand
  reader；三参 u64 helper 增加 `preserveOperandOrder`，确保 subtract 只接受
  `(arg0 - arg1) - arg2`。u64 三参 can-emit gate 现在覆盖 add/multiply/subtract，
  thunk writer 发出 `return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`。
  调用侧复用已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer 与 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺 subtract 三参 return 文本，
  新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call
  contracts 4/0、u64 smoke 17/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建
  通过；合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 17/0、f64 13/0、typed
  arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic
  numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-subtract-typed-thunk.md`。
  备注：三参 u64 add/multiply/subtract 窄形态已覆盖；u64 三参 bitwise、inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、
  dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 09:24:25 +08:00 · M1.5 / 07-S5 static u64 three-arg multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return()` 和 multiply operand
  reader，复用三参 u64 binary-return helper；u64 三参 can-emit gate 现在覆盖
  add/multiply，thunk writer 发出
  `return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_u64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 multiply 三参 return 文本，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  u64 smoke 16/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed
  direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 16/0、f64 13/0、typed
  arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic
  numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-multiply-typed-thunk.md`。
  备注：三参 u64 add/multiply 窄形态已覆盖；u64 三参 subtract/bitwise、inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、
  dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 09:13:42 +08:00 · M1.5 / 07-S5 static u64 three-arg add
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return()`、三参 u64 binary-return
  helper 和 add operand reader；`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  接入 can-emit，thunk writer 发出
  `return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。direct-call
  router 证明 destination 与三个 call-window arguments 都是已写入 u64 scalar
  locals，lowering 发出 `zr_aot_static_u64_three_arg_direct_call` 并保持
  scalar-only destination sync elision。RED/GREEN：RED 为 typed call contracts 缺
  三参 u64 can-emit 声明，新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused
  GREEN 为 typed call contracts 4/0、u64 smoke 15/0。测试结果：实际存在的较宽 WSL GCC
  AOT 目标构建通过；合约组 source 19/0、call contracts 4/0、typed call contracts
  4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、
  call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、bool 19/0、u64 15/0、
  f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke
  1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-add-typed-thunk.md`。
  备注：这是首个三参 u64 add 窄覆盖；u64 三参其他表达式、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、dynamic
  value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:52:55 +08:00 · M1.5 / 07-S5 static i64 three-arg subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return()` 和 subtract operand
  reader；三参 binary helper 增加 `preserveOperandOrder`，subtract 严格校验
  `(arg0 - arg1) - arg2`，不复用 commutative operand set。i64 三参 can-emit gate
  现在覆盖 add/subtract/multiply/bitwise-and/bitwise-or/bitwise-xor，thunk writer
  发出 `return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`，调用侧复用已有
  `zr_aot_static_i64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 subtract recognizer，新增 smoke 失败为
  `Expected Non-NULL`；初次实现暴露不存在的 `SUB_SIGNED_LOAD_STACK` opcode 约束后，
  收紧到 `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` 并 focused GREEN 为 typed call contracts
  4/0、i64 three-arg smoke 6/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；
  合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call
  5/0、i64 three-arg 6/0、bool 19/0、u64 14/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-subtract-typed-thunk.md`。
  备注：三参 i64 add/subtract/multiply/AND/OR/XOR 窄形态已覆盖；inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、dynamic
  value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:40:52 +08:00 · M1.5 / 07-S5 static i64 three-arg bitwise-xor
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return()` 和
  `backend_aot_c_try_read_i64_bitwise_xor_operands()`，复用三参 binary-return helper
  校验三参数 i64 metadata、两条 `BITWISE_XOR` 指令和 `FUNCTION_RETURN` result。
  i64 三参 can-emit gate 现在覆盖 add/multiply/bitwise-and/bitwise-or/bitwise-xor，
  thunk writer 发出 `return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`，
  调用侧复用已有 `zr_aot_static_i64_three_arg_direct_call` proof/writer 与
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺 bitwise-xor
  recognizer，新增 smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为
  typed call contracts 4/0、i64 three-arg smoke 5/0。测试结果：实际存在的较宽 WSL GCC
  AOT 目标构建通过；合约组 source 19/0、call contracts 4/0、typed call contracts
  4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float contracts 1/0；共享库烟测组 shared 8/0、
  call smoke 5/0、typed direct-call 5/0、i64 three-arg 5/0、bool 19/0、u64 14/0、
  f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke
  1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-bitwise-xor-typed-thunk.md`。
  备注：三参 i64 add/multiply/AND/OR/XOR 窄形态已覆盖；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general multi-arg returns、dynamic value access
  helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:32:29 +08:00 · M1.5 / 07-S5 static i64 three-arg bitwise-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return()` 和
  `backend_aot_c_try_read_i64_bitwise_or_operands()`，复用三参 binary-return helper
  校验三参数 i64 metadata、两条 `BITWISE_OR` 指令和 `FUNCTION_RETURN` result。
  i64 三参 can-emit gate 现在覆盖 add/multiply/bitwise-and/bitwise-or，thunk writer
  发出 `return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);`，调用侧复用
  已有 `zr_aot_static_i64_three_arg_direct_call` proof/writer 与 destination sync
  elision。RED/GREEN：RED 为 typed call contracts 缺 bitwise-or recognizer，新增
  smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts
  4/0、i64 three-arg smoke 4/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；
  合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed
  direct-call 5/0、i64 three-arg 4/0、bool 19/0、u64 14/0、f64 13/0、typed
  arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic
  numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-bitwise-or-typed-thunk.md`。
  备注：这是三参 i64 bitwise-or 窄覆盖；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:24:03 +08:00 · M1.5 / 07-S5 static i64 three-arg bitwise-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return()` 和
  `backend_aot_c_try_read_i64_bitwise_and_operands()`，复用三参 binary-return helper
  校验三参数 i64 metadata、两条 `BITWISE_AND` 指令和 `FUNCTION_RETURN` result。
  i64 三参 can-emit gate 现在覆盖 add/multiply/bitwise-and，thunk writer 发出
  `return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);`，调用侧复用已有
  `zr_aot_static_i64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 bitwise-and recognizer，新增 smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  i64 three-arg smoke 3/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 3/0、bool 19/0、u64 14/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-bitwise-and-typed-thunk.md`。
  备注：这是三参 i64 bitwise-and 窄覆盖；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:13:44 +08:00 · M1.5 / 07-S5 static i64 three-arg multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return()` 和 multiply operand
  reader，并用私有三参 binary-return helper 共享 add/multiply 的三参数 i64 metadata、
  两段二元指令和 return-result 校验。i64 三参 can-emit gate 现在覆盖 add/multiply，
  thunk writer 对 multiply callee 发出
  `return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`，调用侧复用已有
  `zr_aot_static_i64_three_arg_direct_call` proof/writer 与 destination sync elision。
  RED/GREEN：RED 为 typed call contracts 缺 multiply recognizer，新增 multiply smoke
  失败为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  i64 three-arg smoke 2/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；合约组
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 2/0、bool 19/0、u64 14/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-multiply-typed-thunk.md`。
  备注：这是三参 i64 乘法窄覆盖；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:00:16 +08:00 · M1.5 / 07-S5 i64 three-arg thunk shape split
  支持切片 · 状态：支持切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：将三参 i64 add-return shape 识别从 `backend_aot_c_typed_i64_thunks.c`
  拆到 `backend_aot_c_typed_i64_thunk_shapes.{h,c}`，主文件继续负责 can-emit 与
  thunk writer，契约测试改为同时读取主 thunk 文件和 shape 文件。RED/GREEN：同行为拆分
  不新增行为 RED；首次 focused split 暴露契约仍只读主文件，缺少
  `function->parameterMetadataCount < 3u`，调整测试归属后 focused GREEN 为 typed call
  contracts 4/0、i64 three-arg smoke 1/0。测试结果：较宽 WSL GCC focused AOT 组通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts
  1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup
  1/0、return 1/0、value SemIR 4/0、float contracts 1/0、shared 8/0、call smoke 5/0、
  typed direct-call i64 5/0、i64 three-arg 1/0、bool 19/0、u64 14/0、f64 13/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-i64-three-arg-shape-split.md`。
  备注：这是为后续三参 i64 表达式扩展准备的支持拆分；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 07:47:14 +08:00 · M1.5 / 07-S5 static i64 three-arg add
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`i64(i64, i64, i64)` typed direct-call route 首次把
  native i64 参数 ABI 从 no/one/two-arg 扩展到三参窄形态。`backend_aot_c_typed_i64_thunks.c`
  新增 `arg0 + arg1 + arg2` recognizer/can-emit/writer，生成
  `static TZrInt64 zr_aot_typed_i64_fn_N(..., TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2)`
  并直接返回
  `return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。调用侧新增
  三参 i64 route proof，证明 destination 与三个 call-window arguments 都是已写入
  i64 scalar locals 后发出
  `zr_aot_sD = zr_aot_typed_i64_fn_N(state, zr_aot_sA, zr_aot_sB, zr_aot_sC)`，
  并保留 destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function)`，
  新增 i64 three-arg smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为
  typed call contracts 4/0、i64 three-arg smoke 1/0。测试结果：较宽 WSL GCC focused
  AOT 组通过 source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0、shared 8/0、
  call smoke 5/0、typed direct-call i64 5/0、i64 three-arg 1/0、bool 19/0、u64 14/0、
  f64 13/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-add-typed-thunk.md`。
  备注：这是 broader typed parameter ABI 的三参 i64 加法窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 07:27:42 +08:00 · M1.5 / 07-S5 static f64 two-arg modulo
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`f64(f64, f64)` typed direct-call route 从
  add/subtract/multiply/divide 扩展到带运行期零除失败通道的 ordered modulo。
  `backend_aot_c_typed_f64_thunks.c` 新增 `MOD_FLOAT/FUNCTION_RETURN` recognizer，
  two-arg f64 can-emit gate 现在覆盖 add/subtract/multiply/divide/modulo；modulo thunk
  先检查 `zr_aot_arg1 == 0.0`，通过
  `ZrCore_Debug_RunError(state, "generated AOT float modulo by zero")` 进入运行期错误通道，
  正常路径发出 `return (TZrFloat64)fmod(zr_aot_arg0, zr_aot_arg1);`。调用侧复用
  `zr_aot_static_f64_two_arg_direct_call` 证明与写入路径，继续以
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` 直调并保留 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_f64_arg0_arg1_modulo_return(`；新增 f64 smoke
  `remainder(93.0, 51.0)` 覆盖运行路径；补 recognizer/can-emit/writer 后 focused GREEN
  为 typed call contracts 4/0、f64 typed direct-call smoke 13/0。测试结果：较宽 WSL GCC
  focused AOT 组通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、
  power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts
  1/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 19/0、u64 14/0、
  f64 13/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-modulo-typed-thunk.md`。
  备注：这是 f64 二参 modulo-return 窄覆盖并补齐 typed dynamic denominator 的零除失败通道；
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 07:08:37 +08:00 · M1.5 / 07-S5 static f64 two-arg divide
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`f64(f64, f64)` typed direct-call route 从 add/subtract/multiply
  扩展到带运行期零除失败通道的 ordered divide。`backend_aot_c_typed_f64_thunks.c`
  新增 `DIV_FLOAT/FUNCTION_RETURN` recognizer，two-arg f64 can-emit gate 现在覆盖
  add/subtract/multiply/divide；divide thunk 先检查 `zr_aot_arg1 == 0.0`，通过
  `ZrCore_Debug_RunError(state, "generated AOT float divide by zero")` 进入运行期错误通道，
  正常路径发出 `return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1);`。调用侧复用
  `zr_aot_static_f64_two_arg_direct_call` 证明与写入路径，继续以
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` 直调并保留 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_f64_arg0_arg1_divide_return(`；新增 f64 smoke `ratio(84.0, 2.0)`
  覆盖运行路径；补 recognizer/can-emit/writer 后 focused GREEN 为 typed call contracts 4/0、
  f64 typed direct-call smoke 12/0。测试结果：较宽 WSL GCC focused AOT 组通过 source
  19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 19/0、u64 14/0、f64 12/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-divide-typed-thunk.md`。
  备注：这是 f64 二参 divide-return 窄覆盖并补上 typed dynamic denominator 的零除失败通道；
  f64 modulo runtime-failure route、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 06:53:10 +08:00 · M1.5 / 07-S5 static u64 two-arg greater-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route 从
  unsigned less-than-or-equal 扩展到 unsigned greater-than-or-equal。bool thunk 模块复用 shared u64
  bool-return compare helper，新增 `LOGICAL_GREATER_EQUAL_UNSIGNED/FUNCTION_RETURN` wrapper；
  u64 greater-equal thunk 发出 `return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);`，且 can-emit
  gate 现在覆盖 `<`、`==`、`!=`、`>`、`<=`、`>=`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` route proof/writer，继续证明 bool destination
  与两个已写入 u64 参数 locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)`，并允许 scalar-only
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_greater_equal_return(`；新增 bool shared-library
  smoke `at_least(50u, 8u)` 覆盖运行路径；补 greater-equal wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 19/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 19/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-greater-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return greater-than-or-equal 窄覆盖并完成当前 unsigned comparison
  subset；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 06:44:19 +08:00 · M1.5 / 07-S5 static u64 two-arg less-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route 从
  unsigned greater-than 扩展到 unsigned less-than-or-equal。bool thunk 模块复用 shared u64
  bool-return compare helper，新增 `LOGICAL_LESS_EQUAL_UNSIGNED/FUNCTION_RETURN` wrapper；
  u64 less-equal thunk 发出 `return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);`，且 can-emit
  gate 现在接受 `<`、`==`、`!=`、`>`、`<=`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` route proof/writer，继续证明 bool destination
  与两个已写入 u64 参数 locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)`，并允许 scalar-only
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_less_equal_return(`；新增 bool shared-library
  smoke `at_most(8u, 50u)` 覆盖运行路径；补 less-equal wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 18/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 18/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-less-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return less-than-or-equal 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 06:34:55 +08:00 · M1.5 / 07-S5 static u64 two-arg greater bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route 从
  unsigned inequality 扩展到 unsigned greater-than。bool thunk 模块复用 shared u64
  bool-return compare helper，新增 `LOGICAL_GREATER_UNSIGNED/FUNCTION_RETURN` wrapper；
  u64 greater thunk 发出 `return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);`，且 can-emit
  gate 现在接受 `<`、`==`、`!=`、`>`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` route proof/writer，继续证明 bool destination
  与两个已写入 u64 参数 locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)`，并允许 scalar-only
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(`；新增 bool shared-library
  smoke `greater(50u, 8u)` 覆盖运行路径；补 greater wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 17/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 17/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-greater-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return greater-than 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 06:24:43 +08:00 · M1.5 / 07-S5 static u64 two-arg not-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route 从
  unsigned equality 扩展到 unsigned inequality。bool thunk 模块复用 shared u64
  bool-return compare helper，新增 `LOGICAL_NOT_EQUAL_UNSIGNED/FUNCTION_RETURN` wrapper；
  u64 inequality thunk 发出 `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`，且 can-emit
  gate 现在接受 `<`、`==`、`!=`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` route proof/writer，继续证明 bool destination
  与两个已写入 u64 参数 locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)`，并允许 scalar-only
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_not_equal_return(`；新增 bool shared-library
  smoke `different(21u, 22u)` 覆盖运行路径；补 not-equal wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 16/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 16/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-not-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return inequality 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 06:10:01 +08:00 · M1.5 / 07-S5 static u64 two-arg equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route 从
  unsigned less-than 扩展到 unsigned equality。bool thunk 模块复用 shared u64
  bool-return compare helper，新增 `LOGICAL_EQUAL_UNSIGNED/FUNCTION_RETURN` wrapper；
  u64 equality thunk 发出 `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` route proof/writer，继续证明 bool destination
  与两个已写入 u64 参数 locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)`，并允许 scalar-only
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_equal_return(`；新增 bool shared-library
  smoke `same(21u, 21u)` 覆盖运行路径；补 equality wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 15/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 15/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return equality 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 05:56:13 +08:00 · M1.5 / 07-S5 static u64 two-arg less bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route 覆盖第一条
  unsigned comparison：`uint < uint -> bool`。bool thunk 模块新增 u64 type-ref 判定、
  u64 bool-return compare helper、`LOGICAL_LESS_UNSIGNED/FUNCTION_RETURN` wrapper 与
  `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk()`；生成
  `TZrBool ... (TZrUInt64, TZrUInt64)` thunk，直接返回
  `(TZrBool)(zr_aot_arg0 < zr_aot_arg1)`。调用侧新增
  `zr_aot_static_u64_bool_two_arg_direct_call` route proof/writer，证明 bool destination
  与两个已写入 u64 参数 locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)`，并允许 scalar-only
  destination sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function)`；
  新增 bool shared-library smoke `smaller(7u, 9u)` 覆盖运行路径；补 recognizer、thunk
  writer 与 u64-bool route/writer 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 14/0。测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 14/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-less-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return less-than 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 05:34:37 +08:00 · M1.5 / 07-S5 static i64 two-arg greater-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route 扩展到
  signed greater-than-or-equal，并完成当前窄范围 signed compare set
  `<`、`==`、`!=`、`>`、`<=`、`>=`。bool thunk 模块复用 shared i64 bool-return
  compare helper，新增 `LOGICAL_GREATER_EQUAL_SIGNED/FUNCTION_RETURN` wrapper；
  greater-equal thunk 发出 `return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);`。调用侧复用
  `zr_aot_static_i64_bool_two_arg_direct_call` proof/writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_i64_arg0_arg1_greater_equal_return(`；新增 bool shared-library
  smoke `at_least(50, 8)` 覆盖运行路径；补 greater-equal wrapper 与 writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 13/0。测试结果：
  较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 13/0、
  u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-greater-equal-bool-typed-thunk.md`。
  备注：这是 mixed i64-parameter bool-return greater-than-or-equal 窄覆盖；当前 signed
  compare set 已覆盖，但 07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 05:24:23 +08:00 · M1.5 / 07-S5 static i64 two-arg less-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route 从
  signed less-than/equality/inequality/greater-than 扩展到 signed less-than-or-equal。
  bool thunk 模块复用 shared i64 bool-return compare helper，新增
  `LOGICAL_LESS_EQUAL_SIGNED/FUNCTION_RETURN` wrapper；less-equal thunk 发出
  `return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);`。调用侧复用
  `zr_aot_static_i64_bool_two_arg_direct_call` proof/writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(`；新增 bool shared-library
  smoke `at_most(8, 50)` 覆盖运行路径；补 less-equal wrapper 与 writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 12/0。测试结果：
  较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 12/0、
  u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-less-equal-bool-typed-thunk.md`。
  备注：这是 mixed i64-parameter bool-return less-than-or-equal 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 05:10:52 +08:00 · M1.5 / 07-S5 static i64 two-arg greater bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route 从
  signed less-than/equality/inequality 扩展到 signed greater-than。bool thunk 模块复用
  shared i64 bool-return compare helper，新增 `LOGICAL_GREATER_SIGNED/FUNCTION_RETURN`
  wrapper；greater thunk 发出 `return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);`。调用侧复用
  `zr_aot_static_i64_bool_two_arg_direct_call` proof/writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `greater(50, 8)`
  case 缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；
  补 greater wrapper 与 writer branch 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 11/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 11/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-greater-bool-typed-thunk.md`。
  备注：这是 mixed i64-parameter bool-return greater-than 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 04:58:12 +08:00 · M1.5 / 07-S5 static i64 two-arg not-equal
  bool typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route
  从 signed less-than/equality 扩展到 signed inequality。bool thunk 模块复用 shared
  i64 bool-return compare helper，新增 `LOGICAL_NOT_EQUAL_SIGNED/FUNCTION_RETURN`
  wrapper；inequality thunk 发出 `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`。
  调用侧复用 `zr_aot_static_i64_bool_two_arg_direct_call` proof/writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `different(21, 22)`
  case 缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；
  补 not-equal wrapper 与 writer branch 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 10/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 10/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-not-equal-bool-typed-thunk.md`。
  备注：这是 mixed i64-parameter bool-return inequality 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 04:43:21 +08:00 · M1.5 / 07-S5 static i64 two-arg equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route 从
  signed less-than 扩展到 signed equality。bool thunk 模块将 i64 bool-return recognizer
  泛化为 shared compare helper，新增 `LOGICAL_EQUAL_SIGNED/FUNCTION_RETURN` wrapper；
  equality thunk 发出 `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`。调用侧复用
  `zr_aot_static_i64_bool_two_arg_direct_call` proof/writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `same(21, 21)` case
  缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；补
  shared compare recognizer、equality wrapper 与 writer branch 后 focused GREEN 为
  typed call contracts 4/0、bool typed direct-call smoke 9/0。测试结果：较宽 WSL GCC
  focused AOT 组继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、
  power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool 9/0、u64 14/0、f64 11/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-equal-bool-typed-thunk.md`。
  备注：这是 mixed i64-parameter bool-return equality 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 04:30:53 +08:00 · M1.5 / 07-S5 static i64 two-arg less bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：新增 mixed scalar native signature 覆盖
  `bool smaller(int left, int right) { return left < right; }`。bool thunk 模块识别
  bool 返回、两个 i64 参数、`LOGICAL_LESS_SIGNED/FUNCTION_RETURN` 形态，并生成
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)`
  与 `return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);`；typed direct-call route 证明 bool
  destination local + 两个已写入 i64 argument locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，并继续允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `smaller(7, 9)` case
  缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；补
  recognizer、writer 与 route 后 focused GREEN 为 typed call contracts 4/0、bool typed
  direct-call smoke 8/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 8/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-less-bool-typed-thunk.md`。
  备注：这是 mixed i64-parameter bool-return 窄覆盖；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 04:01:46 +08:00 · M1.5 / 07-S5 static two-arg bool inequality
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool thunk 模块复用 compact compare helper 新增 `left != right`
  二参 recognizer，覆盖 `LOGICAL_NOT_EQUAL_BOOL/FUNCTION_RETURN` 形态；thunk 发出
  `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`；bool two-arg direct-call route 复用
  scalar-local proof 并继续直接发 `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`。
  bool smoke 新增 `different(true, false)` 运行结果 42，并继续拒绝 `CallStaticDirect` /
  `CallStackValue` / typed destination sync。RED/GREEN：RED 为 typed call contract 缺少
  not-equal recognizer；smoke 证明旧生成物仍走 `zr_aot_bool_compare_exec` /
  `SZrTypeValue` frame path；补 compare helper、not-equal wrapper、two-arg can-emit gate 与
  exact writer 后 typed call contracts 4/0、bool typed direct-call smoke 7/0。测试结果：
  较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 7/0、u64 14/0、f64 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-two-arg-not-equal-typed-thunk.md`。
  备注：这是窄 bool 二参 `!=` 覆盖；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 03:46:11 +08:00 · M1.5 / 07-S5 typed direct-call bool smoke
  support split · 状态：支持子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 bool typed direct-call smoke support header，把重复 shared-library harness
  收敛到 `SZrAotTypedDirectCallBoolSmokeCase`；bool smoke 主文件只保留 6 个 case 与
  `RUN_TEST` 列表。RED/GREEN：support split 无新增行为 RED；拆分后 bool typed direct-call
  smoke 6/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-typed-direct-call-bool-smoke-support-split.md`。
  备注：这是测试承载拆分，不新增 typed thunk 行为；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 03:39:28 +08:00 · M1.5 / 07-S5 static two-arg bool equality
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool thunk 模块新增 `left == right` 二参 recognizer，
  覆盖 `LOGICAL_EQUAL_BOOL/FUNCTION_RETURN` 形态；thunk 发出
  `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`；bool two-arg direct-call route 复用
  scalar-local proof 并继续直接发 `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`。
  bool smoke 新增 `same(true, true)` 运行结果 42，并继续拒绝 `CallStaticDirect` /
  `CallStackValue` / typed destination sync。RED/GREEN：RED 为 typed call contract 缺少
  equality recognizer；smoke 证明旧生成物仍走 `zr_aot_bool_compare_exec` /
  `SZrTypeValue` frame path；补 recognizer、two-arg can-emit gate 与 exact writer 后
  typed call contracts 4/0、bool typed direct-call smoke 6/0。测试结果：较宽 WSL GCC
  focused AOT 组继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 6/0、u64 14/0、f64 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  generic numeric contracts/smoke 1/0、global contracts 7/0、global smoke 9/0、logical 4/0、
  power contracts 2/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-bool-two-arg-equal-typed-thunk.md`。
  备注：这是窄 bool 二参 `==` 覆盖；07-S5 完整验收和 08-12 仍未完成。

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
