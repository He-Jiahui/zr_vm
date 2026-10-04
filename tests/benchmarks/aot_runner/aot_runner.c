/* 仅执行显式注册的 AOT 条目，并将解释器回退标成独立结果状态。 */
#include "aot_runner.h"

#include <string.h>

/** @brief 为注册和结果结构检查排除 UNKNOWN/COUNT 及越界枚举；合法枚举不意味着已注册可执行入口。 */
static TZrBool zr_aot_runner_backend_is_valid(EZrAotBackend backend) {
    return (TZrBool)(backend > ZR_AOT_BACKEND_UNKNOWN &&
                     backend < ZR_AOT_BACKEND_COUNT);
}

/** @brief 仅 C/LLVM 请求允许查找整入口解释器回退；也用于识别编译入口的解释器站点。 */
static TZrBool zr_aot_runner_requested_backend_is_aot(EZrAotBackend backend) {
    return (TZrBool)(backend == ZR_AOT_BACKEND_C || backend == ZR_AOT_BACKEND_LLVM);
}

/** @brief 在固定容量内要求非空 NUL 结尾的无空白可打印 ASCII 名称，供 registry 复制及结果形状检查共用。失败不发布长度。 */
static TZrBool zr_aot_runner_name_is_valid(const TZrChar *name, size_t *lengthOut) {
    size_t length = 0u;

    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    while (length < (size_t)ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY && name[length] != '\0') {
        const unsigned char value = (unsigned char)name[length];
        if (value < 0x21u || value > 0x7eu) {
            return ZR_FALSE;
        }
        ++length;
    }
    if (length == 0u || length >= (size_t)ZR_AOT_RUNNER_ENTRY_NAME_CAPACITY) {
        return ZR_FALSE;
    }
    if (lengthOut != ZR_NULL) {
        *lengthOut = length;
    }
    return ZR_TRUE;
}

/** @brief 按 backend/token 二元键借用 registry 中的首个入口；不得越过 runner 存储生命周期，entryCount 必须来自合法初始化与注册。 */
static const SZrAotRunnerEntry *zr_aot_runner_find(const SZrAotRunner *runner,
                                                   EZrAotBackend backend,
                                                   TZrUInt64 entryToken) {
    TZrUInt32 index;

    if (runner == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < runner->entryCount; ++index) {
        const SZrAotRunnerEntry *entry = &runner->entries[index];
        if (entry->backend == backend && entry->entryToken == entryToken) {
            return entry;
        }
    }
    return ZR_NULL;
}

