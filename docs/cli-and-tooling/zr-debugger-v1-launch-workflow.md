---
related_code:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_lib_debug/include/zr_vm_debug/debug.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/profile.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_reference_summary.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_union.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c
  - zr_vm_lib_network/CMakeLists.txt
  - zr_vm_lib_network/include/zr_vm_network/network.h
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_heap.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_lib_debug/include/zr_vm_debug/debug.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/profile.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_reference_summary.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_union.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c
  - zr_vm_lib_network/CMakeLists.txt
  - zr_vm_lib_network/include/zr_vm_network/network.h
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_heap.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-04-05 实现 ZR Debugger V1 Architecture，主路线为 launch-under-debug，先支持 interp + binary
  - user: 2026-06-20 参照 docs/plans/debug 分阶段优化 debug 调试能力
  - docs/plans/debug/01-core-hook-fixes.md
  - lua/src/ldblib.c
  - lua/cpython/Lib/bdb.py
  - lua/cpython/Lib/pdb.py
  - lua/jdk/src/jdk.jshell/share/classes/jdk/jshell/execution/JdiInitiator.java
  - lua/mono-6.12.0.199/mono/mini/debugger-agent.c
  - lua/mono-6.12.0.199/mono/metadata/attach.c
tests:
  - tests/debug/test_debug_metadata.c
  - tests/debug/test_debug_hook_core.c
  - tests/debug/test_debug_snapshot_contracts.c
  - tests/debug/test_debug_step_edges.c
  - tests/debug/test_debug_truncation.c
  - tests/debug/test_debug_threads.c
  - tests/debug/test_debug_data_breakpoint.c
  - tests/profile/test_profile_deterministic.c
  - tests/profile/test_coverage.c
  - tests/debug/test_disassemble.c
  - tests/debug/test_heap_summary.c
  - tests/debug/test_debug_trace.c
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/debug/test_debug_agent.c
  - tests/debug/test_debug_agent_protocol.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_debug_e2e.c
  - tests/cli/test_cli_import_basic_fixture.c
  - tests/acceptance/2026-06-20-debug-phase1-core-hooks.md
  - tests/acceptance/2026-06-22-debug-phase5-dap-snapshot.md
  - tests/acceptance/2026-06-22-debug-phase5-step-edges.md
  - tests/acceptance/2026-06-22-debug-phase5-truncation.md
  - tests/acceptance/2026-06-22-debug-phase5-threads.md
  - tests/acceptance/2026-06-22-debug-phase5-data-breakpoint.md
  - tests/acceptance/2026-06-22-debug-phase6-profile.md
  - tests/acceptance/2026-06-22-debug-phase6-coverage.md
  - tests/acceptance/2026-06-22-debug-phase6-disassembly.md
  - tests/acceptance/2026-06-22-debug-phase6-heap-summary.md
  - tests/CMakeLists.txt
doc_type: module-detail
---

# ZR Debugger V1 Launch Workflow

## Scope

`ZR Debugger V1` 固定采用 `launch-under-debug` 路线，不把 `ptrace`、`gdb/lldb` 风格的外部进程控制作为主模型。运行时会在同一个 VM 进程内启动一个调试 agent，再通过本地 loopback TCP 暴露只读调试会话。

当前交付边界如下：

- 支持 `interp` 和 `binary` 两条 VM 执行路径。
- CLI 暴露 `--debug`、`--debug-address`、`--debug-wait`、`--debug-print-endpoint`。
- CLI 也暴露测量类工具开关 `--profile[=out]`、`--coverage[=out]`、`--dump-bytecode <out>` 与 `--heap-summary[=out]`，默认关闭或按需输出。
- 调试协议固定为 `zrdbg/1`，消息形状是 JSON-RPC 风格。
- 支持断点、data breakpoint/watchpoint、pause、`stepIn`、`stepOver`、`stepOut`、`threads`、stack/scopes/variables、uncaught exception stop、terminated event。
- 主仓只承诺 `interp` / `binary`；历史 AOT 调试需求已随 `zr_vm_aot/` 分离出主链路。

## Module Split

### `zr_vm_lib_debug`

`zr_vm_lib_debug` 是调试会话层，不把 socket、listener 或 framing 逻辑塞进 `zr_vm_core`。它负责：

- `ZrDebugAgent` 生命周期
- breakpoint 表和 source line 解析
- safepoint pause loop
- step controller
- thread registry and per-thread state routing
- stack / scopes / variables 只读快照
- safe evaluate 表达式解析、只读求值和诊断归因
- 成功 `evaluate` 结果的 parser semantic fact 摘要桥接
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

