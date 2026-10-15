/**
 * @file fs_internal.h
 * ## Summary
 * 声明 system.fs 的对象字段、native callback 与宿主句柄共享层。它把 `fs_entry.c` 的一次性文件操作和 `fs_stream.c` 的持久流操作接到同一组 VM 对象/字段约定。
 * ## Constraints and lifecycle
 * 这些接口只供同一 VM state 的 native 执行路径使用；VM 分配可能触发 GC，调用者必须按各声明保持对象/值有效。字段 setter 为 void，不能据此确认写入成功。
 * ## References
 * `fs_common.c` 实现对象快照、实参读取和 IOException 路径；`fs_entry.c`、`fs_stream.c` 是直接消费者。
 */
#ifndef ZR_VM_LIB_SYSTEM_FS_INTERNAL_H
#define ZR_VM_LIB_SYSTEM_FS_INTERNAL_H

#include "zr_vm_lib_system/fs.h"

#include "zr_vm_core/raw_object.h"
#include "zr_vm_library/file.h"

/** `FileStream` 中保存 native owner 的私有字段名；值是本模块 malloc 的 `ZrSystemFsStreamData *`。 */
#define ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD "__zr_fs_stream"
/** FFI lowering 使用的整数句柄字段名；它只是视图，不拥有宿主句柄。 */
#define ZR_SYSTEM_FS_HIDDEN_HANDLE_ID_FIELD "__zr_ffi_handleId"

/**
 * ## Summary
 * 保存一个 `FileStream` 包装对象独占的宿主句柄和打开选项；只由 FileStream native 实现访问。
 * ## Fields
 * handle 是当前宿主句柄；readable/writable/append 是打开模式快照。closed 在显式 close 成功后置位，finalizer 则在尝试 close 后忽略结果并置位，不能据此推断 OS 状态；finalized 保护 native 记录释放流程。
 * ## Invariants and lifecycle
 * 记录由 NewStreamObject 的 malloc 创建，经隐藏 native-pointer 字段交给 wrapper；StreamFinalize 尝试关闭后释放记录，关闭失败后的 OS 状态未知。隐藏 owner 写入软失败可发布不持有记录的 wrapper，见 NewStreamObject。
 * ## References
 * `fs_stream.c` 的 `NewStreamObject`、`GetStreamData`、`StreamCloseInternal` 与 `StreamFinalize`。
 */
typedef struct ZrSystemFsStreamData {
    /** 平台 File API 返回并由此记录独占的句柄；closed 后置为 invalid sentinel。 */
    TZrLibrary_File_Handle handle;
    /** 是否允许通过该句柄读取，取自成功打开的结果。 */
    TZrBool readable;
    /** 是否允许通过该句柄写入，取自成功打开的结果。 */
    TZrBool writable;
    /** 是否以追加模式打开；用于记录打开模式，不替代宿主句柄状态。 */
    TZrBool append;
    /** 显式 close 失败不置位；finalizer 尝试 close 后无论结果都置位并立即释放记录，宿主状态依平台定义。 */
    TZrBool closed;
    /** finalizer 是否已处理此记录，防止重复关闭/释放。 */
    TZrBool finalized;
} ZrSystemFsStreamData;

/** 将有符号整数值交给对象字段 setter。 */
/**
 * ## Summary
 * 为 FileSystemEntry、SystemFileInfo 和 FileStream 的整数属性构造 VM int 并尝试写字段。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 目标对象；调用期间须保持有效。
 * @param[in] fieldName NUL 结尾的字段名。
 * @param[in] value 写入的有符号 64 位值。
 * ## Returns
 * void；参数无效时不操作，`Object_SetFieldCString` 的软失败不会返回给调用者。
 * ## Constraints and lifecycle
 * 写入通常不需要字符串物化，但创建字段键仍可能分配；不得将返回到上层等同于字段一定存在。
 * ## References
 * `MakeInfoObject`、`StreamSyncFields`、`StreamCloseInternal`；底层 setter 为 `ZrLib_Object_SetFieldCString`。
 */
void ZrSystem_Fs_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value);

/** 构造 bool 并尝试写入对象字段；其状态/失败约定与整数 setter 相同。 */
/**
 * ## Summary
 * 写入 FileSystemEntry、SystemFileInfo 或 FileStream 的布尔属性。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 目标对象。
 * @param[in] fieldName NUL 结尾的字段名。
 * @param[in] value 要物化的布尔值。
 * ## Returns
 * void；无效参数或底层 void setter 的软失败都不产生本地状态。
 * ## Constraints and lifecycle
 * 目标对象须在调用期间可达；调用者应独立决定是否能继续发布部分填充的对象。
 * ## References
 * `MakeInfoObject`、`StreamSyncFields` 与 `ZrLib_Object_SetFieldCString`。
 */
