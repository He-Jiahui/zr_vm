---
related_code:
  - CMakeLists.txt
  - docs/zr_language_specification.md
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
implementation_files:
  - CMakeLists.txt
  - docs/wiki/manifest.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
  - docs/wiki/manifest.json
tests:
  - tests/parser/test_syntax_reference_v1.c
  - tests/library/test_official_provider_convergence.c
  - tests/cli/test_cli_args.c
  - tests/thread/test_thread_runtime.c
doc_type: reference
---

# 功能状态矩阵

状态是当前源码快照的文档分类，不替代 CI 结果：`current` 表示已有生产入口和测试证据；
`experimental` 表示入口存在但跨后端/平台边界仍需宿主显式选择；`planned` 表示只有设计或
占位，不应写进生产示例。

| 范围 | 状态 | 证据/边界 |
| --- | --- | --- |
| module/import、显式分号、函数、class/struct/interface/enum/union | `current` | `docs/zr_language_specification.md`、syntax reference fixture |
| canonical Type/Place/CFG、definite assignment、borrow/loan | `current` | parser tests and SemIR API |
| GC、exception、object/prototype、call binding | `current` | core headers/tests |
| `.zrp`、`.zro`、`.zri`、`.zrs` 读写 | `current` | artifact schema/writer tests |
| `zr.builtin` canonical type/protocol surface | `current` | official inventory N0、provider convergence、canonical type tests |
| `zr.system`、`zr.container`、`zr.math`、`zr.iteration`、`zr.ffi` | `current` | official provider inventory |
| `zr.network` | `experimental` | `BUILD_NETWORK_LIB` 可选，loopback fixture 优先 |
| `zr.task` canonical Job/Task/Scheduler | `current` | task runtime/job tests |
| `zr.thread` Attached/Isolated provider | `experimental` | platform/transport/quota 由 thread tests 覆盖 |
| `zr.compile` build facts/diagnostics/conditional | `current` | compile-time contract and execution tests |
| `zr.compile.declaration` typed GeneratedField/Patch | `current` | declaration-transform contract/transaction tests；其它 generated variants 未发布 |
| reflection construction/generic instances | `experimental` | metadata preserve 和 AOT level 约束 |
| stable-slot pooling / pinned pointer views | `experimental` | generation/barrier/loan tests |
| AOT C | `experimental` | ABI v16、shared-library smoke；需逐 opcode parity |
| AOT LLVM | `experimental` | emitter/stripping tests，运行时覆盖较窄 |
| CLI run/compile/REPL/test/migration | `current` | CLI and CMake suites |
| LSP stdio semantic features | `current` | interface/stdio tests；能力按 initialize 协商 |
| LSP WASM/VS Code extension | `experimental` | WASM/extension tests和工具链依赖 |
| Rust binding | `experimental` | C ABI 稳定，Rust crate/平台发布仍独立验证 |

旧 `%module`、`%import`、`%owned`、旧 `func`、`$Type`、`zr.coroutine` 和 TaskRunner 属于
`removed`，只能在 migration/negative fixtures 中出现。任何页面若引用它们，必须明确“已移除”。
