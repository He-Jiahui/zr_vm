---
related_code:
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
  - zr_vm_cli/src/zr_vm_cli/commands/test_command.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.h
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_process.h
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/commands/test_command.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_process.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/plans/debug/07-testing-and-acceptance.md
tests:
  - tests/testing/test_test_role_binding.c
  - tests/testing/test_assertions.c
  - tests/testing/test_runner.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/cli/test_cli_args.c
  - tests/cmake/run_testing_reference.cmake
doc_type: tool-reference
---

# TestManifest 与 Test Runner 参考

ZR 测试不是“扫描名字以 `test` 开头的函数”。编译器从 `zr.testing` 的 structured role
构造 typed `SZrParserTestManifest`，CLI 再将 manifest entry 展开为稳定 case id、筛选、
模块分组、隔离执行和结果汇总。这让 parser、artifact、CLI、LSP/DAP 和 native test provider
共享同一份测试身份，而不是各自猜测源码文本。

本页描述当前 Test phase 的语法、manifest ABI、runner 调度和 C 扩展点。CLI 参数总览见
[CLI 完整命令参考](cli-command-reference.md)，断言库 surface 见 [Debug/Testing API](../03-modules/debug-testing-api.md)。

## 1. 源码标注

测试函数先导入 provider，再使用 `#zr.testing.*#` decorator：

```zr
let testing = import("zr.testing");
let task = import("zr.task");

#zr.testing.test#
fn synchronousPass(): void {
    testing.assert(true);
}

#zr.testing.test#
#zr.testing.case(1, 2)#
#zr.testing.case(-1, 1)#
fn orderedPair(left: int, right: int): void {
    testing.assert(left < right);
}

#zr.testing.test#
#zr.testing.skip(reason: "reference skip")#
fn skippedCase(): void {
    testing.assert(false);
}

#zr.testing.test#
async fn asynchronousPass(): task.Task<void> {
    testing.assert(true);
    return task.yieldNow();
}
```

当前 canonical role 名称为：

| decorator | qualified role | 作用 |
| --- | --- | --- |
| `#zr.testing.test#` | `zr.testing.test` | 将 function 作为 test entry |
| `#zr.testing.case(...)#` | `zr.testing.case` | 声明一个 compile-time constant 参数 case |
| `#zr.testing.skip(reason: ...)#` | `zr.testing.skip` | 保留 entry，但标为不执行 |

`case` argument 必须能编码为 manifest constant，不是任意 runtime expression。当前
`EZrParserTestConstantKind` 支持 `null`、bool、signed int、unsigned int、float、string。
`skip` reason 必须是非空可保存文本。普通 production phase 仍会 type-check test function，
但会裁剪 test root、不写 TestManifest，也不注入隐藏 test main。

## 2. 从声明到 Manifest

```text
#zr.testing.test# function
    -> semantic role binding
    -> Test phase compiler
    -> SZrParserTestManifest
    -> encoded artifact bytes
    -> CLI discovery / LSP / test runner
```

manifest header 由以下事实组成：

| 字段 | 含义 |
| --- | --- |
| `schemaVersion` | 当前 `ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION`，值为 1 |
| `targetTriple` | 生成 target 的身份 |
| `moduleGraphHash` | 编译时的可达 module graph 身份 |
| `entries` / `entryCount` | 结构化 test function 列表 |

每个 `SZrParserTestEntry` 包含：

| 字段 | 用途 |
| --- | --- |
| `functionSymbolId` / `functionTypeId` | canonical function identity，不靠显示名称解析 |
| `callableChildIndex` | 编译后 callable 定位 |
| `moduleId` / `qualifiedName` | 稳定的人类可读定位 |
| `sourceRange` | 编辑器和报告定位 |
| `isAsync` | runner 是否按 Task contract 等待 |
| `skipReason` | 可观察 skip 原因 |
| `cases` / `caseCount` | 固定实参的 case descriptors |

未声明 `#case` 的 test entry 仍产生一个默认 case；有 case 时，每个
`SZrParserTestCaseDescriptor` 包含 `ordinal`、常量 `arguments` 和 `argumentCount`。parser
限制包含：最多 100000 entry、每 entry 最多 10000 cases、每 case 最多 256 arguments、
单个字符串最多 65536 bytes。超出限制不是可接受的“长测试列表”，而是 manifest validation
失败边界。

## 3. 编码与 C API

manifest 是 versioned bytes，不要手写其二进制布局。公共 API 负责 encode、decode、validate
和 owner-consistent free：

```c
#include "zr_vm_parser/test_contract.h"

TZrByte *bytes = ZR_NULL;
TZrUInt32 byteCount = 0U;
SZrParserTestManifest decoded = {0};

if (!ZrParser_TestManifest_Encode(state, &manifest, &bytes, &byteCount)) {
    /* Compiler/allocator error. */
}

if (bytes != ZR_NULL &&
    ZrParser_TestManifest_Validate(state, bytes, byteCount) &&
    ZrParser_TestManifest_Decode(state, bytes, byteCount, &decoded)) {
    /* Consume decoded entries while state is valid. */
    ZrParser_TestManifest_Free(state, &decoded);
}
```

