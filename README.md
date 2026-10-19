# ZR VM

ZR 是面向模块化应用、原生互操作与多后端执行的静态语言。此仓库包含 C11 运行时、解析器、标准库、CLI、语言服务器、VS Code 扩展和 Rust 绑定。

项目仍在开发中。语言表层、已实现行为与验证状态见 [语言指南](docs/wiki/language-guide.md)、[语言规范](docs/zr_language_specification.md)和 [Wiki](docs/wiki/README.md)。

## 获取源码

```sh
git clone --recurse-submodules https://github.com/He-Jiahui/zr_vm.git
cd zr_vm
```

已有 checkout 可以运行 `git submodule update --init --recursive`。GitHub 的源码压缩包不包含子模块内容；xxHash、utf8proc、cJSON、tinydir、libffi、libuv 和 Unity 缺失时无法完整构建。

## 构建与运行

需要 CMake 3.20 或更高版本，以及 C11 编译器。完整测试还需要 C++ 编译器。Windows 使用 Visual Studio 2022 的 x64 开发者命令环境；Linux/macOS 可用 GCC 或 Clang。Ninja 可选。Rust/Cargo、Node.js/npm 和 Emscripten 分别用于绑定、扩展和 WASM 构建。

Linux/macOS 的最小 CLI 构建：

```sh
cmake -S . -B ../zr-vm-build -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=OFF -DBUILD_RUST_BINDING=OFF
cmake --build ../zr-vm-build --target zr_vm_cli_executable --parallel 8
../zr-vm-build/bin/zr_vm_cli tests/fixtures/projects/hello_world/hello_world.zrp
```

成功时输出 `hello world`。启用 `BUILD_TESTS=ON` 后可构建测试并运行 `ctest --test-dir ../zr-vm-build --output-on-failure`；多配置生成器需要额外传 `--config Debug` 和 `ctest -C Debug`。

本仓库的受管理 Codex checkout 按 `.codex/config.toml` 使用 **E:/cargo-targets** 存放产物和缓存。Windows 示例：

```powershell
cmake -S . -B E:/cargo-targets/zr_vm/build/local-msvc `
  -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_TESTS=OFF -DBUILD_RUST_BINDING=OFF
cmake --build E:/cargo-targets/zr_vm/build/local-msvc `
  --config Debug --target zr_vm_cli_executable --parallel 8
& E:/cargo-targets/zr_vm/build/local-msvc/bin/Debug/zr_vm_cli.exe `
  tests/fixtures/projects/hello_world/hello_world.zrp
```

受管理 WSL 构建使用 `/mnt/e/cargo-targets/zr_vm/build/` 下的独立 GCC/Clang 配置，不在仓库中创建 build/target 或源码快照。普通 checkout 不需要特定 Windows 盘符。

## 可选模块

- Rust 绑定默认启用；没有 Cargo 时仍可配置原生库，Cargo 检查/测试目标会提示工具不可用。工作区提交 `zr_vm_rust_binding/rust/Cargo.lock`，检查使用 `--locked`。普通构建默认在 CMake binary 目录下生成 Cargo 产物；显式设置 `ZR_VM_CARGO_TARGET_ROOT` / `ZR_VM_CARGO_OUTPUT_ROOT` 时执行受管理物理路径校验。详见 [Rust 绑定构建](docs/bindings/rust-build-and-generics.md)。
- VS Code 扩展统一使用 npm 和 `package-lock.json`。在 `zr_vm_language_server_extension` 中运行 `npm ci`，之后运行 `npm run test:unit`。打包步骤见该目录的 `scripts/package-vsix.ps1`。
- WASM 语言服务器需要 Emscripten。`build_wasm_for_extension` 使用独立的 Emscripten 构建目录；Windows 路径由构建脚本转换为 WSL 路径，宿主 CMake cache 不会改成 WASM 配置。
- `ZR_VM_ENABLE_HOST_JIT=ON` 启用可选 host JIT，并需要对应的 C++/LLVM 工具链。

## 目录

| 路径 | 内容 |
| --- | --- |
| `zr_vm_common`, `zr_vm_core` | 公共 ABI、内存、对象和字节码运行时 |
| `zr_vm_parser` | 词法、语法、语义和编译后端 |
| `zr_vm_library`, `zr_vm_lib_*` | 项目系统、标准模块和原生能力 |
| `zr_vm_cli` | 项目运行、构建、REPL 和调试入口 |
| `zr_vm_language_server*` | 原生/WASM LSP 和 VS Code 扩展 |
| `zr_vm_rust_binding` | C ABI、Rust sys crate 和安全封装 |
| `zr_vm_aot` | 当前 AOT backend，以及被契约测试使用的历史参考文件，见 [目录状态](zr_vm_aot/README.md) |
| `tests` | 回归、集成、固定输入与验证 harness |
| `docs` | 语言指南、模块文档、设计与验收记录 |

更多操作见 [构建与第一个项目](docs/wiki/02-getting-started.md)、[CLI 文档](docs/cli-and-tooling/zr-vm-cli-command-system.md)和 [测试验证](docs/wiki/04-tools/testing.md)。
