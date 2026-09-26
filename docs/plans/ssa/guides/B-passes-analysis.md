---
related_code:
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/architecture-design.md
doc_type: implementation-guide
status: planned
---

# 实现指南 B：Pass 管理、标量优化、GVN/Range 与逃逸分析（02.01–02.03）

> 待实现草案。每个 pass 的通用契约：输入必过 Verify(STRUCTURE|SSA)，输出必过 Verify(STRUCTURE|SSA|EFFECT)；证明不足时保留原指令并 emit 一条 missed/blocked remark，绝不静默放弃。

## B.1 Pass manager 与分析缓存（02.01）

```c
typedef struct SZrExecIrAnalysisCache {
    void *analyses[ZR_EXEC_IR_ANALYSIS_COUNT];   /* NULL = 未计算/已失效 */
    TZrUInt32 validMask;
} SZrExecIrAnalysisCache;

typedef struct SZrExecIrPassContext {
    SZrExecIrAnalysisCache *cache;
    SZrExecIrRemarkSink *remarks;        /* 11.02 消费；EmitRemark 走这里 */
    const SZrExecIrPassBudget *budget;   /* 02.05 专门化预算；标量 pass 忽略 */
    SZrExecIrArena *scratchArena;        /* pass 结束整体重置，禁止残留指针进 module */
} SZrExecIrPassContext;

TZrBool ZrParser_ExecIr_RunPassPipeline(SZrExecIrFunction *function,
        const SZrExecIrPassInfo *passes, TZrUInt32 passCount,
        SZrExecIrPassContext *context, SZrExecIrDiagnostic *diagnostic) {
    for (TZrUInt32 i = 0; i < passCount; ++i) {
        if (!ZrParser_ExecIr_Verify(function, ZR_EXEC_IR_VERIFY_STRUCTURE_SSA, diagnostic))
            return zr_pass_fail_with_pass_name(diagnostic, passes[i].name, ZR_FALSE);
        TZrBool modified = ZR_FALSE;
        if (!passes[i].run(function, context, &modified, diagnostic))
            return zr_pass_fail_with_pass_name(diagnostic, passes[i].name, ZR_FALSE);
        if (modified) {
            context->cache->validMask &= passes[i].preservedAnalysisMask;
            /* 被失效的分析指针不清空数据（在 scratch arena），只清 mask */
        }
        if (!ZrParser_ExecIr_Verify(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic))
            return zr_pass_fail_with_pass_name(diagnostic, passes[i].name, ZR_FALSE);
        zr_exec_ir_arena_reset(context->scratchArena);
    }
    return ZR_TRUE;
}
```

关键决策：**分析结果全部放 scratch arena 并在每个 pass 后重置**——因此"保留分析"意味着该 pass 承诺不使 CFG/def-use 变化影响它，manager 在重置前把保留的分析深拷到下一轮 scratch。这比指针共享更贵但杜绝悬垂；分析本身都是 O(n) 数组，拷贝成本可忽略。

## B.2 SCCP（02.01）

标准 Wegman–Zadeck 双 worklist，lattice 三层 TOP→CONST→BOTTOM：

```c
typedef struct SZrSccpLatticeValue {
    EZrSccpLatticeKind kind;     /* TOP / CONSTANT / BOTTOM */
    SZrTypeValue constant;       /* kind==CONSTANT 时有效，复用现有值表示 */
} SZrSccpLatticeValue;

/* 求值一条指令的传递函数。ZR 特有约束集中在这里： */
static SZrSccpLatticeValue zr_sccp_evaluate(const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction, const SZrSccpLatticeValue *values,
        const SZrExecIrRangeFacts *rangeFacts) {
    switch (instruction->opcode) {
    case ZR_EXEC_IR_OP_ARITHMETIC: {
        /* checked 算术折叠三态：
           1) 两操作数 CONST 且结果不溢出 → CONST(结果)
           2) 两操作数 CONST 且必溢出 → 不折叠为 UB，折叠为"必抛"事实：
              指令保留，给 02.02 的不可达后继剪枝用，remark 记 will-throw
           3) 其余 → BOTTOM。绝不在没有 range 证明时把 checked op
              替换成 unchecked。 */
    }
    case ZR_EXEC_IR_OP_LOAD:
        /* 仅当 memoryIn token 的定义是同地址 STORE 且中间无该 class 的
           其他 def（SSA token 保证）时前推常量；否则 BOTTOM。 */
    default:
        /* .def 表标注了 foldable 的 opcode 才尝试；其余直接 BOTTOM */
    }
}
```

替换阶段：CONST 值重写为 `CONSTANT` 指令 + 全部 use 重定向；被证明不可达的 CFG 边不在 SCCP 内删除——只标记，删除权在 CFG 清理 pass（保持每个 pass 单一职责，verifier 可逐 pass 定位）。

## B.3 GVN 与 memory token 约束（02.02）

