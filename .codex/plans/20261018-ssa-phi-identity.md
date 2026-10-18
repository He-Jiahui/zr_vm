# Source41 候选：普通 PHI 同源恒等消除

状态：最小RED、六生产源及十一测试源已独审；首次完整20/21失败证据保留。SCCP修正后实际21/21与974/974故障点通过，独立实际审查5A51F4D4接受；当前13C有限MSVC已独审接受。六回归6/6、232/232用例与2350/2350故障场景已独审8EE2FF6D接受。本计划冻结于发布前；提交和清理状态以本批artifacts.json与finite-close.json为准，完整47叶目标继续active。

## 已确认的实际缺口

Root与两位gpt-6-sol只读确认：现有exec_ir_copy_aliases.c只登记COPY别名，重写普通PHI的incoming只消除前驱COPY；live普通p=PHI(a,a)仍被RETURN根化保留。SCCP对合法external值没有常量证明，既有DCE应保留live p。最小目标是同一未知值身份的复制传播，不依赖常量事实或外层固定点循环。

最小候选：合法四块diamond；entry具有external BOOL c与external Int64 a；左右纯branch到merge；p的两ordered incoming均为a；RETURN p。预期当前Core+Parser ALL Verify必须先通过，Optimize成功且postVerify仍有效，唯独RETURN未指向a、logical phi仍存活暴露功能缺口。负对照PHI(a,b)不得改写。第二正例为重复边，必须按occurrence逐slot核；两个相同pred的incoming a/b不得被误认为同源。

## 责任与参考证据

保持GetScalarPasses的SCCP+DCE两阶段合同，不新增第三pass；拟在现有copy alias职责中识别普通PHI源身份，现有DCE负责随后删除无用logical phi。生产预计仅exec_ir_copy_aliases.c；若实际设计需要私有helper，先核职责和文件规模，不改public ABI/manager/常量合同/旧oracle。

Root已读本地JDK cfgnode.cpp:1519–1530、1578–1616：Identity调用unique_input并按control输入检查唯一源；其TOP/self/cast更强规则不直接照搬。Mono mini/ssa.c:569–585逐输入检查相同并转move。JDK TestPhiElimination.java:37–74给出丢失NotNull后破坏Safepoint/OopMap的实际反例，因此typeToken、nullability和身份map必须保真。

## 设计约束

- 同一非self输入；源与结果typeToken/nullability一致、ownership UNKNOWN、无PLACE；不得绕过MOVE/DROP/DROP_IF_INITIALIZED tombstone。
- 显式区分PHI块入口定义与instruction定义，不能把PHI伪装instructionId；证明定义到每个普通use的可达性/支配与同块顺序。external值按真实Core准入处理。
- COPY/PHI链需有界解析；拒绝无根self/mutual cycle、仅backedge提供的源和未经证明的自环PHI(a,p)。不得借鉴参考实现的更强cast/TOP规则。
- 保守拒绝identity-bearing state/gc/deopt/frame布局映射；新路径不修复这些表。区分普通phi和effect/memory token，尤其普通incoming与token range共享backing时不能误改slot。
- 保持ordered CFG、ValueId/InstId、phiIncoming backing/count、source和可观察边界。最小RETURN witness的incoming原字节须保持；若扩展到普通PHI uses，只允许已证明的精确use slot身份替换，其他slot与token共享范围必须保持，禁止全局改写或压缩incoming。只由现有DCE压缩logical phi，不重排边。
- 新scratch/扫描/解析/DOM证明须纳入budget，未完成证明不得发布半成别名。沿现有private candidate与public事务/OOM/remarks协议，不能弱化原测试合同。

## 后续执行门禁

1. [x] 先为稳定任务ssa-phi-identity-01a0fe2b登记E盘reports/artifacts.json，复用现有实际checkout与B工具链配置。无源码副本或跨盘构建。
2. [x] 设计和最小fixture独审；实际Core+Parser ALL合法，真正功能RED经red-review 9AFD3518独审接受。
3. [x] 最小生产实现与独立源码审查；全部正反例采用实际IR构建/Verify/Optimize/Clone，不运行VM/Oracle/ExecBC/FFI/MaterializeState/resume或网络/安全测试。
4. [x] 测试同源/异源、duplicate-edge同源/异源、PHI到COPY链及逆物理顺序、typed/nullability/ownership/tombstone/cycle/map/token边界、disable/repeat、NULL diagnostic、预算逐cutoff及实际逐站FI。
5. [x] 复用同一实际构建目录；新CMake入口先核现有standalone显式扩展图，不修改外来共享CMake。按source/header/tool/env/argv/product精确决定补编/复用，不复制源码或复用身份不符对象。
6. [x] 当前BOOL/Int64/branch/DCE/metadata/GC必要依赖回归与有限MSVC，结果和失败证据如实登记，不外推平台或整叶信用。
7. [ ] 维护模块/验证/叶计划和进度文档；exact owned commit双He身份，完整保留foreign index/raw/cached/dirty/jobs/leases。
8. [ ] 自有进程自然退出；标准明确路径/hash清理临时helpers和私有indexes，登记释放/保留/未知项；仅实际有限完成后增加计数，完整47目标继续active。

设计启动时：实际HEAD68d68cc8、copyalias生产基线8AA2E421、configF2524A6B；新任务清单已登记。当时两Sol设计与Luna预检并行，生产和测试代码尚未授权，native/commit GO=false。以下按实际推进保留历史，不以早期状态覆盖当前租约。


## 独审后的必要设计修正

