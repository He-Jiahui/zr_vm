---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 struct 栈上连续布局 / 值表示去 tag 化 / metadata 驱动存储
related_code:
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/include/zr_vm_core/object.h
references:
  - lua/hybridclr/libil2cpp/hybridclr/metadata/ClassFieldLayoutCalculator.h
---

# 02 · 值表示去 tag 化 + 栈/struct 连续布局 + metadata

承接差距 G1/G4/G5。目标：让标量与 struct 在 typed 路径以**裸 C 类型/连续字节**存在，
赋值是 `=`/`memcpy`，布局由唯一 metadata 描述。tagged-union 仅保留给 dynamic 路径。

## 1. 值的两态模型

引入显式的**两态值模型**，由编译期分类决定（见 `01` §4）：

- **Typed 值**：以裸 C 形式存在——`TZrInt64` / `TZrFloat64` / `struct ZrLayout_<TypeId>` /
  `SZrRawObject*`（引用）。无 type tag、无随值携带的所有权控制块指针。
  类型信息在**槽的静态元数据**里，不在值里。
- **Dynamic 值**：保持现有 `SZrTypeValue`（tagged union）。用于反射、`dyn`、跨未知边界。

> 关键转变：tag 从“每个值都带”变为“仅 dynamic 值带”。typed 路径用**槽级静态类型**取代值级 tag。

## 2. 类型布局：作为唯一真相

### 2.1 扩展 SZrTypeLayout

在 `type_layout.h` 现有字段（`byteSize/byteAlign/kind/copyKind/dropKind/fields[]/gcFieldCount/ownershipFieldCount/tagOffset/tagSize`）基础上补：

- `TZrBool blittable`：所有字段 POD 且无 GC/所有权字段（对标 il2cpp `ClassLayoutInfo.blittable`）。
  blittable → 复制可用单次 `memcpy`，drop 为 no-op。
- `TZrUInt32 cTypeId`：稳定的生成 C 类型标识，用于发射 `struct ZrLayout_<cTypeId>` 与跨模块引用。
- `gcFieldOffsets[] / ownershipFieldOffsets[]`：预计算的 GC/所有权子字段偏移序列
  （供 GC 扫描与 drop 直接遍历，避免运行期再算；对标 il2cpp 把引用位置表化）。

### 2.2 布局计算唯一化

- 现有 `function_type_layout.c` / `type_layout.c` 的布局计算抽为**唯一入口**
  `ZrCore_TypeLayout_Compute(prototype) -> SZrTypeLayout`，C 后端与解释器都调用它。
- 字段偏移规则与 il2cpp 一致：按声明序，每字段对齐到自然对齐，结构体尾部对齐到最大字段对齐。
- 嵌套 struct 递归内联（`fields[i].typeLayoutIndex` 指向子布局），形成完整连续布局。

### 2.3 生成 C struct 与一致性断言

为每个 typed 值类型发射：

```c
typedef struct ZrLayout_42 {       /* cTypeId = 42 */
    TZrInt64  field0_x;            /* byteOffset 0 */
    TZrFloat64 field1_y;          /* byteOffset 8 */
    SZrRawObject *field2_ref;     /* GC 字段，偏移登记到 gcFieldOffsets */
} ZrLayout_42;
ZR_STATIC_ASSERT(offsetof(ZrLayout_42, field1_y) == 8);  /* 对标 metadata.byteOffset */
ZR_STATIC_ASSERT(sizeof(ZrLayout_42) == /* metadata.byteSize */ 24);
```

不变量 C 落地：`offsetof`/`sizeof` 与 metadata 编译期一致，否则编译失败。

## 3. 栈与栈帧：连续字节为主

### 3.1 统一到 byte-offset 栈帧

- 以 `SZrFunctionFrameSlotLayout`（`function.h`：`stackSlot/byteOffset/byteSize/byteAlign/typeLayoutId/slotKind`）
  为**主**寻址方式，逐步取代 `functionBase + slot`（差距 G4）。
