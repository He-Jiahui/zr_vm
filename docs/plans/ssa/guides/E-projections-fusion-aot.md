---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/architecture-design.md
doc_type: implementation-guide
status: planned
---

# 实现指南 E：ExecBC 投影、Superinstruction 生成与 AOT 同源 lowering（01.05、03.04、07.01–07.02）

> 待实现草案。既有事实：`SZrInstruction` 固定宽度 `{u16 operationCode, u16 operandExtra, TZrInstructionType operand}`（zr_instruction_conf.h:361）；zr_vm_aot 已有 `SZrAotExecIrInstruction/FrameLayout/BasicBlock/Function/Module` 私有记录（backend_aot_exec_ir.h）待迁移收敛。

## E.1 ExecIR → ExecBC 无优化投影（01.05）

投影是**寄存器分配 + 指令选择的最保守版本**，正确性优先：

```text
步骤 1  值落槽：每个 ExecIR value 直接映射一个 frame slot
        （无优化投影不做槽复用——复用是 04.01 布局着色的事，
        投影只消费 frameLayout 的结果映射表 valueId→slotIndex）。
步骤 2  phi 消解：SSA 出口标准问题。在每条进入含 phi 块的边上
        插入并行拷贝（parallel copy），再串行化：
        - 用 guide C §C.4 相同的拓扑排序 + 单格中转算法
          （同一个问题的编译期形态——lost copy / swap problem）
        - 关键边（critical edge：前驱多后继 + 后继多前驱）必须
          先 split 出空块再插拷贝，verifier STRUCTURE 级
          在投影前强制检查无关键边。
步骤 3  指令选择：一对一表驱动。ExecIR opcode + typeToken
        → ExecBC opcode（现有 EZrOpcode 空间），操作数编码进
        operand/operandExtra；超出 16 位的索引进 side table，
        指令存 side table 下标——固定宽度不破。
步骤 4  maps 重定位：GcMap/DeoptState/SourceMap 的
        instructionId 重映射为 ExecBC pc 偏移，行序保持。
```

差分义务：投影后立即用同一输入跑 oracle（guide A §A.7）与 ExecBC 解释器，逐事件比较——这在 01.05 就要建成 CI 常态，不等 07.02。

## E.2 Superinstruction 模式定义与生成器（03.04）

模式是数据不是代码——单一 `.def` 源生成匹配器、融合 opcode 枚举、解释器 handler 三者：

```c
/* zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_fusion_patterns.def —— 草案 */
/* ZR_EXECBC_FUSION(name, headOp, tailOp, constraint, resultForm)
   constraint 是生成器可检查的谓词标识，不是自由代码： */
ZR_EXECBC_FUSION(LOAD_ADD_INT,   LOAD_SLOT,  ADD_INT,
                 ZR_FUSE_SAME_RESULT_OPERAND | ZR_FUSE_NO_INTERVENING_EFFECT,
                 ZR_FUSE_RESULT_OF_TAIL)
ZR_EXECBC_FUSION(CMP_BRANCH_INT, CMP_LT_INT, BRANCH_IF,
                 ZR_FUSE_SAME_RESULT_OPERAND | ZR_FUSE_RESULT_SINGLE_USE,
                 ZR_FUSE_RESULT_NONE)
ZR_EXECBC_FUSION(INDEX_LOAD_F64, INDEX_ADDR, LOAD_F64,
                 ZR_FUSE_SAME_RESULT_OPERAND | ZR_FUSE_BOUNDS_ALREADY_CHECKED,
                 ZR_FUSE_RESULT_OF_TAIL)
/* … member lookup + call / increment + loop branch / call + return … */
```

生成器（构建期工具或编译期展开）产出三样：

```text
1. enum 扩展：融合 opcode 追加在现有 opcode 空间尾部（预算上限
   检查：总 opcode 数 ≤ dispatch table 容量，超限构建失败）。
2. 匹配器：ExecBC lowering 末尾的窗口扫描（窗口=2，不递归叠加
   ——融合指令不再作为下次融合的输入，防组合爆炸；03.04 尺寸
   预算的第一道闸）。约束检查全部机械化：
   SAME_RESULT_OPERAND = head.result 是 tail 的第 N 操作数；
   NO_INTERVENING_EFFECT = 两指令 effect token 直接相连；
   RESULT_SINGLE_USE = def-use 计数为 1（融合后中间值不落槽）。
3. handler：由 head/tail 的 handler 体模板拼接生成进
   execution_dispatch.c 的生成段（单独 include 文件，
   手写区与生成区物理隔离——index"不堆积巨型文件"）。
```

