#ifndef ZR_VM_RUST_BINDING_H
#define ZR_VM_RUST_BINDING_H

#include "zr_vm_rust_binding/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief C ABI 的返回分类；Rust 安全层据此读取最近错误快照，调用方不能只检查输出指针。 */
typedef enum EZrRustBindingStatus {
    ZR_RUST_BINDING_STATUS_OK = 0,
    ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT = 1,
    ZR_RUST_BINDING_STATUS_IO_ERROR = 2,
    ZR_RUST_BINDING_STATUS_NOT_FOUND = 3,
    ZR_RUST_BINDING_STATUS_ALREADY_EXISTS = 4,
    ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL = 5,
    ZR_RUST_BINDING_STATUS_COMPILE_ERROR = 6,
    ZR_RUST_BINDING_STATUS_RUNTIME_ERROR = 7,
    ZR_RUST_BINDING_STATUS_UNSUPPORTED = 8,
    ZR_RUST_BINDING_STATUS_INTERNAL_ERROR = 9,
    ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED = 10
} ZrRustBindingStatus;

typedef enum EZrRustBindingExecutionMode {
    ZR_RUST_BINDING_EXECUTION_MODE_INTERP = 0,
    ZR_RUST_BINDING_EXECUTION_MODE_BINARY = 1
} ZrRustBindingExecutionMode;

typedef enum EZrRustBindingValueKind {
    ZR_RUST_BINDING_VALUE_KIND_NULL = 0,
    ZR_RUST_BINDING_VALUE_KIND_BOOL = 1,
    ZR_RUST_BINDING_VALUE_KIND_INT = 2,
    ZR_RUST_BINDING_VALUE_KIND_FLOAT = 3,
    ZR_RUST_BINDING_VALUE_KIND_STRING = 4,
    ZR_RUST_BINDING_VALUE_KIND_ARRAY = 5,
    ZR_RUST_BINDING_VALUE_KIND_OBJECT = 6,
    ZR_RUST_BINDING_VALUE_KIND_FUNCTION = 7,
    ZR_RUST_BINDING_VALUE_KIND_NATIVE_POINTER = 8,
    ZR_RUST_BINDING_VALUE_KIND_UNKNOWN = 255
} ZrRustBindingValueKind;

typedef enum EZrRustBindingOwnershipKind {
    ZR_RUST_BINDING_OWNERSHIP_KIND_NONE = 0,
    ZR_RUST_BINDING_OWNERSHIP_KIND_UNIQUE = 1,
    ZR_RUST_BINDING_OWNERSHIP_KIND_SHARED = 2,
    ZR_RUST_BINDING_OWNERSHIP_KIND_WEAK = 3,
    ZR_RUST_BINDING_OWNERSHIP_KIND_BORROWED = 4,
    ZR_RUST_BINDING_OWNERSHIP_KIND_LOANED = 5
} ZrRustBindingOwnershipKind;

typedef struct ZrRustBindingErrorInfo {
    ZrRustBindingStatus status;
    TZrChar message[ZR_RUST_BINDING_ERROR_MESSAGE_CAPACITY];
} ZrRustBindingErrorInfo;

typedef struct ZrRustBindingRuntimeOptions {
    TZrUInt64 heapLimitBytes;
    TZrUInt64 pauseBudgetUs;
    TZrUInt64 remarkBudgetUs;
    TZrUInt32 workerCount;
} ZrRustBindingRuntimeOptions;

typedef struct ZrRustBindingScaffoldOptions {
    const TZrChar *rootPath;
    const TZrChar *projectName;
    TZrBool overwriteExisting;
} ZrRustBindingScaffoldOptions;

typedef struct ZrRustBindingCompileOptions {
    TZrBool emitIntermediate;
    TZrBool incremental;
} ZrRustBindingCompileOptions;

/** @brief 一次项目执行的借用参数；moduleName 和 programArgs 只需保持到调用返回。 */
typedef struct ZrRustBindingRunOptions {
    ZrRustBindingExecutionMode executionMode;
    const TZrChar *moduleName;
    const TZrChar *const *programArgs;
    TZrSize programArgCount;
} ZrRustBindingRunOptions;

typedef struct ZrRustBindingGcStepResult {
    TZrUInt64 pauseMicros;
    TZrUInt64 rootCount;
    TZrUInt64 crossBoundaryReferenceCount;
} ZrRustBindingGcStepResult;

