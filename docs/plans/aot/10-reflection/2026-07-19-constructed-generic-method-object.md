---
plan_id: aot-10-reflection
record_id: 2026-07-19-constructed-generic-method-object
status: completed
completed_at: 2026-07-19 06:00:49 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 10-S4Z45 Constructed Generic Method Object

## 状态与产出记录

- 完成时间：2026-07-19 06:00:49 +08:00
- 状态：10 动态泛型方法反射对象子里程碑完成；10 完整里程碑仍为部分完成。
- 完成项目：从经过 revalidation 的 MethodSpec resolution carrier 创建 constructed method object，并把真实方法
  definition、具体参数和 metadata identity 暴露为稳定的反射字段。
- 计划映射：10-S4Z45；对应公开反射计划的 token/TypeId 定位和 generic method invoke 前置对象契约。

## 行为契约

- 仅消费 attached runtime 中已经存在并能精确重新解析的 MethodSpec，不按名称近似匹配，不生成 metadata entity。
- `kind=constructedGenericMethod`，真实 `name` 来自 MethodDef definition object；definition、constructed 和 generic
  flags 保持互斥一致。
- `genericMethodDefinition`、`genericArguments`、MethodSpec/MethodDef token、signature hash 与 runtime identity 同时
  保留；篡改 carrier 或参数后 fail closed。
- 共享对象 helper 在创建 key/value 字符串和嵌套对象字段期间保持 GC roots，对象图通过 full GC 聚焦验证。

## 验证与边界

- MSVC/GCC/Clang 动态泛型反射 29/0，聚焦 CTest 6/6。
- 三编译器 GC、instruction execution、instruction table 分别为 66/0、31/0、95/0。
- script `MakeGenericMethod`、object argument decoding、method invoke thunk、跨模块 method identity 和 trimming
  diagnostic 仍未完成。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6x-10-s4z45-11-s5c-constructed-generic-method-object.md`
