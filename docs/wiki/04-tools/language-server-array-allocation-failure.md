---
related_code:
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/index/symbol_table.c
  - zr_vm_language_server/src/zr_vm_language_server/index/reference_tracker.c
  - tests/language_server/test_lsp_array_oom.c
  - tests/cmake/lsp-array-oom-tests.cmake
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/index/symbol_table.c
  - zr_vm_language_server/src/zr_vm_language_server/index/reference_tracker.c
plan_sources:
  - "user: 2026-10-16 修复现役数组分配失败与资源清理缺陷"
tests:
  - tests/language_server/test_lsp_array_oom.c
  - tests/cmake/lsp-array-oom-tests.cmake
doc_type: module-detail
---

# LSP 数组分配失败契约

本页描述语义快照、符号和引用索引中已覆盖的数组分配失败路径。测试直接链接当前 checkout 的语言服务器、解析器和 Core 实现，由真实 VM 上游分配器注入失败；没有复制生产实现或以模拟数组替代被测代码。

## 构造失败

`LspSemanticSnapshotCache_New`、`SymbolTable_New`、`Symbol_New` 和 `ReferenceTracker_New` 在数组初始化失败时返回 `NULL`。这些路径使用 `Array_TryInit`，释放此前分配的本地原生资源，不通过异常跳过清理。

符号表先将两个数组和备用哈希表构造成空态，再逐项初始化。这样任意顶层数组失败均可调用统一析构。全局作用域在加入 `allScopes` 前仍由构造函数持有；其结构或符号数组分配失败时，构造函数负责回收。加入 `allScopes` 后，作用域和符号由符号表析构统一释放。名称索引仅借用符号，GC 对象本体仍由 GC 管理。

`Symbol_New` 失败时还释放已复制的推断类型；输入名称、AST 与 GC 字符串不转移所有权。`ReferenceTracker_New` 不拥有传入的符号表。

## 引用登记

`Symbol_AddReference` 只有在 `Array_TryPush` 成功后才增加 `referenceCount`。失败返回 `false`，引用数组的已有内容、长度和计数保持不变。

`ReferenceTracker_AddReference` 为新位置分配原生记录，再加入 tracker 索引。tracker 数组增长失败时释放尚未登记的记录并返回 `false`。若随后 symbol 数组增长失败，则弹出刚加入 tracker 的记录、释放该记录并返回 `false`；两份索引的已有内容、长度与引用计数保持一致。容量可能保留已成功扩出的空间。

## 快照与 AST 所有权

`Capture` 返回 `false` 时，调用方仍负责处理传入的 `retainedAst`。因此在 `DetachCurrentStateForSnapshot` 已接管 AST 后再尝试分配数组，会让失败清理与调用方所有权冲突。

实现先统计同 URI 的已有快照，计算本次插入时将淘汰的数量，并对最终总长度调用 `Array_TryReserve`。容量不足或溢出时立即返回 `false`，缓存内容和当前分析器保持原状。预留成功后才分离分析器、淘汰旧快照并追加记录；最后一次追加无需分配。若 URI 已达上限，预留按淘汰后的总长度计算，避免无必要的扩容失败。

## 验证与边界

CTest 为每个注入点启动独立进程，并设置 30 秒超时。Windows 禁用崩溃与断言对话框，避免失败用例等待交互。

已记录修复前七项真实失败：缓存构造、符号表顶层两个数组构造、引用追踪器构造、符号构造、tracker 增长、symbol 引用增长及真实文档更新触发的快照增长。这七项修复后已由统一构建验证通过。新增跨索引失败用例已记录真实 RED：返回值、双索引回退及记录释放三个断言均失败；回退修复后，统一 support-oom-string-red-r2 验证八项全部通过。验证收据由统一构建协调者保存在 `E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/`；结果以实际 CTest 收据为准。

这些用例只证明所列分配边界。`AddSymbolEx`、`LookupAll` 和 `EnterScope` 的其他数组路径尚未包含在本回归组中，不能由这里的结果推断其全部 OOM 行为已验证。
