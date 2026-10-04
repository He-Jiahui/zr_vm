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
  - scripts/codegen/generate_execbc_patterns.py
  - tests/library/test_close_proxy_aot_runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
implementation_files:
  - scripts/code_review_inventory.py
  - scripts/codegen/generate_execbc_patterns.py
  - tests/library/test_close_proxy_aot_runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
plan_sources:
  - "user: whole-workspace caller/intention review and comments"
tests:
  - tests/core/test_ssa_roots_observation.c
  - tests/core/test_ssa_young_allocation.c
  - tests/library/test_close_proxy_aot_runtime.c
  - tests/acceptance/ssa-generated-fusion.md
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/parser/test_buffer_pool_ffi.c
doc_type: testing-guide
---

# 2026-10-04 当前文件复审

本批确认 22 个当前文件的完整审查单元，并同步首方文件清单。函数、声明、类型、状态与需独立解释的块分别登记；普通保护和局部条件随所属函数审查。源码内容变化后，清单重新置为 `pending`。

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
| `scripts/codegen/generate_execbc_patterns.py` | 18 |
| `tests/library/test_close_proxy_aot_runtime.c` | 12 |
| `zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c` | 39 |

共 322 个单元：performance 39、GC 37、host/service 50、NLL 45、iteration 20、views 29、Complex 33、ExecBC generator 18、close-proxy fixture 12、FFI runtime 39。15 个此前已应用文件经过独立当前全文盘点；Complex 四文件的 33 个单元经过独立复核并已提交 `09a85edcfbb76160f9305dfc7ae26b299afe3200`。此前六批文件已实际纳入共享提交 `b3b42d539075a3988c498131f8cf2f13dc238072`；该共享快照提交由另一会话完成。

## 新增完整文件的意图与提交

- ExecBC generator 的 18 单位解释 schema 展开、模式约束、符号表达式与文本发布边界（8 函数、7 状态、3 块）；已提交 `c33de2687ef7f6e63d3a140750e01e3b851d3dca`。仅完成该 Python 文件，不提升 C matcher 或其他 codegen 文件。
- Close-proxy fixture 的 12 单位解释全局观察状态、Unity 调用、普通 native callback、dense/physical 双 source 与两次 CloseScope 的不同职责（5 函数、4 状态、3 块）；已提交 `f5ced67624c81662727ff535ff38a03661d53267`。正式 12 行九字段经修正与独审通过；旧 postapply 格式 HOLD 原件保留。
- FFI runtime 的 39 单位解释库/symbol/callback/pointer/buffer 的 owner、活动帧、关闭与 pin 生命周期以及结果/输入错误边界（25 函数、14 块）；已提交 `4b75477fb05b140e6821c0acfef488cca8d9455c`。仅该实现文件完整闭合，头文件、内部子模块及 aot/native 反向锚点维护不提升为完整审查。

## 验证范围

- Performance 原版/注释版既有 Windows Clang+LLD fixture 均通过；`/dev/full` 因 Windows 无该设备而跳过。
- Roots 和 young allocation 原版/注释版共四个既有 fixture 均通过编译、链接、运行（22 步），断言启用、输出为空，依赖在链接前与运行后相同。两个旧进程观察失败仍保留失败；新验证使用经独立复核的观察修订，要求同一已核实成员句柄 signaled 并成功读取退出码后才略过其映像名查询，随后继续等待本次 Job 自然归零。此处只授予实际夹具场景的验证；完整收集器、线程、OOM、Linux 和 CTest 未执行。
- Complex 原版/注释版回调及 registry 共四次编译通过；原版与注释版 registry 的七条既有警告保持相同。脚本、极端数值、OOM 和运行时未执行。
- Host/service、NLL、iteration、views 本轮完成静态意图复审，没有新增运行结果。
- ExecBC generator 原版/注释版以相同 schema/header 执行已有只读 `--check`，两次自然退出 0；仅证明文本 freshness，不覆盖默认写入/replace/OOM、C matcher 编译、native 或 CTest。
- Close-proxy fixture 原版/注释版已有两次 Windows C 编译通过；未链接或运行，不证明真实 AOT generated entry、移动 GC、异常清理或 CTest。人工 generatedFrame 与永久标记对象的限制继续保留。
- FFI runtime 原版/注释版已有两次 Windows C 编译通过，均禁用 libffi（`ZR_VM_HAS_LIBFFI=0`）；仅为 fallback 编译证据，不证明 foreign call、libffi closure、脚本或运行时。先前失败收据保留，本次维护不重跑编译。

## 问题与台账限制

已有 BUG/TODO 保留其具体触发条件与核查入口：普通 performance 中位数分配失败可发布错误零峰值；NLL 夹具重复 LOAD result token 导致目标 LOAD 未建立；极大有限 Complex 平方范数溢出影响 magnitude/normalized。未核实的 GC 字节数组对齐、provider 注册/能力负例、view 初始化和字符串结果失败契约继续使用 TODO。

19 次外部 `ssa-tests.cmake` 锚点维护只移动至同一语句和上下文的新位置，16 个单位的其他字段及结论保持不变。Views 仅接受当前 C/H 的 29 单元；canonical 的旧 pooling 行及 native 表的旧空行锚点仍需其所属批次复审。Math 仅接受本批 Complex 33 单元，其他历史行的锚点格式问题仍在。

FFI 的路径复制失败与关闭后 getVersion fresh lookup 两项静态 BUG 保留；高索引、closed-as、typed read 容量、read 追加失败以及 write 前缀/截断继续 TODO。其他 FFI/native/aot 历史台账行未重新全审。

## 当前清单

本次盘点 3641 个首方文件，其中 1235 个仍为 `pending`。这一统计绑定本批清单中的内容摘要；并发会话的新文件或后续变更会要求再次同步。整个工作区审查仍在进行。
