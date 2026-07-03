# AOT 10-S5F / 12-S5E dynamic dependency MethodDef token root

时间：2026-06-30 13:22:30 +08:00

## 范围

- 完成 `dynamicDependencyMethodToken` decorator metadata carrier。
- 支持当前 root module 内的 exported function `MEMBER_DEF` token。
- 解析成功后把目标 function flat index 注入既有 reflection annotation root 集合。
- 继续复用 `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]` generated-C 诊断。

## RED

- `tests/parser/test_aot_reachability.c` 新增 token dependency fixture 后，`annotationRootCount` 仍为 0。
- `tests/parser/test_aot_c_reflection_annotation_preserve.c` 新增 generated-C fixture 后，otherwise-unreachable target 未被保留。

## GREEN

- `backend_aot_reachability_function_graph.c` 读取 `dynamicDependencyMethodToken` uint metadata。
- token table 必须是 `ZR_METADATA_TABLE_MEMBER_DEF`。
- resolver 只遍历 root function 的 `typedExportedSymbols` function exports。
- 匹配的 exported symbol 通过 `callableChildIndex` 找到 callable child，再映射回 pre-trim function table flat index。
- generated C fixture 看到 `annotationRoot[0] = 2`，并保留 `zr_aot_fn_2`。
- source contract 锁定 metadata field、MEMBER_DEF gate、typed exported symbol lookup、child index guard 和 flat-index lookup。

## 验证

- WSL gcc: CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；reachability 9/0；annotation preserve 8/0；global shared-library smoke 10/0；call shared-library smoke 5/0；dynamic deopt bridge smoke 7/0；source contracts 23/0。
- WSL clang: 同组 CTest 3/3；reachability 9/0；annotation preserve 8/0；global shared-library smoke 10/0；call shared-library smoke 5/0；dynamic deopt bridge smoke 7/0；source contracts 23/0。
- Windows MSVC Debug: 同组 CTest 3/3；reachability 9/0；annotation preserve 8/0；source contracts 23/0；Unix-only smoke tests 为 0 failures / ignored。

## 未覆盖

- 按名 member dependency。
- 非导出 member token dependency。
- field/type dependency。
- 跨模块 annotation dependency。
- `@dynamically_accessed` 数据流。
- warning promotion / per-warning suppression。
- 未注解反射 warning。
- 类型/成员级 DESCRIPTION 提升。
- 完整 trim analyzer 或 metadata sweep。
