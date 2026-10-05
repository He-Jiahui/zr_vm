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
  - tests/core/test_close_proxy.c
  - tests/core/test_close_proxy_instruction.c
  - tests/core/test_close_meta_exception.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_aot_gc_root_frame_exception.inc
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_core/include/zr_vm_core/execution.h
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_context.h
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - tests/task/test_task_frame_runtime.c
  - tests/core/test_execution_checked_divide.c
  - tests/core/test_execution_checked_multiply.c
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/core/test_hash_set_dense_paths.c
implementation_files:
  - scripts/code_review_inventory.py
  - scripts/codegen/generate_execbc_patterns.py
  - tests/library/test_close_proxy_aot_runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - tests/core/test_close_proxy.c
  - tests/core/test_close_proxy_instruction.c
  - tests/core/test_close_meta_exception.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_aot_gc_root_frame_exception.inc
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h
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
  - tests/core/test_close_proxy.c
  - tests/core/test_close_proxy_instruction.c
  - tests/core/test_close_meta_exception.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_aot_gc_root_frame_exception.inc
doc_type: testing-guide
---

# 2026-10-04 当前文件复审

本批当前确认 49 个完整文件、1962 单元。原完整正文/census/实际源提交证据与当前源码字节绑定；纯canonical修订经真正用途独审并正常提交后，正式code-anchor标准重新逐项核对。普通guard随所属函数审查，no-comment理由保持。


## 文件与单元

