---
related_code:
  - scripts/code_review_inventory.py
  - zr_vm_common/ThirdPartyMacros.cmake
  - tests/CMakeLists.txt
  - zr_vm_core/CMakeLists.txt
  - zr_vm_library/CMakeLists.txt
implementation_files:
  - tests/third_party/CMakeLists.txt
  - tests/third_party/zr_unity/CMakeLists.txt
  - zr_vm_core/third_party/CMakeLists.txt
  - zr_vm_core/third_party/zr_utf8proc/CMakeLists.txt
  - zr_vm_core/third_party/zr_xx_hash/CMakeLists.txt
  - zr_vm_library/third_party/CMakeLists.txt
  - zr_vm_library/third_party/zr_c_json/CMakeLists.txt
  - zr_vm_library/third_party/zr_miniz/CMakeLists.txt
  - zr_vm_library/third_party/zr_tiny_dir/CMakeLists.txt
  - zr_vm_library/third_party/zr_tiny_dir/tinydir.c
  - tests/fixtures/syntax_migration_inventory/docs/current-snippets.md
  - tests/fixtures/projects/syntax_reference_v1/generated/file_locator_import.zr
  - tests/fixtures/syntax_migration_frontend/expected/machine_forms.json
  - tests/fixtures/syntax_migration_inventory/expected/inventory.json
  - tests/fixtures/syntax_migration_inventory/expected/repository-inventory.json
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
  - tests/fixtures/projects/syntax_reference_v1/golden/provider-locator.json
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/scripts/test_syntax_migration_inventory.py
  - tests/parser/test_syntax_reference_v1.c
  - tests/cli/syntax_migration_smoke.js
doc_type: workflow-detail
---

# 首方范围边界复核

本批只补范围审计证据，不改源文件、共享 `inventory.tsv`、scanner 或 Git index。coverage 台账有 18 行：9 个项目维护的 third-party CMake 包装、1 个本地占位 C 源、2 个由测试读取的源样例和 6 个 JSON 数据文件。行状态是注释/契约审查结论，不代表测试通过或 scanner 语义审查已完成。

## 已确认的边界

`scripts/code_review_inventory.py:49-53` 按完整路径段跳过 `third_party`、`generated`、`expected`、`golden` 和 `snapshots`；`classify()` 在检查后缀前执行路径段排除（`scripts/code_review_inventory.py:101-125`）。因此，仅靠这些目录名无法区分项目维护的 CMake glue、手写夹具和上游代码。本批台账中的 9 个 CMake 文件及 `zr_vm_library/third_party/zr_tiny_dir/tinydir.c` 都是普通跟踪文件，不是 gitlink；九个包装文件分别由当前模块 CMake 的 `add_subdirectory()` 链接进构建图。对应真正的外部依赖内容，则需依据索引对象与来源证据分别判断。

`.gitmodules:1-28` 声明 7 个子模块；本次 `git ls-files --stage` 核对这 7 个路径均为模式 `160000`：Unity、xxHash、utf8proc、cJSON、tinydir、libffi 和 libuv。子模块内容仍属上游依赖，应与仓库中普通文件模式 `100644` 的包装区分。`zr_vm_library/third_party/zr_miniz/miniz/miniz.c:1-25` 保留 miniz 上游版权与许可，头文件 `miniz.h:1-4` 标为 3.0.0；它是仓库内的 vendored 上游实现而非 gitlink。此证据支持排除该实现的普通源码注释审查，但不支持排除同目录下的项目 CMake 包装。

九个包装文件当前均被 `third_party` 路径规则排除，却分别有可达的父级 `add_subdirectory()`，并通过 `zr_register_owned_third_party()` 注册静态目标。Unity 通过 `tests/CMakeLists.txt:74` 的 target-link helper 被测试可执行文件消费；core 的 utf8proc/xxHash targets 在 `zr_vm_core/CMakeLists.txt:22-23` 链接；library 的 cJSON/miniz/tinydir targets 在 `zr_vm_library/CMakeLists.txt:8-10` 链接。逐文件入口、实际源调用与配置约束见 [scope_boundaries.tsv](../coverage/scope_boundaries.tsv)。这些配置应进入首方清单。本批授权只覆盖这两个审查文档；需要源码注释的具体核查工作保留在各行 `TODO` 中，不把文档修订冒充为源码已注释。

