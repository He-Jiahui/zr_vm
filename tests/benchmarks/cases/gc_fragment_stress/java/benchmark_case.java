final class GcFragmentStressCase {
    static final String NAME = "gc_fragment_stress";
    static final String PASS_BANNER = "BENCH_GC_FRAGMENT_STRESS_PASS";

    private GcFragmentStressCase() {}

    static long run(int scale) {
        return BenchmarkSupport.gcFragmentStress(scale);
    }
}
