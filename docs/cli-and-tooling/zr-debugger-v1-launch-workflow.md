---
related_code:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_lib_debug/include/zr_vm_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_network/CMakeLists.txt
  - zr_vm_lib_network/include/zr_vm_network/network.h
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_lib_debug/include/zr_vm_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_network/CMakeLists.txt
  - zr_vm_lib_network/include/zr_vm_network/network.h
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-04-05 实现 ZR Debugger V1 Architecture，主路线为 launch-under-debug，先支持 interp + binary
  - lua/src/ldblib.c
  - lua/cpython/Lib/bdb.py
  - lua/cpython/Lib/pdb.py
  - lua/jdk/src/jdk.jshell/share/classes/jdk/jshell/execution/JdiInitiator.java
  - lua/mono-6.12.0.199/mono/mini/debugger-agent.c
  - lua/mono-6.12.0.199/mono/metadata/attach.c
tests:
  - tests/debug/test_debug_metadata.c
  - tests/debug/test_debug_trace.c
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/debug/test_debug_agent.c
  - tests/debug/test_debug_agent_protocol.c
  - tests/cli/test_cli_args.c
  - tests/CMakeLists.txt
doc_type: module-detail
---

# ZR Debugger V1 Launch Workflow

## Scope

`ZR Debugger V1` 固定采用 `launch-under-debug` 路线，不把 `ptrace`、`gdb/lldb` 风格的外部进程控制作为主模型。运行时会在同一个 VM 进程内启动一个调试 agent，再通过本地 loopback TCP 暴露只读调试会话。

当前交付边界如下：

- 支持 `interp` 和 `binary` 两条 VM 执行路径。
- CLI 暴露 `--debug`、`--debug-address`、`--debug-wait`、`--debug-print-endpoint`。
- 调试协议固定为 `zrdbg/1`，消息形状是 JSON-RPC 风格。
- 支持断点、pause、`stepIn`、`stepOver`、`stepOut`、stack/scopes/variables、uncaught exception stop、terminated event。
- 主仓只承诺 `interp` / `binary`；历史 AOT 调试需求已随 `zr_vm_aot/` 分离出主链路。

## Module Split

### `zr_vm_lib_debug`

`zr_vm_lib_debug` 是调试会话层，不把 socket、listener 或 framing 逻辑塞进 `zr_vm_core`。它负责：

- `ZrDebugAgent` 生命周期
- breakpoint 表和 source line 解析
- safepoint pause loop
- step controller
- stack / scopes / variables 只读快照
- safe evaluate 表达式解析、只读求值和诊断归因
- `stopped` / `continued` / `terminated` 等协议事件

会话层本身只依赖 VM 的 trace hook、call stack、line table 和 value stringification。

### `zr_vm_lib_network`

`zr_vm_lib_network` 只提供 v1 需要的最小 transport 原语：

- `host:port` 解析
- loopback 限制
- listener open / accept
- stream connect / close
- newline framed text message read / write
- token constant-time compare

这里刻意不引入 HTTP 或 WebSocket。以后要做 VS Code、DAP 或 web adapter，可以在更上层翻译到 `zrdbg/1`，不污染 runtime 基础库。

## Runtime Hooking

`zr_vm_core` 里的 `ZrCore_Debug_TraceExecution(...)` 现在是正式调试入口，不再只是 TODO。debug agent 在启动时把自己的 trace observer 装到 `SZrState` 上，并显式打开 `ZR_DEBUG_HOOK_MASK_LINE`，这样 VM dispatch 会在每个 safepoint 回调到调试层。

observer 的 stop reason 优先级固定为：

1. uncaught exception
2. user pause
3. entry stop / wait-for-client
4. breakpoint
5. step

step 语义按“源码位置推进”而不是“同一行内任意下一条 instruction”判停，所以 `stepOver` 不会在同一源码行内来回抖动。

## Breakpoint Mapping

V1 统一用 `module_name + source_file + line + optional function_name` 解析断点，再映射到 `function + instruction_offset`。

### Source path

源码执行直接使用函数上的：

- `sourceCodeList`
- `executionLocationInfoList`
- child function graph

递归解析时会把 child function 一起纳入搜索，所以嵌套函数/闭包里的 line breakpoint 也能绑定。

### Binary path

`.zro` 现在会把以下调试元数据正式序列化进 function debug info：

- source file
- source hash
- per-instruction source line
- per-instruction source start/end column range
- per-instruction source start/end line range

runtime 读取 `.zro` 时会把这些信息恢复到 `SZrFunction`：

- `function->sourceCodeList`
- `function->sourceHash`
- `function->executionLocationInfoList`

这意味着 `binary` 路径下断点仍然按源码文件和行号工作，而且 `zrvm` 现在也可以把 `.zro` 里的 line+column/range 映射一并挂回运行时 function，作为后续 debug link / source map 式能力的正式基础，而不是退化成“只有 instruction line number”。

