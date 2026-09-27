#ifndef ZR_VM_CLI_EXPLAIN_OPTIMIZE_COMMAND_H
#define ZR_VM_CLI_EXPLAIN_OPTIMIZE_COMMAND_H

#include <stdio.h>

#include "zr_vm_core/optimization_remark.h"
#include "zr_vm_cli/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 优化记录查询的 CLI 值对象；has* 区分未指定与数值为零的过滤条件。 */
typedef struct SZrCliExplainOptimizeOptions {
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reasonMask;
    TZrUInt32 statusMask;
    TZrUInt32 backendMask;
    TZrUInt32 evidenceMask;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrBool json;
    TZrBool hasModuleHash;
    TZrBool hasIrHash;
    TZrBool hasSourceVersion;
    TZrBool hasSourceId;
    TZrBool hasSourceRange;
    TZrBool hasPass;
    TZrBool hasModule;
    TZrBool reserved;
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
} SZrCliExplainOptimizeOptions;

/** @brief 初始化查询默认分页大小；解析器也会先调用此函数。 */
ZR_CLI_API void ZrCli_ExplainOptimizeOptions_Init(
        SZrCliExplainOptimizeOptions *options);

/** @brief 解析 explain optimize 后的过滤参数，与项目或产物装载无关。
 * @note 成功的 options 是可按值传递的查询；记录库由后续 RunStore 调用方提供。
 */
ZR_CLI_API TZrBool ZrCli_ExplainOptimizeOptions_Parse(
        int argc,
        const TZrChar *const *argv,
        SZrCliExplainOptimizeOptions *options,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

/** @brief 将借用的规范优化记录库查询并投影成文本或 JSON。
 * @pre store/options/output 在调用期间有效；函数释放查询页，不接管 store 或流。
 */
ZR_CLI_API int ZrCli_ExplainOptimize_RunStore(
        const SZrOptimizationRemarkStore *store,
        const SZrCliExplainOptimizeOptions *options,
        FILE *output,
        FILE *errorOutput);

/* 兼容已解析查询的命令分派名称，不创建第二套执行或解析逻辑。 */
#define ZrCli_ExplainOptimize_Run ZrCli_ExplainOptimize_RunStore
#define ZrCli_ExplainOptimize_Parse ZrCli_ExplainOptimizeOptions_Parse

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CLI_EXPLAIN_OPTIMIZE_COMMAND_H */
