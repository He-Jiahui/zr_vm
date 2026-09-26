---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/memory.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 为 SSA 计划补充代码实现架构指导、关键数据结构与关键函数设计"
doc_type: architecture-guide
status: planned
---

# SSA 计划实现架构指导：关键数据结构与函数设计

> 本文是 [总索引](index.md) 下全部 47 份子计划共享的实现指导。所有 `SZr*/EZr*/TZr*/FZr*` 类型与 `Zr<Module>_*` 函数均为**待实现设计草案**，不是当前仓库 API；命名遵循 [zr_code_naming_conventions.md](../../zr_code_naming_conventions.md)。每个结构/函数由括号内标注的子计划负责实现，实现时以对应叶子计划的任务与测试为准；本文与叶子冲突时以叶子为准并回改本文。
>
> 更细粒度的算法骨架、完整函数实现草案与常见坑清单见分册：
> - [指南 A：ExecIR 模型、Builder、SSA 构造与 Verifier](guides/A-execir-builder-verifier.md)（01.01–01.03、01.05 oracle）
> - [指南 B：Pass 管理、SCCP、GVN/Range 与逃逸分析](guides/B-passes-analysis.md)（02.01–02.03、remark 数据流）
> - [指南 C：解释器边界、Guard 缓存与 Frame/调用运行时](guides/C-interpreter-frame-runtime.md)（03.01–03.03、04.01–04.02）
> - [指南 D：GC 状态机、Domain 边界、Artifact 与热更发布](guides/D-gc-domain-artifact-hotpatch.md)（06.01–06.04、08.01–08.03）
> - [指南 E：ExecBC 投影、Superinstruction 与 AOT 同源 lowering](guides/E-projections-fusion-aot.md)（01.05、03.04、07.01–07.04）

## 1. 总体架构与模块分层

### 1.1 分层原则（对应 01.01、07.01、08.01）

```text
zr_vm_common      纯 ABI/配置常量：opcode schema 常量、AOT ABI、数值 profile 标识
      ▲
zr_vm_core        ExecIR 只读模型 + 生命周期 + 运行期验证/消费
      ▲               exec_ir.h / exec_ir.c / execbc_verify.c / artifact_exec_ir.c
zr_vm_parser      builder、SSA 构造、全部优化 pass、ExecBC/AOTIR lowering
      ▲               exec_ir_builder.h / exec_ir/ 子目录 / passes/ / analysis/
zr_vm_aot         C/LLVM emitter，只消费 AOTIR，不 include parser 私有分析头
```

硬边界（verifier 与 CMake 双重保证）：

1. core **绝不** include parser/aot 头；`zr_vm_core/include/zr_vm_core/exec_ir.h` 可被只链接 core 的读取程序独立编译（01.01 退出门禁）。
2. parser 的可变 builder API 放内部头（`exec_ir_builder_internal.h`），公开头只暴露只读查询；后端不能随意 mutate 共享 IR。
3. 诊断类型分层：`SZrExecIrDiagnostic` 由 01.01 定义在 core 的 exec_ir.h，凡以 ExecIR 为操作对象的 API（`ZrCore_ExecIr_*`、`ZrParser_ExecIr_*`、`ZrCore_ExecutionBackend_*`）均可使用；与 ExecIR 无关的运行期域用各自类型（`SZrExecutionDiagnostic`、`SZrDomainDiagnostic`、`SZrHotPatchDiagnostic`、`SZrNativeCallDiagnostic`、`SZrViewDiagnostic`），并复用既有 `SZrArtifactDiagnostic`（artifact_schema.h:288）与 `SZrCallBindingDiagnostic`（call_binding.h）。禁止 domain/GC/native/view 等运行期 API 借用 ExecIR 诊断类型。
4. 持久化字段只允许 token/index/offset/hash/version/generation/relocation record；任何含裸指针的结构（如 `SZrCallBindingTarget` 的 union）永不序列化——沿用 call_binding.h 中 “Never serialize SZrCallBinding” 的现行做法。

### 1.2 数据流总图