| 当前完整文件 | 单元数 |
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
| `tests/core/test_close_proxy.c` | 46 |
| `tests/core/test_close_proxy_instruction.c` | 12 |
| `tests/core/test_close_meta_exception.c` | 23 |
| `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h` | 37 |
| `zr_vm_common/include/zr_vm_common/zr_io_conf.h` | 61 |
| `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 91 |
| `tests/core/test_aot_gc_root_frame.c` | 17 |
| `tests/core/test_aot_gc_root_frame_exception.inc` | 27 |
| `zr_vm_core/include/zr_vm_core/closure.h` | 69 |
| `zr_vm_core/src/zr_vm_core/closure.c` | 47 |
| `zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c` | 13 |
| `zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h` | 2 |
| `zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c` | 4 |
| `zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h` | 4 |
| `zr_vm_library/include/zr_vm_library/aot_runtime.h` | 302 |
| `zr_vm_library/src/zr_vm_library/aot_runtime.c` | 496 |
| `zr_vm_core/include/zr_vm_core/execution.h` | 8 |
| `zr_vm_core/include/zr_vm_core/execution_call_transfer.h` | 30 |
| `zr_vm_core/src/zr_vm_core/execution/execution_context.h` | 2 |
| `zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c` | 4 |
| `zr_vm_library/src/zr_vm_library/task_runtime.c` | 125 |
| `zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc` | 14 |
| `tests/task/test_task_frame_runtime.c` | 75 |
| `tests/core/test_execution_checked_divide.c` | 39 |
| `tests/core/test_execution_checked_multiply.c` | 38 |
| `tests/core/test_type_layout_metadata_contracts.c` | 31 |
| `tests/core/test_hash_set_dense_paths.c` | 23 |

当前完整闭合合计1962单元。r6历史44、旧selected/current-code59、r7当前canonical33均保留为历史；本次实际currentcanonical code-anchor缺口0。此结论由每行九字段/用途独审/实际commit链和原wholebody证明共同支持，不由计数降零单独推断。


## 新增完整文件的意图与提交

- ExecBC generator 的 18 单位解释 schema 展开、模式约束、符号表达式与文本发布边界（8 函数、7 状态、3 块）；已提交 `c33de2687ef7f6e63d3a140750e01e3b851d3dca`。仅完成该 Python 文件，不提升 C matcher 或其他 codegen 文件。
- Close-proxy fixture 的 12 单位解释全局观察状态、Unity 调用、普通 native callback、dense/physical 双 source 与两次 CloseScope 的不同职责（5 函数、4 状态、3 块）；已提交 `f5ced67624c81662727ff535ff38a03661d53267`。正式 12 行九字段经修正与独审通过；旧 postapply 格式 HOLD 原件保留。
- FFI runtime 的 39 单位解释库/symbol/callback/pointer/buffer 的 owner、活动帧、关闭与 pin 生命周期以及结果/输入错误边界（25 函数、14 块）；已提交 `4b75477fb05b140e6821c0acfef488cca8d9455c`。仅该实现文件完整闭合，头文件、内部子模块及 aot/native 反向锚点维护不提升为完整审查。

- Core-close 三个完整测试文件共 81 单位，包含 C 函数/类型/状态/独立块与内嵌语言方法、类型和状态；分别解释普通 @close、手写 close 指令和异常清理路径。已提交 `470fc4510caa0ea0e0d7f5eb86980a2dbecd75ac`。原 empty setUp/tearDown 的资源边界、替换异常及不同 call-info 场景保留；不推广为完整 GC/运行时验证。
- Common 三个完整公共头文件共 189 单位：instruction 37、IO 61、AOT 91。覆盖公开宏、enum/成员、struct/字段、typedef、声明与独立约束块；完整成员 census 已独立复核。已提交 `6f96987f494e72e9c092bb91fbfac53c3a139a34`。schema 顺序、重叠 operand、有效 PC 义务、legacy wire 与 C struct 差异以及 borrowed AOT 表/索引能力边界仍适用，不提升所有消费者。

- AOT GC root fixture 的 C/inc 两文件共44单位：24函数、5类型、15块，12 Unity场景，解释root-frame配对、slot/map布局、栈扩容及异常清理边界。已提交 `a960ae27dea287408e29fdf1cd4d66f352b61e51`。common RootSlot.reserved 唯一caller从旧C:34维护到当前C:40，实际置零代码相同；common其他188条、core-close81保持。旧inc:132闭括号证据已明确补正为inc:131的Pop语句，旧证据未追认通过。

- Closure 六个完整生产 C/H 共139单位，覆盖闭包与upvalue公开接口、普通关闭/close meta guard及close proxy token的身份和生命周期约束。已提交 `08791acf104c73a2553ae1782c37c0018181a222`。完整census与canonical139沿已验收包；原636里实际受影响的qualified闭包锚点按同一原语句维护，其他fields保留。仅这六文件完成，不提升所有函数消费者或旧表全部行。

- AOT runtime：完整 2 文件/798 单元，已提交 `9223380440f9646a59537f2aa1ebe23be9edc012`。
- execution entry：完整 4 文件/44 单元，已提交 `732f2562298af75d2fd5c8fc7190e46eed72b9f4`。
- library Task runtime：完整 2 文件/139 单元，已提交 `ff90fd3dd89bc7d3ddb8837ba6db83a9d4ae1bcf`。
- task-frame fixture：完整 1 文件/75 单元，已提交 `172b27c3cfc2efe4e9d3dafa2fb29daa86bf0537`。
- checked integer fixtures：完整 2 文件/77 单元，已提交 `5fa5a513f1da87eba85a2ae41c6e2157835da59c`。
- layout fixture：完整 1 文件/31 单元，已提交 `71d8523b12acbf2538e670f0dae79a618687874b`。
- dense hash fixture：完整 1 文件/23 单元，已提交 `c70a88ec6cfeea15dcc0d57ede06cfa86beed5e2`。

AOT 两源采用完整 798 单元（H302/C496），不是原540局部台账；两份源码及四分区完整语义/有限修订相同。旧 reverse 的94行角色修正已提交 `afdbc15f4a860f826885e07b733c50bc22cea84a`，只授其明确差量，不提升其他1248历史消费者源码或旧BUG。AOT配置/模块借用寿命、帧刷新、清理/所有权包装与生成分派契约见[当前模块文档](../../library-and-builtins/aot-runtime-adapter.md)。

Execution 四源44单位解释入口帧恢复、staging诊断/布尔判定、Add/ToObject/ToStruct调用义务和context include桥；library Task 两源139单位解释cold Job、domain-root交接、provider接管与本地queue推进/失败消费边界。Task-frame75、checked77、layout31、hash23分别限制为完整夹具自身，不能扩为生产TaskFrame、算术生产实现、布局或hash模块全审。

## 验证范围

- Performance 原版/注释版既有 Windows Clang+LLD fixture 均通过；`/dev/full` 因 Windows 无该设备而跳过。
- Roots 和 young allocation 原版/注释版共四个既有 fixture 均通过编译、链接、运行（22 步），断言启用、输出为空，依赖在链接前与运行后相同。两个旧进程观察失败仍保留失败；新验证使用经独立复核的观察修订，要求同一已核实成员句柄 signaled 并成功读取退出码后才略过其映像名查询，随后继续等待本次 Job 自然归零。此处只授予实际夹具场景的验证；完整收集器、线程、OOM、Linux 和 CTest 未执行。
- Complex 原版/注释版回调及 registry 共四次编译通过；原版与注释版 registry 的七条既有警告保持相同。脚本、极端数值、OOM 和运行时未执行。
- Host/service、NLL、iteration、views 本轮完成静态意图复审，没有新增运行结果。
- ExecBC generator 原版/注释版以相同 schema/header 执行已有只读 `--check`，两次自然退出 0；仅证明文本 freshness，不覆盖默认写入/replace/OOM、C matcher 编译、native 或 CTest。
- Close-proxy fixture 原版/注释版已有两次 Windows C 编译通过；未链接或运行，不证明真实 AOT generated entry、移动 GC、异常清理或 CTest。人工 generatedFrame 与永久标记对象的限制继续保留。
- FFI runtime 原版/注释版已有两次 Windows C 编译通过，均禁用 libffi（`ZR_VM_HAS_LIBFFI=0`）；仅为 fallback 编译证据，不证明 foreign call、libffi closure、脚本或运行时。先前失败收据保留，本次维护不重跑编译。

- Core-close 原版/注释版三个 translation units 的既有六次 Windows 编译退出 0；只授编译范围，未链接/运行、CTest、真实移动 GC 或异常场景执行。
- Common 三头文件均有实际编译依赖 MD 使用证据。既有八步（四组原版/注释版）结果整体为失败，其中 AOT GC 两版均退出 1、各五个宏相关错误保留；错误数组相同仅属失败比较，不证明完整 stderr 相同。不能写成八步编译全部通过，也不授 ABI、native、runtime 或消费者整体闭合。本次库存维护没有新运行。

- AOT GC原版/旧43单位注释候选版的既有两次编译均失败退出1，各五条Unity/UCRT宏兼容错误；仅错误数组比较相同，非完整stderr身份或compile pass。当前44单位新增一条具体兼容TODO，保持code identity但未重跑，不授精确44源码编译、runtime、GC或CTest信用。

- Closure 本轮只继承完整静态契约审查、token/literal恒等与已提交事实，没有新增编译、运行、GC、异常重入、ABI或CTest结果。

新增 AOT798、execution44、library Task139及五夹具206只继承已接受静态全文/单元/caller契约和实际共享提交，本次没有运行 native、build、GC、ABI、generated entry、故障注入或CTest。Task-frame历史7/7属于旧快照而非当前注释运行；AOTGC旧43失败与新44未重跑事实保持。hash fixture reverse八行仍有session_checkpoint.c:478继承债，只source23与八处已修锚点被接受，不称旧八行全绿。

## 问题与台账限制

已有 BUG/TODO 保留其具体触发条件与核查入口：普通 performance 中位数分配失败可发布错误零峰值；NLL 夹具重复 LOAD result token 导致目标 LOAD 未建立；极大有限 Complex 平方范数溢出影响 magnitude/normalized。未核实的 GC 字节数组对齐、provider 注册/能力负例、view 初始化和字符串结果失败契约继续使用 TODO。

19 次外部 `ssa-tests.cmake` 锚点维护只移动至同一语句和上下文的新位置，16 个单位的其他字段及结论保持不变。Views 仅接受当前 C/H 的 29 单元；canonical 的旧 pooling 行及 native 表的旧空行锚点仍需其所属批次复审。Math 仅接受本批 Complex 33 单元，其他历史行的锚点格式问题仍在。

FFI 的路径复制失败与关闭后 getVersion fresh lookup 两项静态 BUG 保留；高索引、closed-as、typed read 容量、read 追加失败以及 write 前缀/截断继续 TODO。其他 FFI/native/aot 历史台账行未重新全审。

Core-close81 与 Common189 仅授六个完整 source/header 文件的静态覆盖；其他 core GC/resource 与 common 历史台账行不重新验收。AOT GC两文件44单位已纳入有限静态复审；closure六文件139单位已提交并纳入静态复审；本次已实际提交的AOT两源798、execution四源44、library Task两源139及五夹具206计入；coreframe152、checkpoint11、bindingguard43、async48、container357等本包未纳入的范围不提升。

## 当前清单

本次盘点 3659 个首方文件，其中 1228 个仍为 `pending`。扫描时间 2026-10-05T08:25:45.105784+00:00 至 2026-10-05T08:25:48.831640+00:00；只代表这一次currency观察，不保证foreign pending未来不变。这一统计绑定本批清单中的内容摘要；并发会话的新文件或后续变更会要求再次同步。整个工作区审查仍在进行。

纯台账修订已实际提交：tests `88c8c76`（11行13锚点）；common/views/FFI `3606a3c`（4行4锚点）；iteration/math/AOT `7fa037a`（16行21锚点）。Container此前已修11历史锚点；当前33加额外5处共38的修订不能推成其他历史表/旧BUG证明。归档H1089注释明确改H960实际声明，仅支持有限接口边界对照。

Coreframe阶段 `0f796dd` 对19外部行22同操作锚点的已审迁移，按实际before/after链更新本批Taskfixture75受影响字段；未改source/census/body，也不把该新生产source2计入本49范围。Binding/Container/Async/frame等不属本scope的文件仍只同步pendingcurrency，本次没有新native/runtime信用。

Function有限同operation台账阶段 `2bc3cb3` 已正常提交四表20行23字段、61处qualified引用。实际旧4214/当前4218是function_pre_call_native调用；旧4217 nativeFunction仅参数，prepared-frame经3843到受executionBudget条件控制的3630间接调用。用途独审与原六source194完整正文/census证明共同闭合，不能把每个非空参数当callee调用。function.c仅四insert raw恢复身份的有限观察，未授其外来实现/commit/runtime信用。r8因缺此证明HOLD保留。当前49source字节及原完整单元身份保持；新compilequeue/codehandle/indent阶段仅按实际before/after继承依赖角色，未加新whole文件。

Backend 阶段已实际正常提交 `aabb7da2664c988bd11e7aeaf31af681cbb9299a`。本49文件范围仅继承受影响fixture的同操作调用迁移及两项依赖守卫；test_destroy当前1577是非空destroy回调的条件派发，fixture233注册且179定义只记录事件、不释放userData。旧1549是旧source snapshot实际同一句，并非当前1549非代码位置。Backend source3/407不计入本49/1962。本49之外的历史reviewed状态不能自动解释为九字段/完整单元当前全部闭合；budget旧17/15不足29 identities的债不由本包消除。r9 CONTENT PASS/solecurrencyHOLD、错误初始Backend commit归属及原失败收据保留；b035a440只属于SSA RED测试提交。
