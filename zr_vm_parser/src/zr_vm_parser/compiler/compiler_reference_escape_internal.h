#ifndef ZR_VM_PARSER_COMPILER_REFERENCE_ESCAPE_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_REFERENCE_ESCAPE_INTERNAL_H

#include "compiler_internal.h"

typedef struct SZrReferenceEscapeBinding {
    SZrString *name;
    const SZrType *declaredType;
    TZrInt32 scopeDepth;
    TZrBool isReference;
    TZrBool isRefLike;
    TZrBool isScoped;
    TZrBool isOut;
    TZrBool isWritable;
    EZrSemanticEscapeState escapeBound;
    SZrFileRange originRange;
    TZrUInt32 declarationSuspensionEpoch;
    TZrSize mutableCaptureLastUseOffset;
    SZrFileRange mutableCaptureRange;
    TZrBool isClosure;
    EZrSemanticEscapeState closureEscapeBound;
    SZrString *closureWritableCaptureName;
    SZrFileRange closureCaptureOriginRange;
} SZrReferenceEscapeBinding;

typedef struct SZrReferenceEscapeProvenance {
    TZrBool isReference;
    TZrBool isRefLike;
    TZrBool isScoped;
    TZrBool isOut;
    TZrBool isWritable;
    EZrSemanticEscapeState escapeBound;
    SZrFileRange originRange;
    SZrString *bindingName;
    TZrBool isClosure;
    EZrSemanticEscapeState closureEscapeBound;
    SZrString *closureWritableCaptureName;
    SZrFileRange closureCaptureOriginRange;
} SZrReferenceEscapeProvenance;

typedef struct SZrReferenceEscapeContext {
    SZrCompilerState *compiler;
    struct SZrReferenceEscapeContext *parent;
    SZrRefStructTypeSet refStructTypeStorage;
    SZrRefStructTypeSet *refStructTypes;
    SZrArray bindings; /* SZrReferenceEscapeBinding */
    TZrInt32 scopeDepth;
    TZrUInt32 suspensionEpoch;
    SZrFileRange suspensionRange;
    const TZrChar *suspensionName;
    SZrAstNode *bodyRoot;
    SZrType *returnType;
    TZrBool isFunctionBody;
    TZrBool isClosureBody;
    TZrBool isGeneratorBody;
    SZrFileRange closureRange;
    TZrBool hasReferenceCapture;
    TZrBool hasScopedCapture;
    EZrSemanticEscapeState captureEscapeBound;
    SZrFileRange captureOriginRange;
    SZrString *writableCaptureName;
    SZrFileRange writableCaptureOriginRange;
} SZrReferenceEscapeContext;

void reference_escape_provenance_reset(
        SZrReferenceEscapeProvenance *provenance);
TZrBool reference_escape_context_init(
        SZrReferenceEscapeContext *context,
        SZrCompilerState *compiler,
        SZrReferenceEscapeContext *parent);
void reference_escape_context_free(SZrReferenceEscapeContext *context);
SZrReferenceEscapeBinding *reference_escape_find_binding(
        SZrReferenceEscapeContext *context,
        SZrString *name);
void reference_escape_enter_scope(SZrReferenceEscapeContext *context);
void reference_escape_leave_scope(SZrReferenceEscapeContext *context);
SZrReferenceEscapeBinding *reference_escape_push_binding(
        SZrReferenceEscapeContext *context,
        const SZrReferenceEscapeBinding *binding);
TZrBool reference_escape_validate_target(
        SZrReferenceEscapeContext *context,
        const SZrReferenceEscapeProvenance *provenance,
        EZrSemanticEscapeState target,
        SZrFileRange escapeRange,
        const TZrChar *reason);
TZrBool reference_escape_type_is_reference(const SZrType *type);
TZrBool reference_escape_type_is_ref_like(
        const SZrReferenceEscapeContext *context,
        const SZrType *type);
void reference_escape_register_parameters(
        SZrReferenceEscapeContext *context,
        SZrAstNodeArray *parameters,
        SZrParameter *vararg);
TZrSize reference_escape_last_identifier_offset(
        SZrAstNode *node,
        SZrString *name);
TZrBool reference_escape_analyze_expression(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node,
        TZrBool wantReference,
        SZrReferenceEscapeProvenance *provenance);
TZrBool reference_escape_analyze_node(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node);

#endif
