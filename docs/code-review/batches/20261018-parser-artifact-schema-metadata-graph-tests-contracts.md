---
related_code:
  - tests/parser/test_artifact_schema_metadata_graph.c
  - tests/cmake/parser-foundation-targets.cmake
implementation_files:
  - tests/parser/test_artifact_schema_metadata_graph.c
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

# Artifact Schema 元数据图测试

## 范围和职责

本文件用栈内类型、签名、字符串和节行建立元数据图，核验 ZRI/ZRO 图往返、剥离/伪造/损坏拒绝，以及源码、native 和二进制投影的一致字段。内部指针借用初始化后的本对象或只读载荷，普通按值复制不自动重定位。GRAPH_ADD_SECTION 在初始化夹具时追加带 ZR_ARTIFACT_SECTION_FLAG_MANDATORY 标志的借用节视图并递增 sectionCount；宏展开不是独立 C 函数。

本模块 23 个规范单元，其中 23 个 C 物理单元、0 个独立语言体；7 个真实 C 函数体，8 段 Doxygen。9 个旧键在混合叶原位复核，14 个新键按本模块追加，旧键不重复记新信用。

## 当前单元

| Canonical ID | kind | unit | decision |
| --- | --- | --- | --- |
| ASTEST-055 | macro | GRAPH_TYPE_ID | no-comment |
| ASTEST-056 | macro | GRAPH_TYPE_DEF_TOKEN | no-comment |
| ASTEST-057 | macro | GRAPH_TYPE_REF_TOKEN | no-comment |
| ASTEST-058 | macro | GRAPH_TYPE_SPEC_TOKEN | no-comment |
| ASTEST-059 | macro | GRAPH_MEMBER_TOKEN | no-comment |
| ASTEST-060 | macro | GRAPH_SIGNATURE_TOKEN | no-comment |
| ASTEST-061 | macro | GRAPH_TYPE_HASH | no-comment |
| ASTEST-062 | macro | GRAPH_LAYOUT_HASH | no-comment |
| ASTEST-063 | macro | GRAPH_CONTRACT_HASH | no-comment |
| ASTEST-064 | macro | GRAPH_MODULE_HASH | no-comment |
| ASTEST-065 | declaration | test_artifact_metadata_graph_roundtrips_zri_and_zro | no-comment |
| ASTEST-066 | declaration | test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs | no-comment |
| ASTEST-067 | declaration | test_artifact_metadata_projection_matches_source_native_and_binary | no-comment |
| ASTEST-068 | struct | SZrArtifactMetadataGraphFixture | commented |
| ASTEST-069 | function | graph_write_u32 | no-comment |
| ASTEST-070 | function | graph_init_fixture | commented |
| ASTEST-071 | macro | GRAPH_ADD_SECTION(KIND, COUNT, DATA) | commented |
| ASTEST-072 | block | graph_init_fixture::GRAPH_ADD_SECTION_undef | no-comment |
| ASTEST-073 | function | graph_write_fixture | commented |
| ASTEST-074 | function | graph_assert_roundtrip | commented |
| ASTEST-075 | function | test_artifact_metadata_graph_roundtrips_zri_and_zro | commented |
| ASTEST-076 | function | test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs | commented |
| ASTEST-077 | function | test_artifact_metadata_projection_matches_source_native_and_binary | commented |

## 参数、失败与资源边界

按各单元系统合同和原调用者约束审查；借用的栈行、section.data、static payload、编译器/VM 对象不因台账迁移转移所有权。普通尾部销毁与 Unity 非局部失败分开说明，参数、返回、无注释判断和原 EOL 锚均绑定冻结 F1。只追加完整 EOF 注释和空白，保留全部原程序字节、字节偏移与行列；不据此声称执行了 compiler lexer/AST 或验证了全部平台。

本文件本批没有新 BUG/TODO。共同运行的其他翻译单元仍保留 F1 的既有问题，不能因本模块没有问题状态就称整个目标故障路径已解决。

## 实际源码和交付证据

独立语义审查：parser-artifact-schema-tests-current-contract-independent-review-r2.json SHA256 51b61e137aa5fd3be505caf615eeb1e915b39b76e3bc768426489bb757d6803f；writer9820/fresh35732实际0，effectiveR2合同接受。