- typed 函数的帧是一段 `frameByteSize` 对齐缓冲；每个 typed 槽是其中一段连续字节，
  scalar 槽就是 `TZrInt64`/`TZrFloat64`，struct 槽就是 `ZrLayout_*` 的连续内存。
- 解释器侧已有 `ZrCore_Stack_CopyInline` / `ZrCore_Function_MakeFrameSlotPlace`，扩展为
  typed 主路径；旧固定槽路径仅 dynamic 函数使用，标记为 legacy。

### 3.2 AOT 帧 = C 局部变量

typed 函数在生成的 C 里，帧槽**直接落为 C 局部变量**（il2cpp 风格），而非一段手工管理的字节区：

```c
void zr_fn_Foo(SZrState *state /*, typed 参数...*/) {
    TZrInt64      s0;             /* scalar slot */
    ZrLayout_42   s1;            /* inline struct slot */
    SZrRawObject *s2;            /* gc ref slot（需登记到帧 GC map，见 05） */
    ...
    s0 = a + b;                  /* 纯 C */
    s1 = make_point(...);        /* struct = struct */
    ...
}
```

GC 可达性：含引用的槽不能只是裸 C 局部（GC 看不见），需登记到帧的 GC 根描述符（见 `05` §3）。
纯标量/blittable struct 局部无需登记。

## 4. struct 值语义 ABI（对标 il2cpp）

定义 typed struct 的传参/返回约定（差距 G5），供 `04`/`05` 的调用降级使用：

- **小 blittable struct（≤ 2 机器字、无 GC 字段）**：按值传 C struct（`ZrLayout_x p`），
  返回直接 `return st;`。
- **大 struct 或含 GC 字段**：隐藏指针 ABI——
  `void zr_fn(ZrLayout_x *__ret, const ZrLayout_x *p, ...)`，复制用 `memcpy`/逐字段 `=`。
- **赋值/拷贝**：blittable → `memcpy(&dst, &src, sizeof)`；非 blittable → 逐字段 `=`（引用字段经写屏障，见 `05`）。
- **drop**：blittable → no-op；否则遍历 `dropKind` 字段做所有权释放/GC 解引用。

## 5. union 类型（tagged value-type union）

zr_vm 已有 union 类型工作（见 `docs/parser-and-semantics/union-types.md`）。typed union：

- 布局 `kind=UNION`，`tagOffset/tagSize` 描述判别式，payload 区为各分支最大尺寸。
- 生成 C：`struct ZrLayout_u { TZrUInt32 tag; union { ... } payload; }`。
- 模式匹配降级为 `switch (u.tag)` + 对应分支字段访问，**纯 C 控制流**，不经 VM。

## 6. 落地切片（建议顺序）

| 切片 | 内容 | 验收 |
|------|------|------|
| 02-S1 | `SZrTypeLayout` 补 `blittable/cTypeId/gc&ownership offset 表`，布局计算唯一入口 | 单测：嵌套 struct 偏移/对齐/blittable 判定正确 |
| 02-S2 | 生成 `struct ZrLayout_*` + `offsetof/sizeof` 静态断言 | 生成物编译通过且断言成立 |
| 02-S3 | typed scalar 槽落为 C 局部，AOT 算术读写去 tagged-union | 算术函数 AOT 产物零 `ZrCore_Stack_GetValue` |
| 02-S4 | inline struct 槽 = C 局部/连续字节，复制走 `memcpy`/逐字段 | `test_type_layout_inline_copy` 扩展 GREEN |
| 02-S5 | struct 传参/返回 ABI（小按值/大隐藏指针） | 跨函数 struct 传递解释器与 AOT 结果一致 |
| 02-S6 | union typed 布局 + `switch` 降级 | union 匹配 AOT 为纯 `switch`，结果一致 |

## 7. 风险与缓解

- **GC 可见性**：typed C 局部中的引用槽对 GC 不可见 → 帧 GC 根描述符 + 安全点（`05`）。
- **ABI 漂移**：生成 struct 与 metadata 偏移不一致 → 静态断言强制一致。
- **旧路径并存期**：typed 与 legacy 槽寻址混用 → 以函数为单位整体二选一，禁止单函数内混寻址。
