---
related_code:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/project/project.h
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - tests/cli/test_cli_args.c
  - tests/cmake/run_cli_suite.cmake
  - tests/fixtures/projects/cli_args/cli_args.zrp
  - tests/fixtures/projects/cli_args/src/main.zr
  - tests/fixtures/projects/cli_args/src/tools/seed.zr
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.h
  - tests/cli/test_cli_args.c
  - tests/cmake/run_cli_suite.cmake
  - tests/fixtures/projects/cli_args/cli_args.zrp
  - tests/fixtures/projects/cli_args/src/main.zr
  - tests/fixtures/projects/cli_args/src/tools/seed.zr
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
doc_type: module-detail
---

# ZR VM CLI Command System

需要快速核对某个入口模式、flag 组合、稳定错误片段或 `zr.system.process.arguments` 行为时，直接看 `zr-vm-cli-coverage-matrix.md`。本文件保留设计语义、实现边界和测试解释，不再承担所有查表职责。

## Purpose

这版 CLI 不再把“位置参数运行项目”当成唯一主线，而是显式拆成“互斥主模式 + 可验证修饰符 + 用户参数透传”的命令系统。目标有三条：

- 保留旧入口兼容性：
  - 无参数进入 REPL
  - `zr_vm_cli <project.zrp>` 直接运行项目
  - `zr_vm_cli --compile <project.zrp>` 编译，带 `--run` 时同时运行
- 补齐主流语言 CLI 常见入口：
  - `-h/-?/--help`
  - `-V/--version`
  - `-e/-c`
  - `--project <project.zrp> -m <module>`
  - `-i/--interactive`
- 把用户程序参数稳定注入 `zr.system.process.arguments`，而不是继续停留在 metadata 声明层。

实现边界仍然保持原来的“解析对象 + 分发层 + 运行层”结构。`main` 只做 parse 和 dispatch，不再直接理解每个 flag 组合。

## Public CLI Contract

`zr_vm_cli/src/zr_vm_cli/command/command.h` 定义了两个核心类型：

- `EZrCliMode`
  - `HELP`
  - `VERSION`
  - `REPL`
  - `RUN_PROJECT`
  - `COMPILE_PROJECT`
  - `RUN_INLINE`
  - `RUN_PROJECT_MODULE`
- `SZrCliCommand`
  - `projectPath`
  - `inlineCode`
  - `inlineModeAlias`
  - `moduleName`
  - `programArgs`
  - `programArgCount`
  - `runAfterCompile`
  - `interactiveAfterRun`
  - `emitIntermediate`
  - `incremental`
  - `executionMode`
  - `emitExecutedVia`
  - `debugEnabled`
  - `debugAddress`
  - `debugWait`
  - `debugPrintEndpoint`

`zr_vm_cli/src/zr_vm_cli/command/command.c` 只负责两件事：

- 把 argv 解析成 `SZrCliCommand`
- 在非法组合时给出稳定错误文案

当前主模式固定互斥，只允许出现一个：

- 无参数
- `<project.zrp>`
- `--compile <project.zrp>`
- `-e <code>`
- `-c <code>`
- `--project <project.zrp> -m <module>`
- `-h | --help | -?`
- `-V | --version`

`zr_vm_cli/src/zr_vm_cli/app/app.c` 是唯一分发点。它根据 `SZrCliCommand.mode` 调用 help、REPL、source-first run、compile、compile+run handler，不再让 `main` 自己决定流程。

## Parse Rules And Boundaries

解析层当前遵守这些边界：

- `--help` 和 `--version` 必须独占，不能混入其它 flag、透传参数或修饰符。
- `-i/--interactive` 只能用于有 active run path 的模式：
  - 合法：位置参数项目运行、`--project -m`、`-e/-c`、`--compile --run`
  - 非法：help、version、compile-only
- `-m` 只能和 `--project <project.zrp>` 组合。
- `-m` 不能和 `--compile` 组合。
- `--project ... -m ...` 在 v1 只支持：
  - `--execution-mode interp`
  - `--execution-mode binary`
