---
related_code:
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_debug_e2e.c
  - tests/cmake/run_cli_suite.cmake
  - tests/fixtures/projects/cli_args/cli_args.zrp
  - tests/fixtures/projects/cli_args/src/main.zr
  - tests/fixtures/projects/cli_args/src/tools/seed.zr
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_debug_e2e.c
  - tests/cmake/run_cli_suite.cmake
plan_sources:
  - user: 2026-03-31 实现 ZR VM CLI 命令系统与 compile/run/REPL 计划
  - user: 2026-04-06 扩展 zr_vm_cli 入口参数、透传参数与 process.arguments 契约
  - user: 2026-04-06 扩成 CLI 覆盖矩阵，列出入口模式、合法组合、非法组合和 process.arguments 契约
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_debug_e2e.c
  - tests/cmake/run_cli_suite.cmake
  - tests/fixtures/projects/cli_args/cli_args.zrp
  - tests/fixtures/projects/cli_args/src/main.zr
  - tests/fixtures/projects/cli_args/src/tools/seed.zr
doc_type: testing-guide
---

# ZR VM CLI Coverage Matrix

这份文档把 `zr-vm-cli-command-system.md` 里的行为压平成四张表：

- 入口模式表
- 合法组合表
- 非法组合表
- `zr.system.process.arguments` 契约表

如果你要改 parser、runtime 分发、REPL 尾随进入逻辑，先看这份矩阵，再回到 `command.c`、`app.c`、`runtime.c` 和对应测试。

## Entry Mode Matrix

| CLI 形式 | 主模式来源 | 最终 `EZrCliMode` | 默认执行路径 | `-i` 含义 | `-- <args...>` | `process.arguments[0]` | 覆盖证据 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `zr_vm_cli` | 无主模式 | `ZR_CLI_MODE_REPL` | REPL | 不适用 | 不支持 | `<repl>` | Parse: `test_no_args_enters_repl`、`test_interactive_modifier_parse_and_validate`；Integration: `run_repl`、`run_repl_native_import`、`run_repl_runtime_error`；E2E: `test_repl_help_is_visible_before_quit` |
| `zr_vm_cli -i` | 无主模式 | `ZR_CLI_MODE_REPL` | REPL | 单独使用时等价于 REPL | 不支持 | `<repl>` | Parse: `test_interactive_modifier_parse_and_validate`；E2E: `test_repl_help_is_visible_before_quit` |
| `zr_vm_cli <project.zrp>` | 位置参数项目 | `ZR_CLI_MODE_RUN_PROJECT` | `interp` | 运行成功后进入 fresh REPL | 支持 | 传入的 `.zrp` 路径文本 | Parse: `test_positional_project_keeps_run_compatibility`、`test_passthrough_arguments_stop_cli_parsing`；Integration: `run_positional`、`run_positional_args`、`run_interactive_after_run` |
| `zr_vm_cli --compile <project.zrp>` | `--compile` | `ZR_CLI_MODE_COMPILE_PROJECT` | 无 active run path | 非法 | 不支持 | 不注入 | Parse: `test_compile_flag_combinations_parse`、`test_compile_only_modifiers_require_compile`；Integration: `run_compile_hello`、`run_compile_intermediate` |
| `zr_vm_cli --compile <project.zrp> --run` | `--compile` | `ZR_CLI_MODE_COMPILE_PROJECT` | `binary` | 运行成功后进入 fresh REPL | 支持 | 传入的 `.zrp` 路径文本 | Parse: `test_compile_flag_combinations_parse`、`test_compile_run_interactive_parse`；Integration: `run_compile_run_args`、`run_compile_run_default_binary`、`run_compile_interactive_after_run` |
| `zr_vm_cli -e <code>` | `-e` | `ZR_CLI_MODE_RUN_INLINE` | bare global inline runtime | 运行成功后进入 fresh REPL | 支持 | `-e` | Parse: `test_inline_code_aliases_parse`、`test_inline_and_module_passthrough_parse`；Integration: `run_inline_code_eval_alias` |
| `zr_vm_cli -c <code>` | `-c` | `ZR_CLI_MODE_RUN_INLINE` | bare global inline runtime | 运行成功后进入 fresh REPL | 支持 | `-c` | Parse: `test_inline_code_aliases_parse`、`test_inline_and_module_passthrough_parse`；Integration: `run_inline_code` |
| `zr_vm_cli --project <project.zrp> -m <module>` | `--project` + `-m` | `ZR_CLI_MODE_RUN_PROJECT_MODULE` | `interp` | 运行成功后进入 fresh REPL | 支持 | 原始模块名文本 | Parse: `test_project_module_mode_parse`、`test_inline_and_module_passthrough_parse`；Integration: `run_project_module`、`run_project_module_interp` |
| `zr_vm_cli -h` / `--help` / `-?` | help alias | `ZR_CLI_MODE_HELP` | 无 | 非法 | 不支持 | 不注入 | Parse: `test_help_aliases_and_exclusivity`；Integration: `run_help`、`run_help_alias`、`run_help_question_alias` |
| `zr_vm_cli -V` / `--version` | version alias | `ZR_CLI_MODE_VERSION` | 无 | 非法 | 不支持 | 不注入 | Parse: `test_version_aliases_and_exclusivity`；Integration: `run_version`、`run_version_long` |

