# 03-M2 Receiver effect 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md`
的 `M2 Receiver effect`。

## 状态与产出记录

- 完成时间：2026-07-21 03:12 +08:00
- 状态：已完成
- 完成项目：
  - 新增 contextual `readonly struct` declaration；instance field 在 constructor 外统一只读，
    普通 instance `fn` 规范化为 readonly receiver，显式 `const fn` 保持同一 canonical effect，
    static member 与 constructor 不获得 readonly receiver effect。
  - readonly struct、class、interface、override、generic、dynamic 与 native dispatch 共用既有
    canonical receiver capability matrix；getter 为 readonly，setter 为 mutable。
  - inline struct readonly instance method 通过 resolved compiled function 与隐藏 receiver
    alias 进入 known VM call；caller metadata 使用 `ALIAS | INLINE_RECEIVER_ARGUMENT` 保留
    source Place，callee metadata 使用 `ALIAS | INDIRECT_ALIAS | BORROWED_ALIAS` 保存 source
    call identity、frame anchor 与 stack slot。call window 不物化 payload，栈迁移后仍按 Place
    解析，且不使用会结束 source frame 生命周期的 tail-frame reuse。
  - `in Snapshot`、`ref readonly Snapshot` 与普通 readonly struct local 的 readonly method call
    均保留 inline receiver Place，不产生 defensive copy；writable member、构造后 field write 与
    readonly method 内 field write均静态拒绝。
  - source unbound method reference 在 declaration identity、非 variadic 参数与返回类型完整时
    发布 canonical function TypeId、receiver effect、SymbolId 与 declaration range；publisher
    重新验证每个参数 AST type 与 prototype inferred type 完全一致，open generic、缺失参数类型
    与其他不完整声明继续 unavailable，不按类型名或 member name 重建。
  - readonly 与 mutable method-reference TypeId 通过 schema-v1 artifact signature write/intern
    roundtrip；property getter runtime 与 readonly setter negative gate 已覆盖。
  - function artifact 升级为 v1 patch 35，持久化 source patch，并在 loader 统一校验 known
    frame flags、slot kind、size/alignment、frame bounds 与 borrowed alias 的精确 flag 组合；
    patch 34 的 borrowed artifact 与 malformed binding 均拒绝加载。
  - generated AOT direct call 在 caller 完整 frame storage top 之后建立独立 call window，缓存
    `GetFrameStorageSlotCount()`，并复用 VM shared PreCall 完成 VALUE copy 与 borrowed alias
    binding。root/mirror runtime 均在 materialize、PreCall 和 debug CALL hook 可能迁移栈后从
    anchor/callInfo 恢复地址，失败路径释放 staging ownership，成功路径不覆盖 caller frame。
  - LLVM runtime regression 使用 moving allocator 与 CALL hook 强制扩栈，验证迁移后的 callee
    frame top、source/binding 地址相同、callee write 对 source 可见，以及 Finish/GC teardown。
    execbc mirror fixture 使用 generated slots=6、full storage slots=9，锁定缓存与 call-window
    边界不退回旧 generated-count 语义。
  - 最终 GCC/Clang/MSVC focused matrix 均为 10/10 targets、真实进程退出码 0。每套汇总为
    receiver 28/28、canonical consumers 15/15、syntax contract 8/8、artifact schema 13/13、
    struct init 15/15、core layout/relocation 38/38、AOT C SemIR 8/8、LLVM runtime 3/3、
    compiler integration 127/127；GCC/Clang generated-C shared-library smoke 5/5，MSVC 同一
    target 4 Pass/0 Fail/1 Ignore，唯一 ignore 为测试明确标注的 Unix-only 动态库执行路径。
- 验收证据：
  - `tests/parser/test_reference_receiver_call_boundary.c`
  - `tests/parser/test_canonical_consumers.c`
  - `tests/parser/test_reference_syntax_contract.c`
  - `tests/parser/test_artifact_schema.c`
  - `tests/parser/test_compiler_features.c`
  - `tests/core/test_type_layout_inline_copy.c`
  - `tests/parser/test_aot_c_value_semir_contracts.c`
  - `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  - `tests/parser/test_aot_llvm_symbol_stripping.c`
  - `zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c`

## 验收结果

- GCC、Clang、MSVC 均完成同一 10-target build/run matrix，全部目标真实退出 0；MSVC
  generated-C smoke 的 Unix-only case 按测试平台门禁 ignore，其余 case 全部通过。
- AOT mirror runtime 与 execbc source 在 GCC/Clang `-fsyntax-only` 下退出 0；仅保留 execbc
  source-sync fixture 的既有 missing-field initializer warnings。
- `git diff --check` 通过；第二轮只读代码审计结果为 0 Critical、0 Important。

## 残余覆盖边界

- ownership-bearing VALUE 参数的 success/materialize-failure/PreCall-failure 引用计数回滚尚无
  独立定向用例；当前 shared PreCall 与 discard 顺序已审计，但后续 AOT hardening 应补齐。
- LLVM regression 覆盖 runtime Prepare/Finish 与强制栈迁移，尚未执行真实生成的 LLVM callee
  thunk；generated-C Unix smoke 与 compiler/runtime matrix 分别覆盖代码生成和执行边界。
- artifact validator 已覆盖 old-patch 与 malformed borrowed binding；unknown flags、重复 slot、
  错误 offset/alignment 的逐分支负例留给 artifact hardening，不改变本里程碑冻结的 schema。

## 边界与后继

- M2 不把 open generic method reference 猜成 closed callable；只有具备结构化 generic binding
  的消费者才能发布 closed contract。
- M2 不引入普通 struct object wrapper，也不改变 M1 TypeLayout size/alignment/field map/hash。
- M3 继续实现 ref struct 的 storage/escape/GC frame-map restrictions；本里程碑不提前放宽
  ref-like storage。
