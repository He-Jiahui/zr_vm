---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
  - tests/parser/test_ssa_phi_identity.c
  - tests/parser/support/ssa_phi_identity_fixture.h
  - tests/parser/support/ssa_phi_identity_fixture.c
  - tests/parser/support/ssa_phi_identity_faults.c
  - tests/parser/support/ssa_phi_identity_support.h
  - tests/parser/support/ssa_phi_identity_builders.c
  - tests/parser/support/ssa_phi_identity_edge_cases.c
  - tests/parser/support/ssa_phi_identity_metadata.c
  - tests/parser/support/ssa_phi_identity_oracle.c
  - tests/parser/support/ssa_phi_identity_budget.c
  - tests/parser/support/ssa_phi_identity_transactions.c
  - tests/cmake/ssa-phi-identity.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
plan_sources:
  - .codex/plans/20261018-ssa-phi-identity.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_phi_identity.c
  - tests/parser/support/ssa_phi_identity_fixture.h
  - tests/parser/support/ssa_phi_identity_fixture.c
  - tests/parser/support/ssa_phi_identity_faults.c
  - tests/parser/support/ssa_phi_identity_support.h
  - tests/parser/support/ssa_phi_identity_builders.c
  - tests/parser/support/ssa_phi_identity_edge_cases.c
  - tests/parser/support/ssa_phi_identity_metadata.c
  - tests/parser/support/ssa_phi_identity_oracle.c
  - tests/parser/support/ssa_phi_identity_budget.c
  - tests/parser/support/ssa_phi_identity_transactions.c
  - tests/cmake/ssa-phi-identity.cmake
doc_type: testing-guide
module: parser
---

# PHI 同源身份传播验证

## 当前状态

实际最小RED已接受。完整21组首跑20组通过，唯一失败为direct SCCP早期三处
OOM未写新诊断；实际收据保持失败。测试两处类型修正与SCCP最小诊断修正
均已独审，测试oracle不变。当前13C有限MSVC语法已独审接受；完整矩阵重试
已自然退出0，21/21组及974/974故障点通过，经5A51F4D4独审接受。六项依赖
回归6/6、232/232个Unity用例通过，8EE2FF6D独立实际审查接受。提交和产物
处置由本批artifacts.json及finite-close.json记录；不外推整叶或完整47叶验收。
[模块合同](../instruction-generation/execir-phi-identity.md)说明目标及保守边界。

所有验证直接使用实际checkout与唯一复用构建目录：

- 源码：`E:/Git/zr_vm`，main，不创建源码副本、snapshot、tar或worktree。
- 构建：`E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-execir-owner`。
- 清单：`E:/cargo-targets/zr_vm/reports/ssa-phi-identity-01a0fe2b/artifacts.json`。
- 自有临时文件：`E:/cargo-targets/zr_vm/tmp/ssa-phi-identity-01a0fe2b`。

有限执行只构造IR并调用Verify、Optimize、Clone、analysis、BuildStateMaps及释放API。
不运行VM、Oracle、ExecBC、SourceExecute、FunctionExecute、FFI、MaterializeState、
resume、网络或信息安全探测。回归须检查实际入口和允许的IR模式。

## 实际最小 RED

四块 diamond 使用真实 Module/Function 构造 API、external BOOL 条件和 external
Int64 来源，ordered CFG 和 incoming 相互一致。每案分别使用 writable diagnostic
和 NULL diagnostic，完整 owner 处置后才进入 Unity 断言。

| ID | 实际输入 | 基线结果 | 合法性与安全 |
| --- | --- | --- | --- |
| 1 | `PHI(a,a)`，RETURN p | RETURN 仍为 3，logical PHI 为 1；所需 RETURN 2、PHI 0 未实现，feature FAIL。 | 两配置 before/after Core ALL、Parser、CFG/slot/map/owner 均 1。 |
| 2 | `PHI(a,b)`，RETURN p | RETURN 4、PHI 1 保持，control PASS。 | 两配置全部安全检查为 1。 |
| 3 | `PHI(a,a)`，禁 SCCP | RETURN 3、PHI 1 保持，control PASS。 | 两配置全部安全检查为 1。 |

