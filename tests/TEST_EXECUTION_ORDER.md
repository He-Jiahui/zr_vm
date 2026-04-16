# ZR-VM 测试拓扑与执行顺序

本文档描述当前主仓实际暴露给 `CTest` 的测试入口，以及 `smoke/core/stress` 三档过滤如何叠加到现有 suite 上。主仓现在只覆盖 interp、binary、`.zri` 和项目/CLI 工作流；AOT 已从主链路分离到独立归档目录，不再属于默认测试合同。

## 活跃 Suite

主仓默认聚合入口保持为以下 suite：

1. `core_runtime`
2. `language_pipeline`
3. `containers`
4. `language_server`
5. `language_server_stdio_smoke`
6. `projects`
7. `cli_args`
8. `cli_integration`

可选长耗时入口：

- `performance_report`
  - 仅在配置时设置 `ZR_VM_REGISTER_PERFORMANCE_CTEST=ON` 时注册。
  - 默认性能矩阵只跟踪 `c`、`zr_interp`、`zr_binary` 和外部语言实现。

## 分层职责

- `core_runtime`
  - GC、instruction、meta、runtime probe 的底层执行语义。
- `language_pipeline`
  - parser、SemIR/ExecBC、reference full-stack matrix、小型 parity fixture。
- `containers`
  - `zr.container` 的 metadata、type inference、runtime 行为。
- `language_server`
  - symbol/reference/semantic/incremental/LSP project features。
- `language_server_stdio_smoke`
  - stdio 端到端烟雾验证。
- `projects`
  - 项目级 CLI 回归、source/binary import、多模块工程和大型 fixture。
- `cli_args`
  - CLI 参数、帮助输出和非法组合错误。
- `cli_integration`
  - `--compile`、`--intermediate`、`--run`、`--incremental`、REPL。

## Tier 过滤

`smoke/core/stress` 是叠加在现有 suite 上的过滤维度：

- `smoke`
  - 每次必跑，分钟级预算。
- `core`
  - PR 必跑，展开 happy/negative/boundary/regression。
- `stress`
  - 夜间或手动运行，覆盖长循环、深模块图和大型 fixture。

入口：

- 环境变量：`ZR_VM_TEST_TIER=smoke|core|stress`
- runner 参数：`zr_vm_test_runner --tier <smoke|core|stress> --ctest ...`

## 推荐执行顺序

1. `core_runtime`
2. `language_pipeline`
3. `containers`
4. `language_server`
5. `language_server_stdio_smoke`
6. `projects`
7. `cli_args`
8. `cli_integration`
9. 可选 `performance_report`

## 资产布局

主仓有效测试资产位于：

- `tests/fixtures/parser/`
- `tests/fixtures/projects/`
- `tests/benchmarks/`
- `tests/fixtures/reference/core_semantics/`
- `tests/fixtures/scripts/`
- `tests/golden/ast/`
- `tests/golden/intermediate/`
- `tests/golden/binary/`

调试辅助脚本位于：

- `tests/debug/projects/`

构建生成物统一输出到构建目录下的 `tests_generated/`。

## 运行方式

```bash
ctest -N --test-dir build/codex-wsl-gcc-debug
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure
ZR_VM_TEST_TIER=smoke ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure -R "language_pipeline|projects|cli_integration"
./build/codex-wsl-gcc-debug/bin/zr_vm_test_runner --ctest --output-on-failure
```

## 维护规则

1. 新增回归优先并入现有 suite，不再把已分离的 AOT 路径带回主仓脚本。
2. `smoke/core/stress` 只作为过滤维度，不改一级 suite 组织。
3. golden 快照只提交到 `tests/golden/` 的 AST / `.zri` / `.zro` 主链路目录。
4. AOT 相关源码、测试和历史资产只允许留在 `zr_vm_aot/`，不再参与主仓 `CTest`。
