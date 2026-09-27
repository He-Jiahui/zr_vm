#include "zr_vm_language_server_stdio_internal.h"

#include <math.h>
#include <stdint.h>

/** @brief 借出入站 JSON 对象的字段，供各请求处理器逐层校验协议形状。
 *  @return 缺字段或无效输入返回 NULL；结果由原 JSON 树持有，不能单独释放。 */
const cJSON *get_object_item(const cJSON *json, const char *key) {
    if (json == NULL || key == NULL) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive((cJSON *)json, key);
}

/** @brief 将协议数字转换为数组大小、文档版本或符号标识所用的 TZrSize。
 *  @return 空输出指针或非法数字返回 ZR_FALSE；失败时不改写输出，调用方据此拒绝请求或选用默认值。
 *  @note 仅接受可由 cJSON double 表示的非负整数，并在转换前排除类型上界。 */
TZrBool parse_size_value_strict(const cJSON *json, TZrSize *outValue) {
    /* SIZE_MAX rounds up in double on 64-bit hosts. Use the exact 2^N bound. */
    const double sizeLimit = (double)(ZR_MAX_SIZE / 2 + 1) * 2.0;
    double value;
    TZrSize parsed;

    if (!cJSON_IsNumber((cJSON *)json) || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    /* TODO: cJSON 已把 JSON 数字折叠为 double；超过 2^53 的十进制整数可能先被舍入，
     * 再以另一个有效整数通过校验。核查版本号与语义 ID 的协议精度要求，并用原始报文测试。 */
    value = json->valuedouble;
    if (!isfinite(value) || value < 0 || value >= sizeLimit) {
        return ZR_FALSE;
    }

    parsed = (TZrSize)value;
    if ((double)parsed != value) {
        return ZR_FALSE;
    }

    *outValue = parsed;
    return ZR_TRUE;
}

/* 位置字段只校验非负 int32 形状；需按文本解释坐标的调用方自行选择后续边界检查。 */
static TZrBool parse_position_value(const cJSON *json, TZrInt32 *outValue) {
    double value;
    TZrInt32 parsed;

    if (!cJSON_IsNumber((cJSON *)json) || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = json->valuedouble;
    if (!isfinite(value) || value < 0 || value > (double)INT32_MAX) {
        return ZR_FALSE;
    }

    parsed = (TZrInt32)value;
    if ((double)parsed != value) {
        return ZR_FALSE;
    }

    *outValue = parsed;
    return ZR_TRUE;
}

/** @brief 将可选协议数字映射为工作区文件事件所需的默认类别。
 *  @return 非法或缺失值返回 fallback；有效整数原样返回。 */
TZrSize parse_size_value(const cJSON *json, TZrSize fallback) {
    TZrSize value;

    if (!parse_size_value_strict(json, &value)) {
        return fallback;
    }
    return value;
}

/** @brief 验证客户端位置或补全项缓存位置的 LSP 数值形状。
 *  @return 成功返回 1；失败返回 0，此时输出可能已写入 line，调用方不得使用。
 *  @note 调用方按坐标来源决定是否继续用 parse_position_for_uri 校验文本和客户端编码边界。 */
int parse_position(const cJSON *json, SZrLspPosition *outPosition) {
    const cJSON *line;
    const cJSON *character;

    if (json == NULL || outPosition == NULL) {
        return 0;
    }

    line = get_object_item(json, ZR_LSP_FIELD_LINE);
    character = get_object_item(json, ZR_LSP_FIELD_CHARACTER);
    if (!parse_position_value(line, &outPosition->line) ||
        !parse_position_value(character, &outPosition->character)) {
        return 0;
    }
    return 1;
}

/** @brief 拒绝反向或无效的 LSP range，给内容修改与导航提供共同的形状检查。
 *  @return 成功返回 1；失败返回 0，此时输出可能只写入部分端点，调用方不得使用。
 *  @note 上层按具体请求转换客户端坐标；内容变更路径还会按实际文本检查边界。 */
int parse_range(const cJSON *json, SZrLspRange *outRange) {
    const cJSON *start;
    const cJSON *end;

    if (json == NULL || outRange == NULL) {
        return 0;
    }

    start = get_object_item(json, ZR_LSP_FIELD_START);
    end = get_object_item(json, ZR_LSP_FIELD_END);
    if (!parse_position(start, &outRange->start) || !parse_position(end, &outRange->end)) {
        return 0;
    }

    if (outRange->start.line > outRange->end.line ||
        (outRange->start.line == outRange->end.line &&
         outRange->start.character > outRange->end.character)) {
        return 0;
    }

    return 1;
}
