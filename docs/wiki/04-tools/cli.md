---
related_code:
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/cli-and-tooling/zr-vm-cli-command-system.md
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_project_incremental.c
  - tests/cli/test_cli_zrm_fixture.c
  - tests/cmake/run_cli_suite.cmake
doc_type: module-detail
---

# `zr_vm_cli`

## 模式

`SZrCliCommand.mode` 是唯一分发依据，当前枚举为 `HELP`、`VERSION`、`REPL`、
`RUN_PROJECT`、`COMPILE_PROJECT`、`RUN_INLINE`、`RUN_PROJECT_MODULE`、
`DUMP_ZRP_METADATA`、`DIFF_ZRP_METADATA`、`CHECK_ZRP_METADATA_VERSION`、
`MIGRATE_SYNTAX` 和 `TEST`。

| 命令 | 作用 |
| --- | --- |
| `zr_vm_cli` | 进入 REPL |
| `zr_vm_cli app.zrp` | 按 `.zrp.entry` 运行项目 |
| `zr_vm_cli --compile app.zrp` | 只编译，输出 `.zro`/metadata |
| `zr_vm_cli --compile app.zrp --run` | 编译后按 binary 路径运行 |
| `zr_vm_cli -e 'return 1 + 2;'` / `-c` | bare global inline 运行 |
| `zr_vm_cli --project app.zrp -m tools.seed` | 指定模块运行 |
| `zr_vm_cli test app.zrp` | 编译 Test phase 并运行 TestManifest |
| `--help` / `--version` | 独占帮助/版本输出 |

运行修饰符包括 `--execution-mode interp|binary`、`--emit-executed-via`、
`--intermediate`、`--incremental`、`--debug`、`--debug-address <host:port>`、`--debug-wait`、
`--profile <file>`、`--coverage <file>`、`--dump-bytecode <file>` 和
`--heap-summary <file>`。`--` 是唯一透传分隔符，之后的参数写入
`zr.system.process.arguments`；第 0 项是入口标识，第 1 项开始是用户参数。

`--help`/`--version` 不能和其它 flag 混用；`-m` 必须与 `--project` 同时出现且不与
`--compile` 组合；inline 模式不接受 compile/debug 修饰符；execution mode 只在有 active
run path 时有效。解析错误在 dispatch 前返回，不会创建 VM state。

## REPL

REPL 支持 `:help`、`:quit`、`:reset`、`:type <expression>` 和空行提交多行缓冲。每次提交
使用 fresh global/state，但把已成功声明的源码前缀重新编译，因此绑定可跨提交观察；这不等于
持久 runtime object。裸表达式包装成 `return <expr>;`，声明、控制流和显式 return 不包装。
`:type` 只执行 parser/type inference，输出 Type、constant、numeric range、logical flow、
ownership/reference 和 call facts，绝不执行目标表达式。

## 项目编译和缓存

compile handler 从 `.zrp.entry` 建 module graph，`--incremental` 使用
`binary/.zr_cli_manifest` 记录源码 hash/imports/可达模块；依赖变化传播重编译，失去可达性的
旧 `.zro/.zri` 被标记 stale。`--intermediate` 额外写 `.zri`，`--emit-zrm` 写 package
容器。source-first、binary-first 和 AOT 运行最终都通过 library/core loader，CLI 不复制
binary decoder。

## 元数据和诊断

`--dump-zrp-metadata`、`--diff-zrp-metadata`、`--check-zrp-metadata-version` 用结构化 JSON
读取 project manifest；`migrate syntax <path> --check|--write` 只对明确的 legacy token 生成
edit，`--check` 不改文件，`--write` 通过当前 parser 重验证后才原子覆盖。退出码约定：0
成功，1 用户程序/测试失败，2 命令误用，3 runner/VM
基础设施失败。宿主可用 `ZrCli_Command_Parse` 直接获得结构化 command 对象。
