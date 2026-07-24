#include <string.h>

#include "unity.h"
#include "zr_vm_library/project.h"

void setUp(void) {
}

void tearDown(void) {
}

static void assert_specifier_parses(const TZrChar *literal, SZrLibrary_ModuleSpecifier *outSpecifier) {
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(outSpecifier, 0, sizeof(*outSpecifier));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_Parse(literal,
                                                              outSpecifier,
                                                              error,
                                                              sizeof(error)),
                             error);
}

static void assert_specifier_rejected(const TZrChar *literal) {
    SZrLibrary_ModuleSpecifier specifier;
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(&specifier, 0, sizeof(specifier));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_ModuleSpecifier_Parse(literal, &specifier, error, sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);
}

static void test_module_specifier_classifies_absolute_domains_and_separator_equivalence(void) {
    SZrLibrary_ModuleSpecifier officialDot;
    SZrLibrary_ModuleSpecifier officialSlash;
    SZrLibrary_ModuleSpecifier nativeDot;
    SZrLibrary_ModuleSpecifier nativeSlash;
    SZrLibrary_ModuleSpecifier workspaceDot;
    SZrLibrary_ModuleSpecifier workspaceSlash;

    assert_specifier_parses("zr.task", &officialDot);
    assert_specifier_parses("zr/task", &officialSlash);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_OFFICIAL_NATIVE, officialDot.kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_OFFICIAL_NATIVE, officialDot.identity.domain);
    TEST_ASSERT_EQUAL_STRING("zr.task", officialDot.identity.segments);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleIdentity_Equals(&officialDot.identity, &officialSlash.identity));

    assert_specifier_parses("native:engine.render", &nativeDot);
    assert_specifier_parses("native:engine/render", &nativeSlash);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_REGISTERED_NATIVE, nativeDot.kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_REGISTERED_NATIVE, nativeDot.identity.domain);
    TEST_ASSERT_EQUAL_STRING("engine.render", nativeDot.identity.segments);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleIdentity_Equals(&nativeDot.identity, &nativeSlash.identity));

    assert_specifier_parses("engine.render", &workspaceDot);
    assert_specifier_parses("engine/render", &workspaceSlash);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE, workspaceDot.kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE, workspaceDot.identity.domain);
    TEST_ASSERT_EQUAL_STRING("engine.render", workspaceDot.identity.segments);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleIdentity_Equals(&workspaceDot.identity, &workspaceSlash.identity));
    TEST_ASSERT_FALSE(ZrLibrary_ModuleIdentity_Equals(&nativeDot.identity, &workspaceDot.identity));
}

static void test_module_specifier_resolves_relative_identity_without_changing_domain(void) {
    SZrLibrary_ModuleSpecifier childSpecifier;
    SZrLibrary_ModuleSpecifier parentSpecifier;
    SZrLibrary_ModuleSpecifier ancestorSpecifier;
    SZrLibrary_ModuleSpecifier dotAncestorSpecifier;
    SZrLibrary_ModuleSpecifier escapingSpecifier;
    SZrLibrary_ModuleSpecifier inplaceRelativeSpecifier;
    SZrLibrary_ModuleIdentity currentIdentity;
    SZrLibrary_ModuleIdentity resolvedIdentity;
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(&currentIdentity, 0, sizeof(currentIdentity));
    currentIdentity.domain = ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE;
    strcpy(currentIdentity.segments, "engine.render.canvas");
    assert_specifier_parses("./mesh", &childSpecifier);
    assert_specifier_parses("../mesh", &parentSpecifier);
    assert_specifier_parses("../../mesh", &ancestorSpecifier);
    assert_specifier_parses("...mesh", &dotAncestorSpecifier);
    assert_specifier_parses("../../../mesh", &escapingSpecifier);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_RELATIVE, childSpecifier.kind);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)childSpecifier.relativeParentLevels);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_RELATIVE, parentSpecifier.kind);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)parentSpecifier.relativeParentLevels);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ancestorSpecifier.relativeParentLevels);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)dotAncestorSpecifier.relativeParentLevels);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)escapingSpecifier.relativeParentLevels);

    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                        &childSpecifier,
                                                                        &resolvedIdentity,
                                                                        error,
                                                                        sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE, resolvedIdentity.domain);
    TEST_ASSERT_EQUAL_STRING("engine.render.mesh", resolvedIdentity.segments);

    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                        &parentSpecifier,
                                                                        &resolvedIdentity,
                                                                        error,
                                                                        sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_STRING("engine.mesh", resolvedIdentity.segments);

    strcpy(currentIdentity.segments, "engine.render.canvas.layer");
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                        &ancestorSpecifier,
                                                                        &resolvedIdentity,
                                                                        error,
                                                                        sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_STRING("engine.mesh", resolvedIdentity.segments);

    strcpy(currentIdentity.segments, "engine.render.canvas");
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                 &escapingSpecifier,
                                                                 &resolvedIdentity,
                                                                 error,
                                                                 sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);

    memset(&currentIdentity, 0, sizeof(currentIdentity));
    currentIdentity.domain = ZR_LIBRARY_MODULE_DOMAIN_PACKAGE;
    strcpy(currentIdentity.packageName, "math");
    strcpy(currentIdentity.segments, "graphics.canvas");
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                        &childSpecifier,
                                                                        &resolvedIdentity,
                                                                        error,
                                                                        sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_PACKAGE, resolvedIdentity.domain);
    TEST_ASSERT_EQUAL_STRING("math", resolvedIdentity.packageName);
    TEST_ASSERT_EQUAL_STRING("graphics.mesh", resolvedIdentity.segments);

    currentIdentity.packageName[0] = '\0';
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                 &childSpecifier,
                                                                 &resolvedIdentity,
                                                                 error,
                                                                 sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);

    strcpy(currentIdentity.packageName, "math");
    currentIdentity.segments[0] = '\0';
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                 &childSpecifier,
                                                                 &resolvedIdentity,
                                                                 error,
                                                                 sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);

    memset(&currentIdentity, 0, sizeof(currentIdentity));
    currentIdentity.domain = ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE;
    strcpy(currentIdentity.segments, "engine.render.canvas");
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                        &parentSpecifier,
                                                                        &currentIdentity,
                                                                        error,
                                                                        sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE, currentIdentity.domain);
    TEST_ASSERT_EQUAL_STRING("engine.mesh", currentIdentity.segments);

    memset(&currentIdentity, 0, sizeof(currentIdentity));
    currentIdentity.domain = ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE;
    strcpy(currentIdentity.segments, "engine.render.canvas");
    assert_specifier_parses("../mesh", &inplaceRelativeSpecifier);
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ModuleSpecifier_ResolveRelative(&currentIdentity,
                                                                        &inplaceRelativeSpecifier,
                                                                        &inplaceRelativeSpecifier.identity,
                                                                        error,
                                                                        sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE, inplaceRelativeSpecifier.identity.domain);
    TEST_ASSERT_EQUAL_STRING("engine.mesh", inplaceRelativeSpecifier.identity.segments);
}

