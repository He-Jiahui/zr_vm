---
related_code:
  - CMakeLists.txt
  - zr_vm_common/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/include/zr_vm_common/zr_api_conf.h
  - zr_vm_library/include/zr_vm_library/conf.h
  - zr_vm_lib_math/include/zr_vm_lib_math/module.h
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - scripts/audit_magic_constants.py
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/module/test_module_system.c
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/conf.h
  - zr_vm_lib_math/CMakeLists.txt
tests:
  - tests/module/test_module_system.c
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/cmake/run_projects_suite.cmake
  - docs/wiki/03-modules/math-api.md
plan_sources:
  - docs/code-review/comment-standard.md
  - E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/math-config-source-leases.json
doc_type: code-review-batch
---

# Math 编译配置与常量契约

## 范围与单元

本批只覆盖 `zr_vm_lib_math/include/zr_vm_lib_math/conf.h`、`zr_vm_lib_math/CMakeLists.txt` 及本批 leaf 台账。配置头审查 6 个宏、文件职责和 include guard；CMake 将声明为 1 个 target 配置单元，共 9 个唯一键。台账结论为 7 `commented`、1 `no-comment`、1 `TODO`。新增 Doxygen 集中在 conf.h 的 EOF `@file`/`@def` 块，CMake 文件说明放在原有 19 行命令之后，原宏定义与 CMake 命令的物理行位置保持不变。未修改数值实现、模块 descriptor、测试或共享文档。

## 宏定义与实际消费

`ZR_VM_LIB_MATH_API` 是 `ZR_API` 的模块前缀别名，但仓内 Math 声明在 `module.h` 中仍直接使用 `ZR_API`；排除 third-party、`.git`、`.claude`、`.codex`、build/target 后的仓内可检索内容未发现该别名的展开点。注释只记录当前可见事实，并把仓外嵌入者兼容性留作具体 `TODO`，不称该别名已控制现有符号导出，也不据此判为可删除。

`ZR_MATH_PI` 同时进入 `g_math_constants.PI` 和 Scalar 的 Degrees/Radians 公式。`ZR_MATH_TAU` 只在 descriptor 的 `TAU` 值展开为 `PI * 2.0`；`ZR_MATH_E` 只进入 `E` descriptor。三项都是编译期浮点表达式/字面量；模块可见性还需要 `module.c` 中独立的静态常量表及模块注册过程。

`ZR_MATH_EPSILON` 固定为 `1.0e-9`，供默认 `almostEqual` 参数及 vector、quaternion、complex、matrix 的不同近零/主元分支复用。它不是统一比较函数：`almostEqual` 比较绝对差，其它算法使用各自的长度、平方长度或主元条件；explicit epsilon 也不由该宏覆盖。

`ZR_MATH_FORMAT_BUFFER_LENGTH` 在 `ZrMath_MakeStringResult` 的栈数组声明处展开；该 helper 的公开声明位于 `math_common.h:929`、实现位于 `common.c:512`，并由八个 `MetaToString` callback 调用。helper 的数组声明是唯一宏展开点，不是第九个 callback。`vsnprintf` 最多写入容量减去终止字节的内容，返回长度未被消费，因此超长格式字符串以可容纳前缀继续走字符串 setter。`audit_magic_constants.py` 里的名称是静态审计规则的 target 文本，不是运行时数值消费。现有仓内测试未发现 PI/TAU/E/EPSILON 数值断言，也没有该固定缓冲区的长字符串截断用例。

## CMake 配置入口与依赖

顶层 `CMakeLists.txt` 先加入 common、随后加入 Math。`zr_declare_module(... ON)` 使用 common 的缓存源清单，按 `BUILD_STATIC_LIB` / `BUILD_SHARED_LIB` 建立可选变体，并从 Math 的 `src/*.c` 配置源；其余 helper 把 `zr_vm_core`、`zr_vm_library` 的同类变体接入，并配置 public include 路径、依赖顺序与模块安装。安装规则同时复制 Math 的公开 include 目录；库目标安装仍受上述静态/共享开关控制。

非 Windows 分支对系统库名 `m` 调用 `find_library`，只有查找结果为真才把解析路径交给 `zr_link_internal_for_module`，没有 `REQUIRED` 或失败时强行加入链接项。Windows 不进入该分支。本批只说明当前配置的条件关系，不声明某个平台实际成功找到了 `m`。

