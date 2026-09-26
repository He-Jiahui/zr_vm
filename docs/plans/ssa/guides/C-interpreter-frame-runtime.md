---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/object.h
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/architecture-design.md
doc_type: implementation-guide
status: planned
---

# 实现指南 C：解释器边界、Guard 缓存与 Frame/调用运行时（03.01–03.03、04.01–04.02）

> 待实现草案。现有事实已核实：dispatch 用 `goto *fastDispatchTable[...]`（execution_dispatch.c:2540）；`SZrState` 持 `stackBase/stackTop/stackTail`（TZrStackPointer union）、`callInfoList`、异常/`pendingControl`/debug 字段（state.h:62-131）；`SZrCallInfo` 持 `functionBase/functionTop/metadataFunction/previous/next`（call_info.h:67）；`SZrObjectPrototype` 已有 `shapeId/shapeGeneration/layoutGeneration`（object.h:223-225）。

## C.1 局部执行上下文与 publish/reload（03.01）

现状问题：dispatch 循环直接读写 `state->stackTop`/`callInfo->functionBase` 等字段，每条指令都有内存往返。目标是把热字段收进一个寄存器友好的局部结构，只在边界同步：

```c
/* zr_vm_core/src/zr_vm_core/execution/execution_context.h —— 待实现草案 */
typedef struct SZrExecutionLocalContext {
    const TZrInstruction *pc;            /* 复用现有 TZrInstruction union */
    TZrStackValuePointer frameBase;      /* == callInfo->functionBase 的缓存 */
    TZrStackValuePointer stackTop;
    SZrCallInfo *callInfo;
    struct SZrGcDomain *domain;
    SZrProfileRuntime *profile;          /* 复用 profile.h 现有 thread-local runtime */
    TZrUInt64 activeGeneration;          /* 本 frame 进入时的 version generation（08.03）*/
    const struct SZrExecIrFrameLayout *frameLayout;
} SZrExecutionLocalContext;

/* publish：把局部状态写回 SZrState，使 GC/debug/异常路径可见。
   ★ 必须在任何 mayGc/mayThrow/native/suspend/debug 边界前调用。 */
ZR_FORCE_INLINE void zr_execution_publish(SZrState *state,
        const SZrExecutionLocalContext *local) {
    state->stackTop.valuePointer = local->stackTop;
    local->callInfo->functionTop.valuePointer = local->stackTop;
    state->previousProgramCounter =
        (TZrMemoryOffset)(local->pc - zr_execution_code_base(local->callInfo));
    /* pc 存偏移而非指针：GC 移动 code object 后偏移仍有效 */
}

/* reload：边界返回后重取一切可能被移动/切换的东西。
   ★ 旧 frameBase/stackTop/frameLayout 一律视为失效，无条件重读。 */
ZR_FORCE_INLINE void zr_execution_reload(SZrState *state,
        SZrExecutionLocalContext *local) {
    local->callInfo = zr_state_current_call_info(state);
    local->frameBase = local->callInfo->functionBase.valuePointer;
    local->stackTop = state->stackTop.valuePointer;
    local->pc = zr_execution_code_base(local->callInfo)
              + state->previousProgramCounter;
    /* domain/profile/generation 同理；generation 从 callInfo 所持
       version record 读，绝不从全局最新读（03.03/08.03 亲和性） */
}
```

迁移策略（03.01 批次 1 的"无行为拆分"具体做法）：先引入 context 结构但让字段直接别名 state（宏 `#define ZR_LOCAL_TOP local.stackTop` 阶段性指向 `state->stackTop`），逐 handler family 切换到真局部缓存，每切一族跑全量 opcode 差分。这样任何一步都可 bisect。

leaf handler 合约的机器检查：`ZR_DEBUG` 构建下 publish/reload 各自翻转 `state` 上的 `boundaryEpoch` 计数；instrumented allocator/throw 入口断言 `boundaryEpoch 为已发布态`，违例即 03.01 断言草案的 `BOUNDARY_LEAF_ALLOCATED`。

