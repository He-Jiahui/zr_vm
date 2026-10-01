---
related_code:
  - CMakeLists.txt
  - zr_vm_common/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/ThirdPartyMacros.cmake
  - zr_vm_common/include/zr_vm_common/zr_api_conf.h
  - zr_vm_core/CMakeLists.txt
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_library/CMakeLists.txt
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_lib_thread/CMakeLists.txt
  - zr_vm_lib_ffi/CMakeLists.txt
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - tests/fixtures/projects/hello_world/hello_world.zrp
implementation_files:
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/CMakeLists.txt
  - zr_vm_common/ThirdPartyMacros.cmake
plan_sources:
  - user: 全仓库首方代码调用链审查与意图注释任务
  - docs/code-review/comment-standard.md
tests:
  - tests/fixtures/projects/hello_world/hello_world.zrp
doc_type: build-contract
---

# 首方模块构建契约

## 调用入口与目录关系

顶层先加入 `zr_vm_common`，发布 `zr_vm_common_src` 源列表；各产品模块与测试配置再 include `CommonMacros.cmake`。include 同时提供第三方注册与链接函数，真正注册第三方目标由其拥有者另行执行。

| 入口 | 当前使用者与目的 | 调用限制 |
| --- | --- | --- |
| `zr_declare_module` | core、parser、library 与其他产品库建立启用的 static/shared 变体 | 目标 basename 全图唯一；`ON` 使用已发布的 common 源清单 |
| `zr_declare_executable` | CLI 建立唯一 executable 目标，并按 `ON` 编入 common 源 | 后续链接依赖所选库变体；CLI 自行声明安装 |
| `zr_link_library_for_module` | 产品库将首方同类型变体作为私有依赖 | 当前模块已创建，依赖目标在配置结束前存在；公开头的依赖使用要求由调用方补充 |
| `zr_link_internal_for_module` | core 链接线程/系统数学库，math 链接已找到的系统数学库 | 参数是目标、系统库名或路径；平台与依赖查找由调用方处理 |
| `zr_link_library_for_executable` | CLI 累积首方库依赖 | 当前 executable 已创建，库存在；同开两种库时普通 CLI 依赖选择 static |
| `zr_install_module` | 产品库声明启用变体的安装，并传播裸模块名定义 | 构建选择与声明阶段一致；模块头安装由各模块另外声明 |

`module_name` 是逻辑目标 basename，内部源码根取当前目录名。debug、thread、task 的逻辑名与目录名不同，不能把这两个名字合并为同一约束。首方链接函数拼接的私有头路径按依赖逻辑名生成；依赖目标自身的 PUBLIC include 同时提供它实际的公共头目录。

模块自己的 C 源通过 `GLOB_RECURSE ... CONFIGURE_DEPENDS` 收集，parser 的 AOT 目录由 parser 配置额外加入。common 源列表来自另一个 glob，当前没有该标志；新 common 源的增量发现仍需核查其配置入口。

## 前向依赖与公开头

parser 在顶层加入 library 目录之前请求 library 依赖。当前构建图支持在配置结束前解析该目标，因此接口要求依赖最终存在。要求每个依赖在调用瞬间已经加入，会与当前顶层顺序矛盾。

模块链接入口使用 PRIVATE。依赖的 PUBLIC include 为当前模块编译提供头路径；如果当前模块公开头中继续暴露依赖类型，还须自行声明 PUBLIC 使用要求。parser 公开头暴露 library 类型，所以它分别为 static/shared 目标补充 library 的 PUBLIC include。

## 标志、链接与安装职责

声明模块时，`ZR_CURRENT_MODULE` 是供版本日志使用的私有字符串，`ZR_LIBRARY_TYPE_STATIC` / `SHARED` 是库种类标志。Windows `ZR_API` 会读取共享标志；依赖头同时带 dllexport 的边界保留头文件原有 TODO，构建成功不能单独确认该导出契约。

