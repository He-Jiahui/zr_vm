# Syntax 13 M1 Enumerator Protocol Record

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 23:10 +08:00
- 完成时间：2026-07-25 00:55 +08:00
- 完成项目：
  - 新增 `zr.iteration` N1 native descriptor，并由它独占 `Iterable<T>`、`Enumerator<T>`、`Iterator<T>`、`AsyncIterator<T>` 四个公开 TypeId。
  - 移除 builtin 的 `IEnumerable` / `IEnumerator` descriptor owner；`Array`、`Map`、`Set`、`LinkedList` 只发布 `zr.iteration.Iterable<...>` capability metadata，工厂返回 `zr.iteration.Enumerator<...>`。
  - `for` 元素绑定改由 `ZrParser_EnumeratorBinding_ResolveElementType` 消费 `ITERATOR` / `ITERABLE` protocol fact；数组 inference 显式投影 `ITERABLE` fact，既有静态 `ITER_*` lowering 未改写。
  - 增加 descriptor owner、容器迁移、直接 Enumerator、Iterable factory、无 capability、ref-like capability、loop break cleanup 与静态 lowering 回归；补齐语言测试要求与模块文档。
- 验收产出：独立 GCC、Clang、MSVC `s13m1` 构建目录中，enumerator protocol（5/5）、type inference（119/119）、container type inference（12/12）、numeric foreach cardinality（2/2）和 numeric loop assignment（16/16）均以真实进程 exit 0 通过。
- 基线说明：focused CTest 选择没有已注册条目，因此不将其 exit 0 计作测试通过；直接 Unity 二进制是验收证据。`zr_vm_container_metadata_test` 的 MSVC 重放保留 closed `Map.containsKey` prototype-materialization marker（3 tests、1 failure）；GCC/Clang 的该非验收目标在更新断言后的隔离重建未在时限内完成。GCC/Clang `zr_vm_container_runtime_test` 均保留同一既有 `string` import-metadata marker（49 tests、1 failure）。这些标记均未计入 M1 通过证据。

## 当前公共合同

- `zr.iteration` 独占 `Iterable<T>`、`Enumerator<T>`、`Iterator<T>`、`AsyncIterator<T>` TypeId。
- `for` 只通过 `ITERATOR` / `ITERABLE` canonical protocol fact 绑定元素类型；数组 inference 显式发布 `ITERABLE` fact。
- M1 不引入 `yield`、generator、async lowering 或 boxing adapter。
