---
doc_type: plan-index
plan_sources:
  - user: 2026-06-20 参照 lua/ 下各语言运行时源码，完善 zr 的 debug 能力、补齐缺失、修复 bug
reference_runtimes:
  - lua/src/ldebug.c            # Lua debug 核心：hook、getstack、getinfo、getlocal、行号表
  - lua/src/ldblib.c            # Lua debug 标准库（脚本层 debug.*）
  - lua/src/ldo.c               # luaD_hook：hook 调用机制
  - lua/src/lvm.c               # VM 主循环 trap 检查接入点
  - lua/src/lobject.h           # Proto/LocVar/AbsLineInfo 调试信息结构
  - lua/src/lauxlib.c           # luaL_traceback：栈回溯字符串
  - lua/QuickJS-master/quickjs.c # pc2line 编码、build_backtrace
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_heap.c
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/debug.c
  - zr_vm_lib_debug/src/debug_protocol.c
  - zr_vm_lib_debug/src/debug_snapshot.c
  - zr_vm_lib_debug/src/debug_eval.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_language_server_extension/src/debug/dapSession.ts
  - tests/debug/
  - tests/cli/test_cli_debug_e2e.c
---

# zr Debug 能力完善计划

## 0. 目标与范围

参照 `lua/` 目录下成熟运行时（Lua、QuickJS、CPython、.NET/Mono、JDK）的调试设施，
对 zr 虚拟机的 debug 能力做一次系统化查漏补缺：**修复已存在的 bug、补齐内核缺失的
调试 API、向脚本层和 DAP 客户端开放更多能力**。

设计取向：

- **内核保持 Lua 式轻量 hook 模型**（事件掩码 + 单 hook 回调），与 zr 现有
  `FZrDebugHook` / `FZrDebugTraceObserver` 双层机制吻合。
- **DAP 代理（`zr_vm_lib_debug`）继续作为面向 IDE 的高层协议层**，复用内核新增的自省
  API，避免各自重复实现栈/变量遍历逻辑。
- **不追求企业级特性**（time-travel、edit-and-continue、混合本机调试），列为「明确不做」。

## 1. 现状盘点（已实现）

每一项均对应真实代码，已在调查中核实。

### 1.1 内核 hook 与 trace（`zr_vm_core/debug.{h,c}`）

| 能力 | 实现位置 | 状态 |
|------|----------|------|
| hook 事件枚举 CALL/RETURN/LINE/COUNT | `debug.h:15-31` | 枚举齐全，COUNT **未接线**（见 §2） |
| `FZrDebugHook` 单 hook 回调 | `debug.h:89`, `state.h:93` | 已定义，**无公开 setter** |
| LINE hook 触发 | `debug.c:157-171` `ZrCore_Debug_TraceExecution` | 已实现 |
| CALL hook 触发 | `function.c:2463`, `function.c:3266` | 已实现 |
| RETURN hook 触发 | `function.c:3814` → `ZrCore_Debug_HookReturn` `debug.c:287` | 已实现 |
| hook 重入保护 | `debug.c:256` `allowDebugHook` | 已实现 |
| Trace observer（旁路观察者） | `debug.c:102` `SetTraceObserver` | 已实现，被 DAP 代理使用 |
| VM 主循环 trap 接入 | `execution_dispatch.c:2009` | 已实现 |
| pc → 源行/列查询 | `function.h:95` `SZrFunctionExecutionLocationInfo` + `ZrCore_Exception_FindSourceLine` | 已实现（含列号，优于 Lua） |
| 局部变量调试信息 | `function.h:55` `SZrFunctionLocalVariable` + `ZrCore_Function_GetLocalVariableName` `function.h:600` | 数据齐备，**未暴露自省 API** |
| Prototype/Object 打印 | `debug.c:343/440/521` | 已实现（开发期诊断） |

### 1.2 DAP 调试代理（`zr_vm_lib_debug`）

已实现面向 VS Code 的较完整代理：

- 行/函数/异常断点，含**条件断点、命中计数、logpoint**。
- continue / pause / step in / step out / step over。
- 读取调用栈、作用域（arguments/locals/closures/globals/prototype/statics/exception）、
  分页变量、表达式求值。
- JSON-RPC 协议 `zrdbg/1`，经 loopback TCP 传输。
- VS Code 扩展完整 DAP adapter（`dapSession.ts`）。
- CLI 开关：`--debug` / `--debug-address` / `--debug-wait` / `--debug-print-endpoint`。

## 2. 缺口与 bug 分析

按「先修 bug、再补内核、最后扩协议/工具」排序。

### P0 — 内核既有 bug / 半成品