## C.2 Guard 检查的指令序（03.03）

```c
ZR_FORCE_INLINE EZrBindingGuardResult zr_execution_check_binding_guard(
        const SZrCallBinding *binding,
        const SZrExecutionLocalContext *local,
        const SZrObjectPrototype *receiverPrototype) {
    /* 判序固定为 generation → shape → layout，且短路。理由：
       generation 失配意味着热更/失效已发生，后续字段不可信；
       shape 失配是常态多态 miss，最高频，放第二位使 STALE 检查
       不污染热路径分支预测。 */
    if (ZR_UNLIKELY(binding->generation != local->activeGeneration)) {
        return ZR_BINDING_GUARD_STALE;         /* → 结构化 link error 路径 */
    }
    if (ZR_UNLIKELY(binding->target.ownerLayoutGeneration
                    != receiverPrototype->layoutGeneration)) {
        /* 复用 object.h:225 现有字段。layout 变化 ⇒ 此前 offset 证明作废 */
        return ZR_BINDING_GUARD_POLY_MISS;     /* → 同 token/slot 基线操作 */
    }
    if (ZR_UNLIKELY(binding->contract.layoutHash
                    != receiverPrototype->currentLayoutHash)) {
        return ZR_BINDING_GUARD_POLY_MISS;
    }
    return ZR_BINDING_GUARD_HIT;
}
```

三个出口的落点：

```text
HIT        → 直接用 binding->target（union 按 targetKind 分派，
             call_binding.h 现有 VM/NATIVE/AOT 三臂）
POLY_MISS  → zr_execution_rebind_site()：按 binding->contract 的
             targetMetadataToken + signatureToken 走既有
             metadata_runtime_method_binding.c 解析路径重绑；
             解析失败 = 编译期本应拒绝的站点 ⇒ 升格 STALE 处理。
             命中率计数 ++，超过站点 PIC 容量转 megamorphic slot。
             ★ 全程无成员名字符串参与——token 就是查找键。
STALE      → 填 state->lastCallBindingError（state.h:103 现有字段）为
             {ZR_CALL_BINDING_STALE_GENERATION, token, instructionIndex}，
             走 ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_LINK_ERROR)。
             测试断言：nameLookupCount == 0 且 diagnostic.status 精确。
```

PIC 站点结构（4 entry 内联 + 溢出链）：

```c
typedef struct SZrPolymorphicCacheSite {
    struct { TZrUInt64 shapeId; TZrUInt32 bindingRow; TZrUInt32 hitCount; } entries[4];
    TZrUInt32 entryCount;
    TZrUInt32 missCount;                 /* 连续 miss 超阈值 → megamorphic */
    TZrUInt32 invalidateCount;           /* generation 失效次数，03.04 预算输入 */
    TZrUInt32 megamorphicSlot;           /* 转入 slot dispatch 后的槽位 */
} SZrPolymorphicCacheSite;
/* 站点数组按 function 存 side table（固定宽度指令不扩位），
   指令 operandExtra 存站点下标——复用 SZrInstruction 现有 16 位副操作数。 */
```

## C.3 Frame 布局构建算法（04.01）

`ZrParser_ExecIr_LayoutFrame` 的分配顺序决定一切下游（GcMap、transfer plan、AOT frame）：

