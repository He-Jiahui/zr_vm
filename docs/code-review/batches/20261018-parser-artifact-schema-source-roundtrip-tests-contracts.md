---
related_code:
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/cmake/parser-foundation-targets.cmake
implementation_files:
  - tests/parser/test_artifact_schema_source_roundtrip.c
plan_sources:
  - user: 首方逐函数、人工夹具与调用链注释审查
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_artifact_schema_metadata_graph.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/cmake/parser-foundation-targets.cmake
doc_type: review-batch
---

# Artifact Schema 源码往返与调度事实测试

## 范围和职责

本文件把七段手写源码送入 parser/compiler，核验单个 int 参数/int 返回签名、canonical 类型、调度源事实合并/缺失、工件写入导入及 provider 拒绝。六个语言函数体分别追溯：一个 identity 和五个 lambda；它们的静态文本被 Parse/Compile 消费，没有 Function_Execute、任务调度或 result 的运行证明。

本模块 34 个规范单元，其中 28 个 C 物理单元、6 个独立语言体；9 个真实 C 函数体，18 段 Doxygen。10 个旧键在混合叶原位复核，24 个新键按本模块追加，旧键不重复记新信用。

## 当前单元

| Canonical ID | kind | unit | decision |
| --- | --- | --- | --- |
| ASTEST-078 | declaration | test_source_without_scheduler_call_rejects_artifact_write | no-comment |
| ASTEST-079 | declaration | test_scheduler_artifact_writer_rejects_unavailable_provider | no-comment |
| ASTEST-080 | macro | SOURCE_TYPE_REF_TOKEN | no-comment |
| ASTEST-081 | macro | SOURCE_TYPE_SPEC_TOKEN | no-comment |
| ASTEST-082 | macro | SOURCE_SIGNATURE_TOKEN | no-comment |
| ASTEST-083 | declaration | test_real_source_compile_and_binary_signature_import_are_identical | no-comment |
| ASTEST-084 | declaration | test_real_source_scheduler_call_publishes_canonical_source_fact | no-comment |
| ASTEST-085 | declaration | test_repeated_scheduler_calls_coalesce_canonical_source_fact | no-comment |
| ASTEST-086 | declaration | test_source_without_scheduler_call_publishes_no_scheduler_fact | no-comment |
| ASTEST-087 | declaration | test_real_source_scheduler_call_writes_and_imports_canonical_artifact | no-comment |
| ASTEST-088 | function | test_real_source_compile_and_binary_signature_import_are_identical | BUG |
| ASTEST-089 | state | test_real_source_compile_and_binary_signature_import_are_identical::source | commented |
| ASTEST-090 | function | create_scheduler_artifact_test_state | commented |
| ASTEST-091 | function | compile_scheduler_artifact_source | commented |
| ASTEST-092 | function | test_real_source_scheduler_call_publishes_canonical_source_fact | BUG |
| ASTEST-093 | state | test_real_source_scheduler_call_publishes_canonical_source_fact::source | commented |
| ASTEST-094 | function | test_repeated_scheduler_calls_coalesce_canonical_source_fact | BUG |
| ASTEST-095 | state | test_repeated_scheduler_calls_coalesce_canonical_source_fact::source | commented |
| ASTEST-096 | function | test_source_without_scheduler_call_publishes_no_scheduler_fact | BUG |
| ASTEST-097 | state | test_source_without_scheduler_call_publishes_no_scheduler_fact::source | commented |
| ASTEST-098 | function | test_real_source_scheduler_call_writes_and_imports_canonical_artifact | BUG |
| ASTEST-099 | state | test_real_source_scheduler_call_writes_and_imports_canonical_artifact::source | commented |
| ASTEST-100 | state | test_real_source_scheduler_call_writes_and_imports_canonical_artifact::artifactPath | commented |
| ASTEST-101 | block | test_real_source_scheduler_call_writes_and_imports_canonical_artifact::file_read_cleanup | BUG |
| ASTEST-102 | function | test_source_without_scheduler_call_rejects_artifact_write | BUG |
| ASTEST-103 | state | test_source_without_scheduler_call_rejects_artifact_write::source | commented |
| ASTEST-104 | function | test_scheduler_artifact_writer_rejects_unavailable_provider | BUG |
| ASTEST-105 | state | test_scheduler_artifact_writer_rejects_unavailable_provider::source | commented |
| ASTEST-LANG-001 | fixture-function | test_real_source_compile_and_binary_signature_import_are_identical::source::identity | commented |
| ASTEST-LANG-002 | fixture-lambda | test_real_source_scheduler_call_publishes_canonical_source_fact::source::lambda-1 | commented |
| ASTEST-LANG-003 | fixture-lambda | test_repeated_scheduler_calls_coalesce_canonical_source_fact::source::lambda-1 | commented |
| ASTEST-LANG-004 | fixture-lambda | test_repeated_scheduler_calls_coalesce_canonical_source_fact::source::lambda-2 | commented |
| ASTEST-LANG-005 | fixture-lambda | test_real_source_scheduler_call_writes_and_imports_canonical_artifact::source::lambda-1 | commented |
| ASTEST-LANG-006 | fixture-lambda | test_scheduler_artifact_writer_rejects_unavailable_provider::source::lambda-1 | commented |

## 手写语言函数体

以下六项复用对应 static source state 的 Doxygen 段落与原 C 行锚，单列规范键，未另计 C 函数或重复文档。源码字符串保持原字节。

