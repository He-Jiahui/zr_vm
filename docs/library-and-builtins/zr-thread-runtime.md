---
related_code:
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/module.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_transport.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_wrappers.c
  - zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_generic_calls.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_transport.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_wrappers.c
  - zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_generic_calls.c
plan_sources:
  - user: 2026-04-08 zr.thread 线程安全与跨线程所有权收口计划
tests:
  - tests/thread/test_thread_runtime.c
  - tests/task/test_task_runtime.c
doc_type: module-detail
---

# zr.thread Built-in Runtime

## Scope

`zr.thread` 现在固定为 isolate-first、static-first 的线程模型。

- 一个 `SZrGlobalState` 只属于一个 GC 域和一个线程执行域。
- 普通 isolate 堆对象不允许被两个线程 VM state 直接并发解引用。
- 跨线程关系必须显式落在 `zr.thread` 容器和 marker contract 上。

`supportMultithread = false` 只关闭 worker/shared transport 路径，不改变单线程 `zr.task` / `zr.coroutine` 的本地调度语义。

## Public Surface

当前公开面收敛为：

- `zr.thread.Send`
- `zr.thread.Sync`
- `Thread`
- `Scheduler`
- `Transfer<T: Send>`
- `Channel<T: Send>`
- `Shared<T: Send + Sync>`
- `WeakShared<T: Send + Sync>`
- `UniqueMutex<T: Send>`
- `SharedMutex<T: Send + Sync>`
- `Lock<T>`
- `SharedLock<T>`
- `spawnThread()`
- `getCurrentThreadScheduler()`

本轮不再公开旧接口：

- `%mutex`
- `%atomic`
- `AtomicBool`
- `AtomicInt`
- `AtomicUInt`
- 任何按 concrete type-name 硬编码的 “sendable whitelist” 语义

parser 现在直接拒绝 `%mutex` / `%atomic`，不会再走兼容 wrapper。

## Ownership Layering

`zr.thread` 明确把 isolate 内所有权和跨线程所有权拆成两层：

- `%unique/%shared/%weak/%borrowed/%loaned`
  - 继续只表达 isolate 内生命周期和别名关系
- `Send/Sync`
  - 只表达跨线程 move / shared contract

两套规则不再隐式互推。

这意味着：

- 一个 primitive `int` 可以天然满足 `Send/Sync`
- 但 `%borrowed int`、`%shared int`、`%weak int`、`%loaned int` 不会因为底层是 primitive 就自动满足 `Send/Sync`
- `Lock<T>` / `SharedLock<T>` 虽然承载 `T`，自身仍然是仿射 guard，故意不实现 `Send/Sync`

## Compile-Time Contracts

`zr.thread` 的泛型约束已经转到 descriptor metadata，而不是旧的类型名分派：

- `Transfer<T>` / `Channel<T>`
  - `T: zr.thread.Send`
- `Shared<T>` / `WeakShared<T>`
  - `T: zr.thread.Send + zr.thread.Sync`
- `UniqueMutex<T>`
  - `T: zr.thread.Send`
- `SharedMutex<T>`
  - `T: zr.thread.Send + zr.thread.Sync`
- `Thread.start(runner)` / `Scheduler.start(runner)`
  - runner result `T: zr.thread.Send`

当前 parser/type inference 已经做的静态约束：

- primitive、字符串、递归数组 element 会按值语义推导 `Send/Sync`
- `%borrowed/%shared/%weak/%loaned` ownership qualifier 不满足 `Send/Sync`
- `Lock<T>` / `SharedLock<T>` 不实现 marker contract
- `compiler_task_effects.c` 会在 `%await` 边界后拒绝继续使用 borrowed / loaned binding 和 affine guard

### Inference Repair for Thread Runners and Generic Containers

这轮补齐的关键点不只是公开 API，而是把 thread-safe 约束真正压回共享低层：

- lambda return inference 现在会在分析 block body 之前，先把 block 内可见的局部函数声明预注册到 type environment
  - 这修复了 `spawnThread(() => { %async runner() { ... }; return runner(); })` 这类写法
  - worker runner lambda 里定义的局部 `%async` 函数现在能稳定推导成 `zr.task.TaskRunner<T>`
- generic call / member call 在完成泛型实参绑定后，会再次统一校验 contract 约束
  - 这保证 inferred `T` 即使不是显式写出来的，也仍然必须满足 `Send` / `Sync`
- open generic construct expression 现在会从 constructor arguments 反推闭合后的 generic type，并在物化前校验约束
  - `Transfer(...)`、`Shared(...)`、`UniqueMutex(...)`、`SharedMutex(...)` 这类构造不再能绕过 generic contract 检查
  - 旧的 “constructor shorthand 先过，约束晚一点再说” 路径已经被收口
- `%await` 相关的 borrowed / loaned / guard 诊断继续作为 compile pre-pass 运行，而不是依赖后续 statement compile 偶然触发
  - 这样 worker/task 相关的仿射与借用错误会在真正进入 script compile 之前被稳定拦截

当前已经稳定报错的路径包括：

- `%mutex` / `%atomic`
- `Transfer<NonSend>`
- `Shared<NonSync>`
- borrowed / loaned binding 在 `await` 之后继续使用
- `Lock` 在 `await` 之后继续读取、解锁或被转存

