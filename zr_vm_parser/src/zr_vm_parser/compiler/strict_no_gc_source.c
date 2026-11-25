#include "zr_vm_parser/strict_no_gc.h"
#include "zr_vm_parser/parser.h"

/* Explicit own and ordinary new currently share the construct AST kind, but
 * only the former has the resource-surface marker from the source parser. */
typedef struct SZrStrictNoGcSourceCheck {
    TZrStrictNoGcSourceDiagnostic diagnostic;
    void *context;
    TZrBool rejected;
} SZrStrictNoGcSourceCheck;

static TZrBool check_node(const SZrAstNode *node, void *context) {
    SZrStrictNoGcSourceCheck *check = context;
    const TZrChar *code;
    const TZrChar *message;
    if (node->type == ZR_AST_CONSTRUCT_EXPRESSION && node->data.constructExpression.isNew &&
        !node->data.constructExpression.isResourceSurface) {
        code = "strict_no_gc_new";
        message = "StrictNoGC forbids ordinary new. Use init for an inline value, a borrow for temporary access, or own for an escaping owner.";
    } else if (node->type == ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION &&
               node->data.ownershipIntrinsicExpression.operation == ZR_OWNERSHIP_INTRINSIC_INTO_GC) {
        code = "strict_no_gc_into_gc";
        message = "StrictNoGC forbids intoGc. Keep the Unique or Shared owner, or pass a scoped borrow.";
    } else {
        return ZR_TRUE;
    }
    check->rejected = ZR_TRUE;
    if (check->diagnostic != ZR_NULL) {
        check->diagnostic(check->context, &node->location, code, message);
    }
    return ZR_TRUE;
}

EZrStrictNoGcSourceResult ZrParser_StrictNoGc_CheckSource(
        SZrAstNode *root, TZrStrictNoGcSourceDiagnostic diagnostic, void *context) {
    SZrStrictNoGcSourceCheck check = {diagnostic, context, ZR_FALSE};
    if (!ZrParser_Ast_Visit(root, check_node, &check)) {
        return ZR_STRICT_NO_GC_SOURCE_UNSUPPORTED_AST;
    }
    return check.rejected ? ZR_STRICT_NO_GC_SOURCE_REJECTED : ZR_STRICT_NO_GC_SOURCE_OK;
}
