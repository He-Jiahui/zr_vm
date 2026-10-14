---
related_code:
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/include/zr_vm_lib_math/conf.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/value/value.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/conversion.h
  - zr_vm_library/src/zr_vm_library/common_state.c
  - tests/cmake/run_projects_suite.cmake
  - tests/cmake/lsp-cli-smoke-registration.cmake
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix4x4.h
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix4x4_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 Matrix4x4 全链审查与文档注释任务
  - root: Matrix4x4 六路径写入授权与验证范围
tests:
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/lin_alg.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr
  - tests/cmake/run_projects_suite.cmake
  - tests/language_server/test_semantic_analyzer.c
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
doc_type: code-review-batch
---

# Matrix4x4 契约审查批次（2026-10-14）

本批限四份 Matrix4x4 头文件与实现、逐单元覆盖台账及本说明。源码只增加中文 Doxygen 与局部契约注释，不调整声明、数值行为或 ABI。全部 table entry 作为独立配置单元逐项登记，函数指针表项按 descriptor 消费和 dispatcher 间接调用追踪，不把数组元素称为直接函数调用。

## 构建入口与调用链

`zr_vm_lib_math/CMakeLists.txt:1,4` 引入 `CommonMacros.cmake` 并声明 math 模块；`CommonMacros.cmake:6,9` 以递归源文件 glob 纳入模块实现。`module.c:119,131,144` 分别聚合自由函数、类型与 hint getter。类型 descriptor 返回字段、方法及 meta 数组地址；`native_binding_metadata.c:1296-1396` 消费方法/meta 记录，`native_binding_dispatch_lanes.h:192-205` 解析表内 callback 指针并在 `:277` 间接调用，generic dispatch 入口在 `native_binding_dispatch.c:615`。乘法元方法从 `execution_dispatch.c:7074` 进入；对象字符串化的调用方见 `value.c:329-335,545`。

Matrix4x4 字段按 row-major 的 `m00` 到 `m33` 顺序与 constructor `kFields`、`ZrMath_ReadMatrix4Object` 对齐。运算按 column vector 解释矩阵，平移落在 m03/m13/m23，旋转参数为弧度。native context、self 和结果槽都借用当前同步 callback frame，不跨回调保留；矩阵和向量中间值用独立栈快照。

## 数值结果与失败状态

Determinant 与 Inverse 共享固定绝对主元阈值 `ZR_MATH_EPSILON`（`conf.h:16`）。可逆矩阵 `diag(1e-10,1e10,1,1)` 的精确 determinant 为 1，但当前首主元比较使结果为 0。另一有限对角矩阵 `diag(1.5e154,1.5e154,2e-9,2e-9)` 的每个 pivot 都高于阈值且精确 determinant 为 `9e290`；实现累乘头两个 pivot 时 double 先溢出为 Inf。Inverse 对条件数为 1 的 `diag(1e-10,1e-10,1e-10,1e-10)` 也因首 pivot 返回普通 false，VM 状态仍正常时 dispatcher 将 false 归一化为 null；公开返回类型仍标 Matrix4x4，数值阈值政策及 false-to-null 是否符合脚本契约保留为两个 TODO。

MulVector 和 MulMatrix 分别具有可达的有限输入抵消反例。矩阵首行与向量首列取 `[1e200,1e200,0,0]`，另一侧取 `[1e200,-1e200,0,0]` 时精确结果为 0；两个乘积先分别成为正、负 Inf，double 累加得到 NaN。台账分别覆盖 helper 的数值实现和 callback 的输入读取/结果发布链；本批没有运行数值探针。

## 构造器和字段发布

Constructor descriptor 的 metadata 范围是 0..16，但 callback 只接受 0 或 16；1..15 会抛出范围看似允许中间值的 ArityError。零参数构造单位矩阵，十六参数按 descriptor 字段顺序尝试写入；compiler 兼容 receiver 可复用为构造目标。

新对象工厂和 Constructor 都经 void 字段 setter 发布 nodeMap。对象映射初始容量 4、resize threshold 3；无故障成功字段在第 4/7/13 次插入（m03/m12/m30）分别跨过 3/6/12 阈值。默认 `ZrLibrary_CommonState_BuiltinAllocator` 将有效旧块传给 realloc；若仅其中一次 HASH_BUCKET 扩容申请软失败、realloc 保留原桶且其它对象/key/value pin、key 创建和分配均成功，setter 不返回失败状态、该字段缺失而循环继续，回调仍可交付对象。失败插入不增加 count，所以后续跨阈值序号会顺延。本批未注入分配失败；自定义 allocator 是否也保证保留旧块仍待核，入口为 `zr_vm_core/include/zr_vm_core/memory.h:17`。

## 字符串结果

Matrix4x4.identity() 格式化样本含16个0/1分量，ASCII结果长度为50字节，小于短串上限；其它矩阵的 `%g` 分量使结果长度变化，短/长输出均可达。在 API-cache 未命中、短串驻留 HASH_BUCKET 扩容 `Memory_Allocate` 返回 null 的单一软失败条件下，`String_Create` 返回 null，`SetStringObject` 不写预置结果槽，但 `MakeStringResult` 仍返回 true。该链是短串驻留桶的软失败，不是 RawMalloc 或 VM 抛错。

