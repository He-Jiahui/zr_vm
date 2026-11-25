#ifndef ZR_VM_PARSER_STRICT_NO_GC_H
#define ZR_VM_PARSER_STRICT_NO_GC_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/conf.h"

/* Report source policy violations before dead code or declaration initializers
 * can disappear during lowering. Messages and locations are borrowed. */
typedef void (*TZrStrictNoGcSourceDiagnostic)(void *context, const SZrFileRange *location,
                                               const TZrChar *code, const TZrChar *message);

typedef enum EZrStrictNoGcSourceResult {
    ZR_STRICT_NO_GC_SOURCE_OK = 0,
    ZR_STRICT_NO_GC_SOURCE_REJECTED,
    ZR_STRICT_NO_GC_SOURCE_UNSUPPORTED_AST
} EZrStrictNoGcSourceResult;

ZR_PARSER_API EZrStrictNoGcSourceResult ZrParser_StrictNoGc_CheckSource(
        SZrAstNode *root, TZrStrictNoGcSourceDiagnostic diagnostic, void *context);

/* Compiler-only bridge to the canonical, retained scalar IR producer. Source
 * must already have passed the closed scalar input gate. Never execute the
 * returned managed Function; only project its verified IR, then free it. */
struct SZrFunction;
ZR_PARSER_API struct SZrFunction *ZrParser_StrictNoGc_CompileScalarIr(
        SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName);

#endif
