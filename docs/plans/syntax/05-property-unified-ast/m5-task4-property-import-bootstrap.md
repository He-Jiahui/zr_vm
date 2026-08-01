---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
plan_sources:
  - docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md
  - user: 2026-08-01 prioritize and exact-commit Task4 parser property/import support
tests:
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_property_consumer_runtime_bootstrap_cases.h
  - tests/acceptance/2026-08-01-syntax-05-m5-task4-property-import-bootstrap.md
doc_type: milestone-detail
---

# Syntax 05 M5 Task4 Property/Import Support

## 状态与产出记录

- 完成时间：2026-08-01 08:03 +08:00
- 状态：completed
- 完成项目：
  - 复核并冻结空 imported placeholder 的 structured compiled-row 合并：只有
    `propertyIdentity`、`accessorRole`、canonical value TypeId 与 reference fields 完整一致时
    发布 PropertyQuery；普通 `__get_*` 方法和 identity 缺失的 accessor 都保持 unavailable。
  - 公共 `ZrParser_TypeInference_RegisterRuntimePrototypes(SZrCompilerState *, const SZrFunction *)`
    现对无效参数 fail-closed；合法无 prototype payload 保持成功 no-op。
  - 新增 parser-only 公共 API 回归，对 source carrier、write/reload `.zro` carrier、空 imported
    placeholder、ref-readonly type/ref 合同、hidden-accessor 负边界和破坏 identity 的 unavailable
    边界逐项断言。
  - GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228.0 focused target 均为 11/11、
    真实进程 `exit 0`；最终证据基于 `HEAD 1c50bad + Task4 exact overlay`。

## Exact Ownership

- production：`zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c`
- tests：`tests/parser/test_property_consumer_contracts.c`、
  `tests/parser/test_property_consumer_runtime_bootstrap_cases.h`
- docs/acceptance：本记录、type-inference module doc、Task4 acceptance 和 M5 状态记录
- forbidden：`zr_vm_lib_debug/**`、`tests/debug/**`、`zr_vm_language_server/**`、
  `tests/language_server/**`、shared CMake/build/generated artifact 路径均为 0

## 结论

Task4 parser property/import support 已完成独立 exact 收口。Debug/LSP 可消费公共 bootstrap，
但其自身 runner、runtime-root、closure capture 和 ownership-builtin 后续不属于本记录。