## Runtime Materialization Model

跨线程 transport 不再允许裸 isolate 指针直接穿过队列或 worker 边界。

### Transfer / Channel

- `Transfer<T>` 仍然保持 move-only 语义
- `Channel<T>` 和 `Transfer<T>` 共用 transport 编码层
- payload 必须先 materialize 成可跨线程 transport 的值
- runtime 诊断也改成直接使用 `Send` 术语，而不是旧的 “sendable values”

### Shared / WeakShared

`Shared<T>` 使用独立 shared control cell，而不是复用 isolate 内 weak/GC 账本。

当前 shared cell 维护：

- strong refcount
- weak refcount
- 显式生命周期状态
  - `Alive`
  - `Destroying`
  - `Dropped`

当前实现约束：

- `WeakShared.upgrade()` 通过单步 strong-ref 提升完成，不走 “先 isAlive 再使用” 的分离路径
- `Shared.downgrade()` 也改成同一个锁区间内检查存活并添加 weak ref，避免生成已经失效的 weak handle
- shared payload 释放时先切到 `Destroying`，清空已 materialize 的 payload，再进入 `Dropped`

shared cell 只存 transport 后的 payload，不允许长期保留 isolate stack slot、borrowed alias 或 isolate heap 指针。

### Mutex Containers

`UniqueMutex<T>` / `SharedMutex<T>` 只是同步容器，不承担 shared ownership。

- 需要跨线程共享时，外层必须再放进 `Shared<...>`
- `UniqueMutex<T>` 只接受 `Send`
- `SharedMutex<T>` 只接受 `Send + Sync`

guard 规则：

- `Lock<T>` 是独占 guard
- `SharedLock<T>` 是共享读 guard
- 两者都是 affine，不能跨 `await`
- 两者也不实现 `Send/Sync`

## Worker Isolates

`Thread.start(...)` 和 `Scheduler.start(...)` 的 worker 路径当前遵循：

- capture 必须能 materialize 成 `Send` payload
- result 必须能 materialize 成 `Send` payload
- worker isolate 通过 transport/value materialization 与 owner isolate 交换数据
- completion continuation 仍回到 awaiter 所在 scheduler

也就是说，“两个线程 VM state 同时解引用” 只对 shared cell / mutex cell 这类显式跨线程容器成立，不对普通 isolate heap object 成立。

## Legacy Cleanup

本轮清理的遗留点包括：

- parser 中 `%atomic` wrapper-name 兼容逻辑删除
- `zr.thread` 的 legacy mutex / atomic public shape 删除
- runtime 错误文案改成 `Send` / `Sync` 约束语言
- 文档不再把 `%mutex` / `%atomic` 当作仍可兼容的旧接口说明

如果未来需要重新公开 lock-free 单值原语，需要单独设计新的 API；当前不会复活旧 `AtomicBool/AtomicInt/AtomicUInt` 名字。

## Validation Status

2026-04-08 针对本里程碑重新做了 focused WSL Debug 验证，覆盖 gcc 与 clang 两条工具链。

执行命令：

- `wsl.exe bash -lc "cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug --target zr_vm_task_runtime_test zr_vm_thread_runtime_test -j4"`
- `wsl.exe bash -lc "stdbuf -o0 /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug/bin/zr_vm_task_runtime_test"`
- `wsl.exe bash -lc "stdbuf -o0 /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug/bin/zr_vm_thread_runtime_test"`
- `wsl.exe bash -lc "cmake -S /mnt/e/Git/zr_vm -B /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make --target zr_vm_task_runtime_test zr_vm_thread_runtime_test -j4 && stdbuf -o0 /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make/bin/zr_vm_task_runtime_test && stdbuf -o0 /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make/bin/zr_vm_thread_runtime_test"`

结果：

- gcc `zr_vm_task_runtime_test`
  - `16 Tests 0 Failures 0 Ignored`
  - 覆盖 `%mutex/%atomic` rejection、`%await` 对 `TaskRunner` 的拒绝、borrowed / loaned value 跨 await 诊断、默认 scheduler 行为
- gcc `zr_vm_thread_runtime_test`
  - `20 Tests 0 Failures 0 Ignored`
  - 覆盖 `Send/Sync` descriptor metadata、thread marker 对 isolate-local ownership qualifier 的拒绝、local async runner inference、worker isolate result roundtrip、`Transfer/Shared/WeakShared` contract 拒绝、`UniqueMutex/SharedMutex` guard 行为
- clang `zr_vm_task_runtime_test`
  - `16 Tests 0 Failures 0 Ignored`
- clang `zr_vm_thread_runtime_test`
  - `20 Tests 0 Failures 0 Ignored`

这轮额外确认了两个低层修复都已经被 thread/task 回归真正消费到：

- 局部 `%async` 函数在 lambda-wrapped worker runner 中能稳定推导并执行
- `Transfer<NonSend>`、`Shared<NonSync>`、guard transfer rejection 现在会在 generic constructor / contract 路径上稳定失败，而不是依赖旧的 wrapper-name 或 runtime whitelist

clang 构建过程中仍有若干既有 warning，但本里程碑没有引入 gcc/clang 结果分叉。

本页只记录本里程碑的 focused 证据；它不替代整个仓库的 gcc/clang/MSVC 全矩阵验收。
