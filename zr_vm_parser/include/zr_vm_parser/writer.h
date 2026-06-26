//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_WRITER_H
#define ZR_VM_PARSER_WRITER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/state.h"

typedef struct SZrBinaryWriterOptions {
    const TZrChar *moduleName;
    const TZrChar *moduleHash;
} SZrBinaryWriterOptions;

typedef struct SZrAotManifestGenericRoot {
    const TZrChar *target;
    const TZrChar **arguments;
    TZrUInt32 argumentCount;
    TZrBool hasTypeSpecBinding;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 signatureHash;
    TZrBool hasMethodSpecBinding;
    TZrMetadataToken methodSpecToken;
    TZrMetadataToken methodSpecMethodToken;
    TZrUInt64 methodSpecSignatureHash;
    TZrBool hasGenericInstantiationBinding;
    TZrMetadataToken genericInstantiationBaseToken;
    TZrUInt32 genericInstantiationInstanceId;
    TZrUInt32 genericInstantiationShareKind;
} SZrAotManifestGenericRoot;

typedef enum EZrAotRuntimeFallbackWarningFlag {
    ZR_AOT_RUNTIME_FALLBACK_WARNING_NONE = 0,
    ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_CALL = 1u << 0,
    ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_VALUE_ACCESS = 1u << 1,
    ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_ITERATOR = 1u << 2,
    ZR_AOT_RUNTIME_FALLBACK_WARNING_REFLECTION = 1u << 3,
    ZR_AOT_RUNTIME_FALLBACK_WARNING_ALL = ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_CALL |
                                          ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_VALUE_ACCESS |
                                          ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_ITERATOR |
                                          ZR_AOT_RUNTIME_FALLBACK_WARNING_REFLECTION
} EZrAotRuntimeFallbackWarningFlag;

typedef struct SZrAotWriterOptions {
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;
    TZrUInt32 inputKind;
    const TZrChar *inputHash;
    const TZrByte *embeddedModuleBlob;
    TZrSize embeddedModuleBlobLength;
    TZrBool requireExecutableLowering;
    TZrBool requireFullAot;
    TZrBool enableCodeStripping;
    TZrBool stripGeneratedSymbols;
    TZrBool suppressRuntimeFallbackWarnings;
    TZrUInt32 suppressRuntimeFallbackWarningReasonMask;
    const TZrUInt32 *manifestPreserveFunctionFlatIndices;
    TZrUInt32 manifestPreserveFunctionFlatIndexCount;
    const SZrAotManifestGenericRoot *manifestPreserveGenericRoots;
    TZrUInt32 manifestPreserveGenericRootCount;
} SZrAotWriterOptions;

// 写入二进制文件 (.zro)
// 将编译后的函数写入二进制格式文件，符合 zr_io_conf.h 中定义的 .SOURCE 格式
ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFileWithOptions(SZrState *state,
                                                                 SZrFunction *function,
                                                                 const TZrChar *filename,
                                                                 const SZrBinaryWriterOptions *options);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

// 将 native helper 函数指针映射为稳定的可序列化 helper id
ZR_PARSER_API TZrUInt64 ZrParser_Writer_GetSerializableNativeHelperId(FZrNativeFunction function);

// 写入明文中间文件 (.zri)
// 将编译后的函数写入可读的明文格式文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteIntermediateFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

// 写入语法树文件 (.zrs)
// 将解析后的 AST 写入可读的明文格式文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteSyntaxTreeFile(SZrState *state, SZrAstNode *ast, const TZrChar *filename);

// 写入 AOT C 文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions(SZrState *state,
                                                               SZrFunction *function,
                                                               const TZrChar *filename,
                                                               const SZrAotWriterOptions *options);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile(SZrState *state, SZrFunction *function, const TZrChar *filename);
ZR_PARSER_API TZrBool ZrParser_Writer_ResolveTopLevelCallableFlatIndex(SZrState *state,
                                                                       SZrFunction *function,
                                                                       const TZrChar *callableName,
                                                                       TZrUInt32 *outFlatIndex);

// 写入 AOT LLVM IR 文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions(SZrState *state,
                                                                  SZrFunction *function,
                                                                  const TZrChar *filename,
                                                                  const SZrAotWriterOptions *options);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile(SZrState *state,
                                                       SZrFunction *function,
                                                       const TZrChar *filename);

#endif //ZR_VM_PARSER_WRITER_H