typedef struct SZrExecutionCancelToken ZrRustBindingCancellationToken;

typedef enum EZrRustBindingTermination {
    ZR_RUST_BINDING_TERMINATION_NONE = 0,
    ZR_RUST_BINDING_TERMINATION_INSTRUCTION_LIMIT = 1,
    ZR_RUST_BINDING_TERMINATION_DEADLINE = 2,
    ZR_RUST_BINDING_TERMINATION_CANCELLED = 3,
    ZR_RUST_BINDING_TERMINATION_HEAP_LIMIT = 4,
    ZR_RUST_BINDING_TERMINATION_NATIVE_CALL_LIMIT = 5,
    ZR_RUST_BINDING_TERMINATION_GC_TIME_LIMIT = 6
} ZrRustBindingTermination;

/** @brief 仅约束 session 导出调用；限制在 VM 协作检查点生效，不回滚已发生的副作用。 */
typedef struct ZrRustBindingCallBudget {
    TZrUInt64 maxInstructions;
    TZrUInt64 deadlineMicros;
    const ZrRustBindingCancellationToken *cancelToken;
    TZrBool hasInstructionLimit;
    TZrBool hasDeadline;
    TZrUInt64 maxHeapBytes;
    TZrUInt64 maxNativeCalls;
    TZrUInt64 maxGcMicros;
    TZrBool hasHeapLimit;
    TZrBool hasNativeCallLimit;
    TZrBool hasGcTimeLimit;
} ZrRustBindingCallBudget;

typedef struct ZrRustBindingCallUsage {
    TZrUInt64 executedInstructions;
    TZrUInt64 elapsedMicros;
    ZrRustBindingTermination termination;
    TZrUInt64 peakHeapBytes;
    TZrUInt64 nativeCalls;
    TZrUInt64 gcMicros;
} ZrRustBindingCallUsage;

typedef enum EZrRustBindingNativeConstantKind {
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_NULL = 0,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_BOOL = 1,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT = 2,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_FLOAT = 3,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_STRING = 4,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_ARRAY = 5
} ZrRustBindingNativeConstantKind;

typedef enum EZrRustBindingPrototypeType {
    ZR_RUST_BINDING_PROTOTYPE_TYPE_INVALID = 0,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_MODULE = 1,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_CLASS = 2,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_INTERFACE = 3,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_STRUCT = 4,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_ENUM = 5,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_NATIVE = 6
} ZrRustBindingPrototypeType;

typedef enum EZrRustBindingMetaMethodType {
    ZR_RUST_BINDING_META_METHOD_CONSTRUCTOR = 0,
    ZR_RUST_BINDING_META_METHOD_DESTRUCTOR = 1,
    ZR_RUST_BINDING_META_METHOD_ADD = 2,
    ZR_RUST_BINDING_META_METHOD_SUB = 3,
    ZR_RUST_BINDING_META_METHOD_MUL = 4,
    ZR_RUST_BINDING_META_METHOD_DIV = 5,
    ZR_RUST_BINDING_META_METHOD_MOD = 6,
    ZR_RUST_BINDING_META_METHOD_POW = 7,
    ZR_RUST_BINDING_META_METHOD_NEG = 8,
    ZR_RUST_BINDING_META_METHOD_COMPARE = 9,
    ZR_RUST_BINDING_META_METHOD_TO_BOOL = 10,
    ZR_RUST_BINDING_META_METHOD_TO_STRING = 11,
    ZR_RUST_BINDING_META_METHOD_TO_INT = 12,
    ZR_RUST_BINDING_META_METHOD_TO_UINT = 13,
    ZR_RUST_BINDING_META_METHOD_TO_FLOAT = 14,
    ZR_RUST_BINDING_META_METHOD_CALL = 15,
    ZR_RUST_BINDING_META_METHOD_GETTER = 16,
    ZR_RUST_BINDING_META_METHOD_SETTER = 17,
    ZR_RUST_BINDING_META_METHOD_SHIFT_LEFT = 18,
    ZR_RUST_BINDING_META_METHOD_SHIFT_RIGHT = 19,
    ZR_RUST_BINDING_META_METHOD_BIT_AND = 20,
    ZR_RUST_BINDING_META_METHOD_BIT_OR = 21,
    ZR_RUST_BINDING_META_METHOD_BIT_XOR = 22,
    ZR_RUST_BINDING_META_METHOD_BIT_NOT = 23,
    ZR_RUST_BINDING_META_METHOD_GET_ITEM = 24,
    ZR_RUST_BINDING_META_METHOD_SET_ITEM = 25,
    ZR_RUST_BINDING_META_METHOD_CLOSE = 26,
    ZR_RUST_BINDING_META_METHOD_DECORATE = 27
} ZrRustBindingMetaMethodType;