## CLI Contract

### Supported commands

- `zr_vm_cli <project.zrp> --debug`
- `zr_vm_cli <project.zrp> --debug --debug-address 127.0.0.1:4711`
- `zr_vm_cli <project.zrp> --debug --debug-wait --debug-print-endpoint`
- `zr_vm_cli --compile <project.zrp> --run --debug --execution-mode binary`

### Runtime behavior

- `--debug` 但不带 `--debug-wait`
  - 立即启动 agent 并继续执行用户代码
  - 如果客户端够快，可以在运行期间 attach 到当前会话
- `--debug-wait`
  - 在 entry safepoint 停住
  - 等待客户端连接并初始化后再继续
- `--debug-print-endpoint`
  - 打印实际绑定的 loopback endpoint，尤其适合 `port=0` 自动分配场景

CLI 在 `interp` 和 `binary` 调试路径里都绕过了旧的“直接一把跑完”快捷入口，统一改成：

1. 解析 project context
2. 加载 entry function
3. 预建 module runtime metadata
4. 启动 `ZrDebugAgent`
5. 执行 entry
6. 在 failure 上调用 `ZrDebug_NotifyException(...)`
7. 在结束时调用 `ZrDebug_NotifyTerminated(...)`

## Protocol Surface

v1 请求集：

- `initialize`
- `setBreakpoints`
- `continue`
- `pause`
- `next`
- `stepIn`
- `stepOut`
- `stackTrace`
- `scopes`
- `variables`
- `evaluate`
- `disconnect`

v1 事件集：

- `initialized`
- `breakpointResolved`
- `stopped`
- `continued`
- `moduleLoaded`
- `terminated`

这里没有直接讲 DAP。`zrdbg/1` 是 runtime 内部协议；以后如果要接 VS Code，应该额外做 adapter，把 DAP 翻译成 `zrdbg/1`。

## Safe Evaluate And Conditions

`evaluate`、条件断点和 logpoint 插值共用 `zr_debug_evaluate_expression(...)` 的安全表达式子集。该子集只允许无副作用读取和计算：

- literal、局部变量、参数、`this` / `self`
- member access、index access、debug index window
- unary `!` / `-`
- numeric `+ - * / %` 和关系比较
- equality、`&&`、`||`
- conditional `?:`

调试表达式解析失败时，不能只返回“expected expression”。二元或逻辑运算符右侧缺失时，错误必须说明具体运算符、原因和建议，例如：

- `Missing expression after '+'`
- `Cause: ... ended after a binary/logical operator`
- `Suggestion: add the right-hand expression or remove the operator`

调试表达式的语义错误也必须保留同样的可解释性。数值运算遇到非数值 operand、`/` 右侧为零、`%` 右侧为零或非整数 operand、以及 unary `-` 作用在非数值上时，错误信息需要包含实际 operand 类型、具体原因和下一步建议。这样 `evaluate` 失败响应和条件断点失败输出不会退化成只有“numeric operator failed”这类短文本。

括号、成员、索引和字符串结构错误也走同一类 richer diagnostic 文本。`(true || false`、`true.`、`true[0`、`"open` 和 `"bad\q"` 这类 safe-evaluate 表达式必须说明缺少的 token 或非法结构、错误原因和建议修复动作，而不是只返回 `missing ')'`、`expected member name`、`missing ']'`、`unsupported string escape` 或 `expected expression` 这类短句。字符串 literal 解析失败时，具体的 unterminated-string 或 unsupported-escape 诊断不能被 primary-expression fallback 覆盖。

函数调用语法必须在 identifier lookup 前被识别并拒绝。`sideEffect()` 这类表达式不能退化成 `unknown identifier`，而要明确说明 `safe debug evaluate` 是只读子集、调用可能执行用户代码或改变状态，并给出检查局部变量、成员、索引或纯 literal 表达式的建议。

赋值语法也必须在 identifier lookup 或最终 trailing-token fallback 前被识别并拒绝。`true = false`、`local = 1` 这类表达式不能只返回 `unexpected trailing tokens` 或 `unknown identifier`，而要明确说明 safe evaluate 和条件断点是 read-only 子集，不能改变程序状态，并建议改用无 `=` 的检查表达式或在源码中改变状态。

逻辑运算遵循语言层已经暴露给 LSP 的短路语义：`true || rhs` 和 `false && rhs` 会消费右侧表达式的语法，但不会解析局部变量、读取成员、读取索引或执行数值/比较计算。这样条件断点可以安全处理 `true || missingLocal`、`false && missingLocal` 这类被跳过的数据路径，不会因为右侧局部变量当前不存在而失败。未跳过的右侧仍按 safe evaluate 子集正常解析和只读求值。

