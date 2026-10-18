---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - .codex/plans/20261017-ssa-scalar-pipeline.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
doc_type: testing-guide
---

# Rooted ordinary PHI DCE：Source39 验证记录

## 当前状态

实际 RED、focused GREEN、三项适用回归及四C有限 MSVC 均已验收；实现和测试
通过对侧 Sol 静态审查，GREEN 与回归原始协议通过 Luna 独审。提交与自有清理
仍待完成，本批暂不计入已提交38项。只有下面登记的实际结果具有运行信用。

## 实现合同

`exec_ir_dce.c` 从可观察指令及现有 GC/deopt/aggregate/state-map 引用根化
普通值。仅 live PHI 结果传播 incoming；单调标记保留需要的生产者并删除
无根的 self/mutual cycle。Legacy PHI opcode 保持可观察，普通结果与 incoming
保守存活。效果、owner、GC/throw/debug/suspend 等边界不因结果未使用而删除。

只按原物理顺序压缩逻辑 `phiPool`，以旧位置 prefix 重映射全部 block.phis；
逆 block 顺序与 inactive hole 同样适用。`phiIncoming` 指针、数量、容量和 bytes
以及 surviving ordinary/effect/memory/legacy ranges 保持；不会释放 incoming backing。
Value/Instruction/CFG 身份、边顺序及重复 predecessor occurrence 保持。

同一 incoming 池里的普通值和 token 按消费者各自验证。普通 PHI/legacy PHI
即使与 token range 别名，也必须符合1..valueCount。Token 可超过 valueCount，
不索引普通 used 数组；Core 允许的初始零 token 按真实 terminal 合同保留。
该 storage-domain 分类没有取代 Core 的 effect/memory 语义 verifier。

五项 scratch、范围和元数据验证、标记、删除决定与全部发布 work 预付均在
首次写 IR 之前完成。预算耗尽成功返回但 changed=false，IR/owner/metadata 不变；
分配失败在发布前返回 false。真正修改 PHI 的批次 changed=true；再次无变化
必须 false。普通 manager 仍采用 invalidatesAnalysis=ALL，重复 NOOP 也可以增加
cache revision；本批未改变 manager/Core/公开头/宏/state-map 逻辑。

## 测试矩阵

| 范围 | 断言 |
| --- | --- |
| 15个语义 case、120 profiles | 每例实际构造后 Core/Parser Verify，运行后 Verify、IR 期望和身份稳定 |
| 纯 PHI 删除 | case5 profile3 的两块、两个平行 successor 槽与 external 输入；只删 dead PHI，指令 bytes 全保持 |
| 根与效果 | live return、互递归 live root、GC/deopt/aggregate/state roots、CALL/DIV/flags/MOVE/DROP 保留 |
| 共享 incoming | token 大于 valueCount、普通/token alias、重复前驱、逆物理排列及 hole |
| work | direct/manager 各自实测 W，fresh 1..W；public 使用明确有限上界，不伪称精确 public W |
| 内存分配失败 | 按实际健康 trace 的每个站点注入一次失败；验证内容/owner/cache/context/remarks/result 与重试 |
| 模块事务 | 真实 first-function prefix、late-function OOM 与 remark realloc 失败 |
| 固定点 | mixed 与纯 PHI 两形状，各3 entry×2 diag，每 profile 连续3次；真实 bool/remarks/cache/public counter |
| 生命周期 | 释放后 ledger live/errors=0，再输出结果；禁止以失效指针继续 hash/Verify |

默认21个 Unity case、140 profiles；`--semantic` 为15 case/120 profiles；
原 `--red` 两 case/8 profiles 的输入与断言保持。main、Unity、allocator 使用
普通 CRT；fixture/faults 与生产 providers 接入显式分配 shim。测试仅调用
IR Optimize/Verify/Clone/analysis/state-map build，不进入执行后端。

## 实际 RED 与前置纠正

最初编译有两条 fixture 缩进警告，按零警告门停止；排版修复不改语义。
第一次 runtime 还未到 DCE：fixture 提前设置 entryBlockId，导致 Core
AddBlock(ENTRY) 拒绝。第二处是纯 CONSTANT 被虚构 effect token/PHI，实际
Core 只按 opcode schema 识别 effect definition。两类错误精确修复，原失败
收据保留，均未当作功能 RED。

最终 baseline r4 只编译修复 fixture 并按实际配置链接，零诊断、自然 exit0。
最小 RED8 均 preVerify/run/postVerify/stable/safety=1：死 PHI case1 仅因
缺失变换 FAIL，live PHI case7 PASS。原116个语义 profile 均前后 Verify 有效，
36个健康控制、68个运行成功但缺变换、12个合法 token profile 被旧 DCE 拒绝
（case6:8、case11:4）。Ledger live/errors=0，无 UBSan 诊断。

- `red-semantic-review.json`：3195B，SHA `4DB68EA99BC50639AA2D22008B92E9484D3F18E1ECB8905CF9A51404EF63E3AC`。
- 实际 `red-test-r3-receipt.json`：6603B，SHA `EC99EF3C715A3B5B286ED2F0367478EDA41BE79C616A6E638E9DC29EF3F5215B`。
- 两位独审分别重算 raw 分类并接受；旧非法 fixture 的失败没有升级。

## GREEN 与适用回归

