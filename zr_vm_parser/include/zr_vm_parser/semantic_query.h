#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/semantic.h"

typedef enum EZrParserSemanticQueryScopeKind {
    ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE = 0,
    ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE
} EZrParserSemanticQueryScopeKind;

typedef struct SZrParserSemanticQueryScope {
    EZrParserSemanticQueryScopeKind kind;
    const SZrAstNode *root;
} SZrParserSemanticQueryScope;

typedef struct SZrParserSemanticQueryFacts {
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticNumericFact *numeric;
    const SZrSemanticReachabilityFact *reachability;
    const SZrSemanticLogicalFact *logical;
    const SZrSemanticOwnershipFact *ownership;
} SZrParserSemanticQueryFacts;

typedef struct SZrParserSemanticQueryDiagnostics {
    const SZrStructuredDiagnostic *items;
    TZrSize count;
} SZrParserSemanticQueryDiagnostics;

ZR_PARSER_API void ZrParser_SemanticQueryScope_Module(SZrParserSemanticQueryScope *scope);
ZR_PARSER_API void ZrParser_SemanticQueryScope_Node(SZrParserSemanticQueryScope *scope,
                                                    const SZrAstNode *root);

ZR_PARSER_API TZrBool ZrParser_SemanticQuery_TypeAt(const SZrSemanticContext *context,
                                                    SZrFileRange position,
                                                    const SZrParserSemanticQueryScope *scope,
                                                    SZrInferredType *outType);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DefinitionOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DefinitionsOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outDefinitions);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_ReferencesOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_FactsAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryFacts *outFacts);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_Diagnostics(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryDiagnostics *outDiagnostics);

#endif // ZR_VM_PARSER_SEMANTIC_QUERY_H
