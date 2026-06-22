---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono 完善内存管理（GC/分配/根/屏障）
references:
  - lua/mono/mono/sgen/sgen-descriptor.h     # DESC_TYPE run-length/bitmap/complex 引用描述
  - lua/mono/mono/sgen/sgen-cardtable.c       # card table 写屏障
  - lua/hybridclr/libil2cpp/gc/WriteBarrier.h
  - lua/hybridclr/libil2cpp/gc/GarbageCollector.h   # 静态预计算 descriptor / 根登记
related_code:
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/gc.h           # region 种类 / 阶段 / 分代年龄
  - zr_vm_core/include/zr_vm_core/raw_object.h    # SZrRawObject.garbageCollectMark
  - zr_vm_core/include/zr_vm_core/type_layout.h   # gcFieldOffsets / VisitGcValues
  - zr_vm_core/include/zr_vm_core/ownership.h
---

# 09 · 内存管理（AOT 视角：GC descriptor / 精确根 / safepoint / 写屏障）

> 与 `05` 分工：`05` 讲「**所有权语义**如何降级到生成 C + typed↔dynamic 桥」；本文 `09` 讲
> 「**GC 子系统机制**（引用描述符、根扫描、安全点、写屏障、分代/移动）在 AOT native 环境下
> 如何与现有解释器 GC 统一」。两者共享同一 `SZrTypeLayout` 与同一 GC（不变量 C）。

## 0. 现状与关键约束

zr_vm 已有相当完整的 GC（不是从零）：
- **分代 + 增量三色标记**（`gc_mark.c`/`gc_cycle.c`）：阶段含 `MINOR_MARK/MINOR_EVACUATE/
  MAJOR_MARK_CONCURRENT/MAJOR_REMARK/SWEEP/COMPACT`；年龄 `NEW/SURVIVAL/BARRIER/ALIVE/LONG_ALIVE/...`。
- **region 堆**：`eden/survivor/old/pinned/large/permanent`。
- **对象头** `SZrRawObject.garbageCollectMark`；记忆集 `rememberedObjects`。
- **值类型 GC**：`SZrTypeLayout.gcFieldOffsets/gcFieldCount` + `ZrCore_TypeLayout_VisitGcValues()`。

> **关键约束（决定 AOT 根方案）**：zr_vm 存在 `MINOR_EVACUATE` 与 `COMPACT` 阶段 → 这是**移动式 GC**。
> 因此 AOT 生成的 C 里持有的裸 `SZrRawObject*`（`07` 的引用寄存器）在 GC 移动后会**失效**。
> 这排除了「裸指针 + 保守扫描」的简单方案（il2cpp 用 Boehm 不移动可以保守扫，zr_vm 不行）。
> AOT 必须提供**精确、可更新**的根，让 GC 在 safepoint 重定位寄存器中的指针。

## 1. GC 引用描述符（descriptor，对标 mono DESC_TYPE / il2cpp 静态预计算）

把 `gcFieldOffsets` 形式化为每类型一张**静态引用描述符**，GC 扫描值类型实例时遍历它：

```c
/* 为每个含引用的值类型发射；纯标量/blittable 类型不发（descriptor = PTRFREE） */
static const TZrUInt32 ZrGcOffsets_<cTypeId>[] = { 16, 40 /* 引用字段字节偏移 */ };
```

- 编码选型：偏移数较少用 **offset 列表**（对标 mono RUN_LENGTH）；字段密集可选 **bitmap**（mono BITMAP）。
  首版用 offset 列表（实现简单、与现有 `gcFieldOffsets` 一一对应），bitmap 留作优化。
- **静态预计算**（对标 il2cpp，而非 mono 运行期算）：descriptor 在 AOT 编译期由唯一 `SZrTypeLayout`
  产出，运行期直接用，零计算（不变量 C）。
- blittable 判定（`02`§2.1）：`gcFieldCount==0` → `PTRFREE`，GC 跳过、复制可纯 `memcpy`。
- 嵌套 struct：descriptor 递归展开父偏移 + 子偏移（与 layout 计算同源）。

## 2. AOT 精确栈根（root map + 寄存器引用）

`07` 把帧槽落为裸 C 局部，GC 看不见、且移动后失效。方案（对标 il2cpp 帧根登记，但要求**精确**）：

- **帧根描述符** `SZrAotGcRootMap`（由 `SZrAotMethodInfo.gcRootMap` 指向，`07`§4）：
  列出函数内「持有引用的寄存器/可寻址槽」及其在帧内的定位方式。