`tinydir.c` 另有一处容易被目录名遮住的人工类型单元：本地源只定义 `zr_tinydir_placeholder_t`，真实 tinydir API 位于 `.gitmodules` 指向的子模块头文件，`file.c:528`、`:539`、`:566` 调用该 API。包装把本地占位编译单元注册为静态 target，并把子模块头文件目录公开给 library 消费者。CMake 第 3 行的 `# no src files` 只能说明没有上游 `.c` 实现；它没有解释保留本地占位 typedef 的构建用途，这个问题作为具体 `TODO` 留在台账中。该 C 文件是普通跟踪的本地首方源，不能因目录名将其归为第三方副本。

Windows 分支在 `zr_vm_library/third_party/zr_tiny_dir/CMakeLists.txt:13` 将 `UNICODE` 与 `_UNICODE` 以 `PRIVATE` 加到 `zr_third_party_zr_tiny_dir_static`。这个 target 当前只编译上述占位 typedef；`PRIVATE` 定义不向链接它的 `zr_vm_library/src/zr_vm_library/file.c` 传播，而后者在 `:9` include 子模块头文件并在 `:528`、`:539` 使用 API。因此目前只能确认定义作用于 stub target，不能声称它配置了 `file.c` 的编译，也没有证据证明存在行为缺陷。台账保留下一步：向该 wrapper 与 `file.c` 的维护者核实，Unicode 宏是否仅服务于 stub，或原意是控制 file.c 所见的 tinydir API；在取得证据前不将疑点升级成 `BUG`。

## 夹具与数据文件

`tests/fixtures/syntax_migration_inventory/docs/current-snippets.md` 由 `tests/scripts/test_syntax_migration_inventory.py:97` 传入 `build_inventory(FIXTURE_ROOT)` 扫描，并在 `:106` 检查 `CURRENT_DOCUMENTATION` 分类、`:198` 检查 `%module` finding。这里的 `.md` 缺省只指首方审查清单 scanner `scripts/code_review_inventory.py` 的 `SOURCE_SUFFIXES`（`:42` 起）与 `classify()` 后缀分支（`:101-125`）；语法迁移 scanner 有独立分类逻辑，明确支持该 Markdown 输入（`scripts/syntax_migration_inventory.py:249`）。因此不能把清单 scanner 的后缀限制推广成“项目忽略 Markdown”；夹具确实是测试输入，目录名 `docs` 不改变其角色。

`tests/fixtures/projects/syntax_reference_v1/generated/file_locator_import.zr` 是 Git 跟踪的 3 行占位源码，保留 `${SYNTAX_REFERENCE_FILE_URI}`，不存储绝对路径（该文件 `:1-3`）。parser 测试的 `kCurrentEvidence` 与 current collection 将其列为输入（`tests/parser/test_syntax_reference_v1.c:90`、`:116`），测试函数在 `:413`、`:430` 实际读取该文件，随后由 `main()` 的 `RUN_TEST` `:867` 执行；target 与 CTest 注册位于 `tests/cmake/parser-foundation-targets.cmake:351-363`。同名 `.zrp` 的 project-suite 用例不构成此 `.zr` 路径的消费者，本批不计入它的调用证据。路径名不足以证明它由生成器产出：设计/验证文档称 focused target 会运行时渲染 file URI（`docs/testing-and-validation/syntax-reference-v1-fixture.md:63-66`），但当前测试代码中的 helper 只在内存中格式化路径并验证 URI（`tests/parser/test_syntax_reference_v1.c:168-196`、`:560-575`），没有发现写回或替换已跟踪 `.zr` 文件的调用。故保留具体 `TODO`：核查 fixture owner 是否有本批未覆盖的 producer，或设计文档是否已过时；不据目录名认定它是生成物。

六个 JSON 路径均被 `expected` 或 `golden` 路径段排除，但消费方式不同：