`zr_vm_link_parser_core_plus_library` 的定义在 `tests/CMakeLists.txt:198–220`：`BUILD_SHARED_LIB` 为真时链接 Math shared，否则链接 Math static；`tests/cmake/compiler-project-targets.cmake:224` 只是为 `zr_vm_module_system_test` 调用该 helper 的位置。`test_module_system.c` 直接注册 Math 并测试 `sqrt` 导出路径。`native_math_export_probe` 项目调用 `sqrt` 和 `Matrix4x4.scale`，runner 登记该 project case 并检查项目结果。它们给出目标连线与模块使用入口，不验证此 CMake 文件的非 Windows `m` 分支，也不单独断言上述常量值或 `ZR_VM_LIB_MATH_API` 别名。

## 测试与验证边界

本 producer 只读核对已有 CMake target 和项目 runner 定义；producer 没有运行测试或构建。Root 的最终运行结果另列验收段。模块测试中的 Math 注册/导出断言仅覆盖 `sqrt` 路径和模块可用性。项目样例的结果检查来自平方根与 Matrix scale 的组合，不是常量配置测试。`find_library(m)` 的平台分支、所有常量数值和 512 字节格式截断均无本批运行证据。

本批源代码只添加注释，不改 C / CMake 程序命令、宏展开或数值行为。严格词法等价和后续构建验证由 Root 独立处理；此处不把未执行的验证写成通过。


## Root 规范台账与词法验收

旧 zr_vm_lib_math.tsv 剩余7个配置/构建键已逐项映射至本批9键；保留6个宏的同名身份，target配置键明确更名，新增文件职责和include guard记录。旧表目前仅保留表头，不与新表重复计数。ZR_VM_LIB_MATH_API从旧的泛化“全部公开头消费者”说明收紧为仓内零展开点，仓外兼容性留具体TODO；不据此断言可删除。9键为7 commented、1 no-comment、1 TODO，没有未审查项。

Root亲读两源全部163行、9条leaf、完整batch及tests/CMakeLists.txt:198–220的实际链接helper。math-config-full-macro-build-caller-evidence-r1.json记录6宏定义、17个程序消费及TAU派生引用；8个MetaToString callback调用格式helper，helper自身不算第9 callback。直接module.h包含与math_common.h经module.h间接包含已区分，测试helper定义与调用点已更正。

严格证明 math-config-final-r1-comment-equivalence.json：39个程序token、两个lexer、token种类/拼写/词法flags及原物理行一致。math-config-final-r1-cmake-command-equivalence.json：原19行命令逐行不变，EOF只追加单行注释与空行；不新增配置行为。最终实际native及独立exactstage签收由Root后续验收段记录，本段不把待运行验证写成通过。不新增重复实现的测试或源码副本。

## Root 实际构建与本批验收

2026-10-15 Root 复用实际 checkout 的 clang-cl-products-debug，运行 math-config-fs-entry-final-native-r1：build exit 0（75.453 s，105 条 warning 行、0 error）。Module 70/78、Provider 9/9、Fs 7/8；三个 suite 的逐项 case lines 与 tensor-stream-final-native-r1 完全一致，保留原8个Module失败及Fs测试断言1035的cached native member调用失败。results SHA256 de826a391eccc12ca129ff63a853b4d7ceddafa84cb913554edcf126c68a4384；comparison SHA256 032bf97f9a3510a41d86f468022a95bcdd72ceaacf96de67858e12dea9181694。

当前 conf.h 与 Math CMake 原始字节都由该构建直接使用；3216输入及31冻结源码在运行前后稳定。Math配置头的39程序token与原CMake19行命令等价已独立验证；本次没有常量数值、格式截断、非Windows m分支或shared变体的专项执行证据。修改前全量构建因其他会话Parser输入失败，不能作注释的隔离行为归因。未新增测试或源码副本。

9键清单无未审查项，旧表7键规范迁移且不重复计数；Root和独立reviewer逐项暂存验收、三次cached check及最终提交身份另见 math-config-index-r1-receipt.json、math-config-root-staged-r1-audit.json 和 math-config-independent-staged-review.json。He-Jiahui串行提交本批6路径，其他会话暂存及inventory其余行保留。

自有lexer/build/test子进程均已退出，本批实际释放0 B；构建、缓存、索引及必要失败证据保留。Fs运行夹具保留36文件/117 B，原32文件/104 B未改。此前自动清理被策略拒绝，未重试或绕过，产物清理仍未关闭。
