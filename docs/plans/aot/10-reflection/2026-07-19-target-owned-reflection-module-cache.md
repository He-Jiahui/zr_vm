---
plan_id: aot-10-reflection
record_id: 2026-07-19-target-owned-reflection-module-cache
status: completed
completed_at: 2026-07-19 11:49:43 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 10-S4Z50 Target-Owned zr.reflection Service Cache

## 状态与产出记录

- 完成时间：2026-07-19 11:49:43 +08:00
- 状态：10 target-owned `zr.reflection` service cache 子里程碑完成；10 完整反射模块与 API 里程碑仍为部分完成。
- 完成项目：把 runtime-bound `zr.reflection` 工厂提升为 target-owned get-or-create 服务，建立稳定复用、
  runtime 隔离、受保护缓存和污染值 fail-closed 契约。
- 计划映射：10-S4Z50。

## 模块与缓存契约

- service 仍只有公共 `MakeGenericMethod`，自身不附加 metadata runtime；缓存键只存在于 target module 的
  protected export map，public lookup 不可见。
- 同一 target runtime 重复调用返回同一 service object；不同 target runtimes 返回不同 service objects；
  generational full GC 后通过 target protected cache 恢复相同 identity。
- 缓存校验覆盖模块种类、READY 状态、name/fullPath/pathHash、公共导出数量、native closure 函数和 owner-backed
  capture 形状。保留键被非模块值或伪造服务占用时直接返回 null，不自动替换。
- 捕获关闭和 module-root escape 都可能取消 temporary ignore；实现逐层恢复 ignore 所有权，缓存安装失败时不泄露
  pin、不改变调用方 root ownership，成功后才转换为 `NATIVE_HANDLE`。

## 验证与边界

- TDD API RED 为单一缺失 get-or-create 符号；审查 RED 为 post-capture cache-install failure 34/1。
- 最终 MSVC/GCC/Clang 动态泛型均为 34/0；MSVC 聚焦 CTest 6/6，共享回归 66/0 + 31/0 + 95/0。
- 独立审查发现并闭合五个 Important：temporary-ignore ownership、open stack-backed capture、非字符串模块字段、
  cache-slot flags 和 prototype fallback；对称自查补齐 export-slot GC flag，最终复审无剩余 Critical/Important。
- 普通 `import zr.reflection`、declaration 分类、createInstance/property/invoke API、global registration、
  replacement/unload 仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ac-10-s4z50-11-s5h-target-owned-reflection-module-cache.md`
