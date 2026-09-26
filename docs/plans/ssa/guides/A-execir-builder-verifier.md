---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_core/include/zr_vm_core/memory.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/architecture-design.md
doc_type: implementation-guide
status: planned
---

# 实现指南 A：ExecIR 模型、Builder、SSA 构造与 Verifier（01.01–01.03）

> 本文代码全部为**待实现草案**，展示完整算法骨架与错误处理路径；落地时以叶子计划（01.01/01.02/01.03）的任务与测试为准。所有现有类型引用（`SZrSemanticIrInstruction`、`TZrValueId`、`SZrFileRange`、`ZrCore_Memory_*`）已对照仓库真实声明核实。

## A.1 Arena 与容器基础设施（01.01 批次 1）

ExecIR module 拥有单一 arena，所有 side array 从 arena 分配，`FreeModule` 一次回收。arena 直接封装 `ZrCore_Memory_Allocate`（memory.h:14），不引入新分配器：

```c
/* zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_internal.h —— 内部头，不进公开 include */
typedef struct SZrExecIrArenaBlock {
    struct SZrExecIrArenaBlock *previous;
    TZrSize used;
    TZrSize capacity;
    /* 数据紧随其后 */
} SZrExecIrArenaBlock;

typedef struct SZrExecIrArena {
    struct SZrGlobalState *global;      /* 分配走 ZrCore_Memory_RawMallocWithType */
    SZrExecIrArenaBlock *head;
    TZrSize defaultBlockSize;           /* 64 KiB 起步，超大请求单独成块 */
} SZrExecIrArena;

/* 溢出检查是每个容器增长的强制前置。返回 ZR_FALSE 时 *outBytes 未定义。 */
ZR_FORCE_INLINE TZrBool zr_exec_ir_checked_array_bytes(
        TZrUInt32 count, TZrSize elementSize, TZrSize *outBytes) {
    if (count != 0 && elementSize > (SIZE_MAX / count)) {
        return ZR_FALSE;                /* 01.01 断言草案：reject before allocation */
    }
    *outBytes = (TZrSize)count * elementSize;
    return ZR_TRUE;
}
```

通用增长 vector：所有 `instructions/values/blocks/operandPool/...` 复用同一 helper，容量策略 `max(8, capacity + capacity/2)`：

```c
typedef struct SZrExecIrVector {
    void *data;
    TZrUInt32 count;
    TZrUInt32 capacity;
} SZrExecIrVector;

static TZrBool zr_exec_ir_vector_reserve(SZrExecIrArena *arena, SZrExecIrVector *vector,
        TZrSize elementSize, TZrUInt32 needed, SZrExecIrDiagnostic *diagnostic) {
    if (needed <= vector->capacity) return ZR_TRUE;
    TZrUInt32 newCapacity = vector->capacity < 8u ? 8u : vector->capacity + vector->capacity / 2u;
    if (newCapacity < needed) newCapacity = needed;
    TZrSize bytes;
    if (!zr_exec_ir_checked_array_bytes(newCapacity, elementSize, &bytes)) {
        zr_exec_ir_diag_set(diagnostic, ZR_EXEC_IR_STATUS_CAPACITY_OVERFLOW,
                            /*blockId*/0, /*instructionId*/0);
        return ZR_FALSE;
    }
    void *grown = zr_exec_ir_arena_allocate(arena, bytes, ZR_EXEC_IR_ALIGN_DEFAULT);
    if (grown == NULL) {
        zr_exec_ir_diag_set(diagnostic, ZR_EXEC_IR_STATUS_OUT_OF_MEMORY, 0, 0);
        return ZR_FALSE;                /* arena 旧块保留，module 仍可安全 Free */
    }
    if (vector->count != 0) {
        ZrCore_Memory_RawCopy(grown, vector->data, (TZrSize)vector->count * elementSize);
    }
    vector->data = grown;               /* arena 分配不回收旧块——克隆/构建期空间换安全 */
    vector->capacity = newCapacity;
    return ZR_TRUE;
}
```

设计要点：arena 增长**不释放旧块**，因此任何持有旧指针的悬垂都被避免；构建期空间放大在 seal 时通过一次紧凑 clone（`ZrCore_ExecIr_CloneModule` 到精确容量）回收。这就是 01.01 "指令数组允许构建期增长，发布后只读" 的实现机制。

## A.2 CloneModule 的失败原子性（01.01 批次 2）

