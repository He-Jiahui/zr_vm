---
plan_id: aot-11-metadata
record_id: 2026-07-19-generic-method-native-entry
status: completed
completed_at: 2026-07-19 08:57:21 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
evidence_scope: sub-milestone
---

# 11-S5F Runtime-Bound MethodSpec Native Callable

## 状态与产出记录

- 完成时间：2026-07-19 08:57:21 +08:00
- 状态：11 runtime-bound MethodSpec callable 子里程碑完成；11 统一 artifact 里程碑仍为部分完成。
- 完成项目：为现有 MethodSpec consumer 提供 runtime-bound native callable，受信 metadata module 由 GC-owned
  closure owner 保存，runtime 从 module 派生且不作为脚本参数或持久 metadata identity。
- 计划映射：11-S5F。

## Metadata 与生命周期约束

- closure capture 保存进程内 module object，不保存裸 runtime pointer；成功闭合后 module 以 `NATIVE_HANDLE`
  固定为不可移动对象，持久 identity 仍由 module/token/signature carrier 决定。
- closure factory 不创建 MethodSpec row、signature、token record、cache 或 code-registration slot。
- 分代 full GC 可重写 capture owner 并沿 capture 保活已固定 module；entry 每次通过 owner-aware accessor 获取
  module，再读取其当前 runtime，不保存栈地址或可移动的嵌入式 runtime 地址。
- `.zrp`/`.zro` schema、metadata table 和 code-registration ABI 均未改变。

## 验证与未完成边界

- 最终实现的 MSVC/GCC/Clang 动态泛型 32/0，新实现无 GCC/Clang 自身诊断。
- 最终实现的 MSVC 聚焦 CTest 6/6、共享回归 66/0 + 31/0 + 95/0；GCC/Clang 最终广域复跑因 WSL
  重启并清空隔离目录而未完成，修复前对应矩阵曾通过。
- runtime-specific module registration、module unload/refresh 生命周期和 Canonical ModuleIdentity 仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6aa-10-s4z48-11-s5f-generic-method-native-entry.md`
