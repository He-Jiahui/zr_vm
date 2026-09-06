# LSP 基线冻结与能力契约实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `test-driven-development`, `cross-session-coordination`, `evidence-driven-wsl-validation`, and `verification-before-completion` while executing this plan.

**Goal:** 冻结当前工作树的真实行为与失败，建立协议能力清单和负向测试，使后续优化不能用过度声明、空数组或旧二进制掩盖缺陷。

**Architecture:** 先把 capability、handler、核心 API、WASM export 和测试映射成机器可检查的单一清单；能力只有在实现、协议测试、客户端协商和运行时导出同时存在时才能进入 initialize 结果。

**Tech Stack:** C11、cJSON、Node.js test runner、TypeScript、CMake/CTest、Ninja、LSP 3.17。

---

## Task 1：建立可复现基线

**Files:**
- Create: `docs/acceptance/lsp/optimize-baseline.md`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/language_server/stdio_protocol_inventory.js`

- [ ] 记录 `git rev-parse HEAD`、`git status --short`、编译器版本、Node/npm/emsdk 版本；不得把未提交 L8 overlay 描述成 commit 内能力。
- [ ] 先读取 `.codex/sessions` 的活动所有权；等待 L8 external-callable 路径 exact commit 或明确 release 后再建立全量基线。
- [x] 为 `docs/plans/lsp/00-current-state.md`、01-05 主计划与所有 active leaf 建立 [completed/pending/superseded crosswalk](2026-09-05-plan00-task01-sub01-execution-crosswalk.md)：331 个日期记录逐项保留来源、证据边界与后续责任，Task 1 其余基线门槛仍进行中。
- [x] Sub02：完成冻结提交 `670e3cd0` 的 [GCC 全成员失败收集](2026-09-05-plan00-task01-sub02-gcc-baseline.md)：83 个 aggregate executable，73 pass / 10 fail / 66 failure blocks，逐项责任与精确 JSON 归档；收集器 11/11 回归和 CTest 注册通过。当前集成版本仍待前述会话提交，未勾选下方全量门槛。
- [x] Sub03：修复 `lsp_project_features_test` 打印失败但退出 0 的测试 harness；[完成记录](2026-09-05-plan00-task01-sub03-project-test-exit.md)；GCC/Clang/MSVC 保留 46 pass、14 fail 并全部退出 1。语义失败仍由原责任层处理。
- [ ] 使用全新且互不共享的构建目录，避免旧 build cache 与并发 CMake regeneration：

```powershell
wsl.exe bash -lc 'cmake -S /mnt/e/Git/zr_vm -B /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF'
wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_stdio zr_vm_language_server_lsp_interface_test zr_vm_language_server_lsp_project_features_test zr_vm_language_server_lsp_advanced_editor_features_test --parallel 8'
```

- [ ] 运行现有 LSP/extension 测试并把每个失败记录为“测试名、输入、预期、实际、所有者”，不得只记录进程退出码。

```powershell
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure -R "language_server"'
npm --prefix zr_vm_language_server_extension run test:unit
Push-Location zr_vm_language_server_extension
npx tsc -p . --noEmit
Pop-Location
```

**当前审查快照：** extension unit tests 30/30 和 TypeScript no-emit 通过；并发 L8 日志显示 isolated GCC build、canonical consumers、project features 通过。活动会话仍记录 interface 两项与 aggregate language-server 十一项失败，因此全量绿色尚未成立。

## Task 2：创建能力清单

**Files:**
- Create: `zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c`
- Create: `tests/language_server/test_lsp_capability_registry.c`
- Modify: `zr_vm_language_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [x] Sub01：完成实际 core/native/WASM 名称、CTest ID、implementation ownership 和 runtime 字段约束的修复；[记录](2026-09-05-plan00-task02-sub01-registry-metadata.md)。三工具链 focused 各 9/9，最终 registry 单项各 1/1，独立审查通过；下方完整清单与运行时映射门槛仍未验收。
- [x] Sub02：完成编译期 native registry/initialize/dispatch inventory；[记录](2026-09-05-plan00-task02-sub02-compiled-native-inventory.md)。实际清单含 30 个 descriptor、43 条 native route、3 个 metadata-only control、0 个 orphan，四个协商 profile 和 GCC/Clang/MSVC focused 各 14/14；WASM export/worker 映射与通知行为仍待验收。
- [x] Sub03：完成 WASM CMake export、C++ 定义/声明、bridge `ccall` 和 worker handler 的静态 inventory，并修正 Web semantic-token legend；[记录](2026-09-06-plan00-task02-sub03-wasm-static-inventory.md)。30 个 runtime export、28 个 bridge 调用、22 条 worker route、13 个 token type 均通过；真实链接资产的隔离 Emscripten 构建在私有 overlay 补齐该未提交头文件后仍因系统内存压力被 `Killed`，未生成资产，仍待验收。
- [ ] 先写失败测试：每项能力必须声明协议版本、client capability path、core entry point、native adapter、WASM export、resolve 行为与 test id。
- [ ] 建立不含 cJSON/TypeScript 的核心描述：

