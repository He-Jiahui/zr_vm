#ifndef ZR_VM_TESTS_REFERENCE_SUPPORT_H
#define ZR_VM_TESTS_REFERENCE_SUPPORT_H

#include "path_support.h"

/** @brief 读取 reference fixture；返回由调用方 free 的 NUL 结尾缓冲区。 */
TZrChar *ZrTests_Reference_ReadFixture(const TZrChar *relativePath, TZrSize *outLength);

/** @brief 读取仓库文档以检查公开语法约定；返回缓冲区由调用方释放。 */
TZrChar *ZrTests_Reference_ReadDoc(const TZrChar *relativePath, TZrSize *outLength);

/** @brief 统计非重叠文本片段，用于 fixture 和文档约定的轻量断言。 */
TZrSize ZrTests_Reference_CountOccurrences(const TZrChar *text, const TZrChar *needle);

/** @brief 统计规范 JSON 文本中的字符串字段值；此辅助函数不是通用 JSON 解析器。 */
TZrSize ZrTests_Reference_CountJsonStringFieldValueOccurrences(const TZrChar *text,
                                                               const TZrChar *fieldName,
                                                               const TZrChar *value);

/** @brief 检查所有必需片段出现，不要求其顺序。 */
TZrBool ZrTests_Reference_TextContainsAll(const TZrChar *text,
                                          const TZrChar *const *needles,
                                          TZrSize needleCount);

/** @brief 检查各片段按指定顺序出现，用于文档形状约定。 */
TZrBool ZrTests_Reference_TextContainsInOrder(const TZrChar *text,
                                              const TZrChar *const *fragments,
                                              TZrSize fragmentCount);

/** @brief 匹配测试用简化模式，仅支持 ^、$、. 和 * 等有限运算。 */
TZrBool ZrTests_Reference_TextMatchesRegex(const TZrChar *text, const TZrChar *pattern);

/** @brief 用文本断言 manifest 的域、最少用例数和必需字段。 */
void ZrTests_Reference_AssertManifestShape(const TZrChar *manifestText,
                                           const TZrChar *domainSlug,
                                           TZrSize minimumCases,
                                           const TZrChar *const *requiredFields,
                                           TZrSize requiredFieldCount);

/** @brief 逐类检查 manifest 的 case_kind 最少出现次数。 */
void ZrTests_Reference_AssertCaseKindsCovered(const TZrChar *manifestText,
                                              const TZrChar *const *caseKinds,
                                              TZrSize caseKindCount,
                                              TZrSize minimumOccurrencesPerKind);

/** @brief 逐值检查规范 JSON 文本中的字符串字段覆盖。 */
void ZrTests_Reference_AssertJsonStringFieldValueCoverage(const TZrChar *text,
                                                          const TZrChar *fieldName,
                                                          const TZrChar *const *values,
                                                          TZrSize valueCount,
                                                          TZrSize minimumOccurrencesPerValue);

/** @brief 逐字段检查文档或 fixture 中的文本覆盖。 */
void ZrTests_Reference_AssertFieldCoverage(const TZrChar *text,
                                           const TZrChar *const *fields,
                                           TZrSize fieldCount,
                                           TZrSize minimumOccurrencesPerField);

#endif
