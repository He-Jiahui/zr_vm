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
} SZrModuleInitExportInfo;

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
    const SZrAstNode *astIdentity;
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
ZR_PARSER_API TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(SZrState *state,
                                                                                     const SZrIo *io,
                                                                                     SZrIoSource **outSource);
ZR_PARSER_API TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromPath(SZrState *state,
                                                                                       const TZrChar *binaryPath,
                                                                                       SZrIoSource **outSource);
ZR_PARSER_API void ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(SZrGlobalState *global, SZrIoSource *source);
ZR_PARSER_API void ZrParser_ModuleInitAnalysis_GlobalCleanup(SZrGlobalState *global, TZrPtr opaqueState);

#endif