actual test 自然 exit 1：3 Tests、1 Failure、0 Ignored；1512 allocation attempts，
live/errors 均 0。精确十行 `PHI_IDENTITY_` 协议已获收集器接受；同源转换缺失
是唯一失败，不能把安全检查失败当作功能 RED。

### 构建与收据

同 B 的 configure 自然 exit 0。252 immutable、376 protected 保持，6 个修改和
2 个新增文件均在登记的 39 输出范围内。原 configure 收据因收集本来不存在的
可选 `InstallScripts.json` 保持 false；独审 `configure-review.json`（B570495A）
离线接受此次配置，未重复 configure。

首轮 27 个编译角色自然 0、零诊断；fixture 的第 28 角色因缺内部构造 API 声明
头及匿名 union range 初始化告警失败。r3 仅补真实声明头和 7 个指定字段初始化，
数值及全部 oracle 未变；Root 内存逆变换精确重得旧 fixture 哈希 55BC6188，
修正后 fixture 为 A3A9A006，无源码副本落盘。

r2 按实际 source、observed header、tool、environment、argv、product pins 复用
上述 27 角色，补编 4 角色和一次实际 link 均自然 0、零诊断。link 的 exe/lib/pdb/map
均有实际收据。初始失败收据不改写为成功。

r2 test 原收据仍为 false：Unity 真实 footer 的末尾空格不符合收集器的严格
fullmatch。该空格由本地 Unity 格式确认；十行语义协议、唯一 Unity feature
failure、两个 PASS、实际退出码和全部安全检查均通过独立核对。
`red-review.json`（9AFD3518）离线接受真实最小 RED；这不授予完整矩阵或完成信用。
partial-cache 独审 `red-build-review.json`（45FDB71F）只接受首轮 27 角色的复用，
不把后续 r2 产物归入首轮失败运行。

## 完整矩阵的验收安排

以下列出实现和验证目标。完整首跑的实际覆盖与失败单列在后文；不能把静态循环数当作实测通过。
合法语义 profile 必须先通过真实 owned-ledger、Core ALL 和 Parser 准入；非法
SSA profile 单列 rejection，不计作保守优化通过。

冻结实现报告 A20817FC 列出 21 组、548 个静态 profile rows，其中 24 个为明确的
非法 SSA 拒绝用例、524 个为待准入的合法语义配置。重复调用计入后，计划中的
合法优化调用为 992 次；预算和 FI 循环另计。这些是源码循环清单；独立实际统计结果见下文。

| IDs | 目标与主要 oracle |
| --- | --- |
| 1–4 | 保留原三案；重复边 a/a 与 a/b 按 occurrence 检查。 |
| 5–7 | COPY/PHI 链、逆物理顺序、三类合法根和两个函数局部 ID 隔离。 |
| 8 | 每条 COPY/PHI edge 的 type/nullability/UNKNOWN/place 正反配对，连续调用三次。 |
| 9 | self、互递归及混合环拒绝；有根逆序链正例。 |
| 10 | MOVE/DROP/guarded DROP 的全局消费 tombstone，包含定义之后的消费。 |
| 11 | state/gc/deopt/aggregateFields/frame 全 metadata 冻结，source-map-only 正例。 |
| 12–13 | active ordinary/token domain、真实 backing overlap、instruction phiRange、shared operand/incoming owner。 |
| 14–15 | 禁 DCE、全部禁用、direct/manager/public 重复调用和准确 changed/remark。 |
| 16–17 | private 实测 W 的逐边界和 instruction budget；public 有来源的有限上界。 |
| 18 | direct SCCP 每个实际 allocation site 的 FI、真实命中、清理及 retry。 |
| 19 | manager 每站 FI、完整 sentinel remark prefix、按实际合同回滚 function/cache/context。 |
| 20 | public 两函数逐站 FI，覆盖第二函数 late hit，失败原模块保持且 result 清零。 |
| 21 | 无 PHI、distinct-only、无候选不产生虚假 changed。 |

