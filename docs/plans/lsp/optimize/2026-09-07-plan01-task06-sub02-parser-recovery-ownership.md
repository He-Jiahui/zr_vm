---
plan_id: optimize
task: plan01-task06-sub02
status: completed
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - tests/CMakeLists.txt
  - tests/parser/test_parser_recovery_ownership.c
  - tests/cmake/zr_vm_parser_recovery_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - docs/plans/lsp/optimize/2026-09-07-plan01-task02-sub24-dispatch-handler-status.md
tests:
  - parser_recovery_ownership
  - zr_vm_parser_test
  - language_server_stdio_handler_cancellation
  - language_server_stdio_server_lifecycle
  - language_server_stdio_protocol_conformance
  - language_server_stdio_document_sync_conformance
  - language_server_stdio_diagnostic_fix_smoke
doc_type: plan-record
---

# Plan 01 Task 6 Sub02: Parser Recovery Ownership

## 状态与产出记录

- 开始时间: 2026-09-07 15:08 +08:00
- 实际完成时间: 2026-09-07 15:18 +08:00
- 状态: 已完成恢复路径所有权修复；完整诊断语义门禁未完成
- 源码版本: `96bba841` 加共享工作树 overlay
- 触发证据: diagnostic-fix smoke 的 Clang LSan 报告 4,056 字节/20 次分配泄漏；
  只回放 initialize/didOpen/shutdown/exit 的 24 帧也能复现。
- 产出路径: 四个 parser 模块、独立 Unity 测试及 CMake 入口、模块文档和本记录

## 最小 RED 与修复

独立测试直接调用 parser，成功或失败后都释放 AST、parser 和 global，并检查
追踪 allocator 的活动块数。错误输入必须产生诊断；同时接收 structured 和 legacy
error callback，因为现有恢复路径两者并用。初版只接收 structured callback，
三个样例未被正确识别为出错输入；补齐 legacy capture 后得到有效 RED：
15 项中 13 项失败，每项遗留 1 到 14 个分配块。正常复合语法和已有清理的错误
数组样例通过。缺失参数右括号遗留 7 块，缺失函数体遗留 14 块。

数组和对象失败分支改用已有 `free_ast_node_array_with_elements`，释放已经
附加到容器的子节点。对象当前尚未附加的 key/value 由局部失败分支释放；缺失
分组右括号时释放已解析的 expression。函数声明将所有局部 owner 初始化并统一
交给 cleanup，成功时仍转交 AST；名称、泛型、普通/可变参数、返回类型、函数体
和 decorators 均有归属。参数名称失败释放 decorators；参数节点分配失败也释放
名称、默认值和类型。没有改变 parser 的 token 消费、错误恢复或诊断内容。

`parser_expression_primary.c` 和 `parser_types.c` 已较大，本次仅补现有失败分支
的释放，不增加职责；测试独立于已有较大的 test_parser.c，CMake 注册也位于新
的窄模块。初次构建暴露一处清理语句匹配到错误函数，修正到 parse_parameter 后
重新完成 GCC/MSVC 构建；以下结果均对应修正后的源码。

## 验证命令及结果

```text
GCC: /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc
Clang ASan/UBSan: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
MSVC Debug: .codex/lsp-optimize-validation/msvc-current

cmake --build <build> --target zr_vm_parser_recovery_ownership_test \
  zr_vm_parser_test zr_vm_language_server_stdio -j4
  GCC, Clang, MSVC: passed, exit 0

<build>/bin/zr_vm_parser_test
  GCC, Clang ASan/UBSan, MSVC: 74/74 passed, exit 0

ctest --test-dir <build> --output-on-failure \
  -R "^(parser_recovery_ownership|language_server_stdio_(handler_cancellation|protocol_conformance|document_sync_conformance|server_lifecycle))$"
  GCC: 5/5 passed, exit 0 (14.71 seconds)
  Clang ASan/UBSan: 5/5 passed, exit 0 (30.12 seconds)
  MSVC Debug: 5/5 passed, exit 0 (17.00 seconds)
  parser_recovery_ownership: 15/15 cases on each build

valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_parser_recovery_ownership_test
  15/15 passed, exit 0
  7539 allocs, 7539 frees; 0 bytes in 0 blocks at exit
  ERROR SUMMARY: 0 errors from 0 contexts, no suppressions

node tests/language_server/stdio_diagnostic_fix_smoke.js <Clang server>
  Child server: status 0, signal null, stderrBytes 0
  JavaScript test: exit 1, Expected possibly_uninitialized_read publication (line 731)
```

最后一项使用 spawnSync wrapper 记录 child status/stderr，并运行完全相同的原始
协议输入和断言。原先第 720 行 server exit 1 的 LSan 失败已经消失，现到达与
GCC/MSVC 和 09-05 基线相同的缺失诊断断言。没有关闭 sanitizer 或删除断言。
Clang parser 全量和定向回归没有 sanitizer 报告。

完整 parser 日志位于 `.codex/lsp-optimize-validation/` 下的
`plan01-task06-sub02-{gcc,clang,msvc}-parser.log`；原始诊断回放 stderr 保存在
`plan01-task06-sub02-diagnostic.stderr.log`，为空文件。MSVC 通过 VsDevCommand，
WSL 使用 Node 22.13.1；Clang 保留 address/undefined、frame pointer 和 `-no-pie`。

## 接受决定

接受这组 parser 恢复清理修复，作为 Plan 01 内存门禁的底层支持子项。这里的
测试是语法错误恢复矩阵，不是整个 parser 的穷举分配失败验收。缺失诊断、泛型
completion detail、initialize 状态接口和 Plan 01 Task 2/4/6 其余要求仍 pending。
tests/CMakeLists.txt 只提交新 parser 测试 include；共享工作树中的 call-binding
测试注册保留在原有未提交状态。