克隆遵循 "失败不修改 source、destination 可安全 Free" 的契约。实现模式是**先全量分配后填充**：

```c
TZrBool ZrCore_ExecIr_CloneModule(const SZrExecIrModule *source,
        SZrExecIrModule *destination, SZrExecIrDiagnostic *diagnostic) {
    ZrCore_Memory_RawSet((TZrPtr)destination, 0, sizeof(*destination));
    if (!zr_exec_ir_arena_init(&destination->arena, source->arena.global)) {
        zr_exec_ir_diag_set(diagnostic, ZR_EXEC_IR_STATUS_OUT_OF_MEMORY, 0, 0);
        return ZR_FALSE;                /* destination 全零：Free 是无害 no-op */
    }
    /* 阶段 1：按 source 的精确 count（非 capacity）分配所有目标数组。
       任何一步失败 → 释放 destination arena 整体，source 从未被触碰。 */
    for (TZrUInt32 f = 0; f < source->functionCount; ++f) {
        if (!zr_exec_ir_clone_function_shell(&destination->arena,
                &source->functions[f], &destination->functions[f], diagnostic)) {
            ZrCore_ExecIr_FreeModule(destination);
            return ZR_FALSE;
        }
    }
    /* 阶段 2：填充与重映射。局部 ID（ValueId/BlockId/InstructionId）在克隆中
       保持数值不变——因为它们是函数内索引；跨函数移动才需要重编号，
       由单独的 zr_exec_ir_renumber_function 完成。稳定 token
       （TZrMetadataToken/TypeToken/layoutHash）逐字段复制，克隆前后 hash 一致
       是 01.01 测试断言。 */
    ...
}
```

分配失败注入测试（01.01 断言草案 "fail allocator at side-array allocation N"）：给 arena 加 `failAtAllocationIndex` 测试钩子字段（仅 `ZR_DEBUG` 编译），Unity 测试循环 N=1..K 逐点注入并断言 source 校验和不变、destination Free 后 LSan 无泄漏。

## A.3 SemIR → ExecIR lowering（01.02 批次 1）

输入是 semantic_ir.h 现有事实：`SZrSemanticIrInstruction`（含 `opcode/typeId/placeId/valueId/resultValueId/ownershipOperation/targetBlockId/loanId/regionId`）、`SZrSemanticIrValue`（含 `definitionInstructionId/sourceRange`）、cleanup scope 与 loan facts。lowering 分四步，每步产出可独立断言：

```text
步骤 1  块划分：leader 集合 = {入口, 每个 targetBlockId 目标, 每条
        BRANCH/SWITCH/RETURN/THROW 的后继, 每个 cleanup scope 的
        firstInstructionIndex, 每个 CALL_* 的异常后继}。
        SemIR 的 SCOPE_ENTER/SCOPE_EXIT 不生成 ExecIR 指令，而是转为
        block 级 cleanup 边（见步骤 2）。
步骤 2  建边：普通边 + 异常边 + cleanup 边。凡带 MAY_THROW flag 的
        ExecIR 指令，若处于活跃 cleanup scope 内，其所在块必须有一条
        到 cleanup landing block 的异常后继；这是 verifier EFFECT 级
        检查 "mayThrow 边覆盖" 的数据来源。
步骤 3  逐指令翻译：opcode 映射表由 exec_ir_opcode.def 的 X-macro 第二次
        展开生成（ZR_SEMANTIC_IR_LOAD → ZR_EXEC_IR_OP_LOAD 等）。
        SemIR 的 place 投影链（PLACE_BASE + PLACE_PROJECT*）折叠为
        ExecIR 单条 PLACE_PROJECT 携带投影路径 range；
        ownershipOperation 展开为独立 DROP/MOVE/COPY 指令而不是标志位，
        使 DCE/逃逸分析能独立处理它们。
步骤 4  source map：SemIR 的 SZrSemanticIrSourceMapEntry{instructionId,
        sourceRange} 直接转存 sourcePool；每条 ExecIR 指令的 sourceId
        指向翻译它的 SemIR 指令的 range。多对一（折叠的投影链）取
        最外层 range。
```

翻译期临时映射表（`semanticValueId → execValueId`、`semanticBlockLeader → execBlockId`）放栈上 scratch arena，函数结束整体丢弃，不进 module arena。

## A.4 支配树：Cooper–Harvey–Kennedy（01.02 批次 2）

