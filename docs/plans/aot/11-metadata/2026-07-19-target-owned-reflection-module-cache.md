---
plan_id: aot-11-metadata
record_id: 2026-07-19-target-owned-reflection-module-cache
status: completed
completed_at: 2026-07-19 11:49:43 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
evidence_scope: sub-milestone
---

# 11-S5H Target-Owned Reflection Service Identity

## 状态与产出记录

- 完成时间：2026-07-19 11:49:43 +08:00
- 状态：11 target-owned reflection service identity 子里程碑完成；11 ModuleIdentity/registration 里程碑仍为
  部分完成。
- 完成项目：以 target metadata module 作为 runtime-specific service cache owner，保持 target、service、
  metadata runtime 与 process-global path identity 的边界。
- 计划映射：11-S5H。

## Metadata 与生命周期约束

- 缓存不创建或修改 `.zrp` row、token、signature、code-registration slot、manifest export 或 metadata runtime；
  service module 继续保持 `hasMetadataRuntime == false`。
- runtime 必须与 target module 双向一致；protected cache 中的 service closure capture 必须回指同一 target module，
  不以裸 runtime pointer 作为持久 identity。
- target protected export 提供 service reachability；service closed capture 提供 target reachability。成功缓存后 target 的
  `NATIVE_HANDLE` 保证嵌入式 runtime pointer 地址稳定。
- temporary ignore 在 capture close 和 export escape 后恢复；失败时保留调用前 ownership，成功时才由 GC pin 语义
  移除 ignore 并转换为 pinned region。

## 验证与未完成边界

- 最终 MSVC/GCC/Clang 动态泛型 34/0，本次源码 GCC/Clang warning 为 0。
- 最终 MSVC 聚焦 CTest 6/6；共享回归 GC 66/0、instruction execution 31/0、instruction table 95/0。
- 故障注入验证 protected cache 安装失败不丢 caller-owned ignore、不遗留 cache/pin 且恢复栈顶。
- 污染测试覆盖 cache/export value flags、module string fields、prototype fallback 和 open capture；独立复审确认
  无剩余 Critical/Important。
- Canonical ModuleIdentity、global registration、runtime replacement/unload、module generation 和 artifact ABI
  仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ac-10-s4z50-11-s5h-target-owned-reflection-module-cache.md`