1. **COUNT hook 是死代码（bug）**。`ZR_DEBUG_HOOK_EVENT_COUNT`、`state.baseDebugHookCount`/
   `debugHookCount`、`ZrStateResetDebugHookCount`（`state.c:63`）全部存在，但
   `ZrCore_Debug_TraceExecution`（`debug.c:119`）**只处理 LINE 掩码**，从不递减计数、从不
   触发 COUNT 事件。指令计数 hook（性能采样/超时/插桩的基础）完全不可用。
2. **`ZrCore_DebugInfo_Get` 忽略 `type` 位掩码（半成品）**。首行 `ZR_UNUSED_PARAMETER(type)`
   （`debug.c:62`），无论请求什么都填充全部字段，且**只读栈顶 callInfo**，缺「按层级取栈帧」。
   与 Lua `lua_getstack(level)` + `lua_getinfo(what)` 契约不符。
3. **`SZrDebugInfo` 的 `// todo`（半成品）**。`debug.h:64` 处缺 Lua 的 `namewhat`
   （global/local/field/method 来源分类），调用栈函数名归类不全。

### P1 — 内核缺失的自省 API

4. **无公开 hook setter**。缺 `lua_sethook` 等价的 `ZrCore_Debug_SetHook(state,hook,mask,count)`，
   embedder/库无法启用内核 hook；现有 hook 基础设施实际不可达（只有 DAP 经 trace observer 旁路）。
5. **无变量自省 API**。缺 `GetStack(level)` / `GetLocal` / `SetLocal` / `GetUpvalue` /
   `SetUpvalue` / `GetUpvalueId`。数据结构已具备，但无「层级+索引/名字+PC 活跃区间」的读写接口。
   DAP 的 `debug_snapshot.c` 自己内联了一份，逻辑无法复用、易与内核语义漂移。

### P2 — 脚本层与错误诊断

6. **无脚本可调用的 `debug` 标准库**。Lua 通过 `debug.getinfo/traceback/getlocal/sethook/...`
   开放调试能力；zr 目前没有任何脚本层 debug 模块。
7. **无人类可读 traceback**。`ZrCore_Debug_RunError`（`debug.c:190`）只生成单行 message 并规范化
   为 Error，未生成 QuickJS `build_backtrace` 式多帧 `file:line: in func` 栈字符串供未捕获错误
   展示 / `debug.traceback` 复用。

### P3 — DAP 代理与工具增强

8. **固定缓冲区静默截断**：`ZR_DEBUG_TEXT_CAPACITY=256` / `ZR_DEBUG_NAME_CAPACITY=128`
   会无声截断长路径/长名字/长值。
9. **task/协程线程不可见**：代理只暴露主线程（thread id 固定 1），zr 有 task/thread 库，多执行体
   调试缺失。
10. **缺 data breakpoint/watchpoint、profiling hook、coverage、反汇编视图、堆/内存检查、GC 事件**。
11. **远程调试仅限 loopback**：`zrdbgClient.ts` 强制 loopback；`auth_token` 字段已留但未启用鉴权远程。

### 明确不做（范围外）

time-travel / 反向执行；edit-and-continue 热替换；本机+托管混合调试；独立 wire protocol。
继续复用现有 `zrdbg/1` + DAP。

## 3. 分阶段实施

| 阶段 | 文档 | 主题 | 依赖 |
|------|------|------|------|
| Phase 1 | [01-core-hook-fixes.md](01-core-hook-fixes.md) | 修 COUNT hook、补 `SetHook`、修 `DebugInfo_Get` 的 type/层级语义 | — |
| Phase 2 | [02-introspection-api.md](02-introspection-api.md) | getstack/getlocal/setlocal/getupvalue 自省 API；补全 `SZrDebugInfo` | P1 |
| Phase 3 | [03-traceback-and-errors.md](03-traceback-and-errors.md) | 多帧 traceback、错误源定位、异常栈渲染 | P2 |
| Phase 4 | [04-script-debug-library.md](04-script-debug-library.md) | 脚本层 `debug` 标准库 | P2 |
| Phase 5 | [05-dap-agent-enhancements.md](05-dap-agent-enhancements.md) | DAP：复用内核 API、修 step 边界、task 线程、截断治理、data breakpoint | P2 |
| Phase 6 | [06-profiling-and-tooling.md](06-profiling-and-tooling.md) | profiling hook、coverage、反汇编、内存检查 | P1 |
| 贯穿 | [07-testing-and-acceptance.md](07-testing-and-acceptance.md) | 测试矩阵与验收标准 | all |

## 状态与产出记录

