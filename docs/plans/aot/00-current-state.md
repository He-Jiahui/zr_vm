---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 AOT 纯 C 降级 / 指令集与数据模型 C# 化
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
---

# 00 · 现状基线与差距清单

本文是事实基线（不含设计主张），为 01–06 的设计提供坐标。所有结论均来自代码探查。

## 1. 指令集现状

### 1.1 编码与格式

- 指令定义集中在 `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h`，
  以 X-macro 列出 **251 条**执行指令。
- 指令定长 **8 字节**：`SZrInstruction { TZrUInt16 operationCode; TZrUInt16 operandExtra; TZrInstructionType operand; }`，
  `operand` 是 4 字节联合（`uint8[4]` / `uint16[2]` / `int32[1]`）。
- 分发：GNU/Clang 用 computed-goto（`ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED`），
  MSVC 用 switch fallback（`execution_dispatch.c`）。

### 1.2 已有的“类型特化”倾向（正向资产）

指令集**已经**沿 il2cpp/hybridclr 思路做了大量类型特化，这是本计划的有利起点：

- 算术：`ADD` / `ADD_SIGNED` / `ADD_UNSIGNED` / `ADD_FLOAT`，并有
  `ADD_SIGNED_LOAD_CONST` / `ADD_SIGNED_LOAD_STACK_CONST` / `ADD_SIGNED_MOD_CONST` 等融合变体。
- 比较/跳转：`LOGICAL_EQUAL_SIGNED` / `..._FLOAT` / `..._STRING`、
  `JUMP_IF_GREATER_SIGNED` / `JUMP_IF_NOT_EQUAL_SIGNED_CONST` 等。
- 转换：`TO_INT` / `TO_UINT` / `TO_FLOAT_SIGNED` / `TO_FLOAT_UNSIGNED` 等显式化转换。
- 成员/元数据：`GET_MEMBER_SLOT` / `SET_MEMBER_SLOT`、`SUPER_META_GET_CACHED` 等带缓存的访问。

### 1.3 仍存在的“不确定性”（待消除）

- 通用算术 `ADD` 等在执行时仍**按操作数 tag 现场分支**（float/uint/int 三路），
  含义依赖运行时类型而非编译期类型。
- 多数指令以 `SZrTypeValue`（tagged union）为操作对象，指令本身不携带“此槽恒为 int64”这类静态事实。
- 成员访问、调用存在动态/元/已知多条路径（`GET_MEMBER` vs `GET_MEMBER_SLOT`、
  `DYN_CALL` vs `KNOWN_VM_CALL` vs `KNOWN_NATIVE_CALL`），选择逻辑分散。

## 2. 值表示与栈现状

### 2.1 值容器：tagged union

`zr_vm_core/include/zr_vm_core/value.h`：

```c
struct SZrTypeValue {
    EZrValueType type;                    // 运行时类型 tag
    TZrPureValue value;                   // union: object* / nativeObject(i64/u64/bool/f64/ptr) / nativeFunction
    TZrBool isGarbageCollectable;
    TZrBool isNative;
    EZrOwnershipValueKind ownershipKind;  // NONE/UNIQUE/SHARED/WEAK/BORROW/LOAN...
    struct SZrOwnershipControl *ownershipControl;
    struct SZrOwnershipWeakRef *ownershipWeakRef;
};
```

- 非 NaN-boxing；每个槽 ~60–80 字节，绝大部分对纯标量运算是冗余负载。
- 栈单位 `SZrTypeValueOnStack { SZrTypeValue value; TZrUInt32 toBeClosedValueOffset; }`（`stack.h`）。
- 旧 ABI 以逻辑槽寻址：`functionBase + slot`，每槽固定为一个 `SZrTypeValue`。

### 2.2 已有的连续布局雏形（地基）

- `zr_vm_core/include/zr_vm_core/type_layout.h`：
  `SZrTypeLayout { byteSize; byteAlign; kind(VALUE/STRUCT/UNION); copyKind(POD/FIELD); dropKind(NONE/FIELD); fields[]; gcFieldCount; ownershipFieldCount; tagOffset; tagSize; }`，
  `SZrTypeLayoutField { byteOffset; byteSize; typeLayoutIndex; flags(VALUE_SLOT/GC_VALUE/OWNERSHIP_VALUE); }`。
- `function.h` 的 `SZrFunctionFrameSlotLayout { stackSlot; byteOffset; byteSize; byteAlign; typeLayoutId; slotKind(VALUE/INLINE_STRUCT); isParameter; }`
  + `SZrFunction.frameByteSize/frameByteAlign/frameSlotLayouts`。
- 运行时原语：`ZrCore_Stack_CopyInline()`、`ZrCore_TypeLayout_CopyInline()`、
  `ZrCore_Function_DropInlineFrameValues()`（见
  `docs/core-runtime/inline-type-layout-and-byte-stack.md`）。