```text
输入：函数的 value 列表 + 生命周期区间（liveness）+ 类型/ownership
步骤 1  槽位分类：每个需要落栈的 value 按 §8 四类分类
        （BOXED / SCALAR / INLINE_SPAN / REF）。判据：
        - 被 borrow / 地址观察 / 跨边界 → BOXED（完整 SZrTypeValue）
        - 纯标量且 escape=NONE → SCALAR
        - inline struct 且 layout 已知 → INLINE_SPAN
        - GC 引用且无需完整 value 语义 → REF
步骤 2  布局顺序：参数 prefix 先（caller 直写约定），
        然后 REF 槽连续排列（GcMap 位图因此致密），
        然后 SCALAR 按 alignment 降序（8→4→1，无 padding 浪费），
        INLINE_SPAN 按各自 alignment 插入，BOXED 最后。
步骤 3  同生命周期不重叠的槽做着色复用（interval graph 着色，
        线性扫描即可）；REF 槽复用必须在切换点插 clear——
        未初始化 REF 槽不得被 GC 扫描（04.01 断言）。
步骤 4  计算 storageSlotCount（物理）/logicalSlotCount（调试观察用），
        全部 offset 经 checkedAdd/checkedMul，超 frame 上限
        在布局期报错，绝不到运行期。
        layoutHash = 逐 slot {class,offset,type} 的稳定 hash，
        进 artifact 与 guard。
```

## C.4 调用传递与尾调用搬移（04.02）

五类传递计划的分类谓词（编译期一次判定，运行期查表执行）：

```c
typedef enum EZrCallTransferKind {
    ZR_CALL_TRANSFER_SCALAR_COPY = 0,   /* 标量：memcpy 8/4 字节 payload */
    ZR_CALL_TRANSFER_SPAN_COPY,         /* inline span：memcpy + 内部 ref 逐个 barrier */
    ZR_CALL_TRANSFER_MOVE,              /* ownership move：位拷 + 源槽置 moved-from */
    ZR_CALL_TRANSFER_BORROW,            /* 传 place 引用：借用区间 ⊆ 调用区间已证明 */
    ZR_CALL_TRANSFER_BOXED_BRIDGE       /* 兜底：完整 SZrTypeValue copy + barrier */
} EZrCallTransferKind;
```

尾调用重叠参数搬移——先建"读依赖图"再拓扑序执行，环用一格中转：

```c
/* execution_tail_call.c 内扩展（既有文件，见 04.02 文件表）。
   问题：尾调用参数源槽与目标槽同帧重叠，直接顺序搬移会覆盖未读源。 */
static void zr_execution_move_tail_arguments(SZrExecutionLocalContext *local,
        const SZrCallTransferPlan *plan) {
    /* plan->moves[] 是 {sourceSlot, targetSlot, kind} 数组。
       1. 计数每个槽作为 source 的未完成读（inDegree on target）。
       2. worklist 起始 = 所有 targetSlot 不是任何未完成 move 的 source 的项。
       3. 依次执行并递减其 source 的计数，新变为 0 的入列。
       4. worklist 空而 move 未尽 ⇒ 存在环：取环上任一项，把其 source
          搬到 return buffer 中转槽（04.01 已保证 returnBufferOffset 可用），
          断开环后继续。中转最多一格——单环性质由 target 唯一保证。 */
}
```

尾调用 eligibility 在编译期由 ExecIR 判定并烙进指令 flags；运行期只补一个 debug 策略检查（`state->allowDebugHook` 活跃且策略要求保帧时走普通调用）。`noPendingCleanup` 直接查 ExecIR cleanup scope 覆盖，`noEscapingFrameAlias` 来自 02.03 逃逸事实——运行期零重新分析。

## C.5 冷路径分离的机械做法（03.01 批次 3）

- 所有错误构造/动态 meta/异常慢路径函数标 `ZR_NO_INLINE ZR_COLD`（新增宏进 zr_common_conf.h：GCC/Clang `__attribute__((noinline,cold))`，MSVC `__declspec(noinline)` + `/Gw`）。
- 热 handler 内的失败分支只留 `if (ZR_UNLIKELY(...)) return zr_execution_cold_xxx(state, local, ...);` 一行，参数打包最小化。
- 验证手段写进测试而非口头：对比拆分前后 `objdump --disassemble` 的热循环函数体积（GCC/Clang），MSVC 用 `/Fa` 汇编清单；03.01 退出门禁的"检查汇编/代码尺寸"落成 CI 可跑的尺寸回归脚本（阈值 ±5%）。
