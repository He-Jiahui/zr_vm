---
plan_id: aot-07-codegen
record_id: 2026-08-01-value-semir-parameter-layout-consumption
status: completed
completed_at: 2026-08-01 08:42:38 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Value-SemIR Parameter Layout Consumption

## 状态与产出记录

- 完成时间：2026-08-01 08:42:38 +08:00
- 状态：A7.2H value-SemIR parameter layout consumption 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：generic/shared inline-struct `CALL_TYPED` 不再从 `SZrFunction.parameterMetadata` 推断引用参数，
  而是按 callee ExecIR flat function index 解析 `frameLayout.parameterLayouts`。
- 完成项目：shared-method route 仅在 parameter sidecar 非空、`parameterLayoutCount` 同时等于
  `frameLayout.parameterCount` 与 call `argumentCount`、所有 role 为普通参数、至少一个对应 TypeRef 为
  OBJECT/ARRAY，且 caller source slot 仍是容量不少于 `sizeof(SZrTypeValue)` 的 VALUE slot 时启用。
- 完成项目：missing/unknown projection、receiver role、count 漂移、slot kind/size 漂移均 fail closed 到普通
  inline-struct call 路径；删除旧的 `parameterMetadataCount - argumentCount` 偏移与 trimmed table 访问风险。
- 完成项目：producer 已物化并计入 `argumentCount` 的 default argument 可通过 exact-arity gate；本切片不推导
  default origin、passing direction 或 receiver 映射，也不改变 public function/artifact/manifest schema。
- 计划映射：完成 AOT 07 A7.2H retained parameter-layout consumption，并证明 AOT 12 裁剪后的 flat-index
  callee 仍使用保留 sidecar；不新增 reachability node，AOT 12 保持部分完成。

## 代码与文档产出

- `backend_aot_c_value_semir.c/.h` 直接接收 ExecIR module 并解析 callee ExecIR；function-body 编排删除不再需要的
  legacy callee function 参数。
- `backend_aot_c_value_semir_calls.c/.h` 集中执行 exact-count/no-receiver/type/slot gate，并只把已证明的
  parameter sidecar 交给 generic shared inline-struct callsite writer。
- `test_aot_c_generic_call_typed.c` 与拆出的
  `test_aot_c_generic_call_typed_parameter_layout_cases.h` 覆盖冲突 legacy metadata 的正例和 unknown projected
  TypeRef、receiver role 的反例，并在 callee 前裁掉未引用 nested function 后覆盖 sparse flat-index lookup；
  `test_aot_c_value_semir_contracts.c` 锁定直接 ExecIR lookup 与旧 offset/metadata 依赖移除。
- 模块文档、AOT 07/12 完成链接和 acceptance evidence 同步更新。
- 大文件决策：A7.2H fixture/mutator/test 已从 generic-call 主文件拆到 301 行 focused header，主文件保持
  831 个物理行；`backend_aot_c_function_body.c` 只删除过时实参/局部变量，没有新增职责，因此不做无关拆分。

## 验证结果

- 初始 RED：仅加入 ExecIR-authority 正例时，冻结 WSL GCC generic typed-call 为 8/1；旧实现仍读取被改为 I64
  的 legacy parameter metadata，未生成 shared marker。改用 sidecar 后为 8/0。
- 独立设计审查指出 `CALL_TYPED.argumentCount` 已包含 receiver，拒绝初版 `argumentCount + 1` 映射；同时指出
  default 可能由 producer 物化，不能宣称 exact count 一律拒绝 default。实现收窄为 exact-count/no-receiver，
  并增加 unknown projection 反例。最终复审要求补齐 receiver reject 与 stripping 后 sparse callee lookup；两项
  source-level 回归加入后，三编译器 generic typed-call 为 11/0。
- WSL GCC 11.4：generic typed-call 11/0、value-SemIR 8/0、MethodInfo 3/0、code stripping 37/0、SemIR 10/0、
  generic sharing 9/0、debug metadata 6/0、typed-call 4/0。
- WSL Clang 14.0：同一八组矩阵分别为 11/0、8/0、3/0、37/0、10/0、9/0、6/0、4/0。
- Windows MSVC 19.44.35228.0 x64 Debug 增量重建退出码 0；同一矩阵全部通过，generic typed-call 为
  11/0 且 3 个 Unix-only runtime case 按预期 ignored。仅保留 `%TEMP%` 目录既有 MSB8029/第三方 warning。
- 主工作树、WSL 冻结树与 Windows 冻结树的 8 个实现/测试文件 SHA-256 逐项一致；`git diff --check` 与
  独立最终复审结果在提交前再次确认，最终复审返回 `No findings.`。

## 基线偏差

- GCC 全量 source-contract binary 为 20/24；4 个失败分别属于 direct stack-copy scalar sync、typed arithmetic
  literal/written-before、`NEG_SIGNED` bool equality 与 method-token table emitter 的既有静态文本漂移。
  A7.2H 新增 value-SemIR source contract 自身通过，且失败文件不在本切片生产修改范围。
- GCC call shared-library smoke 为 4/5；失败仍是 A7.2G 已记录的 binary-input quickened dynamic-call writer
  基线问题，value typed-call case 通过。本切片不扩大修复范围。

## 后续证据更正

- A7.2I 于 2026-08-01 发现本记录所称的 receiver consumer 反例实际从源码编译为 `DYN_CALL`，没有进入
  `CALL_TYPED` selector。A7.2H 的 no-role 生产 gate 本身不受影响，但该 fixture 只能证明动态实例调用不进入
  typed route；有效的 receiver-bearing `CALL_TYPED` RED/GREEN 覆盖由 A7.2I 的可控 parameter sidecar 用例补齐。

## 未完成边界

- receiver-bearing parameter layout、`in/ref/out`/readonly/direction、default origin、TypeId/TypeRef/
  CallableContract 等价性仍未建模；本切片只消费已投影的 shallow TypeRef 和普通 runtime argument 顺序。
- 不生成 aggregate return destination、spill、address-taken slot、GC/ref provenance 或 safepoint/debug map，
  不声明 C/LLVM frame ABI golden parity。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
