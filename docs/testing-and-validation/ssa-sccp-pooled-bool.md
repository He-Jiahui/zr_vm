---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - tests/parser/test_ssa_sccp_pooled_bool.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.h
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/cmake/ssa-sccp-pooled-bool.cmake
doc_type: testing-guide
module: parser
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
tests:
  - tests/parser/test_ssa_sccp_pooled_bool.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.h
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/cmake/ssa-sccp-pooled-bool.cmake
plan_sources:
  - .codex/plans/20261018-ssa-scalar-completion.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
---

# 非空池 BOOL 比较常量写回验证

## 当前状态

Source40的实现、focused、五项必要回归与有限MSVC已实际通过。功能已提交为`c6847c3a093245b554d34672dd0440ddc949b4e1`，精确16路径、作者与提交者均为He-Jiahui；postcommit独审B1FD0DFA接受。7415条范围外index完整raw及4条foreign cached保持，当前整index哈希与历史发布值不同的限制已在报告说明。自有清理及保留构建以本批清单收据为准。最小实际RED使用真实Module构建、Core与Parser Verify及public Optimize：2案4profiles，要求写回EQ(40,40)的用例因缺失变换FAIL，disabled SCCP对照PASS；全部preverify/run/postverify/safety均1，634 allocation attempts、live/errors0。自然exit1是一个已登记Unity功能失败。生产三文件修改后，完整focused CTest实际22案、84固定profiles全部PASS，focused独审A9214238与有限MSVC独审AC90E867均已接受；五项必要回归亦全部实际PASS，详见下表。

构建复用实际checkout和同一物理E盘目录；31个C单元全部fresh编译、1link、零诊断。初次configure检查因无关foreign文件变化而在子进程启动前拒绝；第一轮compile01自然0但collector漏列真实exec_ir_opcode.def依赖而拒绝。两个原失败收据保持失败；修正范围与dependency声明后实际r2 configure、31fresh build、最小RED分别收据接受，未重写旧失败证据。

## 本批合同

- 复用唯一private scalar materializer，按(typeToken, flags0, bits)精确去重；BOOL只允许bits0/1，Int64同bits不能代替BOOL。
- 六种COMPARE的两个实际来源须为严格canonical Int64，结果与事实为BOOL。UNKNOWN、retagged或不支持来源不得获得证明。COMPARE.typeToken是谓词，改成CONSTANT必须显式写BOOL。
- 新BOOL写回保留可观察flags、effect/memory、deopt/binding、match和控制payload边界。指令、result/value、source和CFG身份保持。
- pool扫描逐tuple1work，复用预付rewrite1work，append预付append+rewrite2work；ready后立即执行无失败写回。预算partial TRUE保持有效IR，无孤立append；private FALSE丢弃candidate，public FALSE保持原owner与内容，清零result并恢复remarks前缀。
- 原Int64 instruction token0、空池/NULL owner数值路径保持；不因单轮调用结构添加未证明必要的外层固定点循环。

## 历史测试合同迁移

Source37 case32保留ID及非Int64要求，预期改为精确BOOL tuple。Source38原CFG witness对COMPARE形态的冻结须精确改成合法纯COMPARE到正确BOOL tuple的证明；保留ordered CFG/PHI、其他COMPARE、DIV/effect以及所有故障和预算安全断言，不删除整个witness。旧验收收据仍表示旧实现对应的历史合同。

## 实际 focused 验证

实际GREEN-r2收据E88693D1、Root语义验收F3C5E605：22案/84固定profiles（28 IR、56 advanced），全部Core/Parser前后Verify、精确tuple与身份、重复运行检查通过。覆盖六谓词和signed极值、BOOL0/1首个精确tuple、跨类型/flags碰撞、池增长与双函数复用、external/非Int64来源保留、禁用选项、cache/remark与source map。

八组private预算均实测W=37，逐一运行1..W共296个截断点。public双函数预算按明确有限上界2*privateW+64=138运行两种diagnostic配置，共276点；138不是实测public工作量。每个partial结果均逐函数匹配原COMPARE或正确BOOL CONSTANT，保留prefix/RETURN/result/source且无孤立append。

