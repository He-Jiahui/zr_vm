---
related_code:
  - tests/parser/test_artifact_schema.c
  - tests/cmake/parser-foundation-targets.cmake
implementation_files:
  - tests/parser/test_artifact_schema.c
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

# Artifact Schema 基础格式与合同测试

## 范围和职责

本文件建立固定 token、公开身份、签名和节视图，覆盖 ZRO 定宽公开合同、ZRS/ZRI 文本往返、身份失配、未知节、长度及 token 拒绝、签名图、member/property/relocation、domain-transfer 和 scheduler-schema 合同。多个断言或同一函数内顺序场景只构成其所属 Unity 注册项，前段失败可跳过后段。

本模块 54 个规范单元，其中 54 个 C 物理单元、0 个独立语言体；23 个真实 C 函数体，29 段 Doxygen。26 个旧键在混合叶原位复核，28 个新键按本模块追加，旧键不重复记新信用。

## 当前单元

| Canonical ID | kind | unit | decision |
| --- | --- | --- | --- |
| ASTEST-001 | macro | TEST_TYPE_ID | no-comment |
| ASTEST-002 | macro | TEST_TYPE_DEF_TOKEN | no-comment |
| ASTEST-003 | macro | TEST_TYPE_REF_TOKEN | no-comment |
| ASTEST-004 | macro | TEST_TYPE_SPEC_TOKEN | no-comment |
| ASTEST-005 | macro | TEST_SIGNATURE_TOKEN | no-comment |
| ASTEST-006 | macro | TEST_CONSTRUCTOR_TOKEN | no-comment |
| ASTEST-007 | macro | TEST_MEMBER_TOKEN | no-comment |
| ASTEST-008 | macro | TEST_TYPE_REF_HASH | no-comment |
| ASTEST-009 | macro | TEST_TYPE_SPEC_HASH | no-comment |
| ASTEST-010 | macro | TEST_SIGNATURE_HASH | no-comment |
| ASTEST-011 | macro | TEST_LAYOUT_HASH | no-comment |
| ASTEST-012 | macro | TEST_CONTRACT_HASH | no-comment |
| ASTEST-013 | macro | TEST_MODULE_HASH | no-comment |
| ASTEST-014 | struct | SZrArtifactTestFixture | commented |
| ASTEST-015 | function | setUp | no-comment |
| ASTEST-016 | function | tearDown | BUG |
| ASTEST-017 | declaration | test_real_source_compile_and_binary_signature_import_are_identical | no-comment |
| ASTEST-018 | declaration | test_real_source_scheduler_call_publishes_canonical_source_fact | no-comment |
| ASTEST-019 | declaration | test_repeated_scheduler_calls_coalesce_canonical_source_fact | no-comment |
| ASTEST-020 | declaration | test_source_without_scheduler_call_publishes_no_scheduler_fact | no-comment |
| ASTEST-021 | declaration | test_real_source_scheduler_call_writes_and_imports_canonical_artifact | no-comment |
| ASTEST-022 | declaration | test_source_without_scheduler_call_rejects_artifact_write | no-comment |
| ASTEST-023 | declaration | test_scheduler_artifact_writer_rejects_unavailable_provider | no-comment |
| ASTEST-024 | declaration | test_artifact_metadata_graph_roundtrips_zri_and_zro | no-comment |
| ASTEST-025 | declaration | test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs | no-comment |
| ASTEST-026 | declaration | test_artifact_metadata_projection_matches_source_native_and_binary | no-comment |
| ASTEST-027 | function | write_u32 | no-comment |
| ASTEST-028 | function | make_identity | commented |
| ASTEST-029 | function | init_fixture | commented |
| ASTEST-030 | state | init_fixture::strings | commented |
| ASTEST-031 | state | init_fixture::syntaxTree | commented |
| ASTEST-032 | state | init_fixture::semanticIr | commented |
| ASTEST-033 | state | init_fixture::code | commented |
| ASTEST-034 | block | init_fixture::kind_sections | commented |
| ASTEST-035 | function | write_fixture | commented |
| ASTEST-036 | function | test_zro_roundtrips_fixed_width_public_contract_sections | commented |
| ASTEST-037 | function | assert_text_roundtrip | commented |
| ASTEST-038 | function | test_zrs_and_zri_have_readable_stable_roundtrips | commented |
| ASTEST-039 | function | test_public_identity_mismatches_are_precise | commented |
| ASTEST-040 | function | test_reader_rejects_unknown_mandatory_but_skips_unknown_optional_section | commented |
| ASTEST-041 | function | test_reader_rejects_truncation_count_limit_and_illegal_tokens | commented |
| ASTEST-042 | function | test_writer_and_reader_reject_dangling_member_owner_token | commented |
| ASTEST-043 | function | test_signature_nodes_validate_ref_readonly_owner_and_callable_contracts | commented |
| ASTEST-044 | block | test_signature_nodes_validate_ref_readonly_owner_and_callable_contracts::hash_self_check | TODO |
| ASTEST-045 | function | test_repeat_encoding_and_text_roundtrip_keep_hashes_stable | commented |
| ASTEST-046 | function | test_internal_public_rows_report_the_exact_mismatch | commented |
| ASTEST-047 | function | test_zero_many_and_duplicate_signature_rows_roundtrip_safely | commented |
| ASTEST-048 | function | test_duplicate_forbidden_and_recursive_signature_inputs_are_rejected | commented |
| ASTEST-049 | function | test_member_property_and_relocation_contracts_validate_tokens_and_code_bounds | commented |
| ASTEST-050 | function | test_source_canonical_type_and_binary_import_share_type_id_and_public_contract | BUG |
| ASTEST-051 | state | test_source_canonical_type_and_binary_import_share_type_id_and_public_contract::code | commented |
| ASTEST-052 | function | test_domain_transfer_contract_roundtrips_as_independent_artifact_schema | commented |
| ASTEST-053 | function | test_scheduler_contract_roundtrips_as_independent_artifact_schema | commented |
| ASTEST-054 | function | main | commented |

