---
plan_id: aot-10-reflection
record_id: 2026-07-19-generic-method-argument-object-decoding
status: completed
completed_at: 2026-07-19 06:46:41 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 10-S4Z47 Generic Method Reflection Object Decoder

## 状态与产出记录

- 完成时间：2026-07-19 06:46:41 +08:00
- 状态：10 脚本反射对象解码子里程碑完成；10 完整反射里程碑仍为部分完成。
- 完成项目：建立 method definition object + argument object array 到 constructed generic method object 的受检 C
  边界，并复用现有 recursive descriptor validator、MethodSpec resolver 与 object builder。
- 计划映射：10-S4Z47。

## 安全契约

- definition 必须带一致的 `kind` 与 generic/definition/constructed flags，并声明与数组一致的 arity。
- runtime 是受信显式参数；对象中的 `metadataRuntime` 只比较 identity，不作为可解引用来源。
- argument 的 kind string/value 必须一致，递归节点数和深度有界，数组必须连续且 element count 精确。
- 所有失败返回 null，不泄漏临时 arena，不生成 metadata 或 registration entity。

## 验证与边界

- MSVC RED 仅缺目标符号；MSVC/GCC/Clang GREEN 31/0，聚焦 CTest 6/6。
- 三编译器共享回归为 66/0、31/0、95/0；新增 decoder 无 GCC/Clang 自身诊断。
- native entry 栈协议、模块导出、错误对象/诊断映射和 Invoke 仍未完成。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6z-10-s4z47-11-s5e-generic-method-argument-object-decoding.md`
