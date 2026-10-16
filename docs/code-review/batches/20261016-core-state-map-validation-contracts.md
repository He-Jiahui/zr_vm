---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - tests/parser/test_ssa_state_maps.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - tests/parser/test_ssa_state_maps.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/test_ssa_licm_scalar_legality.c
  - tests/parser/test_ssa_licm_scalar_context.inc
  - tests/parser/ssa_state_map_verifier_cases.inc
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
doc_type: code-review-batch
---

# StateMap 验证契约审查 — 2026-10-16

## 范围与结论

本批台账为 `docs/code-review/coverage/zr_vm_core_state_map_validation.tsv`，共41个唯一(file, unit, kind)：Core验证14函数、公开声明1、checkpoint身份语句块1，以及测试C的25定义（18个case、4个fixture helper、2个Unity hook、main）。结论为37项已注释、3项无需注释、1项TODO；追加38份中文Doxygen，不修改公开签名、执行语句、断言或构建设置。既有Query/Builder/Clone/Materialized单元及文本include定义不重复计数。

三个文件继续标为部分审查：实现17/19函数定义，公开头10/26强制单元，测试C42/44函数定义。MaterializeState/size helper、5种头文件类型、10个boundary宏与两个剩余case尚待各自批次；include guard是可选额外单元，不混入头文件26口径。main的42个RUN_TEST由当前C的35个case、verifier.inc的3个case和clone-fault header的4个case组成；注册数及同TU编译不等于新增审查信用。

## 验证职责、前提与资源

ValidateStateMap可验证未附着于function的显式map，但token/signature/generation须匹配。它先清零独立可写的diagnostic，再检查基础形状、IR/deopt结构、构造owner/liveness scratch并验证已有entry与跨entry身份。输入及嵌套池在同步调用期间须稳定可读；没有保存、复制或内部同步。整数range/count与非空指针检查不证明实际分配extent。该接口不要求map覆盖所有潜在boundary。

OwnerAnalysisBuild自身失败负责清理部分scratch；其后的失败与成功均释放owner/liveness scratch。返回false可来自参数、结构、identity、deopt、owner/liveness、entry或资源失败，不能把所有false解释为单一诊断。handler按source block的successor数组顺序选择首个可解析EXCEPTION或CLEANUP块；两种标记没有额外优先级，THROW且无合格successor时INVALID/INVALID允许通过。pairwise规则分别限制同instruction、同resume与重复(source,resume,phase)，sourceId本身不要求全局唯一。

私有helper在台账内分别记录其全部可检索调用、前置形状与诊断边界。语句块随所属函数审查；checkpoint identity pairwise块另列，说明三类相容性规则与统一失败释放路径。查找未发现的直接或跨语言引用不等于宣布死代码。

## 测试所证明的范围

固定fixture仅构造逻辑IR和ID，不创建真实GC对象或证明其保活。构造helper要求新鲜或已释放的function，FunctionInit覆盖旧拥有池不提供释放保障。各case正常尾部释放自身map/function/target；Unity断言longjmp可跳过局部清理，空setUp/tearDown不补偿。main返回UnityEnd记录的失败case数。

TODO: `tests/parser/test_ssa_state_maps.c:1140` 的borrowed-across-suspend case仅断言false。calloc清零entry后未补exceptionState，而EXCEPTION_MASK包含SUSPEND；MaterializeState:685 → ValidateStateMap:621 → entry_valid:486先拒绝exceptionState，尚未到validate_value_range:505/275的BORROWED检查。待核查入口状态和目标分支覆盖，不能把测试名或整体PASS作为该分支证据；本轮不修改case，不记生产BUG。

`rejects_deopt_value_outside_live_state`:688保留deopt payload却清空live/root/owner范围；liveness含该deopt value，owners_valid在materialize_owners.c:52/62先拒绝缺live occurrence，不为后继entry_deopt membership分支授信。`rejects_malformed_owner_operand`:759在OwnerAnalysisBuild输入验证先拒绝越界operand，未进入逐entry owner比较。UNIQUE/SHARED正例只断言成功与rootCount=1；malformed-function负例只证明所构造NULL池被拒绝及旧target.values指针/首值保留，不推广成任意地址安全。

