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

#endif