void ZrSystem_Fs_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value);

/**
 * ## Summary
 * 创建 VM string 并尝试写入对象字段；FileInfo 快照和路径包装属性共用此路径。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 目标对象；此函数用临时根跨字符串分配保活并在分配后重取地址。
 * @param[in] fieldName NUL 结尾的字段名。
 * @param[in] value NUL 结尾的 native string；NULL 按空串处理。
 * ## Returns
 * void；底层字段 setter 无法报告软失败，故不保证字段已写入。
 * ## Constraints and lifecycle
 * 字符串创建可能触发 GC；`value` 必须在同步构造期间可读。BUG: 若短串驻留表在跨阈值插入时仅 `HASH_BUCKET` 扩容分配失败，`SetString` 不写局部 `fieldValue`，随后字段 setter 仍读取它；具体合法窄触发链见叶台账。
 * ## References
 * `MakeInfoObject`、`PopulateEntryObject`、`fs_stream.c` 的 `NewStreamObject`；`native_binding_dispatch.c` 的 `ZrLib_Value_SetString` 和 `ZrLib_Object_SetFieldCString`。
 */
void ZrSystem_Fs_WriteStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName, const TZrChar *value);

/**
 * ## Summary
 * 尝试将对象字段设为 VM null，供无父目录等可选关联显式初始化。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 目标对象。
 * @param[in] fieldName NUL 结尾字段名。
 * ## Returns
 * void；无效参数时不操作，字段写入失败不回传。
 * ## Constraints and lifecycle
 * 这不会删除字段，只会尝试设置 null；底层字段 setter 可能软失败。
 * ## References
 * `PopulateEntryObject`；`ZrLib_Object_SetFieldCString`。
 */
void ZrSystem_Fs_WriteNullField(SZrState *state, SZrObject *object, const TZrChar *fieldName);

/**
 * ## Summary
 * 将已有 VM object/array 字段值交给对象字段 setter，用于 parent、fileInfo 等关联。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 目标对象。
 * @param[in] fieldName NUL 结尾字段名。
 * @param[in] fieldObject 要关联的对象；不得为 NULL。
 * @param[in] valueType 与 `fieldObject` 实际种类一致的 OBJECT 或 ARRAY 值类型。
 * ## Returns
 * void；无效参数或底层 setter 软失败均不回传。
 * ## Constraints and lifecycle
 * 底层 setter 在创建字段键前固定值对象；调用者仍须让输入对象在进入 setter 前可达。不能据此确认字段写入成功。
 * ## References
 * `PopulateEntryObject`、`RefreshEntryObject`；`ZrLib_Value_SetObject`、`ZrLib_Object_SetFieldCString`。
 */
void ZrSystem_Fs_WriteObjectField(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  SZrObject *fieldObject,
                                  EZrValueType valueType);

/**
 * ## Summary
 * 按 NUL 结尾键名查找一个对象字段，供本模块的属性读取器共享。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 已初始化对象。
 * @param[in] fieldName NUL 结尾键名。
 * ## Returns
 * @return 命中时返回对象内部字段值的只读借用指针；无效参数或缺字段返回 NULL。
 * ## Constraints and lifecycle
 * 返回值不拥有字段槽；不得跨可能修改对象字段/容器或使对象失效的操作保存。
 * ## References
 * `GetStringField`、`GetIntField`、`GetBoolField`；`ZrLib_Object_GetFieldCString`。
 */
const SZrTypeValue *ZrSystem_Fs_GetFieldValue(SZrState *state, SZrObject *object, const TZrChar *fieldName);

/**
 * ## Summary
 * 读取指定字段的 VM string，并返回其 native C 字符串视图。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 被读取对象。
 * @param[in] fieldName NUL 结尾键名。
 * ## Returns
 * @return 字段确为有效 VM string 时返回其内部 native 字符串；缺字段、其他类型或空对象返回 NULL。
 * ## Constraints and lifecycle
 * 返回的是 VM 字符串内部借用视图，不复制也不延长字符串/对象寿命；调用方只应同步读取并在 GC/修改前消费。
 * ## References
 * `GetFieldValue`；`fs_entry.c` 的路径获取与 `fs_stream.c` 的 mode/path 诊断。
 */
