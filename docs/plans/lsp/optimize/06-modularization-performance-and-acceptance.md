# LSP 模块化、性能与最终验收实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `modularize-large-files`, `evidence-driven-wsl-validation`, `requesting-code-review`, `code-module-docs-maintenance`, and `verification-before-completion` while executing this plan.

**Goal:** 在功能正确后拆除超大混合职责文件和测试，建立跨编译器、sanitizer、WASM、性能与真实编辑器的最终发布门禁。

**Architecture:** 按 protocol、snapshot、semantic query projection、navigation、formatting、diagnostics、project indexing 等责任组织文件；测试按同样边界拆分。性能通过可重复的 workload、cache hit/fallback 指标和内存上限验收，不靠单次 microcase。

**Tech Stack:** CMake/Ninja、GCC/Clang/MSVC、ASan/UBSan/LSan/Valgrind、Node/VS Code smoke、Emscripten、benchmark JSON/CSV。

---

## Task 1：冻结拆分前行为与依赖图

**Files:**
- Create: `docs/architecture/language-server/module-boundaries.md`
- Create: `docs/acceptance/lsp/optimize-modularization-baseline.md`
- Create: `tests/language_server/lsp_source_boundaries_test.c`

- [ ] 记录每个超限文件的行数、职责、static/public symbols、include graph、test owners 和计划目标模块。
- [ ] source-boundary test 阻止 stdio transport 包含 feature handlers、protocol serializers 调用 parser internals、LSP features 读取 mutable FileVersion content、semantic projections进行名称推断。
- [ ] 拆分前先跑对应测试；任何已有失败必须归因，不得在纯移动中顺手改变行为。

**审查时主要超限：**

| 文件 | 约行数 | 拆分边界 |
|---|---:|---|
| `interface/lsp_interface_support.c` | 5352 | completion/hover/navigation/edit serialization/snapshot helpers |
| `lsp_signature_help.c` | 4195 | call-site syntax、canonical signature、provider projection |
| `semantic/lsp_semantic_query.c` | 3403 | local/import/metadata query adapters |
| `semantic_analyzer_symbols.c` | 3004 | 待计划 03 删除或降为 syntax recovery |
| `semantic_analyzer.c` | 2964 | lifecycle、walk、diagnostic projection |
| `semantic_analyzer_typecheck.c` | 2876 | 迁移到 compiler diagnostics 后删除 |
| `project/lsp_project.c` | 2761 | workspace discovery、project load、file records、provider lifecycle |
| `interface/lsp_interface.c` | 2517 | context/document orchestration 与 feature façade |
| `metadata/lsp_metadata_provider.c` | 2293 | descriptor load、generation、query projection |
| `project/lsp_project_navigation.c` | 2209 | module resolution、symbol navigation、virtual URI |
| `project/lsp_project_imports.c` | 2199 | import graph、canonicalization、diagnostics |
| `tests/language_server/test_lsp_interface.c` | 8270 | 按 feature 拆成独立 tests |
| `tests/language_server/test_lsp_project_features.c` | 8100 | 按 source/binary/native/provider generation 拆分 |
| `tests/language_server/stdio_smoke.js` | 4456 | protocol client + focused smoke suites |

## Task 2：拆分 production modules

