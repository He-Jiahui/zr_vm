---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 所有权/GC 在生成 C 中的表达 / 解释器与 AOT 互操作 / metadata 驱动
related_code:
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/function.h
references:
  - lua/hybridclr/libil2cpp/hybridclr/interpreter/MethodBridge.h
  - lua/hybridclr/libil2cpp/hybridclr/metadata/ClassFieldLayoutCalculator.h
---

# 05 · 所有权/GC 在生成 C 中的表达 + 解释器与 AOT 桥

承接差距 G5/G6/G7。目标：让 typed 路径在生成的纯 C 中正确处理 GC 可达性、写屏障、
所有权语义，并尽量**编译期消除**冗余检查；同时定义 typed↔dynamic 的 bridge/deopt 封送。

## 1. 所有权的编译期消除

zr_vm 把所有权种类随值携带（`SZrTypeValue.ownershipKind/ownershipControl`，`value.h`）。
typed 路径不让所有权随值跑，而是把它变成**编译期事实 + 必要的运行时调用点**：

- SemIR 已有 `EZrSemIrOwnershipState`（PLAIN_GC/UNIQUE/SHARED/WEAK/BORROW_SHARED/BORROW_MUT）
  与 effect 表（`SZrSemIrEffectEntry`，`function.h:336-360`）。以此做**所有权流分析**：
  - `unique`/`borrow` 在 typed 块内若能证明不逃逸、不别名违规 → **零运行时检查**，
    赋值就是 `=`/`memcpy`，drop 在作用域末尾静态展开。
  - `shared`（引用计数）→ 仅在**所有权转移点**生成显式 `ZrCore_Ownership_Retain/Release` 调用，
    普通使用不插桩。
  - `weak`/`upgrade` → 在 upgrade 点生成显式检查调用，其余无开销。
- 对应 using 计划（`docs/plans/using/01-ownership-as-generics.md`）的所有权种类，
  本节负责其**生成 C 形态**：把所有权种类降级为“静态可消除 / 转移点调用”两类。

## 2. GC 字段扫描（metadata 驱动）

- `SZrTypeLayout` 的 `gcFieldOffsets[]`（`02` §2.1）为每个值类型预计算引用字段偏移。
- 为每个 typed 值类型发射 GC 扫描描述符（而非生成代码里散落标记）：
  ```c
  static const TZrUInt32 ZrGcOffsets_42[] = { 16 /* field2_ref */ };
  /* GC 扫描 ZrLayout_42 实例时遍历该表，对标 il2cpp 的引用位置表 */
  ```
- blittable 类型 `gcFieldOffsets` 为空 → GC 跳过，复制可纯 `memcpy`。

## 3. typed C 局部中的引用根

typed 函数把帧槽落为 C 局部（`02` §3.2），但 GC 看不见裸 C 局部里的引用。方案：

- **帧 GC 根描述符**：为每个 typed 函数生成一张“含引用的槽 + 其布局”的静态表，
  函数入口把帧基址 + 该表登记到 state 的根栈，出口注销（RAII 风格，单次登记）。
- **安全点（safepoint）**：仅在分配点/调用点/回边插入 GC 安全点调用；纯标量段无安全点。
- blittable / 纯标量槽不进根表，零开销。
- 对标 hybridclr：解释器栈帧已知值类型内部引用位置，AOT 帧用等价的静态根表替代。

## 4. 写屏障

- 引用字段 `STORE_VALUE`（`04` §2）在分代/增量 GC 下需写屏障：
  ```c
  *pf = newref; ZrCore_Gc_WriteBarrier(state, owner, newref);
  ```
- 写屏障是**白名单内**的受控 runtime 调用（不违反不变量 B）。
- 编译期可证明 owner 为新生代/栈上局部时省略屏障（优化）。

## 5. typed ↔ dynamic 桥（MethodBridge 对标）

衔接差距 G7，定义两个方向的封送：

- **typed → dynamic（deopt / 调 dynamic 函数）**：
  把相关 typed 槽按 layout 封送为 `SZrTypeValue`（填 tag/ownership），
  跳到字节码解释或 dynamic callee。封送代码由 layout 驱动生成（`ZrCore_Bridge_BoxTyped`）。
- **dynamic → typed（dynamic 调已 AOT 函数）**：
  从 `SZrTypeValue` 解包为 typed 槽（校验 tag 与静态类型一致，不一致则报错/回退）。
- bridge 入口表登记到函数 metadata，供解释器在 `CALL_TYPED`/`DYN_CALL` 边界选择直调或封送。
- 对标 hybridclr `MethodBridge`：managed↔native/interp 的参数打包/展开，此处是 typed↔dynamic。

## 6. 异常与作用域

- typed 路径的 `try/catch/finally`、`using`/to-be-closed 在 C 中降级为结构化控制流 +
  作用域末尾的 drop 展开（静态序列），异常 throw/unwind 为白名单 runtime 调用。
- `MARK_TO_BE_CLOSED/CLOSE_SCOPE`（字节码层）在 typed 块编译期展开为确定的 drop 调用序列，
  不依赖运行时 to-be-closed 链表（dynamic 路径仍用链表）。

## 7. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 05-S1 | layout 生成 `gcFieldOffsets` + GC 扫描走该表 | GC 测试：struct 内引用被正确标记，blittable 被跳过 |
| 05-S2 | typed 函数帧 GC 根描述符 + 安全点 | 压力 GC 下 typed C 局部引用不被误回收 |
| 05-S3 | 所有权流分析：unique/borrow 静态消除，shared 仅转移点插桩 | 单测：典型函数无多余 retain/release |
| 05-S4 | 引用字段 STORE 写屏障 + 可证明省略 | 分代 GC 一致性测试 GREEN |
| 05-S5 | typed↔dynamic bridge 封送（双向） | 混合函数跨边界结果与全解释一致 |
| 05-S6 | typed try/using 作用域 drop 静态展开 | 异常路径资源释放与解释器一致 |

## 8. 不变量校验

- 不变量 B：本文新增的 runtime 调用（retain/release/writebarrier/safepoint/box/unbox/throw）
  全部纳入 `04` §4 白名单；typed 纯标量段仍零调用。
- 不变量 C：GC 偏移、drop 字段、bridge 封送全部由唯一 layout 驱动，无散落硬编码。