四个健康trace实测N为125/125/248/248，746/746实际分配故障均触发、拒绝、保持原owner与内容、清零result、保留完整remark前缀并成功重试。双函数真实published pool record确认增长站83，第一函数prefix131后有第二函数失败，remark增长站96（单函数为78）；不依赖realloc是否搬家。459698次分配attempts，live/errors均0。

生产与测试共31个唯一C单元取得当前实际编译证据：首轮前27个accepted、自然0且诊断0，第28个fixture因枚举/unsigned比较warning拒绝；参数改为对应enum后，r2为4fresh编译、27个逐source/actual header/tool/env/argv/product核对的精确复用、1link及1个anchored CTest，诊断0。首轮整体失败收据90F8FE63保持失败。初次测试静审发现的四处oracle/覆盖缺口及后续公共BOOL Value类型补齐均保留修订记录，没有升级旧失败结果。

有限MSVC语法验证覆盖8个当前C文件：首轮前5个syntax自然0/W0；旧Int64 main的Unity计数窄化警告改为真实UNITY_UINT后，r2的剩余3个syntax自然0/W0。98个实际header并集，233个已知构建产品保持；CF63D99D为实际r2收据，旧81D87AC1整体失败保留。有限MSVC独审AC90E867接受这8个当前C的 /W4 /Zs 语法证据，不包含对象、链接、运行时或完整MSVC/W0信用。

合法COMPARE元数据边界逐项实际preverify：deopt和真实value池、LEGACY opaque binding、物理phiIncoming/successorRange及untagged memoryIn/Out。非TYPE_TEST的matchTypeToken、typed非CALL/INVOKE的bindingRow等被Core拒绝的结构没有冒充合法输入；物理successor payload不等同于新增CFG边。

## 回归与交付范围

focused实际证据独审A9214238与有限MSVC独审AC90E867已经接受。五项必要IR回归的实际r2收据0DC200AA、Root协议验收A192C7D2与独审CA091A58均接受，总计210个Unity用例、零失败：

| 回归 | 实际验收范围 |
| --- | --- |
| pooled Int64 | 53案/113 profiles、94 fold；4组W51/204 private cutoffs、public显式上界46及1个instruction-limit case；239故障、117024 attempts/live0/errors0 |
| branch-sensitive | 22案/47 profiles、39 fold；4组W113/452 cutoffs；N125/125/248/248共746故障、388768 attempts/live0/errors0 |
| Source39 DCE | 21案/140 profiles、120语义IR；8组W85/680 cutoffs、public显式202×2=404；6个trace共610故障、532777 attempts/live0/errors0 |
| metadata | 安全wrapper精确15个RUN_TEST及marker；不调用included旧main或MaterializeState |
| GC-map parser | 99案；21个独立合法输入、78个fresh合法baseline与78个畸形拒绝，失败输入无分配并保留内容 |

回归只新编译35个不同role对象，另26个provider对象按实际accepted compile phase、source/header/tool/env/argv/product精确复用；5次实际link、5个anchored CTest。首次link自然0、诊断0，但旧collector因正常lld /verbose stderr而拒绝；逐377行及31对象/16个before-pinned库复核后有限link01离线接受43FC6D28，remaining4 link与5 CTest通过r2。原06CFF794整体失败保持，没有重复35个已通过编译。DCE private pipeline失败允许通过替换owned heaps恢复同一逻辑IR；public/direct故障仍要求原owner身份。文档与回归独立复核、精确提交和自有清理收据均在本批证据根登记。

证据根：E:/cargo-targets/zr_vm/reports/ssa-scalar-completion-01a0fe2b。当前RED receipt 034255E2，31fresh build E2FACC46。源代码直接修改，不生成副本；本批只运行IR构建/Verify/Optimize/Clone/BuildStateMaps，未运行VM/Oracle/ExecBC/FFI/MaterializeState/恢复或网络安全能力。Linux、完整MSVC、fullW0及整叶02.01/47退出仍须另验。