```text
AST ──parser──► SemIR facts (semantic_ir.h, 现有)
                    │  canonical lowering (01.02)
                    ▼
             ExecIR (typed SSA)  ──verify(01.03)──►  每个 pass 前后
                    │
        ┌───────────┼──────────────────┐
        ▼           ▼                  ▼
  直接解释 Oracle  ExecBC 投影      AOTIR 投影 (07.01)
   (01.05)        (01.05/03.04)        │
        │           │             ┌────┴────┐
        ▼           ▼             ▼         ▼
     差分测试     解释器执行    C emitter  LLVM emitter (07.02)
                    │                │
                    └──── .zro/.zrm artifact (08.01) ◄── 热更 patch (08.2-4)
```

关键不变量：ExecIR 是唯一语义源；ExecBC/AOTIR 是投影，**任何后端不得从 quickened bytecode 反推语义**（index 实施边界 2）。差分测试支架（00.03）在四个执行面（oracle/ExecBC/C/LLVM）上比较逐事件序列。

## 2. ExecIR 核心数据模型（01.01）

### 2.1 ID 与容器约定

沿用 semantic_ir.h 的 `TZrUInt32` ID + INVALID sentinel 惯例：

```c
/* zr_vm_core/include/zr_vm_core/exec_ir.h —— 待实现草案 */
typedef TZrUInt32 TZrExecIrValueId;        /* 函数内唯一，0 = INVALID */
typedef TZrUInt32 TZrExecIrBlockId;        /* 0 = INVALID，1 = ENTRY（显式区分） */
typedef TZrUInt32 TZrExecIrInstructionId;
typedef TZrUInt32 TZrExecIrMemoryTokenId;  /* memory token 与普通值分离编号 */
typedef TZrUInt32 TZrExecIrEffectTokenId;
typedef TZrUInt32 TZrExecIrTypeToken;      /* 模块级稳定 token，复用 TZrMetadataToken 空间 */
typedef TZrUInt32 TZrExecIrSourceId;       /* source map side array 索引 */
typedef TZrUInt32 TZrExecIrDeoptId;

#define ZR_EXEC_IR_VALUE_ID_INVALID   ((TZrExecIrValueId)0u)
#define ZR_EXEC_IR_BLOCK_ID_INVALID   ((TZrExecIrBlockId)0u)
#define ZR_EXEC_IR_BLOCK_ID_ENTRY     ((TZrExecIrBlockId)1u)

/* 通用 range：所有变长 operand/phi/successor 数据进侧数组，指令本体定长 */
typedef struct SZrExecIrRange {
    TZrUInt32 offset;   /* 进入所属 side array 的起始下标 */
    TZrUInt32 count;
} SZrExecIrRange;
```

容量增长一律走 `checkedMul(count, elementSize)` + `checkedAdd` 的溢出检查 helper，失败先报 `ZR_EXEC_IR_STATUS_CAPACITY_OVERFLOW` 再分配（01.01 断言草案）。分配统一使用 `ZrCore_Memory_Allocate` 家族并挂在 module arena 上，保证 `FreeModule` 一次性回收、失败 clone 不修改 source。

### 2.2 指令与函数（SoA 布局）

指令定长 + side array 的 SoA 布局是本设计的核心决策，理由：(a) pass 遍历时 cache 友好；(b) 克隆/序列化按数组批量处理；(c) 与 08.01 磁盘 codec 一一对应。