const TZrChar *ZrSystem_Fs_GetStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName);

/**
 * ## Summary
 * 读取 signed/unsigned integer 字段并将其规整为 TZrInt64，缺失或非整数回退到默认值。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 被读取对象。
 * @param[in] fieldName NUL 结尾键名。
 * @param[in] defaultValue 缺失或不匹配时返回的值。
 * ## Returns
 * @return signed 值原样返回；unsigned 值按 C 转换转为 TZrInt64；其他情况返回 defaultValue。
 * ## Constraints and lifecycle
 * 字段槽只在本次读取中借用；调用者应保证对象有效。
 * ## References
 * `GetFieldValue`；`fs_stream.c` 的 `StreamSyncFields` 在关闭后读取缓存快照。
 */
TZrInt64 ZrSystem_Fs_GetIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 defaultValue);

/**
 * ## Summary
 * 读取 bool 字段，缺失或非 bool 时使用调用方提供的默认值。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 被读取对象。
 * @param[in] fieldName NUL 结尾键名。
 * @param[in] defaultValue 缺失/类型不符时的返回值。
 * ## Returns
 * @return 字段为 bool 时返回规整后的 ZR_TRUE/ZR_FALSE，否则返回 defaultValue。
 * ## Constraints and lifecycle
 * 当前仓内只有声明和定义，没有实际消费者；保留的 closed 字段旧读取用途需后续决定是否移除。
 * ## References
 * `GetFieldValue`；无仓内调用点，定义与唯一声明分别在 fs_common.c 和本头文件。
 */
TZrBool ZrSystem_Fs_GetBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool defaultValue);

/**
 * ## Summary
 * 从 native callback context 提取普通 OBJECT 类型的 self。
 * ## Parameters
 * @param[in] context 正在派发的有效调用上下文。
 * ## Returns
 * @return self 缺失、非 OBJECT 或 null 时返回 NULL，否则返回借用的对象视图。
 * ## Constraints and lifecycle
 * context/state 归派发器管理；返回值不创建根，限当前 callback 的有效期，跨 VM 分配须按调用协议保活。
 * ## References
 * `fs_entry.c` 与 `fs_stream.c` 的对象方法 callbacks；`ZrLib_CallContext_Self`。
 */
SZrObject *ZrSystem_Fs_SelfObject(const ZrLibCallContext *context);

/**
 * ## Summary
 * 为 native 构造选择调度器传入的实例或 construct target 原型对应的新对象。
 * ## Parameters
 * @param[in] context 有效构造 callback context，包含 state、owner 与目标原型信息。
 * ## Returns
 * @return owner 类型匹配的已有 self，或新建对象；缺少有效 context/原型或分配失败时为 NULL。
 * ## Constraints and lifecycle
 * 只有 self 确实属于 owner prototype 才复用；返回的新对象需由调用者立即根住/交给结果槽。
 * ## References
 * `fs_entry.c` 的 Entry/File/Folder 构造器；`SelfObject`、`ZrLib_Type_NewInstanceWithPrototype`。
 */
SZrObject *ZrSystem_Fs_ResolveConstructTarget(ZrLibCallContext *context);

/**
 * ## Summary
 * 把构造成功的对象写入 native callback 的结果槽。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[out] result 可写 native callback 结果槽。
 * @param[in] object 要发布的对象。
 * ## Returns
 * @return 参数均非 NULL 时写入 OBJECT 值并返回 true；否则不写并返回 false。
 * ## Constraints and lifecycle
 * 该值初始化不分配；调用方须保证 object 在写槽时仍有效，VM dispatcher 接管后续结果生命周期。
 * ## References
 * `fs_entry.c` 构造、copy/move/open callback；`ZrLib_Value_SetObject`。
 */
TZrBool ZrSystem_Fs_FinishObjectResult(SZrState *state, SZrTypeValue *result, SZrObject *object);

