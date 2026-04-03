# ZR-VM 测试拓扑与执行顺序

本文档描述当前仓库中实际暴露给 CTest 的测试入口，以及 `smoke/core/stress` 三档过滤如何叠加到现有 suite 上。AOT 不是另一套平行测试世界，而是现有 reference/project/golden/CLI 资产上的第二维合同。

## 活跃 Suite

当前有效的 CTest 入口固定为 9 个：

1. `core_runtime`
2. `language_pipeline`
3. `containers`
4. `language_server`
5. `language_server_stdio_smoke`
6. `projects`
7. `cli_args`
8. `cli_integration`
9. `golden_regression`

不再把旧的单个二进制入口描述为“活跃 suite”。诸如 `zr_vm_gc_test`、`zr_vm_parser_test`、`zr_vm_scripts_test` 这类目标只作为 suite 内部实现细节存在，由 `tests/cmake/run_executable_suite.cmake`、`tests/cmake/run_projects_suite.cmake`、`tests/cmake/run_cli_suite.cmake` 统一编排。

## 分层职责

### `core_runtime`
- 覆盖范围：GC、instruction、meta、runtime probe。
- 角色：承接 L3 `Runtime Path Probes`，锁住最底层执行语义。

### `language_pipeline`
- 覆盖范围：parser、SemIR/ExecBC/AOT pipeline、reference full-stack matrix、小型 parity/contract fixture。
- 角色：承接 L0/L1/L2 的小夹具矩阵。

### `containers`
- 覆盖范围：`zr.container` 的 metadata、type inference、runtime 行为。
- 角色：作为项目/语言夹具的共享支撑层。

### `language_server`
- 覆盖范围：symbol/reference/semantic/incremental/LSP project features。
- 角色：保持语言服务回归集中暴露为一个 suite。

### `language_server_stdio_smoke`
- 覆盖范围：stdio 端到端烟雾验证。
- 角色：确认独立可执行入口仍可被 Node 驱动。

### `projects`
- 覆盖范围：项目级 CLI 回归、source/binary import、多模块工程、大型 fixture。
- 角色：承接 L4 `Project Fixtures`，包括：
  - `aot_module_graph_pipeline`
  - `aot_dynamic_meta_ownership_lab`
  - `aot_eh_tail_gc_stress`

### `cli_args`
- 覆盖范围：CLI 参数与帮助输出。
- 角色：锁定命令面合同。

### `cli_integration`
- 覆盖范围：`--compile`、`--intermediate`、`--run`、`--incremental`、REPL。
- 角色：验证 compile/run/source-binary roundtrip 工作流。

### `golden_regression`
- 覆盖范围：`tests/golden/` 下的 AST / `.zri` / `.zro` / AOT-C / AOT-LLVM 快照。
- 角色：锁定稳定 artifact contract，而不是仅比较“能不能生成”。

## Tier 过滤

`smoke/core/stress` 不是新的顶层 suite，而是叠加在现有 suite 上的过滤维度：

- `smoke`
  - 每次必跑，分钟级预算。
  - 每个覆盖带至少保留一个最小夹具。
- `core`
  - PR 必跑。
  - 展开 happy/negative/boundary/combination/regression。
- `stress`
  - 夜间或手动运行。
  - 放长循环、深 CFG、重复 deopt/requickening、深模块图、大 fixture 全规模版本。

当前 tier 过滤入口：

- 环境变量：`ZR_VM_TEST_TIER=smoke|core|stress`
- runner 参数：`zr_vm_test_runner --tier <smoke|core|stress> --ctest ...`

项目/AOT 路径证明还使用：

- `ZR_VM_REQUIRE_AOT_PATH=1`
  - 对声明了 `require_aot_path` 的项目 case 启用严格检查。

## 推荐执行顺序

推荐顺序仍然遵循“底层支撑 -> 语言主链路 -> 项目/CLI -> 快照回归”：

1. `core_runtime`
2. `language_pipeline`
3. `containers`
4. `language_server`
5. `language_server_stdio_smoke`
6. `projects`
7. `cli_args`
8. `cli_integration`
9. `golden_regression`

其中 `golden_regression` 保持最后执行，用于在功能与项目工作流已确认有效后，再锁定 `.zri/.zro/AOT C/AOT LLVM` 漂移。

## 资产布局

当前有效测试资产统一放在以下目录：

- `tests/fixtures/parser/`
- `tests/fixtures/projects/`
- `tests/fixtures/reference/core_semantics/`
- `tests/fixtures/scripts/`
- `tests/golden/ast/`
- `tests/golden/intermediate/`
- `tests/golden/binary/`
- `tests/golden/aot_c/`
- `tests/golden/aot_llvm/`

构建时生成的临时产物统一输出到构建目录下的 `tests_generated/`，例如：

- `build/codex-wsl-gcc-debug/tests_generated/language_pipeline/`
- `build/codex-wsl-gcc-debug/tests_generated/scripts/`
- `build/codex-wsl-gcc-debug/tests_generated/scripts/aot_c/`
- `build/codex-wsl-gcc-debug/tests_generated/scripts/aot_llvm/`

## 运行方式

列出当前活跃 suite：

```bash
ctest -N --test-dir build/codex-wsl-gcc-debug
```

运行全部：

```bash
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure
```

运行指定 suite：

```bash
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure -R '^language_pipeline$'
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure -R '^projects$'
```

运行指定 tier：

```bash
ZR_VM_TEST_TIER=smoke ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure -R 'language_pipeline|projects|golden_regression'
ZR_VM_TEST_TIER=core ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure
```

使用薄封装 runner：

```bash
./build/codex-wsl-gcc-debug/bin/zr_vm_test_runner --ctest --output-on-failure
./build/codex-wsl-gcc-debug/bin/zr_vm_test_runner --tier smoke --ctest --output-on-failure -R "language_pipeline|projects|golden_regression"
```

## 维护规则

1. 新增覆盖优先并入现有 9 个 suite，不新增长期第 10 个顶层 CTest 入口。
2. AOT 测试继续复用 `tests/fixtures/reference/core_semantics/`，不新开平行 reference 根目录。
3. `smoke/core/stress` 作为标签/runner filter/CI job 维度叠加，不改一级 suite 组织。
4. 正式矩阵中的 AOT case 必须同时具备前端 `source` oracle 和至少一个后端 `artifact/parity/probe` oracle。
5. golden 快照只提交到 `tests/golden/`，不要回写旧的散落输出目录。
