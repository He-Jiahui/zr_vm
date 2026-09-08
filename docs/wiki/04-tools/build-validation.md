---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - scripts
  - .github/workflows
implementation_files:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_executable_suite.cmake
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/cmake/run_performance_suite.cmake
doc_type: workflow-detail
---

# 构建与验证

## CMake 配置

```bash
cmake -S . -B build/gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON -DBUILD_CLI=ON
cmake --build build/gcc-debug -j 8
ctest --test-dir build/gcc-debug --output-on-failure --parallel 8
```

顶层默认 C11、严格 warning；可选开关包括 `BUILD_NETWORK_LIB`、`BUILD_DEBUG_LIB`、
`BUILD_THREAD_LIB`、`BUILD_LANGUAGE_SERVER`、`BUILD_RUST_BINDING`、`BUILD_LANGUAGE_SERVER_EXTENSION`。
MSVC 使用 multi-config generator，应传 `--config Debug`；Unix 会加入 `-fPIC`。

## 验证层级

1. 单元层：parser/core/library/provider contract 测试。
2. 集成层：CLI、module graph、artifact、LSP stdio/WASM、Rust binding。
3. acceptance 层：VM/AOT equivalence、shared-library smoke、跨编译器矩阵。
4. 压力层：GC、pool generation、并发 scheduler、benchmark。

失败诊断应先定位最底层失败（例如 layout/GC/descriptor），再判断上层 CLI/LSP 现象；不要
用禁用测试或放宽 contract 规避错误。启用 sanitizer、ASan/UBSan 或 MSVC runtime checks
时，保留同一 fixture 和 seed，便于跨后端复现。
