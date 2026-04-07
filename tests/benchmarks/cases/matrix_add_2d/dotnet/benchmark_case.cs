namespace BenchmarkRunner;

internal static class MatrixAdd2dCase
{
    public const string Name = "matrix_add_2d";
    public const string PassBanner = "BENCH_MATRIX_ADD_2D_PASS";

    public static long Run(int scale) => BenchmarkSupport.MatrixAdd2d(scale);
}
