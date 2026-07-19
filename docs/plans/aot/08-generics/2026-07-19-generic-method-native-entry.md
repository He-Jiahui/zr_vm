---
plan_id: aot-08-generics
record_id: 2026-07-19-generic-method-native-entry
status: completed
completed_at: 2026-07-19 08:57:21 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 08-S6AA Generic Method Native Entry

## 状态与产出记录

- 完成时间：2026-07-19 08:57:21 +08:00
- 状态：08 generic method native 调用入口子里程碑完成；08 完整里程碑仍为部分完成。
- 完成项目：新增只接收 method definition 与 argument array 的 native stack entry；宿主创建的 native closure
  捕获并保活 metadata module，脚本参数不携带可伪造的 runtime pointer。
- 计划映射：08-S6AA；对应新计划里程碑 5“generic reflection/method invoke”。

## 产出

- `ZrCore_Reflection_CreateMakeGenericMethodNativeClosure()` 只接受真实 module-owned runtime，并把 metadata module
  OBJECT 保存在 GC-owned、已关闭的 `SZrClosureValue` 中；capture 绑定成功后再以 `NATIVE_HANDLE` 固定模块。
- owner-backed capture 不保留栈上 direct pointer；模块进入 `OLD_PINNED`/`PINNED`，分代 full GC 后仍存活且地址
  稳定，entry 从模块取得当前受信 runtime。
- `ZrCore_Reflection_MakeGenericMethodNativeEntry()` 验证 native function identity、单捕获 module owner、精确两参数
  和 OBJECT/ARRAY shape，再调用 08-S6Z 的有界对象解码边界。
- 所有受检失败路径返回一个 `null`，并把 `stackTop` 归一化为 `functionBase + 1`。

## 验证

- RED 1：MSVC 链接只缺 native entry 与 closure factory 两个目标符号。
- RED 2：owner-backed capture 的 direct pointer 在 full GC 后与 owner value 不一致，测试稳定 31/1；工厂改为
  owner-only capture 后 GREEN。
- RED 3：旧 factory 接受未由 GC module 拥有的 fixture runtime，测试稳定 31/1；改为 module capture 后 GREEN。
- RED 4：仅捕获 module 仍未声明嵌入式 runtime 与 native carrier 所需的不可移动性，固定断言稳定 31/1；成功
  闭合 capture 后追加 `NATIVE_HANDLE` pin，并以分代 full GC 验证后 GREEN。
- 最终实现的 MSVC/GCC/Clang 动态泛型均为 32/0；新 native entry 源文件无 GCC/Clang 自身诊断。
- 最终实现的 MSVC 聚焦 CTest 为 6/6，共享回归为 GC 66/0、instruction execution 31/0、instruction table 95/0。
- 生命周期修复前，三编译器曾完成 6/6 与 66/31/95；修复后 WSL 重启并清空隔离目录，GCC/Clang 最终广域
  复跑未完成，不计为最终通过证据。

## 未完成边界

- `zr.reflection` 模块创建、`MakeGenericMethod` 导出和 runtime-specific registration 生命周期。
- 跨模块 method binding、invoke thunk、错误对象映射与 full-AOT closure。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6aa-10-s4z48-11-s5f-generic-method-native-entry.md`
