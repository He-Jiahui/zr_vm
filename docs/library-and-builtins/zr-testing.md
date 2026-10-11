---
related_code:
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
implementation_files:
  - zr_vm_lib_testing/src/zr_vm_lib_testing/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/2026-07-20-14-test-function-harness-design.md
tests:
  - tests/testing/test_test_role_binding.c
  - tests/testing/test_assertions.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/fixtures/projects/testing_reference
  - tests/testing/test_runner.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/debug/test_debug_agent_protocol.c
  - tests/migration/test_percent_test_migration.c
doc_type: module-detail
---

# zr.testing Test-Phase Provider

## Public Contract

`zr.testing` is the official N3 Test-phase provider. Runtime hosts cannot
consume it. A test host may consume Runtime and Test providers, but never
CompileTool providers. Its descriptor owns three retained metadata roles:

- `zr.testing.test` marks an ordinary top-level `fn` as a test root.
- repeatable `zr.testing.case` supplies compile-time constant arguments.
- `zr.testing.skip` retains one non-empty skip reason.

The callable surface is `assert`, generic `equal<T>`, and synchronous
`throws<E>`. The compiler binds the public `throws<E>(fn() -> void)` call and
passes canonical `E` TypeId metadata through a compiler-hidden native argument;
the runtime accepts only the exact exception type or an allowed subtype.
Failures are bounded structured `AssertionFailure` values rather than
process-global text matching. They carry the caller source span and bounded
type/value snapshots. In `equal`, a value formatter fault sets the snapshot's
`formatterFaulted` flag without replacing the comparison failure. The `throws`
path snapshots a caught exception before matching its type; whether a formatter
fault in that snapshot can clear the caught exception still needs a targeted
test, as recorded by the `TODO:` in `runtime/assertions.c`. The provider
descriptor, phase, role ids, and public contract hash participate in the
official-provider inventory.

The current `testing_copy_bounded` helper truncates these diagnostic strings by
byte count. A multibyte UTF-8 character crossing the limit can leave an invalid
trailing byte sequence in the copied failure message or snapshot; see its `BUG:`
review note in `runtime/assertions.c`. The existing assertion tests do not cover
this boundary case.

## 宿主 C 接口与失败副本

公共头部的八个 C 接口保持原签名。查询描述符、登记 VM、调用断言和读取诊断副本有各自的使用窗口。

| 入口 | 调用目的 | 使用边界 |
| --- | --- | --- |
| GetModuleDescriptor | 核查静态 Test provider 签名、属性和 phase。 | 无需 VM；返回 const 静态借用表，provider 须活到最后一次消费。 |
| Register | 登记 zr.task、按需附着 native registry，再登记 zr.testing。 | 有效 global/mainThread；不自动切 Test phase，失败保留前序登记，不回滚。 |
| ClearLastFailure | 在当前 OS 线程开始或结束诊断窗口。 | 不清 VM currentException，也不清其它线程记录；已复制值仍有效。 |
| GetLastFailure | 按值复制当前 OS 线程的最近失败。 | out 非空且有记录才成功；false 时不改输出，不消费记录，不按 GlobalState 分区。 |
| Assert | 把 false 条件转成结构化失败与 VM 异常。 | 正常 native context；一或两个实参，成功写 null 并清 TLS；失败由 VM 异常边界处理。 |
| Equal | 先作规范相等比较，失败后制作 actual/expected 诊断。 | 两实参；格式化故障不是相等性结论；有界文本仍有按字节截断 BUG。 |
| Throws | 同步捕获 action 异常并匹配 Error 派生 E。 | public 一实参/native 两实参含隐藏 TypeId；result 沿 VM 初始化/复制协议，不按诊断结构处理。 |
| ZrVm_GetNativeModule_v1 | 共享 provider 的固定描述符查询符号。 | 仅共享构建；查询不执行宿主 Register 依赖链；加载器先查 ABI/登记，后续解析或物化时查 phase。 |

诊断结构全部内嵌字符数组与普通值，不保活 VM 对象，宿主可保存成功读取的副本。
TLS 以 OS 线程为单位，同一线程的多个 GlobalState 或重入用例共享它；宿主应先读取再清理。
`hasValue` 表示视图有生产记录，合法 null 值仍可为真，Throws 的 expected 还可能只有 typeName。
`truncated` 只报告正常 snapshot.text 的裁剪，message/sourceFile/typeName 没有独立标志。
没有可用 VM 帧/调试元数据时 source span 清零；有源码名称但没有匹配位置记录时可以只有 sourceFile 非空。
TODO: endColumn 的包含/排除语义仍须沿执行位置生产者核对，不能仅凭字段名推定字符串切片规则；
具体从 `zr_vm_core/src/zr_vm_core/io_runtime.c:695` 的 instructionRanges 复制继续追踪。

原生回调 bool 与宿主 `ZrLib_CallModuleExport` 的 bool 含义不同：回调 false 且 threadStatus 正常时，
派发层把结果置 null，宿主仍可返回 true。非法 E 的用例验证 action 未执行和 null 结果；
宿主 true 只说明调用完成且没有 VM 异常，不能作为断言已经执行的证明。

当前证据为 `zr_vm_cli/src/zr_vm_cli/commands/test_command.c:551`、`:570`、`:592` 的诊断窗口，
`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:375`、`:380`、`:383` 的 TLS 边界，
`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:136`、`:513` 的 hasValue 生产者，
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619`、`:1997` 的两层返回路径，
及 `tests/testing/test_assertions.c:337` 的非法 E 调用。
本节只更新公共头部契约；内部 assertions.c 的完整文件单元和文档格式仍待重核，不据此授予整个模块或运行时验收。

## TestManifest

Test compilation type-checks ordinary functions and emits a versioned binary
`TestManifest`. Each entry carries the bound function's canonical semantic
SymbolId/TypeId, callable child index, module-qualified name, source range,
async bit, skip reason, and constant case arguments. The manifest carries the
compiler's module signature identity rather than a name-derived surrogate.
Decode rejects unknown schema versions, oversized counts, invalid booleans,
truncation, trailing bytes, and overlong strings. Cleanup is valid for partially
decoded input, including allocation failure and corrupt-count paths.

Production compilation still type-checks test declarations, then trims their
roots and emits no manifest. A production declaration cannot call a trimmed
test function. There is no hidden `main`, source-text test discovery, or old
`%test` AST/lowering path.

## Async And Isolation Boundary

Synchronous tests return `void`. Async tests explicitly return
`zr.task.Task<void>`; the host awaits the result through the canonical task
contract. Test execution happens in a fresh global state, and the CLI process
runner can place each selected case in a child process with timeout and output
capture.

## Tooling Projection

LSP run/debug/case CodeLens is derived from the compiled `TestManifest`; an AST
decorator that fails test-role binding cannot produce a lens. The debugger
decodes the same manifest and projects the active test's canonical symbol,
type, module, case, and qualified name onto stack frames. Parameter values stay
ordinary Arguments-scope values, and async tests retain the scheduler's logical
stack contract. Legacy `%test`, draft `test fn`, and `#zr.test.*#` are accepted
only by migration or negative fixtures, never by the production parser.
