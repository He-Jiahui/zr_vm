---
related_code:
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/cli-and-tooling/zr-vm-cli-command-system.md
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_project_incremental.c
  - tests/cli/test_cli_zrp_metadata_dump.c
  - tests/cmake/run_cli_suite.cmake
doc_type: tool-reference
---

# CLI 完整命令参考

`zr_vm_cli` 是当前仓库的命令行入口。命令行首先由 `ZrCli_Command_Parse` 归一化为
`SZrCliCommand`，只有通过组合校验后才创建 project/global/state 或触发 compiler。因此
命令误用不会作为“程序运行失败”进入 VM；脚本和 CI 应以 exit code 与结构化 metadata 输出
为依据，不应解析帮助文本的排版。

本页是 [CLI/REPL 概览](cli.md) 的可查版本。`.zrp`、module resolver 与输出产物请结合
[项目、文件与 ZRM API](../05-interop/project-file-zrm-api.md) 和 [产物与格式](../06-reference/artifacts.md) 阅读。

## 1. 命令模型

`SZrCliCommand.mode` 是解析器唯一的顶层分发结果：

| mode | 常见输入 | 是否创建运行路径 |
| --- | --- | --- |
| `HELP` | `--help`、`-h`、`-?` | 否 |
| `VERSION` | `--version`、`-V` | 否 |
| `REPL` | 无项目/inline 输入 | 交互式按提交创建 |
| `RUN_PROJECT` | `app.zrp` | 是 |
| `COMPILE_PROJECT` | `--compile app.zrp` | 只编译，除非带 `--run` |
| `RUN_INLINE` | `-e code` 或 `-c code` | 是 |
| `RUN_PROJECT_MODULE` | `--project app.zrp -m path` | 是 |
| metadata modes | `--dump-zrp-metadata` 等 | 否 |
| `MIGRATE_SYNTAX` | `migrate syntax ...` | 否 |
| `TEST` | `test target` | Test phase，不是普通 run |

Parser 先给结构体写入默认值：execution mode 为 `interp`、test jobs 为 `1`、test timeout
为 `0`（无显式 timeout）。后续遇到 `--` 时，剩余 argv 不再被解析为 CLI flag，而是借用
为 `programArgs` 与 `programArgCount`。这意味着 shell 传参必须把 VM 选项放在 `--` 之前：

```text
zr_vm_cli app.zrp -- --input file.zr --verbose
```

## 2. 基本命令语法

```text
zr_vm_cli [project.zrp] [run-options] [-- program-args...]
zr_vm_cli --compile project.zrp [compile-options] [--run] [run-options]
zr_vm_cli --project project.zrp -m module.name [run-options] [-- program-args...]
zr_vm_cli -e source-text [run-options]
zr_vm_cli test target [--filter glob] [--list] [--jobs n] [--timeout duration]
zr_vm_cli migrate syntax path (--check | --write) [migration-options]
```

`target` 在 test command 中可按实现识别为 project 或单一 source 路径；真正的 TestManifest
发现、Test provider 注册和子进程隔离在 [TestManifest 与 Runner](test-manifest-runner-reference.md)
中说明。

## 3. 运行与编译选项

### 3.1 项目和执行模式

| 选项 | `SZrCliCommand` 字段 | 规则 |
| --- | --- | --- |
| `project.zrp` | `projectPath` | 选择按 `.zrp.entry` 的项目运行 |
| `--compile project.zrp` | `projectPath`、compile mode | 进入项目编译路径 |
| `--run` | `runAfterCompile` | 仅与 `--compile` 一起有效；默认把该路径切到 binary 执行 |
| `--project project.zrp -m name` | `projectPath`、`moduleName` | `-m` 必须配合 `--project`，不能同 `--compile` 混用 |
| `-e code` / `-c code` | `inlineCode`、`inlineModeAlias` | bare global inline path |
| `--execution-mode interp|binary` | `executionMode` | 仅 active run path 有意义 |
| `--emit-executed-via` | `emitExecutedVia` | 运行路径的可观测输出 |
| `-i` / `--interactive` | `interactiveAfterRun` | 运行结束后进入 REPL；不适用于 compile-only/help/version |

