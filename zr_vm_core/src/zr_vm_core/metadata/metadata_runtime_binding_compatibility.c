#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

/**
 * @brief 元数据绑定兼容性门禁的共享判定实现。
 *
 * 本文件把链接期保存的引用身份与运行时解析结果按固定优先级比较；模块加载器、
 * typed export 检查和 AOT 直接调用回退共用此处结果，避免各调用链采用不同身份规则。
 * 报告中的字符串和记录指针均借用调用方对象，调用方必须维持其所属 runtime/function
 * 生命周期。TODO: 明确旧版或不可解析版本字符串继续绕过版本范围检查的长期兼容协议。
 */

/**
 * @brief 解析本运行时接受的三段式十进制版本。
 * @pre text 必须指向以 NUL 结尾的字符串；输出参数可分别省略。
 * @return 仅当完整字符串严格为 major.minor.patch 且三段均无溢出时返回真。
 *
 * 这是运行时绑定范围比较所需的最小版本语法，不处理 prerelease/build metadata。
 * TODO: 若元数据版本协议扩展到完整 SemVer，需同步扩展这里及其边界测试。
 */
static TZrBool metadata_runtime_parse_semver(const TZrChar *text,
                                             TZrUInt32 *outMajor,
                                             TZrUInt32 *outMinor,
                                             TZrUInt32 *outPatch) {
    TZrUInt32 parts[3] = {0u, 0u, 0u};
    TZrUInt32 partIndex = 0u;
    TZrSize offset = 0u;

    if (outMajor != ZR_NULL) {
        *outMajor = 0u;
    }
    if (outMinor != ZR_NULL) {
        *outMinor = 0u;
    }
    if (outPatch != ZR_NULL) {
        *outPatch = 0u;
    }
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    while (partIndex < 3u) {
        TZrUInt32 value = 0u;
        TZrBool hasDigit = ZR_FALSE;

        while (text[offset] >= '0' && text[offset] <= '9') {
            TZrUInt32 digit = (TZrUInt32)(text[offset] - '0');
            if (value > (((TZrUInt32)0xFFFFFFFFu) - digit) / 10u) {
                return ZR_FALSE;
            }
            value = value * 10u + digit;
            hasDigit = ZR_TRUE;
            offset++;
        }
        if (!hasDigit) {
            return ZR_FALSE;
        }
        parts[partIndex++] = value;
        if (partIndex == 3u) {
            break;
        }
        if (text[offset] != '.') {
            return ZR_FALSE;
        }
        offset++;
    }

    if (text[offset] != '\0') {
        return ZR_FALSE;
    }

    if (outMajor != ZR_NULL) {
        *outMajor = parts[0];
    }
    if (outMinor != ZR_NULL) {
        *outMinor = parts[1];
    }
    if (outPatch != ZR_NULL) {
        *outPatch = parts[2];
    }
    return ZR_TRUE;
}

/** @brief 将托管字符串适配到本文件的严格版本语法检查；空指针视为不可解析。 */
static TZrBool metadata_runtime_string_is_semver(SZrString *value) {
    TZrUInt32 major;
    TZrUInt32 minor;
    TZrUInt32 patch;

    return metadata_runtime_parse_semver(value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL,
                                         &major,
                                         &minor,
                                         &patch);
}

/**
 * @brief 按 major/minor/patch 数值顺序比较已验证版本。
 * @note 范围门禁先验证两侧；无效输入的 0 仅是防御性返回，不代表版本相等。
 */
static int metadata_runtime_compare_semver(SZrString *left, SZrString *right) {
    TZrUInt32 leftMajor;
    TZrUInt32 leftMinor;
    TZrUInt32 leftPatch;
    TZrUInt32 rightMajor;
    TZrUInt32 rightMinor;
    TZrUInt32 rightPatch;

    if (!metadata_runtime_parse_semver(left != ZR_NULL ? ZrCore_String_GetNativeString(left) : ZR_NULL,
                                       &leftMajor,
                                       &leftMinor,
                                       &leftPatch) ||
        !metadata_runtime_parse_semver(right != ZR_NULL ? ZrCore_String_GetNativeString(right) : ZR_NULL,
                                       &rightMajor,
                                       &rightMinor,
                                       &rightPatch)) {
        return 0;
    }

    if (leftMajor != rightMajor) {
        return leftMajor < rightMajor ? -1 : 1;
    }
    if (leftMinor != rightMinor) {
        return leftMinor < rightMinor ? -1 : 1;
    }
    if (leftPatch != rightPatch) {
        return leftPatch < rightPatch ? -1 : 1;
    }
    return 0;
}

