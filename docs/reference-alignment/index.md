---
related_code:
  - tests/parser/test_char_and_type_cast.c
  - tests/harness/reference_support.h
  - tests/harness/reference_support.c
  - tests/fixtures/reference/core_semantics/lexing_literals_diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/expressions_precedence_chains/manifest.json
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/manifest.json
  - tests/fixtures/reference/core_semantics/types_casts_const/manifest.json
  - tests/fixtures/reference/core_semantics/object_member_index_construct_target/manifest.json
  - tests/fixtures/reference/core_semantics/protocols_iteration_comparable/manifest.json
  - tests/fixtures/reference/core_semantics/modules_imports_artifacts/manifest.json
  - tests/fixtures/reference/core_semantics/oop_inheritance_descriptors/manifest.json
  - tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/manifest.json
  - tests/fixtures/reference/core_semantics/exceptions_gc_native_stress/manifest.json
implementation_files:
  - tests/parser/test_char_and_type_cast.c
  - tests/harness/reference_support.h
  - tests/harness/reference_support.c
  - docs/reference-alignment/core-semantics-matrix.md
  - docs/reference-alignment/full-stack-test-matrix.md
  - docs/reference-alignment/project-magic-constants-inventory.md
  - scripts/audit_magic_constants.py
plan_sources:
  - user: 2026-04-03 implement the full-stack reference-language-driven test matrix
  - .codex/plans/ZR 全栈参考语言共性测试矩阵设计.md
  - docs/testing-and-validation/core-semantics-reference-alignment.md
tests:
  - tests/parser/test_char_and_type_cast.c
  - tests/fixtures/reference/core_semantics/lexing_literals_diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/expressions_precedence_chains/manifest.json
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/manifest.json
  - tests/fixtures/reference/core_semantics/types_casts_const/manifest.json
  - tests/fixtures/reference/core_semantics/object_member_index_construct_target/manifest.json
  - tests/fixtures/reference/core_semantics/protocols_iteration_comparable/manifest.json
  - tests/fixtures/reference/core_semantics/modules_imports_artifacts/manifest.json
  - tests/fixtures/reference/core_semantics/oop_inheritance_descriptors/manifest.json
  - tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/manifest.json
  - tests/fixtures/reference/core_semantics/exceptions_gc_native_stress/manifest.json
doc_type: category-index
---

# Reference Alignment

本目录承载 `ZR` 参考语言对齐资产的主入口。这里记录的不是临时笔记，而是后续 parser、compiler、runtime、artifact、project、stress 回归都要复用的长期合同。

## 当前文档

- `core-semantics-matrix.md`
  - 第一阶段 6 个核心主题的 capability matrix
  - `parser / semantic / compiler / runtime / project / golden` 六层状态
  - 当前 `const` 控制流 reference fixture 与批次 C 证据
- `full-stack-test-matrix.md`
  - 10 个固定语义域
  - `tests/fixtures/reference/core_semantics/` 下的全栈 manifest 结构
  - 120 条首轮核心 case 的配额与类型分布
  - 现有 executable 与验证层映射
  - 首轮 30 条高风险优先 case 清单
  - `source / artifact / runtime / project` 分层验证入口
  - `smoke/core/stress` 三档与 interp/binary 主链路合同
- `project-magic-constants-inventory.md`
  - 项目级魔法数与常量收敛的唯一 inventory
  - 第一批进入 `zr_vm_common` 与模块 `conf.h` 的最终映射
  - backlog 模块与 `scripts/audit_magic_constants.py` 的使用方式

## 阅读顺序

1. 先读 `core-semantics-matrix.md`，确认计划文件里第一阶段 6 个主题目前到底处于什么状态。
2. 再读 `full-stack-test-matrix.md`，理解 10 域主矩阵、分层验证入口、配额、helper 合同和 rollout 顺序。
3. 进入 `tests/fixtures/reference/core_semantics/<domain>/manifest.json` 看某一域的上游证据与 `ZR` 决策。
4. 如果在做跨模块配置收敛，再读 `project-magic-constants-inventory.md`，先看哪些值已经进入 `zr_vm_common`，哪些仍明确留在模块 `conf.h` 或 backlog。
5. 最后沿 frontmatter 的 `tests` 和 `related_code` 回到具体 C 测试、artifact 校验和项目 fixture。
