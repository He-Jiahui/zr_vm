#ifndef ZR_VM_PARSER_COMPILER_METADATA_STRING_HEAP_H
#define ZR_VM_PARSER_COMPILER_METADATA_STRING_HEAP_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_core/function.h"

struct SZrMetadataTokenTargetSignature;

typedef struct SZrMetadataStringHeapBuilder {
    SZrCompilerState *compiler;
    SZrMetadataStringHeapEntry *entries;
    TZrUInt32 length;
    TZrUInt32 capacity;
} SZrMetadataStringHeapBuilder;

/* Private metadata string heap builder; entries own capacity rows until compacted
 * and transferred. Values borrow the compiler's live string owners. */
TZrBool metadata_token_string_heap_add(SZrMetadataStringHeapBuilder *builder, SZrString *value);
TZrBool metadata_token_string_heap_collect_type(SZrMetadataStringHeapBuilder *builder,
        const SZrFunctionTypedTypeRef *typeRef);
TZrBool metadata_token_string_heap_collect_external_type_ref_names(SZrMetadataStringHeapBuilder *builder,
        const SZrFunctionTypedTypeRef *typeRef);
TZrBool metadata_token_string_heap_collect_symbol(SZrMetadataStringHeapBuilder *builder,
        const SZrFunctionTypedExportSymbol *symbol);
TZrBool metadata_token_type_def_string_collector(SZrCompilerState *cs, SZrString *value, void *userData);
TZrBool metadata_token_string_heap_collect_effect(SZrMetadataStringHeapBuilder *builder,
        const SZrFunctionModuleEffect *effect,
        const struct SZrMetadataTokenTargetSignature *targetSignature);
void metadata_token_sort_string_heap(SZrMetadataStringHeapBuilder *builder);
void metadata_token_string_heap_free(SZrCompilerState *cs, SZrMetadataStringHeapBuilder *builder);

/* Match allocation capacity to live length before Function ownership transfer.
 * A live compiler/state/global and valid independently owned builder storage are
 * required. Invalid geometry or allocation failure returns false without changing
 * any builder byte or freeing its entries. Equal length/capacity allocates nothing;
 * zero live rows release any capacity allocation without a zero-byte allocation.
 * A nonzero shrink uses an independent candidate, copies live rows and releases
 * the old block by its original capacity. No realloc or string/hash change occurs.
 * On success capacity == length; the caller may transfer entries and live length.
 */
TZrBool metadata_token_string_heap_compact(SZrMetadataStringHeapBuilder *builder);

#endif
