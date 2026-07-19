---
plan_id: aot-11-metadata
record_id: 2026-07-19-reflection-caller-import-bridge
status: completed
completed_at: 2026-07-19 15:29:57 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
evidence_scope: sub-milestone
---

# 11-S5I Caller-Owned Reflection Runtime Resolution

## 状态与产出记录

- 完成时间：2026-07-19 15:29:57 +08:00
- 状态：11 caller-owned reflection runtime resolution 子里程碑完成；11 Canonical ModuleIdentity/registration
  里程碑仍为部分完成。
- 完成项目：以 loaded module metadata runtime 与 caller canonical owner root 的一致性解析 reflection service，
  保持 target module、service module、runtime 和 process-global path identity 的边界。
- 计划映射：11-S5I。

## Metadata 与 identity 约束

- bridge 不创建或修改 `.zrp` row、token、signature、manifest export、metadata runtime 或 code-registration slot；
  service module 继续保持无 metadata runtime。
- caller identity 由 forwarding-aware owner chain 归一化；loaded runtime 必须反向指向 registry module 且具有
  code registration，匹配结果必须唯一到一个 module identity。
- registry 结构校验覆盖 key/value type、GC/native flags、raw-object type、module internal type、遍历计数与链界限；
  malformed 数据不能被跳过后继续产生看似唯一的匹配。
- service 仍由 target protected export 持有，不注册到 process-global path cache；alias 只为同一 module identity
  提供多个查找入口，不产生第二个 service identity。

## 验证与未完成边界

- OOM full-GC 测试使用 GC-managed metadata function、VM stack-rooted post-GC caller 和 stack anchor，验证 loader
  在 bridge 分配后重新获取 caller 并执行 import signature effect。
- 最终三编译器动态泛型为 35/0，MSVC 聚焦 CTest 6/6，共享回归为 66/0、31/0、95/0。
- 独立复审最终无 Critical/Important。
- Canonical ModuleIdentity artifact、global native registration、runtime replacement/unload、module generation 与
  public contract hash integration 仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ad-10-s4z51-11-s5i-reflection-caller-import-bridge.md`
