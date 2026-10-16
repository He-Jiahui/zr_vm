---
doc_type: code-review-batch
related_code:
  - docs/code-review/inventory.tsv
  - scripts/code_review_inventory.py
implementation_files:
  - scripts/code_review_inventory.py
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/code-review/batches/20261013-scope-boundaries.md
tests: []
---

# 文件清单刷新与旧审查版本漂移登记

本批只更新文件级清单元数据，新增逐单元审查覆盖为 **0**。Root 在 `1ff40154ac353fb9414174ef22075dd2ab61e387` 上重新核对 HEAD、普通索引、工作树原始行及源码 raw SHA256、字节数、读取稳定性，选定 275 行。选定行均保持或进入 pending，函数、结构、接口仍需后续逐单元复审。

提案有限扫描为 4111 个输入，独立复核时为 4112 个；时间点不同，晚发现文件未自动并入。309 行提案中有 275 行可拆分候选、34 行延后。本轮实际新增 pending 128 行，刷新 pending raw 哈希 103 行，旧 reviewed 漂移退回 pending 44 行；端点重新核验再延后 0 行。本批没有新增 reviewed 行。

外来工作树 53 行的字段、顺序、原始行字节，以及已提交 Constants 4 行、Deopt 3 行均保留。暂存以当前 HEAD 原始清单为底，只应用本批选定字段和新增行，未暂存外来 53 行。既有身份前三列保持不变；pending 刷新只替换 raw SHA256；reviewed 漂移保留旧 hash、旧 leaf 引用和旧 note；新增行按路径排序追加。

原 42 项中的 4 个注释/格式等价项也退回 pending，词法等价不足以证明当前完整调用契约和旧物理定义锚点。后发现的 GC budget 两行一并登记，CLI 晚发现漂移仍延后。573 组旧 leaf file 字段及可能的重命名迁移留待单独复审，本批没有迁移叶台账或借同名符号取得覆盖。

首方文本归属以固定 scanner、构建/include 和独立手工入口证据交叉核查，不根据 dormant、AOT、generated 等目录名称推断调用状态。新增文件发现与入口证据保存在提案逐文件记录，文件进入清单不代表函数契约已审查。

验证范围为逐行提案、源码 raw/字节数、三种清单状态及未选中原始行。本批只改清单和 Markdown，不执行产品构建或测试；提交前执行普通索引完整/本批及自有备用索引的 git diff --cached --check，并独立核对真实暂存内容。其他产品批次的构建证据不扩展为本批或全仓库运行验收。

不可变证据位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/：20261016-inventory-refresh-proposal-r1.json、20261016-inventory-refresh-independent-proposal-review-r1.json、20261016-reviewed-drift-attribution-audit-r1.json，以及本批 Root lease、handoff、serial receipt、独立 staged review。必要证据、共享缓存与复用构建继续保留；本批释放 0 B，产物生命周期仍开放。

## 本批实际选定行

旧完整行、新完整行和两次源码端点保存在 Root lease。下表为文件级操作，全部不计逐单元审查覆盖。

