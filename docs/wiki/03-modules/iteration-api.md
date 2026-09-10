---
related_code:
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_core/include/zr_vm_core/iterator_runtime.h
  - zr_vm_core/src/zr_vm_core/iterator_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
implementation_files:
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_core/src/zr_vm_core/iterator_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/iteration/test_iteration_module.c
  - tests/container/test_container_module.c
  - tests/parser/test_syntax_reference_v1.c
doc_type: api-reference
---

# `zr.iteration` API 参考

迭代 provider 只发布协议和 opaque iterator；具体集合负责实现 `getIterator`。同步协议和
异步协议不互相隐式转换，避免把挂起语义藏在普通 `for` 循环里。

## 协议目录

| 类型 | 成员 | 说明 |
| --- | --- | --- |
| `Iterable<T>` | `getEnumerator(): Enumerator<T>` | 同步生产者协议。 |
| `Enumerator<T>` | readonly `current:T`、`moveNext(): bool` | `moveNext` 成功后 current 才更新。 |
| `Iterator<T>` | opaque struct，实现 Enumerator | 由 container/adapter 创建，脚本不能伪造内部 cursor。 |
| `AsyncIterator<T>` | `current:T`、`moveNext(): zr.task.Task<bool>`、`close(): zr.task.Task<void>` | 异步生产者，必须显式 close。 |

## foreach 语法

```zr
for (let item in collection) {
    consume(item);
}
```

parser 把 foreach 记录为 `in` 形式；semantic phase 查找 `getEnumerator`，然后生成
`moveNext/current` 调用和 cleanup edge。无法证明 collection 是 Iterable<T> 时报告 protocol
error。迭代变量是当前元素的 value；若需要修改集合元素，显式取得 `ref`/index place，不能
假定 `let item` 是可写引用。

## 生命周期

同步 iterator 通常只借用 collection；collection 修改可能使 cursor 失效，provider 可以抛
invalidated iterator error。异步 iterator 的 `moveNext` 会挂起，collection/iterator owner
必须可 frame-safe；活动 `ref` 或 lock 不能跨 await。使用 `using` 或 finally 调用 `close`：

```zr
let iterator = producer.getEnumerator();
try {
    while (iterator.moveNext()) {
        consume(iterator.current);
    }
} finally {
    // 若 concrete iterator 有 close，在这里释放
}
```

`AsyncIterator.close` 返回 Task<void>，关闭失败应在调用方 await 并保留原始异常。重复 close
是否幂等由具体 provider descriptor 声明，网络/文件 iterator 通常采用幂等 close。

## 容器适配

`Array<T>`、`Map<K,V>`、`Set<T>` 和 `LinkedList<T>` 的 `getIterator` 都返回
`Enumerator<...>`；Map 的元素是 `Pair<K,V>`。Array iterator 使用连续 index，LinkedList
使用 node cursor，Map/Set 使用 entries table。所有 adapter 都在 `zr.container` 注册时
安装，不能在没有 container provider 的 state 中单独构造。

## C 侧实现要点

native module 发布 Iterable/Enumerator descriptor 时要声明 generic parameter、current field
的 readonly reference access 和 method return type。callback 中按 index 读取参数、创建
iterator object 后，临时 object 必须 root；返回的 current value 需要 `ZrLib_Value_Set*` 或
`ZrCore_Value_Copy` 写入 result。不要把 C stack 上的 cursor 地址塞进 managed field，使用
native pointer + finalizer 或 managed state wrapper。

core 的 iterator runtime 负责识别 `moveNext/current` contract、绑定 method token 和处理
pending cleanup；module unload/reload 后旧 token 需重新解析。调试器看到的 iterator frame
只保证 snapshot，不保证 cursor 地址稳定。

## 常见错误

| 错误 | 原因 |
| --- | --- |
| `not iterable` | 类型没有满足 Iterable<T>。 |
| `current before moveNext` | 在第一次成功移动前读取 current。 |
| `iterator invalidated` | 集合修改使 cursor 失效。 |
| `async iterator across await` | owner/loan 不能 frame-safe。 |
| `generic mismatch` | producer 的 T 与消费端 T 不一致。 |
| `close after state free` | iterator 仍引用已销毁 state。 |

配合任务使用时参阅[任务 API](task-api.md)，集合具体方法见[容器 API](container-api.md)，语法
规则见[控制流与资源清理](../02-language/control-flow.md)。
