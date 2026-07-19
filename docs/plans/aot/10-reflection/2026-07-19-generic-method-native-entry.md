---
plan_id: aot-10-reflection
record_id: 2026-07-19-generic-method-native-entry
status: completed
completed_at: 2026-07-19 08:57:21 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 10-S4Z48 Trusted MakeGenericMethod Native Dispatch

## 状态与产出记录

- 完成时间：2026-07-19 08:57:21 +08:00
- 状态：10 generic method native dispatch 子里程碑完成；10 完整反射里程碑仍为部分完成。
- 完成项目：把 10-S4Z47 的对象级 make 边界接入 VM native call frame，同时把受信 metadata module 从脚本参数
  移入宿主创建的 closure capture，由 entry 从 module 派生 runtime。
- 计划映射：10-S4Z48。

## 安全与栈契约

- callable 必须是指向目标 entry 的 native closure，且必须包含一个由 `SZrClosureValue` owner 持有的 metadata
  module OBJECT capture；factory 拒绝非 module-owned runtime，并在 capture 成功闭合后以 `NATIVE_HANDLE` 固定
  module，保证嵌入式 runtime 与 reflection carrier native pointer 的地址稳定。
- frame 必须精确包含两个参数：generic method definition OBJECT 与 generic argument ARRAY。
- definition carrier 中的 runtime 只与捕获 module 当前持有的受信 runtime 比较，不作为解引用来源。
- 无捕获、错误捕获类型、错误参数数量、非数组参数和底层 MethodSpec mismatch 均 fail closed 为单个 `null`。

## 验证与边界

- 四轮 RED 分别证明缺少入口符号、direct stack capture 在 full GC 后不能作为稳定地址、裸 runtime capture
  不能证明 GC module 所有权，以及仅保活但可移动的 module 不能满足 runtime/carrier 地址稳定契约。
- 最终实现的 MSVC/GCC/Clang 动态泛型 32/0；MSVC 聚焦 CTest 6/6、共享回归 66/0 + 31/0 + 95/0。
- 生命周期修复后的 GCC/Clang 广域复跑因 WSL 重启并清空隔离目录而未完成；修复前对应矩阵曾通过。
- `zr.reflection` 模块导出、序列化 helper id、脚本级错误对象和 Invoke 仍未完成。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6aa-10-s4z48-11-s5f-generic-method-native-entry.md`
