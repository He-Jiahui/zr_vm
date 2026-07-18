---
plan_id: aot-08-generics
record_id: 2026-07-19-constructed-generic-method-object
status: completed
completed_at: 2026-07-19 06:00:49 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 08-S6X Constructed Generic Method Object

## 状态与产出记录

- 完成时间：2026-07-19 06:00:49 +08:00
- 状态：08 泛型反射子里程碑完成；08 完整里程碑仍为部分完成。
- 完成项目：把已经精确解析的 MethodSpec carrier 物化为 GC 管理的 constructed generic method reflection object，
  保留 MethodSpec、MethodDef、argument signature 与 metadata runtime identity，并链接 open generic method definition。
- 计划映射：08-S6X；对应新计划里程碑 5“generic reflection/method invoke”。

## 产出

- 新增 `ZrCore_Reflection_BuildConstructedGenericMethodObject()`；构造前重新解析并逐项核对 carrier，拒绝篡改的
  token、record、hash、argument count、argument offset 或 request argument。
- 对象公开真实方法名、MethodSpec token、generic MethodDef token、递归 generic arguments、signature hash、runtime
  identity 和 `genericMethodDefinition` 链接，并区分 definition/constructed 标志。
- 反射对象字段写入、字符串创建和 GC 临时 pin/unpin 提取为共享 core-internal helper，generic type object 与 generic
  method object 使用同一套引用安全语义。
- 聚焦用例在根对象保留后执行 full GC，验证 arguments 与 definition 子图仍然可达。

## 验证

- RED：MSVC 编译成功，链接仅缺少 `ZrCore_Reflection_BuildConstructedGenericMethodObject`。
- GREEN：MSVC 19.44、GCC 11.4、Clang 14.0 的动态泛型反射测试均为 29/0。
- 三编译器聚焦 CTest 均为 6/6；共享回归均为 GC 66/0、instruction execution 31/0、instruction table 95/0。
- 新增反射对象实现和共享 helper 在 GCC/Clang 下无自身诊断；既有 computed-goto、const qualifier 等警告不由
  本切片引入。

## 未完成边界

- script-level `MakeGenericMethod` argument-object decoding 与 dispatch。
- 跨模块 generic method binding、invoke thunk 闭环和 closed-world full-AOT closure。
- 本切片不新增 metadata row/section，不改变 code-registration ABI。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6x-10-s4z45-11-s5c-constructed-generic-method-object.md`
