---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/vector2.h
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/include/zr_vm_lib_math/vector3.h
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/include/zr_vm_lib_math/vector4.h
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
  - tests/system/test_system_fs_module.c
tests: []
doc_type: workflow-detail
---

# Vector2/3/4 点积依赖契约同步

## 范围与职责

Math common 提交 c384d0f2 已将结构体地址输入 Dot 的 C11 边界按 count/backend 细分，并反向更新三种 Vector 的全部五组公开声明/定义和两个独立布局块，共32条既有单元。本批把六个 C/H 源码中的旧布局 TODO 和过时 common.h 锚点同步到这些已核实的契约，不增加唯一审查单元，也不修改公开 API、程序语句或数值策略。

六个源码文件由 Root 独占修改；原有 Vector registry、脚本样例和其他模块只读核对。当前 main、工作树差异和活跃会话按本任务总规范维护；core/parser/AOT/LSP 的并发文件未在本批写入。

## 冻结源码

以下 SHA256 对应暂存的注释版本；所有原 C/H 程序 token 行保持不变：

- zr_vm_lib_math/include/zr_vm_lib_math/vector2.h：9e6c4fdde454a71f0e22c90a0e11df14f0fd1365d0965a286600fda7007fc4ef。
- zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c：15e37bcb679b6d4613fc46cd47c6f65d9d04201371d3382a665e066c9c312ab1。
- zr_vm_lib_math/include/zr_vm_lib_math/vector3.h：608fdf9b889ba1cc285288ebc872861d607b160024c84c4d401d65c28e28af91。
- zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c：e2de1a5b7fb6b4a0d7c0a371e7e1999a13ef7315fe5cabce05390ebb9f9331e9。
- zr_vm_lib_math/include/zr_vm_lib_math/vector4.h：931dd01b9a4f98fc1f03756d551ecdb9bd5b8c182255604e58e19128963105cf。
- zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c：a3685b4c00e3d6c29d1c77d2d7725971658a90bae88a1ad1a4e6259240948a21。

## 调用、约束与生命周期

各 Vector registry 将 Length、LengthSquared、Normalized、Dot、MetaCompare 绑定到相应 callback，native binding 在当前 state 的派发期借用 receiver、VM 参数和结果槽。callback 读取数值字段形成局部 struct 快照，再把快照地址和固定 count 交给 ZrMath_Dot。快照不转移 VM 对象所有权；字段缺失的普通 false、ReadObject 的 VM 类型异常、factory 空返回和字段 setter 软失败仍按既有契约分别记录。

LengthSquared 直接发布平方和；Length 还开根，Normalized 根据该范数创建新对象，Dot 计算两个快照的点积，MetaCompare 比较两侧平方和。布局缺陷会沿这五条调用路径传播；既有极值、NaN/Inf、epsilon、factory 和排序策略结论保持原有适用范围。Distance 先构造显式 double 数组，不归入结构体地址转换缺陷。

BUG 的静态证明见 math-common-contracts 批次的 C11 N1570 §6.7.2.1p15、§6.5.6p7–8 及 common.h:134/common.c:48：结构体起址可转为首成员指针，但多个 double 成员不因此成为 double 数组。具体成立条件为：

- Vector2 的 count=2：AVX2 尾循环及纯 scalar 访问下标1，越出首成员标量对象；SSE2/NEON 的单次 intrinsic 对象范围仍为 TODO。
- Vector3 的 count=3：纯 scalar 及 AVX2/SSE2/NEON 的尾循环访问下标2，存在同一对象边界 BUG。
- Vector4 的 count=4：SSE2/NEON 第二轮形成首成员指针+2，纯 scalar 也越界；AVX2 单次 intrinsic 的对象范围仍为 TODO。

本批仅把这些有完整调用路径的条件写入源码，不宣称所有后端均已执行或所有 intrinsic 分支均已判定。尚未闭合的 intrinsic 条件与 MetaCompare 排序消费策略保留具体核查入口。

## 台账与验证

三张 canonical Vector 台账覆盖34个既有单元：32条点积依赖记录补 evidence/reason；另修正两条比较策略块的遗留 Wiki 空行引用。六处过时 Wiki 锚点改到当前 Vector2/3/4 比较策略正文319/334/355；Vector4 块将误列为 caller 的 Wiki 引用移回 evidence，并列 registry/lane 实际入口。保留其原 BUG/TODO 结论，不新增唯一单元。inventory 只更新六个拥有文件的 SHA256、reviewed 状态和交付说明，其他行及未选择列按原始字节保护。没有新增审查数，累计交付仍为1278。

strict proof math-vector-dependency-final-r2-comment-equivalence.json（SHA256 acbd02d00584ec4d329604b932cf0ff0822274d93759c8824f14c828f45c1306）比较 main c384d0f2 与当前六源的4791个非注释 token、lexer flags 和原程序 token 行：完全相等；12个 lexer 子进程全部等待退出。

实际 checkout 复用 clang-cl-products-debug 构建；scalar-stream-vector-final-native-r2 构建退出0（10.375秒），module 70/78、provider 9/9、Fs 7/8，逐条 case 输出与历史稳定基线一致。全部3216个编译输入及24个冻结源码在本轮前后稳定。module 的8个既有失败和 Fs C1035 仍保留，不归因于注释，也不表示本批修复。

此前 native-r1 构建/用例结果相同，但 FileStream 的两处 EOF GC 引用在运行期间更正，输入 guard 失败；原失败记录保留，未计作稳定验证。native-r2 在所有源码负责人停止编辑后重跑通过稳定性 guard。对更早修改前的全量构建，外部 Parser 宏缺失曾阻塞，期间外部 Parser/测试输入有变化，因此不作隔离的全仓行为等价归因。

本轮只取得现有构建和用例的执行证据；没有 Vector 专项后端、极值、UB、OOM、GC 或并发注入信用，也没有 WSL/Linux 执行信用。相关 module/provider 用例不能替代逐 Vector 数值回归。

实际报告：E:\cargo-targets\zr_vm\reports\first-party-review-01a13463\scalar-stream-vector-final-native-r2-results.json（SHA256 4f3dc77668e03b98b3e5d44ef286459801db0133a6e8bd89fb608edca1dc2efb）；comparison SHA256 fd41f0d6dd2572ef25f3e6835ad24ea07327ff66d370a413f7b1d5c2d40de6af。

## 产物边界

未创建源码副本或新测试。构建、共享缓存和 alternate index 复用；本轮自有构建及测试进程均已退出，释放0字节。Fs runtime 累计28个生成文件91字节保留：此前自动清理审批拒绝了精确清理操作，原因为 blocked by policy；本批未重试或绕过，产物清理仍未关闭。

补充锚点核对：上述六处 Wiki 引用在 c013fe8c 版本也已为空，不能归因于本轮 common 提交；本批逐处读当前正文后更正。Vector2 头文件的旧 Wiki 范围296-308同步为307-319，原程序行不变。最终源码与 native-r2 的注释前版本以 math-vector-dependency-final-source-native-token-bridge.json（SHA256 c94ac4b22b27d28316dd93187f9d42ec3c71f78a2c654797b8c5e259da2f49cd）桥接：六源程序token/flags及原行一致，不因一处文档范围修正重复运行相同用例。
