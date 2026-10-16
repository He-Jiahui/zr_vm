---
related_code:
  - zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_type_object.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_type_object.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
plan_sources:
  - user: 2026-10-16 active BUG repair and necessary setter API migration
tests:
  - tests/ffi/test_setter_caller_cleanup.c
  - tests/cmake/setter-caller-cleanup-tests.cmake
  - tests/module/test_reflection_dynamic_generic_instance.c
  - tests/module/test_reflection_dynamic_generic_method_context.h
doc_type: failure-contract
---

# Reflection 与 FFI 字段写入调用方清理

Object 与 Library 提供 bool Try setter，以及失败时抛出异常的 void setter。
持有局部根或临时调用栈锚点、并以 bool 报告失败的构造函数使用 Try setter，
使字段写入失败先走正常清理路径，再把 false 交给上层；不能通过调用 void setter
后读取线程状态来实现这个约定，因为异常跳转会越过局部清理代码。

## Reflection 数组追加

私有 `generic_type_object_array_push` 通过 `ZrCore_Object_TrySetValue` 追加参数对象。
它保存写入结果，再释放本次为 value 和宿主数组新增的两个显式根，最后返回该结果。
原已存在的显式根不由本 helper 撤销。参数校验、数组 materialize 和建立根失败的
既有路径保持原有清理约定；此修改不引入新的公开测试桥接接口。

公开泛型类型、泛型方法定义及方法上下文构造调用方原已检查该 helper 的 bool，
因此失败会进入其已有上层清理，而不继续交付遗漏的参数节点。

## FFI 聚合转换

`zr_ffi_struct_to_object` 建立 `ZrLibTempValueRoot` 保护构造中的对象。每个字段使用
`ZrLib_Object_TrySetFieldCString`；写入失败时先结束临时根并恢复调用栈锚点，再返回
false。所有字段成功后才写入调用方的 result 槽，因而失败不发布部分聚合对象。
这适用于共享该转换入口的 native 返回值、ref/out 写回及回调参数。

此变更仅迁移字段 setter 的失败传播；不宣称所有递归标量转换、字符串转换或
用户代码异常均已获得新的事务保证，也不改变既有字段的复制及所有权语义。

## 验证边界与证据

独立目标 `zr_vm_setter_caller_cleanup_test` 使用真实 Core/Library/FFI，以及与实际
FFI 目标一致的 `ZR_VM_HAS_LIBFFI` 和 ffi.h ABI。CTest 名称为
`ffi_setter_caller_cleanup_field_pair_oom`。夹具预热并保活字段字符串，以持续拒绝
准确的 HASH_PAIR 分配命中聚合字段写入；检查 bool false、输出槽保持、stackTop、
调用帧锚点、GC scope、异常恢复链、根数量及撤销故障后的同入口重试，销毁后检查
分配尺寸、类别和零残留。

Root 实际 `integrated-caller-numeric-red-r2` 在 configure/build 成功且 226 个输入
`changed_inputs=[]` 后完成。FFI 用例的正常 bool 返回失败、stackTop 恢复和重试根
平衡三项断言失败，其他 scope/AOT/anchors/root/teardown 检查通过。修复在该 RED
之后写入；support-batch-5-green 实际重编译后该 FFI 用例和 Reflection 正常集成均通过，1929 个输入哈希无变化。

Reflection 正常集成目标为 `zr_vm_reflection_dynamic_generic_instance_test`，CTest
为 `reflection_dynamic_generic_instance`，覆盖公开泛型类型/方法参数及上下文构造。
本组没有新增针对私有 append 的直接 OOM 注入，也未为测试暴露其内部接口。
