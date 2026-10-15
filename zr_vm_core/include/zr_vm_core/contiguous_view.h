#ifndef ZR_VM_CORE_CONTIGUOUS_VIEW_H
#define ZR_VM_CORE_CONTIGUOUS_VIEW_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/conf.h"

/** 视图描述符的校验结果类别，供数组下标降低和调用方决定回退/报错。 */
typedef enum EZrViewDiagnosticCode {
    ZR_VIEW_DIAGNOSTIC_NONE = 0,
    ZR_VIEW_DIAGNOSTIC_INVALID,
    ZR_VIEW_DIAGNOSTIC_BOUNDS,
    ZR_VIEW_DIAGNOSTIC_OVERFLOW,
    ZR_VIEW_DIAGNOSTIC_GENERATION,
    ZR_VIEW_DIAGNOSTIC_LIFETIME,
    /** TODO: 当前校验器没有产生此码；接入真实 storage adapter 时确认非连续布局的判定与回退。 */
    ZR_VIEW_DIAGNOSTIC_NOT_CONTIGUOUS
} EZrViewDiagnosticCode;

/** 失败时的值型诊断；expected/actual 仅按对应 code 解释，不拥有外部资源。 */
typedef struct SZrViewDiagnostic {
    EZrViewDiagnosticCode code;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrViewDiagnostic;

/** 声明调用方不得经此视图写入；偏移计算器不执行写权限检查。 */
#define ZR_VIEW_FLAG_READ_ONLY ((TZrUInt32)1u << 0u)
/** 声明借用期内由外部维持 pin；校验器只要求 lifetimeRegion 非零。 */
#define ZR_VIEW_FLAG_PINNED ((TZrUInt32)1u << 1u)
/** 标记 owner 的内联存储形态；本描述符不负责扫描内联引用或调整布局。 */
#define ZR_VIEW_FLAG_INLINE_STORAGE ((TZrUInt32)1u << 2u)

/** 创建时的值型元数据；ownerRoot 仅是借用的所有者标识，调用方负责对象保活。
 * byteOffset/stride/elementSize 以字节为单位，length 以元素计；generation 和 region 由存储提供方发布。 */
typedef struct SZrContiguousViewRequest {
    TZrPtr ownerRoot;
    TZrSize byteOffset;
    TZrSize length;
    TZrSize stride;
    TZrSize elementSize;
    TZrUInt64 elementLayoutHash;
    TZrUInt64 storageGeneration;
    TZrUInt64 lifetimeRegion;
    TZrUInt32 flags;
} SZrContiguousViewRequest;

/** 可复制的视图描述符，不保存元素地址；切片保留 owner 与代数元数据。
 * 调用方仍须在实际取址/访问前验证真实存储范围、当前 generation 和借用生命周期。 */
typedef struct SZrContiguousView {
    TZrPtr ownerRoot;
    TZrSize byteOffset;
    TZrSize length;
    TZrSize stride;
    TZrSize elementSize;
    TZrUInt64 elementLayoutHash;
    TZrUInt64 storageGeneration;
    TZrUInt64 lifetimeRegion;
    TZrUInt32 flags;
} SZrContiguousView;

/** @brief 清空复用的描述符，使后续校验把它视作无效视图；允许 view 为空。 */
ZR_CORE_API void ZrCore_View_Init(SZrContiguousView *view);
/** @brief 从借用元数据创建可校验的描述符，供切片和 Exec IR 下标偏移使用。
 * @pre request/view/diagnostic 使用独立存储（diagnostic 可为空）；满足此边界时失败保留 view。实际访问前调用方须保证 owner 保活、所需 pin 与真实 backing storage 边界；本函数不接管它。
 * @return 失败时 view 保持原值；diagnostic 可为空。 */
ZR_CORE_API TZrBool ZrCore_View_Create(const SZrContiguousViewRequest *request,
                                        SZrContiguousView *view,
                                        SZrViewDiagnostic *diagnostic);
/** @brief 检查描述符的算术、标志及可选的存储代数一致性。
 * @note currentGeneration 为零会跳过代数比较；这里只检查 PINNED 的 region 标识，不建立 pin。 */
ZR_CORE_API TZrBool ZrCore_View_Validate(const SZrContiguousView *view,
                                          TZrUInt64 currentGeneration,
                                          SZrViewDiagnostic *diagnostic);
/** @brief 建立共享 owner 和生命周期的子视图；空的尾部切片也可请求。
 * @return 遵守下方别名边界时失败不发布 slice；调用方继续负责 owner 与 generation 的有效性。 */
ZR_CORE_API TZrBool ZrCore_View_Slice(const SZrContiguousView *view,
                                       TZrSize start, TZrSize length,
                                       SZrContiguousView *slice,
                                       SZrViewDiagnostic *diagnostic);
/** @brief 将有符号元素下标转换为相对 owner 的字节偏移，供 Exec IR 适配器使用。
 * @pre 将所得偏移用于真实访问前，调用方必须核对当前 generation、owner 生命周期及 backing 范围。
 * @return 失败时不应使用 offset；本函数不解引用 ownerRoot。 */
ZR_CORE_API TZrBool ZrCore_View_IndexOffset(const SZrContiguousView *view,
                                             TZrInt64 index,
                                             TZrSize *offset,
                                             SZrViewDiagnostic *diagnostic);

#endif

/**
 * @enum EZrViewDiagnosticCode
 * ## Summary
 * 描述 `ZrCore_View_Validate`、`ZrCore_View_Create`、`ZrCore_View_Slice` 与
 * `ZrCore_View_IndexOffset` 的元数据校验结果，供调用方在继续使用偏移、回退或报告
 * 输入错误之间作选择。它只分类当前 descriptor 检查，不能证明真实容器存储安全。
 *
 * ## Value model
 * `ZR_VIEW_DIAGNOSTIC_NONE` 表示成功；`INVALID` 表示空指针、非法 stride/elementSize
 * 或未知 flags；`BOUNDS` 表示切片窗口或元素索引越界；`OVERFLOW` 表示 descriptor
 * 偏移运算无法由 `TZrSize` 表示；`GENERATION` 表示传入的非零当前代数与记录值不匹配；
 * `LIFETIME` 表示设置 `ZR_VIEW_FLAG_PINNED` 时 region id 为零。`NOT_CONTIGUOUS` 当前
 * 没有产生路径，不能据此推断非连续存储已经被检测。
 *
 * ## Invariants and lifecycle
 * 诊断码是由值型输出提供的分类，不分配或拥有资源。`expected` 与 `actual` 的意义
 * 取决于具体码；调用方应先检查 code，再解释这两个数值。
 *
 * ## References
 * `SZrViewDiagnostic` 保存此码；`ZrCore_View_Validate` 是各分类的实现入口；
 * SSA 测试只覆盖部分 `OVERFLOW`、`BOUNDS` 和 `INVALID` 路径，例见
 * `tests/core/test_ssa_arrays_slices.c:42`、`:70`、`:83`、`:128`、`:137` 与 `:142`；
 * 当前没有测试证明 `GENERATION`、`LIFETIME`、未知 flags 或 `NOT_CONTIGUOUS` 的诊断行为。
 * 非连续布局的后续确认入口见 `docs/plans/ssa/05-data-layout/02-arrays-slices.md:63`。
 */

/**
 * @struct SZrViewDiagnostic
 * ## Summary
 * 在 Core view API 返回失败时接收一份无所有权的分类和数值上下文；调用方可传空指针
 * 丢弃诊断，不影响 API 的布尔结果。
 *
 * ## Fields
 * - `code`：失败类别；成功的 Validate 会将其重置为 `ZR_VIEW_DIAGNOSTIC_NONE`。
 * - `expected`：当前类别所需的参考值，例如边界检查中的 view length 或代数检查中的
 *   current generation。
 * - `actual`：当前类别对应的观察值；具体来源依 code 而定，负索引在当前实现中按 0
 *   写入，不能把它当成原始负数。
 *
 * ## Invariants and lifecycle
 * 这是可复制的普通值，不持有 view、owner 或其他外部资源。只有在 `code` 确认具体类别
 * 后才解释 expected/actual；API 不保证未写入的诊断缓冲区内容。
 *
 * ## References
 * `view_diag` 填充该结构；`ZrCore_View_Validate` 清空后按失败分支写入；
 * `tests/core/test_ssa_arrays_slices.c:42` 与 `:70` 断言失败码。
 */

/**
 * @def ZR_VIEW_FLAG_READ_ONLY
 * ## Summary
 * 在请求和 view 中声明借用方不得经该视图写入元素，供上层 storage adapter 或访问方
 * 解释权限意图。
 *
 * ## Value model
 * 该位是 `flags` 中的一个已知 bit；Validate 接受它，但 Core 的索引偏移 API 不执行
 * 写入，也不据此授权或拒绝读写。
 *
 * ## Invariants and lifecycle
 * 设置此位不会把 owner 转为只读，也不会创建隔离副本。实际写权限仍由访问方和 backing
 * storage 的管理者落实；不能仅凭此标志作安全访问判断。
 *
 * ## References
 * `SZrContiguousViewRequest.flags` 与 `SZrContiguousView.flags` 保存该 bit；
 * `ZrCore_View_Validate` 在 `zr_vm_core/src/zr_vm_core/object/contiguous_view.c:37`
 * 仅检查它属于已知掩码。
 */

/**
 * @def ZR_VIEW_FLAG_PINNED
 * ## Summary
 * 声明调用方在 view 的借用期间已由外部维持所需 pin，以表达 backing 不应在访问窗口内
 * 被移动的意图。
 *
 * ## Value model
 * 该位是 descriptor 的已知标志。Validate 只要求 `lifetimeRegion` 非零；它不调用 pin
 * API，也不读取 GC root 或容器状态。
 *
 * ## Invariants and lifecycle
 * 设置此位的调用方仍须负责取得、维持并释放实际 pin，并在借用结束后使 descriptor 失效
 * 或不再使用。非零 region id 只是当前检查条件，不等于 pin 已经生效。
 *
 * ## References
 * `SZrContiguousViewRequest.lifetimeRegion` 提供校验使用的标识；
 * `ZrCore_View_Validate` 在 `zr_vm_core/src/zr_vm_core/object/contiguous_view.c:51`
 * 检查该条件。真实 adapter/pin 入口仍待计划中的 storage adapter 收敛，见
 * `docs/plans/ssa/05-data-layout/02-arrays-slices.md:63`。
 */

/**
 * @def ZR_VIEW_FLAG_INLINE_STORAGE
 * ## Summary
 * 标记 owner 的元素 backing 采用 inline storage 形态，供 descriptor 消费者区分布局来源。
 *
 * ## Value model
 * 当前 Core 仅将该 bit 纳入允许的 flags 掩码；它不扫描 inline 引用、不解码布局，也不
 * 改变偏移计算。
 *
 * ## Invariants and lifecycle
 * 标记必须与实际 owner/layout 来源一致，但当前 Validate 不会比对二者。调用方不得把该
 * bit 当作 GC tracing、alignment、extent 或访问权限证明。
 *
 * ## References
 * `SZrContiguousViewRequest.flags` 是该标志的输入位置；
 * `ZrCore_View_Validate` 在 `zr_vm_core/src/zr_vm_core/object/contiguous_view.c:37`
 * 只检查其属于已知掩码。inline storage adapter 的计划入口见
 * `docs/plans/ssa/05-data-layout/02-arrays-slices.md:63`。
 */

/**
 * @struct SZrContiguousViewRequest
 * ## Summary
 * 调用 `ZrCore_View_Create` 时提供一组 owner 相对布局和借用标记。Create 会把这些值复制
 * 到本地候选 descriptor 并验证元数据；它不接管 owner，也不从 owner 读取 backing。
 *
 * ## Fields
 * - `ownerRoot`：借用的 owner 标识。非空长度必须非空；空视图可为空。Core 不解引用或
 *   保活此指针，真实访问前由调用方保持 owner 有效。
 * - `byteOffset`：从 owner 起点开始的字节偏移；非空视图会参与末元素末端的溢出计算，
 *   但不与真实 backing 容量比较。
 * - `length`：元素数量。零长度允许空 owner 且跳过末元素范围算术；仍要求非零 stride 和
 *   elementSize。
 * - `stride`：相邻逻辑元素的字节步长，必须非零。当前实现允许它小于 elementSize，因而
 *   也允许相邻元素区间重叠。
 * - `elementSize`：单个元素的字节大小，必须非零；Core 不核对元素布局或实际分配大小。
 * - `elementLayoutHash`：随 descriptor 复制的布局标识；当前校验器不验证该值。
 * - `storageGeneration`：存储提供方发布的代数快照。只有 Validate 收到非零
 *   `currentGeneration` 时才与当前值比较。
 * - `lifetimeRegion`：存储提供方发布的借用区域标识；设置 PINNED 时必须非零，但该条件
 *   不执行或证明实际 pin。
 * - `flags`：由三个 `ZR_VIEW_FLAG_*` 位组成；未知位会导致 INVALID。已知位只记录意图，
 *   不代替访问控制、GC tracing 或布局适配。
 *
 * ## Invariants and lifecycle
 * 所有字段都是值或借用指针，不因 Create 成功而延长 owner 生命周期。`length` 以元素计，
 * `byteOffset`、`stride` 和 `elementSize` 以字节计；descriptor 的范围检查只证明整数算术
 * 可表示，不证明地址对应真实连续存储。
 *
 * ## References
 * `ZrCore_View_Create` 消费此请求；`ZrCore_View_Validate` 定义字段检查边界；
 * `tests/core/test_ssa_arrays_slices.c:20` 提供当前测试请求；
 * `docs/plans/ssa/05-data-layout/02-arrays-slices.md:51` 说明 Core view 与 storage adapter
 * 的计划分工。
 */

/**
 * @struct SZrContiguousView
 * ## Summary
 * 保存一个相对 owner 的可复制逻辑窗口，供切片和 Exec IR 下标偏移检查复用。结构只保存
 * owner 标识与整数元数据，不缓存元素地址或建立 owner 引用。
 *
 * ## Fields
 * - `ownerRoot`：借用的 owner 标识；非空视图要求非空，但 Core 不解引用或保活它。
 * - `byteOffset`：owner 起点的字节偏移；Slice 只在成功发布候选时更新该字段。
 * - `length`：当前窗口中的元素数量；零长度窗口仍保留非零 stride/elementSize 元数据。
 * - `stride`：元素步长（字节），必须非零；当前校验不要求 stride 大于等于 elementSize。
 * - `elementSize`：元素大小（字节），必须非零；真实 backing extent 不在本结构中。
 * - `elementLayoutHash`：消费者可用的布局标识，当前 Core 不对照 owner 类型。
 * - `storageGeneration`：存储提供方给出的快照；比较是否启用由 Validate 的参数决定。
 * - `lifetimeRegion`：借用区域标识；仅在 PINNED 时要求非零，不代表 Core 持有 pin。
 * - `flags`：已知 `ZR_VIEW_FLAG_*` 位；标志描述调用方意图，不自动执行其语义。
 *
 * ## Invariants and lifecycle
 * view 的复制只复制元数据，切片也不改变 owner 所有权。调用方必须在实际取址前确保
 * owner、backing、generation 和借用窗口仍有效；当前 Core 只验证字段约束和整数偏移。
 * descriptor 不声明可安全并发共享或任意重叠输出。
 *
 * ## References
 * `ZrCore_View_Create` 建立 descriptor，`ZrCore_View_Slice` 派生窗口，
 * `ZrCore_View_IndexOffset` 返回相对偏移；Parser facade 位于
 * `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c:6`。
 * `tests/core/test_ssa_arrays_slices.c:107` 覆盖 metadata-only 创建路径；其中 owner 是不被
 * 解引用的标记值，不代表真实 backing storage 或生命周期检查。
 */

/**
 * @fn void ZrCore_View_Init(SZrContiguousView *view)
 * ## Summary
 * 在首次使用或复用 descriptor 前将所有字段清零，使它进入 Validate 会拒绝的未初始化
 * 状态；该接口只重置值，不负责析构任何外部对象。
 *
 * ## Parameters
 * @param[in,out] view 可写 descriptor；允许为空，空指针时为空操作。
 *
 * ## Returns
 * 无返回值；非空参数的所有字段清零，空参数不产生状态变化。
 *
 * ## Constraints and lifecycle
 * 该指针只在本次同步调用中使用，不会被保存。清零不会释放或取消 `ownerRoot`、pin 或
 * lifetime region；调用方若在复用前持有外部资源，仍须按该资源自己的协议释放；此函数
 * 不验证输入 descriptor。
 *
 * ## References
 * 声明位于本头文件 `ZrCore_View_Init`；仓内未发现该接口的直接调用。
 */

/**
 * @fn TZrBool ZrCore_View_Create(const SZrContiguousViewRequest *request, SZrContiguousView *view, SZrViewDiagnostic *diagnostic)
 * ## Summary
 * 从调用方提供的借用元数据创建 descriptor，供后续 Slice 或 Parser Exec IR 偏移计算
 * 使用。先验证本地候选，再一次性发布到输出，避免失败时覆盖旧 view。
 *
 * ## Parameters
 * @param[in] request 创建元数据；不能为空，字段按 `SZrContiguousViewRequest` 的值域
 *                    校验，只借用到本次同步调用结束。
 * @param[out] view 成功时接收候选 descriptor；不能为空，存储须与 request 和 diagnostic
 *                  分离。满足此边界时失败保留其调用前内容。
 * @param[out] diagnostic 可选失败诊断；可为空以丢弃分类。非空时须与 request、view 的
 *                        存储分离，因为验证会同步清零或写入该对象。
 *
 * ## Returns
 * @return 候选通过 Validate 时返回 `ZR_TRUE` 并写入 view；否则返回 `ZR_FALSE`，可选
 * diagnostic 接收失败类别。
 *
 * ## Constraints and lifecycle
 * 创建阶段以 currentGeneration=0 验证，因此不比较当前存储代数。它只检查 descriptor
 * 字段、允许标志和整数末端计算，不检查真实连续布局、backing 容量、layout hash、owner
 * 生命周期、权限或 pin。request/view/diagnostic 参数地址只在本次同步调用期间使用，不会
 * 保存；request 的 ownerRoot 值复制到成功输出的 view，但 Core 不解引用、保活或接管其
 * 指向对象。调用期间 request、view、diagnostic 必须使用互不重叠的独立存储，调用方在
 * 真实访问前仍须满足 owner、backing、代数、权限和 pin 条件。
 *
 * ## References
 * `ZrCore_View_Validate` 执行候选校验；`ZrCore_View_Slice` 与
 * `ZrParser_ExecIr_LowerArrayIndex` 是当前后续使用入口；测试创建点见
 * `tests/core/test_ssa_arrays_slices.c:26` 与 `:107`。
 */

/**
 * @fn TZrBool ZrCore_View_Validate(const SZrContiguousView *view, TZrUInt64 currentGeneration, SZrViewDiagnostic *diagnostic)
 * ## Summary
 * 检查 descriptor 的必需字段、flags、非空窗口末端算术以及可选的代数/借用标记，供
 * Create、Slice 和 IndexOffset 在发布结果或计算偏移前复用。
 *
 * ## Parameters
 * @param[in] view 待检查 descriptor；必须指向可读对象，调用期间借用、不保存；空指针会
 *                 被拒绝并报告 INVALID。
 * @param[in] currentGeneration 当前 storage 代数；零明确表示跳过 generation 比较，非零
 *                               时必须等于 descriptor 的 `storageGeneration`。
 * @param[out] diagnostic 可选值型诊断；非空时进入函数即清零，失败时按 code 写入上下文；
 *                        存储须与 view 分离。
 *
 * ## Returns
 * @return descriptor 通过当前元数据检查时返回 `ZR_TRUE`；否则返回 `ZR_FALSE`，并在
 * diagnostic 非空时写入失败类别。
 *
 * ## Constraints and lifecycle
 * 非零 length 要求 ownerRoot 非空；任何长度都要求 stride 与 elementSize 非零、flags
 * 不含未知位。非空范围只检查 `(length-1)*stride + byteOffset + elementSize` 可表示；
 * stride 可小于 elementSize，empty view 不计算末元素范围。PINNED 只要求 region 非零。
 * Validate 只读取 view 的描述符字段，不解引用或保活 owner，也不核对真实 extent、布局、对齐、权限或 GC/pin 状态；
 * view 与 diagnostic 在调用期间须使用不重叠的存储；这两个参数地址都不会被保存。
 *
 * ## References
 * 直接由 Create、Slice、IndexOffset 调用；调用证据位于
 * `zr_vm_core/src/zr_vm_core/object/contiguous_view.c:68`、`:78`、`:95`、`:105`；
 * SSA 断言见 `tests/core/test_ssa_arrays_slices.c:29` 与 `:119`。
 */

/**
 * @fn TZrBool ZrCore_View_Slice(const SZrContiguousView *view, TZrSize start, TZrSize length, SZrContiguousView *slice, SZrViewDiagnostic *diagnostic)
 * ## Summary
 * 从现有 descriptor 建立一个共享 owner 元数据的子窗口；先检查父 view、窗口和候选末端，
 * 仅在完整通过后写入输出。
 *
 * ## Parameters
 * @param[in] view 父 descriptor；必须指向可读对象，调用期间借用、不保存；空指针会由
 *                 Validate 拒绝。
 * @param[in] start 父窗口中的起始元素索引；允许等于父 length，以创建空尾切片。
 * @param[in] length 子窗口元素数；必须不大于 `view->length - start`。
 * @param[out] slice 成功时接收子 descriptor；不能为空。可与 view 指向同一个完整对象，
 *                   除此完整对象别名外须与 view 使用独立存储。
 * @param[out] diagnostic 可选失败诊断；可为空；非空存储须同时与 view、slice 分离。
 *
 * ## Returns
 * @return 窗口边界和候选偏移通过检查时返回 `ZR_TRUE` 并发布 slice；失败返回 `ZR_FALSE`，
 * 不发布局部候选。调用方不应在失败后使用 slice 作为结果。
 *
 * ## Constraints and lifecycle
 * `start * stride` 与加到 byteOffset 的结果必须可由 TZrSize 表示；非空候选还须通过末端
 * 算术校验。`slice == view` 的完整对象别名由本地候选拷贝支持；除此之外，view、slice、
 * diagnostic 存储须分离，接口不承诺部分重叠或诊断与 descriptor 重叠。输入 view 只借用
 * 到本次同步调用；view、slice 和 diagnostic 参数地址均不保存。创建的窗口复制
 * owner/generation/region/flags，不延长 owner 生命周期；调用方负责实际存储、代数和借用
 * 有效期。空尾 child 不含末元素，故其
 * byteOffset 可以恰为 SIZE_MAX。
 *
 * ## References
 * 实现的候选复制、再校验和提交顺序见 `zr_vm_core/src/zr_vm_core/object/contiguous_view.c:92`、
 * `:95`、`:96`；普通、越界、空尾及溢出测试见
 * `tests/core/test_ssa_arrays_slices.c:27`、`:41`、`:55`、`:112`、`:114`、`:117`。
 */

/**
 * @fn TZrBool ZrCore_View_IndexOffset(const SZrContiguousView *view, TZrInt64 index, TZrSize *offset, SZrViewDiagnostic *diagnostic)
 * ## Summary
 * 将有符号元素索引换算为 owner 相对字节偏移，为 Parser Exec IR 适配器提供边界和整数
 * 算术检查；返回的只是偏移，不是可解引用元素地址。
 *
 * ## Parameters
 * @param[in] view 待检查 descriptor；必须指向可读对象，调用期间借用、不保存；空指针会
 *                 被拒绝并报告 INVALID。
 * @param[in] index 有符号元素索引；仅 `0 <= index < view->length` 合法。
 * @param[out] offset 非空输出槽；成功时写入 `byteOffset + index * stride` 的字节偏移；
 *                   存储须与 view 和 diagnostic 分离。
 * @param[out] diagnostic 可选值型失败诊断；可为空；非空存储须与 view、offset 分离。
 *
 * ## Returns
 * @return descriptor、索引、输出槽和偏移算术均有效时返回 `ZR_TRUE`；否则返回
 * `ZR_FALSE`。失败时 offset 不是可用结果，调用方必须丢弃。
 *
 * ## Constraints and lifecycle
 * 当前实现以 currentGeneration=0 调用 Validate，因此这里不核对真实 generation。view
 * 只借用到本次同步调用，view、offset、diagnostic 的参数地址均不保存；函数不解引用或
 * 保活 owner，也不验证 backing extent、连续布局、alignment、pin 或读写权限。三个存储须
 * 分离。调用方必须在访问前自行完成这些检查并维持借用。不要把 offset 当作跨 safepoint
 * 稳定地址。
 *
 * ## References
 * 当前唯一产品调用入口是 `ZrParser_ExecIr_LowerArrayIndex`，其实现位于
 * `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c:6`；成功/负索引/越界/空
 * 输出用例见 `tests/core/test_ssa_arrays_slices.c:108`、`:110`、`:69`、`:82`。
 */
