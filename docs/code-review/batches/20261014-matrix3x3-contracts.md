---
related_code:
  - CMakeLists.txt
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/include/zr_vm_core/memory.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_library/src/zr_vm_library/common_state.c
  - tests/harness/runtime_support.c
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
  - docs/wiki/03-modules/math-api.md
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3.h
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 Matrix3x3 全链审查与文档注释任务
  - root: Matrix3x3 六路径写入租约、冻结与验证记录
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
  - tests/harness/runtime_support.c
doc_type: workflow-detail
---

# Matrix3x3 契约审查批次（2026-10-14）

本批覆盖 Matrix3x3 的公开 callback 声明、registry getter、descriptor 配置、实现函数和独立语义块。本批源码改动限于中文 Doxygen 与局部契约注释，未有意修改 API、实现或测试；最终非注释 token 等价证明见下文 Root 的终态验证记录。逐单元结论以 [zr_vm_lib_math_matrix3x3.tsv](../coverage/zr_vm_lib_math_matrix3x3.tsv) 为准。

四个源码当前冻结 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3.h | C2F6B50D7DDD2C4B29BC88A698ADED2F4B81BE7C3AACD1A7B2AF8105E728B4D1 |
| zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3_registry.h | A583E25ADE57023E971423AD09885B47A2064CEC90D4BB82043EFFB820BFBFC3 |
| zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3.c | 90CD8718B846A79C78B1DCF672431DD75F463F792597DD06999E87C7F470C5AB |
| zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3_registry.c | B3C9B3A6CF54507A2BAC45A475FA3F037FAE3355E2DF0B661AB4636A93C5F875 |

## 单元枚举

台账共登记 50 个唯一的 file/unit/kind 单元，覆盖四个源码文件的全部声明、定义、配置项及独立契约语句块。

| 文件 | 单元范围 | 数量 |
| --- | --- | ---: |
| matrix3x3.h | 文件说明、include guard、9 个 callback 声明 | 11 |
| matrix3x3_registry.h | 文件说明、include guard、3 个 getter 声明 | 5 |
| matrix3x3.c | 文件说明、4 个私有数值 helper、构造字段表、9 个 callback、10 个独立语义块 | 25 |
| matrix3x3_registry.c | 文件说明、3 个 getter、字段/method/meta/type/hint 配置 | 9 |
| **合计** |  | **50** |

决策分布为 BUG 24、commented 23、TODO 1、no-comment 2。no-comment 仅用于已有 guard 命名或同等信息已经由就近声明完整表达的单元；声明和实现分别登记，普通局部控制流随所属函数审阅，不以花括号数量增加台账行。

## 从模块注册到 callback 派发

zr_vm_lib_math/CMakeLists.txt:4 将 math 声明为构建模块；CommonMacros.cmake:6-11 展开模块源文件。module.c:118、:130、:143 分别汇集函数 registry、Matrix3x3 type descriptor 和 hint。matrix3x3_registry.c 的静态 kMethods/kMeta 只登记 arity、返回描述与 callback 地址；native_binding_dispatch.c:34-55、:364 在调用前检查 arity，native_binding_dispatch_lanes.h:277 才实际调用 callback。descriptor 的返回类型文本不是对 callback 普通 false、VM 异常或失败后结果的替代说明。

Parser 的 CREATE_OBJECT/TO_STRUCT 路径位于 compile_expression_types.c:994-1002；隐藏构造调用在同文件 :858-955 建立。九个构造实参超过 inline fast lane 的八个参数容量，合法路径落入 generic dispatcher。Matrix3x3 Construct 当前处理零参数初始化或九个数值参数，而 registry 声明范围为 0..9；1..8 个实参会进入构造器 arity 错误分支，因此该范围不是任意参数数目都可成功构造的承诺。兼容的 seed receiver 可被共用构造 helper 复用；本批对缺字段风险只按已证明的窄分配失败条件登记。

## 对象字段发布与分配失败

Matrix3x3 的 row-major 字段名和值索引必须与 registry descriptor 顺序一致。nodeMap 初始容量为 4，负载阈值为 3；写入第 4 个字段以及容量 8 后写入第 7 个字段需要 HASH_BUCKET 扩容。对象 setter 的字段写入接口为 void，扩容失败只记录错误，不把失败状态返回给调用者。

本批分别记录三条条件性缺字段路径，触发前提不能互相混用：