## 参数、失败与资源边界

按各单元系统合同和原调用者约束审查；借用的栈行、section.data、static payload、编译器/VM 对象不因台账迁移转移所有权。普通尾部销毁与 Unity 非局部失败分开说明，参数、返回、无注释判断和原 EOL 锚均绑定冻结 F1。只追加完整 EOF 注释和空白，保留全部原程序字节、字节偏移与行列；不据此声称执行了 compiler lexer/AST 或验证了全部平台。

保留 ARTIFACT-UNITY-ASSERT-CLEANUP-LEGACY：VM/context/AST/function 或 FILE 取得后，Unity 断言非局部跳转可跳过普通尾部释放，空 tearDown 不补偿；fatal crash guard 还可能跳过 tearDown。本批不修行为，不把正常用例成功当作故障清理证明。

保留 ARTIFACT-HASH-SELF-CHECK-LEGACY TODO：相同输入与自身哈希比较不是独立正确性预言；后续核查入口为 tests/parser/test_artifact_schema.c:522，当前只确认既有非零及自比较断言的有限证据。

## 实际源码和交付证据

独立语义审查：parser-artifact-schema-tests-current-contract-independent-review-r2.json SHA256 51b61e137aa5fd3be505caf615eeb1e915b39b76e3bc768426489bb757d6803f；writer9820/fresh35732实际0，effectiveR2合同接受。

源码应用与保持性：parser-artifact-schema-tests-after-r3-source-restore.json SHA256 0a110502353a9d657446f94b714d071e04e433341b7298cc604153ffdf0068a6；三源精确55Dox恢复、原程序前缀/行列/EOL保持。原文件 SHA256 为 3c2dc7d39b6c67ff616afd146c3e92841afa8137c0e8e489fa484042f60707fd；EOF suffix SHA256 为 3da11629d74923f0d368efba7a56283aaab5ff8c59807af30a4dd423f869fab1（31295 B/608 LF）；应用后 SHA256 为 0ad2b50a221c5e8ce9515b2fdcc2decde3457f6d357009fd7335d80031442ba0（92848 B/1869 LF）。全部原 61553 B 前缀与原行锚保留。

## 现有验证及其限度

验证事实：parser-artifact-schema-tests-native-final-acceptance-r2.json SHA256 cca5ff896cb1f155aaf8dbecb68a05f63692282014679c182c1e4f82c110a4db；ClangCL Debug before-schema-tests-r2/after-schema-tests-r3构建及8direct均0，57 Unity无失败，16原始流/28工件相同；无WSL/CTest/故障注入信用。本批真实基线为当前程序 before-schema-tests-r2，配对为 after-schema-tests-r3；只相差三个测试文件的精确55Dox后缀。历史产品 parser-artifact-family-native-after-r2 以及较早SCCP版本的 before-schema-tests-r1均仅保留历史，不替代本批基线。静态注册数不替代实际原始结果。

现有目标是 zr_vm_artifact_schema_test，CTest 名 artifact_schema，direct 入口无 selector/参数，main 返回 Unity 失败数。当前三个文件总共注册 25 项（本文件 15 项），本文件 151 处 TEST_ASSERT 是静态调用点。共享目标每次仍会运行全部 25 项，不按三个模块重复累计一次运行，也不把 family 的 57 项当作本文件的新用例。

本目标包括 source-roundtrip 的实际文件写入路径；即使交付 main 或 metadata 模块，也应在 Root 预登记的独占可写 E tests cwd 中运行。固定 scheduler_source_contract.zro 的普通路径关闭/删除，断言失败可能保留文件；不得覆盖其他会话夹具或把两个负例的返回状态当作文件不存在断言。当前 CMake 片段没有本 suite 专属 TIMEOUT 或 WORKING_DIRECTORY。没有新增测试、断言、selector、运行语言体或行为修复。

## 台账、清单与后续门禁

仅本模块的 26 个旧行原位更新和 28 个新键追加；混合叶外部 72 条数据和 header 原字节保留。其余两个模块的已交付行或尚未交付原行按当期父版本保留。主 inventory 只改本源码既有键的 sha/status/note 三列，不能暂存整份外部 worktree 清单。每模块真实 parent、key 投影、外部字节、normal/private index、blob/mode、缓存差异检查及串行提交须由各自门禁绑定，候选不预授未来版本权限。

## 可追溯来源

- 冻结 F1：parser-artifact-schema-tests-current-contract-review-r1.json，SHA256 f1207a94096678b7d7acbd789b4170979a1c2bb6427af2af80748a1ce59c4c3e。
- 候选分发：parser-artifact-schema-tests-three-module-ledger-candidate-r1.json；所有事实槽位须以实际报告路径、完整 SHA256、进程终态和证据范围替换后才可应用文档。
- 调用注册：tests/cmake/parser-foundation-targets.cmake:203–224；主 runner：tests/parser/test_artifact_schema.c:1232–1261。
- 借用产品 EOF 保持性：parser-artifact-family-source-comment-equivalence-r1.json（7de541c3ee1dc7cf5159167e80d346cbfbc0551f859871f2ec7d27d2506c0d8b），不借定义或新增信用。
