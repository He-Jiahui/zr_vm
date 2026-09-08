---
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/debug/07-testing-and-acceptance.md
tests:
  - tests/library/test_debug_library.c
  - tests/debug/test_debug_traceback.c
  - tests/debug/test_debug_introspection.c
  - tests/testing/test_assertions.c
  - tests/testing/test_runner.c
  - tests/artifact/test_manifest_roundtrip.c
doc_type: module-detail
---

# `zr.debug` 与 `zr.testing`

`zr.debug` 当前为 `experimental`、按宿主权限启用，descriptor 版本 `1.0.0`，contract
`zr.debug:v1:lua-aligned-debug-surface`。`zr.testing` 为 Test phase provider，版本 `1.0.0`，
contract `zr.testing:v2:typed-manifest-structured-assertions`；它不进入 Production host。

## Debug provider

显式注册后，`zr.debug` 导出 `traceback`、`getinfo`、`getlocal`、`setlocal`、`getupvalue`、
`setupvalue`、`upvalueid`、`sethook`、`gethook`。`getlocal/getupvalue` 返回 `{name, value}`
对象，避免 native binding 的多返回值歧义。hook mask 支持 `c/r/l` 和 count，内核 hook
重入保护阻止脚本 hook 无限递归。

provider 有 trusted 和 sandboxed 两个 descriptor。sandboxed 版本保留读 API，拒绝
`setlocal/setupvalue/sethook`；未显式注册时 `import("zr.debug")` 不存在。hook record 保存
GC-traced 函数值，不保存裸 slot。

| Debug 函数 | 签名 | trusted/sandboxed |
| --- | --- | --- |
| `traceback` | `traceback(message?: string, level?: int): string` | 读/读 |
| `getinfo` | `getinfo(levelOrFunction, what?: string): object` | 读/读 |
| `getlocal` | `getlocal(level: int, index: int): object` | 读/读 |
| `setlocal` | `setlocal(level: int, index: int, value): string` | 写/拒绝 |
| `getupvalue` | `getupvalue(func: function, index: int): object` | 读/读 |
| `setupvalue` | `setupvalue(func: function, index: int, value): string` | 写/拒绝 |
| `upvalueid` | `upvalueid(func: function, index: int): nativePointer` | 读/读 |
| `sethook` | `sethook(hook?: function, mask?: string\|int, count?: int): null` | 写/拒绝 |
| `gethook` | `gethook(): object` | 读/读 |

## Testing provider

测试声明使用结构化 metadata：

```zr
let testing = import("zr.testing");

#zr.testing.test#
#zr.testing.case(1)#
fn adds(value: int): void {
    testing.assert(value > 0);
}
```

metadata role 使用 `#` 成对包围，而不是注解语言常见的 `@` 前缀；role 必须紧邻被标注
的函数。`#zr.testing.case(...)#` 可以重复出现，参数在 Test phase 必须是编译期可序列化
的值，并且与测试函数形参数量和类型一致。

公开断言为 `assert`、泛型 `equal<T>` 和 `throws<E>`。失败记录 assertion kind、source span、
bounded type/value snapshot 和原始 exception；格式化故障不能覆盖断言本身。编译器在 Test
phase 生成 versioned `TestManifest`，包含 canonical SymbolId/TypeId、case 参数、async 位、
skip reason 和 module signature。Production 编译会 type-check 后裁剪 test roots，不生成
manifest，也不生成隐藏 main。

断言函数的实际 arity 是：`assert(condition: bool, message?: string): void`、
`equal<T>(actual: in T, expected: in T): void`、`throws<E>(action: fn() -> void): E`。
`E` 必须满足 `Error` 约束；`throws` 只接受同步 action，不会隐式等待 Task。

## C API

```c
ZrVmLibDebug_Register(global);
ZrVmLibDebug_RegisterSandboxed(global);
ZrVmLibTesting_Register(global);
ZrVmLibTesting_ClearLastFailure();
ZrVmLibTesting_GetLastFailure(&failure);
```

`SZrTestingAssertionFailure` 的消息和 snapshot 有固定上限，调用方需检查 `truncated`。
测试 runner 应在独立 state/进程中执行 case，并把 timeout 映射为结构化终止状态。