```c
typedef struct SZrExecIrInstruction {
    TZrUInt16 opcode;                    /* EZrExecIrOpcode，由 exec_ir_opcode.def 生成 */
    TZrUInt16 flags;                     /* EZrExecIrInstructionFlags 位集 */
    SZrExecIrRange results;              /* -> valuePool */
    SZrExecIrRange operands;             /* -> operandPool (TZrExecIrValueId) */
    TZrExecIrTypeToken typeToken;
    TZrUInt32 layoutId;                  /* 布局 id/hash 表索引，无布局 = INVALID */
    SZrExecIrRange memoryIn;             /* -> memoryTokenPool */
    SZrExecIrRange memoryOut;
    TZrExecIrEffectTokenId effectIn;     /* effect 线性链，单入单出 */
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrSourceId sourceId;
    TZrExecIrDeoptId deoptId;            /* 无 deopt 点 = INVALID */
    TZrUInt32 bindingRow;                /* CallBinding row index，非指针（03.02 生产） */
} SZrExecIrInstruction;

typedef enum EZrExecIrInstructionFlags {
    ZR_EXEC_IR_FLAG_MAY_ALLOCATE = 1u << 0,
    ZR_EXEC_IR_FLAG_MAY_THROW    = 1u << 1,
    ZR_EXEC_IR_FLAG_MAY_GC       = 1u << 2,
    ZR_EXEC_IR_FLAG_MAY_SUSPEND  = 1u << 3
} EZrExecIrInstructionFlags;

typedef struct SZrExecIrValue {
    TZrExecIrInstructionId definition;   /* def-use 的 def 侧；参数/phi 也有定义指令 */
    TZrExecIrTypeToken typeToken;
    TZrUInt8 ownership;                  /* EZrOwnershipValueKind 投影 */
    TZrUInt8 nullability;                /* EZrExecIrNullability */
    TZrUInt16 reserved;
} SZrExecIrValue;

typedef struct SZrExecIrBlock {
    SZrExecIrRange instructions;         /* -> instruction 数组（区间连续，terminator 最后） */
    SZrExecIrRange predecessors;         /* -> blockRefPool */
    SZrExecIrRange successors;
    SZrExecIrRange phis;                 /* -> phiPool；phi incoming 按 predecessors 顺序对齐 */
    TZrExecIrBlockId immediateDominator; /* 由 02 号分析回填，构建期 INVALID */
} SZrExecIrBlock;

typedef struct SZrExecIrFunction {
    TZrMetadataToken functionToken;
    SZrExecIrInstruction *instructions;  TZrUInt32 instructionCount, instructionCapacity;
    SZrExecIrValue *values;              TZrUInt32 valueCount, valueCapacity;
    SZrExecIrBlock *blocks;              TZrUInt32 blockCount, blockCapacity;
    TZrExecIrValueId *operandPool;       TZrUInt32 operandCount, operandCapacity;
    /* … memoryTokenPool / phiPool / blockRefPool / sourcePool / deoptPool 同构 … */
    struct SZrExecIrFrameLayout *frameLayout;   /* 01.04/04.01 填充 */
    struct SZrExecIrGcMap *gcMap;               /* 01.04 填充 */
    TZrBool sealed;                      /* 发布后只读；builder 断言未 sealed 才可写 */
} SZrExecIrFunction;
```

phi 表示：`phiPool` 中每个 phi 为 `{result: TZrExecIrValueId, incomings: SZrExecIrRange}`，incoming 顺序**必须**与 block `predecessors` 顺序一致——verifier 逐块检查该对齐，克隆重编号时同步重排。

### 2.3 opcode schema 单源生成（01.01 任务 2）

`exec_ir_opcode.def` 采用 X-macro，enum、arity 表、effect 表、verifier 检查表同源：

```c
/* zr_vm_core/include/zr_vm_core/exec_ir_opcode.def —— 待实现草案 */
/* ZR_EXEC_IR_OP(name, resultArity, operandArity, memReads, memWrites, flags) */
ZR_EXEC_IR_OP(CONSTANT,        1, 0, 0, 0, 0)
ZR_EXEC_IR_OP(ARITHMETIC,      1, 2, 0, 0, ZR_EXEC_IR_FLAG_MAY_THROW)   /* checked ops */
ZR_EXEC_IR_OP(PLACE_PROJECT,   1, 1, 0, 0, 0)
ZR_EXEC_IR_OP(LOAD,            1, 1, 1, 0, 0)
ZR_EXEC_IR_OP(STORE,           0, 2, 0, 1, 0)
ZR_EXEC_IR_OP(CALL,            1, ZR_EXEC_IR_VARIADIC, 1, 1,
              ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW |
              ZR_EXEC_IR_FLAG_MAY_GC | ZR_EXEC_IR_FLAG_MAY_SUSPEND)
ZR_EXEC_IR_OP(ALLOC,           1, 1, 0, 1, ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_GC)
ZR_EXEC_IR_OP(BARRIER,         0, 2, 1, 1, 0)
ZR_EXEC_IR_OP(DROP,            0, 1, 1, 1, ZR_EXEC_IR_FLAG_MAY_THROW)
ZR_EXEC_IR_OP(BRANCH,          0, 1, 0, 0, 0)
ZR_EXEC_IR_OP(INVOKE,          1, ZR_EXEC_IR_VARIADIC, 1, 1, /* call + 异常边 */ …)
ZR_EXEC_IR_OP(THROW,           0, 1, 1, 1, ZR_EXEC_IR_FLAG_MAY_THROW)
ZR_EXEC_IR_OP(SUSPEND,         1, 1, 1, 1, ZR_EXEC_IR_FLAG_MAY_SUSPEND)
/* … 与 SemIR ZR_SEMANTIC_IR_* 的映射表见 01.02 lowering …*/
```

