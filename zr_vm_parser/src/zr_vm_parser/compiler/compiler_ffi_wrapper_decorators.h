#ifndef ZR_VM_PARSER_COMPILER_FFI_WRAPPER_DECORATORS_H
#define ZR_VM_PARSER_COMPILER_FFI_WRAPPER_DECORATORS_H

#include "compiler_internal.h"

typedef struct SZrFfiWrapperDecoratorContract {
    SZrString *lowering;
    SZrString *viewType;
    SZrString *underlying;
    SZrString *ownerMode;
    SZrString *releaseHook;
    TZrBool hasLowering;
    TZrBool hasViewType;
    TZrBool hasUnderlying;
    TZrBool hasOwnerMode;
    TZrBool hasReleaseHook;
} SZrFfiWrapperDecoratorContract;

TZrBool compiler_ffi_wrapper_bind_decorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration,
        SZrFfiWrapperDecoratorContract *outContract);

#endif
