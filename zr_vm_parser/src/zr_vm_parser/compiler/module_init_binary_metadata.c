#include "module_init_analysis.h"

#include "zr_vm_core/io.h"

/** @brief 从二进制流构建临时源树，供模块初始化摘要、导入类型元数据及 LSP 模块元数据共用。
 *  @note 借用并复制 IO 游标后绑定当前 state；流的 close 仍由上层调用方负责，成功源树须配对 Free。 */
TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(SZrState *state,
                                                                  const SZrIo *io,
                                                                  SZrIoSource **outSource) {
    SZrIo directIo;

    if (outSource != ZR_NULL) {
        *outSource = ZR_NULL;
    }
    /* BUG: read 为空但预装缓冲区非空时仍通过；截断输入耗尽缓冲区后 core 的 io_refill 会调用空 read。 */
    if (state == ZR_NULL || io == ZR_NULL || outSource == ZR_NULL ||
        (io->read == ZR_NULL && io->remained == 0)) {
        return ZR_FALSE;
    }

    directIo = *io;
    directIo.state = state;
    /* BUG: 底层读取器在版本拒绝或截断输入失败时可能返回 NULL 而泄漏已分配的源树；本层无法回收未返回的指针。 */
    *outSource = ZrCore_Io_ReadSourceNew(&directIo);
    return *outSource != ZR_NULL;
}

/** @brief 三条二进制元数据分析路径读取完临时源树后释放原生数组；源树内 GC 对象仍归 VM 管理。 */
void ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(SZrGlobalState *global, SZrIoSource *source) {
    ZrCore_Io_ReadSourceFree(global, source);
}
