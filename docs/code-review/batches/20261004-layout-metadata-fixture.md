---
related_code:
  - tests/core/test_type_layout_metadata_contracts.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
implementation_files:
  - tests/core/test_type_layout_metadata_contracts.c
plan_sources:
  - "user: whole-workspace caller/intention review and comments"
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/CMakeLists.txt
  - tests/cmake/run_executable_suite.cmake
doc_type: testing-guide
---

# 布局元数据 fixture 完整复审

本次范围只有 `tests/core/test_type_layout_metadata_contracts.c` 全文：25 个函数（18 个注册用例、4 个内部 helper/回调、2 个 Unity 钩子、main）、1 个记录类型、3 个字段及 2 个独立说明块，共 31 单元。普通 guard、循环和条件分支随所属完整函数阅读。旧 13 行是局部记录，不能授予全文完成信用。

## 实际入口与用途

`tests/CMakeLists.txt:352` 创建对应测试目标，`:7986` 将其传给套件脚本；`tests/cmake/run_executable_suite.cmake:66` 启动进程。main 的 18 个 RUN_TEST 经 UnityDefaultTestRun 依次执行 setUp、Func、tearDown，并返回汇总退出码。

旧式 metadata、完整 contract 与默认构造入口分别核对 C 类型身份、字段标记计数、复制/释放类别、GC/所有权/引用映射、schema/hash 和跨域策略元数据。测试只执行构造、校验、哈希比较、MOVE_ONLY 复制拒绝及同步访问器记录，不实际执行跨域传递、对象收集或所有权释放。前两组无效跨度/映射用例可能有多个拒绝原因；哈希比较只支持给定描述符的稳定与漂移结果。

显式映射反例分别检查槽内错误类别、重复和遗漏；合法映射允许排列变化或空表从字段推导。联合布局保留合法重叠偏移的出现次数，访问器只报告活动 tag 成员。三个映射类别中只有 GC 类执行访问器；另外两类只执行 Validate。

## 资源、回调与失败边界

字段、映射表、storage 和访问记录均为当前用例的栈上对象。构造器复制 contract 的标量并借用表指针；临时 contract 可结束，字段及偏移表必须保留至最后一次断言或遍历。同步回调注册四处，真实派发在 `zr_vm_core/src/zr_vm_core/type_layout.c:937`（显式非联合表）及 `:974`（字段回退/活动联合成员），原样传递 userData。

访问记录借用同一 SZrTypeValue 数组作为地址基准，最多保存前四项偏移，同时 count 统计所有回调。错误 GC 表的 witness 仅在错误布局意外被 Validate 接受时记录访问偏移。它访问真实对齐的栈槽，不调用收集器。fixture 无动态分配和跨例资源，因此空 Unity 生命周期钩子不遗漏资源清理。

## 台账、反向引用与验证限制

`coverage/tests_core_execution.tsv` 仅将本文件 13 行替换为 31 行，其余当前行原始字节保留。`coverage/zr_vm_core_type_layout.tsv` 的两个 metadata API 记录仍指向本文件实际构造调用 `:23`；该语句未因注释移动，因此无需制造反向行号修改。

源码变化只涉及中文注释，非注释 token、字符串/字符字面量及预处理指令相同，原始 LF 与末尾换行保留。31 行候选台账、两行选定反向记录及当前源码补丁检查均有静态检查结果。未新增 BUG/TODO 标签，未构建或运行本用例，未新增 native、实际 moving GC 或故障注入复现信用。独立审查与当前依赖 guards 支持这一个 fixture 的注释意图，不能扩大为整个 core 模块验收。