/** @brief 先建立失败/未调用和 unavailable 比率哨兵，再复制请求身份；不能把尚未执行的数据表示成成功测量。 */
static void zr_aot_runner_result_init(SZrAotRunnerResult *result,
                                      const SZrAotRunnerRequest *request) {
    if (result == ZR_NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->status = ZR_AOT_RUNNER_STATUS_INVALID;
    result->failure = ZR_AOT_RUNNER_FAILURE_NONE;
    result->actualBackend = ZR_AOT_BACKEND_UNKNOWN;
    result->processExitCode = -1;
    result->coverage.status = ZR_AOT_COVERAGE_STATUS_UNAVAILABLE;
    result->coverage.nativeCoverage = -1.0;
    result->coverage.nativeExecutionCoverage = -1.0;
    result->coverage.nativeHelperShare = -1.0;
    result->coverage.interpreterShare = -1.0;
    result->coverage.fallbackTimeShare = -1.0;
    if (request != ZR_NULL) {
        result->requestedBackend = request->requestedBackend;
        result->entryToken = request->entryToken;
    }
}

/** @brief 只更新状态与原因，保留已知实际 backend、调用标志和校验值，供调用方区分入口缺失与执行失败。 */
static void zr_aot_runner_result_fail(SZrAotRunnerResult *result,
                                      EZrAotRunnerStatus status,
                                      EZrAotRunnerFailure failure) {
    if (result != ZR_NULL) {
        result->status = status;
        result->failure = failure;
    }
}

/** @brief 重置固定容量 registry，不申请资源；清除注册不会释放回调 context，context 所有权仍归调用者。 */
void ZrTests_AotRunner_Init(SZrAotRunner *runner) {
    if (runner != ZR_NULL) {
        memset(runner, 0, sizeof(*runner));
        runner->initialized = ZR_TRUE;
    }
}

/** @brief 注册唯一 backend/token 回调并复制名称，借用 invoke/context；拒绝无效输入、满表和重复键，context 须覆盖后续同步调用。 */
TZrBool ZrTests_AotRunner_Register(SZrAotRunner *runner,
                                   EZrAotBackend backend,
                                   TZrUInt64 entryToken,
                                   const TZrChar *name,
                                   FZrAotRunnerEntry invoke,
                                   TZrPtr context) {
    SZrAotRunnerEntry *entry;
    size_t nameLength;

    if (runner == ZR_NULL || runner->initialized == ZR_FALSE ||
        !zr_aot_runner_backend_is_valid(backend) || entryToken == 0u || invoke == ZR_NULL ||
        !zr_aot_runner_name_is_valid(name, &nameLength)) {
        return ZR_FALSE;
    }
    if (runner->entryCount >= ZR_AOT_RUNNER_MAX_ENTRIES ||
        zr_aot_runner_find(runner, backend, entryToken) != ZR_NULL) {
        return ZR_FALSE;
    }
    entry = &runner->entries[runner->entryCount++];
    memset(entry, 0, sizeof(*entry));
    entry->backend = backend;
    entry->entryToken = entryToken;
    memcpy(entry->name, name, nameLength);
    entry->name[nameLength] = '\0';
    entry->invoke = invoke;
    entry->context = context;
    return ZR_TRUE;
}

/** @brief 查找显式注册入口并同步调用；仅缺少 C/LLVM 入口且请求允许时尝试解释器。调用失败不再回退，coverage/checksum 失败保留实际入口身份。processExitCode 是回调成功映射，不是此处创建的 OS 子进程状态。 */
TZrBool ZrTests_AotRunner_Run(const SZrAotRunner *runner,
                              const SZrAotRunnerRequest *request,
                              SZrAotRunnerResult *result) {
    const SZrAotRunnerEntry *entry;
    SZrAotCoverageCounts counts;
    TZrUInt64 checksum = 0u;
    TZrBool fallback = ZR_FALSE;

    zr_aot_runner_result_init(result, request);
    if (result == ZR_NULL || runner == ZR_NULL || request == ZR_NULL ||
        runner->initialized == ZR_FALSE || request->entryToken == 0u) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                                  ZR_AOT_RUNNER_FAILURE_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    if (request->requestedBackend <= ZR_AOT_BACKEND_UNKNOWN ||
        request->requestedBackend >= ZR_AOT_BACKEND_COUNT) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                                  ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND);
        return ZR_FALSE;
    }

/* 仅入口缺失时可选择显式允许的整入口回退；已找到入口的调用失败不会触发此路径。 */
    entry = zr_aot_runner_find(runner, request->requestedBackend, request->entryToken);
    if (entry == ZR_NULL && request->allowInterpreterFallback != ZR_FALSE &&
        zr_aot_runner_requested_backend_is_aot(request->requestedBackend)) {
        entry = zr_aot_runner_find(runner,
                                   ZR_AOT_BACKEND_INTERPRETER,
                                   request->entryToken);
        fallback = (TZrBool)(entry != ZR_NULL);
    }
    if (entry == ZR_NULL) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_UNAVAILABLE,
                                  ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE);
        return ZR_FALSE;
    }

