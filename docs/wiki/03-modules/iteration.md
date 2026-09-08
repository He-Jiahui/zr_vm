---
related_code:
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_core/include/zr_vm_core/iterator_runtime.h
  - zr_vm_parser/include/zr_vm_parser/iteration_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c
implementation_files:
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
tests:
  - tests/iterator/test_enumerator_protocol.c
  - tests/iterator/test_iterator_runtime.c
  - tests/iterator/test_iterator_gc_drop.c
  - tests/iterator/test_yield_syntax.c
  - tests/iterator/test_yield_semantic.c
doc_type: module-detail
---

# `zr.iteration`

**状态：`current`；Runtime provider，descriptor 版本 `1.0.0`，公开 contract
`zr.iteration:v1:canonical-iterator-protocols`。该模块只注册协议，不注册容器实现。**

## 导出签名

| 协议 | 成员 |
| --- | --- |
| `Iterable<T>` | `getEnumerator(): Enumerator<T>` |
| `Enumerator<T>` | `current: T`（成功推进后有效）；`moveNext(): bool` |
| `Iterator<T>` | 编译器承载的 opaque struct；无脚本构造器，供 `yield` frame 使用 |
| `AsyncIterator<T>` | `current: T`；`moveNext(): zr.task.Task<bool>`；`close(): zr.task.Task<void>` |

`current` 是字段而不是方法；在第一次成功 `moveNext()` 前或返回 `false` 后读取它属于
协议错误。`AsyncIterator.close()` 必须在正常结束、异常和取消路径都被等待。

迭代 provider 只拥有协议类型，不拥有具体容器。协议身份是 `Iterable<T>`、
`Enumerator<T>`、`Iterator<T>` 和 `AsyncIterator<T>`；Array、Map、Set、LinkedList
等通过 descriptor 的 `implements` 行引用这些 canonical TypeId。

## 协议

```zr
interface Iterable<T> { fn getEnumerator(): Enumerator<T>; }
interface Enumerator<T> { fn moveNext(): bool; let current: T; }
interface AsyncIterator<T> {
    fn moveNext(): zr.task.Task<bool>;
    let current: T;
    fn close(): zr.task.Task<void>;
}
```

`Iterator<T>` 是编译器/运行时承载 frame 的非构造值类型。`getEnumerator()` 建立枚举器，
成功的 `moveNext()` 才使 `current` 有效；返回 false 后读取 `current` 属于协议错误。
异步枚举器的 `moveNext` 返回 Task，`await` 边界同样参与借用检查。

## `for` 降低

```zr
for (let item in source) {
    consume(item);
}
```

parser 先解析 `for-in`，枚举绑定器调用
`ZrParser_EnumeratorBinding_ResolveElementType`，从 canonical `ITERABLE`/`ITERATOR`
projection 取得元素 TypeId，再生成 `ITER_INIT`、`ITER_MOVE_NEXT`、`ITER_CURRENT` 事实。
它不比较 `Array` 名称、`getEnumerator` 拼写或 `ARRAY_LIKE` 位。循环退出、异常、break 和
continue 都进入统一 scope cleanup；迭代器 close/drop 不依赖词法路径是否正常结束。

## yield 与 frame

`yield` 将当前 iterator frame 保存为 READY/YIELDED 状态并返回调用方；再次驱动时恢复
栈、局部、借用 loan 和 program counter。终止转换为 COMPLETED，异常转换为 FAULTED，
显式关闭转换为 CLOSED；终态驱动是幂等的。frame 中的 GC 值通过 root map 保持可达，
不会把 C 局部地址当作长期根。

## 注册和 C 入口

```c
const ZrLibModuleDescriptor *d = ZrVmLibIteration_GetModuleDescriptor();
TZrBool ok = ZrVmLibIteration_Register(global);
```

descriptor 阶段为 Runtime，contract hash 为迭代协议版本。provider 只登记协议和角色；
具体枚举器 callback 由容器或宿主 descriptor 提供。共享库入口为
`ZrVm_GetNativeModule_v1()`。任何新容器必须复用协议 role，不能重新声明同名 TypeDef。
