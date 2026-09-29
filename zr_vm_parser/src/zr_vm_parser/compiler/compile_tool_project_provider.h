#ifndef ZR_VM_PARSER_COMPILE_TOOL_PROJECT_PROVIDER_H
#define ZR_VM_PARSER_COMPILE_TOOL_PROJECT_PROVIDER_H /**< @brief 防止项目 provider 内部接口被重复包含。 */

#include "compiler_internal.h"

typedef struct SZrCompileToolProjectProvider SZrCompileToolProjectProvider; /**< @brief 不透明的项目 CompileTool provider 记录。 */

/** @brief 解析 path build dependency 并绑定 alias。 @pre compiler state 已初始化；rawSpecifier 仅需在本次同步调用期间有效，aliasName 须存活至对应别名被移除或 compiler state 释放。 @return 复用或成功装载并绑定时为 true，其余失败为 false。 */ TZrBool ZrParser_CompileToolProjectProvider_Declare(
        SZrCompilerState *cs,
        SZrString *aliasName,
        const TZrChar *rawSpecifier,
        SZrFileRange location);
/** @brief 释放 compiler state 所拥有的项目 provider 与 artifact。 */ void ZrParser_CompileToolProjectProvider_FreeAll(SZrCompilerState *cs);

#endif
