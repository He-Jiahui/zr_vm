# ExecIR 逃逸、分配与 ownership 消除

ExecIR 的逃逸分析由 `ZrParser_ExecIr_AnalyzeEscape` 产生带哈希见证的
摘要，随后由分配与 ownership pass 消费。摘要和两个计划都只保存定宽
标量、区间和原因链，不保存 runtime/parser 指针。

## 不变量

- 未知调用、native 保留、worker 参数、跨挂起点和可观察 identity 都把值
  提升到保守的 heap/unknown 状态；未知证据不会被当作局部值。
- GC 值即使被判定为局部 stack candidate，也保留 `requiresGcRoot`，由 frame
  lowering 继续建立根图。
- `ZrParser_ExecIr_BuildAllocationPlan` 和
  `ZrParser_ExecIr_BuildOwnershipElisionPlan` 绑定
  `ZrParser_ExecIr_EscapeInputHash`。输入函数改变后，Apply 明确返回
  `STALE_GENERATION`，不修改 IR。
- 分配计划是后端边界上的 placement witness；当前 ExecIR 没有伪造的
  stack/region opcode，因此 Apply 不改变 `ALLOC` 的效果语义。
- ownership 消除只把经过唯一 ownership、last-use、metadata、异常与
  suspend 检查的 `COPY` 改为 `MOVE`。批量应用先 clone，任何验证失败都回滚
  到原函数。

## 验证入口

`tests/parser/test_ssa_escape_ownership.c` 覆盖 return/store/closure/native/
worker/suspend/exception、GC root、drop 可观察性、allocation placement、
unique return forwarding、stale hash 和 malformed range。CMake 目标
`zr_vm_ssa_escape_ownership_test` 注册为 CTest `ssa_escape_ownership`。

未知逃逸始终保留原 heap/copy 路径；后续 frame、GC 和 deopt lowering 可以
消费同一份 hash-bound plan，而不需要重新按类型名猜测生命周期。