| 时间 | 阶段 | 状态 | 完成项目 | 后续/阻塞 |
|------|------|------|----------|-----------|
| 2026-06-20 02:05:50 +08:00 | Phase 1 | 核心完成；完整阶段验收阻塞 | 已完成 COUNT hook、公开 hook setter/getter、`GetStack`/`GetInfo` activation API、`DebugInfo_Get` type-mask 兼容包装、LINE/COUNT trap 传播、新增 `debug_hook_core` focused 覆盖，并修复本轮 debug 回归暴露的 semantic-summary assignment write/member-write fact 展示问题 | `tests/cli/test_cli_debug_e2e.c` 的 `debug_wait_hits_import_basic_launch_breakpoint` 当前因 `import_basic` project 的 import signature mismatch 停在 exception；同一 project 直接 CLI 运行也复现，需先恢复 import 基线后才能把 Phase 1 标为完整通过 |
| 2026-06-20 02:22:57 +08:00 | Phase 1 | 补充验证通过；阻塞不变 | WSL clang 与 Windows/MSVC focused debug set 已补跑通过 | full DoD 仍等待 import baseline 修复后重跑 `cli_debug_e2e` |
| 2026-06-20 02:30:05 +08:00 | Phase 1 | 阻塞归因补充 | `import_basic` 重新编译后仍失败；项目 import canonicalization focused 测试 35/0 PASS | 阻塞集中在 exported-callable import fixture 路径，按严格阶段门槛仍不能进入 Phase 2 完整实施 |
| 2026-06-20 04:38:33 +08:00 | Phase 1 | 完成 | 完成核心 hook/API、DebugInfo type mask、`GetStack`/`GetInfo`、Debug semantic-summary write/member-write 与 identifier read fallback；修复 exported callable import summary 刷新，新增 `cli_import_basic_fixture` 防回归，`cli_debug_e2e` 的 import_basic launch breakpoint 门槛恢复通过 | Phase 1 DoD 已满足；进入 Phase 2 前仍建议先处理 `module_init_analysis.c` 后续拆分点，避免在超大文件继续叠加自省 API 无关职责 |
| 2026-06-20 07:59:23 +08:00 | Phase 2 | 核心完成；MSVC 验收通过；WSL 新目标补跑待完成 | 完成 `GetLocal`/`SetLocal`/`GetUpvalue`/`SetUpvalue`/`GetUpvalueId`、`nameWhat` 字段、`debug_introspection` 测试与 DAP closure/upvalue API 复用；修复 byte-frame value slot 与 dense stack overlap 导致的 closure/upvalue 崩溃，`language_debug_gauntlet` 源码解释执行恢复 | Windows/MSVC focused gate 通过；WSL gcc/clang 的新增 `debug_introspection` 可执行目标在旧构建目录未产出且构建多次超时，同机外部 WSL 构建占用中。Phase 3 前需补跑干净 WSL gcc/clang focused gate |
| 2026-06-20 10:52:13 +08:00 | Phase 2 | 完成 | 补齐 WSL gcc/clang 新增 `debug_introspection` 目标构建，并复跑 Phase 2 focused debug gate | WSL gcc：`ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "^(debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` 3/3 PASS；WSL clang：同范围 3/3 PASS。Phase 2 DoD 已满足，可进入 Phase 3 |
| 2026-06-20 11:51:09 +08:00 | Phase 3 | 进行中；核心生成器完成；阶段验收未完成 | 新增 `ZrCore_Debug_Traceback` 统一文本生成器与 `debug_traceback` focused 测试；支持前缀、多帧脚本栈、native/script/native 混合帧、深栈 `skipping` 折叠、小 buffer NUL 截断；异常对象新增 `stack` 文本字段并保留 `stacks` 结构化数组，未捕获异常格式器优先使用统一文本 | 手工 WSL/GCC focused 链接运行 `zr_vm_debug_traceback_test` 4/4 PASS；旧 WSL/GCC 目录正式目标构建两次在 CMake/Ninja glob/regeneration 阶段超时；新 `build/codex-debug-phase3-wsl-gcc` 配置成功，但目标构建因完整依赖编译超时仍未产出正式可执行。剩余：CLI e2e 命令级文本回溯验收、DAP exception detail 验收、正式 MSVC/WSL 目标构建与 CTest |
| 2026-06-20 12:47:18 +08:00 | Phase 3 | 完成；里程碑 A 全矩阵待补跑 | 正式 WSL/GCC CMake 目标 `debug_traceback` 构建并 CTest 通过；补齐 CLI 未捕获错误回溯 e2e，输出包含 `leaf`/`middle`/`root` 源码帧；异常对象 `stack` 文本在未捕获格式器中优先展示；debug agent 异常停止事件新增 `exceptionStack` 并由 VS Code DAP adapter 暴露 `exceptionInfo`/stderr 输出；focused Phase 3 回归 `debug_traceback|cli_debug_e2e|debug_agent|debug_trace|debug_introspection|debug_variable_child_shape|debug_expression_diagnostics` 7/7 PASS；扩展 `compile` 与 `test:unit` 29/29 PASS | Phase 3 DoD 已满足；按 §7.5，进入 Phase 4 前仍需补跑里程碑 A 全量 `tests/` 三构建矩阵与扩展桌面 smoke |
| 2026-06-20 13:40:50 +08:00 | Milestone A | 阻塞；Phase 4 未开始 | 尝试按 §7.5 跑三构建全量矩阵；MSVC Phase 3 focused 目标 `debug_traceback|cli_debug_e2e|debug_agent` 补跑 3/3 PASS | MSVC 全量构建被非 debug 目标链接错误阻塞：`native_binding_dispatch_fast_lane`、`create_instruction_1/2/4`、`compiler_build_function_semir_metadata`、`ensure_native_module_compile_info` 未解析；WSL/GCC 与 WSL/Clang 根构建目录在 `cmake_depends` 依赖扫描阶段超时，残留进程已清理。严格按 §7.5，需先恢复全量矩阵再进入 Phase 4 |
| 2026-06-20 14:27:39 +08:00 | Milestone A | 部分推进；全量 CTest 阻塞 | 为 Windows/MSVC 共享库测试补出内部 helper 导出：`native_binding_dispatch_fast_lane`、`create_instruction_0/1/2/4`、`compiler_build_function_semir_metadata(_shallow)`、`ensure_native_module_compile_info`；先前 MSVC 链接失败目标已逐个构建通过；`build-msvc` 全量构建通过；MSVC debug 相关套件 `cli_debug_e2e`、`debug_traceback`、`debug_agent`、`debug_agent_protocol`、`debug_expression_diagnostics`、`debug_variable_child_shape` 全部通过 | `ctest --test-dir build-msvc -C Debug --output-on-failure` 完成但 11/66 失败：`core_runtime`、`language_pipeline`、`containers`、`language_server`、`language_server_stdio_inline_value_semantic_smoke`、`cli_repl_expression_assignment_context_smoke`、`projects`、`escape_pipeline`、`cli_repl_e2e`、`metadata_module_hash_golden`、`system_fs`；WSL 根目录矩阵仍未恢复。严格按 §7.5，Phase 4 仍未开始 |
| 2026-06-20 15:09:27 +08:00 | Milestone A | 部分解除；全量 CTest 仍阻塞 | 修复解释器 `META_GET`/`META_SET` 与 AOT runtime 的 hidden accessor 调用契约不一致问题：`META_SET` 以目标槽作为 receiver/result、以 `A1` 作为 assigned value，并让 meta getter/setter 通过 `InvokeMember` 调用隐藏访问器；补跑 WSL/GCC 与 Windows/MSVC focused `zr_vm_instructions_test`、MSVC `core_runtime` 均通过 | 初始 MSVC 全量失败集中表已修正为 11/66；`core_runtime` 已解除，但完整 `ctest` 未重跑，`language_pipeline` 当前仍失败，Phase 4 继续按 §7.5 阻塞 |
| 2026-06-20 19:01:51 +08:00 | Milestone A | GC 阻塞解除；AOT contract 阻塞 | 清除本轮 GC/执行器临时追踪；补齐 frame GC value-slot 访问与 dense-stack 跳过逻辑，`zr_vm_precall_frame_slot_reset_test` 14/14 PASS；MSVC `gc_fragment_stress` benchmark 与 fixture 均通过；`ctest --test-dir build-msvc -C Debug -R "^(core_runtime|language_pipeline)$"` 中 `core_runtime` 1/1 PASS | `language_pipeline` 不再停在 GC stress，当前失败集中到 `zr_vm_aot_c_source_contracts_test` 的 3 个 source contract 文本断言：typed arithmetic f64 slot helper、typed bitwise scalar assignment、typed signed branch scalar assignment。严格按 §7.5，需修复 AOT contract 后再进入 Phase 4 |
| 2026-06-20 19:15:20 +08:00 | Milestone A | AOT contract 阻塞解除；value-type return 阻塞 | 单独重建 `zr_vm_aot_c_source_contracts_test` 后 18/18 PASS，确认 19:01 的 AOT 文本断言来自旧测试产物；重跑 `ctest --test-dir build-msvc -C Debug -R "^(core_runtime|language_pipeline)$"` 后 `core_runtime` 继续通过且 AOT contract 通过 | `language_pipeline` 当前剩余 `zr_vm_value_type_runtime_test` 3 个失败：inline struct/large POD/string field return by value 在构造原始值后调用返回函数时报 `Attempted to call non-callable value`。严格按 §7.5，需先修复 value-type return 调用窗口/byte-frame 回写问题 |
| 2026-06-21 03:56:52 +08:00 | Milestone A | Windows 核心/语言阻塞解除；Phase 4 仍阻塞 | 修复 CFG builder 在 `cfg_add_block` 可能重分配 `cfg->blocks` 后继续读取旧 `SZrParserCfgBlock *` 的 ASan heap-use-after-free；循环、switch、try 等入口统一改为使用稳定 block id 连接 fallthrough。保持关闭阶段先释放 GC、后释放 string table，并将 Debug 短字符串 major-root 扫描 guard 改为 capacity 上限，解除 `zr_vm_instructions_test` 关闭阶段断言。清理本轮 GC/PreCall/module 临时追踪；MSVC 全量构建通过；ASan `zr_vm_module_system_test` 88/88 PASS；MSVC `zr_vm_module_system_test`、`zr_vm_parser_test`、`zr_vm_scripts_test`、`zr_vm_value_type_runtime_test`、`zr_vm_instructions_test` 均 PASS；`ctest -R "^(core_runtime|language_pipeline)$"` 2/2 PASS，单独 `language_pipeline` 1/1 PASS | 一次全量 CTest 在 `language_pipeline` 内 `zr_vm_module_system_test` 子进程挂起超过 20 分钟，但同目标随后单独通过。重跑既有失败集合后，Milestone A 仍被 8 个非 debug 套件阻塞：`containers` timeout、`language_server`、`language_server_stdio_inline_value_semantic_smoke`、`cli_repl_expression_assignment_context_smoke`、`projects` timeout、`cli_repl_e2e`、`metadata_module_hash_golden`、`system_fs`。WSL 根目录全量矩阵仍待恢复；严格按 §7.5，Phase 4 继续不开始 |
| 2026-06-21 04:14:20 +08:00 | Milestone A | LSP ownership 阻塞解除；Phase 4 仍阻塞 | 修复 LSP semantic type prototype 对 `%unique T` / `Unique<T>` 等 ownership generic 的声明类型归一化：内部比较使用 inner surface type + ownership qualifier，避免把 `%unique Resource` 渲染/比较成 `Unique<Resource>`；同时修复 hover/completion/signature 展示中的 `%unique Unique<T>` 双重渲染。focused LSP 构建通过，`zr_vm_language_server_inlay_semantic_facts_test`、`zr_vm_language_server_semantic_analyzer_test` 直接运行通过，`ctest -R "^language_server$"` 1/1 PASS | `language_server` 已从既有失败集合移除；`language_server_stdio_inline_value_semantic_smoke` 仍失败，当前最低层现象是 inline value 对 object expression statement 的 computed-key 数值事实未暴露。剩余阻塞仍包括 `containers` timeout、`language_server_stdio_inline_value_semantic_smoke`、`cli_repl_expression_assignment_context_smoke`、`projects` timeout、`cli_repl_e2e`、`metadata_module_hash_golden`、`system_fs`；严格按 §7.5，Phase 4 继续不开始 |
| 2026-06-21 04:26:29 +08:00 | Milestone A | stdio inline computed-key 阻塞解除；Phase 4 仍阻塞 | 修复行首 `{...}` 语句分派：当 `{` 后是 identifier/string key 加 `:`，或 computed key `[...]` 加 `:` 时按 object literal expression statement 解析，普通 block 仍走 block 路径；LSP local semantic query 在选中外层 structural expression 时会回退查找包含 query range 的最窄 numeric fact，因此 `{[1 + 2]: 4};` 的 computed-key 数值事实能由共享 semantic fact 暴露给 inline value。新增 local semantic query 回归；focused 构建通过，`zr_vm_language_server_local_semantic_query_test` 直接运行通过，`zr_vm_parser_test` 75/75 PASS，`ctest -R "^language_server_stdio_inline_value_semantic_smoke$"` 1/1 PASS | `language_server_stdio_inline_value_semantic_smoke` 已从既有失败集合移除。剩余阻塞预计为 `containers` timeout、`cli_repl_expression_assignment_context_smoke`、`projects` timeout、`cli_repl_e2e`、`metadata_module_hash_golden`、`system_fs`；仍需复跑 prior-failure subset 确认最新集合。WSL 根目录全量矩阵仍待恢复；严格按 §7.5，Phase 4 继续不开始 |
| 2026-06-21 04:36:14 +08:00 | Milestone A | 阻塞集合复核完成；Phase 4 仍阻塞 | 复跑 prior-failure subset：`language_server_stdio_inline_value_semantic_smoke` 1/1 PASS，确认 stdio inline computed-key 阻塞已解除 | 最新剩余 6 个阻塞：`containers` timeout、`cli_repl_expression_assignment_context_smoke`、`projects` timeout、`cli_repl_e2e`、`metadata_module_hash_golden`、`system_fs`。WSL 根目录全量矩阵仍待恢复；严格按 §7.5，Phase 4 继续不开始 |
| 2026-06-21 04:39:47 +08:00 | Milestone A | REPL 阻塞解除；Phase 4 仍阻塞 | 修复 `cli_repl_e2e` 在 Windows 输出 `\r\n` 时按裸 `\n3\n` 搜索导致的误判；收窄 `repl_expression_assignment_context_smoke` 对 member assignment 的断言，只验证后续 member read 持久化为 `40`，不再要求 assignment statement 本身打印结果。重建 `zr_vm_cli_repl_e2e_test`/`zr_vm_cli_executable` 通过；直接运行 C e2e 与 JS smoke 均通过；`ctest -R "^(cli_repl_expression_assignment_context_smoke|cli_repl_e2e)$"` 2/2 PASS | 最新剩余 4 个阻塞：`containers` timeout、`projects` timeout、`metadata_module_hash_golden`、`system_fs`。WSL 根目录全量矩阵仍待恢复；严格按 §7.5，Phase 4 继续不开始 |
| 2026-06-21 07:18:13 +08:00 | Milestone A | Windows prior-failure subset 解除；Phase 4 仍阻塞 | 修复 metadata golden hash 漂移并同步文档；修复 `system.fs` object model 中 `FileSystemEntry.refresh()` 返回 ABI 与 native 对象值不一致导致的 `system_fs` 失败；修复容器/项目链路暴露的 receiver staging、native member direct call、static property receiver、string `+` 推断、dynamic no-arg call quickening 覆盖返回槽等问题。focused 验证覆盖 `metadata` 4/4、`system_fs` 1/1、`containers` 49/49 direct、`container_matrix`、`benchmark_container_pipeline`、`native_numeric_pipeline`、`projects` 1/1，以及 prior-failure subset 7/7 PASS | Windows/MSVC 既有失败集合本轮已清空；但 §7.5 要求的全量 `tests/` 三构建矩阵尚未重新完成，WSL gcc/clang 根目录矩阵仍待恢复。因此严格按计划 Phase 4 继续不开始 |

