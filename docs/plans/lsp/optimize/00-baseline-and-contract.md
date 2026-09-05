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
- [ ] 为 3.17 capability matrix 添加 snapshot；3.18 可选能力用单独 client capability fixture。

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
- [ ] `willCreateFiles`/`willDeleteFiles` 若始终返回 null，则只保留 did* 通知注册；`willRenameFiles` 因真实生成 edit 可保留。
- [ ] 不把撤销 capability 视为功能回归；这是使协议声明与实现一致的 P0 修复。

2026-09-05 条款核对：原先要求关闭 implementationProvider 和 workspaceFolders
changeNotifications 的依据已被后续实现取代。当前 implementation 消费
`ImplementationsOf(SymbolId)`，native folder 通知更新实际 workspace；保留能力并
重验实际目标/状态。declaration/typeDefinition 仍是 definition alias。此调整不代表
跨 provider implementation 或 Web workspace 已验收，也不改变 syntax 语义规则。

## Task 5：验收与提交边界

- [ ] `stdio_protocol_inventory.js` 对 registry、initialize JSON、dispatch 和 WASM exports 做一一映射，输出 0 个 orphan/overclaim。
- [ ] 所有当前失败在 acceptance 文档中有 owner 和后续计划链接。
- [ ] `git diff --check`、GCC focused build、extension unit/noEmit 通过。
- [ ] 只提交基线/契约相关路径，不夹带活动 L8 overlay。

```powershell
git diff --check
git status --short
git add docs/acceptance/lsp/optimize-baseline.md tests/language_server/stdio_protocol_inventory.js tests/language_server/stdio_protocol_client.js tests/language_server/stdio_protocol_conformance.js zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c zr_vm_language_server_extension/test/serverCapabilities.test.js
git commit -m "test(lsp): freeze protocol capability baseline"
```