内核 hook 路径现在也有公开的 `ZrCore_Debug_SetHook` / `GetHook` / `GetHookMask` / `GetHookCount`。`LINE` 与 `COUNT` 在 `TraceExecution` 中独立处理：LINE 仍按函数+源码行去重，COUNT 则在每条被 trace 的 instruction 上递减，归零后触发 `ZR_DEBUG_HOOK_EVENT_COUNT` 并重置周期；`count=0` 会禁用 COUNT 位。instruction trap 只对 LINE/COUNT 传播到当前 VM callInfo 链，CALL/RETURN-only hook 继续走函数调用/返回路径，不额外强制每条指令进入慢路径。

同一层还提供 `SZrDebugActivation`、`ZrCore_Debug_GetStack(level, ...)` 和 `ZrCore_Debug_GetInfo(..., type, ...)`。调用方先按层级取得 activation，再用 `EZrDebugInfoType` 位掩码选择 source、line、name、closure、tail-call、return-transfer 或 push-function 字段；旧 `ZrCore_DebugInfo_Get` 保留为 level 0 兼容包装。DAP snapshot 现在也走这条内核自省路径：stackTrace 使用 `GetStack`/`GetInfo`，locals/arguments 使用 `GetLocal`，closure preview 使用 `GetUpvalue`，避免调试代理继续复制 raw callInfo、frame slot 和 upvalue value 遍历逻辑。

错误诊断复用同一组栈自省 API。`ZrCore_Debug_Traceback(...)` 会通过 `GetStack`/`GetInfo` 生成截断安全的文本回溯；异常规范化在 unwind 前把它保存为 `stack` 字段，CLI 未捕获错误输出优先打印该文本，debug agent 异常停止事件转发为 `exceptionStack`，VS Code DAP adapter 再通过 `exceptionInfo` 与 stderr output 暴露给客户端。

observer 的 stop reason 优先级固定为：

1. uncaught exception
2. user pause
3. entry stop / wait-for-client
4. breakpoint
5. data breakpoint / watchpoint
6. step

step 语义按“源码位置推进”而不是“同一行内任意下一条 instruction”判停，所以 `stepOver` 不会在同一源码行内来回抖动。step controller 会记录发起 step 时的 call frame 身份：`stepOver` 会跨过更深的子调用，也会把同深度但不同逻辑帧的 tail-call callee 当作被跨过的调用处理，而不是因为帧深度相同就停进 callee；递归同一源码行上的子调用如果命中原断点，也会在 step-over 完成前被推迟。`stepIn` 遇到没有可见源码行的 native 函数时自然表现为 step-over；`stepOut` 在异常 unwind 期间如果目标帧消失，会等到父帧下一个可见停靠点，而不是永久等待原帧恢复。

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

## Data Breakpoints / Watchpoints

`initialize` 会报告 `supportsDataBreakpoints`。客户端可先展开 scope，再对 Locals 或 Closures 中的变量调用 `dataBreakpointInfo`；调试代理会为局部变量和 upvalue 生成稳定 `dataId`，随后 `setDataBreakpoints` 安装 watch 列表。`write` 和 `readWrite` 访问类型当前都按“值变化即停”处理；未给访问类型时默认为 `write`。

这是软件 watchpoint：agent 只在存在 watch 时进入检查路径，并在 LINE/COUNT hook safepoint 上逐个比对被 watch 槽位的当前值和上一份快照。命中后 stopped event 使用 `reason=dataBreakpoint`，同时携带 `dataId`、`description` 和 `threadId`；agent 会更新该 watch 的上一份快照，避免同一值变化在后续 safepoint 立即重复触发。

当前初版只支持局部变量与 upvalue。对象字段、数组元素和其它 child value 会返回不可持久化的 `dataBreakpointInfo`，后续需要更稳定的对象字段 identity 后再扩展。该实现成本随 watch 数线性增长，调试文档和计划验收都按这个边界记录。

## Profiling

`zr_vm_lib_debug` 提供独立 profiler，不把测量状态塞进 `zr_vm_core`。profiler 通过公开的 CALL/RETURN hook 记录每个函数的调用次数、返回次数、累计耗时和 self time；同时可通过 COUNT hook 每 N 条指令采样当前函数+源码行，聚合成热点桶。内核只继续提供 hook 事件和栈/函数身份。

CLI 的 `--profile` 在正常 project run 上启用 profiler，运行成功后输出文本报告；`--profile=<out>` 写入指定文件，不带路径时写到 stdout。报告头为：

```text
ZR_PROFILE deterministic
calls returns total_ns self_ns function source
```

当采样桶非空时，report 会追加：

```text
ZR_PROFILE samples period=1 total=<N>
samples line function source
```