Root接受design-review A9C7AB98的R1–R4：旧COPY登记一并要求exact typeToken/nullability、UNKNOWN、无PLACE及全局tombstones，避免先擦掉类型事实后新PHI或第二次调用误收敛；保留合法COPY-only能力。旧新operand/incoming写均保护token覆盖和全部共享owner上下文。新scratch与原图proof必须在任何旧写之前完成；direct SCCP已经先发布cache，不虚构完整cache/context失败回滚，manager/public各按真实合同验收。加入正常effects、throwing、staleDOM、双次mismatch/sharedslot实测；测量workUsed须非NULL budget，maxWork=0是unlimited。

最小源码冻结时：四测试源已冻结（实现报告59064D1E），Root新opt-in CMake及一次性finite helper已创建并待独审。当时尚未运行configure/compiler/test，合法fixture与RED尚未实测；生产未授权，不计第41项完成。


## 2026-10-18 最小实际 RED

实际同B configure自然0；252 immutable/376 protected保持，6变更和2新增都在39输出登记内。原configure收据因本来不存在的可选InstallScripts.json收集错误仍为false，独立configure-review B570495A离线接受此次配置，不重跑。

首轮27个编译角色自然0、零诊断；fixture第28角色因缺内部构造API声明及range匿名union初始化告警失败。仅在fixture.c补真实声明头并把7个range改为.start/.count，Root内存逆变换精确重得旧55BC6188，当前A3A9A006。r2复用27个逐source/实际header/tool/env/argv/product核验相等的角色，补编4个角色和一次link均自然0、零诊断。

actual r2 test自然exit1：3case/6profile，全部before/after Core ALL和Parser验证通过；同源PHI保留是唯一预期feature FAIL，distinct和disable两control PASS；1512 attempts/live0/errors0。10行PHI语义协议精确通过；原收据只因Unity真实footer末尾空格与收集器fullmatch不符保持false，交由Sol独审离线判定，不重复运行或改原收据。未执行VM/Oracle/FFI/MaterializeState/resume/网络或安全探测。

native/commit GO=false，自有native进程已自然退出。完整测试与生产仍等待实际RED独审，不计第41项。生产拟拆copy_aliases、private phi_identity和alias_availability共五文件；仅Root opt-in CMake增加实际provider源，不改foreign standalone或旧pooled入口。

## 当前实现租约

RED独审9AFD3518及partial-cache独审45FDB71F已封定。core Sol的五生产文件已按core-implementation A9DD90E8冻结，写入租约撤销，独立Sol源码审查E25A12CB接受；test Sol的完整十一测试源按test-implementation A20817FC冻结并STOP，写入租约撤销，另一Sol独审中。Root维护PHI opt-in CMake、对应文档及已登记的pipeline/leaf/matrix文档。无compiler/runtime/Git授权，当前GO=false、自有native jobs=0，不触碰外来源码/index/jobs。

当前hook CACFE58E经Luna静审D2852E21接受：actual旧compdb的两个COPY alias provider均接入两helper；primary预计Core15+Parser13+test11=39，GC-parser预计Core15+Parser16+test3=34。六consumer十二份TSV刷新最终属性并保持旧schema，须等全部源码冻结后实际配置确认。完成计数仍40，full47 active。

## 完整候选实际配置

configure-r2 plan D712CC1F 经 CA7C0302 静审后精确执行，CMake pid45904 自然退出0；receipt92047861 accepted。D768C344独审核实270immutable/5tools/helper/385protected全部保持；B423→423，仅10个allowlist配置输出内容变，38required均匹配，无新增删除越界。实际153compdb与六consumer十二TSV逐sourcehash/命令/link对象吻合，primary39=15Core+13Parser+11test，GC34=15+16+3。可选InstallScripts不存在且不强求。五生产E25与十一测试BC4均静审接受；补编/完整GREEN/预算FI/六回归/有限MSVC/最终提交清理尚未发生。Root native GO已撤销、processes[]；完成数40/full47 active。

## 完整首跑暴露的下层OOM诊断

实际12补编及39对象链接接受；21组首跑20PASS/1FAIL。284B75A5只接受build prefix；548PROFILE/24rejection/524legal、992次合法调用、W180/245和public上界554均实测，974故障仅971完成。direct writable站1/2/3的其余安全项全1，但diagnostic保留A5，原收据294A8994保持失败。现有SCCP7BF源与HEAD一致，Root未接管foreign dirty；双Sol证实worker合并storage/alloc失败造成错误。最小拆分新源4599，经EDA4静审、反向hash及其余16pin核实；测试oracle不变。新r3仅补SCCP1对象、复用38、重链并full21复验；两个provider的旧SCCP对象均失效。原12C有限MSVC3D114506接受；新增SCCP1C语法尚待。未授41完成或full47信用。


## 修正后完整实际重试

仅SCCP一次补编、38对象复用及39对象重链，三phase自然退出0；收据AAEBAA85 accepted，完整21/21、故障974/974、1556644 attempts/live0/errors0。独立实际审查5A51F4D4接受。新增SCCP的MSVC单C独审2D89B6E0已接受，与原12C合当前13C有限语法证据；B431保持，无新增语法产物，不外推fullMSVC。六回归计划按当前SCCP需4GC补编，不复用旧GC SCCP对象。

## 六项实际回归

807F44BF收据16phase自然0；4fresh/66cache/6links/6CTest，232/232 Unity、2350/2350 FI。独审8EE2FF6D重核1183pins与402protected，B431到433仅允许22changed+2new，无删除未知；历史LastTestsFailed保留、3tmp absent。public有限上界不冒称publicW，非法IR拒绝不计合法优化。原失败证据保留；全47出口仍未授信。