- `--project ... -m ...` 遇到历史 AOT 执行模式名字会直接报未知模式错误，不做隐式回退。
- `-e/-c` 只支持 bare global 运行，不支持 compile、debug 或 module 模式 flag。
- `--run`、`--intermediate`、`--incremental` 仍然从属于 `--compile`。
- `--execution-mode`、`--emit-executed-via`、`--debug` 和用户透传参数都要求存在 active run path。
- `--` 是唯一的用户参数分隔符。之后的内容不再参与 CLI 解析。

`--compile <project.zrp> --run` 仍然保持现有 compile+run 路径，但当前实现会把 `interp` 基线提升为 `binary`。这意味着未传 `--execution-mode` 时默认走 `binary`，而 parser 当前也不会保留“显式传了 `interp`”这一层区别。纯 compile 不会自动运行。

## REPL Rules

`zr_vm_cli/src/zr_vm_cli/repl/repl.c` 实现的是一个刻意保持最小化的 v1：

- 无参数进入
- `:help`
- `:quit`
- `:reset`
- `:type <expression>`
- 多行缓冲，空行提交
- 每次提交仍创建新的瞬时 global/state，但会把本次 REPL 会话中已成功提交的声明源码作为上下文前缀重新编译
- 裸表达式提交会在 REPL 内包装成瞬时 `return <expr>;` 执行，例如 `1 + 2` 会输出 `3`

它现在承诺跨提交保留成功声明出的源码级绑定，例如：

- 第一次提交：`var seed = 2;`
- 第二次提交：`seed + 3`

第二次提交会在同一会话源码前缀下重新编译并输出 `5`。这不是完整的长生命周期运行时 cell 存储：每次提交仍使用 fresh VM runtime，声明 initializer 可能随源码 replay 重新执行，`:reset` 会清空当前会话源码上下文。

裸表达式包装只用于“看起来是表达式”的输入。声明、控制流、函数/类定义、显式 `return`、包含语句分号的提交不会被包装。像 `1 +` 或 `true &&` 这种以操作符结尾的不完整表达式也不会套入内部 `return` 文本，而是直接交给 parser 报出具体问题、原因和建议，例如 `Missing expression after '+'`、`Cause: ...`、`Suggestion: ...`。这样 REPL 可以提升表达式运行能力，同时避免用户在错误信息里看到内部包装细节。

`:type <expression>` 是 REPL 的局部语义查询入口。它创建 fresh analysis state，把当前会话声明源码和参数表达式一起解析，先把 prior variable declarations 注册进 parser type environment，再调用 `ZrParser_ExpressionType_Infer` 推断最后一个 expression statement，并从同一个 `SZrSemanticContext` 读取 numeric/logical/reachability/ownership、reference 和 expression payload facts 输出。`1 + 2` 会显示 `Type: int` 和 `Numeric range: 3..3`；在同一会话内先提交 `var seed = 2;` 后，`:type seed + 3` 会显示 `Numeric range: 5..5`；`:type true || false` 会显示 `Logical flow: short-circuits right operand` 和 `Reachability: unreachable because short-circuit skips evaluation`；若先声明 `var owner: %unique int;`，`:type %borrow(owner)` 会显示 `Type: %borrowed int` 和 `Ownership: borrow %borrowed`；若查询 `pick(42)` 这类调用表达式，会显示 `Call: pick args=1`；`:type seed.value` 会显示 `Member: value` 和 `Reference: member value`；`:type seed[index]` 会显示 `Member: index` 和 `Reference: member index`；`:type seed.value = 3` 会显示 `Reference: member write value`。这个命令不会执行目标表达式，只暴露当前表达式的编译期推断结果和已发射的局部语义事实；unresolved member-access/member-write facts 不会伪造 `Declared at:` 位置。

REPL v1 还支持两条扩展约束：

- `-i/--interactive` 会在成功执行主模式后进入一个 fresh REPL。
- fresh REPL 会重新注入 `<repl>` 的 `zr.system.process.arguments`，不会继承前一个项目或 inline run 的变量状态。
- banner、`:help` 这类控制路径文本会在下一次阻塞读前显式 flush，不依赖宿主终端或 CRT 在进程退出时再统一刷出。

