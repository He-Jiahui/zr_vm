final class StringBuildCase {
    static final String NAME = "string_build";
    static final String PASS_BANNER = "BENCH_STRING_BUILD_PASS";

    private StringBuildCase() {}

    static long run(int scale) {
        return BenchmarkSupport.stringBuild(scale);
    }
}