`--profile` 与 `--debug` 当前互斥，因为 debug agent 已经占用 trace observer / hook 组合，两个实时控制面同时开启会让停靠语义和 hook 生命周期变得不确定。profiling 默认关闭；开启后会在 CALL/RETURN 和 COUNT hook 路径进入测量代码，允许显著变慢。

## Coverage

`zr_vm_core` 提供 `ZrCore_Debug_GetActiveLines(function, outLines, cap)`，从函数的 `executionLocationInfoList` 枚举可执行源行，作为 coverage denominator。`zr_vm_lib_debug` 的 coverage recorder 在执行前注册 entry function tree，先写入所有 executable line，再通过 LINE hook 把实际命中的 `(function,line)` 标记为 executed。

CLI 的 `--coverage` 在正常 project run 上启用 coverage，运行成功后输出文本报告；`--coverage=<out>` 写入指定文件，不带路径时写到 stdout。报告头为：

```text
ZR_COVERAGE lines
source line executable executed function
```

普通 interp project 在 coverage 模式下会显式加载 entry function，而不是走隐藏 entry 的 library shortcut，这样 coverage 能在执行前注册函数树并区分“可执行但未覆盖”和“不可执行”。`--coverage` 与 `--debug`、`--profile` 当前互斥，因为三者都需要拥有或组合 debug hook/control-plane 生命周期。coverage 默认关闭；开启后会在 LINE hook 路径进入测量代码，允许显著变慢。

## Disassembly

`zr_vm_core` 提供 `ZrCore_Debug_DisassembleFunction(state, function, FILE*)`，用于开发期和教学场景检查 codegen 输出。反汇编从 `SZrFunction::instructionsList` 逐条打印 instruction，opcode name 复用 `ZrCore_Profile_InstructionName` 的统一 opcode 表，源码行通过函数 line table 解析。

输出格式固定为文本：

```text
ZR_DISASSEMBLY function <name> source <source> instructions <N>
offset opcode operands
0000 <OPCODE> u8=... u16=... i32=... raw=0x... ; line <N>
```

CLI 的 `--dump-bytecode <out>` 在 project run 路径上启用这条能力。runtime 会在加载 entry function 后、执行用户代码前写入反汇编文件，然后继续原本的 interp/binary/debug 执行路径。这样 dump 本身不改变程序结果，也能在运行失败前保留 entry bytecode。该开关要求显式输出路径；compile-only、REPL、inline、help 和 version 路径都会被解析层拒绝。

## Heap Summary

`zr_vm_core` 提供 `ZrCore_Debug_HeapSummary(state, FILE*)`，用于退出时生成轻量内存快照。实现拆分在 `debug_heap.c`，避免继续扩大核心调试主文件。该摘要只读取现有 GC/object 元数据，不做完整对象图遍历或引用路径分析。

输出格式固定为文本：

```text
ZR_HEAP_SUMMARY objects total=<N> bytes=<N> managed=<N> debt=<N>
type string count <N> bytes <N>
type object count <N> bytes <N>
prototypes tracked <N>
gc regions total=<N> young=<N> old=<N> major_free=<N> ...
gc bytes young=<N> old=<N> major_free=<N> major_limit=<N>
gc collections minor=<N> major=<N> emergency=<N> promoted=<N> collected=<N>
```

CLI 的 `--heap-summary` 在成功 project run 后把摘要写到 stdout；`--heap-summary=<out>` 写入指定文件。该开关不占用 debug hook，因此可独立于 profiling/coverage 的 hook 生命周期；compile-only、REPL、inline、help 和 version 路径都会被解析层拒绝。

## CLI Contract

### Supported commands

- `zr_vm_cli <project.zrp> --debug`
- `zr_vm_cli <project.zrp> --debug --debug-address 127.0.0.1:4711`
- `zr_vm_cli <project.zrp> --debug --debug-wait --debug-print-endpoint`
- `zr_vm_cli --compile <project.zrp> --run --debug --execution-mode binary`
- `zr_vm_cli <project.zrp> --profile`
- `zr_vm_cli <project.zrp> --profile=profile.txt`
- `zr_vm_cli <project.zrp> --coverage`
- `zr_vm_cli <project.zrp> --coverage=coverage.txt`
- `zr_vm_cli <project.zrp> --dump-bytecode bytecode.txt`
- `zr_vm_cli --compile <project.zrp> --run --dump-bytecode bytecode.txt`
- `zr_vm_cli <project.zrp> --heap-summary`
- `zr_vm_cli <project.zrp> --heap-summary=heap.txt`
- `zr_vm_cli --compile <project.zrp> --run --heap-summary=heap.txt`

