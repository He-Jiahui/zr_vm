---
related_code:
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/common_state.h
  - tests/fixtures/projects/hello_world
  - tests/fixtures/projects/syntax_reference_v1
implementation_files:
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli
  - zr_vm_library/src/zr_vm_library/project/project.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/cli-and-tooling/zr-vm-cli-command-system.md
tests:
  - tests/fixtures/projects/hello_world/hello_world.zrp
  - tests/cmake/run_cli_suite.cmake
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
doc_type: workflow-detail
---

# 构建与第一个项目

**状态：`current`**

本页给出从源码 checkout 到运行 ZR 项目的最短路径。命令按仓库现有 CMake target 编写；完整选项见 [CLI](04-tools/cli.md)。

## 环境

必需：

- CMake 3.12 或更高版本。
- C11 编译器：Linux/macOS 上的 GCC 或 Clang，Windows 上的 Visual Studio 2022 MSVC。
- Ninja 可选；Visual Studio generator 使用 `--config` 选择配置。

可选：Node.js/npm（VS Code extension）、Rust/Cargo（Rust binding tests）、Emscripten（WASM LSP）、libffi（FFI provider；Windows 可使用 bundled fallback）。

## Linux/WSL GCC

```bash
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build/codex-wsl-gcc-debug -j 8
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure --parallel 8
./build/codex-wsl-gcc-debug/bin/zr_vm_cli \
  ./tests/fixtures/projects/hello_world/hello_world.zrp
```

CLI smoke 的成功标志是项目输出 `hello world`。仓库当前可能有已知 baseline 失败；不要只看一个 smoke 就声称全套测试通过，使用[验证页](04-tools/testing.md)记录实际失败集合。

## Linux/WSL Clang

```bash
cmake -S . -B build/codex-wsl-clang-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build/codex-wsl-clang-debug -j 8
ctest --test-dir build/codex-wsl-clang-debug --output-on-failure --parallel 8
```

GCC 与 Clang 都是主验证环境。共享 header、CMake、parser/core/library 或跨模块测试变更后应重跑两套 toolchain。

## Windows MSVC smoke

```powershell
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug `
  --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe `
  .\tests\fixtures\projects\hello_world\hello_world.zrp
```

## 项目最小结构

`SZrLibrary_Project` 从 `.zrp` JSON manifest 读取以下逻辑字段：

```text
hello_world/
├─ hello_world.zrp       # project manifest
├─ src/
│  └─ main.zr            # module source
├─ bin/                  # .zro/.zri generated artifacts
└─ lib/                  # optional .zrm/package/native outputs
```

一个当前语法的入口源文件可以是：

```zr
module hello.main;

let system = import("zr.system");

fn main(): int {
    system.console.printLine("hello world");
    return 0;
}
```

具体 entry 选择由 manifest 的 `entry` 和 CLI command 决定；不要假设所有项目都必须导出名为 `main` 的函数。

## 执行模式

CLI 和 library runtime 都区分：

| 模式 | loader 行为 | 用途 |
|---|---|---|
| interpreter/source-first | 从 source 编译并执行，必要时产生 artifact | 开发、诊断、源码路径验证 |
| binary-first | 优先加载 `.zro`，必要时读取 `.zri`/metadata | 发布和快速启动 |
| AOT C/LLVM | 读取 canonical artifact，生成/加载 native entry | 生产性能和静态部署 |
| test | 建立测试 roots/manifest，执行测试隔离和报告 | 回归验证 |

模式不是语义开关：相同输入应保持返回值、异常分类、资源清理和模块 identity 一致；不一致应视为 backend/contract 缺陷。

## 常见故障定位

| 症状 | 优先检查 |
|---|---|
| 找不到项目 | `projectPath` 是否指向 `.zrp` 或项目目录，manifest `entry`/`source` 是否存在。 |
| 找不到导入 | module specifier 是否规范化，alias/package export 是否在 manifest，provider 是否已注册。 |
| stale/ABI mismatch | `.zro/.zri/.zrm` 与当前 runtime 的 schema/layout/signature hash 是否匹配，删除并重新生成产物。 |
| native symbol 错误 | descriptor module name、版本、能力位、provider phase 和 FFI signature。 |
| Windows 运行时 DLL 错误 | `ZR_VM_HOST_LIB_DIR`、PATH 以及 shared library 输出目录。 |
| CTest 大量失败 | 先区分 baseline 与本次变更；用单个 test executable 和 sanitizer 复现。 |

## 下一步

- 学习语法：[语言语法参考](03-language-syntax.md)。
- 嵌入宿主：[C API 总览](05-interop/c-api.md)。
- 需要 IDE：[语言服务器](04-tools/language-server.md)。
