---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
  - zr_vm_core/src/zr_vm_core/gc/gc_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_compact.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
  - zr_vm_core/src/zr_vm_core/gc/gc_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_compact.c
  - tests/core/test_gc_contract_boundaries.c
  - tests/cmake/gc-contract-boundary-tests.cmake
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/core/test_gc_contract_boundaries.c
  - tests/cmake/gc-contract-boundary-tests.cmake
doc_type: regression-contract
---

# GC 预算及压缩计划的拒绝边界

本组通过真实 Core 公共入口检验预算、major 状态和 compact 计划的无效输入。
夹具不启动 VM，也不分配测试文件。真实 RED r2 已完成：14 项新回归加两项既有预算测试中，12 项失败、4 项通过；1954 个输入无变化。生产修复已写入，等待独立审查与统一 GREEN。

十四个独立进程覆盖 reserved 与 flags 的诊断区分、无效和空预算的结果初始化、
非法预算阶段、零工作量、major 游标溢出、非法状态和非法后继阶段、compact 非法模式及区域分区。
合法零状态和最大区域边界是正常控制。负枚举值按当前平台的枚举表示比较，
不要求这些用例在所有编译器上都先失败。

参数校验失败后，调用方仍需要一个可安全复制的拒绝结果。本次明确该合同为：
存在 result 时始终初始化它，保留输入 cursor，其他计量归零，阶段为 IDLE、暂停
原因为 NONE，consistentBoundary 为 false。现有公开说明只保证失败诊断，
公开说明已同步更新；计量只在参数和游标验证通过后填入。夹具用合法、不同的旧字段值检查陈旧结果不会被发布；
不读取未初始化数据，也不对结构体 padding 做比较。

无效 phase 应报告 INVALID_PHASE；游标加法溢出在 major 层仍应报告 OVERFLOW。
compact 的 candidate 和 pinned 区域是互斥分区，二者合计不得超过 regionCount；
验证必须避免 UINT32_MAX 加法回绕。夹具同时覆盖普通超额和加法回绕边界。

中央 Evaluate 的确定性输出与 major 消费链分开记证：本组动态验证中央输出，
major 在失败后复制局部结果的风险另有静态调用链证据，不能宣称已动态复现
未初始化读取。实际运行的源码身份与进程收据存放于 E 根专属任务报告。