基于支配树 DFS 的 dominator-scoped hash GVN（非全局迭代版，实现简单且不需要到不动点）：

```c
typedef struct SZrGvnKey {
    TZrUInt16 opcode;
    TZrExecIrTypeToken typeToken;
    TZrExecIrValueId canonicalOperands[ZR_GVN_MAX_KEY_OPERANDS]; /* 已归一化：可交换 op 排序 */
    TZrExecIrMemoryTokenId memoryVersion;   /* ★ LOAD 的 key 必含它读的 token 版本 */
} SZrGvnKey;

/* 支配树 DFS：进入块时开新 scope（哈希表叠层），离开时弹出。
   命中 → use 重定向到先前值 + 删除本指令（终结指令与带 flags 副作用者
   永不进表）。LOAD 的 memoryVersion 字段保证：
       x = load p   (memory v1)
       store q, ... (memory v1 -> v2)
       y = load p   (memory v2)   ← key 不同，不会错误合并
   除非 02.02 的别名分析证明 q 与 p 不别名并把 store 的 token 类隔离。 */
```

别名分析分层（保守起步，每层有独立测试）：

```text
层 0  类型隔离：不同 memory class 永不别名（§4 八类）。
层 1  SSA 值来源：两个 ALLOC 的结果必不别名；ALLOC 结果与函数入参必不别名
      （逃逸前）。
层 2  SemIR loan facts：借用检查已证明 &mut 独占 —— 活跃 SZrSemanticIrLoanFact
      的独占借用区间内，该 place 与其它任何 place 不别名。这是 ZR 相比
      C/LLVM 的信息优势，lowering 时把 loan 区间转成 ExecIR 指令区间标注。
层 3  字段敏感：同对象不同 MemberId 的字段访问不别名（shape 固定后 offset 不同）。
```

Range 分析用标准区间格 `[lo, hi]`（i64 饱和），在支配树上单向传播 branch 条件收窄（`if (i < n)` 的 true 边内 `i ∈ [lo, min(hi, n-1)]`）；bounds check 消除的谓词是 `provenRange(index) ⊆ [0, provenLength)` 且 length 在该 effect region 内无 def——两个条件各自失败时 remark 分别记 `bounds-index-unproven` 与 `bounds-length-mutable`，供 11.02 解释"为什么无法消除 bounds check"。

## B.4 逃逸分析与分配决策（02.03）

在 SemIR 已有 `EZrSemanticEscapeState`（region 事实）基础上做 ExecIR 级精化，输出三态分配决策：

```c
typedef enum EZrExecIrAllocationDecision {
    ZR_EXEC_IR_ALLOC_HEAP = 0,          /* 默认；不可证明时的保守解 */
    ZR_EXEC_IR_ALLOC_STACK,             /* 不逃逸 + 大小编译期已知 + 生命周期 ⊆ 帧 */
    ZR_EXEC_IR_ALLOC_REGION             /* 逃逸界于 region（06.01 TLAB region）*/
} EZrExecIrAllocationDecision;
```

逃逸传播是 def-use 图上的固定点：worklist 从每个 ALLOC 出发，遇到 `STORE 到已逃逸位置 / 作为 CALL 实参且被调摘要未证明不捕获 / RETURN / 跨 SUSPEND 活跃 / 传入 native` 即升格逃逸。**跨 SUSPEND 活跃即逃逸**是 ZR 特有规则（协程帧可被移动/复制，栈上聚合体地址不稳定），必须有专门负测：安排一个仅因跨 await 而逃逸的对象，断言决策为 HEAP 且 remark reason 为 `escape-across-suspend`。

栈分配落地后强制维持三件事（每件一个断言草案）：
1. GcMap 补录栈上对象的内部引用槽（它现在是 root 的一部分）；
2. DROP 语义不变——栈分配对象的 drop 仍按 ownership 顺序执行，只是免了 free；
3. deopt 重建时栈对象需 rematerialize 到堆（记入 DeoptState reconstruction），否则该 ALLOC 不许栈化。

## B.5 Remark 数据流（贯穿 02.x，11.02 消费）

```c
typedef struct SZrOptimizationRemark {
    const char *pass;                    /* registry 名，静态存储 */
    TZrExecIrSourceId sourceId;
    EZrRemarkOutcome outcome;            /* SUCCESS / MISSED / BLOCKED */
    TZrUInt32 reasonCode;                /* 每 pass 的枚举，如 bounds-length-mutable */
    TZrUInt32 beforeRepresentation;      /* 表示变换前后 id（layout/指令 kind），可 0 */
    TZrUInt32 afterRepresentation;
    TZrUInt64 profileCount;              /* 有 profile 时填充 */
} SZrOptimizationRemark;
```

sink 是 append-only 数组（module 级），pass 内不去重；`zr explain optimize` 查询时按 sourceId 聚合。原则：**pass 做了什么/为什么没做，写 remark；pass 出错，写 diagnostic**——两条通道不混。