/**
 * @brief 将调用方 MemberRef 上的可选版本区间应用到实际模块版本。
 * @note 完整区间采用 [minInclusive, maxExclusive)；无引用记录、缺边界或旧式
 *       不可解析字符串目前沿用兼容路径并放行，不把缺失版本误报为不兼容。
 * TODO: 版本数据不可解析时是否应继续 fail-open，需由元数据格式兼容策略明确。
 */
static TZrBool metadata_runtime_version_range_matches(const SZrMetadataTokenRecord *refRecord,
                                                      SZrString *actualModuleVersion) {
    if (refRecord == ZR_NULL ||
        refRecord->minModuleVersionInclusive == ZR_NULL ||
        refRecord->maxModuleVersionExclusive == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_runtime_string_is_semver(actualModuleVersion) ||
        !metadata_runtime_string_is_semver(refRecord->minModuleVersionInclusive) ||
        !metadata_runtime_string_is_semver(refRecord->maxModuleVersionExclusive)) {
        return ZR_TRUE;
    }

    return metadata_runtime_compare_semver(actualModuleVersion, refRecord->minModuleVersionInclusive) >= 0 &&
                   metadata_runtime_compare_semver(actualModuleVersion, refRecord->maxModuleVersionExclusive) < 0
           ? ZR_TRUE
           : ZR_FALSE;
}

/**
 * @brief 用本次判定的输入快照清空并填充可选诊断报告。
 * @note 报告里的版本字符串指针来自调用者；binding/refRecord 字段按值复制。
 */
