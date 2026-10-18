---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
  - tests/core/test_gc_root_leases.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/frame_slot_layout_initialization_tests.inc
  - tests/core/test_ssa_platform_matrix.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
plan_sources:
  - "user: 2026-10-13 全仓库首方代码审查与注释方案"
tests:
  - tests/core/test_gc_root_leases.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/frame_slot_layout_initialization_tests.inc
  - tests/core/test_ssa_platform_matrix.c
doc_type: module-detail
---

# Common 公共类型与基础宏契约审查

本批全量审查 zr_common_conf.h 的60个单元：33个宏、26个类型和1个有意义的预处理块。59个旧键重新审查，增加1个预处理块记录；46个单元已注释、1项既有TODO保留、13项有证据地无需新增注释。47段中文Doxygen说明职责、使用时机、值模型、参数/返回值（适用时）、生命周期、不变量与真实引用；逐项定义、调用方、入口和测试证据见[九列表](../coverage/zr_vm_common.tsv)。

类型别名不承诺宿主宽度、对象保活、所有权或线程同步。长度和偏移须由消费方校验；allocator回调保留原分配失败和最终释放边界；异常恢复点必须留在有效的同一原生线程调用栈中。编译器与信号宏按当前预处理分支解释；分支提示只向编译器提供预测信息，调用方仍须满足表达式本身的有效性与副作用约束。ZR_BIT_MASK原有signed-shift疑点保留为TODO，未发现仓内生产消费，不据此宣称死代码或已复现BUG。新增Doxygen中有1处TODO标签，是原第30行既有问题的文档镜像；台账按1个问题记录，不声称标签字面数量保持不变。

另更正原12、14行两条完整旧注释：先包含stdlib.h/stddef.h只说明包含顺序，不能说修正头文件搜索路径；clang-cl定义_MSC_VER时也进入该分支。47段EOF文档正文保持独审候选原字节。原212行除这两条注释外逐行原EOL字节保持，所有原程序物理行号和UTF8列保持；注释共增长27B，后续绝对字节偏移随之改变，原前缀不是全字节相同。新头61253B/1241LF，逆向还原原6680B精确。没有修改公开签名、值、分支或程序语句。

调用检索不只复用旧索引。补证按当前Git首方源/构建/测试成员全扫3975路径，与原3962成员比较增加13、无移除；2992原匹配文件重新核对完整符号位置，35项新/变文件逐项核对，保留原F/I、完整当前匹配行和两个不同读取窗口。源码注释命中不是调用；allocator函数指针、新frame预算/OOM/GC重入、异常恢复及Rust sys边界另按实际路径审查。现有3017全量旧pin并非全部当前，不作该声明。新增调用仍满足本批合同，独立复核证据由交付ledger绑定。

使用当前实际checkout和既有E:/cargo-targets构建目录，注释前后各执行一次原有四root构建：Math静态库、GC root leases、frame slot lookup、SSA platform matrix。各阶段真实构建成功后，运行7个GC selector、frame完整39项Unity测试及matrix的20个场景函数。Frame保留原性能断言和全部迭代次数，配对仅允许5条sample及1条median中的tick数值变化；39个用例ID、footer、迭代数和退出状态须一致。Matrix20不是20个平台或只有20条断言，Math只获静态编译证据。

两阶段选定的依赖闭包均含316个C对象编译条目，来自313个物理源文件；这不是本轮实际重新编译次数。重复源ssa_platform_contract.c分别进入四个目标；12个传递target不含Parser。全部316个full-object deps在build后为VALID，唯一tinydir零依赖按原源pin单独批准；每步前后核有限输入、工具、生成配置及产物稳定，不声称完整宿主闭包或无并发写入。GC的预期OOM stderr保留；frame的两项IO测试构造内存布局，不能替代二进制decoder的版本/坏头/截断覆盖。未增加重复实现的测试。

证据保留在 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463：

- common-scalar-io-gc-config-current-contract-review-r1.json 与原独审：全量静态合同及既有19项typed更正；本批只取Common60。
- common-public-header-source-comment-erratum-r1.json 与实际source application：两旧注释例外、47Dox精确插入与可逆证明。
- common-public-header-current-caller-delta-supplement-r1.json：当前成员/调用补扫及明确旧索引边界。
- common-public-header-native-before-r2与after-r2的frozen plan、独审、results、process及Root真实outer终态。最初before-r1虽build实际0，但生成glob再生触发守门，未运行测试；该实际1与独立诊断完整保留，不计通过。
- common-public-header-native-final-acceptance-r1.json：d492559f0ed8b6393fc740bfdd98300444d0da45da362eff8e58853cd5c8afbe，只接受本批有限注释配对。

check-batch和暂存空白检查要求实际成功。全仓inventory原有陈旧状态前后同样退出1，不称全仓验收成功。其他模块、混合leaf外406行及其他inventory行保留；本次仅提交上述头、60条台账、本批说明和导航。产物生命周期OPEN，自有临时根端点实际为空，保留复用构建和证据；手工释放0B。
