---
related_code:
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/zrpSupport.ts
  - zr_vm_language_server_extension/schemas/zrp.schema.json
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
implementation_files:
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/zrpSupport.ts
  - zr_vm_language_server_extension/schemas/zrp.schema.json
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
plan_sources:
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
tests:
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/schemas/zrp.schema.json
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/zrpSupport.ts
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# `.zrp` Editor Schema And LSP Refresh

## 范围

这份文档说明 `.zrp` 项目文件在 VS Code extension 侧的两条职责：

1. 把 `.zrp` 识别成 JSON 文档并挂本地 schema，提供属性提示和基础校验。
2. 确保 `.zrp` 的文档保存或外部文件变更最终能触发 language server 的 project refresh。

这条实现遵循总计划里的边界约束：

- `.zrp` 不进入 ZR 语义分析主链。
- `.zrp` 的编辑体验交给 extension + JSON schema。
- language server 只负责 `.zrp` 文本更新后的 project index refresh。

## 当前扩展行为

`zr_vm_language_server_extension/package.json` 现在新增了 `zr-project` 语言入口：

- `.zrp` 文件先以 `zr-project` 打开，保证扩展会在打开项目文件时激活。
- 激活后，`src/zrpSupport.ts` 会把 `.zrp` 文档立即切换成 VS Code 内置 `json` 语言。
- 这样 `.zrp` 能直接复用 JSON 编辑器能力，而不是继续停留在 plain text 或自定义语法里。

这条设计避免了两个常见问题：

- 只靠 `jsonValidation` 但扩展根本不会在 `.zrp` 打开时激活。
- 把 `.zrp` 注册成独立语言后又失去 JSON schema / 属性提示。

## Schema 覆盖

`zr_vm_language_server_extension/schemas/zrp.schema.json` 当前覆盖了项目解析器已经消费的字段：

- `name`
- `version`
- `description`
- `author`
- `email`
- `url`
- `license`
- `copyright`
- `binary`
- `source`
- `entry`
- `dependency`
- `local`

其中：

- `binary`、`source`、`entry` 被标记为必填，因为 `zr_vm_library/project.c` 会把这三项当成有效项目配置的最低要求。
- `email` 使用 `email` 格式校验。
- `url` 使用 `uri` 格式校验。
- 路径型字段至少给出 `minLength` 和用途说明，保证补全和 hover 时能显示“这是相对项目根目录还是相对 source root 的哪类路径”。

当前 schema 选择了 `additionalProperties: true`：

- 这样不会因为用户试验性字段或未来扩展字段直接把整个文档打成错误。
- 同时核心字段仍然能拿到稳定的 completion 和基本校验。

## Language Client 与 Project Refresh

`src/extension.ts` 和 `src/browser.ts` 现在都把 language client 的 `documentSelector` 扩到了 `**/*.zrp`：

- `.zr` 文档继续按语言 id `zr` 进入 server。
- `.zrp` 文档即使已经切成 `json`，也会因为路径匹配进入 server。

这点很关键，因为 server 当前真正会刷新 project index 的入口仍然是：

- `textDocument/didOpen`
- `textDocument/didChange`
- `textDocument/didSave`

也就是 `ZrLanguageServer_Lsp_UpdateDocument(...)` 最终落到：

- `.zrp` => `ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(...)`
- `.zr` => 解析/语义分析后再按需 refresh

## 当前 Watched-Files 行为

extension 侧的 `createFileSystemWatcher('**/*.{zr,zrp,zro,zri,dll,so,dylib}')` 现在不再只是“重启补偿器”。它会把外部文件事件直接同步给 language server，server 当前已经消费：

- `.zr`
- `.zrp`
- `.zro`
- `.zri`
- `.dll`
- `.so`
- `.dylib`

对应 server 侧入口是 `stdio_requests.c` 里的 `workspace/didChangeWatchedFiles`，当前行为分成三类：

- `.zr`
  - 重新从磁盘载入文档内容
  - 刷新 parser / analyzer / diagnostics
- `.zrp`
  - 重建 project index
  - 重新绑定 project discovery 与 source root
- `.zro/.zri/.dll/.so/.dylib`
  - 定位 owning project
  - 如果该 project 还没进 `projectIndexes`，会沿着变更文件路径向上回溯最近 `.zrp` 并自举 project index
  - 触发 project refresh
  - 失效旧 analyzer，并把已加载源码在新 project 事实下重新分析

这意味着未打开文件的 source change、binary metadata change、plugin descriptor change 现在都能走增量 refresh，而不是只能靠手工重启 language server。对于“先发生 metadata 变更、后第一次打开源码”的工程，server 也会先把 owning project 建起来，再让 `workspace/symbol`、后续 hover/completion 直接命中新索引。

## `.zro` / `.zri` / Native Plugin 在 refresh 里的角色

`lsp_module_metadata.c` 现在把 project binary metadata 和 native/plugin descriptor 统一到同一套 source-kind 解析里：

- `project source`
- `binary metadata`
- `native builtin`
- `native descriptor plugin`

其中 binary metadata 继续支持两种载体：

- `.zro`
  - 走 binary source loader
- `.zri`
  - 走 intermediate text metadata reader
  - 直接解析 `EXPORTED_SYMBOLS` 供 hover / completion 使用

这条分流保证了 `.zri` 变更不会再被错误当成 `.zro` 去做 binary IO 读取，也让 watched-files refresh 能在 `.zri` 变更后立即更新 imported member hover/completion。

这轮又补了一层导航语义：

- `%import("graph_binary_stage")` 这类 binary-only import 的 definition 现在会落到 `.zri` 文件入口
- `binaryStage.binarySeed` 这类 imported member definition 会落到 `.zri` 的 `EXPORTED_SYMBOLS` 对应声明行

也就是说，server 不再只把 `.zri` 当“hover/completion 的备用说明文本”，而是把它当成真正可导航的结构化 metadata 文档。

## 验证证据

2026-04-04 这轮 `.zrp` 扩展支持的验证结果：

- `zr_vm_language_server_extension/package.json` 能正确暴露：
  - `onLanguage:zr-project`
  - `.zrp` 语言注册
  - `.zrp` 的 `jsonValidation` schema 绑定
- `npm run compile` 在 `zr_vm_language_server_extension/` 通过
- `node -e "const schema=require('./schemas/zrp.schema.json')..."` 能成功读取 schema，并确认：
  - `required = [binary, source, entry]`
  - 覆盖字段数量为 13

## 当前限制

- 当前 `.zrp` 仍然是“先以 `zr-project` 打开，再切到 `json`”的两段式路径；这比静态文件关联多一步，但能保证扩展在 `.zrp` 首次打开时就被激活。
- `.zri` 目前在 server 侧优先覆盖 imported member hover/completion 所需的 `EXPORTED_SYMBOLS` 能力；如果后续要把更多 intermediate section 暴露给 LSP，还需要继续扩展 text metadata reader。
- schema 目前只覆盖 `zr_vm_library/project.c` 已知字段；如果 project loader 后续扩展出数组或嵌套对象结构，需要同步升级 schema。