- Construct 的 seed 对象路径：在九个字段写入中，某次 HASH_BUCKET realloc 软失败，且先前的字段写入、receiver/key pin 以及其余分配成功时，setter 继续返回；构造 callback 可完成并交付少字段对象。该窄链由 parser hidden constructor、generic dispatcher、兼容 seed 复用及 setter 调用共同支持。
- 新对象工厂路径：Identity、Transpose、Inverse、MulMatrix 通过 ZrMath_MakeMatrix3x3 创建新对象并逐字段发布。若第 4 或第 7 字段扩桶单次失败，其他步骤均成功，setter 不告知工厂失败，callback 仍可能交付不完整矩阵。
- ignored-root ARRAY 路径：setter pin receiver 和字段 key 时，若 ARRAY 容量恰好耗尽且该次 RawMalloc 软失败，字段写入会提前结束而工厂仍可继续。此路径与 nodeMap 的 HASH_BUCKET/Memory_Allocate 扩容是不同分配器和不同故障点。

前两类 HASH_BUCKET 缺陷仅以仓内默认 builtin allocator 和测试 harness allocator 的 realloc 行为为依据：CommonState 的 allocator 与 runtime_support 中的 harness allocator 将有效旧块交给 realloc；此次扩容失败保留旧桶块，setter记录失败但调用端仍继续。没有据此推断所有自定义 allocator 都保留旧块；memory.h 的自定义 allocator 失败语义仍是 TODO 核查入口。ARRAY 路径使用 ignored-root 表的 RawMalloc，不能写成 HASH_BUCKET 扩容或 VM 抛错。

MetaToString 的短结果还涉及独立的软失败路径：短串缓存未命中且 HASH_BUCKET 分配失败时，字符串 setter 不发布字符串对象；native result 槽预置为 null，格式化 helper 仍报告成功，dispatcher 因而可把 null 作为成功结果交付。长格式结果由 VM 管理，不属于短串驻留缓存；本批 BUG 只限定短串缓存扩桶失败。

另有一个未升级为 BUG 的长输出 TODO：长格式结果仍进入字符串地址缓存路径 `common.c:512-519` → `native_binding_dispatch.c:1303-1308` → `native_binding_support.c:320-324` → `string.c:958-974`，其中转换宏定义在 `conversion.h:73-76`。公开声明与 math-api 文档记录该核查入口；本批只闭合短串 HASH_BUCKET 分配软失败，不把长串缓存路径推断为同一缺陷。

## 数值计算与返回状态

矩阵按 row-major 快照计算；MulVector 将其乘以三分量列向量。Determinant、Inverse、MulVector 和 MulMatrix 使用直接 double 乘加/cofactor 运算，没有中间溢出保护。BUG 记录来自完整公式和 callback 链的静态可达证明，本批没有运行数值故障注入，也没有声称仓内已有对应数值断言。

有限反例使用 A=1e200。对矩阵 [[A,A,0],[A,A,0],[0,0,1]]，精确实数行列式为 0；直接公式中的两个外层乘积各自溢出为正无穷，随后执行 Inf-Inf 得到 NaN。Inverse 对同一矩阵得到 NaN determinant；fabs(det) <= 1e-9 的检查不能拦截 NaN，后续公式仍会写出非有限结果。另有 [A,A,0] 乘 [A,-A,0] 的 MulVector 首分量，以及相同首行乘以首列的 MulMatrix 首元素，精确结果均为 0，而直接 double 运算会计算 Inf-Inf 并得到 NaN。

Inverse 在可读输入的近零 determinant 分支返回普通 false；dispatcher 在 VM 状态仍为 FINE 时将该结果归一化为 null，但 descriptor 把返回描述登记为 Matrix3x3。字段读取失败则可能先设置 VM 异常，不可与普通数值 false 混为一谈。台账将“near-zero 输入时 null 是否符合 Matrix3x3 返回契约”保留为 TODO，并将 registry 返回描述及 dispatcher 处理作为下一步核查入口。

MetaMul 仅当右操作数原型名精确为 Vector3 时进入 MulVector；其他 OBJECT/ARRAY 按九字段矩阵形状尝试读取并进入 MulMatrix。注释和台账将这作为当前分派规则记录，没有把未实现一般协议推断为缺陷。

## 测试与验证边界

全仓可检索测试范围未发现 Matrix3x3 专项数值 callback、HASH_BUCKET 故障注入或 ignored-root 分配故障注入。test_official_provider_convergence.c 的 provider 结果只提供注册/元数据收敛证据；test_module_system.c 的模块运行结果是聚合 suite 证据，不等于 Matrix3x3 数值算法覆盖。runtime_support.c 中的 realloc allocator 只用于闭合其保留旧块的静态失败前提，不表示该测试运行过故障注入。