操作集与 semantic_ir.h 现有 `ZR_SEMANTIC_IR_*`（CONSTANT/CONVERT/PLACE_*/LOAD/STORE/MOVE/COPY/DROP/BORROW_*/CALL_*/BRANCH/SWITCH/RETURN/THROW/CLEANUP…）保持一对一或一对多 lowering 关系，不发明第三套语义。

## 3. SSA 构造与支配树（01.02）

### 3.1 算法选择

- **CFG 构建**：从 SemIR 线性事实按 leader 划块；异常边（INVOKE→handler）与 cleanup 边显式建边，不隐藏在指令语义里。
- **支配树**：Cooper–Harvey–Kennedy 迭代算法（简单、对 <10k 块的函数足够快）；`immediateDominator` 回填进 block。支配边界（DF）用于 phi 放置。
- **SSA 构造**：经典 Cytron 风格——对每个 SemIR place 的 def 集合求迭代支配边界放 phi，再 DFS 重命名。**不用** Braun 按需构造，理由：输入是完整 SemIR 事实而非增量 AST，且 verifier 要求构造完成即验证。
- **memory/effect token 也走 SSA**：每类 memory token（见 §4）与 effect 链同样放 phi（token phi），保证重排合法性可局部判定。

### 3.2 关键函数

```c
/* zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h —— 待实现草案 */
TZrBool ZrParser_ExecIr_Build(const SZrSemanticIrFunction *semanticFunction,
    const SZrExecIrBuildOptions *options, SZrExecIrFunction *out,
    SZrExecIrDiagnostic *diagnostic);
/* 步骤：块划分 → 建边（含异常/cleanup/suspend 边）→ 支配树 → phi 放置 →
   重命名 → token 线程化 → seal → 内部调用 Verify。
   失败行为：任一步骤失败即返回，out 处于可安全 Free 的部分初始化态，
   diagnostic 携带 SemIR instruction id + source range。 */

TZrBool ZrParser_ExecIr_ComputeDominators(SZrExecIrFunction *function,
    SZrExecIrDiagnostic *diagnostic);
/* 幂等；被 pass manager 作为 analysis 缓存（§6），CFG 变更后失效重算。 */
```

## 4. Memory/Effect 模型与 Verifier（01.03）

### 4.1 效果类别

按 index 副作用拆分固定 8 类，每类一条独立 token 链：

```c
typedef enum EZrExecIrMemoryClass {
    ZR_EXEC_IR_MEMORY_STACK_FRAME = 0,
    ZR_EXEC_IR_MEMORY_MANAGED_HEAP,
    ZR_EXEC_IR_MEMORY_MODULE_GLOBAL,
    ZR_EXEC_IR_MEMORY_NATIVE_FFI,
    ZR_EXEC_IR_MEMORY_GC,
    ZR_EXEC_IR_MEMORY_OWNERSHIP,
    ZR_EXEC_IR_MEMORY_SCHEDULER_TASK,
    ZR_EXEC_IR_MEMORY_IO,
    ZR_EXEC_IR_MEMORY_CLASS_COUNT
} EZrExecIrMemoryClass;
```

重排规则（GVN/LICM/调度共同引用，唯一实现在 verifier 的 helper 中）：两条指令可交换 ⇔ 它们的 memoryIn/memoryOut 在所有类上无 def-use/def-def 冲突，且 effect 链允许（`STORE`、`BARRIER`、`DROP`、`THROW`、`SUSPEND`/`AWAIT`、native `CALL` 永不跨越各自类的 token def）。

### 4.2 Verifier 设计

```c
TZrBool ZrParser_ExecIr_Verify(const SZrExecIrFunction *function,
    EZrExecIrVerifyLevel level, SZrExecIrDiagnostic *diagnostic);
/* level: STRUCTURE（ID/range/phi 对齐/终结指令）
        | SSA（单赋值、def 支配 use、token 线性）
        | EFFECT（token 类完整、可重排断言、mayThrow 边覆盖）
   全部 pass 前后强制 STRUCTURE|SSA，优化 pass 后加 EFFECT。
   失败：diagnostic = {errorKind, blockId, instructionId, valueId, sourceId}，
   错误种类必须精确（如 ZR_EXEC_IR_VERIFY_PHI_PREDECESSOR_MISMATCH），
   不能只报"验证失败"——malformed IR 负测逐 kind 断言。 */
```

