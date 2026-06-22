#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CFG_INTERNAL_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CFG_INTERNAL_H

#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic.h"

typedef struct SZrParserCfgLoopTargets {
    TZrUInt32 breakTargetBlockId;
    TZrUInt32 continueTargetBlockId;
} SZrParserCfgLoopTargets;

typedef enum EZrParserCfgConstantKind {
    ZR_PARSER_CFG_CONSTANT_UNKNOWN = 0,
    ZR_PARSER_CFG_CONSTANT_BOOL,
    ZR_PARSER_CFG_CONSTANT_INTEGER,
    ZR_PARSER_CFG_CONSTANT_STRING,
    ZR_PARSER_CFG_CONSTANT_CHAR,
    ZR_PARSER_CFG_CONSTANT_FLOAT,
} EZrParserCfgConstantKind;

typedef struct SZrParserCfgConstant {
    EZrParserCfgConstantKind kind;
    TZrBool boolValue;
    TZrInt64 integerValue;
    SZrString *stringValue;
    TZrChar charValue;
    TZrDouble floatValue;
} SZrParserCfgConstant;

typedef enum EZrParserCfgThrowKind {
    ZR_PARSER_CFG_THROW_KIND_UNKNOWN = 0,
    ZR_PARSER_CFG_THROW_KIND_BOOL,
    ZR_PARSER_CFG_THROW_KIND_INTEGER,
    ZR_PARSER_CFG_THROW_KIND_STRING,
    ZR_PARSER_CFG_THROW_KIND_CHAR,
    ZR_PARSER_CFG_THROW_KIND_FLOAT,
} EZrParserCfgThrowKind;

typedef enum EZrParserCfgCatchMatch {
    ZR_PARSER_CFG_CATCH_MATCH_UNKNOWN = 0,
    ZR_PARSER_CFG_CATCH_MATCH_NO,
    ZR_PARSER_CFG_CATCH_MATCH_YES,
} EZrParserCfgCatchMatch;

typedef struct SZrParserCfgThrowTypeBinding {
    SZrString *name;
    TZrUInt32 knownKindMask;
    const struct SZrParserCfgThrowTypeBinding *next;
} SZrParserCfgThrowTypeBinding;

typedef struct SZrParserCfgThrowTypeBindingArray {
    SZrParserCfgThrowTypeBinding *items;
    TZrSize count;
    TZrSize capacity;
} SZrParserCfgThrowTypeBindingArray;

typedef TZrBool (*TZrParserCfgThrowKindMaskResolver)(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrUInt32 *outKnownKindMask);

SZrParserCfgBlock *cfg_get_block(SZrParserCfg *cfg, TZrUInt32 id);
TZrUInt32 cfg_add_block(SZrState *state,
                         SZrParserCfg *cfg,
                         EZrParserCfgBlockKind kind,
                         SZrAstNode *statement);
TZrBool cfg_add_edge(SZrParserCfg *cfg, TZrUInt32 fromId, TZrUInt32 toId);
TZrBool cfg_node_bool_constant(SZrAstNode *node, TZrBool *outValue);
TZrBool cfg_node_constant(SZrAstNode *node, SZrParserCfgConstant *outValue);
TZrBool cfg_constants_can_compare(const SZrParserCfgConstant *left,
                                  const SZrParserCfgConstant *right);
TZrBool cfg_constants_equal(const SZrParserCfgConstant *left,
                            const SZrParserCfgConstant *right);
TZrUInt32 cfg_throw_kind_mask(EZrParserCfgThrowKind kind);
TZrBool cfg_throw_kind_mask_has_single_kind(TZrUInt32 mask,
                                            EZrParserCfgThrowKind *outKind);
TZrBool cfg_string_equals(SZrString *left, SZrString *right);
SZrString *cfg_type_info_simple_name(SZrType *typeInfo);
TZrBool cfg_throw_kind_matches_type_name(EZrParserCfgThrowKind kind,
                                         SZrString *typeName);
TZrBool cfg_type_name_throw_kind(SZrString *typeName,
                                 EZrParserCfgThrowKind *outKind);
TZrBool cfg_node_identifier_name(SZrAstNode *node, SZrString **outName);
TZrBool cfg_throw_type_binding_lookup(
        const SZrParserCfgThrowTypeBinding *bindings,
        SZrString *name,
        TZrUInt32 *outKnownKindMask);
TZrBool cfg_throw_type_binding_merge_with_incoming(
        const SZrParserCfgThrowTypeBinding *bindings,
        SZrParserCfgThrowTypeBinding *binding);
TZrBool cfg_variable_declaration_throw_binding(
        SZrAstNode *node,
        SZrParserCfgThrowTypeBinding *outBinding);
TZrBool cfg_assignment_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBinding *outBinding);
TZrBool cfg_throw_type_binding_array_init(
        SZrParserCfgThrowTypeBindingArray *array,
        TZrSize capacity);
