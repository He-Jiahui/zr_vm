# LSP Pull/Push Diagnostics Acceptance

## Scope

Plan 02 Task 6 establishes one structured diagnostic result identity for native
stdio and the browser WASM adapter. It also makes workspace diagnostics cover
unopened indexed files and keeps push notifications version-aware.

## Contract

- A resultId includes snapshot generations and a sorted complete structured
  diagnostic payload. Dependency changes produce a new importer resultId even
  when importer text is unchanged.
- Invalid document-diagnostic parameters return JSON-RPC `-32602`; only a
  matching previous result identity may return `unchanged`.
- Workspace diagnostics enumerate project-indexed source files plus open
  overlays. Unopened entries have `version: null`.
- Pull reports never suppress push. Push deduplication requires both equal
  resultId and equal open-document version.
- Browser diagnostics call the exported C store and do not recreate a
  TypeScript text hash.

## Evidence

- GCC and Clang Debug shared: the Task 6 generation smoke and full stdio smoke
  both exit `0` on the final source baseline.
- Fresh MSVC 19.44.35228 Debug shared: stdio, CLI, and descriptor plugin build;
  both smoke commands exit `0`.
- Browser worker: `npm run compile` and the diagnostics bridge static test exit
  `0`.
- Emscripten: current store/export objects compile, and both diagnostic export
  symbols are present. Full linked WASM/browser end-to-end acceptance is a
  Task 7 gate and is not claimed here.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 18:45 +08:00 | 已完成 | Plan 02 Task 6 diagnostic identity、workspace coverage、push/pull coexistence 与 WASM bridge delegation。 | Native GCC/Clang/MSVC smoke 真实 exit 0；browser compile/static test exit 0；WASM objects export diagnostics entry points。 |
