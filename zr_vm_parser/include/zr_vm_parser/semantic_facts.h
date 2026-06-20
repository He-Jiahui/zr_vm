#ifndef ZR_VM_PARSER_SEMANTIC_FACTS_H
#define ZR_VM_PARSER_SEMANTIC_FACTS_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

#ifndef ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
#define ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
typedef TZrUInt32 TZrTypeId;
typedef TZrUInt32 TZrSymbolId;
typedef TZrUInt32 TZrOverloadSetId;
typedef TZrUInt32 TZrLifetimeRegionId;
#endif

#ifndef ZR_SEMANTIC_ID_INVALID
#define ZR_SEMANTIC_ID_INVALID ((TZrUInt32)0U)
#endif

typedef struct SZrSemanticContext SZrSemanticContext;

typedef enum EZrSemanticFactExactness {
    ZR_SEMANTIC_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_FACT_APPROXIMATE,
    ZR_SEMANTIC_FACT_EXACT
} EZrSemanticFactExactness;

typedef enum EZrSemanticExpressionFactKind {
    ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_EXPRESSION_FACT_LITERAL,
    ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER,
    ZR_SEMANTIC_EXPRESSION_FACT_BINARY,
    ZR_SEMANTIC_EXPRESSION_FACT_UNARY,
    ZR_SEMANTIC_EXPRESSION_FACT_CALL,
    ZR_SEMANTIC_EXPRESSION_FACT_MEMBER,
    ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT,
    ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL,
    ZR_SEMANTIC_EXPRESSION_FACT_ARRAY,
    ZR_SEMANTIC_EXPRESSION_FACT_OBJECT,
    ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA,
    ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN,
    ZR_SEMANTIC_EXPRESSION_FACT_CONVERSION,
    ZR_SEMANTIC_EXPRESSION_FACT_ERROR
} EZrSemanticExpressionFactKind;

typedef enum EZrSemanticValueKind {
    ZR_SEMANTIC_VALUE_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_VALUE_KIND_NULL,
    ZR_SEMANTIC_VALUE_KIND_BOOL,
    ZR_SEMANTIC_VALUE_KIND_INT64,
    ZR_SEMANTIC_VALUE_KIND_UINT64,
    ZR_SEMANTIC_VALUE_KIND_DOUBLE,
    ZR_SEMANTIC_VALUE_KIND_STRING
} EZrSemanticValueKind;

typedef enum EZrSemanticReferenceKind {
    ZR_SEMANTIC_REFERENCE_UNKNOWN = 0,
    ZR_SEMANTIC_REFERENCE_DECLARATION,
    ZR_SEMANTIC_REFERENCE_READ,
    ZR_SEMANTIC_REFERENCE_WRITE,
    ZR_SEMANTIC_REFERENCE_CALL,
    ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS,
    ZR_SEMANTIC_REFERENCE_MEMBER_WRITE
} EZrSemanticReferenceKind;

typedef enum EZrSemanticNumericFactKind {
    ZR_SEMANTIC_NUMERIC_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_NUMERIC_FACT_LITERAL,
    ZR_SEMANTIC_NUMERIC_FACT_PROMOTION,
    ZR_SEMANTIC_NUMERIC_FACT_CONVERSION,
    ZR_SEMANTIC_NUMERIC_FACT_RANGE
} EZrSemanticNumericFactKind;

typedef enum EZrSemanticReachabilityState {
    ZR_SEMANTIC_REACHABILITY_UNKNOWN = 0,
    ZR_SEMANTIC_REACHABILITY_REACHABLE,
    ZR_SEMANTIC_REACHABILITY_UNREACHABLE
} EZrSemanticReachabilityState;

typedef enum EZrSemanticReachabilityCause {
    ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN = 0,
    ZR_SEMANTIC_REACHABILITY_AFTER_RETURN,
    ZR_SEMANTIC_REACHABILITY_AFTER_THROW,
    ZR_SEMANTIC_REACHABILITY_AFTER_BREAK,
    ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE,
    ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE,
    ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
    ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT,
    ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH,
    ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP
} EZrSemanticReachabilityCause;

typedef enum EZrSemanticLogicalFactKind {
    ZR_SEMANTIC_LOGICAL_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_LOGICAL_FACT_TRUTHY,
    ZR_SEMANTIC_LOGICAL_FACT_FALSY,
    ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE,
    ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
    ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT
} EZrSemanticLogicalFactKind;

