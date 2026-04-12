final class GcFragmentBaselineCase {
    static final String NAME = "gc_fragment_baseline";
    static final String PASS_BANNER = "BENCH_GC_FRAGMENT_BASELINE_PASS";

    private GcFragmentBaselineCase() {}

    static long run(int scale) {
        return BenchmarkSupport.gcFragmentBaseline(scale);
    }
}