| 2026-06-21 11:48:01 +08:00 | Milestone A | Windows 全量通过；WSL focused debug 通过；Phase 4 仍阻塞 | 修复 debug hook 后 stack/base 同步、frame-layout generic call 在 stack relocation 后重算 return destination，以及 generic PostCall 与 inline return stackTop 契约冲突；Windows/MSVC Debug 全量构建通过，`ctest --test-dir build-msvc -C Debug --output-on-failure --timeout 480` 66/66 PASS；WSL/GCC `debug_traceback|debug_agent` 2/2 PASS，WSL/Clang `debug_traceback|debug_agent` 2/2 PASS | §7.5 要求的 WSL gcc/clang 全量 `tests/` 矩阵仍未完成；扩展桌面 smoke 仍未补做。严格按计划，Milestone A 未关闭，Phase 4 继续不开始 |
| 2026-06-21 22:43:43 +08:00 | Milestone A | 完成；Phase 4 入口解除阻塞 | 补齐 §7.5 剩余矩阵：WSL/GCC Debug 全量构建通过，focused gate 覆盖 `language_pipeline`、metadata golden、LSP position smoke、AOT scalar 与 `debug_traceback|debug_agent` 7/7 PASS，`ctest --test-dir build-wsl-gcc --output-on-failure --timeout 600` 66/66 PASS；WSL/Clang Debug 全量构建通过，focused gate 7/7 PASS，`ctest --test-dir build-wsl-clang --output-on-failure --timeout 600` 66/66 PASS；扩展 `compile` PASS、`test:unit` 29/29 PASS、`test:e2e:desktop:debug` exit 0 | Milestone A 的三构建全量 `tests/` + 扩展桌面 debug smoke 已满足；本轮未开始 Phase 4，下一步按计划进入脚本层 `debug` 标准库 |
| 2026-06-21 23:35:14 +08:00 | Phase 4 | 完成；脚本层 `debug` 标准库首批 API 通过验收 | 新增 `debug` native module 注册入口（受信/沙箱）、`traceback/getinfo/getlocal/setlocal/getupvalue/setupvalue/upvalueid/sethook/gethook` 桥接、沙箱写保护、脚本 hook trampoline 与 `__hook` root；新增 `tests/library/test_debug_library.c` 并接入 CMake/CTest；新增 `docs/library-and-builtins/zr-debug-module.md` | WSL/GCC 与 WSL/Clang 的 `debug_library` 单项和 `debug_metadata|debug_trace|debug_traceback|debug_library` focused gate 均通过。Phase 5 未开始；Milestone B 全量三构建 + 扩展 smoke 留到 Phase 5 完成后执行 |
| 2026-06-22 00:11:45 +08:00 | Phase 5.1 | 完成；Phase 5 继续 | `debug_snapshot.c` 的 stackTrace/scopes/variables/evaluate-frame lookup 改为复用 `ZrCore_Debug_GetStack`、`ZrCore_Debug_GetInfo`、`ZrCore_Debug_GetLocal`、`ZrCore_Debug_GetUpvalue`；新增 `tests/debug/test_debug_snapshot_contracts.c` source contract，锁定 DAP 快照不得回退到 raw callInfo/local slot/upvalue value 遍历 | RED：contract 初始 2/2 FAIL；完成后 WSL/GCC 与 WSL/Clang focused gate `debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` 均 8/8 PASS；Windows/MSVC CLI `hello_world.zrp` smoke 输出 `hello world`。下一步按 Phase 5.2 修 step 语义边界 |
| 2026-06-22 00:50:51 +08:00 | Phase 5.2 | 完成；Phase 5 继续 | 新增 `tests/debug/test_debug_step_edges.c` 覆盖 tail-call step-over、native step-in、exception unwind step-out、recursive same-line step-over；step controller 记录 step 起点 call frame 身份，step-over 跨过更深子帧与同深度 tail-call 逻辑帧，step-out 在异常 unwind 期间等待父帧下一个可见停靠点，step-over 期间推迟被跨过子调用里的断点 | RED：新用例初始 3/4 FAIL（tail-call 停进 callee、exception unwind 断言需按计划改为 next visible parent location、recursive same-line 被原断点抢先停下）；完成后 WSL/GCC 与 WSL/Clang focused gate 9/9 PASS，Windows/MSVC `zr_vm_debug_step_edges_test` 4/4 PASS。下一步按 Phase 5.3 治理截断标记与分页 |
| 2026-06-22 01:42:22 +08:00 | Phase 5.3 | 完成；Phase 5 继续 | `zr_debug_copy_text` 对超长文本统一追加 `...[+N]` 省略标记，路径优先保留尾部；长字符串 value/evaluate preview 在被固定缓冲截断时创建 `STRING_CHUNKS` 变量句柄，`variables` 支持按 `start/count` 惰性读取 64-byte 字符串片段；新增/扩展 `tests/debug/test_debug_truncation.c` 并接入 CMake/CTest | RED：`debug_truncation` 初始 3/4 FAIL，新增分页断言后曾暴露 evaluate 超长字面量限制，改为从运行时长字符串值验证；完成后 WSL/GCC 与 WSL/Clang focused gate `debug_truncation|debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` 均 10/10 PASS，Windows/MSVC `debug_truncation` 5/5 PASS 且 `debug_step_edges|debug_truncation` CTest 2/2 PASS。下一步 Phase 5.4 task/协程多执行体调试 |
| 2026-06-22 02:36:30 +08:00 | Phase 5.4 | 完成；Phase 5 继续 | 新增 `tests/debug/test_debug_threads.c` 并接入 CMake/CTest；DAP `initialize` 报告 `supportsThreads`，新增 `threads` 请求；agent 内部维护 `SZrState -> threadId` 稳定注册表，主执行体注册为 `threadId=1/main`；`stopped`/`continued` 事件带 `threadId`；`stackTrace`、`scopes`、`variables`、`evaluate` 可按请求 `threadId` 临时路由到目标 state 后恢复；frame/scope/result 层输出 `threadId`，不在每个变量项重复输出以避免放大大对象响应 | RED：`debug_threads` 初始 1/1 FAIL，`supportsThreads` 缺失；实现后 WSL/GCC 与 WSL/Clang focused gate `debug_threads|debug_truncation|debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` 均 11/11 PASS，Windows/MSVC `debug_threads|debug_step_edges|debug_truncation` CTest 3/3 PASS。当前 task runtime 未暴露额外活跃 `SZrState` 列表，Phase 5.4 MVP 先完成协议枚举与按 thread 栈检查路由；跨 task 同步单步仍按计划留后续 |
| 2026-06-22 03:32:22 +08:00 | Phase 5.5 | 完成；Phase 5 继续 | 新增 `tests/debug/test_debug_data_breakpoint.c` 并接入 CMake/CTest；DAP `initialize` 报告 `supportsDataBreakpoints`；协议层新增 `dataBreakpointInfo` 与 `setDataBreakpoints`；Locals/Closures scope 可为局部变量和 upvalue 生成稳定 dataId；agent 在 LINE/COUNT hook 路径按 watch 列表比对当前值与上一快照，变化即以 `reason=dataBreakpoint` 停止，并在 stopped event 输出 `dataId`、`description`、`threadId` | RED：`debug_data_breakpoint` 初始缺 `supportsDataBreakpoints`；实现后 WSL/GCC、WSL/Clang、Windows/MSVC focused gate `debug_data_breakpoint|debug_threads|debug_truncation|debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` 均 12/12 PASS。对象字段 watch 仍按计划留后续，当前软件 watchpoint 成本随 watch 数线性增长 |
| 2026-06-22 03:43:08 +08:00 | Phase 5.6 | 按计划暂不启用；Phase 5 完成 | 远程调试鉴权为可选增强；当前没有开放非 loopback 调试需求，因此保持默认 loopback 限制与 `auth_token` 预留字段，不新增远程握手语义 | 无代码变更；Phase 5 结束时 WSL/GCC、WSL/Clang、Windows/MSVC focused debug gate 均 12/12 PASS。下一步进入 Phase 6 profiling/coverage/反汇编 |
| 2026-06-22 04:21:19 +08:00 | Phase 6.1 | 部分完成；Phase 6 继续 | 完成确定性 profiling：`zr_vm_lib_debug` 新增 profile API，基于 CALL/RETURN hook 统计调用次数与 total/self 时间；CLI 新增 `--profile[=out]` 并在运行成功后输出确定性 profile report；新增 deterministic profile 单测和 CLI parse/e2e 覆盖 | 采样 profiling（COUNT hook 直方图）仍待实现；coverage、反汇编、heap summary 未开始。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `profile_deterministic|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 04:46:47 +08:00 | Phase 6.1 | 完成；Phase 6 继续 | 补齐 COUNT hook 采样 profiling：`ZrDebug_Profile_StartWithSampling` 支持按指令周期采样当前函数+行，CLI `--profile` report 追加 `ZR_PROFILE samples` 热点区块，profile 单测与 CLI e2e 均覆盖采样输出 | Phase 6.1 DoD 已满足；下一步按计划进入 Phase 6.2 coverage。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `profile_deterministic|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 05:44:30 +08:00 | Phase 6.2 | 完成；Phase 6 继续 | 完成 line coverage：核心新增 activelines API，debug 库新增 coverage API，LINE hook 标记已执行行并保留可执行但未覆盖行；CLI 新增 `--coverage[=out]` report；新增 coverage 单测、CLI parse 与 import fixture e2e 覆盖 | Phase 6.2 DoD 已满足；下一步按计划进入 Phase 6.3 反汇编。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `coverage|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 06:36:30 +08:00 | Phase 6.3 | 完成；Phase 6 继续 | 完成字节码反汇编：核心新增 `ZrCore_Debug_DisassembleFunction`，输出 offset/opcode/operands/line；CLI 新增 `--dump-bytecode <out>`，在 entry 加载后、执行前写出反汇编文件；新增 `tests/debug/test_disassemble.c` 与 CLI parse/e2e 覆盖 | Phase 6.3 DoD 已满足；下一步按计划进入 Phase 6.4 heap summary。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `debug_disassemble|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 07:08:48 +08:00 | Phase 6.4 | 完成；Phase 6 计划项已交付 | 完成轻量 heap summary：核心 `ZrCore_Debug_HeapSummary`、CLI `--heap-summary[=out]`、core/CLI tests 与 acceptance 记录；输出 raw object type/prototype buckets、总 base bytes、GC regions/bytes/collection counters | WSL/GCC、WSL/Clang、Windows/MSVC focused gate `debug_heap_summary|cli_args|cli_import_basic_fixture` 均 3/3 PASS；WSL 常规多目标 build 因无关 parser/AOT 重编译超时，已清理并采用 focused 对象重编译验证 |

