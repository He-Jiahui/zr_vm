---
related_code:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli.c
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/command.h
  - zr_vm_cli/src/zr_vm_cli/command.c
  - zr_vm_cli/src/zr_vm_cli/project.h
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler.h
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl.h
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/command.c
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
plan_sources:
  - user: 2026-03-31 实现 ZR VM CLI 命令系统与 compile/run/REPL 计划
tests:
  - tests/cli/test_cli_args.c
  - tests/cmake/run_cli_suite.cmake
  - tests/CMakeLists.txt
doc_type: module-detail
---

# ZR VM CLI Command System

## Purpose

这次 CLI 重构的目标不是只加几个 if/else 开关，而是把 `zr_vm_cli <project.zrp>` 的单一路径入口拆成可扩展的“解析对象 + 独立 handler”结构。这样后续再加模式时，只需要扩展命令对象和分发层，不必把参数判断、项目路径处理、编译图收集和执行逻辑继续堆进 `main`。

第一版固定支持 4 个用户模式：

- 无参数进入最小 REPL。
- `<project.zrp>` 继续保持兼容，按旧语义 source-first 直接运行工程。
- `--compile <project.zrp>` 编译项目内可达模块到 `binary` 目录。
- `--help` 输出帮助和示例。

`--intermediate`、`--incremental`、`--run` 都不是独立模式，而是 `--compile` 的修饰符。

## Public CLI Contract

`zr_vm_cli/src/zr_vm_cli/command.h` 定义了两个核心类型：

- `EZrCliMode`
  - `HELP`
  - `REPL`
  - `RUN_PROJECT`
  - `COMPILE_PROJECT`
- `SZrCliCommand`
  - `projectPath`
  - `runAfterCompile`
  - `emitIntermediate`
  - `incremental`

`zr_vm_cli/src/zr_vm_cli/command.c` 只负责两件事：

- 把 argv 解析成 `SZrCliCommand`
- 在非法组合时给出稳定错误文案

当前解析规则固定为：

- 无参数 => `REPL`
- 单个位置参数 `.zrp` => `RUN_PROJECT`
- `--compile <project.zrp>` => `COMPILE_PROJECT`
- `--run` / `--intermediate` / `--incremental` 脱离 `--compile` 直接报错
- `--help` 不能与其它参数组合
- 额外位置参数、未知 flag、重复 mode flag 直接报错

`zr_vm_cli/src/zr_vm_cli/app.c` 是唯一分发点。它根据 `SZrCliCommand.mode` 调用 help、REPL、source-first run、compile、compile+run handler，不再让 `main` 自己决定流程。

## Project Context And Paths

`zr_vm_cli/src/zr_vm_cli/project.c` 把所有项目级路径逻辑收口到 `SZrCliProjectContext`：

- `projectRoot`
- `sourceRoot`
- `binaryRoot`
- `entryModule`
- `manifestPath`

这些信息都从 `.zrp` 和 `zr_vm_library` 现有 project 解析规则派生，不在 CLI 侧复制第二套 project 语义。

同一文件还负责：

- 模块名标准化
- `source/<module>.zr`、`binary/<module>.zro`、`binary/<module>.zri` 路径解析
- 父目录创建
- 文本文件读取
- manifest 纯文本读写
- FNV-1a 64-bit 稳定哈希

CLI 自己管理的增量缓存清单固定落在 `binary/.zr_cli_manifest`，格式是版本化纯文本。manifest 记录：

- 模块名
- 源码哈希
- 项目内 imports
- `.zro` 路径
- `.zri` 路径

## Compile Pipeline

`zr_vm_cli/src/zr_vm_cli/compiler.c` 拆成三段职责：

### 1. Compile Graph Collection

从 `.zrp` 的 `entry` 出发，先读取源码并构建 AST，然后递归扫描 `%import("...")`：

- 只把能解析到 `sourceRoot` 下真实 `.zr` 源文件的模块纳入本地编译图
- native 模块或项目外依赖只记录 import 名，不参与本地 compile

图中的每个节点用 `SZrCliModuleRecord` 表示：

- 模块名
- 源码路径
- `.zro` / `.zri` 输出路径
- import 列表
- 源码哈希
- dirty 标记

### 2. Dirty Analysis