static void metadata_runtime_fill_binding_report(
        SZrMetadataRuntimeBindingCompatibilityReport *report,
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion,
        EZrMetadataRuntimeBindingCompatibilityStatus status) {
    if (report == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(report, 0, sizeof(*report));
    report->status = status;
    report->actualModuleVersion = actualModuleVersion;
    if (refRecord != ZR_NULL) {
        report->expectedMinVersionInclusive = refRecord->minModuleVersionInclusive;
        report->expectedMaxVersionExclusive = refRecord->maxModuleVersionExclusive;
    }
    if (binding == ZR_NULL) {
        return;
    }

    report->expectedMetadataToken = binding->expectedMetadataToken;
    report->actualMetadataToken = binding->resolvedMetadataToken;
    report->expectedSignatureToken = binding->expectedSignatureToken;
    report->actualSignatureToken = binding->resolvedSignatureToken;
    report->expectedSignatureHash = binding->expectedSignatureHash;
    report->actualSignatureHash = binding->resolvedSignatureHash;
    report->expectedModuleSignatureHash = binding->expectedModuleSignatureHash;
    report->actualModuleSignatureHash = binding->resolvedModuleSignatureHash;
    report->expectedLayoutVersion = binding->expectedLayoutVersion;
    report->actualLayoutVersion = binding->resolvedLayoutVersion;
    report->expectedLayoutHash = binding->expectedLayoutHash;
    report->actualLayoutHash = binding->resolvedLayoutHash;
}

/** @brief 在通用绑定报告上覆盖 expected token，使其指向清单声明的导出身份。 */
static void metadata_runtime_fill_manifest_export_report(
        SZrMetadataRuntimeBindingCompatibilityReport *report,
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion,
        TZrMetadataToken expectedExportToken,
        EZrMetadataRuntimeBindingCompatibilityStatus status) {
    metadata_runtime_fill_binding_report(report, binding, refRecord, actualModuleVersion, status);
    if (report != ZR_NULL) {
        report->expectedMetadataToken = expectedExportToken;
        report->actualMetadataToken = binding != ZR_NULL ? binding->resolvedMetadataToken : 0u;
    }
}

/**
 * @brief 判断绑定是否声明了任一侧布局身份。
 * @note 全零代表旧格式未携带布局约束；只要一侧非零，判定器就要求版本和哈希两项均匹配。
 */
static TZrBool metadata_runtime_layout_identity_is_present(const SZrMetadataTokenBinding *binding) {
    return binding != ZR_NULL &&
           (binding->expectedLayoutVersion != 0u ||
            binding->expectedLayoutHash != 0u ||
            binding->resolvedLayoutVersion != 0u ||
            binding->resolvedLayoutHash != 0u)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/** @brief 识别 AssemblyRef 经链接映射到提供方 Module token 的合法跨表身份。 */
static TZrBool metadata_runtime_binding_is_assembly_ref_to_module(const SZrMetadataTokenBinding *binding) {
    return binding != ZR_NULL &&
           ZR_METADATA_TOKEN_TABLE(binding->expectedMetadataToken) == ZR_METADATA_TABLE_ASSEMBLY_REF &&
           ZR_METADATA_TOKEN_TABLE(binding->resolvedMetadataToken) == ZR_METADATA_TABLE_MODULE
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/**
 * @brief 识别请求端和提供端各自 TypeSpec/Signature token 的规范化映射。
 * @note 此路径仍比较签名哈希及可选布局身份，只跳过要求 token 数值相同的比较。
 */
static TZrBool metadata_runtime_binding_is_type_spec_mapping(const SZrMetadataTokenBinding *binding) {
    return binding != ZR_NULL &&
           ZR_METADATA_TOKEN_TABLE(binding->expectedMetadataToken) == ZR_METADATA_TABLE_TYPE_SPEC &&
           ZR_METADATA_TOKEN_TABLE(binding->resolvedMetadataToken) == ZR_METADATA_TABLE_TYPE_SPEC &&
           ZR_METADATA_TOKEN_TABLE(binding->expectedSignatureToken) == ZR_METADATA_TABLE_SIGNATURE &&
           ZR_METADATA_TOKEN_TABLE(binding->resolvedSignatureToken) == ZR_METADATA_TABLE_SIGNATURE
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/**
 * @brief 执行绑定字段的唯一权威比较，并按诊断优先级返回首个失败状态。
 * @note 版本范围、模块签名、token、成员签名和布局的顺序是调用方可观察的诊断契约；
 *       AssemblyRef/Module 与规范 TypeSpec 映射仅免除 token 数值相等检查。
 */
static EZrMetadataRuntimeBindingCompatibilityStatus metadata_runtime_check_binding_status(
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion) {
    TZrBool isAssemblyRefToModule;
    TZrBool isTypeSpecMapping;

    if (binding == ZR_NULL) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT;
    }

    isAssemblyRefToModule = metadata_runtime_binding_is_assembly_ref_to_module(binding);
    isTypeSpecMapping = metadata_runtime_binding_is_type_spec_mapping(binding);
    /* 先检引用声明的模块版本，确保后续身份差异不会遮蔽更早的部署兼容性失败。 */
    if (!metadata_runtime_version_range_matches(refRecord, actualModuleVersion)) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH;
    }
    if (binding->expectedModuleSignatureHash != 0u &&
        binding->expectedModuleSignatureHash != binding->resolvedModuleSignatureHash) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_SIGNATURE_HASH_MISMATCH;
    }
    /* 两类跨模块映射允许 token 改写；它们仍受后续 hash/layout 身份约束。 */
    if (!isAssemblyRefToModule && !isTypeSpecMapping &&
        binding->expectedMetadataToken != 0u &&
        binding->expectedMetadataToken != binding->resolvedMetadataToken) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_METADATA_TOKEN_MISMATCH;
    }
    if (!isAssemblyRefToModule && !isTypeSpecMapping &&
        binding->expectedSignatureToken != 0u &&
        binding->expectedSignatureToken != binding->resolvedSignatureToken) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_TOKEN_MISMATCH;
    }
    if (binding->expectedSignatureHash != 0u &&
        binding->expectedSignatureHash != binding->resolvedSignatureHash) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH;
    }
    if (metadata_runtime_layout_identity_is_present(binding)) {
        if (binding->expectedLayoutVersion != binding->resolvedLayoutVersion) {
            return ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_VERSION_MISMATCH;
        }
        if (binding->expectedLayoutHash != binding->resolvedLayoutHash) {
            return ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_HASH_MISMATCH;
        }
    }

    return ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE;
}

