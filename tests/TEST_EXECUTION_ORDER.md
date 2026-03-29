# ZR-VM 测试拓扑与执行顺序

本文档描述当前仓库中唯一有效的测试拓扑。CTest 只暴露 5 个分层套件；旧的 `zr_vm_gc_test`、`zr_vm_parser_test`、`zr_vm_scripts_test` 这类分散入口现在只作为套件内部实现细节存在，不再作为活跃测试接口。

## 活跃套件

### 1. `core_runtime`
- 覆盖范围：GC、instructions、meta，以及其他纯 core/runtime 级断言。
- 目标：先验证最底层运行时行为，再给上层语言测试提供稳定基础。

### 2. `language_pipeline`
- 覆盖范围：parser、char/type cast、type inference、compiler features、instruction execution、prototype、const keyword、exceptions、named arguments、module system、compile-time、curated scripts 回归。
- 目标：验证从源码到运行时结果的语言主链路。
- 说明：import/member-call compile-time projection、deep import projection 等语言回归都归入这里。

### 3. `language_server`
- 覆盖范围：语言服务相关的 5 组行为测试。
- 目标：保持语言服务行为断言集中暴露为一个 suite，而不是多个独立 CTest 入口。

### 4. `projects`
- 覆盖范围：项目级 CLI 回归、import-binary fixture 构建链路、native/vector3/numeric pipeline 等工作区项目样例。
- 目标：以数据表驱动的方式验证多项目工作流，而不是“一项目一个 `add_test`”。

### 5. `golden_regression`
- 覆盖范围：版本管理中的 AST / intermediate / binary golden 对比。
- 目标：保证提交到 `tests/golden/` 的快照与当前编译器输出保持一致。

## 执行顺序

推荐按以下顺序执行：

1. `core_runtime`
2. `language_pipeline`
3. `language_server`
4. `projects`
5. `golden_regression`

这个顺序遵循“基础运行时 -> 语言主链路 -> 上层集成 -> 快照回归”的分层原则。`golden_regression` 放在最后，是为了在前面的功能行为已确认有效后再检查产物格式漂移。

## 资产布局

当前有效测试资产统一放在以下目录：

- `tests/fixtures/parser/`
  - parser/compiler 输入样例。
- `tests/fixtures/projects/`
  - `.zrp` 项目和项目源码 fixture。
- `tests/fixtures/scripts/`
  - curated script 回归输入。
- `tests/golden/ast/`
  - 版本管理中的 AST golden。
- `tests/golden/intermediate/`
  - 版本管理中的 intermediate golden。
- `tests/golden/binary/`
  - 版本管理中的 binary golden。

构建时生成的临时产物统一输出到构建目录下的 `tests_generated/`，例如：

- `out/build/msvc/debug/tests_generated/language_pipeline/`
- `out/build/msvc/debug/tests_generated/scripts/`

活跃套件不再依赖以下旧路径：

- `tests/scripts/output/`
- `tests/scripts/test_cases/`
- 仓库根目录下的 `test_simple.zri/.zro/.zrs`

## 运行方式

### 列出当前活跃套件

```bash
ctest -N --test-dir out/build/msvc/debug
```

预期只看到以下 5 个测试：

```text
core_runtime
language_pipeline
language_server
projects
golden_regression
```

### 运行全部测试

```bash
ctest --test-dir out/build/msvc/debug --output-on-failure
```

### 运行单个套件

```bash
ctest --test-dir out/build/msvc/debug --output-on-failure -R language_pipeline
ctest --test-dir out/build/msvc/debug --output-on-failure -R projects
```

### 使用薄封装 runner

```bash
./zr_vm_test_runner --ctest --output-on-failure
./zr_vm_test_runner --ctest --output-on-failure -R golden_regression
```

## 维护规则

1. 新增行为覆盖时，优先并入现有 5 个 suite，不新增第 6 个活跃 CTest 入口。
2. 需要提交的快照只能放到 `tests/golden/`，不能继续写回 `tests/scripts/output/`。
3. fixture 只能放到 `tests/fixtures/*`，不能继续散落在旧测试源码目录下。
4. 只有当旧断言已被等价迁移或被更强断言覆盖后，旧 case 才允许删除。
