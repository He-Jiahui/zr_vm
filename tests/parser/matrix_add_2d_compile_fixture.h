#ifndef ZR_VM_TESTS_PARSER_MATRIX_ADD_2D_COMPILE_FIXTURE_H
#define ZR_VM_TESTS_PARSER_MATRIX_ADD_2D_COMPILE_FIXTURE_H

#include "path_support.h"

typedef struct SZrGlobalState SZrGlobalState;
typedef struct SZrState SZrState;
typedef struct SZrFunction SZrFunction;

typedef struct ZrMatrixAdd2dCompileFixture {
    char projectPath[ZR_TESTS_PATH_MAX];
    char sourcePath[ZR_TESTS_PATH_MAX];
    char sourceRootPath[ZR_TESTS_PATH_MAX];
    char *source;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
} ZrMatrixAdd2dCompileFixture;

TZrBool ZrTests_PrepareMatrixAdd2dCompileFixture(ZrMatrixAdd2dCompileFixture *fixture, const TZrChar *artifactName);
void ZrTests_FreeMatrixAdd2dCompileFixture(ZrMatrixAdd2dCompileFixture *fixture);

#endif
