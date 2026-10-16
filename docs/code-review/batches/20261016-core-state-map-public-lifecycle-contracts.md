---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
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
  - tests/parser/test_ssa_conditional_cleanup.c
doc_type: code-review-batch
---

# StateMap 公开数据与物化生命周期审查 — 2026-10-16

## 范围与计数

本批21个唯一(file,unit,kind)：Materialize相关4、公开schema15、最后FunctionClone case1、include guard可选1。20项补中文Doxygen，guard1无需注释；无新增BUG/TODO，不改签名、执行语句、断言或构建。台账为coverage/zr_vm_core_state_map_public_lifecycle.tsv。

前批680f012已交付Validate41；此前三个文件共有70个唯一单元。本批不重复这些键，合并后91项：实现19函数及identity语句块1、头文件26强制单元及guard1、测试C44定义。头文件26=6类型+10boundary宏+10接口；enum成员随类型、opaque typedef随记录定义审查。测试C44=35本文件case+6helper+2hooks+main；main42次dispatch另含inc3和clone-fault header4，include定义各归其自身，不重计。多行/指针返回签名通过顶层token/brace清单核对，单行regex不作为全量覆盖依据。

## 调用契约与所有权

MaterializeState先清独立diagnostic，完整Validate function/map，再查三元组及非零请求身份；零身份字段仅跳过该额外比较。NULLtarget不发布输出，但验证/owner witness分析仍可分配。输入及嵌套存储须同步稳定，不转移源所有权；具体witness按valueId-1，map owner范围按live顺序，两者布局不同。只有当前checkpoint所用live槽取得状态检查，不声称见证每个槽都检查。

普通StateMap拥有四个可独立C-free池；Core AOT owned-module也可把record及四池打包在arena，logicalStateMap只借用模块存储，不能对arena记录调用普通逐池Free。Clone复制used counts和身份不递增epoch；Builder成功可能在generation未变时替换map，旧地址借用仍失效。逻辑ID、boundary标记和owner状态不等于运行时对象、GC root注册或保活。

输出target须已初始化，旧五池独立可释放；target记录及旧池须与request/function/map、被读池及witness分离，diagnostic也须独立。helper不证明allocation provenance、实际地址可读性或全部别名关系。两位独立reviewer提出的target-record分离前提在本批C/H文档与两条contract行中统一明确，精确文字delta由lease记录；这是原调用前提说明，不是新增运行检查。

三个基础池按live/root/owner顺序准备，owner解析可筛除非INITIALIZED根，recipe再准备aggregate/field池。失败释放候选并在独立存储前提下保留旧target；唯一提交点先Free旧五池再赋完整候选，新池交调用方最终Free。成功终止旧池借用，target记录继续有效。参数、phase、身份、witness或分配失败不能混为同一分支。

## 实际测试边界

unknown-phase case只令entry/request phase=-1，实际请求参数早退；仅断言false/INVALID_ARGUMENT及空target两个字段，未到map校验或完整非空结果回滚。最后FunctionClone case只断言成功前提、record/entries地址不同、entryCount相等；释放source后只检查clone.stateMap非NULL，不解引用其内容，不证明全部池/内容的独立或持续可读性。正常局部Free顺序与Unity longjmp跳过剩余局部Free的边界分别说明，空hooks不兜底。

冻结schema R1有一条非源码测试元数据把deoptId误说成不匹配，R2仅更正为两端deoptId均91而resumeId7/701不匹配；正式既有Validate文档已正确，保持不动。M1140 borrowed-across-suspend旧TODO仍归前批，未修fixture或新增信用。39处Materialize直接调用及schema显式类型/成员/宏用法与opaque backend入口均保留证据，未用无grep命中断言死代码。

## 修改前基线与验收

修改前为实际checkout三源，完整raw前缀由lease固定；EOF追加保留所有既有字节及代码行列。最近Validate41修改后实际四目标build0、CTest4/4及直接42/14/8/29共93场景作为既有程序比较基线；它与本批修改前raw仅有已证明的三个尾LF差异。原超时、并发输入变化及native实测raw范围均保留，不能称固定全仓或独立暂存树测试。

本批Clang检查通过：14586个非注释token的种类、拼写、StartOfLine/LeadingSpace及原行列均相同，三个完整原始raw前缀保留。独立实际source/leaf审查ACCEPT，核对20份文档、21行台账、完整91唯一单元、39处Materialize直接调用及1226个有效非空引用锚点。

四目标聚合构建实际exit1，错误来自本批范围外的string/string.c:512：对返回void的ZrCore_HashSet_Init（hash_set.h:464）使用!。62个冻结输入稳定，运行期另有9个外部输入变化；聚合构建失败后未运行测试，不把helper exit0当通过，不使用旧SCCP二进制，也不在本轮修复外部行为。

随后仅对其余三个原有配置目标重新构建，exit0；3项CTest全部通过，直接42/8/29共79个场景的实际结果行逐项等于上述基线对应程序。本轮未执行SCCP的14个场景，不授其修改后通过信用。此限定验证期间62冻结输入稳定，3511个保守输入中另有5个外部输入变化；与前批native的间隔变化及新增文件亦保留实际SHA记录。无新增测试、源码副本或构建配置绕行；不构成四目标、完整套件、固定全仓、独立暂存树或Linux通过声明。

提交门禁为本批三个源码、21行台账、本说明及inventory三个指定行列，共6路径；Root及独立reviewer核实际暂存差异和上述受限验证证据，git diff --cached --check通过后串行提交。提交收据核验路径、blob、parent、He-Jiahui身份及外部索引/工作树保留后才授21项历史交付信用。

## 证据与生命周期

报告根为E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/：Materialize R2、schema R2及clone1提案与各自独审保持不可变；state-map-public-lifecycle-root-proposal-audit-r1.json和source-lease-r1.json记录整合与前提澄清。source/staged/strict/native及commit收据按实际结果补齐。

复用实际checkout和E盘build/cache，无源码副本或新增测试。生命周期open，本批释放0字节；既有清理拒绝、必要失败证据及外部/未知产物保留。
