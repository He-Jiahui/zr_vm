---
related_code:
  - zr_vm_language_server/wasm/wasm_response.h
  - zr_vm_language_server/wasm/wasm_response.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - zr_vm_language_server_extension/tsconfig.worker.json
  - zr_vm_language_server_extension/package.json
  - tests/language_server/test_wasm_response.c
  - tests/language_server/test_wasm_exports.c
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory.js
  - tests/language_server/wasm_capability_inventory_test.js
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_language_server/wasm/wasm_response.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
tests:
  - tests/language_server/test_wasm_response.c
  - tests/language_server/test_wasm_exports.c
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory_test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
doc_type: milestone-record
---

# Plan 00 Task 2 Sub04: WASM response contract correction

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-07 01:15 +08:00 | 2026-09-07 02:42 +08:00 | completed (response boundary slice; Plan 00 and core status pending) | 提取响应封装；显式传递 JSON-RPC code；修正 cJSON 布尔值和 ownership；worker 严格 envelope/typed bridge；workspace diagnostics 不再吞掉文档失败；hover 空结果与 native 对齐；加入 worker 专用 tsc 与 C 故障注入。 | response harness `144/144`；native response/export CTest `2/2`；WASM C++ translation unit compile passed with warnings only；worker `tsc -p tsconfig.worker.json` passed；extension unit `42/42`；worker bridge probe passed；inventory regression `10/10`。 |

## RED 与修复

提交 `90086f8a` 的 RED 由两个问题组成：同一 message 会决定不同错误码，且
`{"success":true}` 会被当成 fallback 成功。响应模块现在接收调用方的整数 code，
使用 `ZR_FALSE/ZR_TRUE` 生成布尔值；success 必须显式拥有 data，error response
必须拥有 code 和非空 message。`SuccessResponse` 消费输入树，即使根节点或字段
分配失败也不会泄漏。

`wasm-bridge.ts` 在 UTF-8 解码、JSON parse 和 null pointer 路径使用 `finally`
释放指针，并将异常分类为 `ResponseError(-32603, ...)`。worker 从同一个
`vscode-languageserver/browser` 导入 error class，严格拒绝非布尔 success、混合
success/error、缺少 data 和非法 code。单独的 `tsconfig.worker.json` 使之前被主
tsconfig 排除的 worker 路径进入 strict typecheck。

WASM workspace diagnostics 现在在枚举 URI、core diagnostics/resultId 或任意报告
序列化失败时释放已有树并返回 InternalError；不会继续返回部分数组。普通 hover
的 legacy core false/no-result 仍输出成功 `data:null`，与 stdio 行为一致；core
尚未提供可区分 no-result 与 internal failure 的 transport-neutral status，因此
该门槛仍 pending。

## 命令与结果

```text
wsl.exe bash -lc 'gcc -std=c11 -Wall -Wextra -Werror ... test_wasm_response.c wasm_response.c cJSON.c'
  WASM response: 144 scenarios, 0 failures
wsl.exe bash -lc 'em++ ... -c zr_vm_language_server/wasm/wasm_exports.cpp'
  exit 0; existing zr_meta_conf.h writable-string warnings only
node node_modules/typescript/bin/tsc -p tsconfig.worker.json
  exit 0
npm run test:unit
  42 passed, 0 failed
node tests/language_server/lsp_wasm_worker_probe.js
  exit 0; production bridge/worker route and malformed-envelope cases passed
node tests/language_server/wasm_capability_inventory_test.js
  WASM inventory regression: 10/10
wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/build-wasm-response-gcc --target zr_vm_language_server_wasm_response_test zr_vm_language_server_wasm_exports_test --parallel 4'
  exit 0; targets up to date
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-wasm-response-gcc --output-on-failure -R "^(language_server_wasm_response|language_server_wasm_exports)$"'
  2/2 passed
```

The existing GCC build directory could not be used for a linked native harness:
its mixed stale artifacts caused `libzr_vm_library.so: file format not recognized`
while linking an unrelated container library. This is retained as an environment
failure; it is not presented as a source failure. The real C++ export translation
unit was compiled independently with Emscripten includes and the same current
checkout.

## Remaining gates

This subitem does not close Plan 00 or Plan 05. A real linked `.wasm` asset and
browser smoke are still absent; core transport-neutral status, nested provider
allocation safety outside workspace reports, versioned edit plans, native/WASM
golden parity, and the full ordered Plan 01–04 gates remain pending.
