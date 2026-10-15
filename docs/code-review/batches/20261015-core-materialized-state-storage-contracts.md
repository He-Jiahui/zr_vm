---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - tests/parser/ssa_state_map_fixture.h
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
  - tests/parser/test_ssa_aotir_state_map.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/cmake/ssa-tests.cmake
  - docs/core-runtime/execution-state-maps.md
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_aotir_state_map.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_aotir_state_map.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/cmake/ssa-tests.cmake
doc_type: code-review-batch
---

# Core materialized state 存储与投影边界

本批审查 19 个精确单元：storage C 中 5 个函数，private header 中 2 个接口，public header 中 1 个类型和 3 个接口，主 state-map 测试 C 中 2 个 fixture helper 与 5 个测试，以及 AOT state-map 测试中的 1 个用例。单元身份、直接调用位置、约束、锚点与逐项决定记录在 docs/code-review/coverage/zr_vm_core_materialized_state_storage.tsv。

这些 key 与 Builder leaf 中的 BuildStateMaps 单元和 Clone leaf 中的存储 shape、Clone、Init/Free 单元不重复。主 test_ssa_state_maps.c 的既有 Builder/Clone 9 个 key 加本批 7 个 key 后为 16 个已登记单元，文件其余单元仍未由本批取得审阅信用；AOT 测试只审 test_aotir_owns_state_map，另一个 frame-layout 用例仍在本批范围之外。Shared fixture 文件的 13 个 key 也未在本批重计。

## 存储检查与池生命周期

ZrCore_ExecIr_StateMapStorageValid 和 Clone 复用同一内部 shape helper：它检查 map 四池 count/capacity、NULL 与 capacity 配对、容量字节可表示性和池区间重叠，不解析 entry/checkpoint 语义，也不证明池指向的内存真实可读或由谁释放。公开 StorageValid 由 Core 校验、Parser lowering/projection 和 AOT 校验调用。它的 true 结果不能代替 ValidateStateMap。

MaterializedStateInit 只清空无资源记录；对仍持有池的记录调用会丢失指针，造成泄漏。MaterializedStateFree 释放 values、roots、ownerStates、aggregates、aggregateFields 后将记录清零，但不释放记录对象。每个非空池必须独立且满足 C free 的前提；释放后池及元素借用失效，记录地址继续有效且字段为零。

输入区间 helper 按输入记录及容量池比较候选字节区间。候选 bytes 为零时 storage 可空，但输入容量尺寸仍需可表示；该检查不证明输入地址对应可读内存。目标 helper 核对五组池的 count/capacity、NULL 配对、尺寸和目标容量区间互斥，也拒绝目标池覆盖 function/map 记录及其池。它不验证 target 记录自身与 function/map/request/diagnostic 的别名，不验证旧池能否释放。MaterializeState 的调用方仍须提供独立可写的 target 记录，并保证被替换的旧池可由 C free 释放。

SZrExecIrMaterializedState 中的五组 count 均不得超过各自 capacity，capacity 为零时指针为 NULL，非零时指针非 NULL。values 与 roots 是逻辑 TZrExecIrValueId 清单，roots 不是对象指针；ownerStates 保存 owner-state 元数据，aggregates 与 aggregateFields 保存 deopt recipe。source/resume/phase/cleanup/boundary/generation 和 effect/exception/deopt 字段必须和产生 checkpoint 的匹配 function/map 上下文一起解释。该记录不携带 functionToken/signatureHash，这两项身份检查来自 ResumeRequest 与其关联的 function/map。ID 清单不会延长 function/map 或运行时对象生命周期。五组池随记录状态管理，record 本身的内存由外部所有者管理；清零记录和释放池是两个不同的生命周期边界。

## 测试实际覆盖

主测试的 map_with_one_entry 固定 generation=7、sourceId=42、instructionId=1、resumeId=7 和单元素池，不填 functionToken/signatureHash。需要与函数匹配的正向验证时，调用方须另行设置匹配身份；phase/flags 原样写入，helper 不保证任意组合构成合法 map。function_with_one_gc_value 固定若干 identity 字段，但只追加一个 GC external value 与 MAY_GC NOP，不构造可执行 CFG。两个 helper 的局部分配断言失败时，Unity longjmp 会跳出当前 case，跳过后续局部资源释放；测试 fixture 的 teardown 不持有这些局部对象。

test_state_map_materialization_successfully_copies_logical_values 检查一个 AFTER_EFFECT/GC checkpoint 成功后 sourceId、valueCount/rootCount 和第一个 values/roots ID。它没有断言 ownerStates 或所有池元素。

test_state_map_materialization_rejects_overlapping_map_pools 构造 rootPool 指向 valuePool 第二个元素的源池重叠，检查失败、STATE_MAP_INVALID 和 target.values 仍为空；清理前手动解除 alias。test_state_map_materialization_rejects_interior_target_alias 让 target.values 借用 map.valuePool 内部元素，只检查 MATERIALIZATION_FAILED，并在释放 map 前分离借用字段。test_state_map_materialization_rejects_overlapping_target_arrays 令 values 与 roots 两个目标容量区间重叠，只检查失败、诊断码及 values 基址保持；共享分配仅手工释放一次。