## 4. 参考映射（zr ← lua/）

| 能力 | Lua 参考 | zr 现状 → 目标 |
|------|----------|----------------|
| sethook | `lua_sethook` `ldebug.c:134` | 无 → `ZrCore_Debug_SetHook`（P1） |
| count hook | `LUA_MASKCOUNT`+`luaG_traceexec` `ldebug.c:928` | 死代码 → 接线（P1） |
| getstack | `lua_getstack` `ldebug.c:163` | 仅栈顶 → 按层级（P1/P2） |
| getinfo | `lua_getinfo` `ldebug.c:388` | 忽略 what → 按位填充（P1/P2） |
| getlocal/setlocal | `ldebug.c:223/245` | 无 → 新增（P2） |
| getupvalue/setupvalue | `lapi.c:1380/1394` | 无 → 新增（P2） |
| 行号表 | `lineinfo`+`abslineinfo` | 已有（含列号）→ 保持 |
| traceback | `luaL_traceback` `lauxlib.c:132` / QuickJS `build_backtrace` | 无 → 新增（P3） |
| debug 标准库 | `ldblib.c` | 无 → 新增脚本模块（P4） |
| profiling | CPython `sys.setprofile` | 无 → count/call hook 之上构建（P6） |

> 建议严格按 Phase 顺序推进：P1 的修复与 setter 是后续所有内容的地基。