/* 先记录实际条目身份，再调用借用 context；后续失败仍需保留已执行入口信息。 */
    result->actualBackend = entry->backend;
    result->entryInvoked = ZR_TRUE;
    memcpy(result->entryName, entry->name, sizeof(result->entryName));
    ZrTests_AotCoverage_Init(&counts);
    if (!entry->invoke(entry->context, &checksum, &counts)) {
        result->processExitCode = 1;
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FAILED,
                                  ZR_AOT_RUNNER_FAILURE_INVOCATION);
        return ZR_FALSE;
    }
    result->processExitCode = 0;
    result->checksum = checksum;
    if (!ZrTests_AotCoverage_Validate(&counts) ||
        !ZrTests_AotCoverage_Compute(&counts, &result->coverage)) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FAILED,
                                  ZR_AOT_RUNNER_FAILURE_MALFORMED_COVERAGE);
        return ZR_FALSE;
    }
    /* A compiled entry can still execute interpreter sites.  Only sampled,
     * available coverage is strong enough to mark that fallback; preserve the
     * compiled actual backend so consumers can distinguish mixed execution
     * from a whole-entry interpreter fallback. */
/* 仅可用采样证据能将编译入口标为 mixed；actualBackend 仍保留编译 backend。 */
    if (fallback == ZR_FALSE &&
        zr_aot_runner_requested_backend_is_aot(request->requestedBackend) &&
        result->actualBackend == request->requestedBackend &&
        result->coverage.available != ZR_FALSE &&
        result->coverage.interpreterSites != 0u) {
        result->coverage.mixedExecution = ZR_TRUE;
        fallback = ZR_TRUE;
    }
    if (request->checksumRequired != ZR_FALSE && checksum != request->expectedChecksum) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FAILED,
                                  ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH);
        return ZR_FALSE;
    }
    if (fallback != ZR_FALSE) {
        zr_aot_runner_result_fail(result,
                                  ZR_AOT_RUNNER_STATUS_FALLBACK,
                                  ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK);
    } else {
        result->status = ZR_AOT_RUNNER_STATUS_RAN;
        result->failure = ZR_AOT_RUNNER_FAILURE_NONE;
    }
    return ZR_TRUE;
}

