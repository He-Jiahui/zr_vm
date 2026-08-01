//
// Typed test metadata shared by the compiler, artifact consumers, and test host.
//

#ifndef ZR_VM_PARSER_TEST_CONTRACT_H
#define ZR_VM_PARSER_TEST_CONTRACT_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/function.h"

#define ZR_PARSER_TEST_MODULE_NAME "zr.testing"
#define ZR_PARSER_TEST_ROLE_QUALIFIED_NAME "zr.testing.test"
#define ZR_PARSER_TEST_CASE_ROLE_QUALIFIED_NAME "zr.testing.case"
#define ZR_PARSER_TEST_SKIP_ROLE_QUALIFIED_NAME "zr.testing.skip"
#define ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION 1U
#define ZR_PARSER_TEST_MANIFEST_MAX_ENTRIES 100000U
#define ZR_PARSER_TEST_MANIFEST_MAX_CASES_PER_ENTRY 10000U
#define ZR_PARSER_TEST_MANIFEST_MAX_ARGUMENTS_PER_CASE 256U
#define ZR_PARSER_TEST_MANIFEST_MAX_STRING_BYTES 65536U

typedef enum EZrParserTestConstantKind {
    ZR_PARSER_TEST_CONSTANT_NULL = 0,
    ZR_PARSER_TEST_CONSTANT_BOOL = 1,
    ZR_PARSER_TEST_CONSTANT_INT = 2,
    ZR_PARSER_TEST_CONSTANT_UINT = 3,
    ZR_PARSER_TEST_CONSTANT_FLOAT = 4,
    ZR_PARSER_TEST_CONSTANT_STRING = 5
} EZrParserTestConstantKind;

typedef struct SZrParserTestConstant {
    EZrParserTestConstantKind kind;
    union {
        TZrBool boolValue;
        TZrInt64 intValue;
        TZrUInt64 uintValue;
        TZrDouble floatValue;
        TZrChar *stringValue;
    } value;
} SZrParserTestConstant;

typedef struct SZrParserTestCaseDescriptor {
    TZrUInt32 ordinal;
    SZrParserTestConstant *arguments;
    TZrUInt32 argumentCount;
} SZrParserTestCaseDescriptor;

typedef struct SZrParserTestEntry {
    TZrUInt32 functionSymbolId;
    TZrUInt32 functionTypeId;
    TZrUInt32 callableChildIndex;
    TZrChar *moduleId;
    TZrChar *qualifiedName;
    SZrFileRange sourceRange;
    TZrBool isAsync;
    TZrChar *skipReason;
    SZrParserTestCaseDescriptor *cases;
    TZrUInt32 caseCount;
} SZrParserTestEntry;

typedef struct SZrParserTestManifest {
    TZrUInt32 schemaVersion;
    TZrChar *targetTriple;
    TZrUInt64 moduleGraphHash;
    SZrParserTestEntry *entries;
    TZrUInt32 entryCount;
} SZrParserTestManifest;

ZR_PARSER_API TZrBool ZrParser_TestManifest_Encode(
        struct SZrState *state,
        const SZrParserTestManifest *manifest,
        TZrByte **outData,
        TZrUInt32 *outLength);
ZR_PARSER_API TZrBool ZrParser_TestManifest_Decode(
        struct SZrState *state,
        const TZrByte *data,
        TZrUInt32 length,
        SZrParserTestManifest *outManifest);
ZR_PARSER_API TZrBool ZrParser_TestManifest_Validate(
        struct SZrState *state,
        const TZrByte *data,
        TZrUInt32 length);
ZR_PARSER_API void ZrParser_TestManifest_Free(
        struct SZrState *state,
        SZrParserTestManifest *manifest);

#endif // ZR_VM_PARSER_TEST_CONTRACT_H
