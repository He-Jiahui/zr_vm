---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/ssa_state_map_verifier_cases.inc
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/cmake/ssa-tests.cmake
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/third_party/zr_unity/Unity/src/unity.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/parser/test_ssa_state_maps.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/ssa_state_map_verifier_cases.inc
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/cmake/ssa-tests.cmake
doc_type: code-review-batch
---

# Core state-map query contracts

本批审查六个精确单元：Core 的 zr_state_map_phase_valid、ZrCore_ExecIr_StateMapBoundaryFlags 与 ZrCore_ExecIr_StateMapFind 三个定义，public header 中 BoundaryFlags/Find 两个接口声明，以及 test_state_map_find_supports_each_logical_phase 一个测试。九列单元合同见 docs/code-review/coverage/zr_vm_core_state_map_queries.tsv。仅在三个文件 EOF 追加 Doxygen；不改变签名、可执行 token、测试断言或原行坐标。无新增 BUG/TODO。

## 阶段与边界位

Core 的 zr_state_map_phase_valid 只接受 BEFORE_EFFECT、AFTER_EFFECT、CLEANUP_COMPLETE；Find、entry 检查和 MaterializeState 请求检查复用此 Core 静态定义。Parser 中的同名静态 helper 位于另一翻译单元，只是相似定义，不与本批 key 合并。

BoundaryFlags 根据指令显式标志与 opcode schema 属性，逐项计算 MAY_ALLOCATE、MAY_GC、MAY_THROW、MAY_SUSPEND 对应的边界位；DEBUG_POLL 取自指令标志，GUARD_EXIT 取自指令 GUARD_EXIT 标志或非零 deoptId，非零 deoptId 另贡献 DEOPT；DROP 与 DROP_IF_INITIALIZED opcode 贡献 CLEANUP。任一参数为空时在输出清零前返回 FALSE；两参数均非空时先清零，未知 opcode 或结果为零也返回 FALSE。flags 必须可写且独立于 instruction 存储，这个函数不检查别名。Core entry 校验和 Parser state-map 构造器是已检索的实际调用者。

## Find 的形状边界与借用

Find 只做轻量检查：map 非空、phase 受支持、resumeId 非零、entryCount 不大于 entryCapacity，且非零 entryCount 对应非空 entries。随后按数组顺序精确比较 sourceId/resumeId/phase，返回首个匹配 entry；sourceId=0 不因本接口额外拒绝，重复键不报错。它不代替 StateMapStorageValid/ValidateStateMap，不检查其他池、checkpoint 语义、底层分配 extent 或内存可读性。调用方须确保 map 描述符与被扫描条目在读取期有效、可读且稳定。返回指针直接借用 entries 中的元素，不是快照；改写条目会改变借用所见内容，替换、重分配或释放 entries 后借用失效。没有内部同步。

全仓直接调用已核对到三个 Core 生产调用（exec_ir_materialize.c:688、exec_ir_interpreter_resume.c:38、exec_ir_materialize_objects.c:117）；另有 verifier 测试 .inc 一处、pass-manager 测试 .inc 两处，以及 main state-map、retained-builder 与 deopt-aggregate 测试中的实际查询点。leaf 按具体行记录。未把类型引用或同名 Parser phase helper算作 Core Find 的调用。

## 测试的实际信用

test_state_map_find_supports_each_logical_phase 初始化一个空 map 并分配三项，共用 sourceId=10、resumeId=20，phase 取0、1、2；每项查询只断言结果非空并且返回 phase 相等，另对 PHASE_COUNT 与枚举负一断言 NULL。它没有断言返回指针恰为对应数组下标，也没有覆盖 NULL map、resumeId=0、miss、重复键首项选择、损坏描述符、全量 map 验证或借用失效。正常路径在函数末尾释放 map。当前 CMake target 包含空的 tests/unity_config.h 配置入口且未排除 Unity setjmp；断言失败会中止当前 case，可能跳过局部 Free，runner 随后调用当前空 tearDown。该测试不单独证明Find的其他失败分支。

## 实测基线与本批状态

修改前使用实际 main 工作树与复用的 clang-cl-products-debug 配置，三个目标构建退出 0。ssa_state_maps 的 42 项与 ssa_oracle_resume 的 22 项通过；额外 conditional_cleanup 的 24 项中，test_elaboration_owner_analysis_oom_is_transactional 在 tests/parser/test_ssa_conditional_cleanup.c:659 失败，消息为 Expected TRUE Was FALSE。合计 88 项为 87 PASS、1 FAIL，CTest 退出 8，conditional 直接运行退出 1。该失败已在 coverage/zr_vm_parser_conditional_cleanup.tsv 及对应 batch 记录，本批不新增同一 BUG，也不修复行为。fresh-before adoption SHA-256 为 04d4737c9a8ea139789510fba96f858eabadd93c55d74a0b3dbb0ff76013f5ae。

修改后重新构建相同三个目标并运行相同 CTest 与三个可执行文件。构建退出 0，42+22 项仍全部通过，conditional 仍为 23 PASS、1 FAIL；两次 CTest 均退出 8。Root 逐项比较了 88 个直接运行用例的文件名、原行号、名称、结果和消息，全部与 fresh-before 相同，并核对 CTest 所列用例与直接运行结果一致。after 摘要 SHA-256 为 aa8a6f43ec77350c16c938925648d38034bb24df2db7ca81ea2ca99ebe3be56d，comparison 为 4e60c33b1a6ffae7f6060c83cc8c89906321d105446e930884869c41b6b632c4。记录位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/，不能将这组结果写成全通过。

