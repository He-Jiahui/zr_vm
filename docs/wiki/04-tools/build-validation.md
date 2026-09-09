---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - scripts
  - .github/workflows
  - zensical.toml
  - requirements-docs.txt
implementation_files:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - scripts/validate_wiki.py
  - tests/scripts/test_validate_wiki.py
  - .github/workflows/wiki-pages.yml
  - zensical.toml
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/scripts/test_validate_wiki.py
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

## GitHub Pages 自动发布

仓库使用根目录的 [`zensical.toml`](https://github.com/He-Jiahui/zr_vm/blob/main/zensical.toml)
将本目录作为文档源，并由
[`.github/workflows/wiki-pages.yml`](https://github.com/He-Jiahui/zr_vm/blob/main/.github/workflows/wiki-pages.yml) 构建和发布
静态站点。发布目标是
[`https://he-jiahui.github.io/zr_vm/`](https://he-jiahui.github.io/zr_vm/)。站点输出目录为
`site/`，已经加入 `.gitignore`，不应提交生成文件。

### 触发与门禁

- 对 `main` 的 push：运行契约测试、源文档校验和严格构建，成功后上传 Pages artifact 并部署。
- 面向 `main` 的 pull request：运行同样的校验和构建，但不会部署，便于在合并前发现链接或语法问题。
- `workflow_dispatch`：仅从 `main` 手动运行时部署；其它分支可用于验证构建而不会覆盖线上站点。

构建阶段按以下顺序执行：

1. `python -m unittest discover -s tests/scripts -p "test_validate_wiki.py" -v` 检查 manifest、front matter、内部链接和锚点。
2. `python scripts/validate_wiki.py --root .` 复查源目录契约。
3. `zensical build --clean --strict` 生成站点，并要求 `site/index.html` 非空。
4. `python scripts/validate_wiki.py --root . --site-dir site` 确认产物入口存在后再上传。

### 首次启用

在 GitHub 仓库的 **Settings > Pages** 中将 **Source** 设置为 **GitHub Actions**。之后合并
一次文档变更到 `main`，在 **Actions > Wiki Pages** 查看 `build` 和 `deploy` 两个 job；部署
完成后，job 的 `github-pages` environment 会显示实际 URL。

### 本地预览

```bash
python -m pip install -r requirements-docs.txt
python scripts/validate_wiki.py --root .
zensical serve
```

默认预览地址为 `http://127.0.0.1:8000/`。需要检查生产构建时运行：

```bash
zensical build --clean --strict
python scripts/validate_wiki.py --root . --site-dir site
```

### 常见故障

- **Pages 没有发布**：确认仓库 Pages source 是 **GitHub Actions**，并且部署 job 使用了
  `github-pages` environment；PR 构建成功本身不会产生线上部署。
- **构建因链接失败**：先看 `validate_wiki.py` 的文件和行号，再修正相对路径或 heading 锚点；
  不要关闭 `strict` 来绕过错误。
- **本地 URL 与线上不同**：检查 `zensical.toml` 的 `site_url`、仓库名和 `edit_uri`，三者应与
  实际 GitHub 用户名、仓库和 `main/docs/wiki` 路径一致。