typedef struct ZrRustBindingNativeTypeHintDescriptor {
    const TZrChar *symbolName;
    const TZrChar *symbolKind;
    const TZrChar *signature;
    const TZrChar *documentation;
} ZrRustBindingNativeTypeHintDescriptor;

typedef struct ZrRustBindingNativeFieldDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
    TZrUInt32 contractRole;
} ZrRustBindingNativeFieldDescriptor;

typedef struct ZrRustBindingNativeParameterDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
} ZrRustBindingNativeParameterDescriptor;

typedef struct ZrRustBindingNativeGenericParameterDescriptor {
    const TZrChar *name;
    const TZrChar *documentation;
    const TZrChar *const *constraintTypeNames;
    TZrSize constraintTypeCount;
} ZrRustBindingNativeGenericParameterDescriptor;

typedef struct ZrRustBindingNativeEnumMemberDescriptor {
    const TZrChar *name;
    ZrRustBindingNativeConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
} ZrRustBindingNativeEnumMemberDescriptor;

typedef struct ZrRustBindingNativeConstantDescriptor {
    const TZrChar *name;
    ZrRustBindingNativeConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
    const TZrChar *typeName;
} ZrRustBindingNativeConstantDescriptor;

typedef struct ZrRustBindingNativeModuleLinkDescriptor {
    const TZrChar *name;
    const TZrChar *moduleName;
    const TZrChar *documentation;
} ZrRustBindingNativeModuleLinkDescriptor;

typedef struct ZrRustBindingRuntime ZrRustBindingRuntime;
typedef struct ZrRustBindingProjectWorkspace ZrRustBindingProjectWorkspace;
typedef struct ZrRustBindingProjectSession ZrRustBindingProjectSession;
typedef struct ZrRustBindingProjectSessionCheckpoint ZrRustBindingProjectSessionCheckpoint;
typedef struct ZrRustBindingCompileResult ZrRustBindingCompileResult;
typedef struct ZrRustBindingManifestSnapshot ZrRustBindingManifestSnapshot;
typedef struct ZrRustBindingNativeCallContext ZrRustBindingNativeCallContext;
typedef struct ZrRustBindingNativeArgumentView ZrRustBindingNativeArgumentView;
typedef struct ZrRustBindingNativeModuleBuilder ZrRustBindingNativeModuleBuilder;
typedef struct ZrRustBindingNativeModule ZrRustBindingNativeModule;
typedef struct ZrRustBindingRuntimeNativeModuleRegistration ZrRustBindingRuntimeNativeModuleRegistration;
typedef struct ZrRustBindingValue ZrRustBindingValue;

typedef ZrRustBindingStatus (*FZrRustBindingNativeCallback)(ZrRustBindingNativeCallContext *context,
                                                            TZrPtr userData,
                                                            ZrRustBindingValue **outResult);
typedef ZrRustBindingStatus (*FZrRustBindingNativeArgumentVisitor)(
        const ZrRustBindingNativeArgumentView *argument,
        TZrPtr userData);
typedef ZrRustBindingStatus (*FZrRustBindingNativeStringVisitor)(
        const TZrChar *utf8,
        TZrSize utf8ByteLength,
        TZrPtr userData);
typedef void (*FZrRustBindingDestroyCallback)(TZrPtr userData);

/** @brief AddFunction 的临时输入；成功后 builder 模块复制文本和参数并接管 callback userData，失败时仍由调用方负责 userData。 */
typedef struct ZrRustBindingNativeFunctionDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrRustBindingNativeParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 contractRole;
} ZrRustBindingNativeFunctionDescriptor;

typedef struct ZrRustBindingNativeMethodDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    TZrBool isStatic;
    const ZrRustBindingNativeParameterDescriptor *parameters;
    TZrSize parameterCount;
    TZrUInt32 contractRole;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} ZrRustBindingNativeMethodDescriptor;

typedef struct ZrRustBindingNativeMetaMethodDescriptor {
    ZrRustBindingMetaMethodType metaType;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrRustBindingNativeParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} ZrRustBindingNativeMetaMethodDescriptor;

