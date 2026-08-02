---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_ownership.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-04-08 Rust-First Ownership / GC 分层设计
  - .codex/plans/Rust-First Ownership  GC 分层设计.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_resource_unique_drop.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - tests/task/test_task_runtime.c
doc_type: module-detail
---

# Ownership Builtins `SemIR -> ExecBC -> AOT`

## 目标

ownership surface 现在按 Rust-first 方式收敛成 canonical type/member/reference surface 和 dedicated opcode。百分号 ownership 形式只存在于迁移诊断与负向测试。

当前 ownership/reference surface 是：

- `own T(...)`
- `owner.share()`
- `shared.weak()`
- `weak.upgrade()`
- `var view: ref readonly T = ref owner`
- `var view: ref T = ref owner`
- `owner.intoGc()`
- `drop(owner)`

普通 `using` statement 只承担受支持的 statement/block lifetime contract，不构造 owner。

## Parser / AST

`SZrConstructExpression` 现在通过 `builtinKind` 区分 ownership builtin：

- `UNIQUE`
- `SHARED`
- `WEAK`
- `BORROW`
- `LOAN`
- `UPGRADE`
- `RELEASE`
- `DETACH`

ownership builtin `USING` 已从 expression surface 移除。

当前 parser 规则：

- 已知百分号 ownership 拼写统一报告 `legacy_syntax_removed`，不生成 ownership AST
- `own T(...)` 是 resource unique 构造入口
- `ref` / `ref readonly` 建立 lexical reference view
- `share` / `weak` / `upgrade` / `intoGc` / `drop` 走类型化 member/builtin contract

## 类型与借用约束

当前前端做了显式 operand 校验：

- `share()` 只接受 `Unique<T>`
- `weak()` 只接受 `Shared<T>`
- `upgrade()` 只接受 `Weak<T>`
- `drop(...)` 只接受可释放 owner
- `intoGc()` 只接受可进入 plain GC world 的独占 owner bridge

borrow / loan 规则当前已覆盖到 task/await 路径：

- local `ref readonly` view 不可跨 `await`
- local writable `ref` view 不可跨 `await`
- borrowed parameter 也不可在 `await` 之后继续使用

v1 仍保持保守限制：

- `drop` 的可消费 Place 仍受 owner/definite-assignment 约束
- `intoGc()` 不接受多 owner shared value
- reference view 的 escape analysis 以“不可返回/不可捕获/不可全局存储”为硬边界

## ExecBC 与 `SemIR`

ownership expression 现在直接落成 dedicated opcode：

- `OWN_UNIQUE`
- `OWN_BORROW`
- `OWN_LOAN`
- `OWN_SHARE`
- `OWN_WEAK`
- `OWN_UPGRADE`
- `OWN_RELEASE`
- `OWN_DETACH`

对应的 `SemIR` opcode 同步保留这些 ownership transition，而不是退回 native helper constant。

statement-level `using` 与 ownership builtin 已明确拆开：

- statement `using`
  - `MARK_TO_BE_CLOSED`
  - `CLOSE_SCOPE`
- ownership expression
  - dedicated `OWN_*`

因此 `.zri`、ExecBC、AOT C、AOT LLVM 不再需要 legacy `OWN_USING` surface。

## Runtime Contract

runtime ownership helper 目前和 surface 对齐：

- `ZrCore_Ownership_ShareValue`
  - 只接受 `Unique<T>`
- `ZrCore_Ownership_WeakValue`
  - 只接受 `Shared<T>`
- `ZrCore_Ownership_LoanValue`
  - 只接受 `Unique<T>`
- `ZrCore_Ownership_UpgradeValue`
  - 只接受 `Weak<T>`
  - weak 失效时返回 `null`
- `ZrCore_Ownership_ReturnToGcValue`
  - 对应 canonical `intoGc()` bridge
  - 只接受可独占转移的 owner

legacy ownership native helper `%using` 已从 writer/runtime helper 映射中移除，helper id `5` 仅保留为二进制编号洞位，不再映射到运行时函数。

## Resource / Unique / Drop

Syntax 04 M1 在同一 opcode family 上增加 type-directed resource surface：

- `resource class T` 把 resource modifier 投影到 prototype metadata。
- `own T(...)` 先构造 direct Unique seed，并在 constructor call 前发出
  `MARK_TO_BE_CLOSED`。
- constructor 成功后通过 `OWN_UNIQUE` move 到最终 expression slot，再关闭空 marker。
- `drop(owner)` 和 scope cleanup 都通过 `OWN_RELEASE`。

direct resource Unique 的 runtime value 不建立 ownership control block。VM 的
`OWN_UNIQUE`/`OWN_RELEASE` 与 AOT C/LLVM 生成的 ownership helper 使用相同 slot 与顺序；
focused pipeline 同时断言 function tree、Semantic IR、`.zri` ownership 文本、generated C/LLVM
helper call，并执行 Drop log `21`。构造抛错时 marker 仍然有效，runtime 只逆序释放已初始化
managed fields，不调用完整对象的 custom Drop。

M1 仍暂时使用 existing GC ignore registry 保持 direct resource storage；这不是 artifact ABI
承诺，Syntax 04 M4 必须以显式 GcDomain/bridge identity 替换。

## M7 ownership opcode cutover

新source语法、Semantic IR、ExecBC与AOT现在使用一一对应的操作：

- readonly/mutable reborrow分别发出`OWN_VIEW_SHARED`/`OWN_VIEW_MUT`；
- `Unique<Resource>.intoGc()`发出`OWN_INTO_GC_BOX`；
- 显式`detach`/return-to-GC发出`OWN_RETURN_TO_GC`。

legacy `OWN_BORROW`、`OWN_LOAN`与`OWN_DETACH`的numeric opcode仍由VM/AOT reader保留，供旧
artifact兼容，但compiler/writer不再从新source发出它们。`OWN_INTO_GC_BOX`只调用
`ZrLibrary_AotRuntime_OwnIntoGcBox`，`OWN_RETURN_TO_GC`只调用
`ZrLibrary_AotRuntime_OwnReturnToGc`；旧`OwnDetach` helper保留旧artifact fallback，不能成为
新lowering的字符串别名。C与LLVM backend按structured opcode选择helper。

## Artifact 收口

当前 artifact 侧已经对齐到新语义：

- parser / compiler 不再从 production source 生成百分号 ownership helper
- writer 不再序列化 legacy ownership using helper
- io runtime 不再反序列化 legacy ownership using helper
- checked-in `.zri` / AOT fixture 需要重生成，避免继续携带旧 `OWN_USING` 文本

## 验证覆盖

当前收口后需要稳定成立的验证点：

- parser diagnostics
  - `%using new ...`
  - `%using(expr)`
  - legacy field-scoped `%using`
- type inference / compile guards
  - invalid ownership builtin operand 立即拒绝
- task / borrow rules
  - borrowed / loaned binding 不可跨 `await`
- SemIR / AOT
  - `OWN_VIEW_SHARED/OWN_VIEW_MUT/OWN_INTO_GC_BOX/OWN_RETURN_TO_GC`在Semantic IR、
    ExecBC、AOT C与AOT LLVM保持一致
  - 新source function tree明确不包含legacy `OWN_BORROW/OWN_LOAN/OWN_DETACH`
  - canonical `using` statement 继续走 `MARK_TO_BE_CLOSED`
- runtime lifecycle
  - `intoGc()` 拒绝 multi-owner shared
  - 最后一个 shared drop 后 weak 升级返回 `null`