## 5. 状态映射（01.04）：GcMap / DeoptState / SuspendMap

```c
typedef struct SZrExecIrGcMap {
    /* 每个 safepoint（MAY_GC 指令）一条记录：活跃 ref slot 位图 + inline span 内部引用偏移 */
    struct SZrExecIrGcMapEntry { TZrExecIrInstructionId site;
        SZrExecIrRange liveRefSlots;       /* -> slotIndexPool（frame slot index，非地址） */
        SZrExecIrRange inlineRefOffsets; } *entries;
    TZrUInt32 entryCount;
} SZrExecIrGcMap;

typedef struct SZrExecIrDeoptState {
    TZrExecIrDeoptId id;
    TZrExecIrSourceId source;
    SZrExecIrRange reconstruction;   /* {logicalSlot, valueId} 对：从优化态重建解释器帧 */
} SZrExecIrDeoptState;
```

约束：map 记录 **slot index/offset**，永不记录地址；04.04 的运行期 root 枚举以此为输入并要求"生成 map 与实际 frame location 一致性由后端差分测试验证"。suspend/resume map 同构，key 为 `SUSPEND` 指令 id。

## 6. Pass 管理与标量优化（02.01）

### 6.1 Pass registry 与分析缓存

```c
typedef enum EZrExecIrAnalysisKind {
    ZR_EXEC_IR_ANALYSIS_DOMINATORS = 0,
    ZR_EXEC_IR_ANALYSIS_LOOPS,
    ZR_EXEC_IR_ANALYSIS_ALIAS,
    ZR_EXEC_IR_ANALYSIS_RANGE,
    ZR_EXEC_IR_ANALYSIS_ESCAPE,
    ZR_EXEC_IR_ANALYSIS_COUNT
} EZrExecIrAnalysisKind;

typedef struct SZrExecIrPassInfo {
    const char *name;                          /* remark 与 explain 用 */
    TZrUInt32 preservedAnalysisMask;           /* 位集：本 pass 保留哪些分析 */
    FZrExecIrPassRun run;                      /* TZrBool (*)(SZrExecIrFunction*, SZrExecIrPassContext*, SZrExecIrDiagnostic*) */
} SZrExecIrPassInfo;
```

失效协议：pass 返回"已修改"时，manager 清除 `~preservedAnalysisMask` 的缓存分析；分析按需惰性重算。每个 pass 运行前后由 manager 统一调 Verify（§4.2），pass 自身不许跳过。

优化事件（remark）在 pass 内通过 `ZrParser_ExecIr_EmitRemark(context, pass, sourceId, kind, reasonCode, …)` 记录，11.02 消费；"证明不足保留原操作"必须落一条 `missed/blocked` remark 而非静默。

### 6.2 SCCP / 复制传播 / DCE 要点

- SCCP：标准 lattice（TOP/CONST/BOTTOM）+ 双 worklist（SSA edge / CFG edge）；checked 算术只有当 range 分析证明不溢出才折叠为无检查常量，否则保留原指令并 remark。
- 复制传播只在同 memory token 版本内替换；跨 token def 的 load 不折叠。
- DCE 以 flags 保守：带 MAY_THROW/MAY_GC 的指令不因结果无用而删除，除非 02.02 证明不抛。

## 7. 静态绑定与 Guard 协议（03.02、03.03）

复用现有 call_binding.h 全套类型（`SZrCallBindingContract`/`SZrCallBindingTarget`/`EZrCallBindingStatus`），ExecIR 指令只存 `bindingRow`。运行期 guard 协议统一为：

```c
typedef enum EZrBindingGuardResult {
    ZR_BINDING_GUARD_HIT = 0,            /* fast path */
    ZR_BINDING_GUARD_POLY_MISS,          /* 合法多态 miss → 回同 token/slot 基线操作 */
    ZR_BINDING_GUARD_STALE               /* generation/contract/hash 不符 → 结构化 link error */
} EZrBindingGuardResult;

ZR_FORCE_INLINE EZrBindingGuardResult zr_execution_check_binding_guard(
    const SZrCallBinding *binding, TZrUInt64 frameActiveGeneration,
    TZrUInt64 receiverShapeGeneration);
/* 判序：先 generation（旧 frame 用其 version record 的 generation，非全局最新，
   见 03.03/08.03 lease 协议），再 shape/layout，再 signature hash。
   STALE 路径构造 SZrCallBindingDiagnostic{ZR_CALL_BINDING_STALE_GENERATION,…}，
   任何分支都不进行成员名字符串查找（断言 nameLookupCount == 0）。 */
```

