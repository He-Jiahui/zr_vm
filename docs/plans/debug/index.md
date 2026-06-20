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