例子：

```text
zr_vm_cli samples/hello.zrp
zr_vm_cli --compile samples/hello.zrp --intermediate --emit-zrm
zr_vm_cli --compile samples/hello.zrp --run --execution-mode binary
zr_vm_cli --project samples/tools.zrp -m tools.seed -- --seed 42
zr_vm_cli -e "return 1 + 2;"
```

`--execution-mode binary` 不是“跳过编译器”。它只选择已有 active run path 的执行方式；
项目路径仍须完成 module resolver、artifact identity 和 runtime/provider admission。inline
模式不接受 project compile、debug、profile、coverage、bytecode dump、heap summary 等运行
修饰组合。

### 3.2 编译输出与增量

| 选项 | 作用 | 组合限制 |
| --- | --- | --- |
| `--intermediate` | 请求中间 `.zri` 输出 | 需要 `--compile` |
| `--emit-zrm` | 请求 `.zrm` package 输出 | 需要 `--compile` |
| `--emit-aot-c` | 请求 AOT C 输出 | 需要 `--compile` |
| `--incremental` | 使用项目增量 manifest | 需要 `--compile` |
| `--run` | 产物生成后运行 | 需要 `--compile` |

增量 manifest 记录 module 输入及依赖关系，供下一次构建判断可复用性。它是缓存实现细节，
不是手工可编辑的稳定 configuration API；一旦 source/import/provider/artifact identity
不匹配，编译器应重新计算或拒绝 stale binary，而不是信任旧字节。

### 3.3 调试和观测

| 选项 | 字段 | 约束 |
| --- | --- | --- |
| `--debug` | `debugEnabled` | 打开调试运行路径 |
| `--debug-address host:port` | `debugAddress` | 需要 `--debug` |
| `--debug-wait` | `debugWait` | 需要 `--debug` |
| `--debug-print-endpoint` | `debugPrintEndpoint` | 需要 `--debug` |
| `--profile path` | `profileEnabled`、`profileOutputPath` | 不能与 `--debug` 或 coverage 并用 |
| `--coverage path` | `coverageEnabled`、`coverageOutputPath` | 不能与 `--debug` 或 profile 并用 |
| `--dump-bytecode path` | `dumpBytecodeEnabled`、路径字段 | 需要 active run path |
| `--heap-summary path` | `heapSummaryEnabled`、路径字段 | 需要 active run path |

因此下面的命令在 parse 阶段被拒绝，而不是“运行时偶然不生效”：

```text
zr_vm_cli app.zrp --debug --profile profile.json
zr_vm_cli --compile app.zrp --coverage coverage.json
zr_vm_cli --help --debug
```

第一条违反 debug/profile 互斥；第二条没有 active run path；第三条违反 help 的独占性。
工具封装层应先调用 `ZrCli_Command_Parse`，将 error buffer 原样呈现给调用者，而不是自行
复刻这些组合规则。

## 4. REPL 与 inline

无显式目标时 CLI 进入 REPL。`:help`、`:quit`、`:reset`、`:type <expression>` 是 REPL
命令；空行可以提交多行缓冲。成功声明的 source prefix 会在下一次提交时重新编译，以便名称
可见，但这不表示每次提交共享同一个 runtime object 或 native handle。

`:type` 是静态查询，不执行目标 expression；它可报告 Type、constant、数值 range、flow、
ownership/reference 和 call facts。普通裸 expression 会被 REPL 包装为 return form，以显示
结果；声明、控制流和显式 `return` 不做这种包装。

```text
zr_vm_cli
> let limit: int = 8;
> :type limit + 1
> limit + 1
```

若编写脚本自动化，优先 `-e`/`-c` 或项目命令，而不是依赖 REPL 提示符和人类可读输出。

## 5. Project metadata 与迁移

### 5.1 `.zrp` metadata

