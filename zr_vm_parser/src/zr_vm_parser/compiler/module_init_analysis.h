#ifndef ZR_VM_PARSER_MODULE_INIT_ANALYSIS_H
#define ZR_VM_PARSER_MODULE_INIT_ANALYSIS_H

#include "compiler_internal.h"

typedef enum EZrParserModuleInitSummaryState {
    ZR_PARSER_MODULE_INIT_SUMMARY_BUILDING = 0,
    ZR_PARSER_MODULE_INIT_SUMMARY_READY = 1,
    ZR_PARSER_MODULE_INIT_SUMMARY_FAILED = 2
} EZrParserModuleInitSummaryState;

typedef struct SZrModuleInitExportInfo {
    SZrString *name;
    TZrUInt8 accessModifier;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 symbolKind;
    TZrUInt8 prototypeType;
    TZrUInt16 reserved0;
    TZrUInt32 callableChildIndex;
    SZrFunctionTypedTypeRef valueType;
    TZrUInt32 parameterCount;
    SZrFunctionTypedTypeRef *parameterTypes;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 columnInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrUInt32 columnInSourceEnd;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 signatureHash;
} SZrModuleInitExportInfo;

typedef struct SZrModuleInitTypeDefInfo {
    SZrString *name;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
} SZrModuleInitTypeDefInfo;

typedef struct SZrModuleInitBindingInfo {
    SZrString *name;
    TZrUInt8 kind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    SZrString *moduleName;
    SZrString *symbolName;
    TZrUInt32 callableChildIndex;
} SZrModuleInitBindingInfo;

typedef struct SZrModuleInitCallableSummary {
    SZrString *name;
    TZrUInt32 callableChildIndex;
    TZrUInt32 effectCount;
    SZrFunctionModuleEffect *effects;
} SZrModuleInitCallableSummary;

typedef struct SZrParserModuleInitSummary {
    SZrString *moduleName;
    SZrString *moduleVersion;
    const SZrAstNode *astIdentity;
    TZrUInt64 moduleSignatureHash;
    TZrUInt8 state;
    TZrUInt8 isBinary;
    TZrUInt8 hasPrescan;
    TZrUInt8 hasAnalysis;
    TZrUInt8 validating;
    TZrUInt8 ownsAst;
    TZrUInt16 reserved0;
    SZrArray staticImports;
    SZrArray exports;
    SZrArray bindings;
    SZrArray typeDefs;
    SZrArray entryEffects;
    SZrArray exportedCallableSummaries;
    SZrFileRange errorLocation;
    TZrChar errorMessage[ZR_PARSER_DETAIL_BUFFER_LENGTH];
} SZrParserModuleInitSummary;

typedef struct SZrParserModuleInitCache {
    SZrArray summaries;
} SZrParserModuleInitCache;

ZR_PARSER_API const SZrParserModuleInitSummary *ZrParser_ModuleInitAnalysis_FindSummary(SZrGlobalState *global,
                                                                                        SZrString *moduleName);
ZR_PARSER_API const SZrParserModuleInitSummary *ZrParser_ModuleInitAnalysis_FindSummaryByAst(SZrGlobalState *global,
                                                                                              const SZrAstNode *ast);
ZR_PARSER_API TZrBool ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(SZrState *state,
                                                                             SZrString *moduleName,
                                                                             SZrAstNode *ast);
ZR_PARSER_API void ZrParser_ModuleInitAnalysis_ClearAstIdentity(SZrGlobalState *global, const SZrAstNode *ast);
ZR_PARSER_API TZrBool ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(SZrCompilerState *cs,
                                                                              SZrString *moduleName,
                                                                              SZrFunction *function);
ZR_PARSER_API TZrBool ZrParser_ModuleInitAnalysis_EnsureSummary(SZrCompilerState *cs, SZrString *moduleName);
/** @brief 将二进制 IO 流解析成供模块初始化摘要、导入类型元数据及 LSP 模块元数据分析的临时源树。
 *  @pre state 与 state->global 有效，outSource 可写；io 应有有效 read 回调和完整游标状态。
 *  @return 成功时把源树交给调用方并返回 true；失败时将 *outSource 置空并返回 false。
 *  @note 不关闭 io；调用方在本函数后关闭流，并在分析结束后调用 FreeBinaryMetadataSource。 */
ZR_PARSER_API TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(SZrState *state,
                                                                                     const SZrIo *io,
                                                                                     SZrIoSource **outSource);
/** @brief 回收 TryLoad 返回源树的原生数组，使用创建它的同一 global allocator。 */
ZR_PARSER_API void ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(SZrGlobalState *global, SZrIoSource *source);
ZR_PARSER_API void ZrParser_ModuleInitAnalysis_GlobalCleanup(SZrGlobalState *global, TZrPtr opaqueState);

#endif
