# LSP 03：Position、Snapshot 与鲁棒性

## 坐标契约

内部source range使用byte offsets + document identity/checksum；LSP边界显式转换UTF-16 line/character。不得把UTF-8 byte、Unicode scalar和UTF-16 code unit混用。

## 文档状态

- didOpen建立versioned text snapshot。
- didChange按声明的sync kind应用；range edit先验证version和边界。
- parse/semantic任务绑定snapshot，完成时若document已更新则丢弃或缓存为历史，不发布旧diagnostic。
- didClose清理overlay并恢复workspace/on-disk provider。
- rename/code action/workspace edit在提交前重新验证所有document version/checksum。

## 边界测试

覆盖ASCII、CJK、emoji/surrogate pair、combining mark、CRLF/LF、空文件、EOF、无尾随newline、超长单行、minified semicolon source、invalid UTF input、rapid cancellation与乱序response。

## 增量失效

改变module/import、type/member signature、generic constraint、property accessor、resource/layout或`.zrp`时必须按ModuleIdentity/dependency graph传播。纯body局部edit只失效相关function CFG和查询cache。实现应测量reparse/rebind范围，而不是以“能返回结果”作为增量完成。

## 完成记录

[2026-06-20 position robustness baseline](./03-robustness/2026-06-20-position-robustness-baseline.md) 是现有位置与协议证据；snapshot race和全新module/package场景仍需扩展。

[2026-07-19 scoped semantic analysis foundation](./03-robustness/2026-07-19-scoped-semantic-analysis-foundation.md) 完成声明级分析入口、作用域根解析和completion空结果回退；这只是L6前置基础，不代表增量失效、snapshot race或性能预算完成。

## 增量图与资源预算

输入包括versioned document edits、workspace/module dependency graph、source encoding、artifact/module generation和cancellation token；任何输入版本不一致都必须先拒绝而非尝试合并。

| 变更 | 最小失效范围 |
|---|---|
| trivia/body local | syntax node + owning function CFG/query cache |
| local signature/generic/receiver | function与direct callers |
| public type/property/layout | module public hash及reverse dependencies |
| import/alias/package export | ModuleIdentity edge与affected namespace queries |
| `.zrp/.zrm` generation | package/provider graph、binary virtual docs |
| native descriptor/schema | module exports、signature help与ABI diagnostics |

第一版默认预算：warm hover p95 <= 50ms，completion/signature p95 <= 100ms，单文件diagnostics p95 <= 250ms，100-file reference workspace单文件增量p95 <= 500ms；cancellation在50ms内被观察。每个open document最多保留当前和两个历史text/semantic snapshots，workspace semantic cache默认上限256MiB并LRU淘汰；预算可配置但CI baseline不可静默放宽。

测试入口包含position acceptance、`tests/language_server/test_lsp_code_lens_utf16_ranges.c`、advanced editor feature、rapid didChange/cancel/close协议case。新增矩阵覆盖CJK/emoji/combining/CRLF、超长minified单行、100次乱序edit、package manifest变更、binary module replacement和stale response suppression。

退出条件：byte↔UTF-16 roundtrip无偏移；任何response/diagnostic对应请求或更新后的合法snapshot；失效范围有计数断言；性能与峰值内存报告可重复，超预算导致测试失败。
