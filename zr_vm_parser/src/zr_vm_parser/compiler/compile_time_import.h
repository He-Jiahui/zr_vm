#ifndef ZR_VM_PARSER_COMPILE_TIME_IMPORT_H
#define ZR_VM_PARSER_COMPILE_TIME_IMPORT_H

#include "compiler_internal.h"

/** @brief 将来源模块的编译期声明接入当前编译器，供后续导入别名和函数解析复用。
 *  @pre cs、cs->state、cs->state->global 与 moduleName 有效；sourceBytes 在本次调用期间可读且 sourceByteCount 非零。
 *  @note 普通源码导入传入 canonicalizeImports=true；已解析的项目构建依赖传入 false。同名模块在当前编译器内复用，返回的模块由编译器持有；moduleName 须存活至该模块被恢复或编译器释放，输入字节只需存活至本次调用结束。
 *  @return 成功时返回编译器持有的模块；解析、构建事实或声明收集失败时返回 NULL，诊断可能写入 cs。
 *  BUG: 模块数组初始化和发布依赖不能报告分配失败的 ZrCore_Array_Init/Push；内存不足时无法按本接口约定返回 NULL，可能在数组写入处崩溃。
 *  TODO: 当前缓存只比较 moduleName；若同一编译器允许同名但不同来源的字节流，需核对来源身份或收紧调用契约。 */
ZR_PARSER_API SZrImportedCompileTimeModule *
ZrParser_CompileTimeImport_LoadSourceModule(
        SZrCompilerState *cs,
        SZrString *moduleName,
        const TZrByte *sourceBytes,
        TZrSize sourceByteCount,
        TZrBool canonicalizeImports);
/** @brief 回滚当前编译器在 mark 之后接入的编译期模块，供项目提供者导入失败时撤销模块所有权。
 *  @pre mark 是同一 cs 的 importedCompileTimeModules 先前长度，且没有仍需使用被撤销模块的引用。
 *  @note 本函数只回滚模块；调用方须另行恢复模块别名、CompileTool 绑定和提供者，并先撤销指向待释放模块的别名。无效 cs 或越界 mark 不产生操作。 */
ZR_PARSER_API void ZrParser_CompileTimeImport_RestoreModules(
        SZrCompilerState *cs,
        TZrSize mark);
/** @brief 将已加载模块挂到当前编译器的别名空间；可按需公开其导出的无前缀编译期函数。
 *  @pre cs 及其 state 已初始化，aliasName 与 module 至少存活到别名被移除或编译器释放。
 *  @note 项目构建依赖目前只传 false，并在绑定失败时按保存的别名长度回滚。
 *  @return 参数无效或公开函数注册失败时返回 false；否则返回 true。
 *  TODO: exposeUnqualifiedFunctions=true 会先追加模块别名再逐个注册函数；未来调用方需要定义函数注册中途失败时的完整回滚契约，不能只缩短别名数组。 */
ZR_PARSER_API TZrBool ZrParser_CompileTimeImport_RegisterModuleAlias(
        SZrCompilerState *cs,
        SZrString *aliasName,
        SZrImportedCompileTimeModule *module,
        SZrFileRange location,
        TZrBool exposeUnqualifiedFunctions);

#endif