| 文件 | 操作 | 原 raw SHA256 | 当前 raw SHA256 |
| --- | --- | --- | --- |
| `.github/workflows/cmake-multi-platform.yml` | reviewed → pending | `0cd475b4de04da9ca593c6afb6e33ca9fa9dc229025210058702d72083917e3a` | `00b4cccd4b48cc0ee7a55bb496ef3530f453764e879e8267a511b08e913a8cf3` |
| `.github/workflows/wiki-pages.yml` | reviewed → pending | `ae5880295c8efaef9384d0e7a6d68be25022a7085c1c731c549ae32c3b60adfa` | `3a51c00413b65dbc0361e5d982e3bafe134aa54473f7078adab99d7e98d76a6c` |
| `scripts/artifacts/ExecutePlan.ps1` | 新增 pending | `—` | `32b857a94e00e97aaa34147a654d04670edae125be06854b99c05b42a3f79e5c` |
| `scripts/artifacts/ScheduledMaintenance.ps1` | 新增 pending | `—` | `f66e1d9c52f114820bdda5f136b842e73f710e021aaa7178e74add4a9418b89f` |
| `scripts/artifacts/maintenance.py` | 新增 pending | `—` | `b0cb7e4edbdd636c73d46afb56782e66f03709e42b34e4817c619acc69b01a8d` |
| `scripts/artifacts/policy.py` | 新增 pending | `—` | `0bb956c716eb383783c03bab8fe529873dcff846c100338f823bd1ddbf24377e` |
| `scripts/benchmark/aggregate_benchmark_summary.py` | reviewed → pending | `df98d428e51a6bd182f4767f885f74504ed304e33d688249154c4bfae1e62802` | `3506220bf4da82fcff6d56e0c603458202766b17440efb03b544a051e54a6547` |
| `scripts/benchmark/benchmark_compare_viewer.html` | reviewed → pending | `f1523ed2f161338928df6808ff0b8c476135f941127e0ac4a5f7ceba2870dc28` | `6a402e49cba4f988e6b31bb75027d4565a932b4461807ec8292ec06aa8795bc2` |
| `scripts/benchmark/benchmark_reports_to_csv.py` | reviewed → pending | `a9c637d612ac63e7c29bbb941c96dffa72d024ebedef57ecf9d7d70e87391f05` | `9b47af95ba983f9a11425bff24f8385cff7494d719d95b979191aeab659deead` |
| `scripts/layout/generated_includes.py` | 新增 pending | `—` | `995acf122bdb38405ad42453416f9c605de435b8dea3b7096680d572d550262d` |
| `scripts/syntax_migration_embedded.py` | 新增 pending | `—` | `20219c380f6f98eb10d77d6eb59b46c5caef801e0af8278242c93d67b6f4e476` |
| `scripts/syntax_migration_inventory.py` | reviewed → pending | `994971db29a74121fe6bd46a7bce256ca4a58070baac4a5e50de6dd747f45c01` | `c036a94c182899d949a13c2c21860c9c68a6807305c467018a804cdfcb655022` |
| `scripts/validate_wiki.py` | reviewed → pending | `b3c6029413efb23bb3d520f3f14f07206d4364295ec36f431c7ad64fb532e6bf` | `7d4cfb99067d827685d27c756374f554dfac98f59deeec2f9c038534ce2ae66d` |
| `tests/benchmarks/benchmark_compare_viewer_harness.js` | 新增 pending | `—` | `d1ce4cbb4999f4e0709c0072864824d30721060ffdb5dff4dd4b8f58fb3ea19d` |
| `tests/benchmarks/test_benchmark_compare_viewer.py` | 新增 pending | `—` | `57ffd004ae779d61d622ae3cb3310b01db3af672c423a7ee8076e4b7bb5df394` |
| `tests/benchmarks/test_benchmark_consumer_scope_types.py` | 新增 pending | `—` | `3e37879ca3eac21fd20541b6047d5f20151e28c26d30176fb9e50c7fc14bb016` |
| `tests/cli/test_cli_command_boundaries.c` | 新增 pending | `—` | `447d234ee77b27d6dd8bb14186d8871416f743579255f3286d03e82836f0c54a` |
| `tests/cli/test_cli_repl_physical_lines.py` | 新增 pending | `—` | `288e495cab3974942c05755a192bc72229a245884ddae303b735a82a75477233` |
| `tests/cmake/artifact-row-boundary-tests.cmake` | 新增 pending | `—` | `9ac75780a5b96fabdaf8cdeb396af63fbd67fcdf3be164a14dec65a6929811d8` |
| `tests/cmake/benchmark-scope-types-tests.cmake` | 新增 pending | `—` | `d503a82286879fbf80f6f3feca1b1e513eb661409713aae4ae20cc9404ac0d1d` |
| `tests/cmake/benchmark-viewer-tests.cmake` | 新增 pending | `—` | `3f4fbc15539955ba703fe9ce70ae7fa86268564cba6a0dc7e12de024047a3b3a` |
| `tests/cmake/cli-command-boundary-tests.cmake` | 新增 pending | `—` | `68d45286366997e7cea1692b7a10956215547977fb4e3ad7e3adadb08115857a` |
| `tests/cmake/cli-repl-physical-line-tests.cmake` | 新增 pending | `—` | `2b87f0ead2e3a7469c3c4731b9d617be5ec2547b5a0e5ddc2f3ce2bb25880114` |
| `tests/cmake/core-allocation-lifecycle-tests.cmake` | 新增 pending | `—` | `13492cfb83b80ba698f0244a0cf24ced4e2118d84bbee9cda19277d516d28832` |
| `tests/cmake/core-bootstrap-support-oom-tests.cmake` | 新增 pending | `—` | `27963eb2ee6c436d5aef46a1a090ad575320c2a99ba4953a1e4764d3c2090b47` |
| `tests/cmake/core-exception-oom-tests.cmake` | 新增 pending | `—` | `6e50f41c3b85c82598e9012e33cdf31d67e9f7f79595371557bc61deae1f60be` |
| `tests/cmake/exception-construction-roots-tests.cmake` | 新增 pending | `—` | `247453dd89385d8fbb3eb833f9eaf215f143952f89b372189b2887eb5f272b64` |
| `tests/cmake/file-path-operations-tests.cmake` | 新增 pending | `—` | `b0af4228baeea42e3fe78d123d2a93c428a29ffb4ccfd5971e4dbb2df15645f6` |
| `tests/cmake/gc-allocation-debt-tests.cmake` | 新增 pending | `—` | `751b5413d78e4820c7d3e5106d287865e71d4b8f1e60d37fb2fe81c26522f978` |
| `tests/cmake/gc-contract-boundary-tests.cmake` | 新增 pending | `—` | `334ce2b27206ef341fbc3b7a9a9382ce5cea21ea38898c0f2a58eb9c255098d3` |
| `tests/cmake/gc-root-leases-tests.cmake` | 新增 pending | `—` | `4d3a431236a3cdd1a677983b0eb023ab9a51c1d58e6fe052aaa2702cac905cd1` |
| `tests/cmake/gc-scan-exception-cleanup-tests.cmake` | 新增 pending | `—` | `484208e8a723a0c5a58faf418d353fc96353645f595654f4c7c578f426147001` |
| `tests/cmake/lsp-array-oom-tests.cmake` | 新增 pending | `—` | `56223a791aa89a2c821349547b1e89bd26cea35ca3fc4ef294421f4615e1f638` |
| `tests/cmake/math-input-boundary-tests.cmake` | 新增 pending | `—` | `45e094c419c35e2d9c5d44be79c3fe6ea7db517002d1a95e054f93c4466feb72` |
| `tests/cmake/meta-string-oom-tests.cmake` | 新增 pending | `—` | `b1845f10c37ee09ce7ee0cfb4f671ce0bad2b4fc1f04fbcfb54db6ce00ca2dac` |
| `tests/cmake/native-array-direct-validation/CMakeLists.txt` | 新增 pending | `—` | `b8421445db74792de2db35ed14bcba7a9db562bbff50b36a137b700b3e594128` |
| `tests/cmake/native-array-tests.cmake` | 新增 pending | `—` | `57038087e457c3d13d708e45c28343d3beff160fa89072797b7e2c9e38caed29` |
| `tests/cmake/native-dispatch-self-oom-tests.cmake` | 新增 pending | `—` | `bb0d5f6e3976edb632a8db520a7d4aa864af999ec101989ce6fa6ddfb38194a3` |
| `tests/cmake/native-math-direct-validation/CMakeLists.txt` | 新增 pending | `—` | `df1146d80410a36613eb5351a8591ce8107a2febde475207e213c9ce9db4e6f0` |
| `tests/cmake/native-math-tests.cmake` | 新增 pending | `—` | `6f25676a4d618bbb4c60160323daf979389961669be31997f4def17a280d7538` |
| `tests/cmake/native-numeric-boundary-tests.cmake` | 新增 pending | `—` | `7382393c54669c383a7f4e2c4e1f1556f13dd9806ad0f5fd0e628a81530f146e` |
| `tests/cmake/native-plugin-shadow-io-tests.cmake` | 新增 pending | `—` | `df1b81d0d9332f90ad0209c0b2d3d6af50bef054857a96b35a8f9f059d11e953` |
| `tests/cmake/native-registry-oom-tests.cmake` | 新增 pending | `—` | `f1999f9ddbc033c986d71cd47dd2c1f5784aac454aac729dc5040a1f038825ca` |
| `tests/cmake/native-string-publication-tests.cmake` | 新增 pending | `—` | `2501a6e7804983fc9f93fb2630f7f60bbc65018eb5c0889ad8ee5210a209fd74` |
| `tests/cmake/object-write-failure-tests.cmake` | 新增 pending | `—` | `44a8c4a0ae15a8757d6bd6bf53e06df16d006e58b200ba34ae1ab45079d7c277` |
| `tests/cmake/setter-caller-cleanup-tests.cmake` | 新增 pending | `—` | `09d45d4834761f798aed2a5e04f2755a4ddc313020b49c77e9fd98a24ab4131a` |
| `tests/cmake/ssa-cleanup-tests.cmake` | 刷新 pending hash | `135acbdc3c14861b45c5897a454207d4096abf8c6e2590e72c66939190cb17d1` | `5b747ac7ddf7eb1d755a3ff91a136470be3ca8ae2aa3cdc28b5c4d9a0859e03a` |
| `tests/cmake/ssa-direct-validation/CMakeLists.txt` | 刷新 pending hash | `12223d249dd4038e4ed866fe26bd1725e9f4e9bb7c341476582d27306bed5a28` | `2fd562b67c95dd40d2acb0de2757d82b9f2e1c6301c84bc0de3770fa4ddaff66` |
| `tests/cmake/ssa-source-finalized-module.cmake` | 刷新 pending hash | `4ca49b8e231bb804c23694966402606ca1c7af8ae70f0f4f5f5568dbd9d0b2f8` | `44aaab3a4e8783159b95078ef6d265d912ca7300f887798b55c484429f822d79` |
| `tests/cmake/ssa-source-integer-expression-retention.cmake` | 新增 pending | `—` | `40ecb118535adec6d5724795de54ac59aabeacd8a8cec1e359fdb6f2f602cd0a` |
| `tests/cmake/ssa-source-returned-execir-retention.cmake` | 刷新 pending hash | `dcb61d53821b7889b5b589840152d904e5f4244e977da66a49d127e9a62b84d1` | `8ed28037b09ef40e5e77c8a1a5a1119143d6d300c1cca233d0fdb21d496b2459` |
| `tests/cmake/string-format-oom-tests.cmake` | 新增 pending | `—` | `b906069877a626978908c340d4db096682cd9d4b3b7424f9b058d65fdea6e4e4` |
| `tests/cmake/string-native-layout-tests.cmake` | 新增 pending | `—` | `4f6ad8017d07e44a2fc644123199dd17b6f4829d8d4df8d281a51b8b92633ef9` |
| `tests/cmake/string-storage-oom-tests.cmake` | 新增 pending | `—` | `dc093e62681fa3678e31f481c1b03b0af62de5503071c2d1f84c78b486952afc` |
| `tests/cmake/syntax-embedded-positions-tests.cmake` | 新增 pending | `—` | `bd737d58613851c5342dc22d2f4172caffdfb1940ba207857fa047f83ef2b7d3` |
| `tests/cmake/system-fs-failure-tests.cmake` | 新增 pending | `—` | `6ffda3bb0525daca26c5a51feed3442e1f92f9034b956cb64d92c8017df91718` |
| `tests/cmake/system-fs-resource-cleanup-tests.cmake` | 新增 pending | `—` | `045e19010401f1d527407c91d9ebf4ac7b92d7943c6baf990b7d9d2d829a7072` |
| `tests/cmake/thread-transport-facade-oom-tests.cmake` | 新增 pending | `—` | `ad8b959066c8ac8db70374df5b099dd44505504f629522c38e2041214d66b1b9` |
| `tests/cmake/thread-transport-string-tests.cmake` | 新增 pending | `—` | `1ddf8654882245ecbb97c59dc51b013507157e42cd5d45e228fd33f1dd4a29e6` |
| `tests/cmake/thread-worker-count-tests.cmake` | 新增 pending | `—` | `9dbf37c01cd6bc24d51f0229b7c2980d7339d12eed7655636f4ad126941b9c75` |
| `tests/cmake/zr_vm_lsp_wasm_response_tests.cmake` | reviewed → pending | `8d8aba3a6d0e21f29d36461279e0d713766e314d72c0d9a588a193dfa15d6745` | `3d49115f74707dd5d48aa56a08721f66b7acd0954684613b95369fbbf73ca6b1` |
| `tests/compileTime/test_comptime_control_flow_edges.c` | 刷新 pending hash | `d85006f47e6a706fbbeb4dc89aa3ad52f4df810282160e421c0ed28e5b3b6e0c` | `7f3a9d2a3bcef3958cfa14d189fc7c92ae0eba3e40f47dd9a3412d3fad56cbe4` |
| `tests/core/test_artifact_row_boundaries.c` | 新增 pending | `—` | `b58de0a4637f05271e43684296dd30f3acb5870fbe2f25432adb96468baa3af7` |
| `tests/core/test_core_allocation_lifecycle.c` | 新增 pending | `—` | `8618d3ebaee9e6ebfc084b731e70d3e500511284fb8133dc7184e5af06781a04` |
| `tests/core/test_core_bootstrap_support_oom.c` | 新增 pending | `—` | `bc54555dd9313a2a6d817b79d333ac68edc8efb32967399a7ca5e2d0942d70c3` |
| `tests/core/test_core_exception_oom.c` | 新增 pending | `—` | `f2ba97de1124cff818941cde1cbfbdb69059be701c5be9f2e087f9ff920689e0` |
| `tests/core/test_exception_construction_roots.c` | 新增 pending | `—` | `8dbc33de8ad04007be4e841929af81ccf41c326a88fb34adfccce4d0950c019c` |
| `tests/core/test_execution_checked_multiply.c` | reviewed → pending | `372784ed5b73b60c34384f506467a24d6c9357594f0e7ed5b65a718e0267e806` | `cd60955e63460d1e9e781a8e59e636aaa4320e4a54c29450f297914e0eba445e` |
| `tests/core/test_gc_allocation_debt.c` | 新增 pending | `—` | `cf53d096ee5d419570ea0ed100f1f9f98c100b93855f6d30f126d265f9b08f8c` |
| `tests/core/test_gc_contract_boundaries.c` | 新增 pending | `—` | `9baf7f0d68a1cc7f65cd3a65f3432d0e60429efcc4d5c92065030938c1b09af1` |
| `tests/core/test_gc_root_leases.c` | 新增 pending | `—` | `d43592d84b7b34e69f313d1d55e117085cdc0d05a5f39a76eb82d48c0d714ad9` |
| `tests/core/test_gc_scan_exception_cleanup.c` | 新增 pending | `—` | `cbc6e007c8cca696b300202a88e02bd52ad456604c2efa3e55a86b5b5d06d12a` |
| `tests/core/test_meta_string_oom.c` | 新增 pending | `—` | `22a19b611f9b53e80ae1d15236d17af8615ccc09d51e95624ec9b6ce593b76c2` |
| `tests/core/test_native_array_failures.c` | 新增 pending | `—` | `e71759725cb7f4e0abe0ee19c2f3c0f5b5f2f173ee6be6c4866a248fa922ddfe` |
| `tests/core/test_native_math_failures.c` | 新增 pending | `—` | `8826390d0b581f60bcfb6bb0a81b3b711b52225f268ada45933e916b7aee4089` |
| `tests/core/test_object_write_failures.c` | 新增 pending | `—` | `9e96a41887f23b7c41e49a91c30ae8cbd5304225862bd19cb8fcd235bb8d4ac0` |
| `tests/core/test_session_checkpoint.c` | 刷新 pending hash | `fa069d0fb3cec6f2901c63f523715063990778fb4f670a7264cd8e411c92d348` | `5926cdaf714c71180b69affae632fd45f83a11688554a28bfabddba0a9f06c7f` |
| `tests/core/test_ssa_differential_harness.c` | 刷新 pending hash | `59467a2e44c04655c78b1b6f31afe18a49c7a478d1616ede3423d44acd05e052` | `55a2e8e74e9ae8216dc72ab1386e5f5d2707e0db619fa88925b391ae7d6049ff` |
| `tests/core/test_string_format_oom.c` | 新增 pending | `—` | `f6540a320521aec61ffddd603abf2a5ef10ba580eca6c96bf3e9386617df64f6` |
| `tests/core/test_string_native_layout.c` | 新增 pending | `—` | `d160b3729ec80ce22d79cd37b4c8bab4068328eee41498d3b7067332958e6aa4` |
| `tests/core/test_string_storage_oom.c` | 新增 pending | `—` | `56523d0ece608d49bfb1c46ef4dfb37d807cb11b59a65b9f42cca575fc1ed82a` |
| `tests/ffi/test_setter_caller_cleanup.c` | 新增 pending | `—` | `f9faaecbae2cdee6f2ec51a40d9549badec722b1c8197c4f9fb7cb676b506fa1` |
| `tests/iterator/test_yield_semantic.c` | reviewed → pending | `30466844e117d0c3b99107ee39577da4f1155fa011e848146d673202de66690b` | `62ceea9b3e0a38cac5848617e47489f5c97bffe1ad65be1060d71b7da185acd8` |
| `tests/iterator/yield_function_body_cases.inc` | 新增 pending | `—` | `6b9f220fe4d4dd7aaecbd7e2d33f2cd5948cc32fd9585e9c2ed954c6f1cdcec3` |
| `tests/iterator/yield_public_compile_graph_cases.inc` | 新增 pending | `—` | `a37c22ccb8d0607f95934d0ef71af787e696f1ce8262ddbbcf54e7d4122ee5fa` |
| `tests/language_server/test_lsp_array_oom.c` | 新增 pending | `—` | `c357274274389a675b41b14ab44ef3abec8aacfedd2ff78251bcdf1256d5e27c` |
| `tests/language_server/test_semantic_analyzer.c` | reviewed → pending | `d7858c955891c089cb700512a189ed25db77a68921abf8538b920be7810b1f73` | `54d8eb908fd22793748063999b550a9022ca306d7a4ea73965cdcf6377a4db88` |
| `tests/language_server/test_semantic_analyzer_declaration_context_cases.h` | 新增 pending | `—` | `1181ab4cec37d76eb03d067a0bed7eb17e8bc06903c0db4a5de9d570a1976963` |
| `tests/language_server/test_semantic_analyzer_hygiene_binding_cases.h` | 新增 pending | `—` | `992772b85f800a3abd0df3339fc13752e64ba820f49f9865ceb254a099de1873` |
| `tests/library/test_call_binding_module.c` | reviewed → pending | `99e1d909d727d8204e9dfc656880c53cf278a5eae014cc7126876d36b2903a9f` | `97095c1a6e1eb7b347fb48b71269d9f14dd9849d449aa91751a000f75ad32572` |
| `tests/library/test_call_binding_module_imported_instance.inc` | 新增 pending | `—` | `cc483ca7bd19c0da489435a9a3fa444668b12bc9435802f97472b54ab81c490a` |
| `tests/library/test_call_binding_module_imported_meta.inc` | 新增 pending | `—` | `52bb511771f2e687195d114bcc239f36e4cbd57e7e83105e4a277838a301e711` |
| `tests/library/test_file_path_operations.c` | 新增 pending | `—` | `79deeafa5ba2b01a0c7dfcc4a62814de973f6c6c26fa1d221b9348d009eb19c4` |
| `tests/library/test_math_input_boundaries.c` | 新增 pending | `—` | `1f1a31efe3dbb349b3dc91eafdbbe276af27372ede3a6687012f33b464f527f4` |
| `tests/library/test_native_dispatch_self_oom.c` | 新增 pending | `—` | `da1e57cc61e7dcfaee70a42164cc5e36fcbb3cb58ae0985bb0033a1ee4df5174` |
| `tests/library/test_native_numeric_boundaries.c` | 新增 pending | `—` | `36367596a72585013b511c7f8ca897d975b8a3bf184179d8f984f8035ae33845` |
| `tests/library/test_native_plugin_shadow_io.c` | 新增 pending | `—` | `f861250044c3217eb9f7c28f69e1a933e22a07ac5065e61bf90299891ab25fd4` |
| `tests/library/test_native_registry_oom.c` | 新增 pending | `—` | `1f48f28eb96032674d8bd1562345a1f03c164f9444c6ce0b70abcbcbd2393967` |
| `tests/library/test_native_string_publication_oom.c` | 新增 pending | `—` | `c88f329f925bb47c9aaca9acf7eb0eeff6dcb363826ecdb59372d34444131b1d` |
| `tests/parser/support/ssa_source_returned_execir_fixture.c` | 刷新 pending hash | `e50b4039ea9d574fff708aa2a88bc2ec9d7ac4522b9ec511a8004ace6bec271e` | `d93419a77446482111685b5c61960f8b015231144155a0f320df2babec3526cb` |
| `tests/parser/support/ssa_source_returned_execir_fixture.h` | 刷新 pending hash | `a61bc3f4983c1c1eff477dfd92fc44c2096bb1f495e5451497848e0d695b3975` | `b4f436ecdcdb4f6b048866589f0bb650c7f95da0b7a2fa28114aed58ee63da1a` |
| `tests/parser/support/ssa_source_returned_execir_frame_shim.h` | 新增 pending | `—` | `04b75504d26e9c1a6d7842b8f2855c20501bc1873adb374eab1fd287e3d17329` |
| `tests/parser/support/ssa_source_returned_execir_owner_shim.h` | 刷新 pending hash | `a2889bcb780dd0bdffabfdd3debba1f3b41d70fa1e822e2f99a4dcf2ba0a49c1` | `c6896e731c6cb01dd6f79035f093e112ea3f859323eed94350c378864bd47361` |
| `tests/parser/test_aot_c_source_contracts.c` | 刷新 pending hash | `52b5965146c5be053a9e5bfebf4c384001211ffddea277372a3329ec33664260` | `5f0f998fbe83667637153af0368ae1416e6a1ed98354910474b4cc661bc69414` |
| `tests/parser/test_aot_c_value_semir_contracts.c` | 刷新 pending hash | `c4ada74c8c4eea20d45f40fef7e1c43922ddbe40b18644ba0a4bf1cef425a9bf` | `4b5e0ab972ff4da84cb693726ebe04316756bc338c0a29e91f1417ff13d4f3fa` |
| `tests/parser/test_compiler_regressions.c` | reviewed → pending | `b8849900932d586769f69073c3d09046fd3ef41651b25d3a1add9d5985b933f6` | `659f32b6fe2702127b623e956fb4d1135cdeb0cb5e7fffe6ea84d4912fdad671` |
| `tests/parser/test_compiler_w2_performance_quickening_main.c` | reviewed → pending | `8980b0f48c2c07baa4bed4bebcf0bf0a675b47a6c1648c62b28ac55f57650b11` | `9b5b8dec5eceac6a4b97fd41a5c3de9bf3056ead9ba1faf51edd6850d93c63b4` |
| `tests/parser/test_compiler_w2_quickening_array_add.inc` | 刷新 pending hash | `ea65bbdc38ae4d846d645676ecb1a8d9e3b1d48586872d792ab0f2680d94cc28` | `a7caff9fc2408f44fbbced197ca7fb92eabc8b5edc09f6ba918daf8c396f6ff5` |
| `tests/parser/test_escape_metadata_pipeline.c` | reviewed → pending | `8a1eb1afc591697ac39614b2c8c427807f5caf37f28281ec0d09912e2b76e19d` | `1058999bb4f9b2ef84c97493cd22e865d487532de320f09487e52b3d3b3fee14` |
| `tests/parser/test_escape_metadata_pipeline_main.c` | 刷新 pending hash | `727ef6996f68ba77b03bb2f06c0cf1477927458ffcada5c39d1b0f2736368232` | `81214708df53853c5227974d104841c45e40d0aeefd0727b973a2eececf133f9` |
| `tests/parser/test_escape_native_member_cases.inc` | 新增 pending | `—` | `9c5896c511c91466bf75790b19003cf6e7c6026699e8c9ceb108206e6ea96c4e` |
| `tests/parser/test_lambda_local_range_cases.h` | 新增 pending | `—` | `d6fa729704adbefbab5eac7045ee6366a007349999c80ac4359e6a9c56d1bddb` |
| `tests/parser/test_lambda_semantic_ir_isolation.c` | 刷新 pending hash | `a805553816a1cdf155e475b3aedb67205f3c0e9bbe85be6a964eb16392250f01` | `915fe17bfa60d500a0dc3c8354a016810760da68d0c152b57cc11686b54f222d` |
| `tests/parser/test_pre_semantic_ir_dynamic_object_local.inc` | 刷新 pending hash | `2ff63c1cddc006189ff36da024a384058406ac9da0f3e7ccaed73a616a6bf2d3` | `4563dde2c65c9b68c96f809e10e91eb11921adfd6933d820570ad168679c995a` |
| `tests/parser/test_project_failure_diagnostic.inc` | 新增 pending | `—` | `b7fe5d2db7a5f7703e71b6bf7fca44757d9a0fde2f75f941c51507e13921fb9b` |
| `tests/parser/test_reference_receiver_call_boundary.c` | 刷新 pending hash | `2d62f323533cd7c0d4a55965028cd47d335018ba46cce8fa69ae3526c176e268` | `26ad161d91feafee1d685707258a7d1423b4137f59a44f7339683fff345356ad` |
| `tests/parser/test_reference_receiver_frame_contract_cases.h` | 新增 pending | `—` | `5a4813367b3543ecbc74a152edaad0dcd68fd6c9b87a1ac191920609246cab37` |
| `tests/parser/test_ssa_aotir_contract.c` | 刷新 pending hash | `8d49474113e74aaf11e397cb7925fded1610191265219cd1d0d246fa7f7b1559` | `caf31847c9de31deff2ecc0163ec558188b3c11802921b872a533769a0462cd7` |
| `tests/parser/test_ssa_compare_metadata_guards.c` | 刷新 pending hash | `4c4e302409678a0d3646eec0bf40068dfa5b2ad4172b55735d72287d3fa06090` | `c4620a968d4b34c383ec6ca45dd6e84e6e3a481e8020cf9ea3010752add1e71d` |
| `tests/parser/test_ssa_conditional_cleanup.c` | reviewed → pending | `f10954b3b24e83f15e317970c9907543c3b18bf87114a92e21692d7d44d074c7` | `039a0819680052d6d5d68bfc1a9a17643bb52228470e2a615589f527ceb28fa1` |
| `tests/parser/test_ssa_contract_freeze.c` | 刷新 pending hash | `4146d2145b297ab6002c1667331eb8e5dba3cdd23945b02d20da5958b5b428dd` | `6ea99bb6fb842b649f6f3458c2ea65da3e96d544ef8fc69e4fef49e443e25122` |
| `tests/parser/test_ssa_execbc_vm.c` | 刷新 pending hash | `1fe7c626d61e3373a65a7be521b86ebf1fa48d22570c3cfe16c60c510a021540` | `7d3123262a38ce1f096e27c460b9beae19f68efedaccddd2b2b56cbce70b44c8` |
| `tests/parser/test_ssa_owned_row_direct_call_graph.c` | 刷新 pending hash | `c3c00f432dda1ab903be5b0284596f1c33c5f165667861712a6608f47e0019a1` | `789f13a6e76fb10c5483cc626307139bd3f08295daf46dbf7fca0b3e7ae9c0b1` |
| `tests/parser/test_ssa_source_finalized_module.c` | 刷新 pending hash | `84fcc3bc29a42baa19fa8fa0ca348f19e4cdc74f4ea2f70a2068a9864f187d1d` | `4d9ab7be8447c96a9016363003fb3a3ebb5d8bd86477ede4b6f5b693fd2bbbfe` |
| `tests/parser/test_ssa_source_integer_expression_retention.c` | 新增 pending | `—` | `9c9455777dd0b4b9542f941d86f29235f677c1d8c674c13c9f4aade84fd2ddb9` |
| `tests/parser/test_ssa_source_returned_execir_retention.c` | 刷新 pending hash | `444905cfddcb2a4278188abe436a105b4dcd760d1a4f7b83841a69f033889b5e` | `3f4d2f9b5c5e1e5603e8f3811d66d0f60ac3b8074109a370417b6b00ac15b04c` |
| `tests/parser/test_token_stream_limits.c` | 刷新 pending hash | `18df83c96e3de0a95e2dfa9a4ae5d1e2290522cc680a7bb1abb54e801beed46b` | `a5e6840b3a6463d7647ce9e0da7128644a3ab88cb0aa24e0640ba2f74cca1e08` |
| `tests/parser/test_typed_call_binding.c` | 刷新 pending hash | `e0f835b0064c56f1619e2c358d7ad3c8c148b53fc77895b15d28947bf0755847` | `ea8a35d8ab7dcf91360df49123dad0785d33bcbd785acc9e9f62668ceb331a23` |
| `tests/parser/test_typed_call_binding_cyclic_returns.inc` | 新增 pending | `—` | `ea4942692723bab527822a134535fa53043b52e30c2899dbb9c73ada70bac6d4` |
| `tests/parser/test_typed_call_binding_imported_returns.inc` | 新增 pending | `—` | `b1e07dc47d92e573f1580cb18590c010b7a31bdc5dfdab692543d958f2da2705` |
| `tests/parser/test_typed_call_binding_member_semantics.inc` | 新增 pending | `—` | `93069a0a303dc6789b6f5e285c8dfa67a78554c6b0f9322d2fff9e606bd8a049` |
| `tests/scripts/test_syntax_migration_embedded_positions.py` | 新增 pending | `—` | `71cf8c7bb11476e0210cae17ad47d7022888324f5735ab245d298207f5b74295` |
| `tests/system/support/fs_resource_crt_observer.c` | 新增 pending | `—` | `1028231f90f29845db0d15bf60b325eb6aa67e93d2ce53d7c2dad83e7d2eef72` |
| `tests/system/support/fs_resource_crt_observer.h` | 新增 pending | `—` | `27f1495f9c0e9fb24c4029d684b85853596b03aa0ffc1f52da5c8749191e0ca2` |
| `tests/system/test_system_fs_failures.c` | 新增 pending | `—` | `8684f6462575329b5742e70c517a8620345bc36438ff0c84445dca04a04a46bf` |
| `tests/system/test_system_fs_resource_cleanup.c` | 新增 pending | `—` | `42da2f7206f6fcc09f9a37acd64463f7984677a0b0466d0dab223d3ba4ab8b54` |
| `tests/thread/test_thread_transport_facade_oom.c` | 新增 pending | `—` | `5e0a2d4728063169b3f1ac0b8276f1b39cc5c7bbf578b14a98f20b723defc96e` |
| `tests/thread/test_thread_transport_string.c` | 新增 pending | `—` | `70bb453702f0941bf722bfd43072eb43aae4aad25a156838b624cdbab16e92ff` |
| `tests/thread/test_thread_worker_count.c` | 新增 pending | `—` | `761b5a60897d21f1c7cd7835732398949780bb66446854f9acbc0fbbc5b283f2` |
| `tests/tooling/test_artifact_maintenance.py` | 新增 pending | `—` | `74ae9c5d4588576388caf5c78977f4acf658f25260686ea4b8e0cba48648c75a` |
| `tests/tooling/test_artifact_schedule.py` | 新增 pending | `—` | `03a706ebb9b4e8e24045c15488dd1b88782658106127d465bc70021c0e7eda11` |
| `tests/tooling/test_layout_generated_includes.py` | 新增 pending | `—` | `179b492967e6425d95e2d92317cd12f0963e41a206bfe6cb73c1d4f726bcd030` |
| `zr_vm_cli/CMakeLists.txt` | 刷新 pending hash | `666f89d1bbce4697cfd57c0b9790ac33a5c413e5b45223feaa00d2a1dd53891c` | `7d1af9172a2772a3975c29042ea6d02428442668b5e553eaba4e07af4ab259be` |
| `zr_vm_common/CommonMacros.cmake` | 刷新 pending hash | `cc1e6cab77bebd81d744026c8dad77e6f5279b76bf905b7a5e0ca433439650b6` | `f3ed2c8496f66268ed079ae608e4919823329efa9bd4b8e6cbf0ce4f0163f3a0` |
| `zr_vm_common/include/zr_vm_common/zr_common_conf.h` | reviewed → pending | `a5b7bd4d0d9468074b51aa978c62ed877f0f1151fc9608df7d9bd3ca8a9f5e10` | `20d79470b6fb0fc180b53251fae936eb882133e9373da1178639ef8cdc8f0b63` |
| `zr_vm_common/include/zr_vm_common/zr_object_conf.h` | reviewed → pending | `09149c8012d56ed97e565d52ed290f459551186f975b118779cc9af14e3a3ea5` | `955c9a48cbbb31fbb72515aed29452dc58e973d55cac3d20bcbfebea3251bd77` |
| `zr_vm_core/include/zr_vm_core/array.h` | reviewed → pending | `c4dda2ba07a964326346328d5522c0c5aa68d7bc1ebe5864f3d80d88fffc7baf` | `ac67bb10dfe429be875723e9ad3fd25ef6c6606e4e0bb11cf243113b1c331ba2` |
| `zr_vm_core/include/zr_vm_core/artifact_schema.h` | 刷新 pending hash | `5cde3d38627749f1fa02fd309c687651483f630e8707d18d26d29e33629f7989` | `124b559f20a29d176ce3fe00de28b636dd655ef588a56f91a9bc037641512659` |
| `zr_vm_core/include/zr_vm_core/conversion.h` | reviewed → pending | `b482ab06eadd2454fbf36e5a0b714c8954e87e9ad13872d1a0bae1da4601f2af` | `4fb1d2432db96ea4e5fc6c78b14eac072fb88bde265744a202c508642f74e994` |
| `zr_vm_core/include/zr_vm_core/gc.h` | 刷新 pending hash | `3ef09b4cd5a77b68492b587f930b049ce715c814d929e8ac4e2de633683f3e1d` | `243ec47466ee524a7efdbcea8d57069fa1d1436b68f734f85f9f8ec0c6542c14` |
| `zr_vm_core/include/zr_vm_core/gc_budget_contract.h` | reviewed → pending | `32c60d6dbbd7df7390ede776ad18c36d32afb350eb2994932a58d5815fb7deec` | `de2a496144b26cfa5e7331faba229c82c2dcad70bffcbd1b7bda4ad102be6fe3` |
| `zr_vm_core/include/zr_vm_core/math.h` | reviewed → pending | `2fe9c0640672f7d932ff80ad6b9d36aa2d96aa5bf3d673785dc276f8143a5dbb` | `e75ed4789395f6a2f4f74a013f6afe44a6fb177f36938b5422615d2e8afd1748` |
| `zr_vm_core/include/zr_vm_core/memory.h` | reviewed → pending | `7b6fe8730e4d93577c77675f9e6f650a2394284a9cbeef56ed8c70a12540cc98` | `6003c3110fafa938c6f44dde62e39a57ffdd89a6580b24d76241ec229e5788a4` |
| `zr_vm_core/include/zr_vm_core/object.h` | 刷新 pending hash | `5ddc17b808958d6f19a08cfdb913d4b5113c67035e220758d2677e991bc94e38` | `0183db99923a9698346a50b5c2ea1d81c87b2f25c96b922d6a7e34ebacd7636e` |
| `zr_vm_core/include/zr_vm_core/raw_object.h` | 刷新 pending hash | `4a3748a8da272ad4755665b01cb90e6a16b19a0ae95522f942a3debc37b0f702` | `c5d03a98133550d4d1c802a631a7ca3ef4f106105d5d3ba3a7451fb422aadd33` |
| `zr_vm_core/include/zr_vm_core/stack.h` | 刷新 pending hash | `b7cd82f2a124f0de0f482bc5cd6e58eef2dc5ea0e1c7f8daf2f3b429f26e63c2` | `7352a2a5404bad3b437297726e05b3b1cc2f543b4a06547ebcbe7a6e6002e9f1` |
| `zr_vm_core/include/zr_vm_core/string.h` | reviewed → pending | `f1d647898d9b8417e4eca3c4ba7fcc4eeda01557967b4bf3d747ebf4eddb793d` | `7ed43a1cc2d08dfcd6f3d8ea856b2ef3a20467490fa7b168c4f8f21e11e9ae75` |
| `zr_vm_core/src/zr_vm_core/aot/aot_ir.c` | 刷新 pending hash | `89047dddc5338ea41c63425fa9445051f07ab1664a00ad94457bb01e6e028bc4` | `aec33cd3304bb67ff796eef3c2d7de482c8c846915dd57d53a031237cd2f4e1a` |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c` | 刷新 pending hash | `7a356855c886f0c3076db08fca10c522ad01aaacce6aa82fb4e36582dd1a9239` | `771277a27905458116a50dad2370b054856a6c903cb446917e5df519c907ddff` |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c` | 刷新 pending hash | `de02be09fa0e83b1622be8c775896fc866df7b79ca12fae9a0f23c89de9c4b38` | `3020ac4bab61f568a6def180ac55b08857935786affb53d945323cce8e23db69` |
| `zr_vm_core/src/zr_vm_core/base/memory.c` | 刷新 pending hash | `fcf36e1890e839b9489bdb929bf74f1fe5b83ab469e9b73b39b5cea54ffd16c8` | `5cf04e3805f502d9ac502820e9dd526d943e08e55ff07dcb95451046720351ec` |
| `zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.c` | 新增 pending | `—` | `38344395196850370ba82115cad87163721fcbc14b481b941d6642885388b473` |
| `zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.h` | 新增 pending | `—` | `8021cbefc2b59e73c46a0f2e43995ab4457cc0db121446d75ac4a063bec7a973` |
| `zr_vm_core/src/zr_vm_core/exception/exception.c` | 刷新 pending hash | `3922a49606bf057d80bab7ce9bf3fb3323ef5fb0f85247319c2cdb815d648216` | `0f47c75c06a161e885ac97f39a3865d2b7392e48d76014533e6d6f045f3157e6` |
| `zr_vm_core/src/zr_vm_core/exception/exception_error_internal.h` | 新增 pending | `—` | `27e3011f0ab7e77ee34747dd2cc1ccfd8ed5f567a2b3d8ffd5c4aab08b103013` |
| `zr_vm_core/src/zr_vm_core/exception/exception_errors.c` | 新增 pending | `—` | `4aced3bed129f48f7a10f71a0a9d8ae7f7ea56a54cb4d31a14c345439feda14e` |
| `zr_vm_core/src/zr_vm_core/exception/exception_internal.h` | 刷新 pending hash | `ccac094d6b5e8a825849c76e8593d3f67a6ab7b2636faff8ff93df90b15a9090` | `048cc71b8b409749b8b1841ff06856b78889a8a57e7f361a646ee83290a6f18d` |
| `zr_vm_core/src/zr_vm_core/exception/exception_roots_internal.h` | 新增 pending | `—` | `40310e9b4e4f5733bf406a2e80db5baa9aa6e3c400688138a6f9edc786213fc8` |
| `zr_vm_core/src/zr_vm_core/exception/exception_try_run.c` | 刷新 pending hash | `e90075c1c428af834f15c2f28a7228940415efff6d80e3c1466b64a950312318` | `14ae25b247640f9015ada468ddff853d018d0043fa7eae8e96b1eb470095f660` |
| `zr_vm_core/src/zr_vm_core/execution/execution_control.c` | 刷新 pending hash | `5a038c30b614fa4ea4e2dff14e36818647777b86d229a799850d3c2ed3bd28ee` | `b6a689d1e1adf7a6c43b79826b00543e76ca9b189921dfd1a2f621cc5e13fa9e` |
| `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` | 刷新 pending hash | `ac9f0372ebf8261073f7eeca49aba808c3b8edb8f981d1d2fafb294b1cee1b46` | `01d7d55fc8a064295932811e64b7e6b7f5da8eb75da31e29bd1a241193f45cb1` |
| `zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h` | 刷新 pending hash | `e1e9ddd2508ec004980abc2e99670fbdf3120e6245457a8896ffb8848d9c43e4` | `bb4cf6fbf4107ec590ebede5419c68a283eaaa5625a17fc5f860536f4a442abd` |
| `zr_vm_core/src/zr_vm_core/gc/gc.c` | 刷新 pending hash | `3d7141d008802a1c925d603f29a84cef30fa35549ec2313806f90519bd260f99` | `b49432c9719b40f62d90ce54bd619f567dc703d94faf3b889be76860497baf1f` |
| `zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c` | reviewed → pending | `1a80fe5a500e93be46b13d57a746487c99917a6bfef1d78db659e5cc7a1f0f15` | `7e211cdf45fe1870fd066d5a47001cdc61a52e22ebc0f4177763c9762ee3088b` |
| `zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c` | 刷新 pending hash | `1c528baf03b3ef6adac17cdd72fb182bd138822f034755c3530d3ec469a081ec` | `dbb41b04ffb8520728e07557db8b865e4e2a3c241399da6a6768b4c296b1d058` |
| `zr_vm_core/src/zr_vm_core/gc/gc_compact.c` | 刷新 pending hash | `6dd8d0d3a0b0d5695a4ace92b9e72ec70f9ec9e36b5d65c3574d247aaa64ac4a` | `a86847c3d4fb60a811443b64d9a6661b885caeb0447ec796d00e0a532984d331` |
| `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c` | 刷新 pending hash | `2c039ad27a1e6e86b46720e559325b10e85ad7896d5016d45d2eaa9b43cb0132` | `e270a2849c0d80634c779431789589c6e2c33f6de5935bd5627cca7ef6ce79a9` |
| `zr_vm_core/src/zr_vm_core/gc/gc_debt.c` | 新增 pending | `—` | `8d415e2a988c42a1ed9966b2e1722ecb34cef7f0a358d73c61fcc070d3835075` |
| `zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h` | 刷新 pending hash | `e2bb21d46850b54a43cf05827cdfabc21f547b27dab87b821bda56fb12a7a824` | `704e1bda96ae16d8d9b514c50e411b48121f68d5f40dfc9df332ef48d9b247ad` |
| `zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c` | 刷新 pending hash | `37cd71fb61475ce81d870872d6d5cf2c2b45cb47e2c9985586745042c3348e72` | `97263661b55a408b40254d581a6328fd004c31523d11f139251d45f7521236b1` |
| `zr_vm_core/src/zr_vm_core/gc/gc_full.c` | 新增 pending | `—` | `088f16ffe0067ffea6cee1620d25e0c25f08be625025f04f2fe9ab2703e5dbc4` |
| `zr_vm_core/src/zr_vm_core/gc/gc_internal.h` | 刷新 pending hash | `bb1270994f76fd9c2f46b28a26e0d2ace4586b61538895075e493929bcfcf148` | `4f7fcaf17935520799bd7c9096ecccb0b1cb8b98a2f3c07bcad15119878de010` |
| `zr_vm_core/src/zr_vm_core/gc/gc_major.c` | 刷新 pending hash | `7b66cd20f6d43eefe89ec9e86f7c91b687bfd6369ea123949e95ae2b91b24230` | `2266a31be574d6964285c23b02d5a0060511a124867db16bcbb376c3f9f807c8` |
| `zr_vm_core/src/zr_vm_core/gc/gc_object.c` | 刷新 pending hash | `01200de3c6819cb7588e131c1b89a8689d7b431f5c02790942a6617766e717ea` | `1970c4cfe903c74febfd7d6c96275bc51dae4b0a3efc7a81081def9689df65eb` |
| `zr_vm_core/src/zr_vm_core/gc/gc_roots.c` | 新增 pending | `—` | `77a2e09d33831ee1410aa0fdb0affe915c44a6fd1ad9b1cfcf0156f4d18b4d93` |
| `zr_vm_core/src/zr_vm_core/gc/gc_sweep.c` | reviewed → pending | `a2a35e4e50d46366e0cc6adcfce59d3ef508f014df4cb0ce1f9f74ced890d580` | `4a7b4ffe53358a66bd39847f10a97b4e2dc4c6cf0d3d064b30bce0a1b2ebb1a4` |
| `zr_vm_core/src/zr_vm_core/module/module_call_binding.c` | 刷新 pending hash | `c51f9862ca2ffe9d9d2ad7906d8161f9817432f6d8d925ed27cdfab27b4ed313` | `adf68dcf1de88619ad46e887a2bc58cdd4ec021fad613a5d22135d85c9edef16` |
| `zr_vm_core/src/zr_vm_core/module/module_prototype.c` | 刷新 pending hash | `b3a8969a1691499989530f0f457a4240820e72a1fd3920f1006b0715a154ea46` | `f21a605ad5280cd6e6d30a3245371450142ceffbcb084993e3f3d9a5d41ed25d` |
| `zr_vm_core/src/zr_vm_core/object/object.c` | 刷新 pending hash | `20d4259017e7e6714ee9c39641e5c10eb30f7d6dc90606c9d92974ee387e5f7a` | `3a16ad71c94f2e9b27662b1ab253848df51cffa2f57af495f8261f9135ad3acf` |
| `zr_vm_core/src/zr_vm_core/object/object_call.c` | 刷新 pending hash | `902cebf13c910ef12d9c60737ecbd97046a4e1f1bfaeebbc0e2892b70f8d7e49` | `ebd018a48c52310eff054ff49999ed57c9beb8c9de72dcaec4b4de1826ad13bf` |
| `zr_vm_core/src/zr_vm_core/object/object_write.c` | 新增 pending | `—` | `29460e5d28aad7dcf01a7842e591d181a345fb51f323b8d64e69e5e83887ab47` |
| `zr_vm_core/src/zr_vm_core/object/object_write_internal.h` | 新增 pending | `—` | `fa58153896c69309f451e48f98f1d068f99280f72e2ca58238eeba7aae18abf6` |
| `zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c` | 刷新 pending hash | `cc80225151b7a8f801c620df1ad7c14142d4514e037eeded1527270e6c6c145b` | `d5c7f9d6b587a6557c548fe1693b318f93edf3ba05de462c6f28dab734ffdc55` |
| `zr_vm_core/src/zr_vm_core/reflection/field/reflection_construction.c` | 刷新 pending hash | `636d25913df23e44612b8b8b397d6014c123e0f2ac9b8dfef053528d14a83f47` | `239dead5b5618394d5d978e0a389e0a71d7de74a1d226ad5b4e71f05df70e517` |
| `zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_type_object.c` | 刷新 pending hash | `fd3a5773ee639603c52a4420b31d9e9ee4f13d821541285c0357881011ef358f` | `e727ae4add4fb0febe415daf67afcd11eccb926e7fc0157d8b8f400f7e0b1e2b` |
| `zr_vm_core/src/zr_vm_core/reflection/reflection_member_query.c` | 刷新 pending hash | `a9fb5829f3c35f8056525e699e9b8cd25cabbb2e59cf394b48f9223e7c0084f7` | `e2e7e56a153d5e152a31996a5506ef8eceac2f22aa802f946843dc0e55270290` |
| `zr_vm_core/src/zr_vm_core/reflection/reflection_object_internal.c` | 刷新 pending hash | `c17419706c96fdf66e216a488c3aea0d926a441050aa578930220dc0cdf80af1` | `d94a0670d53123256dff2917d80632845e47b76e9ab3902196284c9b373ab82d` |
| `zr_vm_core/src/zr_vm_core/reflection/reflection_object_internal.h` | 刷新 pending hash | `e27ac862035693a12732a5a2e842a7aa0d2e65496295032ff486835aa8573db4` | `6fdedd01618214032834073e1bea43c1bc6ce578c165d83c8bc760e147d65cc4` |
| `zr_vm_core/src/zr_vm_core/runtime/execution_contract.c` | 刷新 pending hash | `d7d5240be5ec2de58ee4a45fb277d9bfc135d0c9c7f1fc3a806a603c5ef369c2` | `d82e8233679cf18697fa5422545ac3ab99c93cce058d857bb27fd62619c889ac` |
| `zr_vm_core/src/zr_vm_core/runtime/global.c` | 刷新 pending hash | `c73d50fc8206be3fcb70c9abca09f24d70d00f597c9e86e3de956faff6b0d149` | `5f831655218fb3cb58d37f2d99a71d4859c2a5bb94d7179a5f1aa8d031f21894` |
| `zr_vm_core/src/zr_vm_core/runtime/stack.c` | 刷新 pending hash | `e33a1304773714d982c8916b439985bb8ac5ed97bbad67b0e7b3d3ef94f6f200` | `c12c99d852e7550ac1d78ff5062c2a5492cf324d3aade0fcbf494d0cf65d8315` |
| `zr_vm_core/src/zr_vm_core/runtime/state.c` | 刷新 pending hash | `f74a47f190d9d4c7dc99f4e7bda0b78e1c54e33e7fd44ad6a62108b635072a7d` | `6cd307f39f3157e7139a39626f436a9ff6d6f33e3e1297e5e22d8201dd9a0232` |
| `zr_vm_core/src/zr_vm_core/session/session_checkpoint_capture.c` | 刷新 pending hash | `9b6769caf57e8b4c24e52a58a54afbf5ef9f21002de8ed32b4288ec448656007` | `f30d0660850b1a78374f7b3d7d3a370bc9233559a83477f069d8131ddcf6ee56` |
| `zr_vm_core/src/zr_vm_core/session/session_checkpoint_preflight.c` | 刷新 pending hash | `379eafd6a29557cc9c959b908c98c902bd4ace7fed06829a2a3aaac4af4a4b6d` | `0369b39ea4b528151070306ab46902a0f3814d96a021af9231204e31c68bdad3` |
| `zr_vm_core/src/zr_vm_core/session/session_checkpoint_private.h` | 刷新 pending hash | `878a065bec91bb16dc908305f943120b9343b965a51370ef7bedf5aa05aedf92` | `11499d2e5d2850ee7984ce21bae980ed156f1d157d12edcf9b6bf56a20b8f893` |
| `zr_vm_core/src/zr_vm_core/string/string.c` | 刷新 pending hash | `30a7400afcca5594801fb59124583a2c4438da4672a05b19096644eb0666c964` | `19e7ce7f800255c9b5cf7799bca7b172a7a5f154acf055751108db699f2bed59` |
| `zr_vm_core/src/zr_vm_core/string/string_format.c` | 新增 pending | `—` | `49c856b7f8092b000a17fa9d1511e1448cb3815bc0ecd2791ef26420b3c2e0b0` |
| `zr_vm_core/src/zr_vm_core/value/meta.c` | 刷新 pending hash | `58fb1017773881c414afea5745be51678f48efd4d84413d0eba14f07f8882225` | `ed5dd875d6c7df3d10e7a8fac68e08005fc60adfa71bfbe312c9fecee8bc712c` |
| `zr_vm_language_server/src/zr_vm_language_server/index/reference_tracker.c` | 刷新 pending hash | `7a9437c65dbe8f9cb7ed3f4173f0d34b14fda9a7b68e37b08ae4e8c5369ead06` | `45866214e7ff530c79de39f4ee6318bfd6612cf34ca3de54e1c03725e08497c6` |
| `zr_vm_language_server/src/zr_vm_language_server/index/symbol_table.c` | 刷新 pending hash | `45b3d96376e9b5fc571a883a5abde15855c6e81c5928a8d56a689518eccdc92b` | `012bed8d93ba291b57c21b7a660451b72fa3a7d9514948bcafa6300e88b15853` |
| `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c` | 刷新 pending hash | `f7bf8175591d854875d9bdd39dae838737b2b31b63bc5ea22d33659e9115a2c2` | `bc96b21e5066baad59175186461e39a7403c7a71dda017b7094fd4e0a0dde4b6` |
| `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c` | 刷新 pending hash | `f4f04c5b81c90a686b8acc988bd8704e3e20d1089bbc98483674243e85091a40` | `163796905e521600f63cd46872224c337cbb1b6b0e567a1f68bb602a64246796` |
| `zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_declaration_binding.c` | 刷新 pending hash | `017e264ae0757d3bdd5265a5fd9e58344cdf94d04408dad59d069064536b528f` | `7e13a3398ae49a51f745a27f89604228acb893ae93cd7aed605fc8ddd4b9fbc1` |
| `zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_internal.h` | 刷新 pending hash | `a2506b7362dc4bcc8c17a4687f0589d60b5fe4e7e79699b175264eef04dead0a` | `953be541bdcbec3f9cda3991b80312e9bfec048efa0ad498afe177cf5cc74793` |
| `zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_symbols.c` | 刷新 pending hash | `62ecd68a1acaea8f7f591a2414f44e7fff621780c01cfe1b7db93e0ad86b1dce` | `b82201128484b4bcb322cc988e9cda098d4f130a7330053ecbf4af1ce8921052` |
| `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c` | 刷新 pending hash | `6e47ff1c297975c8bb528d767a05244c9687a6d2ad49e2233a8025cd40c621b8` | `5a5bc5e9c635e97622efcd3dc8c37f59e47747e08934ea9e1f46181f68c268b5` |
| `zr_vm_language_server/wasm/cxx_stdatomic_compat.h` | 新增 pending | `—` | `9fbdb6c6a43b58597f0fe459043107ef03404c23f5999941f530d3db08c1d5c5` |
| `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c` | reviewed → pending | `22387c94028c4fc9390e6376493010c4fcbefbe5bef48ddff822f4abedda8eac` | `f55056acc99ea9e266c7a5d543a1f00662212445e9f384be23af638bc241b849` |
| `zr_vm_lib_math/include/zr_vm_lib_math/math_common.h` | reviewed → pending | `af305f2cb005753fc4f0e8bfe59fcd72ce826d0d95809b822bfc38d25fbd0089` | `72c3514205746552050ac50587fd3115791d0ef7ff5711de22313a694728ac71` |
| `zr_vm_lib_math/src/zr_vm_lib_math/common.c` | reviewed → pending | `8022b685d7f198821c5288cb7a18f0ec04a7bcdd6a7e1468c7f3c6e5805e00f3` | `70c10a10f455cc756c63479233ade3b090b86c297907ab68782401e6c34f4b09` |
| `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c` | 新增 pending | `—` | `9a9cbf63642cb125b1b076a6f4c1a71d558cc9606a036ed907123ad134f29fa4` |
| `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c` | reviewed → pending | `8d496ef6ecb5f344ca14337696beeea3fb775dab31ce72fe2020206d0aad1baa` | `8d7b2712d78309baef0d2b9dfcd7f3966ffe4184a9e1cbc75cdc690e8ce5bb70` |
| `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c` | 新增 pending | `—` | `2c8c17eb9d051f6935ff2a7f0b7b0a41e3b8f971163aa01da55f5381a3a2127e` |
| `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h` | reviewed → pending | `c2684f8bbd837f38a1d470bcccf1b6f507eb575b1b01071892df22d9b6ad6e88` | `f35942dce598075cb61cede01ef9e421f1e89d94b43425871ed37492d8d17437` |
| `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_resource_roots_internal.h` | 新增 pending | `—` | `261c1c815195ca65102186758052c26202cd4490d00cbc9056d003432ffc4881` |
| `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c` | reviewed → pending | `cd903a3d6fd666083d7c5ec95b59b77d8562d229fa281473dd767876b4f4a5c5` | `58fb2ba623dafd18455e3b8cab76b642fd44d4c017aad32847c18f81bba405f1` |
| `zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c` | reviewed → pending | `06083db9201220f6840da6163ae45d3fea67234fb1721d1248b2db4e981543cb` | `0169b3394931c5e8ba6237d446033f5c964cf775796a1ee2644caaa5ac9a5f69` |
| `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c` | reviewed → pending | `0f91f199764acfcac6194e54c341823d45ef7f7b41dab1ab4f81a7cbd908a029` | `8cf6a6b6fd316c783d5b91c98190188a326b6728cf692995a6c5d9e34a494ace` |
| `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c` | reviewed → pending | `bdfa5e3e0374e53d7dbedb8bd08f8efc48d6dc89a60c029880ea0406a6627b1d` | `2506ae841feff6dc633cd64c24b0c8ae21bc043cfc102e3ec8d52a1e78e08818` |
| `zr_vm_library/include/zr_vm_library/file.h` | reviewed → pending | `b32e75d5781422cfe5b15bb0866b32f08061f297585d270dc283b4a2a37eb264` | `b952b123aac6c9e400f94ded3158102a119d59cf57282fb01d5797534db94a79` |
| `zr_vm_library/include/zr_vm_library/native_binding.h` | reviewed → pending | `30289111ff3239b1bc37207df2e7a283654f1824f7ed77d8584568bc1ba630c4` | `bab3a067aa1a7dfb4e63e69aece77441cdc3a4317f45dbd81719e98cc3f25e40` |
| `zr_vm_library/src/zr_vm_library/common_state.c` | reviewed → pending | `951ea4c1cd34dd64229d9772a571ab9388ed6286b39d6dc500e0efa091077203` | `3802388a8bd21cec8efd71c70b490ff91faee8a993ca9e642a6db541f7c89ad8` |
| `zr_vm_library/src/zr_vm_library/file.c` | reviewed → pending | `ac43d93b6538996ed204564a9c03285c07623a8150cccb3991939826661854ee` | `d30ed877e981f15226aaa1c465868f83688fb13c43b666e3336d077c8e7932f8` |
| `zr_vm_library/src/zr_vm_library/file_copy.c` | 新增 pending | `—` | `073b92103f625e9927db7cb3dc2309fd90e0bb0b003707e46c228867cbd7d4b5` |
| `zr_vm_library/src/zr_vm_library/file_path.c` | 新增 pending | `—` | `988a87b3e564572b8765f7498c8c11e39b94e0035833387e8831652004654958` |
| `zr_vm_library/src/zr_vm_library/file_path_internal.h` | 新增 pending | `—` | `2628517880297de8c94b8fddba6fdeda4427487c11538ff6a72e25281d5febe5` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding.c` | reviewed → pending | `0cffab0e3461494a268473d4261aaf3db979e930cde61a2a30d0a61484a0854b` | `c7acec026479c9f49716bc980822e6093194f2f8cb743e79667ee5f5b4fe3804` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c` | reviewed → pending | `af01ae32177685d47c2e1fab472e8ee63d0f46f8b4a8fbd88b56940503a347e0` | `d6ffcb7e1320a30eefc1ed8816b5d3da9c29b7fe07616ed4a293e5f2d6ced03e` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c` | 新增 pending | `—` | `6e4af75aadb97cf5833dff3c3fbb01e8aaaccccacf7257c4cb75ddd0859cc44f` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c` | 新增 pending | `—` | `022d38e1806190f9748e76fcd5651d4b2d2bcc0567f5ea146a78d1c00209f2e6` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h` | 新增 pending | `—` | `6eec38efccaf8743e1158769eae9b44ffac021b0a8f6affc99e56a6b4ac59c5d` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c` | reviewed → pending | `f1354e2e3409b061482e236fbf681ef6fd4be8bb33edd66a9e198a7c4b7ac560` | `f278d39d3ae096622b8950852ace24f41bf823338157103f2e06d342d096c683` |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c` | reviewed → pending | `c75d2a5890651ac551479ac970f628ee5e5058ab71914112ef5f0a2606c8fdec` | `c5a7a1749fe10d7105604db3ce8a72360b0990bea81d748ad2ffd7fea27cddb3` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c` | 刷新 pending hash | `be86d274728498715c6e173fff280fd2d2466be4ab5a0edb246f3bd959a2c3e4` | `c0dd81afff9611492b6c3208be5a09b0b3ae3949538e35e0f05c0754c1473ddf` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.c` | 新增 pending | `—` | `fab1aa900de3dfc67bae5ea572a5cd508b79cb43844ce0033cee7747a64a8507` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.h` | 新增 pending | `—` | `2b02294e2e41ba558805c0c8fdf49cf2d4860c4d0bb464a485f57b8a65785ff0` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c` | 刷新 pending hash | `b2176d17e9630db9e17854231a812c973fbf451be5d27f0ef8848e13cc3384b0` | `ecc2896635123be1488fb25977f95de55bfec78a9e4d581294b5c613b58cc428` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_body.c` | 新增 pending | `—` | `fe9e5809ce94776ad353aa4836339e78d769362c54bede44a09a3a384d73e9c8` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_body.h` | 新增 pending | `—` | `6376d741e25d141bb5fb9d231b4c2db0b2ab6dad9acd9b2e4336ef9693f54a5f` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h` | 刷新 pending hash | `b202f47bc073f64d60223bd624e9fe42aaa79e8c74d85bbb32203169a6f3abf8` | `022a0ac6df1dbc6a31782e19107e15ad329021128b24c5eab6a05adbf67f5e45` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_meta_function.c` | 刷新 pending hash | `e0ed61f6a1a88a5de988cfa50bf8ed1b193b3c8a09ad3d4ee765059f8e95a116` | `ccca7ccd6284114f5b5f4979c2805d252268797b0c934d5d5709ffa4bfd3de41` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.c` | 刷新 pending hash | `235d4f3526d1dbbc76c75d55f12c75cf82d5e1b24f9e7ff7b681cf2a9969f556` | `85df2558af8b8d25642480c12166cb45ebc018f1a51e574d79eaeb96769a0328` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.h` | 刷新 pending hash | `44d82d26fe3d0d497cc0b679e598df4ab2d5e47fc2f0b0f07a7f6d3acbb3209c` | `4219e7d6956d7db4ddb5bfa605cfff77fe8efe1a406cfbb925a0f43a09a1347a` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` | 刷新 pending hash | `bd0db5035f039aaba8a6c3931cd0a07d5f71eb1364b5bc4bcdddfe620b5d9311` | `864a72383d127dd5237e1d9c1710b49607440d5342d1598aece86c9a8f56aa0e` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening_null_state.c` | 新增 pending | `—` | `1bb3bdcb15d7af4d2654a4ab82136d9ae7f9f04aab78df89ed554d3f6c3a5eab` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening_null_state.h` | 新增 pending | `—` | `b7635cdaa0e51367347af68f4834d4e8480c06b7f5943009238fde68285ed1cb` |
| `zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_binding_refinement.c` | 刷新 pending hash | `5f23fc1fb01d8cf43fb7d78e3a30b05d32936d0e70163f843e28ab7e38161cad` | `2d3b9b8f05739fdd8adcd2f280a1c8cf923c564f0285cce830dd72ddbc3d4762` |
| `zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c` | 刷新 pending hash | `37922d77228867420f50f284056cf5f7098480af290b6546ac8cb7f258738952` | `5bb35c2a2dfd324ff1c7a7f342a48d13c4d37f48f1020bf1729f5e8660a13044` |
| `zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_function_assembly.c` | 刷新 pending hash | `8e844d74c9dcddf957b0dc33aa662954a2228dec624ecade2ff935649321acd2` | `c75fba56838ec56b1d711086045782c75d051fe60baff008debabf1d0c7fb139` |
| `zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c` | 刷新 pending hash | `05ece75b31f3f9974bbacbc486e6d245fde1e90fa2ed42c635edadb67b5096ea` | `5b836db9d02a137118749bf102df0cf55112a7d9b8e49f561ce8cb3a956d97b6` |
| `zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c` | 刷新 pending hash | `32ea1120d6819981c99430c87e856324824ae7696b19d0f0671160aeb46f06f3` | `4d649f7f8a9f8e8f4082af80101c7f88fececc196eed1e4ab239491f3820d68a` |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c` | 刷新 pending hash | `8dcfd21add5a61788c25d4490adaa170b09502168415957101f14ecbbead6a01` | `821526dbac54713aeead82dd4d84cc27082aa0286fb21147ab30edee60c623b6` |
| `zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_try.c` | 刷新 pending hash | `d2b88f079d3db1cf396456a0911d4dc57673b536db91c898f04ab2ec9df123c1` | `3387db198137b22e927f6c0c16b5fe13a38e222ddf63f709f0cf2872e9c65735` |
| `zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c` | 刷新 pending hash | `25ee7b1cdbb82f620714ddae304fe3532a14ce53252d06fb45c9f0d06a41f9d0` | `0b2ad1fddb56bb186bf8ba5d512ad64d8ecd8de8ea65bd82033344b947261ff7` |
| `zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c` | reviewed → pending | `26818fab2de47aef8927683fcce16ece29fdb316a4ffd9745fbc2dfd024980e5` | `a793960c188bb57d1b234bae511e155eeb508680ee790bb20aa02d6736cf2da4` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c` | 刷新 pending hash | `52b7f8abe638f5ebac8c895eb6b318bf3a3d795b77e8a3b1d12126ae0f876881` | `d7c8d17aba2973b21f305e954d36b5cac35dd0b0ff1fabd73a7774dec8e49430` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c` | 刷新 pending hash | `6f4b4e2415aff7bb9f301fe2a6772bc3b32e3c42bbf79ea56e9e2d636173addb` | `68c6890958fe8b5c12e71ccd0670c3256fffc916bea48c4152315ad12ece6c0a` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h` | 刷新 pending hash | `7a9432d2f94b8bca92125c3f81adc1575cbfa965490a394c146a33d2a36b8024` | `8e963a863b07af98b0279965a93537955b1962ed0697d2352c8b865972b5a78c` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c` | 刷新 pending hash | `12e3895326feacbe2595ca7f565a725c126adfa7267c2ee03fc092d9b9f489ec` | `fb4be3e4b20f7d4dd33c616cb5db9c6ad6dfae10e88a03cb47ca74ba990c7dc0` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c` | 刷新 pending hash | `590969b2f1fdb322c1d8ae971d10c21f6bc0d8ea710fd977626f075018f18860` | `04fcc1cfac4b345935178f490ac6b76c0030a2abca10218adad40662a7ea01ad` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression_internal.h` | 新增 pending | `—` | `bcd8e42027229cb7938a06bd7e11a3f013b258c128ba62150c737a9200d0e0e6` |
| `zr_vm_parser/src/zr_vm_parser/syntax/token_stream_clone.c` | 刷新 pending hash | `55d11def77e068424771f8b578642e5f1fc2906eca64f8951249a73db2a9a083` | `d0d17b2f58439f3cfc9fda757b1dc9de90ca49d22e62526f327d1bd8708c4f83` |
| `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_callable_member.c` | 刷新 pending hash | `6573b2f88ff2e2b5af4f7decd739d51936404e50366c7261e45f86a851e6f722` | `8d66d524bae33b380c0096969c3716e24563a1f30ba84d3f7769febe2c23af15` |
| `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c` | 刷新 pending hash | `b42dac13d0ae23c471960f4dc8bb774aa2e68e4b12525b4b8c5daa9a47938741` | `d42c34d377929341f74a4af826c2641445549d8c3888a86e76134a8e832bb895` |
| `zr_vm_rust_binding/CMakeLists.txt` | 刷新 pending hash | `c078176ed00310b349d154b829484b39ad1739b6ab01cf629482edc3c8c4b092` | `cd492e5ec0fa416a2d9244efc66238dbc5030c60ce91e3fa53a6462e51ed8af8` |

## 延后范围

| 文件 | 延后原因 |
| --- | --- |
| `scripts/layout/apply_moves.py` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `scripts/layout/dormant_contexts.py` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `tests/CMakeLists.txt` | deferred_source_changed_during_proposal |
| `tests/cmake/container-pair-numeric-tests.cmake` | deferred_source_changed_during_proposal |
| `tests/cmake/ssa-source-direct-validation/CMakeLists.txt` | deferred_source_changed_during_proposal |
| `tests/cmake/ssa-tests.cmake` | deferred_source_changed_during_proposal |
| `tests/container/test_container_pair_numeric.c` | deferred_source_changed_during_proposal |
| `tests/parser/test_pre_semantic_ir.c` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `tests/tooling/test_layout_dormant_contexts.py` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `tests/tooling/test_layout_moves.py` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c` | Same inventory file row already differs across a recorded HEAD/index/WT observation. Preserve it even if current purpose seems equivalent. |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c` | deferred_source_changed_during_proposal |
| `tests/cmake/native-call-binding-status-tests.cmake` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/cmake/ssa-source-integer-expression-faults.cmake` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/cmake/ssa-source-integer-expression-proof.cmake` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/cmake/ssa-source-integer-expression-support.cmake` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/cmake/value-debug-string-boundary-tests.cmake` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/core/test_value_debug_string_bounds.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/library/test_native_call_binding_status.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/support/ssa_source_integer_expression_faults_fixture.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/support/ssa_source_integer_expression_faults_fixture.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/support/ssa_source_integer_expression_faults_frame_shim.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/support/ssa_source_integer_expression_proof_shim.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `tests/parser/test_ssa_source_integer_expression_proof.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `zr_vm_cli/src/zr_vm_cli/command/command.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `zr_vm_cli/src/zr_vm_cli/command/command_numbers_internal.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.c` | deferred_late_discovery_requires_fresh_scope_and_origin_review |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.h` | deferred_late_discovery_requires_fresh_scope_and_origin_review |

独立复核晚发现的 tests/parser/test_ssa_source_integer_expression_faults.c 及之后新增输入不在本轮选定集合，最终重扫时重新清点。
