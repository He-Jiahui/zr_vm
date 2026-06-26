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
  - zr_vm_core/src/zr_vm_core/gc/gc.c             # 09-S2B root-frame stack + 09-S3A safepoint API + 09-S5B native-call pin scope
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c        # 11-S4G GC inline-frame mark resolver prefers metadata runtime for attached AOT functions
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c       # 11-S4G GC inline-frame rewrite resolver prefers metadata runtime for attached AOT functions
  - zr_vm_core/include/zr_vm_core/gc.h           # region 种类 / 阶段 / 分代年龄 / 09-S3A safepoint API / 09-S4A write barrier API / 09-S5B native-call pin API
  - zr_vm_core/include/zr_vm_core/bridge.h       # 09-S5A public typed boxing/unboxing bridge API
  - zr_vm_core/src/zr_vm_core/bridge.c           # 09-S5A bridge delegates to existing execution conversions
  - zr_vm_core/include/zr_vm_core/state.h        # 09-S2B AOT root frame stack carrier
  - zr_vm_core/include/zr_vm_core/raw_object.h    # SZrRawObject.garbageCollectMark
  - zr_vm_core/include/zr_vm_core/type_layout.h   # gcFieldOffsets / VisitGcValues
  - zr_vm_core/src/zr_vm_core/type_layout.c       # 09-S1A metadata offset-table scan path
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h # 09-S1B public SZrAotGcDescriptor / module table
  - zr_vm_core/include/zr_vm_core/function.h      # 11-S4G function-level metadata registry binding for AOT GC inline-frame layout lookup
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h # 11-S4F public metadata-runtime GC descriptor resolver; 11-S4G function-level layout resolver
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c # 11-S4F code-registration GC descriptor lookup guarded by runtime type-layout resolver; 11-S4G attached-function layout resolver
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/object.h # 09-S4C new-owner no-write-barrier object API
  - zr_vm_core/src/zr_vm_core/object/object.c # 09-S4B object/member/index heap-store public write barrier + 09-S4C new-owner no-write-barrier path
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c # 09-S4C super-array no-write-barrier fallback
  - zr_vm_core/src/zr_vm_core/object/object_internal.h # 09-S4B existing string-pair heap-store fast-path write barrier
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 09-S1B module descriptor publication
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c # 09-S2B generated root-frame push
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c # 09-S2B generated root-frame pop
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c # 09-S2B root-frame gating + 09-S3A call safepoint/back-edge detection + 09-S4C new-owner proof
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h # 09-S3A safepoint emitter API + 09-S4C no-barrier writer declarations
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c # 09-S3A safepoint helper + branch back-edge poll
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c # 09-S3A generic truthiness back-edge poll
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_iterators.c # 09-S3A iterator back-edge poll
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c # 09-S3A allocation safepoint
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_access_boundaries.c # 09-S4C no-barrier member/index runtime boundaries
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_super_array.c # 09-S4C no-barrier super-array runtime boundary
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h # 09-S2A root-map emitter API context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 09-S2A root-map emission from ExecIR frame layout
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h # 09-S1B descriptor table API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c # 09-S1A AOT GC descriptor emission
  - zr_vm_library/include/zr_vm_library/aot_runtime.h # 09-S2B generated context methodInfo binding + 09-S4C no-barrier runtime boundary API
  - zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h # 09-S4C mirrored no-barrier runtime boundary API
  - zr_vm_library/src/zr_vm_library/aot_runtime.c # 09-S2B generated context methodInfo resolution + 09-S4A closure write barrier boundary + 09-S4C no-barrier runtime boundary + 09-S5A boxing bridge boundary + 11-S4G loaded function registry attach
  - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c # 09-S4A mirrored AOT runtime write barrier boundary + 09-S4C mirrored no-barrier runtime boundary + 09-S5A mirrored boxing bridge boundary
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c # 09-S5B libffi symbol-call native-call pin scope
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h # 09-S5B GC pin API + callback stack anchor include
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c # 09-S5B callback stack anchor after native callback
  - tests/core/test_aot_gc_root_frame.c           # 09-S2B runtime root stack + 09-S3A safepoint debt + 09-S4A write barrier contract + 09-S5B native-call pin contract
  - tests/ffi/test_ffi_native_call_pin_contract.c # 09-S5B FFI native-call pin source contract
  - tests/CMakeLists.txt # 09-S5B FFI native-call pin contract target
  - tests/parser/test_value_type_runtime.c        # 09-S1A VisitGcValues metadata offsets
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 09-S1A descriptor + 09-S2A generated root-map contracts
  - tests/module/test_metadata_runtime_type_layout.c # 11-S4F GC descriptor resolver regression for 09 descriptor lookup; 11-S4G function-level resolver regression
  - tests/gc/gc_tests.c # 11-S4G non-AOT inline-frame GC fallback regression
  - tests/parser/test_aot_c_type_layout_contracts.c # 09-S1B table emission source contract
  - tests/parser/test_aot_c_frame_setup_contracts.c # 09-S2A ABI/source contract
  - tests/parser/test_aot_c_shared_library_smoke.c # 09-S1B no-GC module descriptor contract
  - tests/parser/test_aot_c_control_contracts.c # 09-S3A source contract
  - tests/parser/test_aot_c_control_shared_library_smoke.c # 09-S3A generated-C safepoint smoke
  - tests/parser/test_aot_c_constant_contracts.c # 09-S4A AOT runtime write barrier source contract
  - tests/parser/test_aot_c_guardrail_contracts.c # 09-S4A allowed GC runtime boundary contract + 09-S4C no-barrier allowlist + 09-S5A bridge allowlist
  - tests/parser/test_aot_c_global_contracts.c # 09-S4B member/index heap-store barrier source contract + 09-S4C new-owner no-barrier source contract + 09-S5A boxing bridge source contract
  - tests/parser/test_aot_c_global_shared_library_smoke.c # 09-S4A closure boundary smoke + 09-S4B member/index generated-C boundary smoke
  - tests/parser/test_aot_c_super_array_contracts.c # 09-S4B super-array generated-C boundary contract
  - tests/parser/test_aot_c_super_array_shared_library_smoke.c # 09-S4B super-array generated-C boundary smoke
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
- 11-S4F 后，运行期 code-registration descriptor 查找可通过
  `ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, typeLayoutId)` 完成，并且必须先经同一
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 验证 layout registry；descriptor 表不能独立绕过
  token↔cTypeId↔layout 单一真相。
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
| 09-S1 | 每值类型发射 GC 引用 descriptor（offset 列表）+ GC 扫描走该表（§1） | ✅ 2026-06-24 完成：09-S1A 覆盖 AOT C offset descriptor 发射、POD 跳过、`VisitGcValues` metadata offset-table 扫描；09-S1B 将生成 descriptor 以 `SZrAotGcDescriptor` 和模块级 `gcDescriptors` 表发布到 AOT ABI |
| 09-S2 | `SZrAotGcRootMap` + 含引用槽落字节帧 + 单次登记（§2） | ✅ 2026-06-25 默认 frame-byte-offset 路径完成：09-S2A 已发布 root-map ABI/generated-C method metadata；09-S2B 已加入 runtime AOT root-frame stack、generated C prologue/epilogue push/pop、GC mark/rewrite 扫描，并验证含引用 byte-frame 根在 minor GC 中保活/重写。可选 `LOCAL_ADDRESS` 局部地址登记未启用 |
| 09-S3 | safepoint 插入（分配/调用/回边）（§3） | ✅ 2026-06-25 完成：`ZrCore_Gc_SafePoint(state)` 推进 pending GC debt；generated C 在 object/array 分配后、函数调用后、循环/回边 `goto` 前插入 safepoint；source contract 与 generated-C smoke 验证 allocation/call/back-edge 三类标记和 Unix shared-library 编译。长时间真实负载压力测试仍可作为后续扩展 |
| 09-S4 | 引用字段 store 写屏障 + 编译期消除（§4） | ✅ 2026-06-25 完成：09-S4A 已新增 `ZrCore_Gc_WriteBarrier(state, ownerObject, value)`，AOT runtime 闭包/capture heap-owner 写入边界改走该白名单 API；09-S4B 已将 generated-C member/index/super-array runtime boundary 的实际 heap-object store 落点收束到 core object 公共 GC 写屏障入口，且栈上 inline struct 字段 store 仍无屏障；09-S4C 已为本函数内线性可证明的新分配 owner 生成 no-write-barrier runtime boundary，并在 core object 写入路径用 `skipWriteBarrier` 候选加 target object `YOUNG_MOVABLE` 运行时复核保守消除屏障。跨控制流、调用、逃逸、晋升后非 young owner 或不能证明的新 owner 继续保留公共写屏障 |
| 09-S5 | 装箱/值类型分配边界化 + FFI pin（§5/§6） | ✅ 2026-06-25 完成：09-S5A 已新增 public boxing/unboxing bridge API，AOT `TO_OBJECT`/`TO_STRUCT` runtime boundary 不再直接调用 execution conversion，而是经 `ZrCore_Bridge_BoxTyped` / `ZrCore_Bridge_UnboxTyped` 收束到显式桥入口；09-S5B 已新增 `SZrGcNativeCallPin` 与 `ZrCore_Gc_NativeCallPinObject/Value/Unpin`，libffi symbol call 在 native invoke 前 pin self/owner/argument GC values 并在 cleanup 反向 unpin，callback trampoline 在 native 回调后用 stack anchor 恢复可比较的 `stackTop`。当前 unpin 清理临时 native pin flag 与 ignore root，不做 pinned region demotion |