| Canonical ID | C 容器原行 | 语言体 | 证据边界 |
| --- | --- | --- | --- |
| ASTEST-LANG-001 | 37 | fn identity(value: int): int { return value; } | Parse/Compile-only；未执行 |
| ASTEST-LANG-002 | 200 | fn() => { return 7; } | Parse/Compile-only；未执行 |
| ASTEST-LANG-003 | 238 | fn() => { return 7; } | Parse/Compile-only；未执行 |
| ASTEST-LANG-004 | 239 | fn() => { return 8; } | Parse/Compile-only；未执行 |
| ASTEST-LANG-005 | 291 | fn() => { return 7; } | Parse/Compile-only；未执行 |
| ASTEST-LANG-006 | 476 | fn() => { return 7; } | Parse/Compile-only；未执行 |

## 参数、失败与资源边界

按各单元系统合同和原调用者约束审查；借用的栈行、section.data、static payload、编译器/VM 对象不因台账迁移转移所有权。普通尾部销毁与 Unity 非局部失败分开说明，参数、返回、无注释判断和原 EOL 锚均绑定冻结 F1。只追加完整 EOF 注释和空白，保留全部原程序字节、字节偏移与行列；不据此声称执行了 compiler lexer/AST 或验证了全部平台。

保留 ARTIFACT-UNITY-ASSERT-CLEANUP-LEGACY：VM/context/AST/function 或 FILE 取得后，Unity 断言非局部跳转可跳过普通尾部释放，空 tearDown 不补偿；fatal crash guard 还可能跳过 tearDown。本批不修行为，不把正常用例成功当作故障清理证明。

## 实际源码和交付证据

独立语义审查：parser-artifact-schema-tests-current-contract-independent-review-r2.json SHA256 51b61e137aa5fd3be505caf615eeb1e915b39b76e3bc768426489bb757d6803f；writer9820/fresh35732实际0，effectiveR2合同接受。

源码应用与保持性：parser-artifact-schema-tests-after-r3-source-restore.json SHA256 0a110502353a9d657446f94b714d071e04e433341b7298cc604153ffdf0068a6；三源精确55Dox恢复、原程序前缀/行列/EOL保持。原文件 SHA256 为 11036ddf0ee5ac3aa73f25f66c343466999ae7a27ca5f11668efc3be4fc493b4；EOF suffix SHA256 为 c921c8dbfa3ed053b219b169d3a7774f56827b5a5e0cfead8476089063c9e8b1（25497 B/393 LF）；应用后 SHA256 为 39610b63152c9936c9dcf653b77aa7d19515b2da4d2f20863e2e69a6b61d9af9（49056 B/896 LF）。全部原 23559 B 前缀与原行锚保留。

## 现有验证及其限度

验证事实：parser-artifact-schema-tests-native-final-acceptance-r2.json SHA256 cca5ff896cb1f155aaf8dbecb68a05f63692282014679c182c1e4f82c110a4db；ClangCL Debug before-schema-tests-r2/after-schema-tests-r3构建及8direct均0，57 Unity无失败，16原始流/28工件相同；无WSL/CTest/故障注入信用；运行结果仅限该冻结窗口，新foreign程序不继承运行信用，后续边界为parser-artifact-schema-post-native-foreign-drift-boundary-r2.json SHA256 19f5115252c7dae65294c9b29184c060bed82e6e3c529eb3a3d12236ae8dd12c。本批运行证据仅限配对冻结窗口： before-schema-tests-r2，配对为 after-schema-tests-r3；两个冻结端点只相差三个测试文件的精确55Dox后缀。冻结后并发程序已有变更，本注释批次不据旧运行结果声明这些新程序已验证。历史产品 parser-artifact-family-native-after-r2 以及较早SCCP版本的 before-schema-tests-r1均仅保留历史，不替代本批基线。静态注册数不替代实际原始结果。

现有目标是 zr_vm_artifact_schema_test，CTest 名 artifact_schema，direct 入口无 selector/参数，main 返回 Unity 失败数。当前三个文件总共注册 25 项（本文件 7 项），本文件 82 处 TEST_ASSERT 是静态调用点。共享目标每次仍会运行全部 25 项，不按三个模块重复累计一次运行，也不把 family 的 57 项当作本文件的新用例。

本目标包括 source-roundtrip 的实际文件写入路径；即使交付 main 或 metadata 模块，也应在 Root 预登记的独占可写 E tests cwd 中运行。固定 scheduler_source_contract.zro 的普通路径关闭/删除，断言失败可能保留文件；不得覆盖其他会话夹具或把两个负例的返回状态当作文件不存在断言。当前 CMake 片段没有本 suite 专属 TIMEOUT 或 WORKING_DIRECTORY。没有新增测试、断言、selector、运行语言体或行为修复。

## 台账、清单与后续门禁

仅本模块的 10 个旧行原位更新和 24 个新键追加；混合叶外部 72 条数据和 header 原字节保留。其余两个模块的已交付行或尚未交付原行按当期父版本保留。主 inventory 只改本源码既有键的 sha/status/note 三列，不能暂存整份外部 worktree 清单。每模块真实 parent、key 投影、外部字节、normal/private index、blob/mode、缓存差异检查及串行提交须由各自门禁绑定，候选不预授未来版本权限。

## 可追溯来源

- 冻结 F1：parser-artifact-schema-tests-current-contract-review-r1.json，SHA256 f1207a94096678b7d7acbd789b4170979a1c2bb6427af2af80748a1ce59c4c3e。
- 候选分发：parser-artifact-schema-tests-three-module-ledger-candidate-r1.json；所有事实槽位须以实际报告路径、完整 SHA256、进程终态和证据范围替换后才可应用文档。
- 调用注册：tests/cmake/parser-foundation-targets.cmake:203–224；主 runner：tests/parser/test_artifact_schema.c:1232–1261。
- 借用产品 EOF 保持性：parser-artifact-family-source-comment-equivalence-r1.json（7de541c3ee1dc7cf5159167e80d346cbfbc0551f859871f2ec7d27d2506c0d8b），不借定义或新增信用。
