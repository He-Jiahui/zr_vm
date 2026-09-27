#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_FRAME_READER_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_FRAME_READER_H

#include <stdio.h>

#include "zr_vm_language_server/conf.h"

/** @brief 将输入流结束、协议帧错误与 I/O 故障分别交给传输循环决定是否继续读取。 */
typedef enum EZrStdioFrameReadStatus {
    ZR_STDIO_FRAME_READ_OK = 0,
    ZR_STDIO_FRAME_READ_EOF,
    ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
    ZR_STDIO_FRAME_READ_PAYLOAD_TRUNCATED,
    ZR_STDIO_FRAME_READ_TOO_LARGE,
    ZR_STDIO_FRAME_READ_IO_ERROR,
} EZrStdioFrameReadStatus;

/** @brief 为单帧提供可收紧的资源预算；零值表示沿用服务端默认上限。 */
typedef struct SZrStdioFrameReaderLimits {
    TZrSize maxHeaderBytes;
    TZrSize maxHeaderCount;
    TZrSize maxMessageBytes;
} SZrStdioFrameReaderLimits;

/** @brief 给 stdio 输入线程及测试装载一致的协议与资源上限。 */
void ZrLanguageServer_StdioFrameReader_DefaultLimits(SZrStdioFrameReaderLimits *outLimits);
/**
 * @brief 从当前位置读取一个 LSP 帧，供输入线程在 JSON 解析前隔离传输错误。
 * @pre input、outPayload、outLength 非空；调用方独占流的读取顺序。
 * @note 成功时调用方负责 free(*outPayload)；失败时两个输出分别为 NULL 和 0。
 */
EZrStdioFrameReadStatus ZrLanguageServer_StdioFrameReader_Read(
        FILE *input,
        const SZrStdioFrameReaderLimits *limits,
        char **outPayload,
        TZrSize *outLength);
/** @brief 为传输循环的 stderr 诊断提供稳定的状态名称。 */
const char *ZrLanguageServer_StdioFrameReader_StatusName(EZrStdioFrameReadStatus status);

#endif