### Runtime behavior

- `--debug` 但不带 `--debug-wait`
  - 立即启动 agent 并继续执行用户代码
  - 如果客户端够快，可以在运行期间 attach 到当前会话
- `--debug-wait`
  - 在 entry safepoint 停住
  - 等待客户端连接并初始化后再继续
- `--debug-print-endpoint`
  - 打印实际绑定的 loopback endpoint，尤其适合 `port=0` 自动分配场景
- `--profile[=out]`
  - 在非 debug 运行路径启用确定性 profiling
  - 同时启用 COUNT hook 采样并在 report 中追加热点桶
  - `out` 缺省时写 stdout；使用 `--profile=<out>` 时写入该文件
  - 与 `--debug`、`--compile` only、`--no-run`、`--help`、`--version` 互斥
- `--coverage[=out]`
  - 在非 debug/profile 运行路径启用 line coverage
  - 执行前注册 entry function tree 的 activelines denominator，执行中通过 LINE hook 标记 executed lines
  - `out` 缺省时写 stdout；使用 `--coverage=<out>` 时写入该文件
  - 与 `--debug`、`--profile`、`--compile` only、`--no-run`、`--help`、`--version` 互斥
- `--dump-bytecode <out>`
  - 在 entry function 加载后、执行前写出反汇编文本
  - 支持 interp、binary、debug project run，以及 `--compile ... --run`
  - 需要显式输出路径；与 compile-only、REPL、inline、help、version 路径互斥
- `--heap-summary[=out]`
  - 在 project run 成功结束后输出轻量 heap/GC 摘要
  - `out` 缺省时写 stdout；使用 `--heap-summary=<out>` 时写入该文件
  - 支持 interp、binary、debug project run，以及 `--compile ... --run`
  - 不占用 hook；与 compile-only、REPL、inline、help、version 路径互斥

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
- `threads`
- `dataBreakpointInfo`
- `setDataBreakpoints`
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

条件表达式遵循同一类安全跳过合同：先求值 condition，true/false 两个 branch 都会被语法消费，但只有被选中的 branch 会解析局部变量、读取成员/索引或执行计算。`true ? 1 : missingLocal` 和 `false ? missingLocal : 2` 可以作为 evaluate 或条件断点表达式通过；缺少 `:`、缺少 `?` 后的 consequent branch，或缺少 `:` 后的 alternate branch 时，必须报告具体缺失结构、原因和建议。这里仍是只读调试表达式子集，不承诺完整语言求值。

这条消息会同时覆盖 VS Code/DAP adapter 未来会转发的 `evaluate` 失败响应，以及条件断点失败时写到 `stderr` 的输出事件。当前实现仍是调试时的安全求值器，不承诺完整语言表达式执行，也不替代 parser/LSP 的编译期语义事实层。

## Snapshot Model

调试代理维护一个 `SZrState -> threadId` 注册表。主执行体稳定映射为 `threadId=1` / `main`，
`threads` 请求返回当前已注册执行体；`stopped` 与 `continued` 事件带 `threadId`。
`stackTrace`、`scopes`、`variables` 与 `evaluate` 都接受可选 `threadId`，缺省使用最近一次
stop 所属 thread。读取时代理只在请求期间把快照上下文切到目标 state，读完立即恢复。
当前 task runtime 仍复用主 `SZrState`，所以本阶段完成的是 DAP 线程枚举和按 thread 快照路由
MVP；跨 task 同步 step 与 task runtime 侧额外 state 注册留后续。

variables 目前是只读快照，不支持变量写回或带副作用求值。scope 划分固定为：

- `Locals`
- `Closures`，只有当前函数真的有闭包捕获时才出现
- `Globals`

`Globals` 目前提供的是 runtime 能稳定只读暴露的核心入口：

- `zr`
- `loadedModules`

`variables` 响应中的每个 value preview 和 `evaluate` 响应都会带 `namedVariables` / `indexedVariables`。这两个字段是协议边界的子形状提示，方便 VS Code/DAP adapter 在不先展开 handle 的情况下判断 named/indexed children。普通 object 的 `namedVariables` 与实际展开计数一致：可见字段加上 `$prototype` 等 synthetic entry，隐藏 `__zr_` 字段不计入；直接 array/index window 使用 `indexedVariables`。

所有固定缓冲文本预览都会显式标记截断：超长普通文本保留前缀并追加 `...[+N]`，路径/源码名优先保留尾部并在前缀放置同样的省略计数。长字符串 value/evaluate preview 不放大 `ZR_DEBUG_TEXT_CAPACITY`；当完整字符串达到预览缓冲上限时，preview 保留带标记的短文本，同时返回 `variablesReference` 和按 64-byte 分块计算的 `indexedVariables`。客户端随后用 `variables(start,count)` 惰性读取完整字符串片段，片段名形如 `[64..128)`，片段自身不再带截断标记。

