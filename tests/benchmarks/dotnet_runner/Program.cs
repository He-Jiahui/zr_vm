using BenchmarkRunner;

var argsList = Environment.GetCommandLineArgs().Skip(1).ToArray();
string? caseName = null;
var runnerArgs = new List<string>();

for (var index = 0; index < argsList.Length; index++)
{
    switch (argsList[index])
    {
        case "--case":
            if (index + 1 >= argsList.Length)
            {
                Console.Error.WriteLine("--case requires a benchmark name");
                return 1;
            }

            caseName = argsList[++index];
            break;
        case "--tier":
            if (index + 1 >= argsList.Length)
            {
                Console.Error.WriteLine("--tier requires smoke, core, stress, or profile");
                return 1;
            }

            runnerArgs.Add(argsList[index]);
            runnerArgs.Add(argsList[++index]);
            break;
        case "--scale":
            if (index + 1 >= argsList.Length)
            {
                Console.Error.WriteLine("--scale requires a positive integer");
                return 1;
            }

            runnerArgs.Add(argsList[index]);
            runnerArgs.Add(argsList[++index]);
            break;
        default:
            Console.Error.WriteLine($"unknown argument: {argsList[index]}");
            return 1;
    }
}

if (string.IsNullOrEmpty(caseName))
{
    Console.Error.WriteLine("--case is required");
    return 1;
}

var scale = BenchmarkSupport.ParseScale(runnerArgs.ToArray(), out var scaleError);
if (!string.IsNullOrEmpty(scaleError))
{
    Console.Error.WriteLine(scaleError);
    return 1;
}

var cases = new Dictionary<string, BenchmarkCaseDescriptor>(StringComparer.Ordinal)
{
    [NumericLoopsCase.Name] = new(NumericLoopsCase.Name, NumericLoopsCase.PassBanner, NumericLoopsCase.Run),
    [DispatchLoopsCase.Name] = new(DispatchLoopsCase.Name, DispatchLoopsCase.PassBanner, DispatchLoopsCase.Run),
    [ContainerPipelineCase.Name] = new(ContainerPipelineCase.Name, ContainerPipelineCase.PassBanner, ContainerPipelineCase.Run),
    [SortArrayCase.Name] = new(SortArrayCase.Name, SortArrayCase.PassBanner, SortArrayCase.Run),
    [PrimeTrialDivisionCase.Name] = new(PrimeTrialDivisionCase.Name, PrimeTrialDivisionCase.PassBanner, PrimeTrialDivisionCase.Run),
    [MatrixAdd2dCase.Name] = new(MatrixAdd2dCase.Name, MatrixAdd2dCase.PassBanner, MatrixAdd2dCase.Run),
    [StringBuildCase.Name] = new(StringBuildCase.Name, StringBuildCase.PassBanner, StringBuildCase.Run),
    [MapObjectAccessCase.Name] = new(MapObjectAccessCase.Name, MapObjectAccessCase.PassBanner, MapObjectAccessCase.Run),
};

if (!cases.TryGetValue(caseName, out var descriptor))
{
    Console.Error.WriteLine($"unknown benchmark case: {caseName}");
    return 1;
}

Console.WriteLine(descriptor.PassBanner);
Console.WriteLine(descriptor.Run(scale));
return 0;