名字含 transactional 的失败用例用 AFTER_EFFECT 且 flags=0 构造 map。MaterializeState 先在 exec_ir_materialize.c 的验证阶段调用 ValidateStateMap；boundaryFlags 为零会在 exec_ir_materialize.c:484 被拒绝，早于 generation 比较的 exec_ir_materialize.c:704。因此本用例只断言验证拒绝后 target.values 指针、valueCount=1 和 values[0]=99 保留，不检查 diagnostic.code，也不覆盖 stale-generation、OOM 或普遍失败事务保证。

test_aotir_owns_state_map 执行 AOT lowering，不执行 oracle 或 MaterializeState。它比较 source/output 的 entries、valuePool、rootPool、ownerStatePool 四个基址，核对 entry/root/owner count，变更 source rootPool[0] 和首个 entry.resumeId 后检查两个对应输出字段仍保持原值；同时检查 limited ABI、一个 constant、一个 layout、少量 deopt 字段及四个 deopt 池基址。无效化源 map 的 valueCount 后，它断言 StorageValid 拒绝、LowerAot 报 INVALID_PROJECTION、旧输出 entries 指针和 rootPool[0] 保留；释放 source 后再次核对这两个字段。测试不证明所有池元素全面独立、不覆盖自然 OOM，也不执行 oracle resume。

## 基线与批次状态

修改前在实际工作树和复用的 clang-cl-products-debug 配置上完成 build、CTest 和两个目标直接运行：ssa_state_maps 42/42，ssa_oracle_resume 22/22，共 64/64。before 摘要 SHA-256 为 d34d77914844951f038fa230a680999600d47fcb247f289eca264d7fe7072d05。

修改后重新构建相同两个目标，CTest 和两个目标直接运行均退出 0；42+22 个用例全部 PASS，其路径、行号、名称、结果和消息与 before 逐项相同。after 摘要 SHA-256 为 851e0e55e53d7cd0e8faa1d635dc95fdde6695c3eb0ca02a8e8b95ef581f3894，comparison 为 635acf56ac15178558afbc36e8421ff7918dac2b1f539a8700a247675ee48e21。driver、helper 和四个子进程均已等到实际终态。

五份源码均保留修改前全部原始字节，只追加 EOF Doxygen。Clang 对 13,894 个非注释 token 的种类、拼写、起行/空白标记及原始行列逐项比较全部相同；10 个 lexer 子进程均正常退出。严格检查摘要为 6eac840a4304a2746149ad55e01e3543925396fb6b1f4a14e59c2091bef130e3。公开 header 在 producer 冻结后由 Root 精确修正文档：Free 清空记录，成功替换发布新状态；旧池借用均随释放失效，不将替换描述为结果为空。该差异已包含在最终严格检查和 after 运行的哈希中。

before 与 after 之间另一会话提交了两份 SSA 验证文档，实际 after 准备时 HEAD 为 eed58071d92a2f8d43d6abcff732499864fb673c。此外，非本批的 zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_try.c 从 d2b88f079d3db1cf396456a0911d4dc57673b536db91c898f04ab2ec9df123c1 变为 3387db198137b22e927f6c0c16b5fe13a38e222ddf63f709f0cf2872e9c65735。Root 保留该文件，未纳入本批，也未证明该外部改动的语义等价。因此这组 64 项结果比较不具有完全隔离的依赖条件；其余 3,210 个保守源码输入与 before 相同，after 运行期间及之后全部 3,216 个当前输入与 15 个冻结依赖保持稳定。保守输入清单不代表每份文件均被选定目标编译。

已执行计划继承的 source_root_head_when_prepared 信息字段仍为旧 HEAD；它不参与 helper 的输入选择。保留原计划不重写，实际 HEAD 和更正已另存 postrun metadata，SHA-256 012f28dd581db7252dd15212abbeb617b935613b2c71a3666fa1b8db2b36b999。

test_ssa_aotir_state_map.c 的修改前工作树已有 zr_vm_parser/exec_ir_projections.h 到 exec_ir/exec_ir_projections.h 的 include 迁移。本批保持该原始前缀，提交只暂存 HEAD 原有前缀后的新增 EOF 注释；迁移继续留在工作树。构建和运行验证的是保留该迁移的实际工作树，不声称暂存树或干净 HEAD 已运行。

调用检索随后取消扩展名过滤，补入 .inc 中 Init/Free 两个调用点和一个类型使用。最终 caller audit R2 为 8317e858a3305cf0317e1af2a447ec5cc7798d78bb38fe258b30083d9d472104：Init 36 个直接调用、Free 46 个直接调用；类型 62 处出现包括定义首尾 2 处和显式使用 60 处，另列 3 处字段层面的状态传递/发布/读取。这些类型使用不视为函数调用，也不给所在单元新增审查信用。

本批 19 个 key 均有逐项决定，未新增 BUG/TODO 标签，未扩展测试或改变可执行代码、公开签名和断言。Storage C 的 13 个函数与 1 个类型、private header 的 3 个声明由 Clone 与本批共同覆盖；public header、本批两份测试文件仍是部分完成。主测试目前为 16/44 个函数定义，AOT 测试为 1/2 个测试定义，其余定义及适用声明继续待审。相关构建未启用 Network、Debug、LSP 或 Rust，不能据此声称全仓库测试通过。

独立 reviewer 与 Root 继续按最终暂存补丁复核后串行提交；历史交付台账项在该提交收据核验前仍为 1,744，提交成功后增加 19 至 1,763。此数字不是全仓库物理定义去重总数。产物清理本批释放 0 B，复用构建、缓存和验证证据继续保留，生命周期未关闭。