```c
typedef enum EZrLspRuntimeMask {
    ZR_LSP_RUNTIME_NATIVE = 1u,
    ZR_LSP_RUNTIME_WASM = 2u,
} EZrLspRuntimeMask;

typedef struct SZrLspCapabilityDescriptor {
    const TZrChar *method;
    TZrUInt32 runtimeMask;
    TZrUInt16 minimumMajor;
    TZrUInt16 minimumMinor;
    TZrBool hasResolve;
    TZrBool isExperimental;
} SZrLspCapabilityDescriptor;
```

- [ ] registry 测试拒绝以下不一致：声明 resolve 但 handler 仅 identity；声明 method 但 runtime 无导出；声明 3.18 method 但未标 experimental；公开 capability 没有协议测试。
- [ ] 将 `stdio_initialize.c`、WASM worker 的硬编码能力迁移留给计划 05；本任务先提供清单与 failing assertions。

## Task 3：建立协议负向测试驱动器

**Files:**
- Create: `tests/language_server/stdio_protocol_conformance.js`
- Create: `tests/language_server/stdio_protocol_client.js`
- Modify: `tests/CMakeLists.txt`

- [ ] 从 `stdio_smoke.js` 抽取 frame 编解码、request id、超时与 stderr 捕获，禁止继续复制第五套 client harness。
- [ ] RED 用例至少包括：初始化前 request、重复 initialize、shutdown 后 request、缺少/错误 jsonrpc、非法 id、非法 params、未知 method、notification 不得收到 response、畸形 frame、超大 Content-Length、重复 request id、取消未知 id。
- [ ] 每个断言检查完整 JSON-RPC envelope 与精确 error code，而不是仅检查返回数组类型。
- [x] Sub01：修正 known-id cancellation fixture 的准备计时，保留精确 URI/version 与 `-32800` envelope；GCC/Clang/MSVC 各 30/30，[完成记录](2026-09-05-plan00-task03-sub01-cancellation-setup.md)。活动查询 50 ms 预算仍由 Plan 01 单独验收。
- [ ] 为 3.17 capability matrix 添加 snapshot；3.18 可选能力用单独 client capability fixture。
- [x] Sub02：完成 3.17/3.18 optional capability 协商、精确 MethodNotFound、完整 capability snapshot 和 cJSON allocation failure 回归；[记录](2026-09-05-plan00-task03-sub02-optional-capabilities.md)。GCC/Clang/MSVC 各 11/11，父 Task 3 的取消、frame 和其余负向门槛仍未验收。
- [x] Sub03：在当前提交后的 GCC/Clang/MSVC 二进制重放协议负向、workspace folder、save、resolve、file-operation 和 client-command smoke；[记录](2026-09-06-plan00-task03-sub03-protocol-replay.md)。protocol conformance 各 30/30，MSVC workspace folders 12/12、save 6/6、optional 25/25；父 Task 3 的 integrated capability/version audit、sanitizer 和 active-query latency 仍未验收。

## Task 4：立即撤销已知过度声明

**Files:**
- Modify: `zr_vm_language_server/stdio/stdio_initialize.c`
- Modify: `zr_vm_language_server/stdio/stdio_initialize_capabilities.c`
- Modify: `zr_vm_language_server_extension/src/browser/worker/server-worker.ts`
- Modify: `tests/language_server/stdio_smoke.js`
- Create: `zr_vm_language_server_extension/test/serverCapabilities.test.js`