static void test_module_specifier_parses_alias_and_single_segment_package_forms(void) {
    SZrLibrary_ModuleSpecifier aliasSpecifier;
    SZrLibrary_ModuleSpecifier packageRoot;
    SZrLibrary_ModuleSpecifier packageRootAgain;
    SZrLibrary_ModuleSpecifier packageDot;
    SZrLibrary_ModuleSpecifier packageSlash;

    assert_specifier_parses("#lib.tool", &aliasSpecifier);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_ALIAS, aliasSpecifier.kind);
    TEST_ASSERT_EQUAL_STRING("lib", aliasSpecifier.aliasRoot);
    TEST_ASSERT_EQUAL_STRING("tool", aliasSpecifier.identity.segments);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_INVALID, aliasSpecifier.identity.domain);

    assert_specifier_parses("@math", &packageRoot);
    assert_specifier_parses("@math", &packageRootAgain);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE, packageRoot.kind);
    TEST_ASSERT_EQUAL_STRING("math", packageRoot.identity.packageName);
    TEST_ASSERT_EQUAL_STRING("", packageRoot.identity.segments);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleIdentity_Equals(&packageRoot.identity, &packageRootAgain.identity));

    assert_specifier_parses("@math.matrix", &packageDot);
    assert_specifier_parses("@math/matrix", &packageSlash);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE, packageDot.kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_PACKAGE, packageDot.identity.domain);
    TEST_ASSERT_EQUAL_STRING("math", packageDot.identity.packageName);
    TEST_ASSERT_EQUAL_STRING("matrix", packageDot.identity.segments);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleIdentity_Equals(&packageDot.identity, &packageSlash.identity));
}

static void test_module_specifier_classifies_canonical_file_locators_without_creating_identity(void) {
    SZrLibrary_ModuleSpecifier driveLocator;
    SZrLibrary_ModuleSpecifier posixLocator;
    SZrLibrary_ModuleSpecifier uncLocator;

    assert_specifier_parses("file:///E:/sdk/engine.zrp", &driveLocator);
    assert_specifier_parses("file:///opt/sdk/engine.zrm", &posixLocator);
    assert_specifier_parses("file://server/share/engine.zr", &uncLocator);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE, driveLocator.kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_INVALID, driveLocator.identity.domain);
    TEST_ASSERT_EQUAL_STRING("file:///E:/sdk/engine.zrp", driveLocator.locator);
    TEST_ASSERT_EQUAL_STRING("file:///opt/sdk/engine.zrm", posixLocator.locator);
    TEST_ASSERT_EQUAL_STRING("file://server/share/engine.zr", uncLocator.locator);
}

static void test_module_specifier_rejects_ambiguous_or_noncanonical_forms(void) {
    assert_specifier_rejected("native:");
    assert_specifier_rejected("native:zr.task");
    assert_specifier_rejected("engine..render");
    assert_specifier_rejected(".");
    assert_specifier_rejected("..");
    assert_specifier_rejected("#lib..tool");
    assert_specifier_rejected("@1math");
    assert_specifier_rejected("@math//matrix");
    assert_specifier_rejected("C:/sdk/engine.zr");
    assert_specifier_rejected("/opt/sdk/engine.zr");
    assert_specifier_rejected("\\\\server\\share\\engine.zr");
    assert_specifier_rejected("file://server");
    assert_specifier_rejected("file://server//share/engine.zr");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_module_specifier_classifies_absolute_domains_and_separator_equivalence);
    RUN_TEST(test_module_specifier_resolves_relative_identity_without_changing_domain);
    RUN_TEST(test_module_specifier_parses_alias_and_single_segment_package_forms);
    RUN_TEST(test_module_specifier_classifies_canonical_file_locators_without_creating_identity);
    RUN_TEST(test_module_specifier_rejects_ambiguous_or_noncanonical_forms);

    return UNITY_END();
}