PIC：每站点固定 4 entry 内联缓存 + 溢出转 megamorphic slot dispatch；每专门化版本记录 `{codeSize, hitCount, invalidateCount}`，超上限（03.04 预算）拒绝再生成并 remark。

## 8. Frame 布局与调用传递（04.01、04.02）

```c
typedef enum EZrExecIrSlotClass {
    ZR_EXEC_IR_SLOT_BOXED = 0,        /* 完整 SZrTypeValue（value.h:37 现有布局） */
    ZR_EXEC_IR_SLOT_SCALAR,           /* 未装箱 int/uint/float/bool payload */
    ZR_EXEC_IR_SLOT_INLINE_SPAN,      /* inline struct byte span，带内部 ref offset 表 */
    ZR_EXEC_IR_SLOT_REF               /* GC 引用，root map 追踪 */
} EZrExecIrSlotClass;

typedef struct SZrExecIrFrameLayout {
    TZrUInt32 storageSlotCount;       /* 物理槽位（与 logicalSlotCount 分别命名，04.01） */
    TZrUInt32 logicalSlotCount;
    TZrUInt32 parameterPrefixCount;   /* 参数占用前缀，caller 直写 */
    TZrUInt32 returnBufferOffset;
    struct SZrExecIrSlotDescriptor { TZrUInt8 slotClass; TZrUInt8 flags;
        TZrUInt16 alignment; TZrUInt32 byteOffset; TZrExecIrTypeToken type; } *slots;
    TZrUInt64 layoutHash;             /* 进 artifact 与 guard */
} SZrExecIrFrameLayout;
```

调用传递计划（04.02）在编译期由 signature+layout+ownership 分类为 `SCALAR_COPY | SPAN_COPY | MOVE | BORROW | BOXED_BRIDGE` 五类之一，运行期 `execution_call_transfer.c` 按类执行，不做运行期猜测。尾调用 eligibility 谓词：`noPendingCleanup && noEscapingFrameAlias && compatibleContinuation && debugPolicyAllows`，重叠参数用逐槽拓扑排序搬移（成环时借 return buffer 中转一格）。

## 9. GC 关键结构（06.01、06.02）

```c
typedef struct SZrGcTlab {              /* 每 worker 一个，region bump 分配 */
    TZrByte *cursor; TZrByte *limit;    /* 仅两字段热路径；refill 走慢路 */
    struct SZrGcRegion *region;
} SZrGcTlab;

typedef enum EZrGcPhase {               /* 可预算状态机；对接 gc.h 既有 pauseBudgetUs/remarkBudgetUs */
    ZR_GC_PHASE_IDLE = 0, ZR_GC_PHASE_MINOR_MARK, ZR_GC_PHASE_MINOR_EVACUATE,
    ZR_GC_PHASE_MAJOR_CONCURRENT_MARK, ZR_GC_PHASE_REMARK,
    ZR_GC_PHASE_SWEEP, ZR_GC_PHASE_COMPACT
} EZrGcPhase;
```

预算切片规则：每帧按 `{microseconds, objectCount, bytes}` 三预算取最先耗尽者暂停，**但 MINOR_EVACUATE 的单对象搬迁与引用修复是不可中断段**——无 read barrier 协议时严禁半完成恢复 mutator（index 边界 5）；超预算返回 backpressure 报告而非缩短不可中断段。card table 512B/卡，old→young 写屏障只在 `BARRIER` 指令类落地，由 ExecIR flags 静态省略已证明不需要的屏障（02.02 range/escape 证明）。

## 10. Artifact 编解码（08.01）

section codec 统一逐字段读写（沿用 writer_call_binding.c 的 84→96 字节逐字段投影惯例，绝不 memcpy 结构体）：