typedef enum EZrSemanticOwnershipFactKind {
    ZR_SEMANTIC_OWNERSHIP_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION,
    ZR_SEMANTIC_OWNERSHIP_FACT_BORROW,
    ZR_SEMANTIC_OWNERSHIP_FACT_MOVE,
    ZR_SEMANTIC_OWNERSHIP_FACT_COPY,
    ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE,
    ZR_SEMANTIC_OWNERSHIP_FACT_ERROR
} EZrSemanticOwnershipFactKind;

typedef union SZrSemanticConstantValue {
    TZrBool boolValue;
    TZrInt64 int64Value;
    TZrUInt64 uint64Value;
    TZrDouble doubleValue;
    SZrString *stringValue;
} SZrSemanticConstantValue;

typedef struct SZrSemanticExpressionFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticExpressionFactKind kind;
    EZrSemanticFactExactness exactness;
    SZrInferredType inferredType;
    EZrSemanticValueKind valueKind;
    TZrBool hasConstant;
    SZrSemanticConstantValue constantValue;
    TZrBool hasCallInfo;
    SZrString *callTargetName;
    SZrFileRange callTargetRange;
    TZrSize argumentCount;
    TZrBool hasNamedArguments;
    TZrBool isMemberCall;
    TZrBool hasMemberInfo;
    SZrString *memberName;
    SZrFileRange memberRange;
    TZrBool memberIsComputed;
    SZrString *diagnosticMessage;
} SZrSemanticExpressionFact;

typedef struct SZrSemanticReferenceFact {
    SZrAstNode *node;
    SZrFileRange range;
    SZrFileRange declarationRange;
    EZrSemanticReferenceKind kind;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    SZrString *name;
    TZrBool isResolved;
} SZrSemanticReferenceFact;

typedef struct SZrSemanticNumericFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticNumericFactKind kind;
    EZrSemanticFactExactness exactness;
    EZrValueType sourceType;
    EZrValueType targetType;
    TZrBool hasRange;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrBool hasUnsignedRange;
    TZrUInt64 minUnsignedValue;
    TZrUInt64 maxUnsignedValue;
    TZrDouble minDoubleValue;
    TZrDouble maxDoubleValue;
    TZrBool mayOverflow;
} SZrSemanticNumericFact;

typedef struct SZrSemanticReachabilityFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticReachabilityState state;
    EZrSemanticReachabilityCause cause;
    SZrAstNode *causeNode;
} SZrSemanticReachabilityFact;

typedef struct SZrSemanticLogicalFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticLogicalFactKind kind;
    EZrSemanticFactExactness exactness;
    TZrBool knownValue;
    TZrBool hasKnownValue;
    SZrAstNode *relatedNode;
} SZrSemanticLogicalFact;

typedef struct SZrSemanticOwnershipFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticOwnershipFactKind kind;
    EZrOwnershipQualifier qualifier;
    TZrSymbolId symbolId;
    TZrLifetimeRegionId lifetimeRegionId;
    TZrLifetimeRegionId ownerLifetimeRegionId;
    SZrAstNode *relatedNode;
    TZrBool isViolation;
    SZrString *diagnosticMessage;
} SZrSemanticOwnershipFact;

ZR_PARSER_API void ZrParser_SemanticFacts_Init(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticFacts_Reset(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticFacts_Free(SZrSemanticContext *context);

ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendExpression(SZrSemanticContext *context,
                                                              const SZrSemanticExpressionFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReference(SZrSemanticContext *context,
                                                             const SZrSemanticReferenceFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendNumeric(SZrSemanticContext *context,
                                                           const SZrSemanticNumericFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReachability(SZrSemanticContext *context,
                                                                const SZrSemanticReachabilityFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendLogical(SZrSemanticContext *context,
                                                           const SZrSemanticLogicalFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendOwnership(SZrSemanticContext *context,
                                                             const SZrSemanticOwnershipFact *fact);

ZR_PARSER_API const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
        const SZrSemanticContext *context,
        SZrFileRange position,
        EZrSemanticReferenceKind kind);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        EZrSemanticReferenceKind kind);
ZR_PARSER_API const SZrSemanticNumericFact *ZrParser_SemanticFacts_FindNumericByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticReachabilityFact *ZrParser_SemanticFacts_FindReachabilityAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
ZR_PARSER_API const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
ZR_PARSER_API const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);

#endif // ZR_VM_PARSER_SEMANTIC_FACTS_H
