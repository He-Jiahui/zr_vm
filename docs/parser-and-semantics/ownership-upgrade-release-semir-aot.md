---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
plan_sources:
  - user: 2026-04-03 实现 ZR 三层 IR 与 Ownership/AOT 字节码重构方案，并继续推进剩余实现
  - .codex/plans/ZR 三层 IR 与 Ownership_AOT 字节码重构方案.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_execbc_aot_pipeline.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_dynamic_iteration_pipeline.c
  - tests/parser/test_meta_call_pipeline.c
  - tests/parser/test_tail_call_pipeline.c
doc_type: module-detail
---

# Ownership Upgrade And Release `SemIR -> ExecBC -> AOT`

## 目标

这一轮把 ownership 生命周期里此前缺失的两段显式语义补齐了：

- `%upgrade(weakRef)`
  - 从 weak 位点显式升级到 nullable shared
- `%release(owner)`
  - 显式释放当前 owner，并把源绑定清成 `null`

它们和 `%unique/%shared/%weak/%using` 一样，已经不是 helper-call 语法糖，而是稳定 semantic opcode。

## Parser / AST

`SZrConstructExpression` 现在新增 `builtinKind`，专门区分 ownership builtin 种类：

- `UNIQUE`
- `SHARED`
- `WEAK`
- `USING`
- `UPGRADE`
- `RELEASE`

这样 parser 不再只能靠 `ownershipQualifier + isUsing` 猜语义。

当前源级规则是：

- `%upgrade(expr)` 合法
- `%release(expr)` 合法
- `%upgrade new ...` 非法
- `%release new ...` 非法

`%release` 目前在 lowering 阶段还只支持 local identifier binding，这是刻意保守边界；在真正引入 `own_take_from_place` / `own_store_to_place` 之前，不去假装支持 member / index / closure-place release。

## ExecBC Lowering

执行层新增两个 dedicated opcode：

- `OWN_UPGRADE`
- `OWN_RELEASE`

lowering 规则：

- `%upgrade(expr)`:
  - 先把表达式结果编到参数槽位
  - 发射 `OWN_UPGRADE dst, arg`
- `%release(localId)`:
  - 直接引用 local binding 槽位
  - 发射 `OWN_RELEASE dst, local_slot`
  - runtime 同时把源 local 清成 `null`

因此 `%release` 没有再走“先 copy 到临时槽位再 release 临时值”的错误路径，避免 shared/weak copy 产生额外 ownership 副作用。

## Runtime Semantics

`ownership.c` 和 `execution_dispatch.c` 现在提供对应运行时行为：

- `ZrCore_Ownership_UpgradeValue`
  - 只接受 weak source
  - 当 strong ref 仍存在时，生成一个新的 shared 值
  - 当 weak 已失效时，返回 `null`
- `OWN_RELEASE`
  - 调用现有 `ZrCore_Ownership_ReleaseValue`
  - 原 source slot 被清空
  - 表达式结果也是 `null`

这让生命周期能够显式表达：

1. shared owner 存在
2. weak watcher 升级成功
3. owner 逐个 release
4. 最后一个 shared owner 消失后，weak 升级失败并返回 `null`

## SemIR / AOT Contract

`SemIR` 现在新增：

- `OWN_UPGRADE`
- `OWN_RELEASE`

语义映射保持显式 ownership effect：

- `OWN_UPGRADE`: `Weak -> Shared`
- `OWN_RELEASE`: `Shared -> PlainGc`

这里的 `OWN_RELEASE` 输入态目前仍是近似建模。它在执行时也可以作用于 `%unique/%using` local owner，但稳定 `SemIR` 先把它记录为“owner release to plain/null”这一类 effect。

`backend_aot.c` 同步把它们视为稳定 AOTIR opcode，并为文本 C / LLVM artifact 写出 runtime contract：

- `ZrCore_Ownership_UpgradeValue`
- `ZrCore_Ownership_ReleaseValue`

这保证：

- `ExecBC` 可以继续独立演进
- `SemIR` / AOT artifact 保持稳定 opcode 名字
- AOT 后端不需要重新发明 ownership 语义

## 当前边界

这一轮故意保留了几个限制：

- `%release` 目前只支持 local identifier binding
- `OWN_RELEASE` 还没有 place-aware 版本
- `BorrowShared/BorrowMut` 仍未和 `%upgrade/%release` 做更深的 borrow-check 联动
- AOT backend 当前只输出 textual runtime contract，不做 native inline drop lowering

这和计划文档一致：先立稳定语义契约，再做 place-aware ownership effect 和更激进的 AOT inline。

## 验证证据

本轮实际跑过的验证：

- WSL gcc Debug:
  - `zr_vm_parser_test`
  - `zr_vm_compiler_features_test`
  - `zr_vm_semir_pipeline_test`
  - `zr_vm_execbc_aot_pipeline_test`
  - `zr_vm_dynamic_iteration_pipeline_test`
  - `zr_vm_meta_call_pipeline_test`
  - `zr_vm_tail_call_pipeline_test`
- WSL clang Debug:
  - 同上 7 个目标
- Windows MSVC CLI smoke:
  - `build/codex-msvc-cli-debug`
  - `zr_vm_cli_executable`
  - `tests/fixtures/projects/hello_world/hello_world.zrp`

额外观察：

- `zr_vm_instructions_test` 在当前仓库基线上仍有一个与本次 ownership 变更无关的 `test_neg_generic` 失败；这次没有改动 `NEG` 路径，也没有把该失败混入 ownership 生命周期验收。
