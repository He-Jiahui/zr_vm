---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h
  - tests/parser/test_ssa_deopt_aggregates.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_deopt_aggregates.c
doc_type: code-review-batch
---

# ExecIR Deopt aggregate recipe 契约审查 — 2026-10-16

## 完整范围与入口

三份物理源码共60单元：生产C16函数、私有头3接口及普通guard、测试37函数与3静态状态。59项使用显式 Doxygen（56函数/接口、3状态），guard无需重复注释；60定义span、53函数体、1,690处文档/台账引用（630唯一）均核。详见 coverage/zr_vm_core_exec_ir_deopt_aggregate.tsv。测试的fault allocator C/H已在前批审查，本批只作为宏重编译、注入及清理依赖，不重复计数；没有把公开头或其他调用方纳入本批信用。

私有validate先核池形状、容量/逻辑区间及尺寸，再核字段种类和值引用与selected state内identity图；不是运行时对象物化。公开无诊断入口和VerifyFunction、Builder、MaterializeState共用recipe前提。跨对象identity及fieldIndex约束在各selected graph内成立，不擅称全局唯一；图用扫描核匹配，不递归遍历对象循环。

ValueReferenced仅查询VALUE字段元数据；At按给定指令selected state和fieldsOnly约束处理。Hash只在前置验证允许的元数据域混入recipe内容，不提供碰撞自由或有效性认证。live只读已有liveness语义、核逻辑字段必须在map中，独立于更早owners检查。prepare为已初始化空候选复制selected graph并重基fields.start，由上层成功后发布。

## 所有权、失败与时序

输入function/state/instruction及各池在同步调用中保持有效、匹配并不并发变更；诊断槽独立可写、可空。诊断助手非空调用始终先清零重建；仅上层成功而未调用助手时可保留旧诊断，不能把此行为错写为助手的成功分支。

prepare失败可能在第一次recipe malloc成功后保留部分候选存储，单独调用者必须回收候选。MaterializeState先在局部prepared构建，失败统一Free候选；只有成功后才Free旧target并发布新记录，源function和旧输出owner保持分离。成功替换与释放会使旧池借用失效；浅拷贝old/published只用于观察，不成为可独立Free的所有者。返回值/recipe是逻辑ID与图描述，不延长真实GC对象生命周期。

测试全局function/target在setUp初始化、tearDown释放；diagnostic未在setUp显式清零，按各被测入口契约消费。Unity失败longjmp跳过本地余下释放或借用恢复，runner另保护执行tearDown并继续其他case；局部clone/map不由global teardown自动接管。测试共享故障注入计数，只串行使用。宏替身只控制本生产C两处recipe malloc，不覆盖全部分析/clone/运行时分配。

## 既有测试与已证缺陷

现有专用目标用Unity注册28case，覆盖有限配方形状、identity/字段类型、循环别名、selected graph复制、逻辑roots、源/输出池隔离、两处recipe分配失败以及DCE/hash协作。各行标出实际断言和首拒边界；调用成功、完整状态字节比较、局部池地址或单个逻辑root不外推所有payload、真实GC/VM执行、任意OOM或全部图形。

BUG: tests/parser/test_ssa_deopt_aggregates.c:535 的前置 VerifyFunction(ALL) 成功断言已在未改源码的实际原基线复现失败：旧map在523/524发布，525–533新增COPY result和recipe引用后未重建旧map，535返回FALSE，536–539预期Builder拒绝、诊断及旧map保留检查未达。原R1证据 exec-ir-deopt-aggregate-native-before-r1-summary.json（SHA256 5534553b99fe8f236a8cbc2708adebb5f5b3abc014862e230dcd299fa4eebe22），28case27PASS1FAIL。新鲜R4在同三份原始字节下再次取得相同28条结果及完整stdout/stderr、build0/CTest8/direct1，仍非全绿。该BUG只标记测试前置场景与断言不符，本轮不改断言、实现、签名或构建配置。

条件静态追踪显示：先前结构/SSA/effects验证及OwnerAnalysis/Liveness成功且无OOM时，VerifyFunction909–912验证已发布旧map；Builder原首entry为BEFORE_EFFECT。liveness122加recipe2、127移除result2、132 fieldsOnly重新加入2、137加operand1，BEFORE需要1/2；Materialize503先owners再504 aggregate-live，旧map未列2导致owners62–63拒绝。此为条件推导，运行日志只证明535断言失败，不冒称已采集最早内部predicate/诊断或生产API存在缺陷。

前次R2因外部Parser输入运行内漂移而不作为最终稳定基线；R3在CMake生成时遇到尚未出现的外部新注册测试源，未运行CTest或本目标。两次失败尝试的原件均保留，未通过改配置或修复行为绕过。

修改前R4运行内86个固定文件和3,583个保守观察输入稳定。修改后严格词法检查通过：3源码共7,483个有效token，其种类、拼写、StartOfLine、LeadingSpace及原始坐标相等。修改后实际build0、CTest8、direct1，28例27PASS1FAIL535；全部28条结果、完整direct stdout/stderr与前基线字节一致，原失败仍未修复。修改后运行内88个固定文件及3,583个观测输入稳定；运行间变化4项，其中3项是本批源码注释，另1项为外会话输入 tests/parser/test_ssa_source_integer_expression_faults.c；新增输入0项。native AFTER summary exec-ir-deopt-aggregate-native-after-r1-summary.json（SHA256 cf573868c866558b7789f40eeee5faafe6e94c5ab0dccb3e4b25379f26476019）。本批未将两次运行间的外部变化视作隔离实验，不外推全仓、Linux、默认SDK、原子checkout或暂存树执行。实际源码/台账与暂存独审、串行提交仍须后续完成。

## 产物边界

只修改三源码注释、模块leaf/batch及inventory三行审核列，共6路径。复用已登记E盘产物，不建源码副本，不新增测试。生命周期open，释放0B；其他会话内容和必要失败证据保留。
