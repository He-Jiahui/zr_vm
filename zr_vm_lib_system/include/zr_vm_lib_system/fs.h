/**
 * @file fs.h
 * ## Summary
 * 声明 zr.system.fs 兼容函数及 FileSystemEntry、File、Folder、FileStream native callbacks。
 *
 * ## Constraints and lifecycle
 * C 返回值表示 callback 是否完成，不等同于脚本 bool；常规 dispatcher 先将 result 置为 null，再按 descriptor 检查元数。
 * 必需 string 的类型错误由 ReadString helper 抛 TypeError；optional bool 类型不符通常仅令 callback 返回 ZR_FALSE，optional int 走 ReadInt 异常路径，optional string 超出缓冲区可返回 ZR_FALSE。
 * 各 callback 对宿主 I/O 失败是否抛 IOException 有自己的路径，见各自 Returns。工作目录是进程全局状态；Entry metadata 是查询快照；只有 FileStream 拥有打开句柄。
 * 可写 result 槽由框架管理；字符串结果槽与 metadata 字段的 void setter 均有独立软失败边界；callback 返回 true 不保证这些 setter 已写入结果或更新全部字段。
 *
 * ## References
 * descriptor 定义见 fs_registry.c；实现见 fs_entry.c 与 fs_stream.c。
 */