Root 的历史原生验证记录为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/exception-matrix3x3-final-r2-results.json，比较记录为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/exception-matrix3x3-final-r2-comparison.json。该次 build exit 0、耗时 19.328 秒；module suite 78 项为 70 PASS/8 FAIL，78 项结果与 r1 相同；provider suite 9/9 PASS，2951 个源码 pins 与当时冻结的 8 个文件稳定。该次验证早于本批最后一次 matrix3x3.h 长短字符串所有权措辞修订，因此仅作为历史基线，不能声称覆盖当前四源码最终 SHA。随后工作树中 `tests/parser/test_typed_call_binding.c`、`zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c` 和 `zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c` 也发生变化；Root 最终运行已记录当前源码 pins 并逐例比较，相关输入漂移仍意味着这不是完全隔离的 M3-only 运行。当前最终版本的 token 等价证明和原生验证结果见下文终态记录。

本批执行的文档机械检查为：

- 命令：E:/cargo-targets/zr_vm/cache/python-3.12.10-embed-amd64/python.exe -B scripts/code_review_inventory.py check-batch docs/code-review/coverage/zr_vm_lib_math_matrix3x3.tsv
- 范围：Matrix3x3 九列台账的全部 50 个单元及其 definition/evidence 锚点
- 实际结果：命令终态 exit 0，输出 review batch: docs/code-review/coverage/zr_vm_lib_math_matrix3x3.tsv valid。check-batch 只验证格式、重复单元和锚点存在，不代替 caller 方向或语义审查；源码终版 token 等价证明及原生验证已由 Root 完成，结果见下文终态记录。

Root 最终验证记录如下，边界与结果分别见 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/matrix3x3-final-r1-comment-equivalence.json`、`matrix3x3-final-r1-results.json` 和 `matrix3x3-final-r1-comparison.json`。相对 HEAD 614c 的四源码注释等价 proof 通过，token kind/spelling/line flags 统计为 119/39/1566/443，无 location macro；proof SHA-256 为 `18819ec21783eba8f3621527820f1975808a075304b7ecdd94f08e01d601a617`。同树 native build exit 0，耗时 28.641 秒；10 个 Ninja 步骤中 9 个编译/链接行包含 Matrix3x3、Math module、Parser lambda/statement 输入，保留 8 条 warning 行。module suite 78 项为 70 PASS/8 FAIL（exit 8，12.078 秒），provider suite 9/9 PASS（exit 0，1.422 秒）；每个测试结果逐项与最近 Exception R3 及原 r3e 比较一致。2951 个当前源码 pins 与四个冻结文件稳定，changed set 为空；comparison SHA-256 为 `48a1e59d581a08d7a5e377294d091e7722c5837584d25edd2fe68bf502e67487`，results SHA-256 为 `f4f28869aec1c0bd585bf16b126bc5d2fc474a388334711df32dad55f736748e`。这次复用构建包含先前 parser 的两个 foreign drift 与一处 typed-call 测试漂移，结果比较用于界定这些输入下的同一性，不宣称完全隔离。该 suite 和 provider 仍不提供 Matrix3x3 数值断言或分配失败注入信用；台账每单元 evidence 指向本节第 102 行的测试边界。

## 最终台账调用方向复核

独立复核后，caller 列只保留真实注册入口、直接 callback 调用点或实际消费位置；结果归一化、字段写入和字符串缓存等下游路径放在 evidence。两个字段发布块的 callers 是 `matrix3x3.c:179/206/238/289` 四个共享工厂调用点；`common.c:170` 的字段写入、`:287` 的通用工厂转发以及 `native_binding_dispatch.c:1369` 的 pin/setter 下游均作为 evidence。四个 callback 结果块的 `native_binding_dispatch.c:619` 仅作为 false/status 归一化 evidence，入口改为对应 descriptor 和真实 callback lane；MulVector/MulMatrix 还列出 `MetaMul` 的直接转调点。private determinant helper 不再把自身定义行列为 caller。GetHints getter 的直接 caller 是 module aggregator；`GetHints.kHints` 的使用锚点是 getter 返回表地址的语句，LSP 投影为下游 evidence。MetaToString 增列 `value.c:335` 的真实 to-string 元调用入口，其共享 factory/string/cache 下游留在 evidence。Determinant 契约的有限例准确描述为两个外层乘积分别溢出为 Inf 后执行 Inf-Inf；该更正不改变源码或本批测试结论。
