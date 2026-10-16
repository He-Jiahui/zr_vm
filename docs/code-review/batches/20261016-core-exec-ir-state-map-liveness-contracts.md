---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h
  - tests/parser/test_ssa_state_map_liveness.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_map_liveness.c
doc_type: code-review-batch
---

# ExecIR StateMapLiveness 契约审查 — 2026-10-16

## 完整范围与协作

三文件30规范单元：生产C的12函数，公开H的1结构、5接口及普通guard，测试9用例、loop与main共11函数。29项有详细中文Doxygen，guard无需额外说明；30定义span、23函数体和1485处引用逐项核查。所有单元见 coverage/zr_vm_core_exec_ir_state_map_liveness.tsv，语句块随所在函数记录。公共fixture已有13项不重复计数。

查询依赖匹配的function、ID、构建版本与借用池；Contains仅对空行早退FALSE，非空行直接读对应位，不校验ID、身份或范围。Parser BuildStateMaps先VerifyFunction，再建Liveness，随后建Owner；不能把已建Owner当成Liveness每位调用方的前提。两种活性都沿CFG收敛、分别处理PHI入边，deopt及aggregate VALUE引用均参与；semanticOnly只跳过普通DROP_IF_INITIALIZED操作数。

## 不变量、构建与失败状态

输出拥有四组池，行指针借用其中存储，不得跨Free、重建或并发写入使用。Build先清零输出，调用前必须为空或已Free，不能覆盖仍拥有池的旧对象。非法deopt在零态返回；第一容量溢出可能保留rowBytes等标量而四池为空，不能泛称所有失败都清零；后续分配或第二阶段失败通过Free清理。两遍分析的临时与最终存储所有权分别记录，无输入所有权转移。

OwnerAt的UNKNOWN、COUNT和CONDITIONAL有不同含义。singleton INITIALIZED与UNKNOWN优先；空mask或semantic-live等路径返回COUNT；cleanup-only允许的UNIQUE/SHARED对象中singleton MOVED、DROPPED、UNINITIALIZED可返回该具体状态，其余合并为CONDITIONAL。OwnerStateAt对所有singleton状态作直接映射，不能漏掉枚举中位于DROPPED与CONDITIONAL之间的UNINITIALIZED。terminator/may-throw结果在after-only且semantic-dead时合入UNINITIALIZED，体现异常路径尚未产生结果。

## 测试证据

完整读9个现有用例的builder、验证、map与断言：回边保活/借用挂起拒绝、兄弟分支、PHI入边、非线性定义顺序、异常后继、deopt checkpoint、高ID反向布局以及空函数。所有9项均建Liveness；验证和物化调用按实际case区分，其中7例共16处assert_only_root调用进行有限根物化检查，borrowed拒绝与empty不冒称物化。loop只构图，fixture统一释放function；局部target与临时分析各由实际case负责，不把tearDown当成任意局部资源清理保障。

本批无已证新BUG或待核实TODO，不更改实现、签名、测试断言、构建配置或行为。普通guard保留无需注释的台账理由；未加注释不等于未审查。

## 基线与验收

修改前复用Objects137实际四目标运行中原始三文件的Liveness9：三文件raw与compile-input观察哈希逐项匹配，运行前后观察输入稳定；额外frozen集合仅含生产C，H与测试C属于稳定观察输入，build/CTest/direct均成功，记录完整9个PASS行。该基线没有更早9例比较，也不把其余92例重复计给本批。实际Clang严格等价检查通过：三源3780个非注释token的kind、spelling、StartOfLine、LeadingSpace及原行列一致，原程序prefix逐字保留。修改后既有Liveness目标build退出0，CTest1/1通过，direct9为9 Tests/0 Failures/0 Ignored，完整9条PASS输出与原始基线逐例相同。当次82项frozen及3552项保守观察输入在运行中稳定；与最近Deopt原始运行相比4项变化、8项新增，与原始Liveness9运行相比18项变化、17项新增均登记，不宣称期间整个checkout不变。仅更新本说明验证段，测试过的三源不变。实际source/leaf独审、暂存独审与串行提交仍待完成；Windows配置不外推Linux、默认SDK、整个仓库或暂存树。 另一个Deopt目标在本批写源前已实际运行：build0、CTest8/direct1，28例27通过1失败；末case在VerifyFunction前置断言535失败，后续预期Build拒绝检查未达，证据另登记且随Deopt模块标记，不归因本批注释。

## 产物与边界

仅三源码、leaf、批次文档及inventory三行审核列，共6路径。沿用登记的E盘build/cache/reports/tmp，无源码副本及新增测试。生命周期open，释放0B；共享/外部/未知归属、Git和已有失败证据保留。
