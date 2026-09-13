---
related_plan: docs/plans/ssa/02-automatic-optimization/04-interprocedural-inlining.md
implementation:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_interprocedural.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_devirtualize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
test: tests/parser/test_ssa_interprocedural_inlining.c
---

# ExecIR 跨函数摘要、去虚化与内联

本实现把跨函数优化事实收敛到一个 parser-owned、无裸指针的数据结构
`SZrExecIrCallGraph`。图中的函数摘要和调用边只保存 function id、metadata
token（同时保留可重定位 contract target token）、signature hash、generation、
effect bits 和 source/instruction id；因此
它们可以安全地作为增量分析缓存键，也不会把运行期 `SZrFunction *` 写入
ExecIR 或 artifact。

## 摘要与调用图

`ZrParser_ExecIr_BuildCallGraph` 先验证 module，再按函数体计算稳定的 FNV-1a
body hash。`CALL`/`INVOKE` 的目标解析顺序是：

1. `layoutId` 的唯一 metadata 匹配优先作为静态目标；`typeToken` 通常是结果/可调用
   类型，只在 virtual/interface/typed hint 下作为间接候选；
2. 若轻量 fixture 没有 target metadata，`bindingRow` 的低 29 位可作为模块内 function id
   （高位可携带 virtual/interface/typed-function 提示）；
3. 其余站点保留 slot/native 边；真正 native-shaped 的未知边标记
   `nativeEffectsUnknown`，普通间接 slot 则以 `INDIRECT` 原因保守提升为全部共享
   effect bits。

解析结果不依赖名字查找。每个 SCC 通过互达关系分组，摘要 effect 使用单调
并集固定点传播：递归或互递归函数不会因为初始化顺序而丢失 I/O、分配、异常、
挂起或逃逸效果。未知 native/FFI 站点始终是保守摘要，不能成为 purity 或内联
证明。没有 ExecIR body 的 native/AOT 函数仍从
`contract.declaredEffects` 产生摘要；DROP、GC 边界和 ownership return 也会
关闭 purity。摘要的 `bodyHash`、`importedHash` 和 `summaryHash` 共同组成失效键；模块
或任一导入函数改变后，旧图会被 `CallGraphValidate` 拒绝。

只有 `sealed` 且 generation 非零的函数才是冻结发布体；generation `0`（缺少发布
contract）以及任何未 sealed 的 generation（包括初始 generation `1`）都按可热更/不稳定
目标处理。sealed 函数是不可变的发布边界，即使经历过 reload、generation 大于 `1` 也
可以作为稳定目标（其 generation 仍会进入摘要和 edge identity）。可热更目标默认禁止
内联；调用仍通过 slot/binding baseline，避免执行过期 body。

## 去虚化与受限内联

`ZrParser_ExecIr_DevirtualizeCalls` 只把 exact receiver、generation 匹配且非
patchable 的边改成直接 function-id binding。profile 站点若只有 guarded/deopt
证据，仍保留 slot；签名、target 或 generation 不匹配返回结构化 diagnostic。
去虚化会改变 module hash；若后续还要运行其他图消费者，应重新调用
`BuildCallGraph` 获取新的 revision，而不是复用已失效的旧图。

`ZrParser_ExecIr_InlineCalls` 当前启用一个可验证的最小子集：冻结、稳定、纯、
不抛错/不挂起/不逃逸，且 body 是若干纯值指令后跟单一 `RETURN` 的 callee。每个
调用按 `maxCostPerCall`、每函数增长和模块总增长预算检查；递归、patchable、
effectful、`INVOKE` 异常边、带 GC/deopt/state map 或超预算站点保持原调用。克隆时：

- 重新映射参数、结果和 SSA definition；
- 对 `return parameter` 的 identity body 生成显式 `COPY`，保留返回值的 SSA
  定义和 ownership/type 检查；
- 保留 callsite 的 source identity，并追加 callee source map；已有 deopt/state
  side table 的 caller/callee 站点目前按 `STATE_MAP` 拒绝，待完整 map remap
  contract 接入后再开放；
- 调整后续 instruction、block 和 GC site id；
- 任何分配或不完整映射失败都回滚整个 caller，不留下半个 inline body。

这是一条安全的基础路径，并不声称能克隆带 cleanup、异常边或 inline frame 的
一般函数；这些函数显式以 `STATE_MAP`/`EFFECTS` 原因拒绝，等待后续 frame/state
map contract 完整后再扩展。

## 依赖失效清单

- 函数指令、frame layout、GC/deopt/state/source side table 改变：函数
  `bodyHash` 改变，旧摘要和所有导入它的 caller 都必须重算；
- callee 的 `signatureHash`、canonical `targetToken` 或 generation 改变：
  对应 edge 变成 link/stale-generation 失败，不能按名称回退；
- module contract、布局/常量池或 facts row 改变：`moduleHash`/`graphHash`
  失配，先丢弃缓存再重新构建；
- 去虚化或内联修改 ExecIR 后，原图不再适用于后续消费者；必须重新调用
  `BuildCallGraph`，让新的 revision 成为失效边界。

生命周期/失败约束：`BuildCallGraph` 先在临时 graph 中完成分配、SCC 和摘要，
只有全部成功才替换旧 graph；OOM、结构验证或 hash 失败会保留旧缓存。`Init` 可
安全地重复调用已初始化 graph，`Free` 可重复调用；当前分析 API 没有取消 token，
因此取消状态不适用，调用方应在 pass 边界丢弃临时 graph。

## 聚焦验证

测试 fixture 覆盖：

- 含未知 native effect 的互递归 SCC，断言双方摘要均保守且 SCC id 相同；
- 唯一 metadata receiver 的 virtual slot 去虚化、signature mismatch 和 guarded
  target resolution；
- 纯常量函数内联、callsite/source-map 保留；
- identity `return parameter` 的 COPY 转发和 ownership/type 校验；
- `maxCostPerCall` 超限、普通 facts row 无 row-table、以及非 exact receiver
  时调用和 IR 完全不变；
- 逆序声明的深层 A→B→C 依赖在 C 改变后同时失效 A/B imported hash。

在尚未接入总 CMake 注册点的开发环境，可用以下命令独立运行同一 fixture：

```bash
gcc -std=c11 -DZR_PLATFORM_UNIX -DZR_DEBUG -Wall -Wextra -Werror \
  -I zr_vm_parser/include -I zr_vm_core/include -I zr_vm_common/include \
  tests/parser/test_ssa_interprocedural_inlining.c \
  zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c \
  zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c \
  zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c \
  zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c \
  zr_vm_core/src/zr_vm_core/execution_contract.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_devirtualize.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c \
  -o /tmp/ssa_interprocedural_inlining_test && \
  /tmp/ssa_interprocedural_inlining_test
```

The same source set is intended for the registered CTest target
`ssa_interprocedural_inlining`. Sanitizer runs should use the exact source set and
the same fixture; no pointer value is part of the expected output.