- **状态**：这是与旧固定槽 ABI **并行**的新层，尚未成为解释器主路径，也未被 AOT 全量采用。

### 2.3 类型/原型 metadata

- `object.h`：`SZrObjectPrototype`（name、layoutByteSize/Align、memberDescriptors、managedFields）、
  `SZrMemberDescriptor`（FIELD/METHOD/PROPERTY、virtualSlotIndex）、
  `SZrStructPrototype`（keyOffsetMap：字段名 → 字节偏移）。
- `metadata_token.h`：C#/CLI 风格 token 模型（MODULE/TYPE_DEF/MEMBER_DEF/TYPE_REF/MEMBER_REF/SIGNATURE…）。
- 原型二进制数据已从常量池迁移到函数体（`SZrFunction.prototypeData`），运行时反序列化为原型实例。

## 3. SemIR / typed IR 现状（地基）

`function.h:306-377`：

- `EZrSemIrOpcode`：除所有权/动态/元操作外，已含
  `VALUE_ADDR(18) / FIELD_ADDR(19) / LOAD_VALUE(20) / STORE_VALUE(21) / INIT_VALUE(22) / COPY_VALUE(23) / CALL_TYPED(24) / RETURN_TYPED(25)`。
- `SZrSemIrInstruction { opcode; execInstructionIndex; typeTableIndex; effectTableIndex; destinationSlot; operand0; operand1; deoptId; }`。
- 辅助表：`semIrTypeTable`、effect 表（`SZrSemIrEffectEntry`：ownership transition / dynamic runtime）、
  block 表、deopt 表（`SZrSemIrDeoptEntry`：typed → exec 指令的回退点）。
- **状态**：结构已立，但 typed 槽尚无“单一静态 C 类型”的完整标注，deopt 触发与覆盖范围有限。

## 4. AOT 后端现状（半降级，待推进）

目录：`zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/`。

- 已有 C 与 LLVM 两套发射器（`backend_aot_c_*` / `backend_aot_llvm_*`）。
- C lowering 已覆盖：常量、全局/子函数、栈/闭包、控制流、TRY/CATCH/FINALLY、迭代器、
  所有权 OWN_*、super array、泛型数值/逻辑/转换、typed 算术/位/转换、
  以及 SemIR 值/字段/调用（`backend_aot_c_value_semir*.c`）。
- **关键差距（本计划核心动机）**：典型算术 lowering 形如——

  ```c
  SZrTypeValue *dst = ZrCore_Stack_GetValue(frame.slotBase + 2);
  const SZrTypeValue *l = ZrCore_Stack_GetValue(frame.slotBase + 0);
  const SZrTypeValue *r = ZrCore_Stack_GetValue(frame.slotBase + 1);
  ... 运行时 tag 分支 ...
  ZR_VALUE_FAST_SET(dst, nativeInt64, l_int + r_int, ZR_VALUE_TYPE_INT64);
  ```

  运算是真实 C（`l_int + r_int`），但**读写仍走 tagged-union 容器与 VM 取值函数**。
  这是“运算降级、存储未降级”的半降级状态——目标形态应是
  `slot2 = slot0 + slot1;`（`slotN` 为函数局部的 `TZrInt64`）。

## 5. 差距清单（→ 后续文档承接）

| # | 差距 | 影响 | 承接文档 |
|---|------|------|----------|
| G1 | 标量槽仍用 80B tagged union，AOT 读写经 VM 取值函数 | 半降级、性能与可读性差 | 02, 04 |
| G2 | 通用算术运行时按 tag 分支，含义不确定 | 阻碍纯 C 降级与确定性 | 03 |
| G3 | typed SemIR 槽缺“单一静态 C 类型”完整标注 | typed 路径无法保证不变量 A | 03, 04 |
| G4 | inline byte-stack 与旧固定槽 ABI 并存，未统一 | 两套寻址、易偏移不一致 | 02, 05 |
| G5 | struct 值传递/返回未走 il2cpp 式 memcpy/隐藏指针 ABI | 值语义不完整 | 02, 05 |
| G6 | 所有权/GC 在 typed 路径未做编译期消除证明 | 生成 C 仍需运行时检查 | 05 |
| G7 | 解释器与 C 后端共享中间层但 deopt 边界未固化 | 双路径语义一致性无保证 | 04, 05 |

## 6. 不需要重做的（已具备，复用即可）

- 类型特化指令的命名/编码骨架（`zr_instruction_conf.h`）。
- `SZrTypeLayout` / `SZrFunctionFrameSlotLayout` / inline copy 原语。
- SemIR 指令与表结构（`function.h`）。
- metadata token 与原型序列化（`metadata_token.h` / `prototypeData`）。
- AOT C/LLVM 发射器框架（`backend_aot_*`）。