/**
 * ## Summary
 * 读取指定索引的必需 VM string，提供同步 native C 字符串视图。
 * ## Parameters
 * @param[in] context native callback context。
 * @param[in] index 零基实参索引。
 * @param[out] outText 输出借用的 NUL 结尾视图；函数入口先置 NULL。
 * ## Returns
 * @return 取得有效 VM string 的 native view 时 true；无效 context/输出槽或无法取得 view 时 false。缺实参与错类型按 CallContext_ReadString 的 VM 错误路径处理。
 * ## Constraints and lifecycle
 * 该视图不复制、不延长 VM string 生命周期，只能在当前调用有效期内同步使用；字符串类型错误依 `CallContext_ReadString` 的 VM 错误语义。
 * ## References
 * `ReadOptionalStringArgument`；`fs_entry.c`/`fs_stream.c` 的必需路径和文本参数读取。
 */
TZrBool ZrSystem_Fs_ReadStringArgument(const ZrLibCallContext *context, TZrSize index, const TZrChar **outText);

/**
 * ## Summary
 * 读取可选短字符串参数或默认值，并复制进调用方拥有的缓冲区。
 * ## Parameters
 * @param[in] context callback context；可为空，此时采用 defaultValue。
 * @param[in] index 零基可选参数索引。
 * @param[in] defaultValue 缺省时使用的 C 字符串；NULL 视为空串。
 * @param[out] buffer 调用方提供的输出缓冲区。
 * @param[in] bufferSize buffer 总容量，包含结尾 NUL。
 * ## Returns
 * @return 完整复制且以 NUL 结尾时 true；无缓冲/容量、类型或长度不适配时 false。
 * ## Constraints and lifecycle
 * 参数过长时不写入 buffer；参数存在时由必需 string helper 读取，类型错误可按 VM TypeError 路径退出。成功后调用方拥有缓冲内容，不再借用实参。
 * ## References
 * fs_entry.c 的 File_Open 读取 mode（:385），fs_stream.c 的 Stream_Seek 读取 origin（:544）；ReadStringArgument。
 */
TZrBool ZrSystem_Fs_ReadOptionalStringArgument(const ZrLibCallContext *context,
                                               TZrSize index,
                                               const TZrChar *defaultValue,
                                               ZR_OUT TZrChar *buffer,
                                               TZrSize bufferSize);

/**
 * ## Summary
 * 读取可选 bool 参数；缺参时只返回调用方默认值。
 * ## Parameters
 * @param[in] context callback context。
 * @param[in] index 零基可选参数索引。
 * @param[in] defaultValue 缺参、context 为空时采用的值。
 * @param[out] outValue 成功时写入已规整 bool；入口先写默认值。
 * ## Returns
 * @return 缺参或实际 bool 时 true；输出槽无效或实参类型非 bool 时 false。
 * ## Constraints and lifecycle
 * 非 bool 仅普通失败，不由本 helper 抛异常；错误默认值仍留在 outValue。具体回调决定如何表示失败。
 * ## References
 * `fs_entry.c` 的递归、覆盖、glob 和 delete 参数 consumers。
 */
TZrBool ZrSystem_Fs_ReadOptionalBoolArgument(const ZrLibCallContext *context,
                                             TZrSize index,
                                             TZrBool defaultValue,
                                             TZrBool *outValue);

/**
 * ## Summary
 * 读取可选整数参数并交给 VM ReadInt helper 执行类型转换。
 * ## Parameters
 * @param[in] context callback context。
 * @param[in] index 零基可选参数索引。
 * @param[in] defaultValue 缺参/context 为空时采用的值。
 * @param[out] outValue 入口先置默认值，提供实参时由 ReadInt 尝试更新。
 * ## Returns
 * @return 缺参时 true；有参时返回 ReadInt 状态。
 * ## Constraints and lifecycle
 * 非整数值沿 `CallContext_ReadInt` 的异常/转换契约处理，不应等同于 optional bool 的普通 false。
 * ## References
 * fs_stream.c 的 Stream_ReadBytes（:441）和 Stream_ReadText（:461）读取可选 count；seek/length 直接调用 ReadInt，不经本 helper。
 */
TZrBool ZrSystem_Fs_ReadOptionalIntArgument(const ZrLibCallContext *context,
                                            TZrSize index,
                                            TZrInt64 defaultValue,
                                            TZrInt64 *outValue);

/**
 * ## Summary
 * 读取并校验指定索引的 VM array 实参。
 * ## Parameters
 * @param[in] context callback context。
 * @param[in] index 零基实参索引。
 * @param[out] outArray 成功时借出 array 对象；入口先置 NULL。
 * ## Returns
 * @return context/输出槽有效且 ReadArray 成功时 true，否则 false/ReadArray 定义的错误路径。
 * ## Constraints and lifecycle
 * 返回对象不归 helper 所有；使用期须依 native 参数/GC root 协议保持有效。
 * ## References
 * `fs_entry.c` 与 `fs_stream.c` 的 writeBytes callbacks；`ZrLib_CallContext_ReadArray`。
 */