由 `Encode` 返回的 byte buffer 与 manifest 内部字符串/array 的释放策略以当前公共头文件和
调用产物 API 为准。不要对 `decoded.entries`、`moduleId`、`skipReason` 或 case arguments
做单独的 `free()`，也不要以 `schemaVersion == 1` 为理由跳过 `Validate`；长度、计数、
UTF-8/constant kind 和 artifact provenance 都是输入边界的一部分。

## 4. Case ID、筛选和确定性

runner 按下列概念格式化 id：

```text
module::qualifiedName#ordinal(arguments)
```

它先收集所有 manifest entry，把每个 entry 展开为 default case 或显式 case，按 id 排序，
再根据排序后的 id 计算 run seed。因此同一批 manifest 产生稳定的 list 顺序、filter 行为和
seed；不要依赖 filesystem enumeration 或 source file 顺序。

```text
zr_vm_cli test app.zrp --list
zr_vm_cli test app.zrp --filter "*::orderedPair#*"
```

`--filter` 是 glob-like pattern；runner 的 C options 还支持 `exactCaseId`，用于 worker
进程执行唯一 case。`--list` 或无匹配 case 不执行 worker，结果 `jobsUsed` 为零；调用者可
据此区分“列出/无选择”与真正执行。

## 5. 并行、隔离和 timeout

Runner 不会把同一 module 的 case 无序并发。它在排序结果中形成 module group：同一
`moduleId` 的 case 是一个 group，不同 group 可最多使用 `--jobs` 个 worker。这样既保持
module 内可预测性，又避免把全部项目强制串行。

```text
sorted cases
  -> consecutive module groups
  -> min(jobs, groupCount) workers
  -> each worker executes one group at a time
  -> merge counters in sorted result order
```

CLI test command 以 fresh global/child process 执行选中的 case，并通过环境变量
`ZR_VM_TEST_CASE_ID` 指向 exact case。`SZrCliTestProcessRequest` 包含 executable path、
target path、case id、timeout milliseconds；`SZrCliTestProcessResult` 回收 exit code、duration、
timeout bit 与有界 output。超时不是 assertion failure 的另一种文本：runner 应先记录
`TimedOut`，而无法建立/维持 worker 的情况则是 `Crashed` 或 infrastructure failure。

## 6. 状态与 exit code

`EZrCliTestStatus` 的状态名是 `Passed`、`Failed`、`Skipped`、`TimedOut`、`Crashed`。结果
对象还记录 executed bit、duration、message、bounded output、各类计数、`jobsUsed` 与 seed。

| 状态 | case 是否实际执行 | 命令结果 |
| --- | --- | --- |
| `Passed` | 是 | 可成功 |
| `Skipped` | 否 | 可成功 |
| `Failed` | 是 | exit 1 |
| `TimedOut` | 可能被终止 | exit 1 |
| `Crashed` | worker/child 无法正常完成 | exit 3 |

`ZrCli_TestRunner_ExitCode` 对空 result 返回 3；对 crash 计数返回 3；对 fail 或 timeout
返回 1；其余为 0。CLI parse/filter/timeout unit 的错误仍是 exit 2，不能和上述执行结果
混为一谈。

## 7. Native testing provider 与 phase

test command 创建 global 后注册 `ZrVmLibTesting_Register(global)`，再将 state provider phase
设置为 `ZR_LIBRARY_PROVIDER_PHASE_TEST`。这一步是 capability boundary：`zr.testing` 不应当
仅凭源码 import 在普通 Runtime phase materialize。测试 provider 的 assertions 会把失败
归一为 `SZrTestingAssertionFailure`，保留 source/context，并与 runtime exception/child process
错误区分。

native module author 不应伪造 `zr.testing` 的官方 module identity 或绕过 registry phase。
若要提供自己的测试辅助库，使用独立的 descriptor、明确的 provider phase/capability、稳定
contract hash，并在 test host 中显式注册。详见 [Native Module 编写](../05-interop/native-module-authoring.md)
和 [Native Provider Descriptor 深度参考](../03-modules/native-provider-descriptor-reference.md)。

## 8. 嵌入式 C runner

最小 runner contract 接受已解码 manifest、options 和每 case 的 executor：

```c
SZrCliTestRunnerOptions options = {
    .filterPattern = "*",
    .exactCaseId = ZR_NULL,
    .jobs = 2U,
    .timeoutMilliseconds = 5000U,
    .listOnly = ZR_FALSE,
};
SZrCliTestRunResult result = {0};

if (ZrCli_TestRunner_Run(manifests,
                         manifestCount,
                         &options,
                         execute_case,
                         userData,
                         &result)) {
    int exitCode = ZrCli_TestRunner_ExitCode(&result);
    ZrCli_TestRunner_Free(&result);
    return exitCode;
}
return 3;
```

`FZrCliTestCaseExecutor` 接收一个 `SZrCliTestCaseReference`、timeout、可写 result 和 opaque
user data。executor 必须完整设置 result 的状态、message/output/duration，并尊重 reference
指向的 manifest lifetime；它不能把 `entry`、`testCase` 或 result 内部字符数组留给一个在
`ZrCli_TestRunner_Free` 后仍运行的线程。对 runtime 执行、GC root、Task await 和 native
exception 的具体调用规则，请复用 Core/Task 的 C API，不要把 runner callback 误当成可绕开
VM 生命周期的线程回调。
