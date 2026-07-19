---
plan_id: aot-08-generics
record_id: 2026-07-19-runtime-bound-reflection-module
status: completed
completed_at: 2026-07-19 09:50:58 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 08-S6AB Runtime-Bound Reflection Module

## 状态与产出记录

- 完成时间：2026-07-19 09:50:58 +08:00
- 状态：08 runtime-bound generic reflection module 子里程碑完成；08 generic reflection/method invoke 里程碑仍为部分完成。
- 完成项目：新增绑定真实 metadata runtime 的 `zr.reflection` 模块工厂，以唯一公共导出
  `MakeGenericMethod` 暴露 08-S6AA 的受信 native closure。
- 计划映射：08-S6AB；对应里程碑 5“generic reflection/method invoke”。

## 产出

- `ZrCore_Reflection_CreateModuleForRuntime()` 拒绝非 module-owned runtime，并创建 name/fullPath 均为
  `zr.reflection`、path hash 一致、状态为 READY 的模块对象。
- 模块本身不伪装成目标 metadata module；`hasMetadataRuntime` 保持 false，目标 runtime 只由导出 closure 的
  owner-backed module capture 绑定。
- `MakeGenericMethod` 是模块唯一 public export，调用仍只接收 method definition 与 argument array。
- 构造期间先以临时 ignore 保活目标 module；模块、名称和 closure 全部通过 VM stack roots 承担移动式 GC，只有
  导出安装和 capture 复核成功后才追加 `NATIVE_HANDLE` pin。

## 验证

- RED：公共声明和新测试在 MSVC 下编译通过，链接仅缺
  `ZrCore_Reflection_CreateModuleForRuntime` 一个符号。
- GREEN：动态泛型目标在 MSVC 19.44、GCC 11.4、Clang 14.0 下均为 33/0；新增源文件无 GCC/Clang 自身诊断。
- 最终 MSVC 聚焦 CTest 为 6/6；共享回归为 GC 66/0、instruction execution 31/0、instruction table 95/0。
- 独立审阅发现并闭合导出后 stale pointer、失败后遗留 pin、测试根化过晚三个 Important 问题；复审无剩余
  Critical/Important。

## 未完成边界

- 全局 native registry/cache 注册、同 runtime 去重、replacement/unload 与 generation policy。
- `zr.reflection.declaration`、generic invoke thunk、跨模块 method binding 与 full-AOT closure。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ab-10-s4z49-11-s5g-runtime-bound-reflection-module.md`