Focused 构建为4次受影响源码编译（DCE/main/fixture/faults）及1次配置中的
链接，27对象按实际 source/object/headers/argv/tool/env 精确复用。全部自然
exit0、warning/error0。主图仍为31 TU，未重建源码副本或改共享 CMake。

默认完整测试自然 exit0：21个 Unity case 全 PASS、140 profiles 全部
safety/feature1，120个语义 profile preVerify/run/postVerify/stable/expected
均1。12组 CHANGE 与12组 FIXEDPOINT 实际观察全部通过；empty case21 的
first_expected=0 与其余真正修改时 first_expected=1 分别成立。

两项预算 case 各四组分别测 W=85，共8组 fresh 1..85、680个边界。
Public 模块使用明确有限上界202，在NULL/writable diag各扫1..202，共404个
边界；202不是实测 public W。所有返回成功的中断仍有有效 IR，direct 的
中断保持内容/owner不变、changed=false，预算充足时执行期望删除。

按真实健康 trace 的5/5（direct）、73/73（manager）、227/227（public）
分配站点逐一注入，共610/610完成，包含真实 remark realloc 与 late-function
失败后重试。Public first-function prefix=123；manager允许clone替换owner但
内容/cache/context/remarks恢复，public/direct原owner保持。532777 allocation
attempts，最终 live/errors=0，stderr空，无UBSan诊断。

- `green-build-receipt.json`：1653535B，SHA `32EA4733005638F32CB5BA02B8469D2E6F2FC2E9F44F85AB51DD39B7281E928E`。
- `green-test-receipt.json`：7193B，SHA `DB41CDC03C8CE9670400CFF7946556AD7C2EE69D23AE35A6D80D63DB618DD934`。
- Root `green-semantic-review.json`：3492B，SHA `F9F1A3F19C7557C17F8C593A51022E3EFF618F4F9B48F565E7C376E5FA523BE1`。

三项当前回归：18 fresh compile、33 exact cache、3 link、3 anchored CTest，
共24个 phase 自然 exit0、诊断0。强SCCP53 case/113 profiles、94 folds，四组
fresh W51共204预算边界，239个实际故障站完成，117024attempts/live0/errors0；
metadata15排除MaterializeState，GC-parser99全PASS。GC日志为99 LEGAL
（21 standalone加78 malformed输入的fresh baseline）与78 MALFORMED有序配对。
合法消费者可以分配，malformed changed按消费者不同；preserved必须1。
三个FI case分别覆盖1..178、prefix+1..remark+1即37..96、独立remark站95，
不能把239总数误称为三个各自完整1..178。Root和Luna独立核对实际协议接受。

MSVC最终四C均以 /Zs /W4 语法检查自然exit0，94个实际header并集和228个
受保护产品针保持、warning/error0，没有MSVC链接或程序执行信用。首次DCE
语法本身自然0，但旧采集器不识别实际中文“注意: 包含文件:”而拒绝；旧失败
receipt/raw保留。仅修正双语include/diagnostic识别，经精确逆变换和实际85条
header重放审查后，四C r2实际成功，不把旧失败收据直接改成通过。

- GREEN独审 `green-independent-review.json`：16943B，SHA `2F818FD5A6CE2670186C7544EF64B805514A012438A3C6A527A764A9C93B3BA5`。
- 实际回归 `regression-receipt.json`：2528194B，SHA `941730EA3D906A178550519F40D6FD4B9BA91A1D9DFDC71CDE11644A023131CA`。
- 回归独审 `regression-semantic-review.json`：16033B，SHA `FC3716B2F5C906391179159ED225E138E679B509D2F68FB3B3669C9EF8585FE4`。
- MSVC实际 `msvc-r2-receipt.json`：92476B，SHA `78245A354FA35D0CF17A01CF39A7F2324885EA81AD20878BCFB21B61E9A5E9D2`。
- MSVC Root验收 `msvc-root-acceptance.json`：20416B，SHA `E9DC9F41124A176757F8B4BE83BA62767C0E2AE9636865701B10C675DACC36BF`。


## 编译、执行与范围

实际 checkout 为 `E:/Git/zr_vm`；复用
`E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-execir-owner`。
新 CMake module 通过 `CMAKE_PROJECT_INCLUDE` 在顶层项目结束时接入，不修改
共享 standalone CMake 的外来工作区变更。主图31 TU=Core15+Parser11+test5，
使用 Clang-cl/UBSan 与物理 lld、显式 SDK 只读依赖；不使用 VS/bootstrap 或
vcpkg_ext。缓存只在 source/object/实际 headers/argv/tool/env 对上时复用。

当前证据不包含 VM/Oracle/ExecBC/SourceExecute/FunctionExecute、FFI、网络、
安全能力探测、MaterializeState 或 resume 执行；也不包含 Linux/fullMSVC/fullW0，
不签收整个02.01或47叶。此前已拒绝的 WSL/全局进程/ignore 路线未重试。

## 提交与产物

本批尚未提交。产物先登记在
`E:/cargo-targets/zr_vm/reports/ssa-scalar-pipeline-01a0fe2b/artifacts.json`，
reports 保留实际成功和失败依据，当前构建复用；tmp 自有 helper 在终态后按
明确路径/hash 清理。当前实际释放0B，Parent1/B11/共享 cache/未知归属原位保留，
全局 cleanup 未关闭。未创建源码副本、snapshot、worktree、source.tar 或跨盘镜像。
