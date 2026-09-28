/**
 * @file
 * @brief 语言服务的聚合入口，供嵌入者和测试同时取得 URI、增量解析、语义与 LSP 契约。
 * @note 按需包含子头文件可以缩小依赖；此头文件本身不建立或持有会话状态。
 */

#ifndef ZR_VM_LANGUAGE_SERVER_H
#define ZR_VM_LANGUAGE_SERVER_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/lsp_uri.h"
#include "zr_vm_language_server/symbol_table.h"
#include "zr_vm_language_server/reference_tracker.h"
#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_language_server/lsp_interface.h"

#endif //ZR_VM_LANGUAGE_SERVER_H
