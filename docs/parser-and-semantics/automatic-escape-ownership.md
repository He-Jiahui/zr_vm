---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_escape.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_hash.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_summary.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_escape.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_hash.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_summary.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/03-escape-ownership.md
  - docs/plans/ssa/guides/B-passes-analysis.md
  - docs/plans/ssa/architecture-design.md
tests:
  - tests/parser/test_ssa_escape_ownership.c
doc_type: module-detail
status: implemented
---

# ExecIR 逃逸摘要（02.03 批次 1）

本阶段只生产 parser-owned 的逃逸摘要；摘要记录和原因边是无裸指针的稳定标量，
仅由摘要自己拥有的数组指针承载存储。它不改写 ExecIR，也不直接决定 frame 或 GC 的物理布局。后续 allocation/ownership-elision pass 必须
把摘要当作证明输入，并在摘要缺失、哈希不匹配或状态为 `unknown` 时保留
managed-heap 和原始 ownership 操作。

## 图和状态

`SZrExecIrEscapeSummary` 为每个 SSA value 保存定义、最后一次使用、半开生命
区间、分配点、状态和第一条原因边；无普通 use 的定义以相等的起止点表示空区间，
`lastUseInstructionId == 0` 另作无 use 标记。`SZrExecIrEscapeEdge` 是稳定的
`valueId/instructionId/sourceId` 记录：value-flow 边的 `targetValueId` 非零，
return/store/call/native/suspend 等观察边的 target 为零。传播到上游时，边的
`parentEdgeIndex` 指向下游事实的第一条原因，因此诊断可以还原完整原因链，
而无需保存 AST 或运行时地址。

`identityObserved` 表示值已经经过任一外部观察边（return、store、调用、
捕获、挂起或异常清理），而不只表示它最终落在静态堆；因此未知边也会把
该位设为真。

摘要必须通过 `ZrParser_ExecIr_EscapeSummaryInit` 初始化，并由对应的 `Free`
释放；结构带有生命周期 magic，重复释放或对零初始化输出对象分析不会解引用
未知指针。若边界保守地延长了一个无普通 use 值的生命，`liveEnd` 会反映边界，
而 `lastUseInstructionId` 仍保持零以便消费者识别该情况。

状态传播遵循保守单调规则：

`ZR_EXEC_IR_ESCAPE_LOCAL` 的诊断名称沿用 SemIR 词汇 `block`；`local` 仍是 C
枚举和字段中更直观的兼容拼写。

| 观察 | 原因 | 状态 |
| --- | --- | --- |
| `RETURN` | `return` | `caller` |
| `STORE` / `BARRIER` | `heap-store` | `heap/static` |
| `CALL` / `INVOKE` | `unknown-call` | `unknown` |
| 有 binding row 的调用 | `native-retained` | `unknown` |
| `SUSPEND` 或 suspend 后仍活跃 | `escape-across-suspend` | `unknown` |
| 带 `MAY_SUSPEND` 的 `CALL` / `INVOKE` 参数 | `worker-parameter` | `unknown` |
| `THROW` | `exception` | `unknown` |
| `MAY_THROW` 边界上仍活跃 | `exception` | `unknown` |

COPY/MOVE/CONVERT/PLACE 的输入和输出建立 value-flow 边；phi incoming 也建立
边。所有未知调用均按捕获处理，不能因为调用有 binding row 就假定参数不被
保留。跨 suspend 的判定同时检查显式参数和定义到后续使用之间的活跃区间，
因此协程帧移动不会留下栈别名。

## 分配决策边界

批次 1 只输出三态决策字段。默认是 `heap`；只有具有 `ALLOC` 定义、没有任何
逃逸观察、没有可观察 `DROP`、且分配指令带有 concrete `layoutId` 的 GC-owned
值才标记为 `stack`（单独的 type token
不能证明 byte size/alignment）。`region`
保留给 06.01 的 TLAB/region 协议。即使摘要显示 `stack`，后续落地仍必须：

1. 在 GC map 中记录栈对象内部引用；
2. 保持 DROP/finalizer 的 ownership 顺序；
3. 为 deopt 提供可验证的堆物化路径。

摘要分析先运行 `VerifyFunction(STRUCTURE|SSA)`，失败时不替换调用方已有的
摘要。成功结果带 `irHash`；消费者可用
`ZrParser_ExecIr_EscapeInputHash` 重算同一逃逸输入见证，再决定是否复用摘要。
它不是安全签名，也不替代 pass manager 的完整 IR hash。

## 验证

`tests/parser/test_ssa_escape_ownership.c` 覆盖：局部分配候选、return forwarding
原因链、未知调用/native retained、heap store 的 identity 观察、跨 suspend 活跃
区间（含函数参数跨边界）、可观察 DROP 保护、stale definition 以及 malformed 输入的结构化诊断。主构建接入后使用：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_escape_ownership_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_escape_ownership$' --output-on-failure --no-tests=error
```

当前实现也以独立 GCC/Clang `-std=c11 -Wall -Wextra -Werror -pedantic` 编译，
并通过 AddressSanitizer/UndefinedBehaviorSanitizer fixture。该批次没有 allocation
或 copy rewrite；因此析构时序、GC roots 和 deopt 状态仍由原始 IR 保持不变。
