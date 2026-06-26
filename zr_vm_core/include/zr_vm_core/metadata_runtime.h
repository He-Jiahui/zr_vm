#ifndef ZR_VM_CORE_METADATA_RUNTIME_H
#define ZR_VM_CORE_METADATA_RUNTIME_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/zrp_metadata.h"

struct SZrFunction;
struct SZrObjectModule;
struct SZrObjectPrototype;

typedef struct SZrMetadataRuntimeSignatureView {
    EZrMetadataSignatureNode rootNode;
    SZrZrpMetadataPoolSliceView blob;
    TZrUInt8 callingConvention;
    TZrUInt8 flags;
    TZrUInt32 genericParameterCount;
    TZrUInt32 parameterCount;
    TZrUInt32 returnTypeBlobOffset;
    TZrUInt32 parameterListBlobOffset;
    TZrUInt32 fieldTypeBlobOffset;
} SZrMetadataRuntimeSignatureView;

typedef struct SZrMetadataRuntimeSignatureTypeNodeView {
    EZrMetadataSignatureNode node;
    TZrUInt32 blobOffset;
    TZrUInt32 nextBlobOffset;
    TZrUInt32 payload0;
    TZrUInt32 payload1;
    TZrUInt32 baseTypeBlobOffset;
    TZrUInt32 childCount;
    TZrUInt32 childListBlobOffset;
} SZrMetadataRuntimeSignatureTypeNodeView;

typedef struct SZrMetadataRuntimeTypeSpecSignatureView {
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 signatureHash;
    SZrZrpMetadataPoolSliceView blob;
    SZrMetadataRuntimeSignatureTypeNodeView genericInstanceNode;
    SZrMetadataRuntimeSignatureTypeNodeView baseTypeNode;
    TZrUInt32 argumentCount;
    TZrUInt32 argumentListBlobOffset;
} SZrMetadataRuntimeTypeSpecSignatureView;

typedef struct SZrMetadataRuntimeTypeSpecGenericBindingView {
    SZrMetadataRuntimeTypeSpecSignatureView signatureView;
    TZrMetadataToken baseToken;
    const SZrMetadataTokenRecord *baseRecord;
} SZrMetadataRuntimeTypeSpecGenericBindingView;

typedef struct SZrMetadataRuntimeTypeSpecGenericArgumentView {
    SZrMetadataRuntimeTypeSpecGenericBindingView bindingView;
    SZrMetadataRuntimeSignatureTypeNodeView argumentNode;
    TZrUInt32 argumentIndex;
    TZrMetadataToken argumentToken;
    const SZrMetadataTokenRecord *argumentRecord;
} SZrMetadataRuntimeTypeSpecGenericArgumentView;

typedef struct SZrMetadataRuntimeMethodSpecSignatureView {
    TZrMetadataToken methodSpecToken;
    TZrMetadataToken methodToken;
    TZrUInt64 signatureHash;
    SZrZrpMetadataPoolSliceView blob;
    SZrMetadataRuntimeSignatureTypeNodeView genericInstanceNode;
    SZrMetadataRuntimeSignatureTypeNodeView methodNode;
    const SZrMetadataTokenRecord *methodRecord;
    TZrUInt32 argumentCount;
    TZrUInt32 argumentListBlobOffset;
} SZrMetadataRuntimeMethodSpecSignatureView;

typedef struct SZrMetadataRuntimeTypeDefLayoutBindingView {
    TZrMetadataToken typeDefToken;
    const SZrMetadataTokenRecord *typeRecord;
    const SZrZrpMetadataTypeDefRow *typeDefRow;
    TZrUInt32 typeLayoutId;
    TZrUInt32 cTypeId;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    const SZrTypeLayout *typeLayout;
} SZrMetadataRuntimeTypeDefLayoutBindingView;

typedef struct SZrMetadataRuntimeTypeSpecLayoutBindingView {
    TZrMetadataToken typeSpecToken;
    const SZrMetadataTokenRecord *typeRecord;
    const SZrZrpMetadataTypeSpecRow *typeSpecRow;
    SZrMetadataRuntimeTypeSpecGenericBindingView genericBindingView;
    TZrUInt32 typeLayoutId;
    TZrUInt32 cTypeId;
    TZrUInt64 signatureHash;
    const SZrTypeLayout *typeLayout;
} SZrMetadataRuntimeTypeSpecLayoutBindingView;

