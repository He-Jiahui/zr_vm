namespace BenchmarkRunner;

internal static class DispatchLoopsCase
{
    public const string Name = "dispatch_loops";
    public const string PassBanner = "BENCH_DISPATCH_LOOPS_PASS";

    public static long Run(int scale) => BenchmarkSupport.DispatchLoops(scale);
}