void cfg_throw_type_binding_array_free(
        SZrParserCfgThrowTypeBindingArray *array);
const SZrParserCfgThrowTypeBinding *cfg_throw_type_binding_array_chain_from(
        SZrParserCfgThrowTypeBindingArray *array,
        const SZrParserCfgThrowTypeBinding *bindings);
TZrBool cfg_throw_type_binding_array_find(
        const SZrParserCfgThrowTypeBindingArray *array,
        SZrString *name,
        TZrSize *outIndex);
TZrBool cfg_throw_type_binding_array_append_or_replace(
        SZrParserCfgThrowTypeBindingArray *array,
        const SZrParserCfgThrowTypeBinding *binding,
        const SZrParserCfgThrowTypeBinding **inOutBindings);
TZrBool cfg_throw_type_binding_array_append_or_merge_alternative(
        SZrParserCfgThrowTypeBindingArray *array,
        const SZrParserCfgThrowTypeBinding *binding,
        const SZrParserCfgThrowTypeBinding **inOutBindings);
TZrSize cfg_switch_expression_result_throw_binding_capacity(SZrAstNode *node);
TZrBool cfg_switch_expression_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings);
TZrBool cfg_if_expression_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings);
TZrBool cfg_while_loop_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings);
TZrBool cfg_for_loop_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings);
TZrBool cfg_foreach_loop_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings);
TZrBool cfg_node_stops_result_throw_binding_flow(SZrAstNode *node);
TZrSize cfg_node_result_throw_binding_capacity(SZrAstNode *node);
TZrBool cfg_node_collect_result_throw_bindings(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings);
TZrBool cfg_node_may_enter_catch(SZrAstNode *node);
TZrBool cfg_try_body_has_single_known_throw_kind(SZrAstNode *body,
                                                 EZrParserCfgThrowKind *outKind);
TZrBool cfg_try_body_throw_profile(SZrAstNode *body,
                                   TZrUInt32 *outKnownKindMask,
                                   TZrBool *outHasUnknownSource);
TZrBool cfg_catch_clause_is_catch_all(SZrAstNode *catchNode);
EZrParserCfgCatchMatch cfg_catch_clause_matches_known_throw_kind(
        SZrAstNode *catchNode,
        EZrParserCfgThrowKind knownThrowKind);
TZrBool cfg_catch_clause_match_known_throw_kinds(SZrAstNode *catchNode,
                                                 TZrUInt32 knownThrowKindMask,
                                                 TZrBool *outIsPrecise,
                                                 TZrUInt32 *outMatchedMask);
TZrBool cfg_connect_fallthrough(SZrParserCfg *cfg, TZrUInt32 fromId, TZrUInt32 toId);
TZrBool cfg_build_statement_body(SZrState *state,
                                 SZrParserCfg *cfg,
                                 SZrAstNode *body,
                                 TZrUInt32 predecessorBlockId,
                                 EZrSemanticReachabilityCause inheritedCause,
                                 SZrAstNode *inheritedCauseNode,
                                 const SZrParserCfgLoopTargets *loopTargets,
                                 TZrUInt32 *outLastBlockId);

TZrBool cfg_build_switch_statement(SZrState *state,
                                   SZrParserCfg *cfg,
                                   SZrAstNode *statement,
                                   TZrUInt32 *inOutPreviousBlockId,
                                   EZrSemanticReachabilityCause pendingCause,
                                   SZrAstNode *pendingCauseNode,
                                   const SZrParserCfgLoopTargets *loopTargets);
TZrBool cfg_build_try_statement(SZrState *state,
                                SZrParserCfg *cfg,
                                SZrAstNode *statement,
                                TZrUInt32 *inOutPreviousBlockId,
                                EZrSemanticReachabilityCause pendingCause,
                                SZrAstNode *pendingCauseNode,
                                const SZrParserCfgLoopTargets *loopTargets);
TZrBool cfg_build_while_statement(SZrState *state,
                                  SZrParserCfg *cfg,
                                  SZrAstNode *statement,
                                  TZrUInt32 *inOutPreviousBlockId,
                                  EZrSemanticReachabilityCause pendingCause,
                                  SZrAstNode *pendingCauseNode);
TZrBool cfg_build_for_statement(SZrState *state,
                                SZrParserCfg *cfg,
                                SZrAstNode *statement,
                                TZrUInt32 *inOutPreviousBlockId,
                                EZrSemanticReachabilityCause pendingCause,
                                SZrAstNode *pendingCauseNode);
TZrBool cfg_build_foreach_statement(SZrState *state,
                                    SZrParserCfg *cfg,
                                    SZrAstNode *statement,
                                    TZrUInt32 *inOutPreviousBlockId,
                                    EZrSemanticReachabilityCause pendingCause,
                                    SZrAstNode *pendingCauseNode);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_CFG_INTERNAL_H