安装入口另外传播裸模块名宏。目前首方条件编译检索未发现这组裸宏的消费方；`TODO:` 核查外部嵌入和导出包的编译定义使用，再决定 PUBLIC 传播目的。版本日志使用私有 `ZR_CURRENT_MODULE`，不能作为裸宏的消费证据。

CLI 的普通首方链接同开两种库时选择 static；生成外部产物使用的宿主库目录在 shared 可用时选择 shared 的目录。这两个配置面分别服务 CLI 本体和外部生成产物，调用链审查须分别读取。

## 已确认问题与待核查边界

### Windows 双库输出碰撞

`BUG:` 在 Windows 同时启用 static/shared 时，两个目标设置相同 `OUTPUT_NAME`，顶层也给它们设置相同 archive 输出目录。静态归档和共享导入库都落为 `lib/<module>.lib`。实际 MSVC19.44.35228.0 + Ninja 配置报 `multiple rules generate lib/zr_vm_core.lib` 并退出1。

项目默认 static OFF、shared ON 的独立缓存配置、CLI 构建和现有 hello_world 验证通过。本轮在产生相同 basename 的位置备注问题，保持所有构建命令及输出选择不变。

### 同一 executable 混合链接签名

`BUG:` 对一个已声明 executable 先调用首方 plain `target_link_libraries` 入口，再调用已注册第三方的 PRIVATE 签名入口，CMake 会拒绝签名混用。两个入口的合法组合前提包含目标创建、首方变体存在及第三方注册。当前产品 CLI 的相关调用均为 plain，已有标签记录的是 API 组合缺陷。

### thread 调用方使用了目录名

`BUG:` 启用 `BUILD_THREAD_LIB` 后，thread 配置用逻辑名 `zr_vm_thread` 创建库目标。CLI 的可选依赖分支与 Rust binding 的能力宏、链接分支却查询 `zr_vm_lib_thread_static/shared`；这些目标不存在，因此分支跳过。CLI project support 随后按 `ZR_VM_HAS_THREAD_MODULE` 控制头文件和 `ZrVmThread_Register`，所以构建成功仍会省略 thread 模块注册。LSP 查询 `zr_vm_thread_*`，与实际目标一致。

证据入口为 `zr_vm_lib_thread/CMakeLists.txt:2/:4`、`zr_vm_cli/CMakeLists.txt:64/:65/:67`、`zr_vm_rust_binding/CMakeLists.txt:27/:29/:65/:66` 和 `zr_vm_cli/src/zr_vm_cli/project/project.c:26/:370/:371`。`TODO:` 后续审查 CLI、Rust binding 及 tests 的相关调用块时，在各自调用位置补充同一触发条件的标签；目标名与能力宏的行为修复另行处理。此批源码注释只涉及已独立审查的 CommonMacros 文件。

### 其他边界

common 新源的增量发现、安装裸模块名宏的外部消费以及 Windows 共享依赖的导出声明仍按各自 TODO 核查。未确认的设计意图保持可追踪的下一入口。

## 审查与验证范围

台账按六函数、一次 include 和八个独立契约块登记15个单元。简单目标名赋值及普通条件块随接口审阅，没有按括号数量增加单位。当前首方扫描按 `scripts/code_review_inventory.py` 分类规则进行，排除 `third_party`，保留人工编写的 `.cmake` 样例。

本轮保留源码161行，非注释 CMake 命令逐行相同，避免本批注释移动其他源码锚点。八条旧宏记录迁至专用台账；common 目录其他文件及其旧式证据仍有独立返工任务。

2026-10-01 修改前验证复用专属 MSVC shared-only 缓存，显式配置同一项目、构建 `zr_vm_cli_executable`、运行现有 `tests/fixtures/projects/hello_world/hello_world.zrp`，三个步骤均退出0，输出 `hello world`。该有限验证不覆盖 Windows 双库配置、其他平台完整矩阵或全仓测试。修改后对照及本批提交结论以验收记录为准。