非增量编译时，所有可达模块都直接标记 dirty。

增量编译时，每个模块先跟 manifest 对比：

- manifest 缺项 => dirty
- 源码哈希变化 => dirty
- import 列表变化 => dirty
- 需要的产物缺失 => dirty

然后再做一轮依赖传播：

- 被 dirty 模块导入的项目内模块也会被重新编译

这保证了导入方不会继续吃旧的类型元数据或旧的编译期投影结果。

### 3. Output And Cleanup

dirty 模块逐个重新编译，并总是写 `.zro`：

- `--intermediate` 打开时额外写 `.zri`
- 编译成功后输出 `compile summary: compiled=... skipped=... removed=...`

增量模式还会清理 manifest 中“之前存在、现在已不可达”的旧模块：

- 删除旧 `.zro`
- 删除旧 `.zri`
- 从新 manifest 中移除条目

## Binary-First Run After Compile

`zr_vm_cli/src/zr_vm_cli/runtime.c` 负责两条运行路径：

- `RunProjectSourceFirst`
  - 继续调用 `ZrLibrary_Project_Run`
  - 保持原有 source-first 语义不变
- `RunProjectBinaryFirst`
  - compile 成功后重新创建一套干净 global/state
  - entry 和 imports 都优先从 `binary` 读 `.zro`
  - binary 缺失时才回退到源码

这里有一个关键接缝：`.zro` 中的 entry function 需要还原成运行时 `SZrFunction`。为避免 CLI 复制 `module_loader.c` 里的旧静态转换逻辑，这个行为收口成了 core helper：

- `zr_vm_core/include/zr_vm_core/io.h`
- `ZrCore_Io_LoadEntryFunctionToRuntime(...)`

本次还额外补上了 `.zro` 常量池里“编译器注入的内部 native callable”恢复路径。否则 `--compile --run` 遇到 `%import(...)` 或 ownership helper 时，entry `.zro` 会因为常量还原失败而无法执行。

具体做法是：

- writer 在常量 debug 槽位里编码稳定 helper id
- runtime loader 按 helper id 重建 native closure 或 native pointer
- helper id 目前覆盖：
  - module import native entry
  - `%unique`
  - `%shared`
  - `%weak`
  - `%using`

import callable 本身也被下沉到 `zr_vm_core`，这样 source compile 路径和 binary load 路径共用同一个 runtime helper，而不是 parser 私有静态函数和 core 侧再各写一份。

## REPL Rules

`zr_vm_cli/src/zr_vm_cli/repl.c` 实现的是一个刻意保持最小化的 v1：

- 无参数进入
- `:help`
- `:quit`
- `:reset`
- 多行缓冲，空行提交
- 每次提交都创建新的瞬时 global/state

它不承诺跨提交保留变量、副作用或模块状态。每次提交都是独立编译执行，成功就打印结果，失败直接透出现有编译或运行时错误。

本次重构里顺手删掉了 REPL 旧执行路径残留，避免后续继续维护一份未使用的兼容代码。

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

- 无参数 => `REPL`
- 单个位置参数 => `RUN_PROJECT`
- `--compile` 与 `--intermediate` / `--incremental` / `--run` 的合法组合
- `--run` / `--intermediate` / `--incremental` 脱离 `--compile` 的非法组合
- 未知参数、重复 mode 参数、额外位置参数

`tests/cmake/run_cli_suite.cmake` 覆盖了：

- `--help`
- 位置参数运行兼容
- `--compile`
- `--compile --intermediate`
- 递归编译项目内 imports
- `--compile --run`
- manifest 首次创建、缓存命中、依赖传播、旧产物清理
- REPL 的 `:help` / `:reset` / `:quit`

其中 `compile_recursive_and_run` 现在会真正经过 binary-first entry load，而不是靠 source fallback 掩盖 `.zro` 常量恢复问题。

## Follow-up Constraints

- manifest 仍然是 CLI 私有纯文本格式，不对外承诺兼容别的工具。
- REPL 仍是无状态瞬时执行，不应把它误当成交互式工程会话。
- 当前 binary helper id 只覆盖编译器已知的内部 runtime callables；如果后续新增其它编译器注入 native helper，必须同时补 writer 和 runtime 的 helper 映射。
