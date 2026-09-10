---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - scripts
  - .github/workflows
  - zensical.toml
  - requirements-docs.txt
  - docs/pygments_zr/pyproject.toml
  - docs/pygments_zr/zr_pygments/lexer.py
  - docs/wiki/stylesheets/extra.css
implementation_files:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - scripts/validate_wiki.py
  - tests/scripts/test_validate_wiki.py
  - .github/workflows/wiki-pages.yml
  - zensical.toml
  - docs/pygments_zr/pyproject.toml
  - docs/pygments_zr/zr_pygments/lexer.py
  - docs/wiki/stylesheets/extra.css
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/scripts/test_validate_wiki.py
  - tests/scripts/test_wiki_theme_assets.py
  - tests/scripts/test_zr_pygments.py
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

1. `python -m unittest tests/scripts/test_validate_wiki.py tests/scripts/test_wiki_theme_assets.py tests/scripts/test_zr_pygments.py -v` 检查 manifest、front matter、内部链接、主题资源和 `zr` 高亮 lexer。
2. `python scripts/validate_wiki.py --root .` 复查源目录契约。
3. `python -c "from pygments.lexers import get_lexer_by_name; print(get_lexer_by_name('zr').name)"` 确认本地插件已安装并注册。
4. `zensical build --clean --strict` 生成站点，并要求 `site/index.html` 非空。
5. `python scripts/validate_wiki.py --root . --site-dir site` 确认产物入口存在后再上传。

本次参考文档扩展的可复核记录保存在仓库文件
`tests/acceptance/2026-09-10-wiki-reference-expansion.md`（该目录不属于 Wiki 页面树）。
它固定了源文件数、manifest 页面数、内部链接数、测试和 strict build 的验收输出；后续
新增页面时应在同一记录或新的日期记录中追加实际命令和结果，不要只写“已验证”。

ZrVm 的源码 fence 使用 `zr` 标记。`docs/pygments_zr` 是一个本地 Pygments plugin，
通过 `requirements-docs.txt` 安装并注册 `zr`、`zro`、`zrs`、`zrp` aliases；没有安装该
包时，构建器会把未知语言降级为纯文本，页面仍能显示但不会有 token class。构建后可用
`rg -n "language-zr|class=\"k\"|class=\"nf\"" site` 检查 HTML，确认高亮不是只在
源 Markdown 中声明。

明暗主题由 `zensical.toml` 的三组 `project.theme.palette` 提供：系统偏好、light/default
和 dark/slate。`docs/wiki/stylesheets/extra.css` 只覆盖 ZR token 颜色、代码块边框和
reduced-motion 行为；正文颜色仍交给主题变量，避免在 dark mode 中写死白底或黑字。

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

打开预览后，应在任一含 `zr` fence 的页面确认三件事：代码行出现关键字/函数名/数字的
不同 token 颜色；页面右上角能在系统、浅色、深色之间切换；深色下正文、代码背景和行号
仍保持足够对比度。切换只影响展示主题，不会改变 source、搜索索引或链接地址。

### 常见故障

- **Pages 没有发布**：确认仓库 Pages source 是 **GitHub Actions**，并且部署 job 使用了
  `github-pages` environment；PR 构建成功本身不会产生线上部署。
- **构建因链接失败**：先看 `validate_wiki.py` 的文件和行号，再修正相对路径或 heading 锚点；
  不要关闭 `strict` 来绕过错误。
- **本地 URL 与线上不同**：检查 `zensical.toml` 的 `site_url`、仓库名和 `edit_uri`，三者应与
  实际 GitHub 用户名、仓库和 `main/docs/wiki` 路径一致。