**Files:**
- Modify: `zr_vm_language_server/CMakeLists.txt`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface*.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/project/lsp_project*.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c`

- [ ] 每次只拆一个 cohesive responsibility；保留 façade 时只做 snapshot acquire/query/project/release。
- [ ] public header 暴露最小 API；内部 helper 放对应 module private header，不创建 `misc`、`helpers2`、`partN`。
- [ ] 移动 static helper 与其 tests 同批完成；CMake、include、docs 同提交。
- [ ] 生产文件目标约 1000 行以内；若单一职责仍超限，在 architecture 文档说明不可再分的具体原因。
- [ ] 每次纯移动提交要求 `git diff --stat`、focused tests 和 behavior golden 无变化；之后功能修改单独提交。

## Task 3：拆分测试与共享 harness

**Files:**
- Create: `tests/language_server/support/lsp_test_state.c`
- Create: `tests/language_server/support/lsp_test_documents.c`
- Create: `tests/language_server/support/lsp_protocol_client.js`
- Modify: `tests/language_server/test_lsp_interface.c`
- Modify: `tests/language_server/test_lsp_project_features.c`
- Modify: `tests/language_server/stdio_smoke.js`
- Modify: `tests/language_server/CMakeLists.txt`

- [ ] C tests 按 completion、hover/signature、navigation、rename、diagnostics、semantic tokens、project source/binary/native、provider generation 分 executable。
- [ ] JS tests 按 lifecycle/frame、document sync、diagnostics、editing、hierarchy、project/files、performance 分文件，复用唯一 protocol client。
- [ ] 测试名表达行为与输入，不使用 milestone 编号或实现 helper 名称。
- [ ] CTest 能单独运行每个 provider；aggregate target 只做组合门禁。

## Task 4：增加 fuzz、sanitizer 与故障注入

**Files:**
- Create: `tests/language_server/fuzz/fuzz_stdio_frame.c`
- Create: `tests/language_server/fuzz/fuzz_lsp_positions.c`
- Create: `tests/language_server/fuzz/fuzz_incremental_edits.c`
- Create: `tests/language_server/test_lsp_fault_injection.c`

- [ ] fuzz frame header/payload、JSON envelope、position/range、UTF-8/UTF-16、incremental edit sequence、URI round trip。
- [ ] fault injection 覆盖 allocation failure、project/provider reload failure、partial metadata、reader EOF、client disconnect、WASM null pointer/invalid JSON。
- [ ] ASan/UBSan/LSan 与 Valgrind 运行 teardown、rename/workspace edit、provider reload、semantic token cache。
- [ ] 任一 crash/leak/use-after-free 都在最低复现层修复，禁止只给上层返回空数组。

## Task 5：定义规模与性能预算

**Files:**
- Create: `tests/language_server/performance/lsp_workspace_benchmark.js`
- Create: `tests/language_server/fixtures/projects/lsp_scale/README.md`
- Create: `docs/acceptance/lsp/optimize-performance.md`

- [ ] 固定生成 workload：1k/10k 文件索引、100k symbols、深 import graph、频繁单行 edit、provider reload、workspace diagnostics、references/rename、semantic delta。
- [ ] 采集 p50/p95/p99、peak RSS/WASM memory、parse/full-fallback ratio、semantic/query cache hit、cancel latency、result size。
- [ ] 建议初始预算（以基线机器校准后冻结）：交互请求 p95 < 100 ms、单文件 edit→diagnostics p95 < 200 ms、取消观察 < 50 ms、无界 cache 增长为 0。
- [ ] 性能结果绑定 commit、compiler、build type、CPU/内存；不得用 Debug 与 Release 相互比较。
- [ ] 新优化必须先通过 correctness differential，再接受性能收益。

## Task 6：跨平台与编辑器矩阵

**Files:**
- Create: `docs/acceptance/lsp/optimize-final-matrix.md`
- Modify: `tests/CMakeLists.txt`
- Modify: `zr_vm_language_server_extension/package.json`

- [ ] GCC 11+ Debug shared、Clang 14+ Debug shared、MSVC x64 Debug static 均从 fresh build directory 编译完整受影响图。
- [ ] Clang ASan/UBSan、Valgrind focused、WASM release build 通过。
- [ ] VS Code desktop native、VS Code Web WASM，以及 desktop Web mode（若计划 05 选择支持）运行同一核心 smoke corpus。
- [ ] Windows path/UNC/case、Linux path/symlink、browser virtual URI 单独验收。
- [ ] 打包 VSIX 后从包内启动 server/worker，验证 assets hash、serverInfo version、capability manifest。

## Task 7：最终 code review 与完成定义

- [ ] 请求独立 code review，优先检查生命周期、snapshot ownership、capability truth、错误映射、semantic fallback、workspace edit 原子性和 Web parity。
- [ ] review finding 逐项以复现测试关闭；不以“当前 smoke 通过”否决结构性问题。
- [ ] `git diff --check`、全量测试、sanitizer、performance budget、desktop/web smoke 均引用同一最终 commit。
- [ ] 所有 acceptance 记录从 pending 更新为 completed；未完成项必须从 capability registry 移除，不得以 known issue 保留过度声明。
- [ ] 删除任务 build/log/session 临时文件；保留用户或其他会话拥有的 artifact。

```powershell
git diff --check
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-final-gcc --output-on-failure'
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-final-clang --output-on-failure'
npm --prefix zr_vm_language_server_extension run test:unit
npm --prefix zr_vm_language_server_extension run test:e2e:web
npm --prefix zr_vm_language_server_extension run test:e2e:desktop
```