## Legal Combination Matrix

表里的 `Y` 表示允许，`N` 表示 parser 直接拒绝，`Cond` 表示允许但有额外前提。

| 入口模式 | `--execution-mode` | `--emit-executed-via` | `--debug*` | compile family | `-i` | `-- <args...>` | 说明 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| REPL | N | N | N | N | `standalone alias only` | N | `zr_vm_cli -i` 只是 REPL 别名，不是“执行后再进 REPL” |
| 位置参数项目运行 | Y: `interp` / `binary` | Y | Y | N | Y | Y | `--debug-address` / `--debug-wait` / `--debug-print-endpoint` 仍然要先有 `--debug` |
| compile-only | N | N | N | Y: `--intermediate` / `--incremental` / `--run` | N | N | 只有加上 `--run` 才会出现 active run path |
| compile+run | Y: 当前实现会把 `interp` 折叠成 `binary` | Y | Y | Y | Y | Y | `--compile --run` 未显式指定执行模式时默认走 `binary` |
| inline `-e/-c` | N | N | N | N | Y | Y | 只走 bare global + 标准模块注册，不接 compile/debug/module 流程 |
| `--project ... -m ...` | Y: 仅 `interp` / `binary` | Y | Y | N | Y | Y | `process.arguments[0]` 保留原始 dotted module 名，内部再归一成 slash path |
| help / version | N | N | N | N | N | N | 必须独占 |

`compile family` 固定指这组 flag：

- `--run`
- `--intermediate`
- `--incremental`

`--debug*` 固定指这组 flag：

- `--debug`
- `--debug-address`
- `--debug-wait`
- `--debug-print-endpoint`

## Illegal Combination Matrix

