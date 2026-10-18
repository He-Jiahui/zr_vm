#include "artifact/artifact_exec_ir.h"

TZrBool ZrParser_ExecIr_Read(SZrExecIrReader *reader,
                             SZrArtifactExecIrView *module,
                             SZrArtifactExecIrDiagnostic *diagnostic) {
    if (reader == ZR_NULL || module == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)(ZrCore_ArtifactExecIr_Read(reader->bytes, reader->length,
                                                 module, diagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK);
}

TZrBool ZrParser_ExecIr_Write(const SZrArtifactExecIrDocument *module,
                              SZrExecIrWriter *writer,
                              SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrUInt32 written = 0u;
    if (module == ZR_NULL || writer == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrCore_ArtifactExecIr_Write(module, writer->bytes, writer->capacity,
                                    &written, diagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK) {
        writer->written = 0u;
        return ZR_FALSE;
    }
    writer->written = written;
    return ZR_TRUE;
}

/**
 * @fn TZrBool ZrParser_ExecIr_Read(SZrExecIrReader *reader, SZrArtifactExecIrView *module, SZrArtifactExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把Core ERI1读取状态收敛为布尔结果，保留Core视图的借用语义。 由内存schema读取调用；不打开路径或解析重定位回调。
 *
 * ## Parameters
 * @param[in] reader 借用非空字节范围，字段不会被修改。
 * @param[out] module 非空视图存储；早期参数拒绝保持旧值，进入Core后的失败由Core清零。
 * @param[out] diagnostic 可空诊断；只在实际调用Core时由Core写入。
 *
 * ## Returns
 * @return Core状态恰为OK时返回TRUE；其他路径FALSE，不能据FALSE断言诊断已更新。
 *
 * ## Constraints and lifecycle
 * 先拒绝空reader/module，且不更新任何输出。否则直接传入bytes/length；Core在发布前验证格式与哈希，其失败清零module。成功不复制payload，reader及字节必须在视图使用期间有效，输出和输入不得别名；该包装器没有全局状态或锁。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:3; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:6; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:9; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:95; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:100; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:158; tests/library/test_ssa_schema_relocation.c:91
 */

/**
 * @fn TZrBool ZrParser_ExecIr_Write(const SZrArtifactExecIrDocument *module, SZrExecIrWriter *writer, SZrArtifactExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 调用Core ERI1编码器并将局部长度提交到writer->written。 由调用者准备借用文档和独立输出缓冲区后调用。
 *
 * ## Parameters
 * @param[in] module 非空借用文档；Core负责格式、容量及期望哈希预检。
 * @param[in,out] writer 提供bytes/capacity并接收written；描述及目标存储由调用者保有。
 * @param[out] diagnostic 可空Core诊断；空参数早退保持原值。
 *
 * ## Returns
 * @return TRUE时written等于Core成功长度；FALSE时只有已进入Core的分支保证written为0。本包装器不额外回滚目的缓冲区。
 *
 * ## Constraints and lifecycle
 * 局部written先置0；空module/writer直接FALSE，旧writer字段不变。Core失败明确写writer->written=0；成功才提交实际长度。源文档和节数据与目标缓冲区应分离并保持有效；不分配/释放调用者存储，不为共享writer提供同步。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:14; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:17; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:18; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:21; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:24; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:27; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:68; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:85; tests/library/test_ssa_schema_relocation.c:87; tests/library/test_ssa_schema_relocation.c:143
 */
