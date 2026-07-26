# Syntax 12 M6.4b: Canonical Generic Projection Convergence

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-26 03:46 +08:00
- 完成时间：2026-07-26 08:13 +08:00
- 完成项目：
  - 完成并收口 M6.4a 留下的六项 Unity marker：receiver completion、
    native receiver hover、foreach shadowing、container matrix、native value
    constructor 与 interface variance；全部继续通过 structured semantic
    facts、canonical prototype 与 resolved callable identity 投影，不按名称、
    source text 或展示字符串回退。
  - source-import 的数组 TypeRef、native generic iterable 和 closed generic
    type prototype 都保留 `Iterable` protocol fact；compiler/LSP 不再将
    `int[6]` 误解释为 source module。
  - const generic parameter reference 与 literal argument 使用相同 canonical
    inferred-type contract。`N` 保留为 `CONST_PARAMETER`，`4` 保留为
    `CONST_INT`；仅持有 canonical `Derived<Item, 4>` type name 的原型物化
    路径也会严格恢复整数实参，不会把它降级为普通 object/type argument。
  - 修正 LSP 共享库公开 semantic analyzer entry point 的导出标记，MSVC 的
    LSP consumer 不再因 unresolved exported symbol 无法链接。
- 验收：
  - 固定 `HEAD + M6.4b exact overlay` 的独立快照上，GCC 11.4、Clang 14.0
    和 MSVC 17.14 均完成 18-target LSP matrix：每套 18 个真实进程 exit 0，
    54 份 Unity 日志均无 `Fail - Cost Time` marker。
  - 每套工具链的 main stdio/CLI、position-encoding stdio 与
    diagnostic-fix stdio smoke 都以真实 exit 0 完成，共九项。
  - `zr_vm_type_inference_test` 中本阶段新增的
    `Native Generic Iterable Propagates Protocol` 和
    `Source Import Array Preserves Iterable Protocol` 均通过。该全量目标仍有
    两项既有失败（callable reflection parser diagnostic count、native generic
    receiver contract expectation），它们未被纳入 M6.4b 通过证据，也未被
    本阶段改动放宽。