normal effect/memory/CALL 前驱须有允许 identity 的正例。真实 DOM 与零或 stale
idom 配对，throwing normal-edge 正例、exceptional closure 保守例和非法 SSA
rejection 分开，不能用禁用全部 throwing 根代替可用性证明。

workUsed 测量使用非 NULL budget，maxWork=0 表示无限制；budget=NULL 不提供
实测工作量。public 的有限显式上界不能称为 measured public W。FI 必须记录
健康路径实测站点、每站实际命中及 owner 释放；direct SCCP 可能已经发布 cache，
不得对其伪造完整 cache/context FALSE 回滚要求。

## 完整矩阵首跑与修正

`green-r2-receipt.json`（294A8994）整体保持false。12次补编与一次39对象链接
均自然退出0、零诊断；独审`green-review.json`（284B75A5）仅接受构建前缀。
实际B423变为431；7个允许的既有文件变化、8个允许的新对象，415个保护文件不变。

独立解析实际stdout得到548个PROFILE行，其中24个以REJECTION join明确排除；
524个合法profile的前后Core ALL/Parser检查全部通过。986条FIXEDPOINT加6个
minimal调用共992次合法语义优化调用。私有direct实测W180、manager W245，
四组逐cutoff及instruction budget3/4/5实际覆盖；public显式上界554的两组序列
完整，554来源为`2*privateW+64`，不是public实测W。

六组分配故障的实际N为13、132、342各两组，共974站，完成971站。唯独direct、
writable诊断的站1/2/3在FALSE后仍留`0xA5A5A5A5`；其余命中、trace、owner、
IR、cache/context、remark/result、Verify、retry和释放字段均为1。manager remark
第86站与public remark第134站均真实命中；public首函数prefix180、late145。
21组因此为20PASS/1FAIL，1556644 attempts，结束live/errors均0。不得把该次运行
改为GREEN，也不得把3个诊断失败站计作完成。

两处测试C4389仅把布尔比较结果转为真实字段类型`TZrUInt32`，独审6581A54B
证实真值与边界保持；其它15源码不变。初轮有限MSVC的4个接受phase加8个重试
phase覆盖当前12C，独审3D114506确认诊断0、97个实际头和B423不变。
原有警告收据仍失败，未改写历史。

真实缺陷位于既有SCCP worker的早期三个calloc失败：只有旧诊断为NONE才写OOM。
修正源4599C61E经EDA4E6D4增量静审接受：先保留具体storage失败，再对alloc失败
无条件写OOM，原三个free、分配次序和预算不变。其它16个冻结源保持；direct
OOM断言原样保留。该SCCP源的两个provider对象均须重编，消费程序须重链。
当前主图仅SCCP对象失效，其余38个对象按完整输入证据复用；修正后的实际运行
结果另行登记，不继承原失败运行的通过信用。

## 当前完整矩阵重试与新增语法检查

仅补编当前SCCP，复用38个逐输入核验的对象，按原39对象顺序重新链接；
完整21组测试协议与oracle不变。实际`green-r3-receipt.json`（AAEBAA85）三phase
均自然退出0，full21/21、fault974/974、1556644 attempts/live0/errors0；
独审`green-r3-review.json`（5A51F4D4）接受本次实际结果：524合法profile与
24非法rejection分别核对，992合法语义调用；私有W180/245的每个cutoff、public
有限上界554的两序列均完整。六FI组N为13/132/342各两次；manager remark站86、
public remark站134、public prefix180及late hit145均实际命中并保持真实事务合同。
直接诊断失败的原三站本次code22/token50331739/safety1；不继承首跑的失败信用。