TZrBool ZrSystem_Fs_ReadArrayArgument(const ZrLibCallContext *context, TZrSize index, SZrObject **outArray);

/**
 * ## Summary
 * 格式化宿主失败消息并将 VM 当前异常设为 IOException。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] format printf 风格格式串；其后为相应 varargs。
 * ## Returns
 * @return 参数无效时返回 false；进入异常状态的可返回路径也返回 false。回退诊断路径可由 `Debug_RunError` 非局部终止。
 * ## Constraints and lifecycle
 * 格式化先写入 512 字节栈缓冲，过长消息按 `vsnprintf` 截断；创建异常时对象临时根住，调用者须在宿主失败仍有上下文时立即调用。
 * ## References
 * `RaiseErrnoIOException`；`system_fs_make_io_exception`；`ZrCore_Exception_NormalizeThrownValue`。
 */
TZrBool ZrSystem_Fs_RaiseIOException(SZrState *state, const TZrChar *format, ...);

/**
 * ## Summary
 * 将当前 errno 转成带 action/path 的 IOException 消息。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] action 描述失败操作的 C 字符串；NULL 使用 `I/O`。
 * @param[in] path 失败路径 C 字符串；NULL 使用 `<path>`。
 * ## Returns
 * @return 继承 `RaiseIOException` 的 false/异常状态契约。
 * ## Constraints and lifecycle
 * 进入时 errno 须仍对应待报告的失败；strerror(errno) 同步读取。TODO: stream.c:451/473 等 caller 先求值 GetStringField(path) 实参再进入本函数，需沿 Object_GetFieldCString 的键物化/分配核查是否覆盖 errno；目前未证实改码。
 * ## References
 * `fs_entry.c`、`fs_stream.c` 的宿主 I/O 失败分支；`RaiseIOException`。
 */
TZrBool ZrSystem_Fs_RaiseErrnoIOException(SZrState *state, const TZrChar *action, const TZrChar *path);

/**
 * ## Summary
 * 将宿主 File_Info 中的路径、时间和类型字段物化为 SystemFileInfo 脚本对象。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] info 同步读取的宿主快照；函数不接管其存储。
 * ## Returns
 * @return 成功创建并填充时返回对象；无效输入、实例或临时根建立失败时为 NULL。
 * ## Constraints and lifecycle
 * 构造对象期间在栈上建立临时根，并每次潜在 VM 分配后重新读取对象；字段 setter 是 void，软失败可能留下未填字段。返回后根结束，由调用者/VM 结果槽负责保持对象可达。
 * ## References
 * `fs_entry.c` 的 GetInfo；`PopulateEntryObject`、`RefreshEntryObject`。
 */
SZrObject *ZrSystem_Fs_MakeInfoObject(SZrState *state, const SZrLibrary_File_Info *info);

/**
 * ## Summary
 * 查询路径并填充 FileSystemEntry/File/Folder 的 path、fullPath、name、parent 与 fileInfo 快照。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 待填充实例；函数建立临时根并在分配后重取。
 * @param[in] originalPath 保留在脚本 path 字段的原始拼写；NULL 时回退 info.path。
 * @param[in] fullPathOverride 查询路径；NULL 时使用 originalPath。
 * ## Returns
 * @return 完成查询、创建 info、并到达字段写入阶段时 true；前置条件、QueryInfo、子对象或根建立失败时 false。
 * ## Constraints and lifecycle
 * info 和 parentPath 是本地宿主快照；parent 递归包装失败时会写 null。TODO: originalPath/fullPathOverride 可能是借用 VM string，后续跨 MakeInfoObject 与 parent 子对象分配再读取；核查各 native lane 的参数保活、string 存储地址及可达 GC 窗口。当前普通 OBJECT 自身由临时根保护且地址原址晋升。
 * ## References
 * `fs_entry.c` 的构造与 Folder 列举/复制/移动结果；`MakeInfoObject`、`NewEntryObject`。
 */
TZrBool ZrSystem_Fs_PopulateEntryObject(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *originalPath,
                                        const TZrChar *fullPathOverride);

