#ifndef ZR_VM_TESTS_PARSER_CALL_CHAIN_POLYMORPHIC_COMPILE_FIXTURE_H
#define ZR_VM_TESTS_PARSER_CALL_CHAIN_POLYMORPHIC_COMPILE_FIXTURE_H

#include "path_support.h"

typedef struct SZrGlobalState SZrGlobalState;
typedef struct SZrState SZrState;
typedef struct SZrFunction SZrFunction;

typedef struct ZrCallChainPolymorphicCompileFixture {
    char projectPath[ZR_TESTS_PATH_MAX];
    char sourcePath[ZR_TESTS_PATH_MAX];
    char sourceRootPath[ZR_TESTS_PATH_MAX];
    char *source;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
} ZrCallChainPolymorphicCompileFixture;

TZrBool ZrTests_PrepareCallChainPolymorphicCompileFixture(ZrCallChainPolymorphicCompileFixture *fixture,
                                                          const TZrChar *artifactName);
void ZrTests_FreeCallChainPolymorphicCompileFixture(ZrCallChainPolymorphicCompileFixture *fixture);

#endif