较长文本经栈局部缓冲进入 `CreateTryHitCache`，地址决定缓存桶；命中项比较会走 `ZR_CAST_STRING_TO_NATIVE`，该宏按短串内联字节布局取值。M4 局部长串/短串共桶和栈地址重用的影响仍记录为 TODO，具体核查入口是 `conversion.h:73-76` 与 `string.c:958-974`；本批不把这一处升级为悬空引用 BUG。

## 测试和执行证据边界

本批作者只读审查测试和入口，没有运行 `projects`、language-server suite 或专用数值/分配故障注入；Root 后续执行的 module/provider 结果记录在本文末尾。此处是本台账各单元共同引用的测试边界；构建、聚合或 fixture 注册不能替代单元级行为断言。

`projects` 的 CTest 注册见 `tests/cmake/lsp-cli-smoke-registration.cmake:267-277`，脚本入口为 `tests/cmake/run_projects_suite.cmake`。`native_math_export_probe` 的注册行 `:503` 要求输出 `9`；其 fixture `tests/fixtures/projects/native_math_export_probe/src/main.zr:5-8` 调用 `Matrix4x4.scale(2,3,4)` 并读取 m11/m22，因此它是 scale 元数据/runtime 路径的现存断言定义，但本批没有执行该项目。

`native_numeric_pipeline` 在 `lin_alg.zr:9-17` 调用 Matrix4x4 translation/scale/mulMatrix/mulVector 并返回 movedXYZ 和 crossLength。可检索的 project cases 在 `run_projects_suite.cmake:506-508`；然而 `probe_callbacks.zr:28` 的 checksum 只消费 `vectorInfo.dot` 及 signal/tensor/state 汇总，不消费 movedXYZ/crossLength，因此 `214` 和阶段标记不能作为这些 Matrix4x4 数值结果的断言。本批没有运行该 runner；脚本会清理并重写项目 fixture 输出和生成目录，不能为审查创建副作用。

language-server 的 `test_semantic_analyzer.c:751-775` 只构造并分析 translation 表达式，检查静态诊断，不调用 native callback 或核验矩阵分量。producer本批只静态检查 `test_official_provider_convergence` 与 `test_module_system`；Root随后实际运行的聚合结果见末尾，但这些 suite 仅提供注册/聚合证据，不是 Matrix4x4 数值断言。全仓可检索范围未发现 Matrix4x4 专项 determinant/inverse/乘法数值断言或 nodeMap/HASH_BUCKET 分配失败注入。

## 本批结果

新增 Doxygen 覆盖 callback 借用边界、row-major/column-vector 布局、参数 Raise 与字段普通 false 的区别、结果所有权、epsilon 失败状态、descriptor 静态存储有效期、局部配置表和非显然失败链。行为问题仅注释和登记，当前不修复。Root 的实际验证证据附于下节；本段仍区分调用链/测试定义与已运行行为证据。
## Root 验证结果

四个 Matrix4x4 源文件在最终冻结版通过严格注释等价校验：原非注释 token 的 kind、spelling、StartOfLine、LeadingSpace 均一致，proof 报告中的位置敏感宏名单为空。收据 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/matrix4x4-fs-public-final-r2-comment-equivalence.json`，SHA-256 `b9ae16dd6ffa1ead79e149dabbba163ff7aef0efd536bfb01ae086e245282a5a`。

同树 Windows Debug 构建使用 Clang 19 clang-cl、lld-link 与 Windows SDK，退出码 0；module system 78 项为 70 PASS、8 FAIL，逐例与本轮前的基线相同；official provider convergence 9/9 PASS。矩阵实现及注册源文件实际重编译，2975 项源码 pin 保持稳定。结果收据 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/matrix4x4-fs-public-final-r1-results.json`，SHA-256 `cf35cb63090a45a4444ef292a3478d7b353492dadb832c51d4d9ed2d129b6774`；逐例比较收据 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/matrix4x4-fs-public-final-r1-comparison.json`，SHA-256 `ad4405c559d7ea99adb08c7945e337cd322bd69c9441560cbdcfc05d0e0172a0`。这些 module/provider 测试提供构建及聚合证据，不包含 Matrix4x4 专项数值断言或分配故障注入。

冻结源码 SHA-256：

- `zr_vm_lib_math/include/zr_vm_lib_math/matrix4x4.h` — `782B73299AB3A09FB5620E40903CC094F25DB6334B554ACA5E0E44C744E198DC`
- `zr_vm_lib_math/include/zr_vm_lib_math/matrix4x4_registry.h` — `822B255893C6CC9EE039F40BA0DE6BA271F4C4A2B4812F45CCFB2B046FBA0BA5`
- `zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c` — `103D642465B2869AA8C2E750DB0EBFB26FDB5C34B51F5557402D2AB179A18097`
- `zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4_registry.c` — `66C26865352F1E4FB17426649F3345D3A6B0A75604B7DA4B2AA4114378635E19`