## 修改前基线与并发边界

实际checkout原始三个SHA及长度由外部source lease记录，所有原字节保留，Doxygen追加在EOF以保留既有非注释坐标。R2修改前构建四个目标exit0；直接运行StateMap42、SCCP14、retained8、LICM29，共93个各runner口径场景均通过。初次CTest3/4通过，LICM在其既有30秒限制下超时；相同binary的单独CTest复核0.08秒通过，原超时日志保留，不据此推断根因或已修复。

R2原运行期间62 frozen源稳定，3482个保守输入仅另一个会话的LSP semantic analyzer测试发生变化；保守清单不表示全部被编译或审查。R2结束后、LICM复核期间，另一个会话继续修改tests/CMakeLists.txt和conditional-cleanup case；复核binary未变但62源保护不成立，所以复核只是同binary运行证据。三份本批源仍等于提案、R2和HEAD原始前缀。上述并发改动完整保留；修改后验证会刷新实际输入，不声称整个checkout固定或全套测试通过。

## 修改后验收

最终Clang raw-token R2检查通过：三个文件合计14586个非注释token的种类、拼写、StartOfLine/LeadingSpace以及原有行列坐标均一致；原始字节前缀完整保留。独立源码与台账审查ACCEPT，核对38份文档、41个唯一单元、37/3/1结论和717个调用/证据锚点，未把既有单元重复授信。

修改后复用Clang-cl Debug构建四目标exit0，四项CTest全部通过；四个程序直接运行仍为42/14/8/29，共93个场景，逐项名称及结果行与修改前完全一致，不与CTest重复累计。修改后62个冻结审查输入在运行期间稳定；3488个保守输入中exception.c在另一会话发生变化。修改前后另有14个既有输入变化（本批3个注释文件与11个外部输入）和6个新增外部输入，均保留实际SHA记录。这些证据支持本批注释等价和相关现有程序运行结果，不构成固定全仓、完整测试套件、独立暂存树或Linux验证。

首次暂存检查发现三份新增注释尾部各多一个空白行，git diff --cached --check拒绝，因此没有形成提交。格式修正仅分别移除最后一个LF，全部此前字节与38份文档正文保持一致，更新三个inventory SHA并重新通过严格token/flag/原坐标检查。上述native构建与运行实际对应修正前raw；当前raw与实测源仅差这三个尾部LF，格式桥接收据逐字节绑定此差异，不声称修正后另做一次native构建。失败日志和原实测SHA均保留。

提交门禁为仅暂存本批三个源码、41行台账、本说明及inventory的三个指定行列，逐项审查差异、运行git diff --cached --check，并要求独立暂存审查ACCEPT。串行收据在核验实际commit的6路径、blob、parent、He-Jiahui身份以及外部索引/工作树保留后授予41项信用；证据存于同任务的stage、commit-delivery和postcommit审计记录。

## 证据与生命周期

外部证据位于 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`：`state-map-validation-comment-proposal-r3.json`、`state-map-main-validation-tests-comment-proposal-r2.json`、`state-map-validation-independent-final-proposal-review-r2.json`、`state-map-validation-preedit-native-r2-summary.json`、`state-map-validation-preedit-ctest-retry-r1-summary.json`、`state-map-validation-root-proposal-audit-r1.json`、`state-map-validation-source-lease-r1.json`、`state-map-validation-format-correction-r1.json`、`state-map-validation-comment-equivalence-r2.json`及`state-map-validation-final-native-r1-summary.json`。TODO唯一格式规范化在lease中绑定；不改其原提案字节或语义。

复用同一实际checkout、E盘build/cache及不可变验证助手；无源码副本或新增测试。所有构建日志/收据按artifacts.json登记保留。生命周期仍open，本批释放0字节；既有自动清理拒绝记录、未知或其他会话产物均保留。
