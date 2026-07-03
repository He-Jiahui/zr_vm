# AOT 10-S5A / 12-S5A Reflectable Annotation Function Root

时间：2026-06-30 11:25:44 +08:00

## 状态

- 10-S5 / 12-S5 首个 annotation-root 子切片完成。
- 完整 `@dynamically_accessed` 数据流、`@dynamic_dependency`、`@requires_unreferenced_code`、类型/成员级 DESCRIPTION 提升、未注解反射 warning 与完整 trim analyzer 仍未完成。

## 完成项目

- 复用现有 compile-time decorator metadata 作为计划级 `@reflectable` 的首个承载面。
- AOT reachability 新增 `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION`。
- `backend_aot_collect_reflection_annotation_roots(...)` 在裁剪前扫描 function table，识别函数 decorator metadata 中的 `reflectable: true`。
- AOT C emitter 将 annotation roots 传入 static callable reachability graph，使 otherwise-unreachable function 在 opt-in code stripping 后保留。
- 生成 C 头部输出 `code_stripping.annotationRoots` 与 `code_stripping.annotationRoot[index]` 诊断标记。
- MSVC shared 构建下的 `zr_vm_aot_reachability_test` 直接编入 backend reachability 内部源文件，避免 Windows DLL 非导出内部符号链接失败。

## RED / GREEN

- RED：新增 `zr_vm_aot_c_reflection_annotation_preserve_test` 后，生成 C 缺少 `code_stripping.annotationRoots` marker，带 `reflectable: true` metadata 的不可达 function 仍未作为 annotation root 保留。
- GREEN：带 `reflectable: true` metadata 的不可达 child function 保留并发射 `zr_aot_fn_2`；移除 metadata 后同一 child function 仍被裁剪。

## 验证

- WSL gcc：CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3。
- WSL clang：CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3。
- Windows MSVC Debug：CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3。
- WSL gcc / clang：`zr_vm_aot_c_source_contracts_test` 22/0。
- `git diff --check` 退出 0，仅报告既有 LF/CRLF 提示。

## 备注

该切片只覆盖函数级 `reflectable: true` bool metadata root，不声明新语法、参数/返回数据流、动态依赖、未注解反射 warning、跨模块 root 或完整 metadata sweep 完成。