条件表达式遵循同一类安全跳过合同：先求值 condition，true/false 两个 branch 都会被语法消费，但只有被选中的 branch 会解析局部变量、读取成员/索引或执行计算。`true ? 1 : missingLocal` 和 `false ? missingLocal : 2` 可以作为 evaluate 或条件断点表达式通过；缺少 `:` 时必须报告具体缺失 token、原因和建议。这里仍是只读调试表达式子集，不承诺完整语言求值。

这条消息会同时覆盖 VS Code/DAP adapter 未来会转发的 `evaluate` 失败响应，以及条件断点失败时写到 `stderr` 的输出事件。当前实现仍是调试时的安全求值器，不承诺完整语言表达式执行，也不替代 parser/LSP 的编译期语义事实层。

## Snapshot Model

variables 目前是只读快照，不支持变量写回或带副作用求值。scope 划分固定为：

- `Locals`
- `Closures`，只有当前函数真的有闭包捕获时才出现
- `Globals`

`Globals` 目前提供的是 runtime 能稳定只读暴露的核心入口：

- `zr`
- `loadedModules`

`variables` 响应中的每个 value preview 和 `evaluate` 响应都会带 `namedVariables` / `indexedVariables`。这两个字段是协议边界的子形状提示，方便 VS Code/DAP adapter 在不先展开 handle 的情况下判断 named/indexed children。普通 object 的 `namedVariables` 与实际展开计数一致：可见字段加上 `$prototype` 等 synthetic entry，隐藏 `__zr_` 字段不计入；直接 array/index window 使用 `indexedVariables`。

这符合 v1 的安全边界，避免把脚本侧的 pause/breakpoint 控制重新暴露回脚本运行时本身。

## Detached Backends

主仓调试合同只覆盖 `interp` 和 `binary`。如果后续要为 `zr_vm_aot/` 归档里的独立后端恢复调试能力，必须走新的独立设计，而不是在当前 v1 合同里隐式复活旧入口。

## Validation Coverage

当前实现对应的回归面：

- `tests/cli/test_cli_args.c`
  - debug flags 解析、依赖关系和 run-path 约束
- `tests/debug/test_debug_trace.c`
  - interpreter trace hook、line event 和 debug info 获取
- `tests/debug/test_debug_metadata.c`
  - `.zro` source identity/debug metadata roundtrip
- `tests/debug/test_debug_expression_diagnostics.c`
  - public `ZrDebug_Evaluate` 缺右操作数时返回具体问题、原因和建议
  - public `ZrDebug_Evaluate` 通过共享 safe-evaluate parser 对 `&&` 缺右操作数给出同一类诊断
  - `true || missingLocal` 和 `false && missingLocal` 验证确定性短路不会解析被跳过的局部变量
  - `(1 < 2) && (3 < 4)`、`!(1 < 2)`、`(1 < 2) || missingLocal` 和 `(2 < 1) && missingLocal` 验证 Debug safe evaluate 已经能把比较结果组合进逻辑表达式和条件断点短路
  - `true ? 1 : missingLocal` 和 `false ? missingLocal : 2` 验证条件表达式只解析被选中分支的数据路径，同时仍消费跳过分支语法
  - `"text" + 1` 和 `1 / 0` 验证 safe evaluate 的数值语义错误也带具体原因和建议
  - `(true || false` 验证 grouped expression 缺少 `)` 时返回具体问题、原因和建议
  - `"open` 和 `"bad\q"` 验证 string literal 结构错误不会退化成 generic expected-expression 或 unsupported-escape 短句
  - `sideEffect()` 验证函数调用在 identifier lookup 前被识别为安全求值策略错误，并返回原因和建议
  - `true = false` 与 `local = 1` 验证赋值语法在 trailing-token fallback 或 identifier lookup 前被识别为 read-only 策略错误，并返回原因和建议
- `tests/debug/test_debug_variable_child_shape.c`
  - `variables` 中的 `zr` value preview 报告与展开结果一致的 `namedVariables` / `indexedVariables`
  - `evaluate("zr")` 返回同一 child-shape metadata，保证 adapter 可以对 evaluate result 做同样的展开判断
- `tests/debug/test_debug_agent.c`
  - invalid token rejection
  - loopback JSON-RPC handshake
  - entry stop
  - line breakpoint hit
  - `stepOver`
  - stackTrace / scopes / variables
  - evaluate 与条件断点/日志断点主流程
  - disconnect-continue policy
- `tests/debug/test_debug_agent_protocol.c`
  - `zrdbg/1` capability、pause、disconnect、reconnect 和协议鲁棒性

手工 smoke 还覆盖了：

- `zr_vm_cli <hello_world.zrp> --debug --debug-print-endpoint`
- `zr_vm_cli --compile <hello_world.zrp> --run --debug --execution-mode binary --debug-print-endpoint`

这两条都必须继续打印 endpoint 并把程序跑完，证明普通运行路径和 debug 运行路径能共存。