/** @brief 检查结果状态、身份及 coverage 形状，不重算三类和或比率；失败 Run 的非法原始请求不保证能通过该 validator。 */
TZrBool ZrTests_AotRunner_ValidateResult(const SZrAotRunnerResult *result) {
    TZrBool coverageShapeValid;
    size_t nameLength;

    if (result == ZR_NULL || !zr_aot_runner_backend_is_valid(result->requestedBackend) ||
        result->actualBackend < ZR_AOT_BACKEND_UNKNOWN ||
        result->actualBackend >= ZR_AOT_BACKEND_COUNT ||
        result->entryToken == 0u || result->status <= ZR_AOT_RUNNER_STATUS_INVALID ||
        result->status >= ZR_AOT_RUNNER_STATUS_COUNT ||
        result->failure < ZR_AOT_RUNNER_FAILURE_NONE ||
        result->failure >= ZR_AOT_RUNNER_FAILURE_COUNT ||
        result->coverage.status < ZR_AOT_COVERAGE_STATUS_INVALID ||
        result->coverage.status >= ZR_AOT_COVERAGE_STATUS_COUNT) {
        return ZR_FALSE;
    }
/* 这里只核对 coverage 状态及比率范围；producer 计数一致性由 Run 中的 coverage validator 检查。 */
    coverageShapeValid = (TZrBool)(
            (result->coverage.available == ZR_FALSE &&
             result->coverage.status == ZR_AOT_COVERAGE_STATUS_UNAVAILABLE &&
             result->coverage.nativeCoverage < 0.0 &&
             result->coverage.nativeExecutionCoverage < 0.0) ||
            (result->coverage.available != ZR_FALSE &&
             result->coverage.status == ZR_AOT_COVERAGE_STATUS_AVAILABLE &&
             result->coverage.semanticSites != 0u &&
             result->coverage.sampleRatePermille != 0u &&
             result->coverage.sampleRatePermille <=
                     ZR_AOT_COVERAGE_MAX_SAMPLE_PERMILLE &&
             result->coverage.nativeCoverage >= 0.0 &&
             result->coverage.nativeCoverage <= 1.0 &&
             result->coverage.nativeExecutionCoverage >= 0.0 &&
             result->coverage.nativeExecutionCoverage <= 1.0 &&
             result->coverage.nativeHelperShare >= 0.0 &&
             result->coverage.nativeHelperShare <= 1.0 &&
             result->coverage.interpreterShare >= 0.0 &&
             result->coverage.interpreterShare <= 1.0));
    if (coverageShapeValid == ZR_FALSE) {
        return ZR_FALSE;
    }
    switch (result->status) {
        case ZR_AOT_RUNNER_STATUS_RAN:
            return (TZrBool)(result->failure == ZR_AOT_RUNNER_FAILURE_NONE &&
                             result->actualBackend == result->requestedBackend &&
                             result->entryInvoked != ZR_FALSE &&
                             result->processExitCode == 0 &&
                             zr_aot_runner_name_is_valid(result->entryName, &nameLength));
        case ZR_AOT_RUNNER_STATUS_FALLBACK:
            return (TZrBool)(result->failure == ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK &&
                             result->entryInvoked != ZR_FALSE &&
                             result->processExitCode == 0 &&
                             zr_aot_runner_name_is_valid(result->entryName, &nameLength) &&
                             ((result->actualBackend == ZR_AOT_BACKEND_INTERPRETER) ||
                              (zr_aot_runner_requested_backend_is_aot(result->requestedBackend) &&
                               result->actualBackend == result->requestedBackend &&
                               result->coverage.available != ZR_FALSE &&
                               result->coverage.interpreterSites != 0u &&
                               result->coverage.mixedExecution != ZR_FALSE)));
        case ZR_AOT_RUNNER_STATUS_FAILED:
            return (TZrBool)(result->failure != ZR_AOT_RUNNER_FAILURE_NONE &&
                             result->entryInvoked != ZR_FALSE &&
                             result->actualBackend != ZR_AOT_BACKEND_UNKNOWN &&
                             zr_aot_runner_name_is_valid(result->entryName, &nameLength));
        case ZR_AOT_RUNNER_STATUS_UNAVAILABLE:
            return (TZrBool)(result->actualBackend == ZR_AOT_BACKEND_UNKNOWN &&
                             result->entryInvoked == ZR_FALSE &&
                             result->failure != ZR_AOT_RUNNER_FAILURE_NONE);
        case ZR_AOT_RUNNER_STATUS_INVALID:
        case ZR_AOT_RUNNER_STATUS_COUNT:
        default:
            return ZR_FALSE;
    }
}

/** @brief 列出 C、LLVM、解释器注册数及目标 token 是否存在；仅描述 registry，不编译或执行条目。 */
TZrBool ZrTests_AotRunner_DescribeMatrix(const SZrAotRunner *runner,
                                         TZrUInt64 entryToken,
                                         SZrAotRunnerMatrix *matrix) {
    static const EZrAotBackend backends[] = {
            ZR_AOT_BACKEND_C,
            ZR_AOT_BACKEND_LLVM,
            ZR_AOT_BACKEND_INTERPRETER
    };
    TZrUInt32 index;
    TZrUInt32 entryIndex;

    if (runner == ZR_NULL || matrix == ZR_NULL || runner->initialized == ZR_FALSE ||
        entryToken == 0u) {
        return ZR_FALSE;
    }
    memset(matrix, 0, sizeof(*matrix));
    matrix->entryToken = entryToken;
    matrix->rowCount = (TZrUInt32)(sizeof(backends) / sizeof(backends[0]));
    for (index = 0u; index < matrix->rowCount; ++index) {
        matrix->rows[index].backend = backends[index];
        for (entryIndex = 0u; entryIndex < runner->entryCount; ++entryIndex) {
            if (runner->entries[entryIndex].backend == backends[index]) {
                matrix->rows[index].registeredEntries++;
                if (runner->entries[entryIndex].entryToken == entryToken) {
                    matrix->rows[index].available = ZR_TRUE;
                }
            }
        }
    }
    return ZR_TRUE;
}