/** @brief 向 VM 注册类型的跨语言合同；AddType 成功后 builder 模块复制嵌套字段和方法，Build 再转出模块句柄。 */
typedef struct ZrRustBindingNativeTypeDescriptor {
    const TZrChar *name;
    ZrRustBindingPrototypeType prototypeType;
    const ZrRustBindingNativeFieldDescriptor *fields;
    TZrSize fieldCount;
    const ZrRustBindingNativeMethodDescriptor *methods;
    TZrSize methodCount;
    const ZrRustBindingNativeMetaMethodDescriptor *metaMethods;
    TZrSize metaMethodCount;
    const TZrChar *documentation;
    const TZrChar *extendsTypeName;
    const TZrChar *const *implementsTypeNames;
    TZrSize implementsTypeCount;
    const ZrRustBindingNativeEnumMemberDescriptor *enumMembers;
    TZrSize enumMemberCount;
    const TZrChar *enumValueTypeName;
    TZrBool allowValueConstruction;
    TZrBool allowBoxedConstruction;
    const TZrChar *constructorSignature;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt64 protocolMask;
    const TZrChar *ffiLoweringKind;
    const TZrChar *ffiViewTypeName;
    const TZrChar *ffiUnderlyingTypeName;
    const TZrChar *ffiOwnerMode;
    const TZrChar *ffiReleaseHook;
} ZrRustBindingNativeTypeDescriptor;

/** @brief 复制最近一次 C ABI 错误信息；应紧跟失败调用读取。
 * BUG: api.c 使用进程级 g_zr_rust_binding_last_error，两个线程交错调用会互相覆盖错误快照。 */
ZR_RUST_BINDING_API void ZrRustBinding_GetLastErrorInfo(ZrRustBindingErrorInfo *outErrorInfo);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_NewBare(const ZrRustBindingRuntimeOptions *options,
                                                                      ZrRustBindingRuntime **outRuntime);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_NewStandard(const ZrRustBindingRuntimeOptions *options,
                                                                          ZrRustBindingRuntime **outRuntime);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_Free(ZrRustBindingRuntime *runtime);

/** @brief 按项目模板创建 .zrp 和入口源码，再打开 workspace；已有文件由 overwriteExisting 决定是否覆盖。
 * BUG: api.c 对路径和 JSON 使用固定缓冲区，却只检查 snprintf 非负；长路径可被截断，项目名中的引号未转义。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Scaffold(
        const ZrRustBindingScaffoldOptions *options,
        ZrRustBindingProjectWorkspace **outWorkspace);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Open(const TZrChar *projectPath,
                                                                   ZrRustBindingProjectWorkspace **outWorkspace);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_Free(ZrRustBindingProjectWorkspace *workspace);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetProjectPath(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetProjectRoot(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetManifestPath(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetEntryModule(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_ResolveArtifacts(
        const ZrRustBindingProjectWorkspace *workspace,
        const TZrChar *moduleName,
        TZrChar *zroBuffer,
        TZrSize zroBufferSize,
        TZrChar *zriBuffer,
        TZrSize zriBufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_LoadManifest(
        const ZrRustBindingProjectWorkspace *workspace,
        ZrRustBindingManifestSnapshot **outManifestSnapshot);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetVersion(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrUInt32 *outVersion);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryCount(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize *outEntryCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_FindEntry(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        const TZrChar *moduleName,
        TZrSize *outEntryIndex);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryModuleName(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntrySourceHash(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZroHash(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZroPath(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZriPath(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryImportCount(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrSize *outImportCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryImport(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrSize importIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_Free(
        ZrRustBindingManifestSnapshot *manifestSnapshot);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Compile(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingCompileOptions *options,
        ZrRustBindingCompileResult **outCompileResult);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_CompileResult_GetCounts(
        const ZrRustBindingCompileResult *compileResult,
        TZrSize *outCompiledCount,
        TZrSize *outSkippedCount,
        TZrSize *outRemovedCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_CompileResult_Free(ZrRustBindingCompileResult *compileResult);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Run(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingRunOptions *options,
        ZrRustBindingValue **outResult);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_CallModuleExport(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingRunOptions *options,
        const TZrChar *moduleName,
        const TZrChar *exportName,
        ZrRustBindingValue *const *arguments,
        TZrSize argumentCount,
        ZrRustBindingValue **outResult);
/** @brief 执行项目入口并保留其 VM global，供后续导出调用和增量 checkpoint 共用。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_Start(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingRunOptions *options,
        ZrRustBindingProjectSession **outSession);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_CallModuleExport(
        ZrRustBindingProjectSession *session,
        const TZrChar *moduleName,
        const TZrChar *exportName,
        ZrRustBindingValue *const *arguments,
        TZrSize argumentCount,
        ZrRustBindingValue **outResult);
/* The deadline uses ExecutionNowMicros' monotonic clock. An enabled instruction
 * limit of zero admits no bytecode instructions; native boundaries do not count.
 * Checks run before bytecode fetch and before/after native callbacks, which must
 * return cooperatively. Termination preserves globals and heap mutations; it is
 * not rollback. Session start/compilation and argument copying are not preempted.
 * Usage is filled even on failure; a terminated call returns no result value.
 * Heap usage is peak live requested bytes from the global VM allocator during
 * this call, including preexisting allocations and allocate/free transients.
 * It excludes allocations outside that allocator. The heap limit is checked
 * after allocation at the next cooperative boundary, not before allocation;
 * a single allocation or running native callback may overshoot without bound.
 * GC usage is cumulative synchronous collection/step time including safepoint
 * waits; the active collection finishes before the time limit is enforced.
 * Native call limits admit function/binding boundaries before the callback.
 * The token must stay alive until the call returns. A session is single-threaded. */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
        ZrRustBindingProjectSession *session,
        const TZrChar *moduleName,
        const TZrChar *exportName,
        ZrRustBindingValue *const *arguments,
        TZrSize argumentCount,
        const ZrRustBindingCallBudget *budget,
        ZrRustBindingCallUsage *outUsage,
        ZrRustBindingValue **outResult);
