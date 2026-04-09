final class PrimeTrialDivisionCase {
    static final String NAME = "prime_trial_division";
    static final String PASS_BANNER = "BENCH_PRIME_TRIAL_DIVISION_PASS";

    private PrimeTrialDivisionCase() {}

    static long run(int scale) {
        return BenchmarkSupport.primeTrialDivision(scale);
    }
}
