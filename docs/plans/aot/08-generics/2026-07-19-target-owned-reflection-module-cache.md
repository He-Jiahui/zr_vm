---
plan_id: aot-08-generics
record_id: 2026-07-19-target-owned-reflection-module-cache
status: completed
completed_at: 2026-07-19 11:49:43 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 08-S6AC Target-Owned Reflection Module Cache

## 状态与产出记录

- 完成时间：2026-07-19 11:49:43 +08:00
- 状态：08 target-owned generic reflection service cache 子里程碑完成；08 generic reflection/method invoke
  里程碑仍为部分完成。
- 完成项目：新增按 target metadata module 持有的 `zr.reflection` service cache；同一 runtime 稳定复用，
  不同 runtime 隔离实例，污染缓存 fail-closed，失败不替换保留值。
- 计划映射：08-S6AC；对应里程碑 5“generic reflection/method invoke”。

## 产出

- `ZrCore_Reflection_GetOrCreateModuleForRuntime()` 只接受真实 module-owned metadata runtime，并把服务模块写入
  target module 的 protected export `__zr_reflection_service_module`，不暴露为脚本公共成员。
- 缓存命中必须重新验证 READY service identity、标准 path hash、唯一 `MakeGenericMethod` 导出、native entry、
  owner-backed capture、GC 属性和 target module identity；任何污染值均失败且不被静默覆盖。
- 新服务在 protected cache 安装和回读成功后才把 target 转换为 `NATIVE_HANDLE` pin；捕获/导出逃逸传播移除
  temporary ignore 时会立即恢复，失败路径保持 caller-owned ignore 与原始栈顶。
- target module 通过 protected export 拥有 service，service closure 通过 closed capture 拥有 target；缓存生命周期
  与 target runtime 绑定，不进入 process-global module path cache。

## 验证

- API RED：MSVC 编译通过后只缺 `ZrCore_Reflection_GetOrCreateModuleForRuntime` 链接符号。
- 审查 RED：protected map 在捕获完成后拒绝安装时，动态泛型套件为 34/1，暴露 caller-owned ignore 丢失；
  修复后同一故障路径和正常 pin 转换均通过。
- 最终动态泛型目标在 MSVC 19.44、GCC 11.4、Clang 14.0 下均为 34/0；本次反射源码 GCC/Clang
  warning 为 0。
- 最终 MSVC 聚焦 CTest 为 6/6；共享回归为 GC 66/0、instruction execution 31/0、instruction table 95/0。
- 独立审查闭合 temporary-ignore ownership、open capture、非字符串模块字段、cache-slot flags 和 prototype
  fallback 五个 Important 问题；对称自查补齐 export-slot GC flag，最终复审无剩余 Critical/Important。

## 未完成边界

- 普通 `import zr.reflection` 的 caller-context bridge、global native registration 和 path-cache 集成。
- replacement/unload、module generation policy、`zr.reflection.declaration`、generic invoke thunk 与 full-AOT closure。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ac-10-s4z50-11-s5h-target-owned-reflection-module-cache.md`