选 CHK 而非 Lengauer–Tarjan：实现约 60 行、无辅助森林、对 <10k 块函数与 LT 差距可忽略，且逆后序数组本身被 phi 放置与后续 pass 复用。

```c
TZrBool ZrParser_ExecIr_ComputeDominators(SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    /* 1. 从 ENTRY 出发 DFS 得逆后序（reverse postorder）编号；
          不可达块 postorderIndex 保持 UINT32_MAX，本 pass 不处理——
          02.01 的 CFG 清理负责删除，verifier STRUCTURE 级报告它们。 */
    TZrUInt32 *postorderOf;   /* blockId -> rpo index，scratch */
    TZrExecIrBlockId *rpoOrder; /* rpo index -> blockId */
    zr_exec_ir_compute_reverse_postorder(function, &postorderOf, &rpoOrder);

    /* 2. 迭代求不动点。immediateDominator 数组以 INVALID 起始，
          entry 的 idom 是自身。 */
    function->blocks[ZR_EXEC_IR_BLOCK_ID_ENTRY].immediateDominator = ZR_EXEC_IR_BLOCK_ID_ENTRY;
    TZrBool changed = ZR_TRUE;
    while (changed) {
        changed = ZR_FALSE;
        for (TZrUInt32 i = 1; i < function->reachableBlockCount; ++i) { /* 跳过 entry，按 rpo */
            TZrExecIrBlockId b = rpoOrder[i];
            TZrExecIrBlockId newIdom = ZR_EXEC_IR_BLOCK_ID_INVALID;
            const SZrExecIrRange predecessors = function->blocks[b].predecessors;
            for (TZrUInt32 p = 0; p < predecessors.count; ++p) {
                TZrExecIrBlockId pred = function->blockRefPool[predecessors.offset + p];
                if (function->blocks[pred].immediateDominator == ZR_EXEC_IR_BLOCK_ID_INVALID)
                    continue;           /* 尚未处理的前驱先跳过 */
                newIdom = (newIdom == ZR_EXEC_IR_BLOCK_ID_INVALID)
                        ? pred
                        : zr_exec_ir_intersect(function, postorderOf, newIdom, pred);
            }
            if (function->blocks[b].immediateDominator != newIdom) {
                function->blocks[b].immediateDominator = newIdom;
                changed = ZR_TRUE;
            }
        }
    }
    return ZR_TRUE;
}

/* 经典 two-finger intersect：沿 idom 链向上走到公共祖先 */
static TZrExecIrBlockId zr_exec_ir_intersect(const SZrExecIrFunction *function,
        const TZrUInt32 *postorderOf, TZrExecIrBlockId a, TZrExecIrBlockId b) {
    while (a != b) {
        while (postorderOf[a] < postorderOf[b])
            a = function->blocks[a].immediateDominator;
        while (postorderOf[b] < postorderOf[a])
            b = function->blocks[b].immediateDominator;
    }
    return a;
}
```

支配边界（DF）在 idom 收敛后一遍计算（对每个 join 块：沿每个前驱的 idom 链向上直到 idom(b)，途经块的 DF 加入 b），存入 pass manager 的分析缓存（guide B §B.1），不进 module 本体。

## A.5 phi 放置与重命名（01.02 批次 3）

对每个需要 SSA 化的实体（SemIR place 的局部槽、每类 memory token、effect 链）运行同一套算法：

```text
放置（iterated dominance frontier）：
  worklist = 该实体的所有 def 块
  while worklist 非空:
      x = pop
      for y in DF(x):
          if y 未放置该实体的 phi:
              放置 phi 于 y（incomings 数 = y.predecessors.count，先填 INVALID）
              if y 不在 def 集合: worklist.push(y)

重命名（支配树 DFS，每实体一个版本栈）:
  visit(block):
      for phi in block.phis: 压栈 phi.result
      for inst in block.instructions:
          把 operand 中对该实体的引用替换为栈顶版本
          if inst 定义该实体: 压栈新版本
      for succ in block.successors:
          填 succ 中该实体 phi 的第 k 个 incoming（k = 本块在 succ.predecessors 中的下标）
          ★ 这里就是 "phi incoming 与 predecessors 顺序对齐" 不变量的建立点
      for child in dominatorTreeChildren(block): visit(child)
      弹出本块压入的所有版本
```