源码应用与保持性：parser-artifact-schema-tests-after-r3-source-restore.json SHA256 0a110502353a9d657446f94b714d071e04e433341b7298cc604153ffdf0068a6；三源精确55Dox恢复、原程序前缀/行列/EOL保持。原文件 SHA256 为 87064de9dcfffa74840c65ac9f439ec43e4ca75c90050835bbab19bdfd9657b0；EOF suffix SHA256 为 d27cca4f08b6cd74917c4f110f68bc88a603409c46774e47e67c765a0bcf5fe4（11871 B/203 LF）；应用后 SHA256 为 bc5275d313fb35a2830d8b5b83d5f3b78221ca0273c0f21a24b0d898d7ecf529（34062 B/727 LF）。全部原 22191 B 前缀与原行锚保留。

## 现有验证及其限度

验证事实：parser-artifact-schema-tests-native-final-acceptance-r2.json SHA256 cca5ff896cb1f155aaf8dbecb68a05f63692282014679c182c1e4f82c110a4db；ClangCL Debug before-schema-tests-r2/after-schema-tests-r3构建及8direct均0，57 Unity无失败，16原始流/28工件相同；无WSL/CTest/故障注入信用；运行结果仅限该冻结窗口，新foreign程序不继承运行信用，后续边界为parser-artifact-schema-post-native-foreign-drift-boundary-r2.json SHA256 19f5115252c7dae65294c9b29184c060bed82e6e3c529eb3a3d12236ae8dd12c。本批运行证据仅限配对冻结窗口： before-schema-tests-r2，配对为 after-schema-tests-r3；两个冻结端点只相差三个测试文件的精确55Dox后缀。冻结后并发程序已有变更，本注释批次不据旧运行结果声明这些新程序已验证。历史产品 parser-artifact-family-native-after-r2 以及较早SCCP版本的 before-schema-tests-r1均仅保留历史，不替代本批基线。静态注册数不替代实际原始结果。

现有目标是 zr_vm_artifact_schema_test，CTest 名 artifact_schema，direct 入口无 selector/参数，main 返回 Unity 失败数。当前三个文件总共注册 25 项（本文件 3 项），本文件 36 处 TEST_ASSERT 是静态调用点。共享目标每次仍会运行全部 25 项，不按三个模块重复累计一次运行，也不把 family 的 57 项当作本文件的新用例。

本目标包括 source-roundtrip 的实际文件写入路径；即使交付 main 或 metadata 模块，也应在 Root 预登记的独占可写 E tests cwd 中运行。固定 scheduler_source_contract.zro 的普通路径关闭/删除，断言失败可能保留文件；不得覆盖其他会话夹具或把两个负例的返回状态当作文件不存在断言。当前 CMake 片段没有本 suite 专属 TIMEOUT 或 WORKING_DIRECTORY。没有新增测试、断言、selector、运行语言体或行为修复。

## 台账、清单与后续门禁

仅本模块的 9 个旧行原位更新和 14 个新键追加；混合叶外部 72 条数据和 header 原字节保留。其余两个模块的已交付行或尚未交付原行按当期父版本保留。主 inventory 只改本源码既有键的 sha/status/note 三列，不能暂存整份外部 worktree 清单。每模块真实 parent、key 投影、外部字节、normal/private index、blob/mode、缓存差异检查及串行提交须由各自门禁绑定，候选不预授未来版本权限。

## 可追溯来源

- 冻结 F1：parser-artifact-schema-tests-current-contract-review-r1.json，SHA256 f1207a94096678b7d7acbd789b4170979a1c2bb6427af2af80748a1ce59c4c3e。
- 候选分发：parser-artifact-schema-tests-three-module-ledger-candidate-r1.json；所有事实槽位须以实际报告路径、完整 SHA256、进程终态和证据范围替换后才可应用文档。
- 调用注册：tests/cmake/parser-foundation-targets.cmake:203–224；主 runner：tests/parser/test_artifact_schema.c:1232–1261。
- 借用产品 EOF 保持性：parser-artifact-family-source-comment-equivalence-r1.json（7de541c3ee1dc7cf5159167e80d346cbfbc0551f859871f2ec7d27d2506c0d8b），不借定义或新增信用。
