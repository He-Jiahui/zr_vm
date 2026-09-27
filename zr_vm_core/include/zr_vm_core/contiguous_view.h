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
 * @pre 调用方在实际访问前保证 ownerRoot 保活、所需 pin 与真实 backing storage 边界；本函数不接管它。
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
 * @return 失败时 slice 保持原值；调用方继续负责 owner 与 generation 的有效性。 */
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