帧内变量读取的实际 name/value 来源是核心 debug API，而不是 DAP 代理直接读 callInfo 和 value slot。`debug_snapshot.c` 仍会读取函数本地元数据来维持 `Arguments` / `Locals` 分组、活跃区间和 inline union preview，但最终的局部变量名和值由 `ZrCore_Debug_GetLocal` 提供；闭包变量预览由 `ZrCore_Debug_GetUpvalue` 提供。这样 Phase 2 的局部变量/upvalue 语义修复会自动覆盖 DAP、evaluate 和变量树。

同一层现在也输出 `semanticSummary`。变量和 value preview 上的内容仍是 runtime snapshot 的短摘要，供 adapter 在变量树、watch、evaluate 面板中直接展示：布尔值显示为 `logical true/false`，整数显示为 `integer <value>`，浮点显示为 `number <value>`，可展开引用显示为 `expandable <type>, named N, indexed M`，带 ownership metadata 的值会追加 `ownership <kind>`。成功的 `evaluate` 结果会在这个 runtime 摘要后追加 parser/type-inference 能在同一个表达式源码上证明的事实，例如 `evaluate("1 + 2")` 会报告 `integer 3, expression binary exact, constant 3, range 3..3, unsigned range 3..3`，`evaluate("true || false")` 会报告 runtime `logical true` 以及 parser-owned `short-circuits` / `unreachable because short-circuit skips evaluation`，`evaluate("true ? 1 : 2")` 会报告被常量条件跳过的 branch reachability，字符串 literal 常量会被转义成一行摘要，避免 quote、backslash、换行、tab 或其它控制字节破坏 adapter 展示。已存在的 parser-owned reference facts 也会进入这条摘要；例如 `evaluate("zr[1]")` 的 parser fact pass 会追加 `reference member access`，而 runtime `referenceSummary` 仍保留实际读取来源 `global zr, index access`。assignment 语义摘要现在也会保留直接 write facts：`globalSeed = 3` 追加 `reference write globalSeed`，`seed.value = 3` 与 `seed[index] = 4` 追加 `reference member write ...`，避免 write/member-write 被同节点 read/access fact 遮蔽。当 evaluate 运行在暂停帧上时，Debug 会先把可见 frame slots 以 runtime value type 注册进临时 parser type environment；同一个 replay 也注册稳定 Debug globals，所以 `evaluate("inside + 1")` 的 `semanticSummary` 能追加 parser-owned `reference read inside`，`evaluate("zr")` 能追加 parser-owned `reference read zr`。当 Debug agent 带有编译入口函数时，semantic-summary fact bridge 还会注册入口函数的 top-level callable metadata；直接语义摘要查询 `pick(1 + 2)` 可追加 parser-owned `call pick args=1`、`reference call pick` 和实参的 `constant 3` / `range 3..3` / `unsigned range 3..3`，`seed[index]` 这类成员表达式摘要可追加 parser-owned `member index`、`reference member access index` 和索引 token 的 `reference read index`，但 safe evaluator 仍按只读策略拒绝实际函数调用或写入。Debug 只消费共享 semantic fact layer，不在 safe evaluator 中重新实现表达式类型、常量折叠、数值范围、unsigned range payload、分支语义推断、引用分类、调用/成员 payload 推断或字符串常量转义策略。

同一协议层还输出 `referenceSummary`。它是 adapter-facing 的引用来源提示，不是完整的 parser reference fact：稳定可解析的顶层 scope 引用会显示为 `argument <name>`、`local <name>`、`closure <name>` 或 `global <name>`；`evaluate` 会累计 safe evaluator 实际解析并读取的 identifier，例如 `evaluate("zr")` 返回 `global zr`，`evaluate("inside + 1")` 返回 `local inside` 或 MSVC fixture 下的 `argument inside`，`evaluate("zr ? loadedModules : missingLocal")` 返回 `global zr, global loadedModules`。debug index-window evaluate 会沿用 base expression 的实际读取归因，例如 `evaluate("zr[1..3]")` 在 indexed-window `semanticSummary` 旁返回 `referenceSummary: global zr`。短路 RHS 和未选中的 `?:` branch 只被语法消费，不会因为里面出现可解析 identifier 就填充 `referenceSummary`；`true || inside`、`false ? inside : 2` 和前述 `missingLocal` 都不会被误报。计算结果、调试器合成入口、展开对象子字段和 member declaration 仍不会伪造 parser source facts。

