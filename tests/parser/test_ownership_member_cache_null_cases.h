#ifndef ZR_TEST_OWNERSHIP_MEMBER_CACHE_NULL_CASES_H
#define ZR_TEST_OWNERSHIP_MEMBER_CACHE_NULL_CASES_H

static void test_member_cache_cold_read_write_and_receiver_change(void) {
    pending_assert_script(
            "class Cell { pub var value: int; }\n"
            "fn access(cell: Cell, value: int): int { cell.value = value; return cell.value; }\n"
            "var first = new Cell(); var second = new Cell();\n"
            "return access(first, 11) + access(first, 13) + access(second, 17);\n",
            41);
}

#endif