//
// zr.system.fs native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_FS_H
#define ZR_VM_LIB_SYSTEM_FS_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 读取当前工作目录并返回规范化绝对路径。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示路径规范化成功且 callback 完成 SetString 调用；规范化失败返回 ZR_FALSE，不抛文件系统异常。SetString 是 void setter，C 返回 true 不证明其软分配失败路径已写入结果。
 *
 * ## Constraints and lifecycle
 * VM 参数：无 VM 实参。
 * 工作目录是进程全局状态；相对路径查询会受其他调用的目录切换影响，宿主需要稳定语义时须协调。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_CurrentDirectory；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_CurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 切换宿主进程的工作目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示 callback 完成并写入脚本 bool；目录切换失败表现为脚本 false，不抛 IOException。无效上下文/结果槽或必需字符串读取失败返回 ZR_FALSE；字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。切换修改宿主进程级工作目录。
 * path：目标目录路径。 这是跨 VM/state 共享的进程级副作用；需要稳定相对路径解析时，宿主须协调目录切换与文件操作。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_ChangeCurrentDirectory；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_ChangeCurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 查询路径是否存在。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示查询完成并写入脚本 bool；路径不存在表现为 false，不抛文件系统异常。无效上下文/结果槽或必需字符串读取失败返回 ZR_FALSE；字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。结果是查询时点快照。
 * path：待查询路径。 结果反映一次宿主文件系统查询，不保证后续调用观察到同一状态。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_PathExists；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_PathExists(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 查询路径当前是否解析为普通文件。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示查询完成并写入脚本 bool；不存在或不是普通文件表现为 false，不抛文件系统异常。无效上下文/结果槽或必需字符串读取失败返回 ZR_FALSE；字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。结果是查询时点快照。
 * path：待查询路径。 结果反映查询时点；缺失路径或其他对象类型不会作为文件返回。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_IsFile；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_IsFile(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 查询路径当前是否解析为目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示查询完成并写入脚本 bool；不存在或不是目录表现为 false，不抛文件系统异常。无效上下文/结果槽或必需字符串读取失败返回 ZR_FALSE；字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。结果是查询时点快照。
 * path：待查询路径。 结果反映查询时点；缺失路径或其他对象类型不会作为目录返回。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_IsDirectory；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_IsDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 尝试创建单级目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示创建成功并写入脚本 true；宿主创建失败抛 IOException 并返回 ZR_FALSE。参数/上下文读取失败也返回 ZR_FALSE，必需字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。只尝试创建单层目录。
 * path：待创建目录路径。 不递归创建缺失父目录；路径和权限按宿主文件层处理。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_CreateDirectory；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_CreateDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 递归创建目录路径及缺失父目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示创建成功并写入脚本 true；宿主创建失败抛 IOException 并返回 ZR_FALSE。参数/上下文读取失败也返回 ZR_FALSE，必需字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。允许创建缺失的父目录。
 * path：待创建目录路径。 此兼容入口固定采用递归创建；外部并发变更不受结果快照约束。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_CreateDirectories；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_CreateDirectories(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 删除文件或空目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示删除成功并写入脚本 true；路径不存在或宿主删除失败抛 IOException 并返回 ZR_FALSE。参数/上下文读取失败也返回 ZR_FALSE，必需字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。文件与目录均按非递归删除。
 * path：待删除路径。 不递归删除目录内容；对象 API 的 Folder.delete 可显式请求递归。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_RemovePath；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_RemovePath(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 打开路径、读取完整文本并关闭临时文件句柄。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示读取完成；文件读取失败抛 IOException 并返回 ZR_FALSE。参数/上下文读取失败返回 ZR_FALSE，必需字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。返回值是文件内容的 VM string。
 * path：待读取文件路径。句柄不会作为脚本资源返回；读取字节虽按长度取得，当前 SetString 按 C 字符串复制，遇到 NUL 时返回 VM string 在该位置截断；结果也不保证 UTF-8 校验。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_ReadText；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 覆盖文件并写入文本。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示写入完成并写入脚本 true；文件写入失败抛 IOException 并返回 ZR_FALSE。参数读取失败返回 ZR_FALSE，path/text 类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path、text：必需 string。以覆盖模式写入。
 * path：目标路径；text：文本内容。 短期句柄由本次回调收尾；不得将兼容 bool 与对象方法的字节数混淆。文本长度由 strlen 计算；嵌入 NUL 时仅写首个 NUL 前缀并返回该前缀的字节数；需要任意字节请使用 writeBytes。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_WriteText；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向文件末尾追加文本。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示追加完成并写入脚本 true；文件写入失败抛 IOException 并返回 ZR_FALSE。参数读取失败返回 ZR_FALSE，path/text 类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path、text：必需 string。以追加模式写入。
 * path：目标路径；text：文本内容。 短期句柄由本次回调收尾；不得将兼容 bool 与对象方法的字节数混淆。文本长度由 strlen 计算；嵌入 NUL 时仅写首个 NUL 前缀并返回该前缀的字节数；需要任意字节请使用 writeBytes。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_AppendText；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_AppendText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 查询路径并返回 SystemFileInfo 元数据快照。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示查询与对象物化完成并返回 SystemFileInfo；查询失败或 MakeInfoObject 返回 null 时直接返回 ZR_FALSE，不在此 callback 将这些失败转换为 IOException。参数字符串类型错误由 helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string。对象字段是查询时点的 metadata 快照。
 * path：待查询路径。 返回值不是实时视图；字段通过共享对象字段 helper 组装，字段写入失败的细节由该实现层决定。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_GetInfo；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_GetInfo(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 初始化构造目标 FileSystemEntry、File 或 Folder 的路径与初始元数据。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示目标解析和回调流程完成，并尝试向 receiver 写入路径及初始 metadata；目标解析或 PopulateEntryObject 返回 ZR_FALSE 时返回 ZR_FALSE。必需 path 类型错误由 helper 抛 TypeError；本 callback 不把普通解析失败转换为 IOException。
 *
 * ## Constraints and lifecycle
 * VM 参数：path：必需 string，作为新 FileSystemEntry/File/Folder receiver 的路径。
 * 构造参数 path；目标原型由 VM 构造上下文提供。 同一 callback 由三个类型的构造 descriptor 复用；receiver 不是普通显式方法参数。TODO:VM path 指针借用后跨 ResolveConstructTarget/PopulateEntryObject 分配窗口，GC root/移动协议仍待强制 GC 入口核实。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Entry_Constructor；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Entry_Constructor(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 刷新路径元数据并返回包装对象当前的存在性。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示读取/刷新快照完成并写入脚本 bool；receiver 无效或刷新失败返回 ZR_FALSE。该结果不抛文件系统 IOException。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；从 FileSystemEntry receiver 的快照更新 exists。
 * 隐式 receiver：FileSystemEntry、File 或 Folder；无显式实参。 fileInfo 是快照字段，不会自动随磁盘变化；此调用执行查询并更新 receiver。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Entry_Exists；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Entry_Exists(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 刷新包装对象路径元数据并返回 SystemFileInfo 快照。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；VM 参数与 receiver 按调用形状在本次 callback 中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示刷新并物化新的 SystemFileInfo；receiver 无效或刷新/对象创建失败返回 ZR_FALSE，不转换为文件系统 IOException。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；receiver 是待刷新的 FileSystemEntry。
 * 隐式 receiver：FileSystemEntry、File 或 Folder；无显式实参。 receiver 的 fileInfo 仅尝试更新；void 字段 setter 软失败时可能保持旧值；之前返回或保存的旧快照不会变成实时对象。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Entry_Refresh；绑定描述符见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Entry_Refresh(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 按打开模式打开 File 路径并返回封装 native handle 的 FileStream。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示尝试构造并交付 FileStream 包装对象；不保证所有字段都已写入。打开失败抛 IOException，包装对象创建失败返回 ZR_FALSE 并尝试关闭原句柄。无效 optional mode 返回 ZR_FALSE，字符串类型错误可抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：mode：可选 string，默认 r；超出本地 mode buffer 返回 ZR_FALSE。
 * 隐式 receiver：File；可选 mode 缺省 r。 mode 接受 r/r+/w/w+/a/a+/x/x+，可带 b 别名。TODO: 只有隐藏 StreamData owner 指针写入成功才确立句柄所有权转交；void setter 软失败可能让回调仍以 TRUE 发布未完整绑定的对象，故障与清理路径由 private stream 批次继续核查。fullPath 从 receiver 字段借用并跨越 NewStreamObject 分配，GC 压力下的指针有效期仍待核验。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_Open；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_Open(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 创建 File 指向的文件，按可选参数决定是否创建父目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示创建操作完成并写入 null；宿主创建失败抛 IOException 并返回 ZR_FALSE。recursively 类型不符由 optional-bool helper 返回 ZR_FALSE，不抛 VM 类型异常；成功后仅尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：recursively：可选 bool，默认 true。
 * 隐式 receiver：File；可选 recursively 缺省 true。 I/O 完成后尝试刷新 receiver 的 fileInfo 快照；刷新失败不回滚已完成 I/O；文件创建失败走文件层错误路径。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_Create；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_Create(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 File 指向的完整文本并关闭本次调用的临时句柄。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示文件读取完成并返回 string；文件读取失败抛 IOException 并返回 ZR_FALSE。读取后尝试刷新 receiver 快照，但不以刷新结果决定成功。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；路径来自 File receiver。
 * 隐式 receiver：File；无显式实参。临时句柄不逃逸回调；I/O 完成后尝试刷新 receiver 的 fileInfo 快照，刷新失败不回滚已完成 I/O。读取字节虽按长度取得，当前 SetString 按 C 字符串复制，遇到 NUL 时返回 VM string 在该位置截断；结果也不保证 UTF-8 校验。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_ReadText；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 覆盖 File 指向的文件并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示写入完成并返回写入字节数；文件写入失败抛 IOException 并返回 ZR_FALSE。text 类型错误由 ReadString helper 抛 TypeError；成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：text：必需 string。以覆盖模式写入。
 * 隐式 receiver：File；索引 0 为 text。 I/O 完成后尝试刷新 receiver 的 fileInfo；刷新失败不回滚已完成的 I/O；输入值由 VM 持有，回调在本次调用中借用文本。文本长度由 strlen 计算；嵌入 NUL 时仅写首个 NUL 前缀并返回该前缀的字节数；需要任意字节请使用 writeBytes。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_WriteText；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 File 指向的文件末尾追加文本并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示追加完成并返回写入字节数；文件写入失败抛 IOException 并返回 ZR_FALSE。text 类型错误由 ReadString helper 抛 TypeError；成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：text：必需 string。以追加模式写入。
 * 隐式 receiver：File；索引 0 为 text。 I/O 完成后尝试刷新 receiver 的 fileInfo；刷新失败不回滚已完成的 I/O；输入值由 VM 持有，回调在本次调用中借用文本。文本长度由 strlen 计算；嵌入 NUL 时仅写首个 NUL 前缀并返回该前缀的字节数；需要任意字节请使用 writeBytes。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_AppendText；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_AppendText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 File 指向的完整文件并返回 0..255 整数数组。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示读取完成并返回 byte array；文件读取失败抛 IOException 并返回 ZR_FALSE。成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；路径来自 File receiver。
 * 隐式 receiver：File；无显式实参。 临时句柄在返回前关闭；I/O 完成后尝试刷新 receiver 的 fileInfo 快照；刷新失败不回滚已完成 I/O。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_ReadBytes；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 用字节数组覆盖 File 指向的文件并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示写入完成并返回写入字节数；文件写入失败抛 IOException 并返回 ZR_FALSE。成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：bytes：必需 Array 参数，由 helper 验证。
 * 隐式 receiver：File；索引 0 为 bytes，元素须表示 0..255 字节。 数组由 VM 持有；临时句柄不逃逸本次调用，I/O 完成后尝试刷新 fileInfo；刷新失败不回滚已完成的 I/O。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_WriteBytes；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把字节数组追加到 File 指向的文件末尾并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示追加完成并返回写入字节数；文件写入失败抛 IOException 并返回 ZR_FALSE。成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：bytes：必需 Array 参数，由 helper 验证。
 * 隐式 receiver：File；索引 0 为 bytes，元素须表示 0..255 字节。 数组由 VM 持有；临时句柄不逃逸本次调用，I/O 完成后尝试刷新 fileInfo；刷新失败不回滚已完成的 I/O。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_AppendBytes；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_AppendBytes(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 复制源 File 到目标路径并返回绑定目标路径的新 File。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示复制完成并返回目标 File；复制失败抛 IOException 并返回 ZR_FALSE，目标对象创建失败也返回 ZR_FALSE。overwrite 的 optional-bool 类型不符只返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：targetPath：必需 string；overwrite：可选 bool，默认 false。
 * 隐式 receiver：File；targetPath 必填；overwrite 可选且缺省 false。 原 receiver 仍绑定源路径；复制完成和创建目标包装对象是分开的步骤。TODO:targetPath 是 ReadStringArgument 返回的 VM 实参借用；RefreshEntryObject 可能分配，随后同一指针传给 NewEntryObject。仍需核实 fast/inline/generic lane 对参数的 pin、minor/major GC 迁移及两次分配间的指针有效期；尚未证实悬空。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_CopyTo；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 移动源 File 到目标路径并返回绑定目标路径的新 File。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示移动完成并返回目标 File；移动失败抛 IOException 并返回 ZR_FALSE，目标对象创建失败也返回 ZR_FALSE。overwrite 的 optional-bool 类型不符只返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：targetPath：必需 string；overwrite：可选 bool，默认 false。
 * 隐式 receiver：File；targetPath 必填；overwrite 可选且缺省 false。 原 receiver 不改绑目标；移动后原路径可能不存在，新 wrapper 表示目标路径。TODO:targetPath 是 ReadStringArgument 返回的 VM 实参借用；RefreshEntryObject 可能分配，随后同一指针传给 NewEntryObject。仍需核实 fast/inline/generic lane 对参数的 pin、minor/major GC 迁移及两次分配间的指针有效期；尚未证实悬空。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_MoveTo；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 删除 File 指向的路径并返回 null。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示删除完成并写入 null；宿主删除失败抛 IOException 并返回 ZR_FALSE。成功后尝试刷新 receiver 快照，但不以刷新结果决定成功。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；删除 File receiver 表示的路径。
 * 隐式 receiver：File；无显式实参。 I/O 完成后尝试刷新 receiver 的 fileInfo；刷新失败不回滚已完成的 I/O；不会关闭其他独立 FileStream 持有的句柄。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_File_Delete；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_File_Delete(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 创建 Folder 对应目录，按可选参数决定是否递归创建父目录。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示创建完成并写入 null；宿主创建失败抛 IOException 并返回 ZR_FALSE。recursively 类型不符由 optional-bool helper 返回 ZR_FALSE，不抛 VM 类型异常；成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：recursively：可选 bool，默认 true。
 * 隐式 receiver：Folder；recursively 可选且缺省 true。 I/O 完成后尝试刷新 receiver 的 fileInfo；刷新失败不回滚已完成的 I/O；宿主文件系统权限与路径规则仍适用。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_Create；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_Create(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 列举 Folder 的直接子项。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示目录枚举及对象数组物化成功；枚举失败抛 IOException 并返回 ZR_FALSE，物化失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；枚举 Folder receiver 的直接子项。
 * 隐式 receiver：Folder；无显式实参。 只列直接子项；元素保存路径和元数据快照，不拥有打开句柄。实现把结果 array 暂存在 C 局部变量，并在逐项构造包装对象时分配 VM 对象；GC 存活与地址更新仍待压力验证。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_Entries；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_Entries(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 列举 Folder 的直接文件子项。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示目录枚举及 File 数组物化成功；枚举失败抛 IOException 并返回 ZR_FALSE，物化失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；仅返回 Folder receiver 的直接文件子项。
 * 隐式 receiver：Folder；无显式实参。 不递归；元素保存路径和元数据快照，不拥有打开句柄。构造期间局部 array 跨 VM 分配的 GC 存活与地址更新仍待压力验证。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_Files；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_Files(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 列举 Folder 的直接目录子项。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示目录枚举及 Folder 数组物化成功；枚举失败抛 IOException 并返回 ZR_FALSE，物化失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；仅返回 Folder receiver 的直接目录子项。
 * 隐式 receiver：Folder；无显式实参。 不递归；元素保存路径和元数据快照，不拥有打开句柄。构造期间局部 array 跨 VM 分配的 GC 存活与地址更新仍待压力验证。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_Folders；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_Folders(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按通配模式列举 Folder 下的匹配路径。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参按本次调用借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示 glob 枚举及结果数组物化成功；宿主 glob 失败抛 IOException 并返回 ZR_FALSE，物化失败返回 ZR_FALSE。recursively 类型不符只返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：pattern：必需 string；recursively：可选 bool，当前默认 true，但现有测试只显式传 true。
 * 隐式 receiver：Folder；pattern 必填，支持 * 与 ?；recursively 可选，当前缺省 true。 descriptor 的 arity 为 1..2，pattern 必填，recursively 可选；它只标出可选性而未编码默认值。实现目前缺省 true，现有测试显式传 true，省略参数的公开默认仍待专项确认。返回 array 的逐项对象构造期间 GC 存活与地址更新也仍待压力验证。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_Glob；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_Glob(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 复制目录树到目标路径并返回新的 Folder 包装对象。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示复制完成并返回目标 Folder；复制失败抛 IOException 并返回 ZR_FALSE，目标对象创建失败返回 ZR_FALSE。overwrite 类型不符只返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：targetPath：必需 string；overwrite：可选 bool，默认 false。
 * 隐式 receiver：Folder；targetPath 必填；overwrite 可选且缺省 false。 源 receiver 仍绑定原路径；目标文件树完成复制后才创建返回对象。 TODO: targetPath 借自 ReadStringArgument 返回的 VM 实参；NewEntryObject 先经 Type_NewInstance 分配，再由 PopulateEntryObject 使用 originalPath。仍需核实 fast/inline/generic lane 的参数 pin、minor/major GC 迁移及分配窗口内指针有效期；尚未证实悬空。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_CopyTo；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 移动目录树到目标路径并返回新的 Folder 包装对象。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示移动完成并返回目标 Folder；移动失败抛 IOException 并返回 ZR_FALSE，目标对象创建失败返回 ZR_FALSE。overwrite 类型不符只返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：targetPath：必需 string；overwrite：可选 bool，默认 false。
 * 隐式 receiver：Folder；targetPath 必填；overwrite 可选且缺省 false。 源 receiver 不改绑目标路径；移动后原路径可能已经不存在。 TODO: targetPath 借自 ReadStringArgument 返回的 VM 实参；NewEntryObject 先经 Type_NewInstance 分配，再由 PopulateEntryObject 使用 originalPath。仍需核实 fast/inline/generic lane 的参数 pin、minor/major GC 迁移及分配窗口内指针有效期；尚未证实悬空。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_MoveTo；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 删除 Folder 对应目录并返回 null。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示删除完成并写入 null；宿主删除失败抛 IOException 并返回 ZR_FALSE。recursively 类型不符由 optional-bool helper 返回 ZR_FALSE，不抛 VM 类型异常；成功后尝试刷新 receiver 快照。
 *
 * ## Constraints and lifecycle
 * VM 参数：recursively：可选 bool，默认 false。
 * 隐式 receiver：Folder；recursively 可选且缺省 false。 缺省只做非递归删除；显式 true 才请求递归删除；I/O 完成后尝试刷新 receiver 的 fileInfo 快照；刷新失败不回滚已完成 I/O。
 *
 * ## References
 * fs_entry.c / ZrSystem_Fs_Folder_Delete；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Folder_Delete(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 从 FileStream 当前字节位置读取数据并返回字节数组。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示读取成功并返回 byte array；closed/non-readable 或读取失败抛相应 IOException 并返回 ZR_FALSE。optional count 经 ReadInt 读取，类型错误走 VM 整数读取异常路径。
 *
 * ## Constraints and lifecycle
 * VM 参数：count：可选 int，默认 -1（由读取 helper 解释为剩余数据）；无效 receiver 返回 ZR_FALSE。
 * 隐式 receiver：FileStream；count 可选且缺省 -1，表示读到剩余内容。 流须保持打开且可读；读取后查询位置/长度并同步公开字段；若同步查询失败仍抛 IOException，已读字节与游标推进不回滚。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_ReadBytes；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 从 FileStream 当前字节位置读取文本。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示读取后按所得字节长度尝试创建 VM string 并返回；closed/non-readable 或读取失败抛相应 IOException 并返回 ZR_FALSE。count 类型错误走 ReadInt 的 VM 异常路径；读取 helper 先取得带显式长度的字节缓冲区，再经 SetString 按 C 字符串复制；遇到 NUL 时返回 VM string 会在该位置截断；本 callback 不保证验证 UTF-8 合法性。count 截断多字节序列的行为仍待专项验证。
 *
 * ## Constraints and lifecycle
 * VM 参数：count：可选 int，默认 -1（由读取 helper 解释为剩余数据）；单位是字节。
 * 隐式 receiver：FileStream；count 可选且缺省 -1，表示读到剩余内容。 count 按字节而非 UTF-8 字符计，可能切开多字节字符；对不完整 UTF-8 的公开约定仍待跨边界用例确认。流须打开且可读；读取后查询位置/长度并同步公开字段，若同步查询失败仍抛 IOException，已读字节与游标推进不回滚。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_ReadText；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 从 FileStream 当前字节位置写入字节数组并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示写入完成并返回写入字节数；closed/non-writable 或写入失败抛相应 IOException 并返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：bytes：必需 Array；无效 receiver 或参数返回 ZR_FALSE。
 * 隐式 receiver：FileStream；索引 0 为 bytes，元素表示 0..255 字节。 流须打开且可写；写入后查询位置/长度并同步公开字段；若同步查询失败仍抛 IOException，已写字节与游标推进不回滚；输入数组由 VM 持有。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_WriteBytes；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 从 FileStream 当前字节位置写入文本并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示写入完成并返回写入字节数；closed/non-writable 或写入失败抛相应 IOException 并返回 ZR_FALSE。text 类型错误由 ReadString helper 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * VM 参数：text：必需 string。
 * 隐式 receiver：FileStream；索引 0 为 text。 流须打开且可写；文本长度由 strlen 计算；嵌入 NUL 时仅写首个 NUL 前缀并返回该前缀的字节数，需要任意字节请使用 writeBytes。写入后查询位置/长度并同步公开字段；若同步查询失败仍抛 IOException，已写字节与游标推进不回滚。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_WriteText；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 刷新 FileStream 的缓冲数据到宿主文件层。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示句柄刷新完成并写入 null；closed 状态或宿主 flush 失败抛 IOException 并返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；刷新 FileStream receiver 的拥有句柄。
 * 隐式 receiver：FileStream；无显式实参。 流须保持打开；刷新不关闭或转移句柄。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_Flush；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_Flush(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按原点调整 FileStream 的字节位置并返回新绝对位置。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示定位完成并返回当前位置；closed 状态、非法 origin 或 seek 失败抛 IOException 并返回 ZR_FALSE。offset 走 ReadInt，类型错误走 VM 整数读取异常；origin 只接受 begin/current/end。
 *
 * ## Constraints and lifecycle
 * VM 参数：offset：必需 int；origin：可选 string，默认 begin。
 * 隐式 receiver：FileStream；offset 必填；origin 可选且缺省 begin，可取 begin/current/end。 流须保持打开；偏移以字节计，非法原点或平台定位失败走错误路径；定位后查询位置/长度并同步公开字段，若同步查询失败仍抛 IOException，已改变的位置不回滚。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_Seek；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_Seek(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 调整 FileStream 关联文件的字节长度。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示长度设置和字段同步成功并写入 null；closed/non-writable 或宿主失败抛 IOException 并返回 ZR_FALSE。length 走 ReadInt，类型错误走 VM 整数读取异常。
 *
 * ## Constraints and lifecycle
 * VM 参数：length：必需 int，单位为字节。
 * 隐式 receiver：FileStream；索引 0 为 length。 流须打开且可写；长度以字节计，负值由实现/平台错误路径拒绝；调整后查询位置/长度并同步公开字段，若同步查询失败仍抛 IOException，已完成的截断或扩展不回滚。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_SetLength；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_SetLength(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 关闭 FileStream 持有的 native handle，也供 using 自动清理。
 *
 * ## Parameters
 * @param[in] context 有效的 native 调用上下文及 VM state；receiver 与 VM 实参在本次调用中借用。
 * @param[out] result 常规 dispatcher 提供的可写 VM 结果槽；具体脚本返回值见 Returns。结果槽及 VM 值由 VM/调用框架管理。
 *
 * ## Returns
 * @return ZR_TRUE 表示 close 成功并写入 null；已经成功关闭时重复调用仍成功。宿主 close 失败抛 IOException 并返回 ZR_FALSE；失败时仅私有 handle 与 closed=false 保留，后续调用可进入重试路径。
 *
 * ## Constraints and lifecycle
 * VM 参数：无显式 VM 实参；关闭 FileStream receiver 拥有的句柄。
 * 隐式 receiver：FileStream；无显式实参。 显式 close 与 descriptor 自动关闭 meta hook 共用；成功关闭后重复调用幂等；TODO: 宿主 close 失败时底层句柄是否仍可重试，须按各平台 close 语义核实；finalizer 是兜底释放路径。
 *
 * ## References
 * fs_stream.c / ZrSystem_Fs_Stream_Close；descriptor 绑定见 fs_registry.c。
 */
TZrBool ZrSystem_Fs_Stream_Close(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_FS_H

/**
 * @fn TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
 *
 * ## Summary
 * 供 System.Fs.File.copyTo 的 native descriptor 使用；完成宿主复制后，刷新原 receiver 的元数据并发布绑定目标路径的新 File。
 *
 * ## Parameters
 * @param[in] context 有效且属于当前 VM/native 调用线程的上下文与 state；receiver 和实参在框架约定的调用期间借用，不由本接口释放。
 * @param[in,out] result 框架提供的可写 VM 结果槽；正常成功发布目标对象，失败不承诺统一清零或恢复旧槽值；对象和槽由 VM 管理。
 *
 * ## Returns
 * @return 正常返回 ZR_TRUE 仅在宿主 Copy、目标 File 创建及结果发布成功时发生；前置检查、宿主失败后的异常状态建立、创建/发布的普通失败返回 ZR_FALSE。分配/异常传播还可能不经普通返回离开。
 *
 * ## Constraints and lifecycle
 * 脚本签名为 copyTo/moveTo(targetPath: string, overwrite?: bool)，overwrite 缺省 false。descriptor 接受 1..2 个显式实参；回调还按 receiver、result、源 fullPath、必需字符串、可选 bool 的顺序短路检查。更早失败时后续类型/参数诊断不一定到达。
 * targetPath 指向 ReadStringArgument 返回的 VM 字符串存储；不是新分配给 C 调用者的缓冲区。没有可由调用者 free 的返回对象；目标对象由 VM/GC 管理，持久保留须使用框架的根/引用规则。
 * 宿主 Copy/Move 为 false 时立即消费 errno 建立 IOException/线程异常状态，普通路径返回 false；它不是保证每种失败均以普通 C 返回交付的接口。内存错误可 Throw/longjmp/C++ throw 或最终终止，不能概括为全部返回 false。
 * 宿主操作与随后 VM 包装/发布不是同一事务；后续分配或结果发布失败不会撤销已经复制/移动的磁盘内容。原 receiver 的路径字段不改绑目标；result 失败状态没有统一清零/回滚保证。
 * File/Folder 是包装类型；本回调没有先按宿主 stat 检查对应类型再调用库层。词法重叠保护、覆盖和部分完成范围继承库 Copy/Move，仍不提供物理别名、并发路径或持久化保证。
 * context/state、receiver、实参及结果槽必须满足 VM 的有效性和调用线程约束；少数 NULL/type 检查不验证任意非 NULL 地址。不得无同步地把同一 VM/结果槽作为并发调用资源使用。
 * 宿主成功后调用 RefreshEntryObject，但忽略其普通 false 返回，再创建目标 File；因此成功返回不保证原 receiver 的 fileInfo 已刷新。刷新自身也可能分配或沿异常路径离开。
 * TODO: zr_vm_lib_system/include/zr_vm_lib_system/fs.h:476、zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:254：targetPath 是 VM 实参字符串借用；File 回调的 RefreshEntryObject 可能分配，随后同一裸指针还用于 NewEntryObject。需沿 fast/inline/generic lane 的 argument pin、minor/major GC 迁移及两次分配窗口核实其有效期；当前静态链不构成已复现悬空 BUG。
 *
 * ## References
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:463
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:476
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:481
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:238
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:244
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:245
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:246
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:247
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:250
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:251
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:253
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:254
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:256
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:257
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:49
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:50
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:51
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:157
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:234
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:269
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:277
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:282
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:286
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:315
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:323
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:327
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:357
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:389
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:467
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:475
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:482
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:584
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:612
 * zr_vm_library/include/zr_vm_library/native_binding.h:355
 * zr_vm_library/include/zr_vm_library/native_binding.h:360
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:237
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:246
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:272
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:278
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1216
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1219
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1223
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1315
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1320
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1325
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:320
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:324
 * zr_vm_core/src/zr_vm_core/exception/exception.c:31
 * zr_vm_core/src/zr_vm_core/exception/exception.c:61
 * zr_vm_core/src/zr_vm_core/exception/exception.c:85
 * zr_vm_core/src/zr_vm_core/exception/exception.c:99
 * zr_vm_core/src/zr_vm_core/exception/exception.c:100
 * zr_vm_core/src/zr_vm_core/exception/exception_internal.h:9
 * zr_vm_core/src/zr_vm_core/exception/exception_internal.h:18
 * tests/system/test_system_fs_module.c:733
 * tests/system/test_system_fs_module.c:757
 * tests/system/test_system_fs_module.c:759
 * tests/system/test_system_fs_module.c:843
 * tests/system/test_system_fs_module.c:846
 * tests/system/test_system_fs_module.c:848
 * tests/system/test_system_fs_module.c:849
 * tests/system/test_system_fs_module.c:851
 * tests/system/test_system_fs_module.c:852
 * tests/system/test_system_fs_module.c:873
 * tests/system/test_system_fs_module.c:882
 * tests/system/test_system_fs_module.c:1519
 * tests/system/test_system_fs_module.c:1523
 * tests/system/test_system_fs_module.c:1524
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
 *
 * ## Summary
 * 供 System.Fs.File.moveTo 的 native descriptor 使用；完成宿主移动后，刷新原 receiver 的元数据并发布绑定目标路径的新 File，原 receiver 不改绑。
 *
 * ## Parameters
 * @param[in] context 有效且属于当前 VM/native 调用线程的上下文与 state；receiver 和实参在框架约定的调用期间借用，不由本接口释放。
 * @param[in,out] result 框架提供的可写 VM 结果槽；正常成功发布目标对象，失败不承诺统一清零或恢复旧槽值；对象和槽由 VM 管理。
 *
 * ## Returns
 * @return 正常返回 ZR_TRUE 仅在宿主 Move、目标 File 创建及结果发布成功时发生；前置检查或创建/发布的普通失败返回 ZR_FALSE，宿主 Move 失败交给 IOException 路径。异常传播可能不普通返回。
 *
 * ## Constraints and lifecycle
 * 脚本签名为 copyTo/moveTo(targetPath: string, overwrite?: bool)，overwrite 缺省 false。descriptor 接受 1..2 个显式实参；回调还按 receiver、result、源 fullPath、必需字符串、可选 bool 的顺序短路检查。更早失败时后续类型/参数诊断不一定到达。
 * targetPath 指向 ReadStringArgument 返回的 VM 字符串存储；不是新分配给 C 调用者的缓冲区。没有可由调用者 free 的返回对象；目标对象由 VM/GC 管理，持久保留须使用框架的根/引用规则。
 * 宿主 Copy/Move 为 false 时立即消费 errno 建立 IOException/线程异常状态，普通路径返回 false；它不是保证每种失败均以普通 C 返回交付的接口。内存错误可 Throw/longjmp/C++ throw 或最终终止，不能概括为全部返回 false。
 * 宿主操作与随后 VM 包装/发布不是同一事务；后续分配或结果发布失败不会撤销已经复制/移动的磁盘内容。原 receiver 的路径字段不改绑目标；result 失败状态没有统一清零/回滚保证。
 * File/Folder 是包装类型；本回调没有先按宿主 stat 检查对应类型再调用库层。词法重叠保护、覆盖和部分完成范围继承库 Copy/Move，仍不提供物理别名、并发路径或持久化保证。
 * context/state、receiver、实参及结果槽必须满足 VM 的有效性和调用线程约束；少数 NULL/type 检查不验证任意非 NULL 地址。不得无同步地把同一 VM/结果槽作为并发调用资源使用。
 * 宿主成功后调用 RefreshEntryObject，但忽略其普通 false 返回，再创建目标 File；因此成功返回不保证原 receiver 的 fileInfo 已刷新。刷新自身也可能分配或沿异常路径离开。
 * 移动后原 receiver 仍描述旧路径，旧路径可以已消失；返回的新 File 才绑定目标路径。
 * TODO: zr_vm_lib_system/include/zr_vm_lib_system/fs.h:495、zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:277：targetPath 是 VM 实参字符串借用；File 回调的 RefreshEntryObject 可能分配，随后同一裸指针还用于 NewEntryObject。需沿 fast/inline/generic lane 的 argument pin、minor/major GC 迁移及两次分配窗口核实其有效期；当前静态链不构成已复现悬空 BUG。
 *
 * ## References
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:482
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:495
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:500
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:261
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:267
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:268
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:269
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:270
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:273
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:274
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:276
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:277
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:279
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:280
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:49
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:160
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:269
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:277
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:315
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:389
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:467
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:475
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:482
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:584
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:612
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1320
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322
 * zr_vm_core/src/zr_vm_core/exception/exception.c:61
 * zr_vm_core/src/zr_vm_core/exception/exception.c:85
 * tests/system/test_system_fs_module.c:759
 * tests/system/test_system_fs_module.c:760
 * tests/system/test_system_fs_module.c:843
 * tests/system/test_system_fs_module.c:848
 * tests/system/test_system_fs_module.c:849
 * tests/system/test_system_fs_module.c:1523
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
 *
 * ## Summary
 * 供 System.Fs.Folder.copyTo 的 native descriptor 使用；完成宿主复制后发布目标 Folder，本入口不刷新原 Folder 的快照。
 *
 * ## Parameters
 * @param[in] context 有效且属于当前 VM/native 调用线程的上下文与 state；receiver 和实参在框架约定的调用期间借用，不由本接口释放。
 * @param[in,out] result 框架提供的可写 VM 结果槽；正常成功发布目标对象，失败不承诺统一清零或恢复旧槽值；对象和槽由 VM 管理。
 *
 * ## Returns
 * @return 正常返回 ZR_TRUE 仅在宿主 Copy 与目标 Folder 创建/发布成功时发生；前置检查、宿主 I/O 错误或普通包装失败返回 ZR_FALSE，VM 分配/异常仍可能非局部离开。
 *
 * ## Constraints and lifecycle
 * 脚本签名为 copyTo/moveTo(targetPath: string, overwrite?: bool)，overwrite 缺省 false。descriptor 接受 1..2 个显式实参；回调还按 receiver、result、源 fullPath、必需字符串、可选 bool 的顺序短路检查。更早失败时后续类型/参数诊断不一定到达。
 * targetPath 指向 ReadStringArgument 返回的 VM 字符串存储；不是新分配给 C 调用者的缓冲区。没有可由调用者 free 的返回对象；目标对象由 VM/GC 管理，持久保留须使用框架的根/引用规则。
 * 宿主 Copy/Move 为 false 时立即消费 errno 建立 IOException/线程异常状态，普通路径返回 false；它不是保证每种失败均以普通 C 返回交付的接口。内存错误可 Throw/longjmp/C++ throw 或最终终止，不能概括为全部返回 false。
 * 宿主操作与随后 VM 包装/发布不是同一事务；后续分配或结果发布失败不会撤销已经复制/移动的磁盘内容。原 receiver 的路径字段不改绑目标；result 失败状态没有统一清零/回滚保证。
 * File/Folder 是包装类型；本回调没有先按宿主 stat 检查对应类型再调用库层。词法重叠保护、覆盖和部分完成范围继承库 Copy/Move，仍不提供物理别名、并发路径或持久化保证。
 * context/state、receiver、实参及结果槽必须满足 VM 的有效性和调用线程约束；少数 NULL/type 检查不验证任意非 NULL 地址。不得无同步地把同一 VM/结果槽作为并发调用资源使用。
 * 此回调没有 RefreshEntryObject 步骤；复制后原 Folder 仍保留源路径/快照描述，不由本回调刷新。仅创建新的目标 Folder 包装对象。
 * TODO: zr_vm_lib_system/include/zr_vm_lib_system/fs.h:629、zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:475：targetPath 借自 VM 实参；NewEntryObject 在 PopulateEntryObject 使用 originalPath 前先经 Type_NewInstance 分配。需核实各调用 lane 的 pin、GC 迁移及分配窗口有效期；尚未用强制 GC 或 fault injection 证实悬空。
 *
 * ## References
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:616
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:629
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:634
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:315
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:321
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:322
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:323
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:324
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:327
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:328
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:330
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:331
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:49
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:180
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:269
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:277
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:315
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:389
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:467
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:475
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:482
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:486
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:496
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:509
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:513
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:523
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:539
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:573
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:579
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1320
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322
 * zr_vm_core/src/zr_vm_core/exception/exception.c:61
 * zr_vm_core/src/zr_vm_core/exception/exception.c:85
 * tests/system/test_system_fs_module.c:776
 * tests/system/test_system_fs_module.c:777
 * tests/system/test_system_fs_module.c:843
 * tests/system/test_system_fs_module.c:848
 * tests/system/test_system_fs_module.c:849
 * tests/system/test_system_fs_module.c:1523
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
 *
 * ## Summary
 * 供 System.Fs.Folder.moveTo 的 native descriptor 使用；完成宿主移动后发布目标 Folder，本入口不改绑或刷新原 Folder。
 *
 * ## Parameters
 * @param[in] context 有效且属于当前 VM/native 调用线程的上下文与 state；receiver 和实参在框架约定的调用期间借用，不由本接口释放。
 * @param[in,out] result 框架提供的可写 VM 结果槽；正常成功发布目标对象，失败不承诺统一清零或恢复旧槽值；对象和槽由 VM 管理。
 *
 * ## Returns
 * @return 正常返回 ZR_TRUE 仅在宿主 Move 与目标 Folder 创建/发布成功时发生；普通失败返回 ZR_FALSE 或沿 VM 异常路径离开，返回失败不表示宿主操作未发生。
 *
 * ## Constraints and lifecycle
 * 脚本签名为 copyTo/moveTo(targetPath: string, overwrite?: bool)，overwrite 缺省 false。descriptor 接受 1..2 个显式实参；回调还按 receiver、result、源 fullPath、必需字符串、可选 bool 的顺序短路检查。更早失败时后续类型/参数诊断不一定到达。
 * targetPath 指向 ReadStringArgument 返回的 VM 字符串存储；不是新分配给 C 调用者的缓冲区。没有可由调用者 free 的返回对象；目标对象由 VM/GC 管理，持久保留须使用框架的根/引用规则。
 * 宿主 Copy/Move 为 false 时立即消费 errno 建立 IOException/线程异常状态，普通路径返回 false；它不是保证每种失败均以普通 C 返回交付的接口。内存错误可 Throw/longjmp/C++ throw 或最终终止，不能概括为全部返回 false。
 * 宿主操作与随后 VM 包装/发布不是同一事务；后续分配或结果发布失败不会撤销已经复制/移动的磁盘内容。原 receiver 的路径字段不改绑目标；result 失败状态没有统一清零/回滚保证。
 * File/Folder 是包装类型；本回调没有先按宿主 stat 检查对应类型再调用库层。词法重叠保护、覆盖和部分完成范围继承库 Copy/Move，仍不提供物理别名、并发路径或持久化保证。
 * context/state、receiver、实参及结果槽必须满足 VM 的有效性和调用线程约束；少数 NULL/type 检查不验证任意非 NULL 地址。不得无同步地把同一 VM/结果槽作为并发调用资源使用。
 * 此回调没有 RefreshEntryObject 步骤；原 Folder 的路径/快照保留原描述，移动后该路径可以已不存在。仅创建新的目标 Folder 包装对象。
 * TODO: zr_vm_lib_system/include/zr_vm_lib_system/fs.h:648、zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:475：targetPath 借自 VM 实参；NewEntryObject 在 PopulateEntryObject 使用 originalPath 前先经 Type_NewInstance 分配。需核实各调用 lane 的 pin、GC 迁移及分配窗口有效期；尚未用强制 GC 或 fault injection 证实悬空。
 *
 * ## References
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:635
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:648
 * zr_vm_lib_system/include/zr_vm_lib_system/fs.h:653
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:334
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:340
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:341
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:342
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:343
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:346
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:347
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:349
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:350
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:49
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:183
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:269
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:277
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:315
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:389
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:467
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:475
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:482
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:486
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:513
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:539
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:579
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1320
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322
 * zr_vm_core/src/zr_vm_core/exception/exception.c:61
 * zr_vm_core/src/zr_vm_core/exception/exception.c:85
 * tests/system/test_system_fs_module.c:778
 * tests/system/test_system_fs_module.c:779
 * tests/system/test_system_fs_module.c:843
 * tests/system/test_system_fs_module.c:848
 * tests/system/test_system_fs_module.c:849
 * tests/system/test_system_fs_module.c:1523
 */