这符合 v1 的安全边界，避免把脚本侧的 pause/breakpoint 控制重新暴露回脚本运行时本身。

### Union Values

`union` values are exposed as first-class debug shapes instead of generic hidden-object internals. The debug layer recognizes two carriers:

- runtime constructor carrier objects with hidden `__zr_unionType`, `__zr_unionVariant`, and `__zr_unionPayloadN` fields
- typed inline union slots stored in a function frame according to `SZrFunctionTypeLayout`

Both forms preview as `<union Type.Variant>`. The `type` field reports the union type name when metadata is available, and `semanticSummary` reports `union Type.Variant, named N`. Expanding the value returns a synthetic `variant` child, followed by payload children in declaration order. Tuple payloads use the declared payload names when present, otherwise `payload0`, `payload1`, and so on. Struct payloads use their field names. Hidden carrier storage names remain available only to the implementation and are not presented as the primary user-facing children.

Safe `evaluate` resolves the same synthetic members. Examples:

- `circle.variant` returns `Circle`
- `circle.radius` returns the tuple payload value
- `rect.width + rect.height` reads struct payload fields and evaluates through the normal numeric safe-evaluate path

Inline union frame slots are materialized to an ignored temporary carrier object only for safe-evaluate member access. This keeps the existing member-access evaluator reusable while preserving the source frame as read-only.

## Detached Backends

主仓调试合同只覆盖 `interp` 和 `binary`。如果后续要为 `zr_vm_aot/` 归档里的独立后端恢复调试能力，必须走新的独立设计，而不是在当前 v1 合同里隐式复活旧入口。

## Validation Coverage

当前实现对应的回归面：

- `tests/cli/test_cli_args.c`
  - debug/profile/coverage/dump-bytecode/heap-summary flags 解析、依赖关系和 run-path 约束
- `tests/cli/test_cli_import_basic_fixture.c`
  - 复制 `tests/fixtures/projects/import_basic` 到临时目录后重编译，断言 `compiled=2 skipped=0 removed=0`
  - binary run 输出 `hello from import`，防止 exported callable import 签名再次阻塞 debug launch 基线
  - 在启用 debug/profile 支持的测试构建里运行 `--profile=<out>` 等价路径，断言生成 `ZR_PROFILE deterministic` report
  - 同一 report 断言包含 `ZR_PROFILE samples` 与 `samples line function source`
  - 在启用 debug/coverage 支持的测试构建里运行 `--coverage=<out>` 等价路径，断言生成 `ZR_COVERAGE lines` report
  - 运行 `--dump-bytecode <out>` 等价路径，断言生成 `ZR_DISASSEMBLY function` report，且包含 opcode name 和 `; line` 注释
  - 运行 `--heap-summary=<out>` 等价路径，断言生成 `ZR_HEAP_SUMMARY objects` report，且保留原程序输出
- `tests/debug/test_disassemble.c`
  - `ZrCore_Debug_DisassembleFunction` 输出 `ZR_DISASSEMBLY function` header 和 `offset opcode operands` 列头
  - 反汇编行数等于 `function->instructionsLength`
  - 输出包含 `FUNCTION_RETURN` opcode name 和源码 `; line N` 注释
- `tests/debug/test_heap_summary.c`
  - `ZrCore_Debug_HeapSummary` 输出 `ZR_HEAP_SUMMARY objects` header
  - 摘要包含 string/object/thread 等 type bucket
  - 摘要包含 GC region 和 collection counters
- `tests/profile/test_profile_deterministic.c`
  - 已知脚本调用 `mid` 两次、`leaf` 四次，断言 deterministic profiler 的 call/return 计数精确
  - 验证 total time 不小于 self time，并在 `Stop` 后恢复原 hook mask
  - COUNT hook 采样模式以 period=1 运行循环脚本，断言采样桶记录到 `leaf` 函数源码行
- `tests/profile/test_coverage.c`
  - `ZrCore_Debug_GetActiveLines` 从分支函数提取唯一可执行行，包含 taken 和 untaken return line
  - coverage recorder 注册函数树后运行部分分支脚本，断言 taken return line `executed=true`，untaken return line `executable=true` 且 `executed=false`
  - `Stop` 后恢复原 hook mask
- `tests/cli/test_cli_debug_e2e.c`
  - launch-under-debug 在 `import_basic` fixture 上命中入口断点，不因 import signature mismatch 先停到 exception
  - 未捕获错误输出包含统一 traceback 文本，覆盖 `leaf`/`middle`/`root` 多帧源码栈
