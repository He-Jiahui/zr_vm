# Thread Helgrind Validation

## Scope
- 验证 `zr_vm_lib_thread` 当前 worker isolate / scheduler external wait / cross-isolate transport 路径的线程安全。
- 受影响层：
  - `zr_vm_lib_thread` runtime
  - focused thread tests under `tests/thread`
  - WSL Helgrind tooling evidence

## Baseline
- 代码变更前，`build/codex-wsl-gcc-debug/bin/zr_vm_thread_runtime_test` 普通运行已通过，9/9。
- 初次 Helgrind 运行在 `zr_vm_task_scheduler_wait_for_external()` 上稳定报错：
  - `pthread_cond_{signal,broadcast}: associated lock is not held by calling thread`
  - 调用链指向 `runtime.c:474` / `runtime_internal.h:145`
- 经过代码审查，worker enqueue / runtime signal 两个 signal 位点都持有同一把 `runtime->mutex`。可见问题集中在 Linux condvar wait 这条工具可见性不佳的等待链，而不是 `Shared/Channel/Transfer` 数据竞争。
- Windows 侧存在独立基线问题：重新构建 `build/codex-msvc-debug` 会在 `zr_vm_core/include/zr_vm_core/io.h:440` 触发语法错误，和本次 thread runtime 修改无直接关系。

## Test Inventory

### Focused Thread Cases
- `test_zr_thread_registers_public_shapes_without_legacy_mutex_or_atomic`
  - 验证公开模块形状没有回退到旧并发 API。
- `test_spawn_thread_requires_support_multithread`
  - 负例；验证 `supportMultithread = false` 门禁。
- `test_async_runner_creation_still_works_with_thread_import_present`
  - 验证 task/coroutine builtin 与 `zr.thread` 并存不破坏 `%async`。
- `test_thread_start_with_precomputed_runner_execute_runner_result`
  - 验证冷 `TaskRunner<T>` 由 thread scheduler 显式启动。
- `test_thread_start_and_await_execute_runner_result`
  - 验证 worker task completion 回到 awaiter 所在 scheduler。
- `test_channel_transports_value_back_from_worker_isolate`
  - 典型跨 isolate transport：`Channel<T>`.
- `test_transfer_moves_value_into_worker_isolate_and_invalidates_source`
  - 典型 move-only transport：`Transfer<T>`.
- `test_shared_handle_capture_roundtrips_across_worker_isolate`
  - 典型共享句柄：`Shared<T>`.
- `test_weak_shared_handle_capture_upgrades_across_worker_isolate`
  - 典型弱句柄：`WeakShared<T>`.

### Tool-Assisted Cases
- Helgrind on `build/codex-wsl-gcc-debug/bin/zr_vm_thread_runtime_test`
  - 聚合验证 worker spawn、scheduler wakeup、shared handle retain/release、channel transport。

## Tooling Evidence

### Tool
- `valgrind-3.22.0`
- 选择原因：用户要求使用 Helgrind 验证线程安全；该工具适合检查竞态、锁使用和跨线程同步问题。

### Commands
- Baseline plain run:
```bash
cd /mnt/d/Git/Github/zr_vm_mig/zr_vm
./build/codex-wsl-gcc-debug/bin/zr_vm_thread_runtime_test
```
- Initial failing Helgrind run:
```bash
cd /mnt/d/Git/Github/zr_vm_mig/zr_vm
valgrind --tool=helgrind --error-exitcode=99 --fair-sched=yes --history-level=approx \
  ./build/codex-wsl-gcc-debug/bin/zr_vm_thread_runtime_test
```
- Final passing Helgrind run:
```bash
cd /mnt/d/Git/Github/zr_vm_mig/zr_vm
valgrind --tool=helgrind --error-exitcode=99 --fair-sched=yes --history-level=approx \
  ./build/codex-wsl-gcc-debug/bin/zr_vm_thread_runtime_test
```
- Post-fix toolchain regressions:
```bash
cd /mnt/d/Git/Github/zr_vm_mig/zr_vm
./build/codex-wsl-gcc-debug/bin/zr_vm_thread_runtime_test
./build/codex-wsl-clang-debug/bin/zr_vm_thread_runtime_test
```

### Key Observed Outputs
- Initial Helgrind failure:
```text
Thread #1: pthread_cond_{signal,broadcast}: associated lock is not held by calling thread
...
zr_vm_task_sync_condition_wait (runtime_internal.h:145)
zr_vm_task_scheduler_wait_for_external (runtime.c:474)
...
ERROR SUMMARY: 5 errors from 1 contexts
```
- Final Helgrind result:
```text
ERROR SUMMARY: 0 errors from 0 contexts
```
- Focused thread suite result:
```text
9 Tests 0 Failures 0 Ignored
OK
```

## Results
- 修复内容：
  - 将 Linux/Windows 共用的 scheduler external wait 从 condvar timed wait 改成短时 polling sleep + queue recheck。
  - 改动文件：
    - `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h`
    - `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c`
- 验证结果：
  - WSL gcc focused thread suite：通过
  - WSL clang focused thread suite：通过
  - WSL Helgrind on gcc binary：0 errors
  - Windows 已存在的 `zr_vm_thread_runtime_test.exe` 运行：通过
  - Windows 重新构建 `build/codex-msvc-debug`：被 `io.h` 既有语法错误阻断，未能作为本次线程改动的接受门

## Acceptance Decision
- Accepted for WSL/Linux thread-safety validation.
- 理由：
  - 当前 thread-focused regression matrix 全通过。
  - Helgrind 对目标二进制给出 `0 errors from 0 contexts`。
  - 覆盖了典型的 worker launch、await continuation、Channel、Transfer、Shared、WeakShared 场景。
- Remaining risks / baseline blockers:
  - 这次 Helgrind 验证聚焦于 `tests/thread/test_thread_runtime.c` 覆盖到的路径，不代表整个仓库的全量 `ctest` 已经完成线程工具清零。
  - `build/codex-msvc-debug` 的重新构建仍被 `zr_vm_core/include/zr_vm_core/io.h:440` 既有编译问题阻断，需要单独处理。