## 8. 不变量校验

- **B 纯降级**：纯标量直线段零 safepoint、零屏障、零分配调用；GC 相关 runtime 调用（safepoint/
  writebarrier/alloc/box/pin）全在 `04`§4 白名单。
- **C 单一真相**：descriptor、root map、装箱布局全部由唯一 `SZrTypeLayout` 派生。
- **D 环境隔离**：root map 经 `SZrAotMethodInfo` 携带；safepoint/屏障是显式调用点，不重建解释器帧。
- 与 `05` 协同：所有权 drop 序列（`05`§1）与 GC 回收互不重复——unique/borrow 静态 drop，GC 只管
  `plain-gc`/`shared` 可达性。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-06-25 19:31:46 +08:00 · 09 GC inline-frame layout lookup hardening via 11-S4G ·
  状态：09 阶段计划切片仍保持完成；本记录是 11-S4G 对 GC inline-frame mark/rewrite layout lookup 的后续
  运行期硬化。完成项目：AOT-loaded functions 通过 `ZrCore_MetadataRuntime_AttachFunction()` 绑定
  code-registration layout registry，GC inline-frame mark/rewrite 对 attached registry 函数改用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()`，registry layout 缺失时不回退 prototype cache；
  function reset 会清空 attached registry 字段；非 AOT/解释器函数未绑定 registry 时继续使用
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout()`，避免普通 VM
  inline-frame GC 扫描退化。RED/GREEN：RED 为 `zr_vm_metadata_runtime_type_layout_test` 缺少 function-level
  attach/resolve API；第二个 RED 为 `zr_vm_gc_test` 中 inline-frame minor rewrite work 计数从 2 降到 1；
  GREEN 后 AOT resolver 拒绝 stale prototype fallback，GC interpreter fallback 恢复。测试结果：WSL gcc/clang
  和 Windows MSVC Debug 均通过 metadata type-layout 7/0、metadata query 20/0、AOT GC root-frame 5/0、GC 66/0、
  value-type runtime 14/0、frame setup 1/0、source contracts 19/0、value-type smoke 2/0、shared-library smoke 8/0、
  descriptor diagnostics 2/0、generic reference sharing 4/0；MSVC 的 value-type/shared-library/descriptor ignored
  仍为既有 Unix-only 分支。产出：
  `tests/acceptance/2026-06-25-aot-11-s4g-gc-inline-frame-runtime-layout-resolver.md`。备注：本记录不重新打开
  09-S2 root-frame、09-S3 safepoint、09-S4 write barrier 或 09-S5 boxing/pin；optional local-address roots、
  长时间 GC 压力测试和 pinned-region demotion 仍是扩展项。