确定性要求（03.04 断言"确定性生成 hash 相同"）：模式表排序固定、窗口扫描顺序固定、无任何依赖指针值/哈希遍历序的决策——同输入两次 lowering 逐字节一致。

## E.3 AOTIR 与 C/LLVM 同源 lowering（07.01–07.02）

AOTIR = ExecIR + ABI 物化，**不重建语义**。现有 `SZrAotExecIrFunction` 等私有记录按 07.01 的 adapter 迁移：

```c
typedef struct SZrAotIrFunction {
    const SZrExecIrFunction *source;     /* 语义只读引用，AOTIR 不复制指令语义 */
    /* 只追加 ABI 物化层： */
    SZrAotCallConvention callConvention; /* 参数寄存器/栈序，来自 zr_aot_abi.h 契约 */
    SZrAotFrameMaterialization frame;    /* frameLayout → 目标机帧（含 spill 区）*/
    SZrAotGcRootMapEmission rootMap;     /* GcMap → 目标 pc 区间表 */
    SZrAotEhTableEmission ehTable;       /* 异常边 → landing pad 表 */
} SZrAotIrFunction;
```

两后端共享一个 lowering 驱动、各自只实现 emit 原语——保证"同源"不是口号而是代码结构：

```c
/* 后端只提供这组原语（草案），驱动统一遍历 AOTIR 调用之： */
typedef struct SZrAotEmitBackend {
    FZrAotEmitPrologue emitPrologue;     /* 帧建立 + root frame 注册（对接
                                            state.h 现有 SZrAotGcRootFrame 链）*/
    FZrAotEmitInstruction emitInstruction;
    FZrAotEmitSafepoint emitSafepoint;   /* publish 语义与解释器同源（guide C §C.1）*/
    FZrAotEmitGuard emitGuard;           /* generation/shape guard，miss 落
                                            结构化出口，语义同 guide C §C.2 */
    FZrAotEmitEpilogue emitEpilogue;
} SZrAotEmitBackend;
/* C 后端 emit 原语产出 C 源文本；LLVM 后端产出 IR builder 调用。
   分派/循环/异常结构由驱动决定，后端无权改变控制流形状——
   两后端行为分歧只能出现在原语内部，差分测试按原语粒度定位。 */
```

C 后端具体注意（07.02 批次内）：
- checked 算术生成 `__builtin_add_overflow`（GCC/Clang）/ `_add_overflow` 系（MSVC 用内联检查序列），**不依赖** `-fwrapv` 或 UB；
- 每个 mayGc 点物化 publish：把 live ref 槽写回 root frame 数组再调用 helper——root map 记录的是"哪些 C 局部变量此刻是 root"，生成的 C 代码用 `state->aotGcRootFrameStack` 现有机制注册；
- 解释器 fallback 调用必须经 `zr_aot_fallback_invoke(...)` 唯一入口并计数（07.02"不能用辅助函数名掩盖解释 fallback"）。

## E.4 覆盖率与 fallback 归因（07.04 的数据结构侧）

```c
typedef enum EZrAotSiteExecutionMode {
    ZR_AOT_SITE_NATIVE = 0,             /* 本站点 AOT 原生执行 */
    ZR_AOT_SITE_NATIVE_HELPER,          /* 原生但经 runtime helper（单列，不算 fallback）*/
    ZR_AOT_SITE_INTERPRETER_FALLBACK    /* 回解释器 */
} EZrAotSiteExecutionMode;
/* 计数按站点静态登记 + 运行期命中累加；coverage 分母 = 全部登记站点，
   融合/内联不得减少分母（07.04）。noCoverageData → 报 unavailable，
   永不显示 100%。 */
```

## E.5 常见坑清单（实现前读一遍）

| 坑 | 症状 | 规避 |
| --- | --- | --- |
| phi 并行拷贝串行化成环未中转 | swap 语义错，差分测试值交换失败 | E.1 步骤 2 的拓扑+中转算法，负测必含 swap fixture |
| 关键边未 split 就插拷贝 | 拷贝污染另一后继路径 | verifier 投影前强制无关键边 |
| 融合指令跨 effect token | GC/异常点被吞，root map 缺口 | NO_INTERVENING_EFFECT 约束机械检查 |
| C 后端依赖有符号溢出 UB | GCC -O2 下删检查，差分过但 sanitizer 炸 | 一律 __builtin_*_overflow |
| oracle 的 phi 顺序求值 | phi 互依赖时读到本轮新值 | guide A §A.7 two-phase 读写 |
| 磁盘行宽 sizeof(内存结构) | 结构体加字段后旧 artifact 静默错读 | guide D §D.4 独立字段表，roundtrip 测试 |
| lease 裸 load+increment | 热更替换窗口 UAF | guide D §D.5 CAS+double-check 协议 |