/**
 * ## Summary
 * 按类型名创建 FileSystemEntry 派生对象并调用 PopulateEntryObject 初始化。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] typeName 已在当前注册表中可构造的类型名。
 * @param[in] originalPath 脚本暴露的输入路径拼写。
 * @param[in] fullPathOverride 可选宿主查询路径；NULL 使用 originalPath。
 * ## Returns
 * @return 初始化成功返回对象；参数、实例分配或填充失败返回 NULL。
 * ## Constraints and lifecycle
 * 当前 ordinary OBJECT GC 原址晋升，Populate 内部临时根保护对象而本层返回地址保持有效；路径字符串跨 Populate 内部分配的借用边界仍由其声明 TODO 覆盖。返回对象须立即由 callback 结果槽或其他根接管。
 * ## References
 * `PopulateEntryObject`；`fs_entry.c` 的 list、copyTo、moveTo 与 parent 包装路径。
 */
SZrObject *ZrSystem_Fs_NewEntryObject(SZrState *state,
                                      const TZrChar *typeName,
                                      const TZrChar *originalPath,
                                      const TZrChar *fullPathOverride);

/**
 * ## Summary
 * 用对象当前 fullPath 重新查询宿主信息，供 exists/refresh 和 I/O 后快照刷新共用。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object FileSystemEntry 派生对象，必须已含 string fullPath。
 * @param[out] outInfo 可选的按值快照输出；入口先清零，仅成功查询后写入。
 * @param[out] outInfoObject 可选的 VM info 对象输出；入口先置 NULL，仅成功物化后写入。
 * ## Returns
 * @return 路径可读、QueryInfo 成功且 info 对象创建成功时 true；否则 false。
 * ## Constraints and lifecycle
 * 只更新 `fileInfo` 字段，不重算 path/name/parent。setter 为 void，故 true 不保证 fileInfo 确实更新；输出快照仍表示本次查询。当前普通 OBJECT 地址在 GC 晋升期间稳定，字段写入会在内部固定值对象。
 * ## References
 * `fs_entry.c` 的 Entry_Exists/Refresh 和 I/O callbacks；`GetStringField`、`MakeInfoObject`、`WriteObjectField`。
 */
TZrBool ZrSystem_Fs_RefreshEntryObject(SZrState *state,
                                       SZrObject *object,
                                       ZR_OUT SZrLibrary_File_Info *outInfo,
                                       ZR_OUT SZrObject **outInfoObject);

/**
 * ## Summary
 * 将刚打开的宿主文件句柄包装为 FileStream，并把 native owner 与 FFI 可见整数视图装入对象。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] path 构造包装所用路径字符串，调用期间须有效。
 * @param[in] openResult 已打开句柄及权限/模式快照；完整 owner 发布成功时由新对象接管，当前非 NULL 软失败例外见 Constraints。
 * ## Returns
 * @return wrapper 进入发布路径时返回对象；输入/根或普通分配失败时为 NULL，由 caller 管理原句柄。BUG: File_Open 已打开句柄且 Type_NewInstance VM 分配异常非局部退出时，越过 NULL-only CloseHandle；TryRun 仅恢复 VM roots/scopes，不释放尚未包装的 C 局部句柄。
 * ## Constraints and lifecycle
 * `NewStreamObject` 设置 finalizer 后以 void setter 写隐藏 owner；若 owner 字段的 key/string 或 pin 软失败，它仍可返回非 NULL。BUG: 在打开句柄后仅此 owner 写入失败时，File_Open 视为成功并不关闭句柄；后续 `GetStreamData`/finalizer 因字段缺失而无法释放 malloc 记录或关闭句柄。确切条件与证据在本批台账；该失败未做 fault injection。
 * ## References
 * `fs_entry.c` 的 File_Open；`fs_stream.c` 的 `GetStreamData`、`StreamFinalize`；`ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD`。
 */
SZrObject *ZrSystem_Fs_NewStreamObject(SZrState *state,
                                       const TZrChar *path,
                                       const SZrLibrary_File_StreamOpenResult *openResult);

/**
 * ## Summary
 * 从 FileStream 隐藏字段取出本模块拥有的宿主句柄记录。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 流包装对象。
 * ## Returns
 * @return 隐藏值是 native pointer 时返回借用记录指针；缺失、其他类型或无效对象返回 NULL。
 * ## Constraints and lifecycle
 * 返回记录不转移所有权，不可由调用方 free；随 wrapper finalizer 释放，调用期间须让对象保持有效。
 * ## References
 * `fs_stream.c` 的所有 stream callbacks 和 `StreamFinalize`。
 */