新增`msvc-sccp-review.json`（2D89B6E0）独立接受当前SCCP单C的`/W4 /Zs`：
自然退出0、零诊断、87实际头、295输入及B431全部保持、无编译产品。
与原3D114506的12C桥接后为当前13C有限语法证据，实际头并集99；
不授予full MSVC或runtime范围。

## 六项实际依赖回归

`regression-receipt.json`（807F44BF）16个阶段均自然退出0：4个GC对象补编、
6次实际链接和6项严格单项CTest。复用66个逐输入核验的对象；其中28个环境
精确相同，38个历史对象仅采用已独审的TEMP/TMP/TMPDIR路由差异，不声称
完整环境相同或重新编译字节相同。链接对象数依次为33/33/33/33/31/34。

| CTest | Unity通过/总数 |
| --- | --- |
| ssa_sccp_pooled_bool | 22/22 |
| ssa_sccp_pooled_int64 | 53/53 |
| ssa_sccp_branch_sensitive_pool | 22/22 |
| ssa_dce_dead_phis | 21/21 |
| ssa_pass_manager_scalar_metadata | 15/15 |
| ssa_gc_map_parser_consumers | 99/99 |

独审`regression-review.json`（8EE2FF6D）重核1183个当前pin、实际协议和全部
自然退出，接受232/232个用例与2350/2350个FI场景（728/230/782/610）。
本次私有W为37/51/326/85，各组逐cutoff连续；public有限上界138/46/202
不能称为public实测W，branch只授maxWork1的有限见证。结束live/errors均0。
GC的78个非法拒绝与合法优化分列；不把畸形IR拒绝计作合法优化。

实际B由431变433文件，22个既有文件变更、2个新增均在登记范围，无删除或
未知变更，402个保护文件保持。三个CTest临时兄弟路径均不存在；历史
LastTestsFailed.log的35字节保留，不能据其历史内容判为本轮失败。

## 配置与产物处置

生产、完整测试及 Root CMake 已冻结并独审接受；随后以当前 source/header/tool/env/
argv/product 身份决定重编与复用。实际 configure-r2 自然退出 0，独审
`configure-r2-review.json`（D768C344）核实主图为 Core15、Parser13、测试11，
共39角色。该数量是已配置的编译角色，不是已编译成功数。

实际旧 compdb 有两个 COPY alias 编译 provider。Root hook 分别给 pooled
scalar 和 GC-parser 私有 target 增加两个生产 helper，后者实际配置为
34角色（Core15、Parser16、测试3）。六个消费者的十二份 TSV 从最终 target 属性
刷新，Int64 和 GC 的七个设置前缀及原排序保持。静审
`cmake-source-graph-review-r2.json`（D2852E21）接受当前 hook CACFE58E；
实际153条compdb、十二份TSV的当次配置source hash、配置命令及链接对象已逐项核验。
后续fixture的两处cast和SCCP诊断分支仅改函数体，实际配置命令不变；TSV保留
配置时的历史source hash，不能冒称当前hash。当前源身份由新的source-only body
bridge和实际补编证据绑定，不据这些函数体修改声称重新配置。
423个既有构建文件仅10个允许的配置输出内容变化；385个 protected 文件保持，
无新增、删除或越界变化。38个必需输出与收据一致，可选 InstallScripts 仍不存在。

必要依赖回归为 BOOL、Int64、branch、DCE、metadata 与 GC-parser 的允许 IR 模式；
有限 MSVC 仅授实际 `/W4 /Zs` 的语法范围，不外推 full MSVC、Linux 或 full W0。
完整生产功能、验证、文档、精确 owned 双 He 提交及自有清理完成后才增加计数。

自有 helper 和无效私有 index 按明确路径与哈希计划清理；成功、失败证据、当前
复用构建、共享依赖和未知归属保持。未知 Parent1/B11 标签和其它会话产物仍为
needs-review，不能据本批有限清理声称全局 closed。实际释放字节及保留清单以
本批最终cleanup收据为准；具体提交与有限收口记录见同根finite-close.json。