- `tests/debug/test_debug_traceback.c`
  - `ZrCore_Debug_Traceback` 覆盖多帧脚本栈、native/script/native 混合帧、深栈折叠、小 buffer NUL 截断，以及异常对象 `stack` 文本字段
- `tests/debug/test_debug_trace.c`
  - interpreter trace hook、line event 和 debug info 获取
- `tests/debug/test_debug_snapshot_contracts.c`
  - source contract 覆盖 DAP stack snapshot 必须包含 `ZrCore_Debug_GetStack` / `ZrCore_Debug_GetInfo`，并禁止在 stack 查找路径回退到 raw `callInfoList` 遍历
  - source contract 覆盖 DAP variables snapshot 必须包含 `ZrCore_Debug_GetLocal` / `ZrCore_Debug_GetUpvalue`，并禁止 locals/arguments 主路径直接用 raw frame slot 或 closure value pointer 替代核心自省 API
- `tests/debug/test_debug_step_edges.c`
  - tail-call `stepOver` 不停进 tail-called callee，而是在调用方下一个可见位置停靠
  - native call `stepIn` 表现为 step-over，停在 native 返回后的脚本行
  - exception unwind 中的 `stepOut` 不永久等待被弹出的目标帧，回到父帧可见位置
  - recursive same-line `stepOver` 不被子调用里同一断点抢先停下
- `tests/debug/test_debug_truncation.c`
  - 超长普通文本预览包含 `...[+N]` 省略标记
  - 超长路径预览保留文件名尾部
  - tiny buffer 仍保证 NUL 终止
  - 长字符串 value preview 标记截断
  - 长字符串 runtime value/evaluate preview 暴露 `variablesReference`，并通过 `variables(start,count)` 分页读取 64-byte 字符串片段
- `tests/debug/test_debug_threads.c`
  - `initialize` 返回 `supportsThreads`
  - `threads` 请求枚举主执行体并稳定返回 `threadId=1` / `main`
  - `stopped` event 标注触发 stop 的 `threadId`
  - `stackTrace`、`scopes`、`variables` 携带 `threadId` 时路由到对应 state，并在 result/frame/scope 层返回 `threadId`
- `tests/debug/test_debug_data_breakpoint.c`
  - `initialize` 返回 `supportsDataBreakpoints`
  - Locals scope 同时返回 DAP `variablesReference` 别名，可用于 `dataBreakpointInfo`
  - `dataBreakpointInfo` 为局部变量和 upvalue 返回稳定 `dataId`
  - `setDataBreakpoints` 安装局部变量/upvalue watch
  - 值变化时 stopped event 使用 `reason=dataBreakpoint`，并携带 `dataId`、`description`、`threadId`
- `tests/debug/test_debug_hook_core.c`
  - COUNT hook 以 `count=1` 覆盖每条已 trace instruction，触发数与 `function->instructionsLength` 一致
  - `ZrCore_Debug_SetHook(NULL,0,0)` 禁用 hook，并验证 hook/mask/count 清零
  - LINE|COUNT 同时启用时两类事件都到达，LINE event 保持按源码行去重
  - `ZrCore_Debug_GetStack` / `ZrCore_Debug_GetInfo` 解析嵌套 frame，并验证 type mask 只填请求字段
- `tests/debug/test_debug_metadata.c`
  - `.zro` source identity/debug metadata roundtrip
