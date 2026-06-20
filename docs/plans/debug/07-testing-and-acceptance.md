---
doc_type: plan-phase
phase: 7
title: 测试矩阵与验收标准
related_code:
  - tests/debug/
  - tests/cli/test_cli_debug_e2e.c
  - tests/cli/test_cli_import_basic_fixture.c
  - tests/CMakeLists.txt
---

# Phase 7 — 测试矩阵与验收标准（贯穿）

汇总各阶段测试，给出整体验收口径。新增测试统一挂到 `tests/CMakeLists.txt` 与现有
`tests/debug/` 套件。

## 7.1 测试矩阵

| 能力 | 测试文件 | 阶段 |
|------|----------|------|
| COUNT hook 触发计数 | `tests/debug/test_debug_hook_core.c` | P1 |
| LINE hook 去重 | 同上 | P1 |
| SetHook 启用即生效（trap 传播） | 同上 | P1 |
| GetStack 多层 | `tests/debug/test_debug_hook_core.c`（P1），`tests/debug/test_debug_introspection.c`（P2 扩展） | P1/P2 |
| GetInfo 按 type 位填充 | `tests/debug/test_debug_hook_core.c`（P1），`tests/debug/test_debug_introspection.c`（P2 扩展） | P1 |
| CLI debug launch/import 基线 | `tests/cli/test_cli_debug_e2e.c`、`tests/cli/test_cli_import_basic_fixture.c` | P1 |
| getlocal/setlocal（含值类型/inline struct） | `tests/debug/test_debug_introspection.c` | P2 |
| getupvalue/setupvalue/upvalueid | `tests/debug/test_debug_introspection.c` | P2 |
| namewhat 分类 | `tests/debug/test_debug_introspection.c` | P2 |
| traceback 多帧/折叠/截断 | `tests/debug/test_debug_traceback.c` | P3 |
| CLI 未捕获错误带回溯 | `tests/cli/test_cli_debug_e2e.c`（扩展） | P3 |
| 脚本 debug.* 全 API | `tests/library/test_debug_library.*` | P4 |
| 沙箱禁用写 API | 同上 | P4 |
| step 边界（尾调用/native/unwind/递归） | `tests/debug/test_debug_step_edges.c` | P5 |
| 截断标记与分页 | `tests/debug/test_debug_truncation.c` | P5 |
| 多 task 线程枚举 | `tests/debug/test_debug_threads.c` | P5 |
| data breakpoint | `tests/debug/test_debug_data_breakpoint.c` | P5 |
| 确定性 profiling 计数 | `tests/profile/test_profile_deterministic.c` | P6 |
| coverage 行集合 | `tests/profile/test_coverage.c` | P6 |
| 反汇编输出 | `tests/debug/test_disassemble.c` | P6 |

## 7.2 回归基线（不得破坏）

- 现有 `tests/debug/test_debug_agent*.c`、`test_debug_trace.c`、
  `test_debug_variable_child_shape.c`、`test_debug_expression_diagnostics.c`、
  `test_debug_metadata.c` 全绿。
- `tests/cli/test_cli_debug_e2e.c` 现有用例全绿。
- `tests/cli/test_cli_import_basic_fixture.c` 保证 `import_basic` source project 可重编译并通过 binary 路径运行。
- VS Code 扩展（`dapSession.ts`）现有手测路径不回归。
- 三套构建均通过：`build-msvc`、`build-wsl-clang`、`build-wsl-gcc`（仓库已有这三目录）。

## 7.3 性能门槛

- `debugHookSignal==0`（无 hook、无 observer）时，VM 走 fast-path，性能与改动前持平
  （以现有基准脚本对比，回归 < 2%）。这是硬性要求：debug 设施**零开销当其未启用**。
- 启用 hook/profiling/coverage 时允许显著变慢，但需在文档标注量级。

## 7.4 验收清单（Definition of Done）

每个阶段独立可交付，DoD：

1. **P1**：COUNT hook 可用且计数精确；`SetHook` 可从 C 启用 line/call/return/count；
   `GetStack`/`GetInfo` 按层级与位掩码工作；旧 `DebugInfo_Get` 兼容包装保留。