这轮实现还专门修了 REPL 提交失败后的生命周期问题。提交生成的 function/closure 会在可能触发 GC 的窗口里临时 pin 住，运行结束后再解 pin，避免错误路径打印完成后在清理阶段触发 access violation。

## Runtime Argument Injection Contract

`zr_vm_cli/src/zr_vm_cli/runtime/runtime.c` 现在会把真实用户参数写入 `zr.system.process.arguments`。这个接口从“模块里声明了一个 `arguments` 常量”升级成了真正可用的运行时契约。

注入规则固定为：

- 数组第 0 位总是入口标识
- 数组第 1 位开始才是用户通过 `--` 透传的参数
- 数组内容不包含：
  - `zr_vm_cli` 可执行名
  - CLI 内部 flag
  - `.zrp` 之外的运行器元信息

当前入口标识规则：

- 直接运行 `.zrp`：传入的 `.zrp` 路径文本
- `--compile <project.zrp> --run`：传入的 `.zrp` 路径文本
- `--project <project.zrp> -m <module>`：模块名文本
- `-e <code>`：`-e`
- `-c <code>`：`-c`
- REPL：`<repl>`

这个注入契约不会改变项目入口调用方式。`.zrp.entry` 仍然按当前 0 参数约定执行，参数读取统一走 `zr.system.process.arguments`。

## Project Run, Inline Run, And Module Run

`zr_vm_cli/src/zr_vm_cli/runtime/runtime.c` 当前负责三类运行型 handler：

- `RUN_PROJECT`
  - 继续尊重 `.zrp.entry`
  - 默认按现有项目语义直接运行
- `RUN_INLINE`
  - `-e/-c` 走 bare global
  - 会注册标准模块并注入 `process.arguments`
  - 不接 compile/debug 流程
- `RUN_PROJECT_MODULE`
  - `--project <project.zrp> -m <module>`
  - 只在 CLI 内临时覆盖有效入口模块
  - 不改写 `.zrp` 文件本身
  - 会把 `tools.seed` 这种 dotted module 名归一到项目路径解析需要的 slash form，同时保留原始模块名作为 `process.arguments[0]`

## Compile Pipeline

`zr_vm_cli/src/zr_vm_cli/compiler/compiler.c` 仍然负责项目级 compile、增量 manifest 和 compile+run 前置阶段。本轮参数扩展没有改 `.zrp` schema，也没有改 entry 语义，compile 侧仍然从 `.zrp.entry` 出发构图和输出。

CLI 自己管理的增量缓存清单仍然固定落在 `binary/.zr_cli_manifest`，记录可达模块、源码哈希、imports 和输出文件，用于：

- `--incremental` 跳过未变化模块
- 依赖传播重编译
- 删除已不可达模块的 stale `.zro` / `.zri`

## Help And Version Output

帮助文本现在按四段输出：

- Usage
- Main Modes
- Modifiers
- Passthrough / Examples

这样帮助内容不再只围着 `--compile` 解释，而是能直接覆盖：

- REPL
- 位置参数项目运行
- compile
- inline
- 显式项目模块运行
- `--` 透传

`--version` 统一输出 `ZR_VM_VERSION_FULL` 的单行版本字符串。

## Extensibility Boundary

后续如果继续扩 CLI，优先遵守这几个边界：

- 新参数先扩 `SZrCliCommand`，不要把 argv 判断塞进 handler
- 新项目路径规则优先落 `project.c`
- 新 compile 行为优先落 `compiler.c`
- 新执行模式优先在 `app.c` 分发，不要让 `main` 自己分叉
- 任何 `.zro` 运行时恢复逻辑优先共用 `zr_vm_core` helper，不要在 CLI 侧复制 binary decode 代码

这套边界是为了保证命令系统可以继续长，而不是再退回“每加一个 flag 就复制一段新流程”的状态。

## Test Coverage

`tests/cli/test_cli_args.c` 覆盖了：

