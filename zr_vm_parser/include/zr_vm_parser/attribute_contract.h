//
// Canonical typed attribute metadata shared by the compiler and consumers.
//

#ifndef ZR_VM_PARSER_ATTRIBUTE_CONTRACT_H
#define ZR_VM_PARSER_ATTRIBUTE_CONTRACT_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_library/zrm.h"

#define ZR_PARSER_ATTRIBUTE_MODULE_REFLECTION "zr.reflection"
#define ZR_PARSER_ATTRIBUTE_USAGE_QUALIFIED_NAME "zr.reflection.attributeUsage"
#define ZR_PARSER_ATTRIBUTE_CONDITIONAL_QUALIFIED_NAME "zr.compile.conditional"
#define ZR_PARSER_ATTRIBUTE_DECLARATION_TRANSFORM_QUALIFIED_NAME \
    "zr.compile.declarationTransform"
#define ZR_PARSER_ATTRIBUTE_MODULE_TESTING "zr.testing"
#define ZR_PARSER_ATTRIBUTE_TEST_QUALIFIED_NAME "zr.testing.test"
#define ZR_PARSER_ATTRIBUTE_TEST_CASE_QUALIFIED_NAME "zr.testing.case"
#define ZR_PARSER_ATTRIBUTE_TEST_SKIP_QUALIFIED_NAME "zr.testing.skip"

typedef enum EZrParserAttributeRole {
    ZR_PARSER_ATTRIBUTE_ROLE_NONE = 0,
    ZR_PARSER_ATTRIBUTE_ROLE_USAGE = 1,
    ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL = 2,
    ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM = 3,
    ZR_PARSER_ATTRIBUTE_ROLE_TEST = 4,
    ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE = 5,
    ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP = 6
} EZrParserAttributeRole;

typedef enum EZrParserAttributeTarget {
    ZR_PARSER_ATTRIBUTE_TARGET_NONE = 0,
    ZR_PARSER_ATTRIBUTE_TARGET_TYPE = 1U << 0,
    ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION = 1U << 1,
    ZR_PARSER_ATTRIBUTE_TARGET_FIELD = 1U << 2,
    ZR_PARSER_ATTRIBUTE_TARGET_METHOD = 1U << 3,
    ZR_PARSER_ATTRIBUTE_TARGET_PROPERTY = 1U << 4,
    ZR_PARSER_ATTRIBUTE_TARGET_PARAMETER = 1U << 5,
    ZR_PARSER_ATTRIBUTE_TARGET_ALL = (1U << 6) - 1U
} EZrParserAttributeTarget;

typedef enum EZrParserAttributeRetention {
    ZR_PARSER_ATTRIBUTE_RETENTION_SOURCE = 0,
    ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT = 1,
    ZR_PARSER_ATTRIBUTE_RETENTION_RUNTIME = 2
} EZrParserAttributeRetention;

typedef enum EZrParserAttributeValueKind {
    ZR_PARSER_ATTRIBUTE_VALUE_INVALID = 0,
    ZR_PARSER_ATTRIBUTE_VALUE_NULL = 1,
    ZR_PARSER_ATTRIBUTE_VALUE_BOOL = 2,
    ZR_PARSER_ATTRIBUTE_VALUE_INT = 3,
    ZR_PARSER_ATTRIBUTE_VALUE_UINT = 4,
    ZR_PARSER_ATTRIBUTE_VALUE_FLOAT = 5,
    ZR_PARSER_ATTRIBUTE_VALUE_STRING = 6,
    ZR_PARSER_ATTRIBUTE_VALUE_ENUM = 7,
    ZR_PARSER_ATTRIBUTE_VALUE_TYPE_ID = 8
} EZrParserAttributeValueKind;

typedef struct SZrParserAttributeFieldSchema {
    const TZrChar *name;
    EZrParserAttributeValueKind valueKind;
    TZrBool nullable;
} SZrParserAttributeFieldSchema;

typedef struct SZrParserAttributeUsage {
    TZrUInt32 targets;
    EZrParserAttributeRetention retention;
    TZrBool repeatable;
    TZrBool inherited;
} SZrParserAttributeUsage;

typedef struct SZrParserAttributeSchema {
    TZrUInt32 attributeId;
    TZrTypeId typeId;
    const TZrChar *qualifiedName;
    const TZrChar *ownerModule;
    EZrLibrary_ProviderPhase providerPhase;
    EZrParserAttributeRole role;
    SZrParserAttributeUsage usage;
    const SZrParserAttributeFieldSchema *fields;
    TZrSize fieldCount;
} SZrParserAttributeSchema;

typedef struct SZrParserAttributeConstant {
    EZrParserAttributeValueKind kind;
    union {
        TZrBool boolValue;
        TZrInt64 intValue;
        TZrUInt64 uintValue;
        TZrDouble floatValue;
        const TZrChar *stringValue;
        TZrInt64 enumValue;
        TZrTypeId typeId;
    } value;
} SZrParserAttributeConstant;

typedef struct SZrParserAttributeData {
    TZrUInt32 attributeId;
    TZrTypeId typeId;
    EZrParserAttributeRole role;
    EZrParserAttributeRetention retention;
    SZrFileRange sourceRange;
    const SZrParserAttributeConstant *fieldValues;
    TZrSize fieldValueCount;
} SZrParserAttributeData;

typedef enum EZrParserAttributeValidationError {
    ZR_PARSER_ATTRIBUTE_VALID = 0,
    ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_NOT_READONLY,
    ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_FIELD_NOT_PUBLIC_LET,
    ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_FIELD_TYPE,
    ZR_PARSER_ATTRIBUTE_ERROR_TARGET,
    ZR_PARSER_ATTRIBUTE_ERROR_REPEATABILITY,
    ZR_PARSER_ATTRIBUTE_ERROR_ARGUMENT_COUNT,
    ZR_PARSER_ATTRIBUTE_ERROR_ARGUMENT_TYPE,
    ZR_PARSER_ATTRIBUTE_ERROR_EMPTY_CONDITIONAL_FEATURE
} EZrParserAttributeValidationError;

ZR_PARSER_API TZrUInt32 ZrParser_AttributeContract_ComputeId(
        const TZrChar *qualifiedName);
ZR_PARSER_API const SZrParserAttributeSchema *ZrParser_AttributeContract_FindBuiltin(
        const TZrChar *qualifiedName);
ZR_PARSER_API const SZrParserAttributeSchema *ZrParser_AttributeContract_FindBuiltinByRole(
        EZrParserAttributeRole role);
// Resolves only canonical built-in attribute paths from a parsed decorator AST.
// Argument values remain the responsibility of the compiler's typed binder.
ZR_PARSER_API TZrBool ZrParser_AttributeContract_ResolveBuiltinDecorator(
        const SZrAstNode *decoratorNode,
        SZrParserAttributeData *outAttribute);
ZR_PARSER_API EZrParserAttributeValidationError ZrParser_AttributeContract_ValidateSchema(
        TZrBool isReadonly,
        const SZrParserAttributeFieldSchema *fields,
        const TZrBool *fieldIsPublicLet,
        TZrSize fieldCount);
ZR_PARSER_API EZrParserAttributeValidationError ZrParser_AttributeContract_ValidateApplication(
        const SZrParserAttributeSchema *schema,
        EZrParserAttributeTarget target,
        TZrSize existingApplicationCount,
        const SZrParserAttributeConstant *values,
        TZrSize valueCount);

#endif // ZR_VM_PARSER_ATTRIBUTE_CONTRACT_H