| 命令 | 输入 | 结果 |
| --- | --- | --- |
| `--dump-zrp-metadata path` | 一个 manifest | 结构化 metadata 输出 |
| `--diff-zrp-metadata before after` | 两个 manifest | metadata 差异 |
| `--check-zrp-metadata-version path` | 一个 manifest | 版本兼容性检查 |

这些 mode 与 run/compile/debug/output modifier 互斥。自动化消费者应读取 JSON 字段，不要
依赖标准输出中的标题或空格。manifest schema、resolver 路径与锁定 identity 的含义见
[项目、文件与 ZRM API](../05-interop/project-file-zrm-api.md)。

### 5.2 语法迁移

```text
zr_vm_cli migrate syntax path --check
zr_vm_cli migrate syntax path --write --format json
zr_vm_cli migrate syntax path --write --include-generated
```

`migrate syntax` 必须带精确子命令和二选一的 `--check` / `--write`。可选的 `--format`
接受 JSON/text 表示；`--language-from legacy` 与 `--language-to current` 是当前允许的
语言端点。`--check` 只生成建议，`--write` 在重新验证之后才更新文件。不要把 migration
模式当作 production parser 的“继续接受旧语法”开关：它的职责是检测与改写，而不是扩大
正常源码 grammar。

## 6. Test 命令

```text
zr_vm_cli test tests/fixtures/projects/testing_reference/testing_reference.zrp
zr_vm_cli test tests/fixtures/projects/testing_reference/testing_reference.zrp --list
zr_vm_cli test tests/fixtures/projects/testing_reference/testing_reference.zrp --filter "*orderedPair*" --jobs 2 --timeout 5s
```

`--filter` 接收 case-id glob；`--jobs` 必须是正整数；`--timeout` 必须带 `ms`、`s` 或 `m`
单位。test-only option 不能附带普通运行/项目/compile modifier 或 `--` 程序参数。case id、
manifest schema、skip/async/timeout 和 C runner callback 的细节见 [TestManifest 与 Runner](test-manifest-runner-reference.md)。

## 7. Exit code

| code | 意义 | 自动化处理建议 |
| --- | --- | --- |
| `0` | 命令成功；test path 表示全部 pass 或 skip | 继续 |
| `1` | 用户程序失败、断言失败或 test timeout | 报告业务/测试失败 |
| `2` | CLI 参数、模式组合、filter 或 duration 误用 | 修复调用参数，不重试 VM |
| `3` | runner、VM、isolate 或基础设施失败 | 收集日志，按基础设施故障处理 |

test runner 还会单独记录 `Passed`、`Failed`、`Skipped`、`TimedOut`、`Crashed`；其中 crash
会提升为 code 3，fail/timeout 为 code 1。不要用“非零都等于 test failed”抹掉这一区别。

## 8. 嵌入式 C 调用

CLI library 使用结构化 command，而不是让宿主拼接 shell 字符串：

```c
#include "zr_vm_cli/command/command.h"

SZrCliCommand command;
TZrChar error[512];

if (!ZrCli_Command_Parse(argc, argv, &command, error, sizeof(error))) {
    /* error describes invalid CLI grammar or mode combination. */
    return 2;
}

switch (command.mode) {
    case ZR_CLI_MODE_COMPILE_PROJECT:
        /* Dispatch through the CLI app/compiler layer. */
        break;
    case ZR_CLI_MODE_RUN_PROJECT:
        /* Dispatch through the CLI app/runtime layer. */
        break;
    default:
        break;
}
```

`SZrCliCommand` 的字符串与 `programArgs` 均借用 argv 存储；在执行期间 argv 内容必须保持
有效。`ZrCli_Command_Parse` 不创建 state，也不拥有/释放 argv。嵌入式宿主若要直接创建
global/state、加载项目或注册 native provider，应走 [C 宿主集成指南](../05-interop/c-host-guide.md)，
不要把 CLI 的私有 app dispatch 当作稳定 public runtime ABI。
