#ifndef ZR_VM_CORE_BATCH_CONTRACT_H
#define ZR_VM_CORE_BATCH_CONTRACT_H

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 批量标量基线与后续向量化共用的诊断种类。
 * @note NONE 表示未记录错误；回调、别名与形状错误也由上层批量协议写入。 */
typedef enum EZrBatchDiagnosticCode {
    ZR_BATCH_DIAGNOSTIC_NONE = 0,
    ZR_BATCH_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_BATCH_DIAGNOSTIC_OVERFLOW,
    ZR_BATCH_DIAGNOSTIC_BOUNDS,
    ZR_BATCH_DIAGNOSTIC_STRIDE,
    ZR_BATCH_DIAGNOSTIC_SHAPE_MISMATCH,
    ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE,
    ZR_BATCH_DIAGNOSTIC_CALLBACK_ERROR,
    ZR_BATCH_DIAGNOSTIC_EFFECT_UNSAFE,
    ZR_BATCH_DIAGNOSTIC_UNSUPPORTED
} EZrBatchDiagnosticCode;

/** @brief 可选诊断输出；index 是 32 位逻辑元素索引，expected/actual 由错误种类解释。
 * @note Clear 将全部字段归零；错误路径通常先清空再填写，不拥有任何外部资源。 */
typedef struct SZrBatchDiagnostic {
    EZrBatchDiagnosticCode code;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrBatchDiagnostic;

/** @brief 二维批量形状；元素总数由 ElementCount 检查乘法溢出后取得。 */
typedef struct SZrBatchShape {
    TZrSize rows;
    TZrSize columns;
} SZrBatchShape;

/** @brief 借用的按字节步长访问视图；data 是逻辑元素零，负 stride 向低地址访问。
 * @pre 调用方须保证所有实际元素字节位于仍有效、可按用途读写的底层分配内；
 * byteLength 仅供当前数值边界检查，不能证明 data 之前的内存可访问。
 * TODO: 核定 byteLength 对负步长的区间定义，并让库调用方共享同一分配边界证明。 */
typedef struct SZrBatchBuffer {
    TZrPtr data;
    TZrSize byteLength;
    TZrMemoryOffset stride;
    TZrSize elementSize;
} SZrBatchBuffer;

/** @brief 清零可选诊断；空指针不执行操作。 */
ZR_CORE_API void ZrCore_BatchDiagnostic_Clear(SZrBatchDiagnostic *diagnostic);
/** @brief 返回诊断码的静态名称；未知值返回 "unknown"，结果无需释放。 */
ZR_CORE_API const TZrChar *ZrCore_BatchDiagnostic_Name(
        EZrBatchDiagnosticCode code);
/** @brief 检查 rows * columns 可由 TZrSize 表示并写入元素数。
 * @return 失败时设置可选诊断，保留原有 *count；空 count 为 INVALID_ARGUMENT。 */
ZR_CORE_API TZrBool ZrCore_BatchShape_ElementCount(
        SZrBatchShape shape, TZrSize *count, SZrBatchDiagnostic *diagnostic);
/** @brief 校验缓冲区描述符、步长乘法及末元素相对距离。
 * @pre buffer 描述的底层分配与存活期由调用方保证，尤其是负 stride 的 data 之前区域。
 * @note count 为零时允许 data 为空，但 elementSize 与 stride 仍须非零；diagnostic 可为空。
 * BUG: 64 位平台上末端偏移为 INT64_MIN 时取负溢出；单元素 INT64_MIN 步长误报溢出。
 * BUG: ptrdiff_t 为 32 位时仍按 64 位界限校验，可在计算偏移前漏掉乘法溢出。 */
ZR_CORE_API TZrBool ZrCore_BatchBuffer_Validate(
        const SZrBatchBuffer *buffer, TZrSize count,
        SZrBatchDiagnostic *diagnostic);
/** @brief 校验整段视图后计算逻辑元素的有符号字节偏移。
 * @return 验证或索引失败时保留 *offset，设置可选诊断；空 offset 当前报告 BOUNDS。 */
ZR_CORE_API TZrBool ZrCore_BatchBuffer_Offset(
        const SZrBatchBuffer *buffer, TZrSize count, TZrSize index,
        TZrMemoryOffset *offset, SZrBatchDiagnostic *diagnostic);
/** @brief 校验两个视图后估算它们是否可能共享字节，以保护 Map/Zip 的写入顺序。
 * @note 成功时写入 *mayAlias；无元素的视图使 *mayAlias 为假。验证失败时不保证写入该输出。
 * BUG: 负 stride 时当前区间仍从 data 向高地址推导，可漏报实际重叠。 */
ZR_CORE_API TZrBool ZrCore_BatchBuffers_MayAlias(
        const SZrBatchBuffer *left, TZrSize leftCount,
        const SZrBatchBuffer *right, TZrSize rightCount,
        TZrBool *mayAlias, SZrBatchDiagnostic *diagnostic);
/** @brief 只比较两视图的起点、长度、步长及槽宽；不校验指针或元素数。 */
ZR_CORE_API TZrBool ZrCore_BatchBuffers_ExactLayout(
        const SZrBatchBuffer *left, const SZrBatchBuffer *right);

#ifdef __cplusplus
}
#endif

#endif