- 2026-06-25 18:45:50 +08:00 · 09 descriptor lookup hardening via 11-S4F ·
  状态：09 阶段计划切片仍保持完成；本记录是 11-S4F 对 09-S1B 生成 descriptor 表的后续运行期查表硬化。
  完成项目：新增 `ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, typeLayoutId)`，让
  code-registration `gcDescriptors[typeLayoutId]` 只有在 descriptor id 匹配且同一 id 能经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析到 registry-backed layout 时才对外暴露；缺失 layout
  registry、稀疏 descriptor、越界 id、`NONE` id 和 descriptor/layout id 不一致均返回 null。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_type_layout_test` 新增 GC descriptor resolver 用例后编译失败；
  GREEN 后 descriptor lookup 不会 fallback 到 metadata function prototype layout cache，也不会让单独存在的
  descriptor 表绕过 layout registry。测试结果：WSL gcc/clang direct 均通过 metadata type-layout 5/0、
  metadata query 20/0、AOT GC root-frame 5/0、frame setup 1/0、source contracts 19/0、value-type smoke 2/0、
  shared-library smoke 8/0、descriptor diagnostics 2/0、generic reference sharing 4/0；Windows MSVC Debug
  同组通过，其中 value-type smoke 1 ignored、shared-library smoke 8 ignored、descriptor diagnostics 2 ignored
  为既有 Unix-only 分支。产出：
  `tests/acceptance/2026-06-25-aot-11-s4f-gc-descriptor-runtime-resolver.md`。备注：本记录不重新打开
  09-S2 root-frame、09-S3 safepoint、09-S4 write barrier 或 09-S5 boxing/pin；optional local-address roots、
  长时间 GC 压力测试和 pinned-region demotion 仍是扩展项。

- 2026-06-25 11:35:46 +08:00 · 09-S4C new-owner write barrier elimination ·
  状态：09-S4 完成；09 阶段计划切片完成。完成项目：AOT C 函数体新增保守线性证明，只在 receiver slot
  最近来源可追溯到本函数 `CREATE_OBJECT` / `CREATE_ARRAY`，且中间没有调用、控制流边界、返回、逃逸或不明 slot
  使用时，才把 member/member-slot/index/super-array heap store 改发
  `*NewOwnerNoWriteBarrier` runtime boundary；核心 object 层新增
  `ZrCore_Object_SetMemberAssumeNewOwnerNoWriteBarrier`、
  `ZrCore_Object_SetByIndexAssumeNewOwnerNoWriteBarrier`、
  `ZrCore_Object_SuperArraySetIntAssumeNewOwnerNoWriteBarrier` 以及 existing-pair no-barrier helper，
  通过 `skipWriteBarrier` 候选和 target object `YOUNG_MOVABLE` 运行时复核只在已证明且仍为 young owner 的路径跳过公共 GC 写屏障；普通 member/index/super-array 写入仍保留
  09-S4B 的 `ZrCore_Gc_WriteBarrier` 路径。RED/GREEN：RED 为 `test_aot_c_global_contracts`
  新增 source contract 找不到 no-barrier writer/API/runtime helper；第二个 RED 为 guardrail 在允许清单缺少
  `ZrLibrary_AotRuntime_*NewOwnerNoWriteBarrier` 时失败；补强 RED 要求 core no-barrier API 出现 young-owner
  运行时确认，避免 allocation safepoint 后 owner 已晋升仍盲跳屏障。GREEN 后 source contract 锁定生成 writer、runtime API、
  core object no-barrier 路由、young-owner guard 和 super-array fallback，guardrail 明确允许四个新 runtime boundary。测试结果：WSL gcc direct
  global contracts 9/0、guardrail 6/0、global smoke 10/0、root-frame 5/0、constant contracts 5/0、
  super-array contracts 1/0、super-array smoke 1/0；WSL clang direct 同组 9/0、6/0、10/0、5/0、5/0、1/0、1/0；
  Windows MSVC Debug direct 同组 global contracts 9/0、guardrail 6/0、global smoke 10/0（10 ignored，Unix
  shared-library path）、root-frame 5/0、constant contracts 5/0、super-array contracts 1/0、super-array smoke 1/0
  （1 ignored）。产出：`tests/acceptance/2026-06-25-aot-09-s4c-new-owner-write-barrier-elision.md`。备注：
  09 阶段的计划切片已关闭；可选 `LOCAL_ADDRESS` 根登记、长时间 GC 压力测试、card table 优化与 pinned-region demotion
  仍作为扩展项，不在本次完成声明内。`object.c`、`aot_runtime.c` 与 `backend_aot_c_function_body.c` 均已超过大文件阈值，
  本切片只接入窄的 flag/boundary/proof plumbing，未在完成 09 阶段时混入结构性拆分。

- 2026-06-25 10:56:08 +08:00 · 09-S5B FFI native-call pin scope ·
  状态：09-S5 完成；09 阶段继续进行中，09-S4C 编译期写屏障消除仍未关闭。完成项目：核心新增
  `SZrGcNativeCallPin` 与 `ZrCore_Gc_NativeCallPinObject/Value/Unpin`，以 native-call 临时根形式记录
  对象、调用方是否新增 ignore root、调用方是否新增 native pin flag；libffi symbol call 在
  `zr_ffi_invoke_native_symbol(...)` 前 pin self object、owner value 以及每个 argument value，并在 cleanup
  中反向释放 argument/owner/self pin；callback trampoline 改用 `SZrFunctionStackAnchor` 在 native 回调后
  重新定位保存的 `stackTop`，避免 GC/栈重分配后用旧裸指针误报 stack corruption；新增
  `zr_vm_ffi_native_call_pin_contract_test` 源码合同目标锁定 FFI pin/unpin 顺序与 callback stack anchor。
  RED/GREEN：RED 先表现为核心 root-frame 测试缺少 `SZrGcNativeCallPin` 与 pin/unpin API 编译失败；
  初版 GREEN 前暴露 pin/ignore 顺序错误，`PinObject` 会清除刚登记的 ignore root，修正为先 pin 再按需 ignore
  并保留调用前已有 ignore 状态；完整 FFI 运行又暴露 callback 路径保存裸 `stackTop` 在回调期间栈重定位后不可靠，
  已用 stack anchor 修复。测试结果：WSL gcc direct `zr_vm_aot_gc_root_frame_test` 5/0 与
  `zr_vm_ffi_native_call_pin_contract_test` 2/0；WSL clang direct 同组 5/0、2/0；Windows MSVC Debug direct
  同组 5/0、2/0。产出：`tests/acceptance/2026-06-25-aot-09-s5b-ffi-native-call-pin.md`。备注：
  当前清理临时 pin flag 与 ignore root，不实现 pinned region demotion；未混入 FFI source-extern `pointer<T>`
  解析修复，完整 `zr_vm_ffi_test` 的后段 source-extern pointer 参数用例仍因既有未限定 `pointer` 类型解析基线失败，
  与 09-S5B native-call pin gate 分离。

- 2026-06-25 10:06:27 +08:00 · 09-S5A public boxing/unboxing bridge boundary ·
  状态：09-S5 子切片完成；完整 09-S5 继续进行中，FFI pin 仍未关闭。完成项目：核心新增
  `ZrCore_Bridge_BoxTyped(state, callInfo, value, typeInfo, outValue)` 与
  `ZrCore_Bridge_UnboxTyped(state, callInfo, value, typeInfo, outValue)` 公共桥 API，当前委托既有
  `ZrCore_Execution_ToObject` / `ZrCore_Execution_ToStruct` 以保持装箱布局与错误语义不变；AOT runtime
  的 `TO_OBJECT` / `TO_STRUCT` 边界改走 bridge API，生成 C 仍只在 typed↔dynamic 边界调用 runtime helper；
  `zr_vm_aot/zr_vm_library` 镜像 runtime 同步切换，避免副本分叉。RED/GREEN：RED 为
  `test_aot_c_global_contracts` 在读取 `zr_vm_core/include/zr_vm_core/bridge.h` 与
  `zr_vm_core/src/zr_vm_core/bridge.c` 时缺少公共 bridge 文件/API，且 runtime source 仍能直接看到
  `ZrCore_Execution_ToObject/ToStruct`；GREEN 后 source contract 验证 AOT runtime 改调
  `ZrCore_Bridge_BoxTyped/UnboxTyped`，bridge implementation 内部委托 execution conversion，guardrail 将
  bridge 调用归入允许 runtime boundary。测试结果：WSL gcc direct global contracts 8/0、global smoke 10/0、
  guardrail 6/0、value-type smoke 2/0、constant contracts 5/0、root/barrier 4/0；WSL clang direct
  同组 8/0、10/0、6/0、2/0、5/0、4/0；Windows MSVC Debug direct 同组 global contracts 8/0、
  global smoke 10/0（10 ignored，Unix shared-library path）、guardrail 6/0、value-type smoke 2/0
  （1 ignored）、constant contracts 5/0、root/barrier 4/0。产出：
  `tests/acceptance/2026-06-25-aot-09-s5a-boxing-bridge-boundary.md`。备注：本切片只边界化已有
  boxing/unboxing conversion，未改变装箱对象布局、值类型分配策略或 GC pinning；FFI 期间引用 pin/unpin
  仍作为 09-S5 后续项。

- 2026-06-25 09:44:28 +08:00 · 09-S4B member/index heap-store public write barrier boundary ·
  状态：09-S4 子切片完成；完整 09-S4 继续进行中，可证明新分配 owner 的编译期消除仍未关闭。完成项目：
  generated-C member/index/super-array 写入继续通过 AOT runtime boundary 执行实际堆写入；core object 的
  object/member/index heap-store 落点从内部 `ZrCore_Value_Barrier(state, ...)` 迁到公共
  `ZrCore_Gc_WriteBarrier(state, ownerObject, value)`；`zr_vm_aot/zr_vm_library` 镜像 runtime 中 3 个旧
  closure/capture value barrier 调用同步切到公共入口，避免 AOT runtime 副本分叉。RED/GREEN：RED 为
  `test_aot_c_global_contracts` 新增的 member/index heap-store source contract 缺少
  `ZrCore_Gc_WriteBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->key/value)` 且仍能看到旧
  object value barrier；GREEN 后 source contract 验证 member/index/super-array 生成 C boundary、AOT runtime
  setter 到 core object setter 的路由，以及 object heap-store/fast-path 都走公共 GC 写屏障。测试结果：WSL gcc direct
  root/barrier 4/0、global contracts 8/0、global smoke 10/0、super-array contracts 1/0、super-array smoke 1/0、
  value-type smoke 2/0、constant contracts 5/0；WSL clang direct 同组 4/0、8/0、10/0、1/0、1/0、2/0、5/0；
  Windows MSVC Debug direct 同组 4/0、8/0、global smoke 10/0（10 ignored，Unix shared-library path）、
  super-array contracts 1/0、super-array smoke 1/0（1 ignored）、value-type smoke 2/0（1 ignored）、
  constant contracts 5/0。产出：`tests/acceptance/2026-06-25-aot-09-s4b-member-index-write-barrier.md`。
  备注：`zr_vm_core/src/zr_vm_core/object/object.c` 已是大文件；本切片只替换既有屏障入口，不新增对象存储责任或 helper，
  因此未在此处拆分。后续 S4C 需要实现或证明可证明新生代 owner 的编译期消除。

- 2026-06-25 09:22:37 +08:00 · 09-S4A public AOT write barrier API + closure heap-owner boundary ·
  状态：09-S4 子切片完成；完整 09-S4 继续进行中，object/member/index 直接生成 C 屏障与新生代 owner
  编译期消除仍未关闭。完成项目：核心 GC 新增 `ZrCore_Gc_WriteBarrier(state, ownerObject, value)`，
  作为 AOT 白名单写屏障入口，当前委托 `ZrCore_Value_Barrier()` 复用现有引用值过滤和 remembered-set
  维护；AOT runtime 中闭包/capture heap-owner 写入路径的 value-based barrier 全部从
  `ZrCore_Value_Barrier(state, ...)` 迁到 `ZrCore_Gc_WriteBarrier(state, ...)`；guardrail 将
  `ZrCore_Gc_SafePoint` 与 `ZrCore_Gc_WriteBarrier` 都归入允许的显式 runtime 边界；value-type
  generated-C smoke 验证栈上 inline struct 字段 store 不出现多余 `ZrCore_Gc_WriteBarrier(`。
  RED/GREEN：RED 为 `test_aot_c_constant_contracts` 缺少 `ZrCore_Gc_WriteBarrier` API/source marker
  且 AOT runtime 仍包含旧 `ZrCore_Value_Barrier(state, ...)`；GREEN 后 source contract 要求
  `aot_runtime.c` 不再出现旧 value barrier，核心 root-frame 测试新增 old-to-young value barrier
  remembered-set 断言并通过。测试结果：WSL gcc direct root/barrier 4/0、constant contracts 5/0、
  guardrail 6/0、value-type smoke 2/0、global smoke 10/0；WSL clang direct 同组 4/0、5/0、6/0、
  2/0、10/0；Windows MSVC Debug direct root/barrier 4/0、constant contracts 5/0、guardrail 6/0、
  value-type smoke 2/0（1 ignored，Unix execution branch）、global smoke 10/0（10 ignored，Unix
  shared-library path）。产出：`tests/acceptance/2026-06-25-aot-09-s4a-write-barrier-api.md`。
  备注：`zr_vm_library/src/zr_vm_library/aot_runtime.c` 已是大文件；本切片只替换 3 个既有屏障调用，
  不在运行时边界重构期间拆分。后续 S4B 应优先针对 member/index heap-owner 写入设计更细粒度
  生成 C 或 AOT runtime slot-level 屏障，并验证可证明新生代 owner 的屏障消除。

- 2026-06-25 08:53:00 +08:00 · 09-S3A safepoint API + allocation/call/back-edge insertion ·
  状态：09-S3 safepoint 插入路径完成；09 阶段继续进行中，09-S4 写屏障、09-S5 装箱/FFI pin
  仍未完成。完成项目：核心 GC 暴露 `ZrCore_Gc_SafePoint(state)`，当前实现委托
  `ZrCore_GarbageCollector_CheckGc(state)` 推进 pending GC debt；AOT C emitter 新增
  `backend_aot_write_c_gc_safepoint()`，生成带 marker 的 `ZrCore_Gc_SafePoint(state);`；
  `CREATE_OBJECT` / `CREATE_ARRAY` 分配 lowering 在 runtime alloc 后插入 allocation safepoint；
  函数体 dispatch 在 dynamic/static typed/direct call 三类调用路径后统一插入 call safepoint；
  `JUMP`、generic truthiness branch、bool/typed signed branch、iterator move-next branch 只在
  `targetInstructionIndex <= instructionIndex` 的回边前插入 back-edge safepoint。RED/GREEN：RED 为新增
  root-frame/safepoint test 与 source contract 先失败，缺少 `ZrCore_Gc_SafePoint` API 及 emitter/source
  marker；GREEN 后 runtime debt 推进、source contract、generated-C safepoint smoke、value-type smoke
  和 iterator smoke 均通过。测试结果：WSL gcc direct `zr_vm_aot_gc_root_frame_test` 3/0、
  `zr_vm_aot_c_control_contracts_test` 2/0、`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_iterator_contracts_test` 1/0、`zr_vm_aot_c_control_shared_library_smoke_test` 2/0，
  CTest `aot_gc_root_frame` 1/1；WSL clang direct root-frame 3/0、control contracts 2/0、
  control shared-library smoke 2/0、value-type smoke 2/0，CTest `aot_gc_root_frame` 1/1；
  Windows MSVC Debug direct root-frame 3/0、control contracts 2/0、control shared-library smoke
  2/0（2 ignored，Unix shared-library path）、value-type smoke 2/0（1 ignored，Unix execution branch），
  CTest `aot_gc_root_frame` 1/1。产出：
  `tests/acceptance/2026-06-25-aot-09-s3a-safepoint-insertion.md`。备注：`zr_vm_aot_c_logical_contracts_test`
  仍有 2 个既有 source-contract 失败，调查结果为 `backend_aot_c_scalar_locals.c` 已改成通用
  result-opcode 推导后，旧测试仍查找过时的显式 scalar-local 文本；该失败与 09-S3A safepoint
  插入无关，未在本切片混入修复。`backend_aot_c_function_body.c` 已超过大文件阈值，本次只做
  dispatch 参数与 safepoint 调用接线，未拆分；后续最小拆分边界是把 call/branch dispatch
  从函数体 orchestration 中抽出。

- 2026-06-25 08:14:05 +08:00 · 09-S2B runtime AOT root-frame stack + generated-C push/pop ·
  状态：09-S2 默认 frame-byte-offset 根路径完成；09 阶段继续进行中，09-S3 safepoint、09-S4 写屏障、
  09-S5 装箱/FFI pin 仍未完成。完成项目：`SZrState` 新增 AOT root-frame stack carrier；
  `ZrCore_Gc_AotRootFramePush/Pop/Depth` 管理 `(frame base, root map)` 单次登记；GC mark/rewrite
  路径遍历 `SZrAotGcRootMap` 的 frame-byte-offset slots，使 AOT byte-frame 中的 `SZrTypeValue` 根可在
  minor GC 中保活并跟随 forwarding address 重写；AOT runtime generated-module context 绑定
  `SZrAotMethodInfo`，generated C 仅在 method metadata 有非空 `gcRootMap` 时生成 prologue push 与
  epilogue pop，POD/blittable 继续不生成 root-frame 调用。RED/GREEN：RED 为新增
  `zr_vm_aot_gc_root_frame_test` 编译失败，缺少 `SZrAotGcRootFrame`、state root-stack 字段与
  `ZrCore_Gc_AotRootFrame*` API；GREEN 后 root-stack push/pop 平衡测试与 “stackTop 以上 AOT root
  经 minor GC 保活/重写” 均通过，frame setup source 合同和 value-type generated-C smoke 也通过。
  测试结果：WSL gcc direct `zr_vm_aot_gc_root_frame_test` 2/0、`zr_vm_aot_c_frame_setup_contracts_test`
  1/0、`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0，CTest `aot_gc_root_frame` 1/1，
  `zr_vm_gc_test` 66/0；WSL clang direct root-frame 2/0、frame setup 1/0、value-type smoke 2/0，
  CTest `aot_gc_root_frame` 1/1；Windows MSVC Debug direct root-frame 2/0、frame setup 1/0、
  value-type smoke 2/0（Unix shared-library execution branch 1 ignored），CTest `aot_gc_root_frame`
  1/1。产出：`tests/core/test_aot_gc_root_frame.c`、
  `tests/acceptance/2026-06-25-aot-09-s2b-runtime-root-frame-stack.md`。备注：本记录不声明
  `LOCAL_ADDRESS` 根位置、安全点插入、写屏障消除、装箱或 FFI pin 完成。

- 2026-06-25 07:10:49 +08:00 · 09-S2A AOT GC root map descriptor ABI + generated-C publication ·
  状态：09-S2 子切片完成；09-S2 仍为部分完成，runtime 根栈登记、safepoint 压力 GC、移动后根更新、
  写屏障、装箱/FFI pin 仍未完成。完成项目：公共 AOT ABI 新增 `EZrAotGcRootLocationKind`、
  `SZrAotGcRootSlot` 与 `SZrAotGcRootMap`；AOT C method metadata emitter 现在从 ExecIR frame layout
  遍历 `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT` 槽，经
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout()` 解析唯一 `SZrTypeLayout`，对 `gcFieldOffsets` /
  GC value fields 生成 frame-byte-offset root slots；含引用字段的 generated method info 将
  `.gcRootMap` 指向 `zr_aot_gc_root_map_<flatIndex>`，POD/blittable struct 和纯标量函数仍写
  `.gcRootMap = ZR_NULL`。RED/GREEN：RED 为新增 value-type smoke 编译生成 C 时缺少
  `SZrAotGcRootMap` / `ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET`；GREEN 后生成共享库可通过
  `ZrVm_GetAotCompiledModule()` 读取 `methodInfos[0]->gcRootMap`，确认 root map 非空、root slots
  均使用 `FRAME_BYTE_OFFSET` 定位，且至少一个 root 精确绑定到 string-field struct descriptor 的
  `typeLayoutId` 与 GC field offset；POD 生成物不含 root map/static root slots。验证：WSL gcc 直接
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0；
  WSL gcc CTest `aot_c_type_layout_contracts|aot_c_generic_monomorphization|aot_c_generic_reference_sharing|aot_c_generic_call_typed`
  4/4；WSL clang 直接同两目标 1/0、2/0，CTest 同组 4/4；Windows MSVC Debug 直接 frame setup
  1/0、value-type smoke 2/0（Unix shared-library execution 分支 1 ignored），CTest
  `aot_c_type_layout_contracts|aot_c_generic_reference_sharing|aot_c_generic_call_typed` 3/3。产出：
  `tests/acceptance/2026-06-25-aot-09-s2a-gc-root-map-descriptor.md`。备注：本切片只发布 root-map
  ABI 与 generated-C method metadata；不会在 runtime GC 根栈中 push/pop 这些 maps，也不触发 safepoint
  或移动后重写，因此不能关闭完整 09-S2。

- 2026-06-24 16:54:42 +08:00 · 09-S1B AOT GC descriptor module table publication ·
  状态：09-S1 完成；09 阶段继续进行中，09-S2 精确栈根、09-S3 safepoint、09-S4 写屏障、09-S5 装箱/FFI pin
  仍未完成。完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 6u`，新增
  `SZrAotGcDescriptor`，`ZrAotCompiledModule` 新增 `gcDescriptors` / `gcDescriptorCount` 模块级表；
  AOT C type-layout emitter 改用公共 descriptor 类型，按 `typeLayoutId` 生成稀疏
  `zr_aot_gc_descriptors[]`，无 GC 字段的 POD/blittable 模块保持 `ZR_NULL`/`0u`；生成共享库 smoke
  通过 `ZrVm_GetAotCompiledModule()` 读取 string-field struct 的 descriptor 并校验 `gcFieldCount==1`。
  RED/GREEN：RED 为新增 value-type smoke 在编译期失败，缺少 `SZrAotGcDescriptor` 与
  `ZrAotCompiledModule.gcDescriptors` / `gcDescriptorCount`；GREEN 后公共 ABI、source contract、
  value-type shared-library smoke、普通 shared-library smoke 和 descriptor diagnostics 全部通过。
  验证：`zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_type_layout_contracts_test` 1/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_descriptor_diagnostics_test` 1/0、
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0。
  产出：`tests/acceptance/2026-06-24-aot-09-s1b-gc-descriptor-module-table.md`。
  备注：本切片关闭 09-S1 的 descriptor 发射、核心扫描与模块 metadata 注册面；运行期 token/layout
  lazy hydration、精确 AOT 栈根、safepoint 和写屏障分别留给 11/09 后续切片。

- 2026-06-24 14:20:45 +08:00 · 09-S1A GC descriptor offset-list emission + metadata scan path ·
  状态：09-S1 子切片完成；完整 09-S1 仍未关闭，生成 descriptor 尚未接入 AOT 方法/模块 metadata 注册面。
  完成项目：`ZrCore_TypeLayout_VisitGcValues()` 在非 union struct 且 `gcFieldOffsets` 可用时优先按
  metadata offset 表访问 `SZrTypeValue` 引用槽；AOT C type-layout emitter 为 `gcFieldCount > 0` 的
  inline struct 生成 `ZrGcOffsets_<id>[]` 与 `ZrGcDescriptor_<id>`，并对 byte offset 做 `SZrTypeValue`
  边界校验；`gcFieldCount == 0` 的 blittable/POD struct 不生成 descriptor。RED/GREEN：RED 为
  `VisitGcValues()` 只按字段表扫描、AOT C 只发 `ZrLayout_*`/`_Static_assert` 而没有 GC descriptor；
  GREEN 后 metadata offset 表测试确认即使字段表重复也按 offset 表访问两个引用槽，生成 C 测试确认
  string 字段 struct 生成 `zr_aot_gc_descriptor_offsets` 而纯 `int` struct 不生成 descriptor。
  验证：`zr_vm_value_type_runtime_test` 14/0、`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_type_layout_contracts_test` 1/0。产出：
  `tests/acceptance/2026-06-24-aot-09-s1a-gc-descriptor-offsets.md`。
  备注：当前 descriptor 仍是生成 C 内的静态契约和后续注册载体；要关闭完整 09-S1，还需要把该表
  通过 AOT MethodInfo/模块 metadata 注册给运行期布局，使 AOT 加载路径直接使用生成 descriptor。