/**
 * @brief 比较一条引用绑定并可选生成诊断快照。
 * @return binding 为空时返回 INVALID_ARGUMENT；refRecord 或 actualModuleVersion 缺失时
 *         相应版本约束不参与判定。outReport 可为空，非空时会先清零再写入结果。
 *
 * 模块导入清单门禁和跨模块泛型 TypeSpec 解析都调用此入口，让 token 映射特例与签名/
 * 布局检查保持一致。调用期间 binding 及关联记录须稳定存活；报告中的版本字符串仍借用原对象。
 */
EZrMetadataRuntimeBindingCompatibilityStatus ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion,
        SZrMetadataRuntimeBindingCompatibilityReport *outReport) {
    EZrMetadataRuntimeBindingCompatibilityStatus status =
            metadata_runtime_check_binding_status(binding, refRecord, actualModuleVersion);

    metadata_runtime_fill_binding_report(outReport, binding, refRecord, actualModuleVersion, status);
    return status;
}

/**
 * @brief 从已读取的 typed export view 取出其唯一公开 metadata token。
 * @note Type 使用 typeToken，Method/Field 使用 memberToken；未知 kind 归零供上层失败处理。
 */
static TZrMetadataToken metadata_runtime_manifest_export_view_token(
        const SZrMetadataRuntimeManifestExportView *view) {
    if (view == ZR_NULL) {
        return 0u;
    }

    switch (view->kind) {
        case ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE:
            return view->typeToken;

        case ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD:
        case ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD:
            return view->memberToken;

        default:
            return 0u;
    }
}

/**
 * @brief 在通用身份匹配后，额外要求解析 token 指向清单中指定的 typed export。
 * @return runtime/exportTarget 无有效导出视图时报告导出缺失；随后依次可能返回通用
 *         兼容失败、导出 token 不匹配或兼容。两个输出参数均可省略。
 *
 * 模块导入只在提供方确有 manifest 时走此加强门禁；返回的 view 借自 runtime 注册数据。
 * @note 调用方须保证 runtime 注册表在消费 outExportView 期间仍然存活且不被替换。
 */
EZrMetadataRuntimeBindingCompatibilityStatus
ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
        SZrMetadataRuntime *runtime,
        TZrUInt32 exportKind,
        const TZrChar *exportTarget,
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion,
        SZrMetadataRuntimeManifestExportView *outExportView,
        SZrMetadataRuntimeBindingCompatibilityReport *outReport) {
    EZrMetadataRuntimeBindingCompatibilityStatus status;
    SZrMetadataRuntimeManifestExportView localExportView;
    TZrMetadataToken expectedExportToken;

    /* manifest 缺失/目标未导出时即终止；不能仅凭解析 token 视为公开 API。 */
    if (!ZrCore_MetadataRuntime_ReadManifestExportView(runtime,
                                                       exportKind,
                                                       exportTarget,
                                                       &localExportView)) {
        if (outExportView != ZR_NULL) {
            ZrCore_Memory_RawSet(outExportView, 0, sizeof(*outExportView));
        }
        metadata_runtime_fill_binding_report(outReport,
                                             binding,
                                             refRecord,
                                             actualModuleVersion,
                                             ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_NOT_FOUND);
        return ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_NOT_FOUND;
    }

    if (outExportView != ZR_NULL) {
        *outExportView = localExportView;
    }

    status = ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(binding,
                                                                  refRecord,
                                                                  actualModuleVersion,
                                                                  outReport);
    if (status != ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE) {
        return status;
    }

    expectedExportToken = metadata_runtime_manifest_export_view_token(&localExportView);
    if (binding == ZR_NULL || binding->resolvedMetadataToken != expectedExportToken) {
        metadata_runtime_fill_manifest_export_report(
                outReport,
                binding,
                refRecord,
                actualModuleVersion,
                expectedExportToken,
                ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_TOKEN_MISMATCH);
        return ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_TOKEN_MISMATCH;
    }

    return ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE;
}