实现细节：版本栈用一条共享 scratch 栈 + 每实体记录深度水位，避免每实体独立分配；`dominatorTreeChildren` 从 idom 数组一次反转构建。memory token 的 phi 与值 phi 共用 phiPool，靠 result 的 value kind 区分——verifier SSA 级检查 token phi 的所有 incoming 均为同 memory class 的 token。

## A.6 Verifier 三级实现（01.03）

verifier 是唯一允许遍历一切的组件，实现为单文件三个入口，逐级包含：

```c
/* STRUCTURE 级：任何 IR 一律先过。O(n)，无需支配信息。 */
static TZrBool zr_exec_ir_verify_structure(const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    /* 每块非空且最后一条是 terminator（BRANCH/SWITCH/RETURN/THROW/SUSPEND 尾）*/
    /* 所有 range.offset + range.count 不越界所属 pool（checked add）*/
    /* 每条指令 operand 数与 .def 声明的 arity 一致（VARIADIC 除外）*/
    /* 每块 phis 的 incoming 数 == predecessors.count —— 违例报
       ZR_EXEC_IR_VERIFY_PHI_PREDECESSOR_MISMATCH{blockId, phiIndex} */
    /* successors/predecessors 互逆：b∈succ(a) ⇔ a∈pred(b) */
}

/* SSA 级：需要支配树（按需触发 ComputeDominators）。 */
static TZrBool zr_exec_ir_verify_ssa(const SZrExecIrFunction *function, ...) {
    /* 每个 ValueId 恰有一个定义点（definition 字段反查一致）*/
    /* def 支配 use：同块用指令序，跨块用 idom 链；phi 的第 k 个 incoming
       只需被第 k 个前驱块的出口支配 */
    /* token 线性：每类 memory token 在任一执行路径上被恰好消费一次
       （memoryOut 的 def 到下一个 memoryIn 的 use 之间无第二消费者）*/
}

/* EFFECT 级：优化 pass 之后追加。 */
static TZrBool zr_exec_ir_verify_effect(const SZrExecIrFunction *function, ...) {
    /* 每条 MAY_THROW 指令所在块存在异常后继或函数级 propagate 声明 */
    /* STORE/BARRIER/DROP/THROW/SUSPEND/native CALL 的 memory class
       与 .def 声明一致，未被优化 pass 篡改 */
    /* GcMap：每条 MAY_GC 指令有 map entry，entry 的 liveRefSlots
       与该点活跃分析结果一致（这里复用 liveness，不重算）*/
}
```

诊断结构固定五元组，负测逐 kind 断言（malformed IR fixture 直接手工构造坏 IR，不经 builder）：

```c
typedef struct SZrExecIrDiagnostic {
    EZrExecIrVerifyErrorKind errorKind;   /* 精确枚举，禁止 GENERIC_FAILURE */
    TZrExecIrBlockId blockId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId valueId;
    TZrExecIrSourceId sourceId;           /* 可回溯到 SZrFileRange */
} SZrExecIrDiagnostic;
```

与 parser 既有 `SZrStructuredDiagnostic`（diagnostic_builder.h:44）的关系：verifier 产出上面的紧凑结构；面向用户的编译错误由 builder 层把它包装成 `SZrStructuredDiagnostic`（填 severity/location/message/code），两层不合并——verifier 在 core，不能依赖 parser 的 SZrString。

## A.7 Oracle 直接解释器骨架（01.05）

oracle 的唯一目标是差分正确性，不追求速度。实现为逐指令 switch + 值环境数组：

```c
typedef struct SZrExecIrOracleFrame {
    const SZrExecIrFunction *function;
    SZrTypeValue *valueEnvironment;     /* valueCount 个，直接用现有 SZrTypeValue */
    TZrExecIrBlockId currentBlock;
    TZrExecIrBlockId previousBlock;     /* phi 求值需要：选第 k 个 incoming */
    TZrUInt32 instructionCursor;
} SZrExecIrOracleFrame;

/* 块进入序：先原子求值本块全部 phi（two-phase：全部读旧值，再全部写），
   防止 phi 相互依赖时的顺序错误——这是 lost-copy problem 的解释器侧对应。 */
```

事件记录：oracle 每执行一条带外部可见效果的指令（STORE 到 heap/global、CALL、THROW、DROP、BARRIER、SUSPEND）追加一条 `{eventKind, instructionId, operandSnapshot}` 到事件带；差分支架（00.03）逐事件比较 oracle vs ExecBC vs AOT，而非只比返回值。事件带即 00.03 差分 fixture 的数据格式。
