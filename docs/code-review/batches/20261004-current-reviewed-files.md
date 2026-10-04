---
related_code:
  - tests/performance/perf_report.c
  - tests/performance/perf_report.h
  - tests/core/test_ssa_roots_observation.c
  - tests/core/test_ssa_young_allocation.c
  - tests/core/test_ssa_backend_service.c
  - tests/core/test_ssa_host_baseline_jit.c
  - tests/core/test_ssa_host_jit_optional.c
  - tests/parser/test_reference_loan_nll.c
  - tests/parser/reference_loan_nll_test_support.h
  - zr_vm_lib_iteration/CMakeLists.txt
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.h
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
  - zr_vm_lib_math/include/zr_vm_lib_math/complex.h
  - zr_vm_lib_math/include/zr_vm_lib_math/complex_registry.h
implementation_files:
  - scripts/code_review_inventory.py
plan_sources:
  - "user: whole-workspace caller/intention review and comments"
tests:
  - tests/core/test_ssa_roots_observation.c
  - tests/core/test_ssa_young_allocation.c
doc_type: testing-guide
---

# 2026-10-04 当前文件复审

本批确认 19 个当前文件的完整审查单元，并同步首方文件清单。函数、声明、类型、状态与需独立解释的块分别登记；普通保护和局部条件随所属函数审查。源码内容变化后，清单重新置为 `pending`。

## 文件与单元

| 文件 | 单元数 |
| --- | ---: |
| `tests/performance/perf_report.c` | 28 |
| `tests/performance/perf_report.h` | 11 |
| `tests/core/test_ssa_roots_observation.c` | 26 |
| `tests/core/test_ssa_young_allocation.c` | 11 |
| `tests/core/test_ssa_backend_service.c` | 29 |
| `tests/core/test_ssa_host_baseline_jit.c` | 9 |
| `tests/core/test_ssa_host_jit_optional.c` | 12 |
| `tests/parser/test_reference_loan_nll.c` | 27 |
| `tests/parser/reference_loan_nll_test_support.h` | 18 |
| `zr_vm_lib_iteration/CMakeLists.txt` | 1 |
| `zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h` | 4 |
| `zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c` | 5 |
| `zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c` | 10 |
| `zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c` | 22 |
| `zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.h` | 7 |
| `zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c` | 11 |
| `zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c` | 8 |
| `zr_vm_lib_math/include/zr_vm_lib_math/complex.h` | 11 |
| `zr_vm_lib_math/include/zr_vm_lib_math/complex_registry.h` | 3 |

共 253 个单元：performance 39、GC 37、host/service 50、NLL 45、iteration 20、views 29、Complex 33。15 个此前已应用文件经过独立当前全文盘点；Complex 四文件的 33 个单元经过独立复核并已提交 `09a85edcfbb76160f9305dfc7ae26b299afe3200`。此前六批文件已实际纳入共享提交 `b3b42d539075a3988c498131f8cf2f13dc238072`；该共享快照提交由另一会话完成。

## 验证范围

- Performance 原版/注释版既有 Windows Clang+LLD fixture 均通过；`/dev/full` 因 Windows 无该设备而跳过。
- Roots 和 young allocation 原版/注释版共四个既有 fixture 均通过编译、链接、运行（22 步），断言启用、输出为空，依赖在链接前与运行后相同。两个旧进程观察失败仍保留失败；新验证使用经独立复核的观察修订，要求同一已核实成员句柄 signaled 并成功读取退出码后才略过其映像名查询，随后继续等待本次 Job 自然归零。此处只授予实际夹具场景的验证；完整收集器、线程、OOM、Linux 和 CTest 未执行。
- Complex 原版/注释版回调及 registry 共四次编译通过；原版与注释版 registry 的七条既有警告保持相同。脚本、极端数值、OOM 和运行时未执行。
- Host/service、NLL、iteration、views 本轮完成静态意图复审，没有新增运行结果。

## 问题与台账限制

已有 BUG/TODO 保留其具体触发条件与核查入口：普通 performance 中位数分配失败可发布错误零峰值；NLL 夹具重复 LOAD result token 导致目标 LOAD 未建立；极大有限 Complex 平方范数溢出影响 magnitude/normalized。未核实的 GC 字节数组对齐、provider 注册/能力负例、view 初始化和字符串结果失败契约继续使用 TODO。

19 次外部 `ssa-tests.cmake` 锚点维护只移动至同一语句和上下文的新位置，16 个单位的其他字段及结论保持不变。Views 仅接受当前 C/H 的 29 单元；canonical 的旧 pooling 行及 native 表的旧空行锚点仍需其所属批次复审。Math 仅接受本批 Complex 33 单元，其他历史行的锚点格式问题仍在。

## 当前清单

本次盘点 3631 个首方文件，其中 1227 个仍为 `pending`。这一统计绑定本批清单中的内容摘要；并发会话的新文件或后续变更会要求再次同步。整个工作区审查仍在进行。
