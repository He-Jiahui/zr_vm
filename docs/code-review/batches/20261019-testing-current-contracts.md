---
related_code:
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_testing/src/zr_vm_lib_testing/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c
  - tests/testing/test_assertions.c
  - tests/library/test_official_provider_convergence.c
  - tests/cmake/container-task-ffi-targets.cmake
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
implementation_files:
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_testing/src/zr_vm_lib_testing/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
plan_sources:
  - "user: 逐单元审查、必要中文注释、保留程序行为并按功能模块交付。"
  - docs/code-review/comment-standard.md
  - docs/library-and-builtins/zr-testing.md
tests:
  - tests/testing/test_assertions.c
  - tests/library/test_official_provider_convergence.c
  - tests/cmake/container-task-ffi-targets.cmake
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
doc_type: review-batch
---

# Testing 当前 provider、断言与失败记录契约

本批复核既有173个完整三列键，不新增定义信用。保留或按需精修102段既有Doxygen，以58处同物理行注释替换落实当前调用、所有权和失败合同；原程序与字符串词元、行号、UTF-8列和逐行EOL保持，注释长度变化不保证绝对字节offset相同。逐项结论为115 commented、8 no-comment、26 BUG、24 TODO；BUG/TODO沿用现有证据家族，注释不修复行为，普通验证不代表对应触发组合已经执行。

源码应用：Root 实际应用进程45496与不同进程40900均退出0；58处精确注释操作正逆校验、逐行EOL及程序词元行列保持，应用报告 testing-library-current-source-comment-application-r1.json（SHA256 0f3ab44a1219f3c03bde36a581fec7d683833fcad7835585bc8579538a170224）。

现有验证：BEFORE/AFTER各 assertions 12/12、provider 9/9 通过，两次build及各4次query均退出0；详见本批记录，有限用例不代表173项或缺陷触发全覆盖。

配对证据：testing-library-current-native-pair-r1.json，44,328 B，SHA256 603fe9803964b8652001c102e8b346cfd77aa8536efaf2955188212e4f188f9e；Root配对进程7064已由工具977911确认实际退出0。

验证采用当前FileAPI解析的两个现有EXE，直接运行assertions的12项及official-provider-convergence的9项既有Unity入口；120秒是本任务runner policy，不改写为源码CTest TIMEOUT。assertions没有专用CTest注册；official_provider_convergence已有CTest，但本次直接EXE运行不冒称执行过CTest。以实际逐项输出和footer为准，不从controller退出码独推全部测试通过。

现有断言只支持各自明确验证的路径：包括Test phase、结构化失败、规范相等、Throws类型匹配及callback内部GC等。它们不构成字符串分配OOM、raw record构造/字段构造窗口GC、TLS重入、TO_STRING抛错匹配、全部profile、跨线程或插件卸载故障的验证；普通清理路径也不保证nonlocal Throw或Unity长跳后执行。

关联模块文档仅按14个精确文字操作纠正当前契约：撤回短串旧前提下SetString普通返回未初始化、void setter可据此推断静默失败、以及固定两个GC roots的旧叙述。当前有效state失败可Throw并跳过后续字段或普通清理，之前工作不整体回滚。这些是对当前callee与pin协议的核实，不归因为本批行为修复；历史提交事实和其它仍有证据的风险保持。

混合投影：descriptor.c的9处本批注释行从HEAD独立投影到提交；实际WT另含6条外来References更正，均保留且不纳入本批。inventory descriptor sha记录提交投影f88be2ba680b0c82e196cd8c1f53ab2cf75437f26580976063286d725fb350fa；当前WT为9276c06b728de5ff40dfb98417070f51661e875f48ba2aa5c81da99804a886ba，两者不相等。inventory仅4行的sha/status/note更改，原历史note前缀和全部外围行保持；coverage仅173旧键第4至9列更新，外围10行原始字节保持。

check-batch只验证格式、完整三列键与现存非空行锚，不校验inventory哈希或语义。全局check的真实退出及原始输出另记于本批checks，与旧基线比较；既有stale不被改写成通过，不宣称descriptor当前WT哈希与清单一致或全仓通过。本批旧173不重新计入新增信用。

整张既有 coverage 的 check-batch 实际退出1：本批范围外两行仍引用 CommonMacros.cmake:24、tests/CMakeLists.txt:4605 的空行或缺失行，当前 HEAD 原台账在同一源码上也报告这两项。本批173行用原 check_batch 函数在内存投影逐项检查，问题数为0；外围10行字节保持。本批不宣称整张台账或全仓检查通过。
