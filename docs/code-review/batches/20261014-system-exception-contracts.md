---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/exception.h
  - zr_vm_lib_system/include/zr_vm_lib_system/exception_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_core/src/zr_vm_core/reflection/field/reflection_construction.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/session/session_checkpoint_capture.c
  - zr_vm_core/src/zr_vm_core/session/session_checkpoint_restore.c
  - zr_vm_core/src/zr_vm_core/value/value.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
  - tests/harness/runtime_support.c
  - tests/exceptions/test_exceptions.c
  - tests/module/test_module_system.c
  - tests/system/test_system_fs_module.c
  - tests/cmake/compiler-project-targets.cmake
  - zr_vm_common/include/zr_vm_common/zr_array_conf.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/src/zr_vm_core/function/function.c
  - zr_vm_core/src/zr_vm_core/runtime/state.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - zr_vm_core/include/zr_vm_core/closure.h
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/exception.h
  - zr_vm_lib_system/include/zr_vm_lib_system/exception_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 System.Exception callback contract audit and documentation work
tests:
  - tests/exceptions/test_exceptions.c
  - tests/module/test_module_system.c
  - tests/system/test_system_fs_module.c
doc_type: review-batch
---

# System.Exception 叶模块契约审查

## 范围与单元台账

本批审查 Exception 叶模块的四个源码文件，并在 `docs/code-review/coverage/zr_vm_lib_system_exception.tsv` 登记 21 个唯一源单元：`exception.h` 3 项、`exception_registry.h` 2 项、`exception.c` 6 项、`exception_registry.c` 10 项。条目包含两个常规 include guard、两个公开 callback 与 registry getter、两个私有 helper、有条件的计数宏 fallback、各 descriptor 表，以及两个行为块。判定为 15 项 `commented`、2 项 `no-comment`、3 项 `TODO`、1 项 `BUG`。

System 模块在 `module.c:88` 获取并注册叶模块 descriptor，随后在 `module.c:108` 导入叶模块以创建 Error 原型。异常构造 callback 作为 meta-method 绑定给 Error 和六个派生异常类型，接受 0..1 个参数；native dispatcher 在进入 callback 前核对注册元数，并同步调用绑定函数。构造 callback 把 result 设为 VM null；反射构造流程仍以 receiver 作为新对象返回。

本地 `ZR_ARRAY_COUNT` 仅是备用定义。产品 include 链从 `exception_registry.h` 经 `exception.h`、System conf、library conf 到 `zr_vm_common.h`，先引入 `zr_vm_common/zr_array_conf.h:13` 的共享定义。因此产品构建路径不会启用本地 fallback，描述符中的计数表达式使用共享宏。

## 审查发现

构造器写入 `stacks` 字段存在一条范围明确的静态软失败路径。反射构造在调用 constructor 前 pin 新 receiver（`reflection_construction.c:760-770`、`:822-823`）。若 ignored-root 数量原为 `capacity - 1`，receiver pin 会填满表。`ZrLib_Array_New` 通过 `Object_NewCustomized` 和 `RawObject_New(..., false)` 创建数组（`native_binding_dispatch.c:1356-1365`、`object.c:1482-1507`、`gc_object.c:1069-1111`），不会增加 ignored root。之后 void 字段 setter 先 pin 已有 root 的 receiver，再尝试 pin 新数组（`native_binding_dispatch.c:1368-1413`、`native_binding_dispatch_lanes.h:358-363`）。若这一次 `RawMallocWithType(ARRAY)` 扩容分配失败，ignored-registry helper 返回 false 且不设置 callback 错误状态（`gc.c:696-705`、`gc_object.c:745-748`）；void setter 就此返回且不写字段。callback 继续执行并返回 true/null，而反射构造只检查调用和线程状态、忽略 callback result，最后仍交付 receiver（`reflection_construction.c:856-870`）。在该次分配失败、后续分配成功的限定条件下，成功返回的 Error 派生对象可能缺少 `stacks`。这是闭合的静态调用路径，不是故障注入实测，也不支持对任意 OOM 作泛化结论。

未处理异常 handler callback 经 ReadFunction 接受 FUNCTION、CLOSURE 或 NATIVE_POINTER，并把该值存入 global slot；注册不校验 hint 的 `(Error)->bool` 签名或原生指针地址。实际调用原生指针须遵守 `FZrNativeFunction` ABI（`value.h:15`、`function/function.c:4273`），原生指针与 native closure（`closure.h:57`）所指向的代码和外部资源须在 global 槽仍可能引用时保持有效，GC 不持有代码映像。GC 标记/搬迁会处理该 slot；checkpoint 仅接受可恢复 VM 函数/闭包及对象图（capture.c:35-84、:481-505、:884-888），NATIVE_POINTER 或 native closure 会使 capture 失败，不会产生持有其代码指针的成功快照。仓库内唯一找到读取并调用它的入口是测试 harness（`runtime_support.c:213-247`，由 `:265` 调用）。CLI 和 REPL 的生产异常退出路径会规范化、记录并重置异常，但不读取这个 slot（`runtime.c:367-380`、`repl_session.c:38-60`）。本批将“handler 派发属于 embedding host，还是也应由 CLI/REPL 接入”保留为 TODO；不据此称 API 已死或已证实存在缺陷。