/**
 * @brief 从函数的主 metadata token 表及导入副本中定位 binding.refToken 的请求记录。
 * @note 返回值借用 function 内存；主表优先，以保持既有记录查找语义。
 * BUG: IO 可装入零或孤儿 refToken；返回空后版本范围门禁放行，AOT 可能把坏绑定报为兼容。
 */
static const SZrMetadataTokenRecord *metadata_runtime_find_binding_ref_record(
        const SZrFunction *function,
        const SZrMetadataTokenBinding *binding) {
    const SZrMetadataTokenRecord *record;

    if (function == ZR_NULL || binding == ZR_NULL || binding->refToken == 0u) {
        return ZR_NULL;
    }

    record = ZrCore_Function_FindMetadataTokenRecord(function, binding->refToken);
    if (record != ZR_NULL) {
        return record;
    }
    return ZrCore_Function_FindModuleMetadataTokenRecord(function, binding->refToken);
}

/**
 * @brief 顺序检查一个函数的全部模块绑定，并返回首个不兼容项供加载器定位。
 * @return function 为空或绑定长度非零但表指针为空时返回 INVALID_ARGUMENT；空绑定集合
 *         视为兼容。失败输出指向函数内借用项。
 *
 * AOT 加载器用它在发布模块前拒绝不兼容绑定，typed direct-call 快速路径也用同一门禁
 * 决定是否安全保留直连；数组顺序因此决定首个诊断对象。输出指针不得越过 function 生命周期。
 */
EZrMetadataRuntimeBindingCompatibilityStatus ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(
        const SZrFunction *function,
        SZrString *actualModuleVersion,
        const SZrMetadataTokenBinding **outBinding,
        const SZrMetadataTokenRecord **outRefRecord,
        SZrMetadataRuntimeBindingCompatibilityReport *outReport) {
    EZrMetadataRuntimeBindingCompatibilityStatus status;
    SZrMetadataRuntimeBindingCompatibilityReport localReport;

    if (outBinding != ZR_NULL) {
        *outBinding = ZR_NULL;
    }
    if (outRefRecord != ZR_NULL) {
        *outRefRecord = ZR_NULL;
    }
    if (function == ZR_NULL) {
        metadata_runtime_fill_binding_report(outReport,
                                             ZR_NULL,
                                             ZR_NULL,
                                             actualModuleVersion,
                                             ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT);
        return ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT;
    }
    /* 长度与存储不一致是损坏元数据，必须在遍历前拒绝，不能静默当作空绑定。 */
    if (function->moduleMetadataBindingLength > 0u && function->moduleMetadataBindings == ZR_NULL) {
        metadata_runtime_fill_binding_report(outReport,
                                             ZR_NULL,
                                             ZR_NULL,
                                             actualModuleVersion,
                                             ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT);
        return ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT;
    }

    /* 保留序列顺序并在第一处失败停止，让加载诊断指向稳定的首个问题绑定。 */
    for (TZrUInt32 index = 0u; index < function->moduleMetadataBindingLength; ++index) {
        const SZrMetadataTokenBinding *binding;
        const SZrMetadataTokenRecord *refRecord;

        binding = &function->moduleMetadataBindings[index];
        refRecord = metadata_runtime_find_binding_ref_record(function, binding);
        status = ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(binding,
                                                                       refRecord,
                                                                       actualModuleVersion,
                                                                       &localReport);
        if (status != ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE) {
            if (outBinding != ZR_NULL) {
                *outBinding = binding;
            }
            if (outRefRecord != ZR_NULL) {
                *outRefRecord = refRecord;
            }
            if (outReport != ZR_NULL) {
                *outReport = localReport;
            }
            return status;
        }
    }

    metadata_runtime_fill_binding_report(outReport,
                                         ZR_NULL,
                                         ZR_NULL,
                                         actualModuleVersion,
                                         ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE);
    return ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE;
}