- `tests/debug/test_debug_expression_diagnostics.c`
  - public `ZrDebug_Evaluate` 缺右操作数时返回具体问题、原因和建议
  - public `ZrDebug_Evaluate` 通过共享 safe-evaluate parser 对 `&&` 缺右操作数给出同一类诊断
  - `1 + 2` 的成功 `evaluate` 验证 `semanticSummary` 中的 parser-owned numeric fact 同时保留 `range 3..3` 和 `unsigned range 3..3`
  - 字符串 literal 的成功 `evaluate` 验证 `semanticSummary` 中的 parser-owned `constant` 会转义 quote、backslash、`\n` 和 `\t`
  - `zr[1]` 的成功 `evaluate` 验证 `semanticSummary` 会追加 parser-owned computed member `reference member access`，同时 `referenceSummary` 仍报告实际 runtime 读取 `global zr, index access`
  - assignment semantic-summary bridge 验证 `globalSeed = 3`、`seed.value = 3` 和 `seed[index] = 4` 会追加 parser-owned write/member-write reference facts
  - compiled entry function 的 direct semantic-summary bridge 验证 `pick(1 + 2)` 会从 top-level callable metadata 追加 parser-owned `call pick args=1` / `reference call pick`，并保留实参的 `expression binary exact` / `constant 3` / `range 3..3`
  - direct semantic-summary bridge 验证 `seed[index]` 会输出 parser-owned `member index`、`reference member access index` 和索引 token 的 `reference read index`
  - `true || missingLocal` 和 `false && missingLocal` 验证确定性短路不会解析被跳过的局部变量
  - `(1 < 2) && (3 < 4)`、`!(1 < 2)`、`(1 < 2) || missingLocal` 和 `(2 < 1) && missingLocal` 验证 Debug safe evaluate 已经能把比较结果组合进逻辑表达式和条件断点短路
  - `true ? 1 : missingLocal` 和 `false ? missingLocal : 2` 验证条件表达式只解析被选中分支的数据路径，同时仍消费跳过分支语法
  - `zr ? loadedModules : missingLocal` 验证 `referenceSummary` 累计 condition 和选中分支的实际读取，同时不把跳过分支的 `missingLocal` 误报为引用
  - `true ? : 2` 和 `true ? 1 :` 验证条件表达式缺少 consequent 或 alternate branch 时返回 branch-specific cause/suggestion 诊断
  - `"text" + 1` 和 `1 / 0` 验证 safe evaluate 的数值语义错误也带具体原因和建议
  - `(true || false` 验证 grouped expression 缺少 `)` 时返回具体问题、原因和建议
  - `"open` 和 `"bad\q"` 验证 string literal 结构错误不会退化成 generic expected-expression 或 unsupported-escape 短句
  - `sideEffect()` 验证函数调用在 identifier lookup 前被识别为安全求值策略错误，并返回原因和建议
  - `true = false` 与 `local = 1` 验证赋值语法在 trailing-token fallback 或 identifier lookup 前被识别为 read-only 策略错误，并返回原因和建议
- `tests/debug/test_debug_variable_child_shape.c`
  - `variables` 中的 `zr` value preview 报告与展开结果一致的 `namedVariables` / `indexedVariables`
  - `evaluate("zr")` 返回同一 child-shape metadata，保证 adapter 可以对 evaluate result 做同样的展开判断
  - `variables` 和 `evaluate` 响应都返回 `semanticSummary`；覆盖 `expandable ... named ...`、`logical true`，以及 `evaluate("1 + 2")` 同时报告 runtime `integer 3` 和 parser-owned `expression binary exact` / `constant 3` / `range 3..3`
  - `evaluate("true || false")` 和 `evaluate("true ? 1 : 2")` 验证成功 evaluate 结果会追加 parser-owned logical flow 和 skipped-branch reachability facts，而不是只显示 runtime boolean/integer 摘要
  - `variables` 中的 `zr` 和 `evaluate("zr")` 都返回 `referenceSummary: global zr`；`evaluate("zr")` 的 `semanticSummary` 还会追加 parser-owned `reference read zr`
  - `evaluate("inside + 1")` 返回计算值 `8`，把 `referenceSummary` 归因到当前帧里的 `inside`（Linux/Clang fixture 为 `local inside`，MSVC fixture 为 `argument inside`），并在 `semanticSummary` 里追加 parser-owned `reference read inside`
  - `evaluate("zr[1..3]")` 验证 debug index window 同时报告 indexed-window `semanticSummary` 和 base expression 的 `referenceSummary: global zr`
  - `evaluate("true || inside")` 与 `evaluate("false ? inside : 2")` 验证被短路或未选中分支里的 `inside` 不会生成 `referenceSummary`
  - `variables` 中的 `Shape` inline union locals show `<union Shape.Circle>` / `<union Shape.Rect>` previews with named child counts
  - expanding a union variable returns `variant` plus declaration-order payload fields such as `radius`, `width`, and `height`
  - `evaluate("circle.variant")`, `evaluate("circle.radius")`, and `evaluate("rect.width + rect.height")` verify synthetic union member access for both tuple and struct variants
- `tests/debug/test_debug_agent.c`
  - invalid token rejection
  - loopback JSON-RPC handshake
  - entry stop
  - line breakpoint hit
  - `stepOver`
  - stackTrace / scopes / variables
  - uncaught exception stopped event 携带 `exceptionStack`，供 DAP `exceptionInfo` 使用
  - evaluate 与条件断点/日志断点主流程
  - disconnect-continue policy
- `tests/debug/test_debug_agent_protocol.c`
  - `zrdbg/1` capability、pause、disconnect、reconnect 和协议鲁棒性

手工 smoke 还覆盖了：

- `zr_vm_cli <hello_world.zrp> --debug --debug-print-endpoint`
- `zr_vm_cli --compile <hello_world.zrp> --run --debug --execution-mode binary --debug-print-endpoint`

这两条都必须继续打印 endpoint 并把程序跑完，证明普通运行路径和 debug 运行路径能共存。