共享 handler 槽还存在待核实的跨 mutator 协调边界：`State_New(global)` 可创建附加 state（`runtime/state.c:61`），`State_MutatorLaunch` 注册运行 mutator（`:173`），native callback 的 `NativeEnter/Leave` 不构成该槽的专用原子替换协议（`native_binding_dispatch_lanes.h:263-279`）。注册实现普通写 handler 和 flag（`exception.c:113-114`），测试宿主普通读取它；公开接口明确不提供跨 mutator 原子替换/并发派发保证。下一步须沿该入口与实际宿主协调协议核实，保留为接口 TODO，不断言所有 global API 均须宿主串行，也不标已证并发 BUG。两个公开 callback 同时补明 result 必须非空可写、正常返回 false 前保留输出槽；构造失败并不承诺撤销 receiver 已有字段写入。

`kErrorFields.message` 声明 `string`，但零参 constructor（`exception.c:78-81`、`:152`）和 Core 缺省消息路径（`exception/exception.c:264-277`）可写 VM null。反射保存 descriptor typeName（`native_binding_metadata.c:267`），LSP 再投影类型文本（`metadata/lsp_metadata_provider.c:1763`、`lsp_native_declaration_projection.c:500`）。**TODO:** 沿这四个入口确认应改 nullable 描述，还是改变缺省消息策略；本轮只记录，不判为已证运行期 BUG。

## 现有测试证据与边界

`tests/cmake/compiler-project-targets.cmake:172-182` 定义 `zr_vm_exceptions_test`；`test_exceptions.c:560-574` 注册了 15 个 case。`test_derived_exception_prefers_first_matching_catch_clause` 构造 `new exception.RuntimeError("boom")` 并断言 typed catch 的顺序（`:221`、`:226`、`:245-246`），但不检查 constructor 写入的字段。`test_throw_string_is_boxed_and_caught_by_base_error` 覆盖 Core 对 throw 值的规范化（`:186`、`:190`、`:210-211`）；`test_caught_error_exposes_stack_frames_in_throw_order` 检查规范化后的 stack-frame 顺序和 source identity（`:468`、`:480-482`、`:500-501`）；它们都没有覆盖 constructor setter 失败。

`test_system_leaf_modules_expose_new_api_and_owned_types` 检查 Exception 导出，包括 `registerUnhandledException`（`tests/module/test_module_system.c:3128`、`:3193-3201`、`:3341-3342`），但不会调用该 callback。Filesystem 测试验证文件系统操作生成的 IOException，不是直接经本叶 callback 构造异常。已查阅路径中没有 Exception constructor 字段写入失败注入，也没有脚本级 handler 注册后派发的 case。

上述共享测试源码只为确认相关入口和断言而只读查看。本批不声称这些测试文件已完成文档级审查，不把其单元加入本叶台账，也不修改它们；Producer 未执行测试，Root 的现有测试执行记录另见下文；其 canonical test-unit 归属仍由对应测试审查批次负责。

## 验证状态

本批四份源码只改注释，不改签名、运行逻辑或测试。Root 另维护两个 wiki 的必要契约更正、旧 System 台账选中 20 行的迁移及 native 公共台账的两个 Exception caller 锚点；其余共享测试单元仍未在此批关闭。

已完成的历史验证为 `exception-matrix3x3-final-r3-comment-equivalence.json` 与 `exception-matrix3x3-final-r2-results.json`：当时八源码严格非注释 token/标志等价，ClangCL Debug/static guarded build 成功，module 78 项中 70 PASS、8 FAIL，provider 9/9 PASS，Exception 15/15 PASS；三套逐 case 行都与 r1 相同，module/provider 也与更早 r3e 相同。该轮 2951 个保守编译/配置/指定样例输入及冻结源在执行期间稳定。其后独立复核合并了 checkpoint 可恢复条件、native closure 代码映像有效期、message nullable 元数据 TODO 及参数错误 noreturn 的注释精度问题。

最终四源码的严格核验见 `system-exception-final-r4-comment-equivalence.json`（SHA-256 `d3f1aab8568db7979a6dfc8a37329840db6c12ef35b738d246d4ef7e644b011a`）：相对 `d45b037e4d87e77af85bc4ef849cfed7659ec59d` 的非注释 token kind、spelling、StartOfLine、LeadingSpace 完全一致，八个 lexer 子进程均已等待退出。源码没有签名或行为变更。

最后 guarded ClangCL/原生 ml64、Debug/static 增量构建成功（20.172 秒），实际重编两个 Exception 实现；工具参数及缺省 initializer 警告保留。`system-exception-final-r3-results.json` 中 module exit 8，78 项为 70 PASS/8 FAIL；provider exit 0，9/9 PASS；Exception exit 0，15/15 PASS。三套逐 case 行与 r2 全部相同，module/provider 也与原始 r3e 全部相同；完整比较见 `system-exception-final-r3-comparison.json`（SHA-256 `6ac191e9858ff850af938a5431c1126f2acdd915db8d8dd35416b8560f1378fb`）。四个 native 子进程均已等待退出，2951 个保守编译/配置/指定样例输入及最终四源在执行期间稳定，r2 至该轮的输入变化只涉及本任务四份注释源码（其中包含另一批的 Matrix3x3 header），没有同期外来输入漂移。

当前只有 module/provider 的原始修改前基线；Exception r1/r2 是修改后历史执行，不能称为隔离的修改前后专项对比。更早轮次有外来 Core/Parser 改动及工具差异，保守输入也不涵盖全部生成/运行时输入或外部 SDK。本批保留 8 个既有 module 失败，未作行为修复；没有新增测试，也未执行分配故障注入、脚本级 handler 派发、并发 mutator 或 checkpoint handler 专项。`check-batch` 通过仅表示格式/路径/非空锚点门通过；契约真实性、调用链和完整枚举另由 Root 与独立 reviewer 核查。
