final class SortArrayCase {
    static final String NAME = "sort_array";
    static final String PASS_BANNER = "BENCH_SORT_ARRAY_PASS";

    private SortArrayCase() {}

    static long run(int scale) {
        return BenchmarkSupport.sortArray(scale);
    }
}