ZR_RUST_BINDING_API TZrUInt64 ZrRustBinding_ExecutionNowMicros(void);
/* Token operations do not access the binding's process-global error snapshot.
 * Cancel and IsCancelled are atomic and may run on other threads. Free requires
 * all calls and token readers/writers to have finished. Cancellation is one-shot. */
ZR_RUST_BINDING_API ZrRustBindingCancellationToken *ZrRustBinding_CancellationToken_New(void);
ZR_RUST_BINDING_API void ZrRustBinding_CancellationToken_Cancel(ZrRustBindingCancellationToken *token);
ZR_RUST_BINDING_API TZrBool ZrRustBinding_CancellationToken_IsCancelled(
        const ZrRustBindingCancellationToken *token);
ZR_RUST_BINDING_API void ZrRustBinding_CancellationToken_Free(ZrRustBindingCancellationToken *token);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_GcStep(
        ZrRustBindingProjectSession *session,
        TZrUInt64 maxPauseMicros,
        ZrRustBindingGcStepResult *outResult);
/** @brief 捕获 session 状态供回滚；有跨边界 live Value root 时会拒绝创建。checkpoint 持有执行 owner，但回滚仍需要 session。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_Checkpoint(
        ZrRustBindingProjectSession *session,
        ZrRustBindingProjectSessionCheckpoint **outCheckpoint);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_Rollback(
        ZrRustBindingProjectSession *session,
        const ZrRustBindingProjectSessionCheckpoint *checkpoint);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSessionCheckpoint_Free(
        ZrRustBindingProjectSessionCheckpoint *checkpoint);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectSession_Free(
        ZrRustBindingProjectSession *session);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_New(
        const TZrChar *moduleName,
        ZrRustBindingNativeModuleBuilder **outBuilder);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetDocumentation(
        ZrRustBindingNativeModuleBuilder *builder,
        const TZrChar *documentation);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetModuleVersion(
        ZrRustBindingNativeModuleBuilder *builder,
        const TZrChar *moduleVersion);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetTypeHintsJson(
        ZrRustBindingNativeModuleBuilder *builder,
        const TZrChar *typeHintsJson);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetRuntimeRequirements(
        ZrRustBindingNativeModuleBuilder *builder,
        TZrUInt32 minRuntimeAbi,
        TZrUInt64 requiredCapabilities);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddTypeHint(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeTypeHintDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddModuleLink(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeModuleLinkDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddConstant(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeConstantDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddFunction(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeFunctionDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddType(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeTypeDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_Build(
        ZrRustBindingNativeModuleBuilder *builder,
        ZrRustBindingNativeModule **outModule);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_Free(
        ZrRustBindingNativeModuleBuilder *builder);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModule_Free(
        ZrRustBindingNativeModule *module);
/** @brief 把已 Build 的模块挂到 runtime；返回注册句柄负责撤销可见性，存活结果仍持有旧 descriptor。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_RegisterNativeModule(
        ZrRustBindingRuntime *runtime,
        ZrRustBindingNativeModule *module,
        ZrRustBindingRuntimeNativeModuleRegistration **outRegistration);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_RuntimeNativeModuleRegistration_Free(
        ZrRustBindingRuntimeNativeModuleRegistration *registration);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetModuleName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetTypeName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetCallableName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetArgumentCount(
        const ZrRustBindingNativeCallContext *context,
        TZrSize *outArgumentCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_CheckArity(
        const ZrRustBindingNativeCallContext *context,
        TZrSize minArgumentCount,
        TZrSize maxArgumentCount);
/** @brief 在回调栈内借用一个参数；visitor 不得保存 view 或其字符串指针到调用结束后。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_WithArgument(
        const ZrRustBindingNativeCallContext *context,
        TZrSize index,
        FZrRustBindingNativeArgumentVisitor visitor,
        TZrPtr userData);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_GetKind(
        const ZrRustBindingNativeArgumentView *argument,
        ZrRustBindingValueKind *outKind);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ReadBool(
        const ZrRustBindingNativeArgumentView *argument,
        TZrBool *outBoolValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ReadInt(
        const ZrRustBindingNativeArgumentView *argument,
        TZrInt64 *outIntValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ReadFloat(
        const ZrRustBindingNativeArgumentView *argument,
        TZrFloat64 *outFloatValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ByteArrayLength(
        const ZrRustBindingNativeArgumentView *argument,
        TZrSize *outLength);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ByteArrayGet(
        const ZrRustBindingNativeArgumentView *argument,
        TZrSize index,
        TZrUInt8 *outByteValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_WithString(
        const ZrRustBindingNativeArgumentView *argument,
        FZrRustBindingNativeStringVisitor visitor,
        TZrPtr userData);
/** @brief 在 native 回调中把 self 转为 Value 句柄；调用方负责 Value_Free。
 * BUG: 当前句柄的 owner 不持有 VM global，若句柄逃逸到 session 释放之后，读取或释放会访问失效 global。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetSelf(
        const ZrRustBindingNativeCallContext *context,
        ZrRustBindingValue **outSelfValue);

/** @brief 创建可独立持有的 host 值；New* 和容器读取返回的新句柄均须由 Value_Free 释放。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewNull(ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewBool(TZrBool boolValue, ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewInt(TZrInt64 intValue, ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewFloat(TZrFloat64 floatValue,
                                                                     ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewString(const TZrChar *stringValue,
                                                                      ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewArray(ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewObject(ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Free(ZrRustBindingValue *value);

ZR_RUST_BINDING_API ZrRustBindingValueKind ZrRustBinding_Value_GetKind(const ZrRustBindingValue *value);
ZR_RUST_BINDING_API ZrRustBindingOwnershipKind ZrRustBinding_Value_GetOwnershipKind(
        const ZrRustBindingValue *value);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadBool(const ZrRustBindingValue *value,
                                                                     TZrBool *outBoolValue);
/** @brief 读取宿主或 VM 整数到 int64。
 * BUG: VM uint64 大于 INT64_MAX 时 value.c 未拒绝窄化，返回成功但值失真。 */
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadInt(const ZrRustBindingValue *value,
                                                                    TZrInt64 *outIntValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadFloat(const ZrRustBindingValue *value,
                                                                      TZrFloat64 *outFloatValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadString(const ZrRustBindingValue *value,
                                                                       TZrChar *buffer,
                                                                       TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Array_Length(const ZrRustBindingValue *value,
                                                                         TZrSize *outLength);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Array_Get(const ZrRustBindingValue *value,
                                                                      TZrSize index,
                                                                      ZrRustBindingValue **outElement);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Array_Push(ZrRustBindingValue *value,
                                                                       const ZrRustBindingValue *element);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Object_Get(const ZrRustBindingValue *value,
                                                                       const TZrChar *fieldName,
                                                                       ZrRustBindingValue **outFieldValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Object_Set(ZrRustBindingValue *value,
                                                                       const TZrChar *fieldName,
                                                                       const ZrRustBindingValue *fieldValue);

#ifdef __cplusplus
}
#endif

#endif