ZrSystemFsStreamData *ZrSystem_Fs_GetStreamData(SZrState *state, SZrObject *object);

/**
 * ## Summary
 * 验证 FileStream 仍关联可操作句柄，并可选输出 native owner 记录。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 流包装对象。
 * @param[out] outData 可选记录输出；若提供则先写查找结果，包括 NULL。
 * ## Returns
 * @return 有非 closed 且非 invalid 句柄记录时 true；否则设置 IOException 并返回 false。
 * ## Constraints and lifecycle
 * 不检查 readable/writable 权限；具体操作需另行检查模式。返回 owner 是借用，不得释放。
 * ## References
 * `fs_stream.c` 读写、seek、flush、setLength callbacks；`GetStreamData`、`RaiseIOException`。
 */
TZrBool ZrSystem_Fs_StreamEnsureOpen(SZrState *state,
                                     SZrObject *object,
                                     ZrSystemFsStreamData **outData);

/**
 * ## Summary
 * 将宿主句柄的 position/length 及 private closed 状态同步到 FileStream 字段。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 流包装对象。
 * @param[in] data 该对象当前 owner 记录。
 * ## Returns
 * @return 有效参数且开放句柄 position/length 查询成功时 true；查询失败或参数无效时 false。
 * ## Constraints and lifecycle
 * closed/invalid 状态改用对象中最后缓存的 position/length。字段 setter 为 void，true 只代表宿主快照查询通过，不保证 VM 字段均写入。
 * ## References
 * `fs_stream.c` 的 read/write/seek/setLength/close 操作；`GetIntField`、Write 字段助手。
 */
TZrBool ZrSystem_Fs_StreamSyncFields(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data);

/**
 * ## Summary
 * 显式 close 与 using 清理共用的宿主句柄状态转换路径；GC finalizer 走独立清理流程。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] object 流包装对象。
 * @param[in,out] data 被更新的 owner 记录。
 * ## Returns
 * @return 已 closed 或平台 close 成功时 true；参数无效或宿主 close 失败时 false。
 * ## Constraints and lifecycle
 * 调用先同步旧快照；只有 close 成功后才置 invalid/closed。平台 close 失败后仅 private `closed=false` 未变化，不能据此断言 OS handle 仍开放；平台重试/句柄复用语义由调用者风险说明处理。
 * ## References
 * fs_stream.c 的 Stream_Close、StreamSyncFields 与 ZrLibrary_File_CloseHandle；GC finalizer 单独尝试关闭并释放记录。
 */
TZrBool ZrSystem_Fs_StreamCloseInternal(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data);

/**
 * ## Summary
 * GC finalizer 的无返回清理回调：尝试关闭仍关联的宿主句柄，再释放 FileStream owner 记录。
 * ## Parameters
 * @param[in] state finalizer 所属 VM state。
 * @param[in] rawObject 被收集的 GC 对象。
 * ## Returns
 * void；owner 缺失或已 finalized 时静默结束；句柄 close 失败不向脚本报告。
 * ## Constraints and lifecycle
 * 仅对隐藏 owner 可读取的 wrapper 生效；finalized 先置位。CloseHandle 返回值被忽略，随后置 invalid/closed 并 free(data)，不保证宿主关闭成功；owner 缺失时无法恢复记录或 handle。
 * ## References
 * `fs_stream.c` 的 `NewStreamObject` 将此函数放入 `scanMarkGcFunction`；Core GC sweep 派发该 callback。
 */
void ZrSystem_Fs_StreamFinalize(SZrState *state, SZrRawObject *rawObject);

/**
 * ## Summary
 * 从句柄读取指定字节数并构造 VM array，每个元素为 0..255 整数。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] handle 调用方所有的开放文件句柄；本函数不关闭。
 * @param[in] count 非负时最多读取该字节数，负数表示读至 EOF。
 * @param[out] result 成功时接收 ARRAY VM 值。
 * @param[out] outReadCount 可选实际消费字节数；入口先清零。
 * ## Returns
 * @return 完整读取并构造数组时 true；宿主读取、native buffer 或数组/元素普通失败时 false；VM 分配异常可非局部退出。
 * ## Constraints and lifecycle
 * native 缓冲在普通返回路径 free，已消费数据不回滚。BUG: Array_New/Array_PushValue 的 VM 分配异常非局部退出会越过 free(bytes)；一次性 File helper 还越过外层 CloseHandle。Exception_Throw/TryRun 不遍历放弃的 C 栈资源，普通 false 清理与此区分。
 * ## References
 * `fs_entry.c` 的兼容 readBytes 与 `fs_stream.c` 的 FileStream.readBytes；`system_fs_read_buffer_from_handle`。
 */