/** @brief 保留计划命名的 DescribeMatrix 同义入口；不执行代码生成或启动 backend。 */
TZrBool ZrTests_AotRunner_BuildMatrix(const SZrAotRunner *runner,
                                      TZrUInt64 entryToken,
                                      SZrAotRunnerMatrix *matrix) {
    return ZrTests_AotRunner_DescribeMatrix(runner, entryToken, matrix);
}

/** @brief 为报告返回 backend 固定文本；未识别值显示 unknown，不表示支持检测。 */
const TZrChar *ZrTests_AotRunner_BackendName(EZrAotBackend backend) {
    switch (backend) {
        case ZR_AOT_BACKEND_C:
            return "aot_c";
        case ZR_AOT_BACKEND_LLVM:
            return "aot_llvm";
        case ZR_AOT_BACKEND_INTERPRETER:
            return "interpreter";
        case ZR_AOT_BACKEND_UNKNOWN:
        case ZR_AOT_BACKEND_COUNT:
        default:
            return "unknown";
    }
}

/** @brief 固定序列化状态名称；未知状态显示 INVALID，不改变执行结果。 */
const TZrChar *ZrTests_AotRunner_StatusName(EZrAotRunnerStatus status) {
    switch (status) {
        case ZR_AOT_RUNNER_STATUS_RAN:
            return "RAN";
        case ZR_AOT_RUNNER_STATUS_FALLBACK:
            return "FALLBACK";
        case ZR_AOT_RUNNER_STATUS_UNAVAILABLE:
            return "UNAVAILABLE";
        case ZR_AOT_RUNNER_STATUS_FAILED:
            return "FAILED";
        case ZR_AOT_RUNNER_STATUS_INVALID:
        case ZR_AOT_RUNNER_STATUS_COUNT:
        default:
            return "INVALID";
    }
}

/** @brief 将失败枚举投影为诊断文本；REGISTRY_FULL/DUPLICATE_ENTRY 名称存在不代表 Register 返回结构化失败原因。 */
const TZrChar *ZrTests_AotRunner_FailureName(EZrAotRunnerFailure failure) {
    switch (failure) {
        case ZR_AOT_RUNNER_FAILURE_NONE:
            return "NONE";
        case ZR_AOT_RUNNER_FAILURE_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case ZR_AOT_RUNNER_FAILURE_UNSUPPORTED_BACKEND:
            return "UNSUPPORTED_BACKEND";
        case ZR_AOT_RUNNER_FAILURE_ENTRY_UNAVAILABLE:
            return "ENTRY_UNAVAILABLE";
        case ZR_AOT_RUNNER_FAILURE_REGISTRY_FULL:
            return "REGISTRY_FULL";
        case ZR_AOT_RUNNER_FAILURE_DUPLICATE_ENTRY:
            return "DUPLICATE_ENTRY";
        case ZR_AOT_RUNNER_FAILURE_INVOCATION:
            return "INVOCATION_FAILED";
        case ZR_AOT_RUNNER_FAILURE_CHECKSUM_MISMATCH:
            return "CHECKSUM_MISMATCH";
        case ZR_AOT_RUNNER_FAILURE_MALFORMED_COVERAGE:
            return "MALFORMED_COVERAGE";
        case ZR_AOT_RUNNER_FAILURE_INTERPRETER_FALLBACK:
            return "INTERPRETER_FALLBACK";
        case ZR_AOT_RUNNER_FAILURE_COUNT:
        default:
            return "UNKNOWN";
    }
}