- [x] Sub01 RED/GREEN：撤销 documentLink/codeLens/inlayHint/workspaceSymbol 的 identity resolve 声明、handler 和 method 常量；初始响应保留完整数据，显式请求返回精确 MethodNotFound。见[完成记录](2026-09-05-plan00-task04-sub01-identity-resolve.md)。
- [x] Sub01：native `codeAction/resolve` 实际复验 snapshot，保留；Web identity resolver 撤销。registry 分别记录 base runtime mask 与 resolve runtime mask。
- [x] Sub02：撤销仍然转发 definition 的 declarationProvider/typeDefinitionProvider；验证 Device 定义的准确 token 范围、Device/Sensor implementation 完整目标集合和 workspace folder 实际更新。见[完成记录](2026-09-05-plan00-task04-sub02-navigation-aliases.md)。跨 provider 和 reaching-write 缺陷仍未完成。
- [x] Sub03：撤销始终返回 null 的 `willCreateFiles`/`willDeleteFiles` 注册和 handler，保留并精确验证 did* 通知、`willRenameFiles` 版本化编辑与 stale disk 拒绝；[完成记录](2026-09-05-plan00-task04-sub03-file-operations.md)，三工具链各 8/8。
- [x] Sub04：撤销缺失 compiler typed color facts 的 colorProvider 与两个颜色方法，删除原字符串扫描器；[完成记录](2026-09-05-plan00-task04-sub04-untyped-color.md)。三工具链各 12/12，专用 fixture 各 30/30，扩展 unit 40/40/noEmit 通过；源码契约保留原有两项 constructor 失败，未晋级后续阶段。
- [x] Sub05：撤销无 notification handler 的 willSave 声明，保留准确的 save-time formatting 和可观察的磁盘 didSave 刷新；[完成记录](2026-09-05-plan00-task04-sub05-save-notification.md)。三工具链各 13/13，最终保存 fixture 各 6/6；忽略第二次 didSave 的 mutation 恰好 2/6 失败，独立复审通过。
- [x] Sub06：移除未登记且始终返回 null 的 executeCommand 路由和 handler，客户端命令收到准确 MethodNotFound；[完成记录](2026-09-05-plan00-task04-sub06-client-commands.md)。三工具链专项各 14/14，命令 fixture 各 10/10；综合 smoke 保留同一既有泛型失败，独立复审通过。
- [x] Sub01-03 均以真实初始结果、目标集合和索引变化验证保留能力；撤销空实现使声明与实现一致，不视为功能回归。最终 core/export/runtime 清单仍由 Task 2/5 验收。

2026-09-05 条款核对：原先要求关闭 implementationProvider 和 workspaceFolders
changeNotifications 的依据已被后续实现取代。当前 implementation 消费
`ImplementationsOf(SymbolId)`，native folder 通知更新实际 workspace；保留能力并
重验实际目标/状态。declaration/typeDefinition 仍是 definition alias。此调整不代表
跨 provider implementation 或 Web workspace 已验收，也不改变 syntax 语义规则。

## Task 5：验收与提交边界

- [x] Native 子项：`stdio_protocol_inventory.js` 已连接编译 registry、initialize JSON、production dispatch 和 CTest 注册，四个 profile 输出 0 个 native orphan/overclaim；完成记录见[compiled native inventory](2026-09-05-plan00-task02-sub02-compiled-native-inventory.md)。
- [x] Web 静态子项：`wasm_capability_inventory.js` 已检查 CMake export、WASM C++ 定义/声明、bridge 调用、worker 路由及 semantic-token legend；完成记录见[WASM static inventory](2026-09-06-plan00-task02-sub03-wasm-static-inventory.md)。真实 `.wasm` export 表和 worker 资产加载仍待验证。
- [x] `stdio_protocol_inventory.js` 对 registry、initialize JSON、dispatch 和 WASM source-level worker/bridge wiring 做一一映射，输出 `integrated-contract-mapped`、0 个 native orphan/overclaim 和 schema 2 WASM 子报告；真实 linked asset 仍待验证，完成记录见[集成 inventory](2026-09-06-plan00-task05-integrated-inventory.md)。
- [x] 所有当前失败在 acceptance 文档中有 owner 和后续计划链接；见[优化 baseline owner ledger](../../../acceptance/lsp/optimize-baseline.md)、[GCC failure baseline](2026-09-05-plan00-task01-sub02-gcc-baseline.md)和[native inventory open gates](../../../../tests/acceptance/2026-09-05-lsp-native-capability-inventory.md)。
- [x] `git diff --check`、GCC focused build、extension unit/noEmit 通过；当前独立构建完成 `841/841`，extension unit `41/41`，TypeScript `noEmit` 退出 0。
- [x] 只提交基线/契约相关路径，不夹带活动 L8 overlay；本轮提交为 `dfe80e8c`、`35a9be1e`，并保留活动工作树的其他修改未暂存。

2026-09-06 当前工作树的三个语义 focused executable 仍未通过：interface
`8` 个失败、project features `14` 个失败、advanced editor features `1` 个失败。
这些失败属于活动 semantic overlay 与 Plan 00 Task 1 的全量基线责任，不能由
本 Task 5 的 inventory/build 门禁代替；精确名称和日志保存在
[WASM worker wiring acceptance](../../../../tests/acceptance/2026-09-06-lsp-wasm-worker-wiring.md)。

```powershell
git diff --check
git status --short
git add docs/acceptance/lsp/optimize-baseline.md tests/language_server/stdio_protocol_inventory.js tests/language_server/stdio_protocol_client.js tests/language_server/stdio_protocol_conformance.js zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c zr_vm_language_server_extension/test/serverCapabilities.test.js
git commit -m "test(lsp): freeze protocol capability baseline"
```