- `machine_forms.json` 是 CLI smoke 的期望报告；脚本在 `tests/cli/syntax_migration_smoke.js:30` 读取它并比较 CLI 输出。`:12` 只构造 golden 路径，`:24` 才是运行 CLI 的位置，二者不是 JSON 的读取点。该脚本注记尚未注册为常规 CTest（`:20-22`），并在 `:29` 记录现存输出与 golden 的 `%module` 分类/plan 不一致；这里保留待维护归属，不裁定产品行为，本批也没有执行 smoke。
- `inventory.json` 在 `tests/scripts/test_syntax_migration_inventory.py:331` 由 `read_text()` 读取，并与 `report.to_fixture_json()` 逐字比较；`:330` 仅构造路径，`:97` 构造被比较的 report。它是不可加源码注释的协议快照；消费者已确认，维护方式不能由 `expected` 目录名推定。
- `repository-inventory.json` 在 `tests/scripts/test_syntax_migration_inventory.py:435-436` 被读取并与 `build_repository_inventory()` 的结果比较；`:433` 构造路径，`:337` 生成实际 report。同一测试源码仍注明仓库旧语法样例可能使空 finding 断言失败（`:335-336`）；本批没有运行测试，也不把该注记升级为新的失败结论。
- `coverage.json` 被 parser 测试多处直接读取（`tests/parser/test_syntax_reference_v1.c:319`、`:447`、`:464`、`:502`）；这些函数核对 schema、feature ID、current/negative/design-pending collection 及必需文件（`:462-483`、`:485-527`）。其相对路径和集合状态是测试数据契约，不是生成物证明。
- `provider-locator.json` 被 parser 测试读取并检查 native/workspace identity 与 URI placeholder，拒绝机器绝对路径（`tests/parser/test_syntax_reference_v1.c:541-575`）。内容有测试消费证据；更新来源仍待维护者确认。
- `diagnostics.json` 当前只作为 required-file 列表项出现在 `tests/parser/test_syntax_reference_v1.c:495-499`；循环在 `:524` 调用 `assert_project_file_exists()`，所属测试由 `main()` 的 `RUN_TEST` `:865` 进入。测试只验证路径存在，不读取或校验 JSON 内容。对这份文件不作语义覆盖声明，台账保留有关格式、producer 与维护者的具体 `TODO`。

这些 JSON 可按数据而非可注释源码处理，但消费者契约和维护状态仍须列账。`expected`/`golden` 命名只能说明约定角色，不能证明文件由当前测试自动生成或全部断言覆盖。

## 后续范围注意

`SOURCE_SUFFIXES` 明确含 `.inc`（`scripts/code_review_inventory.py:42-47`）；本次抽查的生产和测试 `.inc` 文件能找到实际 include 点，例如 `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5302-5303` 与 `zr_vm_library/src/zr_vm_library/task/task_runtime.c:479`。这里确认的是后缀和调用关系，不把行数或 scanner 候选数等同语义审查完成。仓库当前唯一跟踪的 `.h.in` 输入 `zr_vm_lib_ffi/cmake/fficonfig_x86_win64.h.in` 已在共享清单中；`zr_vm_lib_ffi/CMakeLists.txt:48-50` 将它用 `configure_file()` 复制/替换到 `${CMAKE_CURRENT_BINARY_DIR}/generated/libffi/x86_win64/fficonfig.h`。它是人工维护输入，输出是构建目录产物。miniz wrapper 用 `generate_export_header()` 生成 build-dir 头文件（`zr_vm_library/third_party/zr_miniz/CMakeLists.txt:13-20`）；targeted `.zr`/六份 JSON 都没有对应的活动 `configure_file()` 生成入口证据。cJSON 子模块自带的 `CMakeLists.txt` 中虽有 `configure_file()`，当前项目 wrapper 只把 `cJSON.c` 交给项目 target 注册宏，没有 `add_subdirectory(cJSON)`，所以不能把该上游脚本的潜在输出归到当前主构建图。

`worktree_paths()` 用 `git ls-files --cached --others --exclude-standard` 读取路径（`scripts/code_review_inventory.py:75-83`）：已跟踪文件通过 `--cached` 纳入，即使命中 ignore；尚未跟踪且被仓库、信息目录或用户 ignore 规则过滤的文件不可见。本次没有枚举所有 ignored-untracked 路径；先前 Git 读取用户 `core.excludesFile` 时出现权限警告，因此仍需另行核对具体忽略规则下的未跟踪首方候选。不要把本清单或 3965 类 scanner 数量报告为完整语义覆盖。

本批未运行构建或测试。现有 MSVC products configure/build 的通过状态来自 Root 提供的既有基线，本代理未执行该命令；台账中的测试锚点表示代码中实际注册/读取/断言的位置。没有新增产物、源码副本或临时文件。