- help/version 独占规则
- 无参数 => `REPL`
- 单个位置参数 => `RUN_PROJECT`
- `-e/-c` 等价与互斥约束
- `-m` 必须配 `--project`
- `-i` 的合法和非法组合
- `--compile` / `--project` / `-m` / `-e` / `--debug-address` / `--execution-mode` 的缺参错误
- `--` 之后停止解析
- 透传参数在命令对象里的捕获结果
- `--compile --run` 默认切到 `binary`
- inline 与 module 模式的 passthrough + `interactiveAfterRun` 组合
- 旧 `.zrp` / `--compile` / debug 组合不回归

`tests/cmake/run_cli_suite.cmake` 覆盖了：

- `-h/-?/--help`
- `-V/--version`
- 无参 REPL
- 直接运行 `.zrp`
- `--compile`
- `--compile --run`
- `--compile --run` 默认 `executed_via=binary`
- `--project <project.zrp> -m <module>` 的 binary 和 interp 路径
- `--project <project.zrp> -m <module>` 的旧执行模式名拒绝错误
- `-e/-c`
- `-i` 的 post-run fresh REPL
- `--compile --run -i` 的 compile-run 后置交互
- `zr.system.process.arguments` 注入结果
- compile-only 出现透传参数时报错
- manifest 首次创建、缓存命中、依赖传播、旧产物清理
- REPL 的 native import、runtime error 透出和退出稳定性

`tests/cli/test_cli_repl_e2e.c` 额外覆盖了：

- banner/help 在退出前可见
- 裸表达式 `1 + 2` 会输出 `3`
- `var seed = 2;` 后的裸表达式 `seed + 3` 会通过会话源码上下文输出 `5`
- 不完整表达式 `1 +` 会输出具体缺右操作数诊断，并且不会泄露内部 `return 1 +` 包装文本
- `:type 1 + 2` 会显示 `Type: int` 和 `Numeric range: 3..3`，并且不会执行表达式打印 `3`
- `var seed = 2;` 后的 `:type seed + 3` 会显示 `Numeric range: 5..5`，并且不会执行表达式打印 `5`

`tests/cli/repl_type_ownership_smoke.js` 覆盖了：

- `var owner: %unique int;` 后的 `:type %borrow(owner)` 会显示 `Type: %borrowed int`
- 同一次查询还会显示 `Ownership: borrow %borrowed`
- 查询不会退化成只打印 `int`，也不会报 `failed to infer expression type`

`tests/cli/repl_type_call_member_smoke.js` 覆盖了：

- `:type pick(42)` 会显示 `Call: pick args=1`
- `:type seed.value` 会显示 `Member: value` 和 `Reference: member value`
- `:type seed[index]` 会显示 `Member: index` 和 `Reference: member index`
- `:type seed.value = 3` 会显示 `Reference: member write value`，并且不会对 unresolved member-write fact 输出 `Declared at:`
- 查询不会执行 `pick(42)`，也不会退化成 `failed to infer expression type`

`tests/cli/repl_type_reachability_smoke.js` 覆盖了：

- `:type true || false` 会显示 `Type: bool`
- 同一次查询还会显示 `Logical flow: short-circuits right operand`
- 同一次查询还会显示 `Reachability: unreachable because short-circuit skips evaluation`
- 查询不会执行表达式打印 `true`，也不会退化成 `failed to infer expression type`

`tests/fixtures/projects/cli_args` 是这轮新增的专用夹具，用来验证：

- 位置参数运行时 `main_arg0` 是 `.zrp` 路径
- `--compile --run` 时参数注入一致
- `--project ... -m tools.seed` 时 `module_arg0` 是模块名
- 透传参数跟在入口标识之后

## Follow-up Constraints

- manifest 仍然是 CLI 私有纯文本格式，不对外承诺兼容别的工具。
- REPL 只保存本会话成功声明的源码上下文；它仍不是持久工程会话，也不保留长期 runtime object/module cell 状态。
- `zr.system.process.arguments` 现在是用户可依赖接口，后续改动必须先考虑兼容性和现有夹具。
- 如果未来要从 `zr_vm_aot/` 重新引入独立执行后端，必须走新的公开设计和测试入口，不能偷偷回退到 interp/binary。