typedef struct SZrMetadataRuntimeFieldDefLayoutBindingView {
    TZrMetadataToken fieldDefToken;
    const SZrMetadataTokenRecord *fieldRecord;
    const SZrZrpMetadataFieldDefRow *fieldDefRow;
    TZrMetadataToken ownerTypeToken;
    const SZrMetadataTokenRecord *ownerTypeRecord;
    const SZrZrpMetadataTypeDefRow *ownerTypeDefRow;
    TZrUInt32 byteOffset;
    TZrUInt32 fieldTypeLayoutId;
    TZrUInt32 ownerTypeLayoutId;
    const SZrTypeLayout *fieldTypeLayout;
    const SZrTypeLayout *ownerTypeLayout;
} SZrMetadataRuntimeFieldDefLayoutBindingView;

enum {
    ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY = 8u
};

typedef struct SZrMetadataRuntime {
    struct SZrObjectModule *module;
    struct SZrFunction *metadataFunction;
    const SZrAotCodeRegistration *codeRegistration;
    TZrUInt32 functionCount;
    TZrUInt32 methodInfoCount;
    TZrUInt32 invokerCount;
    TZrUInt32 typeLayoutCount;
    TZrUInt32 typeLayoutTokenCount;
    TZrUInt32 gcDescriptorCount;
    TZrMetadataToken methodRecordCacheToken;
    const SZrMetadataTokenRecord *methodRecordCache;
    TZrMetadataToken fieldRecordCacheToken;
    const SZrMetadataTokenRecord *fieldRecordCache;
    TZrMetadataToken typeRecordCacheToken;
    const SZrMetadataTokenRecord *typeRecordCache;
    TZrMetadataToken signatureRecordCacheEntityToken;
    const SZrMetadataTokenRecord *signatureRecordCache;
    TZrUInt32 typeLayoutCacheNextIndex;
    TZrMetadataToken typeLayoutCacheTokens[ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY];
    TZrUInt32 typeLayoutCacheIds[ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY];
    const SZrTypeLayout *typeLayoutCacheLayouts[ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY];
    TZrBool hasZrpMetadata;
    const TZrByte *zrpMetadataBuffer;
    TZrSize zrpMetadataBufferLength;
    SZrZrpMetadataHeader zrpMetadataHeader;
} SZrMetadataRuntime;

ZR_CORE_API TZrBool ZrCore_MetadataRuntime_AttachZrpMetadata(SZrMetadataRuntime *runtime,
                                                             const TZrByte *buffer,
                                                             TZrSize bufferLength);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_GetZrpSectionView(SZrMetadataRuntime *runtime,
                                                             EZrZrpMetadataSectionKind sectionKind,
                                                             SZrZrpMetadataSectionView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_GetSignatureBlob(SZrMetadataRuntime *runtime,
                                                            TZrMetadataToken entityToken,
                                                            SZrZrpMetadataPoolSliceView *outSlice);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadSignatureView(SZrMetadataRuntime *runtime,
                                                             TZrMetadataToken entityToken,
                                                             SZrMetadataRuntimeSignatureView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadSignatureTypeNode(
        const SZrZrpMetadataPoolSliceView *blob,
        TZrUInt32 blobOffset,
        SZrMetadataRuntimeSignatureTypeNodeView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecSignatureView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecGenericBindingView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex,
        SZrMetadataRuntimeTypeSpecGenericArgumentView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        SZrMetadataRuntimeMethodSpecSignatureView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeDefToken,
        SZrMetadataRuntimeTypeDefLayoutBindingView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecLayoutBindingView *outView);
ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldDefToken,
        SZrMetadataRuntimeFieldDefLayoutBindingView *outView);
ZR_CORE_API const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveTypeTokenLayout(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeToken,
        TZrUInt32 *outTypeLayoutId);
ZR_CORE_API TZrMetadataToken ZrCore_MetadataRuntime_ResolveTypeLayoutToken(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId);
ZR_CORE_API TZrMetadataToken ZrCore_MetadataRuntime_ResolveCTypeIdToken(
        SZrMetadataRuntime *runtime,
        TZrUInt32 cTypeId);
ZR_CORE_API const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveTypeLayout(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId);
ZR_CORE_API const SZrAotGcDescriptor *ZrCore_MetadataRuntime_ResolveGcDescriptor(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId);
ZR_CORE_API void ZrCore_MetadataRuntime_AttachFunction(SZrMetadataRuntime *runtime,
                                                       struct SZrFunction *function);
ZR_CORE_API const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(
        const struct SZrFunction *function,
        TZrUInt32 typeLayoutId);
ZR_CORE_API const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(
        const struct SZrFunction *function,
        const struct SZrObjectPrototype *prototype,
        TZrUInt32 *outTypeLayoutId);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveMethodRecord(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken token);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveFieldRecord(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken token);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveTypeRecord(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken token);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveSignatureRecord(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken entityToken);

#endif // ZR_VM_CORE_METADATA_RUNTIME_H