- 含引用的寄存器**不能只是任意 C 局部**——它们必须 GC 可定位且可写回。两种实现，按槽选用：
  1. **引用槽落字节帧**（`07`§3.3 已要求 GC/可寻址槽进字节帧）：root map 记字节偏移，GC 经
     `frame base + offset` 精确读写 → 移动后能就地更新。**默认方案**。
  2. **`volatile SZrRawObject*` C 局部 + 取地址登记**：root map 存其地址；适用于不便落字节帧的临时引用。
- **单次登记（RAII 风格）**：prologue 把 (帧基址, root map) 推入 state 根栈，epilogue 弹出
  （`05`§3 已起头）。纯标量函数 `gcRootMap==NULL`，prologue 为空（`07`§4.3），零开销。

## 3. 安全点（safepoint，对标 mono/il2cpp GC safepoint）

- 移动 GC 只能在**根处于一致、可被精确解释**的点发生 → AOT 代码只在 safepoint 让出：
  **分配点、调用点（call/return）、回边（循环 back-edge）**。纯标量直线段无 safepoint。
- safepoint 是白名单内受控 runtime 调用（`ZrCore_Gc_SafePoint(state)`，`04`§4）。
- safepoint 处 GC 可：标记/移动对象、按 root map 更新寄存器引用、推进增量标记配额。
- 回边 safepoint 保证长循环不饿死 GC（对标各运行时的 loop back-edge poll）。

## 4. 写屏障（对标 mono card table / zr_vm 现有 rememberedObjects）

- zr_vm 已有 `rememberedObjects` + `BARRIER` 年龄 → 已是 remembered-set 式分代屏障。
- 生成 C：**引用字段 store**（写入对象/堆，非写寄存器）插 `ZrCore_Gc_WriteBarrier(state, owner, newRef)`
  （`05`§4）。写寄存器（`07`§5.2）**不**插屏障。
- **编译期消除**（对标各运行时的 nursery/栈局部优化）：
  - owner 可证明为本函数内新分配（未逃逸、必在新生代）→ 省略屏障；
  - owner 为栈上 inline struct（非堆）→ 省略；
  - 不能证明 → 保留（保守）。
- card table 作为可选优化路径（大堆下批量扫脏卡，`gc.h` region 已具备承载），首版沿用 remembered-set。

## 5. 值类型分配与装箱

- 值类型（标量/inline struct/union）：栈/寄存器/内联进对象，**不经 GC 分配**（`02`/`07`）。
- 装箱（值类型 → GC 对象引用）只在**边界**（`07`§6）或显式 box 发生：调 object GC 接口分配
  装箱壳，`memcpy` 载荷，引用进寄存器并登记根。对标 il2cpp box，但 zr_vm 仅在跨 typed↔dynamic 时发生。
- GC 引用对象分配：直接走 object 侧 GC 接口（`07`§5.2），结果 `SZrRawObject*` 入寄存器 + root map。

## 6. FFI / pinned 与外部内存

- 传给 native FFI 的引用/缓冲在调用期间 **pin**（移入/标记 `pinned` region），调用后解 pin
  （对标 GCHandle PINNED）。这是白名单受控调用。
- 大对象进 `large` region（已有），不参与 nursery 复制。

## 7. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 09-S1 | 每值类型发射 GC 引用 descriptor（offset 列表）+ GC 扫描走该表（§1） | struct 内引用被标记，blittable 跳过；与 `VisitGcValues` 结果一致 |
| 09-S2 | `SZrAotGcRootMap` + 含引用槽落字节帧 + 单次登记（§2） | 压力 GC 下 AOT 栈引用不丢；移动后寄存器引用被更新 |
| 09-S3 | safepoint 插入（分配/调用/回边）（§3） | 长循环/分配密集 typed 函数 GC 能推进、不饿死 |
| 09-S4 | 引用字段 store 写屏障 + 编译期消除（§4） | 分代一致性测试 GREEN；新生代/栈局部无多余屏障 |
| 09-S5 | 装箱/值类型分配边界化 + FFI pin（§5/§6） | 装箱仅现于边界；FFI 期间引用不被移动 |

## 8. 不变量校验

- **B 纯降级**：纯标量直线段零 safepoint、零屏障、零分配调用；GC 相关 runtime 调用（safepoint/
  writebarrier/alloc/box/pin）全在 `04`§4 白名单。
- **C 单一真相**：descriptor、root map、装箱布局全部由唯一 `SZrTypeLayout` 派生。
- **D 环境隔离**：root map 经 `SZrAotMethodInfo` 携带；safepoint/屏障是显式调用点，不重建解释器帧。
- 与 `05` 协同：所有权 drop 序列（`05`§1）与 GC 回收互不重复——unique/borrow 静态 drop，GC 只管
  `plain-gc`/`shared` 可达性。
