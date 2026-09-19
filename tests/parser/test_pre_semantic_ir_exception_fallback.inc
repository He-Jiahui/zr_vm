static void test_unmodeled_try_scope_does_not_start_detached_call_cfg(void) {
    static const TZrChar inactiveSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error) { seed = 8; }\n"
            "finally { seed = 9; }\n";
    static TZrChar inactiveSourceName[] = "try_call_cfg_fallback.zr";
    static const TZrChar activeSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) { seed = 8; }\n"
            "try { identity(seed); }\n"
            "catch (error) { seed = 9; }\n";
    static TZrChar activeSourceName[] = "try_abandons_active_cfg.zr";
    static const TZrChar trailingCallSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { seed = 8; }\n"
            "catch (error) { seed = 9; }\n"
            "identity(seed);\n";
    static TZrChar trailingCallSourceName[] =
            "call_after_try_cfg_fallback.zr";
    static const TZrChar nestedTrailingCallSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) {\n"
            "    try { seed = 8; }\n"
            "    catch (error) { seed = 9; }\n"
            "}\n"
            "identity(seed);\n";
    static TZrChar nestedTrailingCallSourceName[] =
            "call_after_nested_try_cfg_fallback.zr";

    assert_nested_call_keeps_legacy_cfg(
            inactiveSource,
            sizeof(inactiveSource) - 1U,
            inactiveSourceName,
            sizeof(inactiveSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            activeSource,
            sizeof(activeSource) - 1U,
            activeSourceName,
            sizeof(activeSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            trailingCallSource,
            sizeof(trailingCallSource) - 1U,
            trailingCallSourceName,
            sizeof(trailingCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            nestedTrailingCallSource,
            sizeof(nestedTrailingCallSource) - 1U,
            nestedTrailingCallSourceName,
            sizeof(nestedTrailingCallSourceName) - 1U);
}
