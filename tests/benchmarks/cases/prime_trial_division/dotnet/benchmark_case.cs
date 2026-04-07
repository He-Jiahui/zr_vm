namespace BenchmarkRunner;

internal static class PrimeTrialDivisionCase
{
    public const string Name = "prime_trial_division";
    public const string PassBanner = "BENCH_PRIME_TRIAL_DIVISION_PASS";

    public static long Run(int scale) => BenchmarkSupport.PrimeTrialDivision(scale);
}