```c
typedef struct SZrArtifactSectionCodec {
    TZrUInt32 sectionKind;              /* 新 section 进 artifact_schema.h registry */
    TZrUInt32 version;
    FZrArtifactSectionWrite write;      /* TZrBool (*)(const void*, SZrExecIrWriter*, SZrArtifactDiagnostic*) */
    FZrArtifactSectionRead  read;       /* 读取先验 magic/version/rowSize/bounds，再逐字段解码 */
    FZrArtifactSectionValidate validate;/* 加载后语义验证（execbc_verify 挂这里） */
} SZrArtifactSectionCodec;
```

无裸地址三层防护：(1) codec 字段白名单——每个 section 的字段表在编译期声明，未声明字段进不了磁盘；(2) 读侧 `SZrArtifactDiagnostic` 逐字段界限检查；(3) 测试层 ASLR 双进程写入比对 + 字节扫描兜底（08.01 断言草案）。版本常量：实施时将 `ZR_ARTIFACT_SCHEMA_VERSION`（现 5）与 `ZR_VM_AOT_ABI_VERSION`（现 16）各 +1，旧版本读取返回 `RECOMPILE_REQUIRED`。

## 11. 关键函数一览（按实施批次排序）

| 函数（草案） | 所属 | 一句话契约 |
| --- | --- | --- |
| `ZrParser_ExecIr_Build` | 01.02 | SemIR→SSA ExecIR；失败部分初始化态可安全 Free |
| `ZrParser_ExecIr_Verify` | 01.03 | 三级验证；错误种类+位置精确 |
| `ZrCore_ExecIr_CloneModule / FreeModule` | 01.01 | 深复制重映射局部 ID；失败不改 source |
| `ZrParser_ExecIr_Interpret`（oracle） | 01.05 | 直接解释 ExecIR，逐事件记录供差分 |
| `ZrParser_ExecIr_LowerExecBc / LowerAot` | 01.05/07.01 | 无优化投影；语义等价由 00.03 差分门禁保证 |
| `ZrParser_ExecIr_Optimize` | 02.01 | 按 registry 顺序跑 pass；每步前后 Verify |
| `ZrParser_ExecIr_ProjectBindingFacts` | 03.02 | 产出 binding rows；歧义/缺失→编译错误非回退 |
| `zr_execution_check_binding_guard` | 03.03 | §7 三态 guard；永无名字查找 |
| `ZrParser_ExecIr_LayoutFrame` | 04.01 | 由类型/生命周期生成 packed layout+hash |
| `ZrCore_Execution_PrepareTransfer` | 04.02 | 五类传递计划；运行期零猜测 |
| `ZrCore_Execution_VisitFrameRoots` | 04.04 | 按 GcMap 精确枚举；拒绝全堆扫描兜底 |
| `ZrCore_Gc_SetBudget / GetStats` | 06.02 | 对接既有 pauseBudgetUs 字段；预算≠硬实时 |
| `ZrCore_Domain_ShareValue / TransferValue` | 06.03/06.04 | Send/Sync 证明或结构化复制；无跨域裸指针 |
| `ZrParser_ExecIr_Read / Write` | 08.01 | §10 codec；token/index/hash-only |
| `ZrCore_HotPatch_Validate / Apply / Rollback` | 08.02–04 | 能力交集校验→generation 原子发布→新 epoch 回滚 |

## 12. 实现顺序建议（与批次表对齐的最小可行链）

1. **骨架先行**（00.02/00.03 + 01.01 批次 1）：opcode .def、ID/range/arena、core-only header probe、`ssa-tests.cmake`。此时即可跑 `ssa_core_model` 空模块测试。
2. **纵向打通一条最小链**（01.02→01.05）：只支持 scalar arithmetic + branch + return 的函数，从 SemIR 一路到 oracle 与 ExecBC 投影，差分测试先跑通**再**横向扩指令集——避免 47 份计划各自铺开后集成失败。
3. **每加一类指令，同批补齐**：lowering、verifier 规则、oracle 语义、ExecBC/AOTIR 投影、差分 fixture 五件套一次提交，保持四执行面始终一致。
4. **优化 pass 全部后置**：M1 门禁（四后端一致）绿之前不合入任何 02.x pass；每个 pass 首个提交必须带"证明不足→保留原操作+remark"的负测。

维护约定：本文各结构落地后，把"待实现草案"标记改为指向真实头文件的路径引用；若实现与本文分歧，以叶子计划评审结论为准并同步回改本文，不允许两处长期不一致。