TZrBool ZrSystem_Fs_ReadBytesFromHandle(SZrState *state,
                                        TZrLibrary_File_Handle handle,
                                        TZrInt64 count,
                                        SZrTypeValue *result,
                                        ZR_OUT TZrInt64 *outReadCount);

/**
 * ## Summary
 * 从句柄按字节读取文本并创建 VM string，同时可回报实际读取数。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] handle 调用方所有的开放文件句柄；不关闭。
 * @param[in] count 非负时最多消费该字节数，负数读至 EOF。
 * @param[in,out] result 建串成功时接收 VM string；SetString 软失败可保留旧值，native dispatcher 在 callback 前预置 null。
 * @param[out] outReadCount 可选实际消费字节数；入口先清零。
 * ## Returns
 * @return 宿主读取及 native buffer 准备通过后返回 true，未检查 void SetString 的结果；参数、宿主读或 native buffer 失败时 false。
 * ## Constraints and lifecycle
 * C-string 建串截断首 NUL 且不验证 UTF-8。BUG: 未驻留短文本的单次 HASH_BUCKET 软失败可让 void SetString 不写 result；其余步骤正常返回时 File/Stream 可 true/null。另在 SetString VM 分配异常非局部退出时，malloc text 越过 free(text)，一次性 File helper 还越过外层 CloseHandle；普通 false 清理不受此条件影响。
 * ## References
 * `fs_entry.c` 的 readText aliases 与 `fs_stream.c` 的 FileStream.readText；`ZrLib_Value_SetString`。
 */
TZrBool ZrSystem_Fs_ReadTextFromHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       TZrInt64 count,
                                       SZrTypeValue *result,
                                       ZR_OUT TZrInt64 *outReadCount);

/**
 * ## Summary
 * 将 VM 数组编码为字节并完整写入宿主句柄。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] handle 调用方所有且可写的开放句柄。
 * @param[in] array 每项必须为 signed/unsigned integer，范围 0..255。
 * @param[out] outWrittenCount 可选已完成总字节数；入口先清零，成功时写完整数组长度。
 * ## Returns
 * @return 所有字节写完时 true；非法元素、native buffer 或宿主写入失败时 false。
 * ## Constraints and lifecycle
 * 短写循环完成，部分已写前缀不可回滚，句柄归 caller。中间 bytes 缓冲仅在普通返回路径释放。BUG: 非空且仍未物化的合法 RAW_CANONICAL 连续整数数组，Array_Get 物化需新增 pair pool；相关 GcMalloc 失败并非局部抛异常时，越过已 malloc 缓冲的 free，一次性 File helper 还越过外层 CloseHandle；不泛化到非 raw/普通 false。
 * ## References
 * `fs_entry.c` 的 File.writeBytes/appendBytes 与 `fs_stream.c` 的 stream.writeBytes；`system_fs_write_all`。
 */
TZrBool ZrSystem_Fs_WriteBytesToHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       SZrObject *array,
                                       ZR_OUT TZrInt64 *outWrittenCount);

/**
 * ## Summary
 * 以 NUL 结尾 C 字符串写入宿主句柄并回报写入字节数。
 * ## Parameters
 * @param[in] state 当前 VM state。
 * @param[in] handle 调用方所有且可写的开放句柄。
 * @param[in] text NUL 结尾的文本视图。
 * @param[out] outWrittenCount 可选长度输出；入口先清零，成功时等于 strlen(text)。
 * ## Returns
 * @return 完整写入时 true；无效参数或宿主写入失败时 false。
 * ## Constraints and lifecycle
 * 长度按 strlen 计算，嵌入 NUL 后缀不写；短写循环但部分宿主写入不回滚。文本/句柄均由调用方管理。
 * ## References
 * `fs_entry.c` 和 `fs_stream.c` 的文本写入 callback；`system_fs_write_all`。
 */
TZrBool ZrSystem_Fs_WriteTextToHandle(SZrState *state,
                                      TZrLibrary_File_Handle handle,
                                      const TZrChar *text,
                                      ZR_OUT TZrInt64 *outWrittenCount);

#endif // ZR_VM_LIB_SYSTEM_FS_INTERNAL_H