| 非法组合 | 阶段 | 稳定错误片段 | 原因 | 覆盖证据 |
| --- | --- | --- | --- | --- |
| help + 任意其它选项 | Parse | `--help cannot be combined with other options` | help 必须独占 | `test_help_aliases_and_exclusivity`；`run_help` / `run_help_alias` / `run_help_question_alias` |
| version + 任意其它选项 | Parse | `--version cannot be combined with other options` | version 必须独占 | `test_version_aliases_and_exclusivity`；`run_version` / `run_version_long` |
| 重复或冲突主模式 | Parse | `Duplicate mode option:` 或 `cannot be combined with other main modes` | 主模式互斥 | `test_unknown_and_duplicate_modes_fail` |
| 多余位置参数 | Parse | `Unexpected positional argument:` | 位置参数模式只接受一个 `.zrp` | `test_unknown_and_duplicate_modes_fail` |
| `-m` + `--compile` | Parse | `-m cannot be combined with --compile` | 模块入口运行和 compile 主模式互斥 | `test_module_mode_requires_project_and_rejects_unsupported_combinations` |
| `--project` 没有 `-m` | Parse | `--project requires -m <module>` | 显式项目绑定必须指定入口模块 | `test_module_mode_requires_project_and_rejects_unsupported_combinations` |
| `-m` 没有 `--project` | Parse | `-m requires --project <project.zrp>` | 模块名不能脱离项目上下文存在 | `test_module_mode_requires_project_and_rejects_unsupported_combinations` |
| inline + compile/debug/module/execution-mode 选项 | Parse | `Inline -e/-c mode does not support compile, debug, module, or execution-mode options` | `-e/-c` 只支持 bare global 执行 | `test_eval_rejects_compile_debug_and_module_options` |
| compile-only + run-path 选项或透传参数 | Parse | `require an active run path` | compile-only 没有 active run path | `test_passthrough_arguments_require_active_run_path`、`test_debug_flags_require_debug_and_active_run_path`；`run_compile_passthrough_error` |
| REPL + run-path 选项或透传参数 | Parse | `require a project run path` | REPL 不是项目运行路径 | `test_passthrough_arguments_require_active_run_path`、`test_debug_flags_require_debug_and_active_run_path` |
| `--project ... -m ... --execution-mode aot_c` | Parse | `Unknown execution mode:` | 主仓已移除 AOT 执行模式 | `run_project_module_unknown_execution_mode` |
| `-i` + help / version / compile-only | Parse | `--interactive cannot be combined with help, version, or compile-only paths` | `-i` 只允许 REPL alias 或 run 成功后的 fresh REPL | `test_interactive_modifier_parse_and_validate` |
| `--debug-address` / `--debug-wait` / `--debug-print-endpoint` 缺少 `--debug` | Parse | `--debug-address, --debug-wait, and --debug-print-endpoint require --debug` | debug 子选项必须挂在 `--debug` 下 | `test_debug_flags_require_debug_and_active_run_path` |
| compile family 缺少 `--compile` | Parse | `--run, --intermediate, and --incremental require --compile <project.zrp>` | compile 修饰符不能独立存在 | `test_compile_only_modifiers_require_compile` |
| 缺少 flag 值 | Parse | `Missing <project.zrp> after --compile`、`Missing <project.zrp> after --project`、`Missing <module> after -m`、`Missing inline code after -e`、`Missing address after --debug-address`、`Missing execution mode after --execution-mode` | 需要参数的 flag 缺值 | `test_missing_required_values_fail` |
| 未知执行模式 | Parse | `Unknown execution mode:` | 只接受 `interp` / `binary` | `test_missing_required_values_fail` |
| 未知选项 | Parse | `Unknown option:` | parser 不做宽松回退 | `test_unknown_and_duplicate_modes_fail` |
| CLI 构建未带 debug agent 却使用 `--debug` | Runtime | `debug agent support is not built into this CLI` | 运行期能力缺失 | 代码路径：`zr_vm_cli/src/zr_vm_cli/runtime/runtime.c` |

## `zr.system.process.arguments` Contract Matrix

| 入口模式 | 是否注入 | `arguments[0]` | `arguments[1+]` 来源 | 例子 | 覆盖证据 |
| --- | --- | --- | --- | --- | --- |
| REPL | Y | `<repl>` | 无；REPL 不接受透传 | `["<repl>"]` | 代码路径：`repl.c`；Integration: `run_repl_native_import` |
| 位置参数项目运行 | Y | 传入的 `.zrp` 路径文本 | `--` 之后的用户参数 | `["demo.zrp", "arg1", "-x"]` | Fixture: `cli_args/src/main.zr`；Integration: `run_positional_args` |
| compile+run | Y | 传入的 `.zrp` 路径文本 | `--` 之后的用户参数 | `["demo.zrp", "arg1", "-x"]` | Fixture: `cli_args/src/main.zr`；Integration: `run_compile_run_args`、`run_compile_run_default_binary` |
| compile-only | N | 不适用 | 不适用 | 无 | Parse: `test_passthrough_arguments_require_active_run_path`；Integration: `run_compile_passthrough_error` |
| `--project ... -m ...` | Y | 原始模块名文本 | `--` 之后的用户参数 | `["tools.seed", "foo", "bar"]` | Fixture: `cli_args/src/tools/seed.zr`；Integration: `run_project_module`、`run_project_module_interp` |
| `-e <code>` | Y | `-e` | `--` 之后的用户参数 | `["-e", "foo", "bar"]` | Integration: `run_inline_code_eval_alias` |
| `-c <code>` | Y | `-c` | `--` 之后的用户参数 | `["-c", "foo", "bar"]` | Integration: `run_inline_code` |
| help / version | N | 不适用 | 不适用 | 无 | `run_help*`、`run_version*` |

契约固定点：

- `arguments[0]` 永远是入口标识，不是 `zr_vm_cli` 可执行名。
- CLI 自己的 flag 不会进入 `zr.system.process.arguments`。
- `.zrp` 入口仍按 0 参数 `main()` 约定调用；参数统一从 `zr.system.process.arguments` 读取。
- 模块入口运行会把 dotted module 名保留给 `arguments[0]`，内部再转换为 slash path 去解析实际模块文件。
