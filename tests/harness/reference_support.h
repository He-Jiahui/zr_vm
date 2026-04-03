#ifndef ZR_VM_TESTS_REFERENCE_SUPPORT_H
#define ZR_VM_TESTS_REFERENCE_SUPPORT_H

#include "path_support.h"

TZrChar *ZrTests_Reference_ReadFixture(const TZrChar *relativePath, TZrSize *outLength);

TZrChar *ZrTests_Reference_ReadDoc(const TZrChar *relativePath, TZrSize *outLength);

TZrSize ZrTests_Reference_CountOccurrences(const TZrChar *text, const TZrChar *needle);

TZrSize ZrTests_Reference_CountJsonStringFieldValueOccurrences(const TZrChar *text,
                                                               const TZrChar *fieldName,
                                                               const TZrChar *value);

TZrBool ZrTests_Reference_TextContainsAll(const TZrChar *text,
                                          const TZrChar *const *needles,
                                          TZrSize needleCount);

TZrBool ZrTests_Reference_TextContainsInOrder(const TZrChar *text,
                                              const TZrChar *const *fragments,
                                              TZrSize fragmentCount);

TZrBool ZrTests_Reference_TextMatchesRegex(const TZrChar *text, const TZrChar *pattern);

void ZrTests_Reference_AssertManifestShape(const TZrChar *manifestText,
                                           const TZrChar *domainSlug,
                                           TZrSize minimumCases,
                                           const TZrChar *const *requiredFields,
                                           TZrSize requiredFieldCount);

void ZrTests_Reference_AssertCaseKindsCovered(const TZrChar *manifestText,
                                              const TZrChar *const *caseKinds,
                                              TZrSize caseKindCount,
                                              TZrSize minimumOccurrencesPerKind);

void ZrTests_Reference_AssertJsonStringFieldValueCoverage(const TZrChar *text,
                                                          const TZrChar *fieldName,
                                                          const TZrChar *const *values,
                                                          TZrSize valueCount,
                                                          TZrSize minimumOccurrencesPerValue);

void ZrTests_Reference_AssertFieldCoverage(const TZrChar *text,
                                           const TZrChar *const *fields,
                                           TZrSize fieldCount,
                                           TZrSize minimumOccurrencesPerField);

#endif
