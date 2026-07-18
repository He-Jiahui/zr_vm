---
plan_id: aot-08-generics
record_id: 2026-07-19-generic-method-argument-object-decoding
status: completed
completed_at: 2026-07-19 06:46:41 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 08-S6Z Generic Method Argument Object Decoding

## 状态与产出记录

- 完成时间：2026-07-19 06:46:41 +08:00
- 状态：08 generic method 对象参数解码子里程碑完成；08 完整里程碑仍为部分完成。
- 完成项目：新增 `ZrCore_Reflection_MakeGenericMethodFromObjects()`，把 generic method definition reflection object
  和 generic argument object array 解码为现有 descriptor vector，再调用精确 MethodSpec make boundary。
- 计划映射：08-S6Z；对应新计划里程碑 5“generic reflection/method invoke”。

## 产出

- 支持 primitive、TypeDef/TypeRef/TypeSpec token、array、tuple、ownership、nullable 和 union 的递归对象 schema。
- 两遍解码先验证/计数，再按精确节点数分配临时 arena；深度限制 64，总节点限制 1024，完成后立即释放。
- definition 与 arguments 在字段读取和对象构造期间临时 pin；输入 runtime 必须由调用方提供并与对象 carrier 相等，
  不解引用脚本对象中的未认证 native pointer。
- kind string/value 不一致、primitive 越界、runtime mismatch、constructed object 冒充 definition、非数组和空输入均
  fail closed。

## 验证

- RED：MSVC 链接仅缺 `ZrCore_Reflection_MakeGenericMethodFromObjects`。
- GREEN：MSVC/GCC/Clang 动态泛型均为 31/0，聚焦 CTest 均为 6/6。
- 最终源码在三编译器共享回归均为 GC 66/0、instruction execution 31/0、instruction table 95/0。
- decoder 无 GCC/Clang 自身诊断；测试拆分后主 method-context 头为 784 行。

## 未完成边界

- native stack entry、受信 runtime 的模块注册上下文和 `zr.reflection` 导出。
- 跨模块 method binding、invoke thunk 与 full-AOT closure。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6z-10-s4z47-11-s5e-generic-method-argument-object-decoding.md`
