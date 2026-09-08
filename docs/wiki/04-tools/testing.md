---
related_code:
  - zr_vm_cli/src/zr_vm_cli/commands/test_command.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_process.c
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/commands/test_command.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_process.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/debug/07-testing-and-acceptance.md
tests:
  - tests/testing/test_test_role_binding.c
  - tests/testing/test_assertions.c
  - tests/testing/test_runner.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/cli/test_cli_args.c
  - tests/cmake/run_testing_reference.cmake
doc_type: module-detail
---

# 测试命令与 TestManifest

## 声明和编译

测试函数是普通 ZR `fn` 加结构化 `zr.testing.test` role；`case` 参数必须是 compile-time
constant，`skip` reason 必须非空。Test phase 编译得到 versioned `TestManifest`，每个 entry
保存 canonical SymbolId/TypeId、module-qualified name、callable child index、source range、
async bit、skip reason 和常量参数。Production phase 仍 type-check 测试声明，但裁剪 root、
不输出 manifest，也不生成隐藏 main。

## CLI

```text
zr_vm_cli test app.zrp [--filter <glob>] [--list]
                     [--jobs <n>] [--timeout <milliseconds>]
```

case id 固定为 `module::qualifiedName#ordinal(arguments)`，用于 list、glob、随机 seed、报告
和 worker 选择。模块内 case 串行，不同模块组最多并行 `--jobs`。每个选中 case 在 fresh
global/child process 中执行，stdout/stderr 有界捕获，timeout 依次使用终止和 kill fallback。

退出码：0 表示全部通过或跳过；1 表示断言失败/timeout；2 表示命令或 filter 误用；3 表示
runner/isolate 失败。报告必须区分 pass、fail、skip、timeout、crash，不能把断言异常当作进程
崩溃。

## 断言和错误

`assert(condition, message?)`、`equal<T>(actual, expected)`、`throws<E>(fn)` 通过 native
binding 写入 `SZrTestingAssertionFailure`。snapshot 有固定容量和 `truncated` 标志；格式化
器抛错时保留原 assertion kind/source span。async 测试返回 `zr.task.Task<void>`，runner 通过
canonical Task contract 等待。