三份源码的原始字节前缀全部保留，只在 EOF 增加六块 Doxygen，含 13 个方向参数标签和 5 个非 void 返回标签。Clang 对 14,586 个非注释 token 的种类、拼写、起行/空白标记及原始行列逐项比较相同，六个 lexer 子进程均退出 0；严格检查摘要 SHA-256 为 64ac8345a8f1eb5388ff9d7df718091c3f3f8aa3d526c54fb4d7f2ae793f73c9。BoundaryFlags 文档经过 Root 最终澄清，强调按各属性分别判断而非将两个不同编码的输入位掩码数值相或，已包含在该检查与 after 构建中。

before 与 after 的 3,216 个保守编译输入中只有本批三份源码发生变化，其他输入相同；after 运行期间及完成时，全部 3,216 个当前输入与 21 个冻结依赖均保持稳定。该清单包含未必被这三个目标实际编译的文件。driver、helper 与五个子进程均已等待到实际终态。使用实际工作树及现有 Unity 配置覆盖，不声称运行了干净 HEAD、暂存树、默认 SDK 配置或全仓库测试。

全部可检索扩展名均纳入调用检索，含 .inc：Core phase helper 有三个实际直接调用，BoundaryFlags 有两个，Find 有十七个；Parser 同名 phase helper 单独排除。测试经 RUN_TEST/UnityDefaultTestRun 间接执行，未发现额外函数指针或跨语言入口。六个 key 均有结论，未新增 BUG/TODO、可执行逻辑、签名、断言或测试。

父文件仍是部分审查：exec_ir_materialize.c 完成 Query 三个函数，另十六个函数待审；public header 与 Clone、Materialized 两批合计九单元，其余类型和声明待审；主测试与既有批次合计 17/44 函数定义，另 27 个定义及适用声明待审。inventory 仅更新这三行的实际源码哈希、部分状态及说明；其他工作树记录保留。

独立 reviewer 与 Root 在最终暂存补丁复核后串行提交。历史交付台账项在提交收据核验前仍为 1,763，提交成功后增加六项至 1,769；此数字不是全仓库物理定义去重总数。复用构建、缓存、临时 helper 和必要验证证据继续保留，本批释放 0 B，产物生命周期保持开放。

## 提交前当前工作树复测

第一次 after 关闭后，另一会话提交了 67a3fed011c3dc971560a772b293d25f7b7d1edc，父提交为 619c9eebfe9ed9a3ed86b613b6fc6eb69981b4d3。该提交只涉及三个 SSA 差分测试文档和 tests/core/test_ssa_differential_harness.c，与本批六个路径无重叠；差分测试源码的实际工作树字节已在第一次 before/after 输入中。Root 保留这一提交，以新 HEAD 为本批提交父节点。

此外，提交前扫描发现两份非本批输入在第一次 after 之后变化：tests/parser/test_lambda_semantic_ir_isolation.c 从 3ae80df40ed55eb0eaf396d0e809e60ff2aeca751b171ccbee25954081f3540f 变为 915fe17bfa60d500a0dc3c8354a016810760da68d0c152b57cc11686b54f222d，zr_vm_core/src/zr_vm_core/runtime/execution_contract.c 从 d7d5240be5ec2de58ee4a45fb277d9bfc135d0c9c7f1fc3a806a603c5ef369c2 变为 d82e8233679cf18697fa5422545ac3ab99c93cce058d857bb27fd62619c889ac。它们不纳入本批，也没有被证明行为等价；不能把第一次 after 结果直接当作当前工作树已验证的证据。

Root 因此对当前工作树重新构建相同三个目标，执行同一 CTest 与三个直接运行。构建退出 0，88 个用例的身份、结果及消息再次与原 before 和第一次 after 完全相同，仍为 87 PASS、1 项既有 :659 FAIL；CTest 退出 8，conditional 直接运行退出 1。当前 R2 摘要 SHA-256 为 38190425ab9c509a40d47129d8d9803c0cd87bb5520d8a75c73809b2ab6fde7a，comparison 为 7f2dfe39bf9281bbb46f14619741a9397c0c141bbf45e30518c9b168aebd865c，记录前缀为 state-map-query-current-native-r2-。R2 运行期间及 runner 实际结束时，3,216 个当前输入及 21 个冻结依赖均稳定；Root 随后复核 21 个冻结依赖未变，并等待 driver/helper/五个子进程实际终态。

本批三份源码从严格注释检查后保持相同哈希；此前的 14,586-token 与原行列等价证据继续适用。新一次结果只授予该工作树、该配置和三个目标的有限运行证据，跨上述两份外部源码变化不具有完整依赖隔离，也不代表干净 HEAD、暂存树、默认 SDK 或全仓库测试通过。原交接、R1 暂存收据与验证报告保持不变；后续授权收据明确记录 Root 的 batch 补充、相同六路径补丁及自有备用索引接到新 HEAD 的过程。