2. **P2**：getlocal/setlocal/getupvalue/setupvalue/upvalueid 可用，值类型局部正确处理；
   DAP snapshot 已切换到内核 API 且回归全绿；`SZrDebugInfo` 无遗留 `// todo`。
3. **P3**：未捕获错误与异常对象携带多帧回溯；生成器截断安全、抓取时机正确。
4. **P4**：脚本 `debug` 模块首批 API 全部可用且受沙箱开关保护。
5. **P5**：step 四类边界用例通过；截断有标记；多 task 可枚举与检查；data breakpoint 可用。
6. **P6**：profiling/coverage/反汇编三件套可用，默认关闭、按需开启，CLI 开关齐备。

## 7.5 推进与里程碑建议

- **里程碑 A（地基）**：P1 + P2 + P3。交付后 zr 内核具备 Lua 级别的内省与回溯能力，
  DAP 也因复用内核 API 更稳。这是优先级最高的一块。
- **里程碑 B（开放）**：P4 + P5。脚本可调试、DAP 体验完善。
- **里程碑 C（测量）**：P6。profiling/coverage/反汇编，作为增值工具。

每个里程碑结束跑全量 `tests/`（三构建）+ 扩展手测，再进入下一里程碑。

## 7.6 阶段验证记录

| 时间 | 阶段 | 构建 | 命令/范围 | 结果 |
|------|------|------|-----------|------|
| 2026-06-20 07:59:23 +08:00 | P2 | Windows/MSVC Debug | `cmake --build build\codex-debug-phase1-msvc --config Debug --target zr_vm_cli_executable zr_vm_debug_agent_test zr_vm_debug_introspection_test zr_vm_debug_variable_child_shape_test zr_vm_debug_expression_diagnostics_test zr_vm_closure_capture_runtime_test --parallel 8` | 构建通过；仅既有 warning |
| 2026-06-20 07:59:23 +08:00 | P2 | Windows/MSVC Debug | `zr_vm_cli --project tests\fixtures\projects\language_debug_gauntlet\language_debug_gauntlet.zrp -m fixed_data --execution-mode interp` | 输出 `null`，exit 0 |
| 2026-06-20 07:59:23 +08:00 | P2 | Windows/MSVC Debug | `zr_vm_cli tests\fixtures\projects\language_debug_gauntlet\language_debug_gauntlet.zrp --execution-mode interp` | 输出 `GAUNTLET_OK checksum=13910`，exit 0 |
| 2026-06-20 07:59:23 +08:00 | P2 | Windows/MSVC Debug | `zr_vm_closure_capture_runtime_test` | 1 test / 0 failures |
| 2026-06-20 07:59:23 +08:00 | P2 | Windows/MSVC Debug | `ctest --test-dir build\codex-debug-phase1-msvc -C Debug -R "^(debug_introspection|debug_variable_child_shape|debug_agent|debug_expression_diagnostics)$" --output-on-failure` | 4/4 PASS |
| 2026-06-20 08:00-08:20 +08:00 | P2 | WSL gcc/clang existing dirs | gcc: `debug_expression_diagnostics` PASS、`debug_variable_child_shape` PASS；clang: `debug_expression_diagnostics` PASS；新增 `debug_introspection` | `debug_introspection` 测试项存在但可执行文件未产出；显式构建 `zr_vm_debug_introspection_test` 多次超时，同机存在外部 WSL 构建占用。未计为通过，需补跑 |
| 2026-06-20 10:52:13 +08:00 | P2 | WSL gcc Debug | `ninja -C build/codex-debug-phase1-wsl-gcc -j2 zr_vm_debug_introspection_test`；`ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "^(debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` | 构建通过；focused gate 3/3 PASS |
| 2026-06-20 10:52:13 +08:00 | P2 | WSL clang Debug | `ninja -C build/codex-debug-phase1-wsl-clang -j2 zr_vm_debug_introspection_test zr_vm_debug_variable_child_shape_test`；`ctest --test-dir build/codex-debug-phase1-wsl-clang -R "^(debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` | 构建通过；focused gate 3/3 PASS |
| 2026-06-20 11:49:13 +08:00 | P3 | WSL gcc focused manual link | `gcc ... tests/debug/test_debug_traceback.c zr_vm_core/src/zr_vm_core/debug_traceback.c zr_vm_core/src/zr_vm_core/exception.c ... -lzr_vm_parser -lzr_vm_core ... -o build/codex-debug-traceback-manual/zr_vm_debug_traceback_test && build/codex-debug-traceback-manual/zr_vm_debug_traceback_test` | 4/4 PASS；覆盖多帧脚本栈、native/script/native 混合帧、小 buffer 截断、深栈折叠、异常对象 `stack` 文本字段。正式 CMake/Ninja 目标构建仍超时，未计为阶段完成 |
| 2026-06-20 11:46:04 +08:00 | P3 | WSL gcc formal build attempt | `cmake -S . -B build/codex-debug-phase3-wsl-gcc -G Ninja ...`；`cmake --build build/codex-debug-phase3-wsl-gcc --target zr_vm_debug_traceback_test -j2` | 配置成功；目标构建两轮因完整依赖编译超时未完成，未产出 `bin/zr_vm_debug_traceback_test`，未计为通过 |
| 2026-06-20 12:47:18 +08:00 | P3 | WSL gcc Debug | 目标构建：`zr_vm_debug_traceback_test`、`zr_vm_cli_executable`、`zr_vm_cli_debug_e2e_test`、`zr_vm_debug_agent_test`；`ctest --test-dir build/codex-debug-phase3-wsl-gcc -R "^(debug_traceback|cli_debug_e2e|debug_agent|debug_trace|debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` | 构建通过；focused Phase 3 回归 7/7 PASS |
| 2026-06-20 12:47:18 +08:00 | P3 | CLI manual smoke | `zr_vm_cli -e` 内联三层 `leaf`/`middle`/`root` 抛错脚本 | 输出统一 traceback，包含 `Error:`、`payload:` 与 `leaf`/`middle`/`root` 源码帧；CLI 未捕获错误 exit 行为本阶段未变更 |
| 2026-06-20 12:47:18 +08:00 | P3 | VS Code extension | `npm --prefix zr_vm_language_server_extension run compile`；`npm --prefix zr_vm_language_server_extension run test:unit` | TypeScript 编译通过；unit tests 29/29 PASS；DAP adapter 已暴露 `exceptionInfo`，底层 `debug_agent` CTest 覆盖 `exceptionStack` 事件字段 |
| 2026-06-20 13:40:50 +08:00 | Milestone A | Windows/MSVC Debug | `cmake --build build-msvc --config Debug --parallel 8` | 构建失败，未进入全量 CTest；非 debug 目标链接缺符号：`native_binding_dispatch_fast_lane`、`create_instruction_1/2/4`、`compiler_build_function_semir_metadata`、`ensure_native_module_compile_info` |
| 2026-06-20 13:40:50 +08:00 | Milestone A | Windows/MSVC Debug focused P3 | `cmake --build build-msvc --config Debug --target zr_vm_debug_traceback_test zr_vm_cli_debug_e2e_test zr_vm_debug_agent_test --parallel 8`；`ctest --test-dir build-msvc -C Debug -R "^(debug_traceback|cli_debug_e2e|debug_agent)$" --output-on-failure` | 构建通过；3/3 PASS |
| 2026-06-20 13:40:50 +08:00 | Milestone A | WSL gcc Debug | `cmake --build build-wsl-gcc -j2 && ctest --test-dir build-wsl-gcc --output-on-failure`；随后尝试 focused P3 目标 | 全量命令 15 分钟超时且未进入 CTest；focused P3 构建在 `cmake_depends` 依赖扫描阶段超时，残留 `build-wsl-gcc` 进程已清理 |
| 2026-06-20 13:40:50 +08:00 | Milestone A | WSL clang Debug | 初次 focused P3 构建报告旧目录缺 `zr_vm_debug_traceback_test` 目标；随后 `cmake -S . -B build-wsl-clang` 并重试 focused P3 目标 | 重新生成/构建仍在 `cmake_depends` 依赖扫描阶段超时，未进入 CTest；残留 `build-wsl-clang` 进程已清理 |
| 2026-06-20 14:27:39 +08:00 | Milestone A | Windows/MSVC Debug build unblock | 补出 Windows shared-lib 内部测试 helper 导出后，顺序构建 `zr_vm_parser_shared`、`zr_vm_native_inline_span_dispatch_test`、`zr_vm_compiler_integration_test`、`zr_vm_semir_dynamic_arithmetic_deopt_test`、`zr_vm_semir_type_conflict_deopt_test`、`zr_vm_thread_runtime_test` | 先前链接缺符号目标全部构建通过 |
| 2026-06-20 14:27:39 +08:00 | Milestone A | Windows/MSVC Debug full | `cmake --build build-msvc --config Debug --parallel 4 -- /nodeReuse:false`；`ctest --test-dir build-msvc -C Debug --output-on-failure` | 构建通过；CTest 完成但 11/66 FAIL：`core_runtime`、`language_pipeline`、`containers`、`language_server`、`language_server_stdio_inline_value_semantic_smoke`、`cli_repl_expression_assignment_context_smoke`、`projects`、`escape_pipeline`、`cli_repl_e2e`、`metadata_module_hash_golden`、`system_fs`；debug 套件通过 |
| 2026-06-20 15:09:27 +08:00 | Milestone A | WSL gcc Debug / Windows MSVC Debug focused core runtime | WSL：`cmake --build build/codex-debug-phase3-wsl-gcc --target zr_vm_instructions_test -j2`、`./build/codex-debug-phase3-wsl-gcc/bin/zr_vm_instructions_test`；MSVC：`cmake --build build-msvc --config Debug --target zr_vm_instructions_test --parallel 4 -- /nodeReuse:false`、`build-msvc\bin\Debug\zr_vm_instructions_test.exe`、`ctest --test-dir build-msvc -C Debug -R "^core_runtime$" --output-on-failure` | WSL `zr_vm_instructions_test` 95/95 PASS；MSVC `zr_vm_instructions_test` 95/95 PASS；MSVC `core_runtime` 1/1 PASS。根因：解释器 meta access 与 AOT hidden accessor 约定不一致；已修复。完整 Milestone A 矩阵仍需继续清理后重跑 |
| 2026-06-20 19:01:51 +08:00 | Milestone A | Windows/MSVC Debug GC/frame focused | `cmake --build build-msvc --config Debug --target zr_vm_core_shared zr_vm_lib_system_shared zr_vm_cli_executable zr_vm_precall_frame_slot_reset_test --parallel 2 -- /nodeReuse:false /v:minimal`；`zr_vm_precall_frame_slot_reset_test.exe`；`zr_vm_cli.exe tests\benchmarks\cases\gc_fragment_stress\zr\benchmark_gc_fragment_stress.zrp`；`zr_vm_cli.exe tests\fixtures\projects\gc_fragment_stress\gc_fragment_stress.zrp` | 构建通过；frame-slot test 14/14 PASS；benchmark 输出 `BENCH_GC_FRAGMENT_STRESS_PASS`；fixture 输出 `GC_FRAGMENT_STRESS_PASS checksum=568637664`。临时 GC/执行器追踪已清除 |
| 2026-06-20 19:01:51 +08:00 | Milestone A | Windows/MSVC Debug focused suite | `ctest --test-dir build-msvc -C Debug -R "^(core_runtime|language_pipeline)$" --output-on-failure` | `core_runtime` 1/1 PASS；`language_pipeline` FAIL，当前仅见 `zr_vm_aot_c_source_contracts_test` 3 个 source contract 文本断言失败：f64 typed arithmetic slot helper、typed bitwise scalar assignment、typed signed branch scalar assignment |
| 2026-06-20 19:15:20 +08:00 | Milestone A | Windows/MSVC Debug AOT contract refresh | `cmake --build build-msvc --config Debug --target zr_vm_aot_c_source_contracts_test --parallel 2 -- /nodeReuse:false /v:minimal`；`zr_vm_aot_c_source_contracts_test.exe` | 构建通过；18/18 PASS，确认 AOT contract 失败由旧产物造成 |
| 2026-06-20 19:15:20 +08:00 | Milestone A | Windows/MSVC Debug focused suite | `ctest --test-dir build-msvc -C Debug -R "^(core_runtime|language_pipeline)$" --output-on-failure` | `core_runtime` PASS，AOT contract PASS；`language_pipeline` 仍 FAIL，剩余 `zr_vm_value_type_runtime_test` 的 inline struct/large POD/string field return by value 3 个用例，错误集中为构造原始 inline struct 后下一次函数调用目标被覆盖成 int/string 等非 callable |
