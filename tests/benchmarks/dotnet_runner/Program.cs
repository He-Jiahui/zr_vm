using BenchmarkRunner;

var argsList = Environment.GetCommandLineArgs().Skip(1).ToArray();
string? caseName = null;
var runnerArgs = new List<string>();
var serverMode = false;

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
        case "--benchmark-server":
            serverMode = true;
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

if (serverMode)
{
    var tier = "core";
    var tierIndex = runnerArgs.IndexOf("--tier");
    if (tierIndex >= 0 && tierIndex + 1 < runnerArgs.Count)
    {
        tier = runnerArgs[tierIndex + 1];
    }
    Console.WriteLine($"READY benchmark-checksum-v1:{caseName}:{tier}");
    Console.Out.Flush();
    while (true)
    {
        var request = Console.ReadLine();
        if (request is null)
        {
            Console.Error.WriteLine("benchmark protocol input closed before STOP");
            return 2;
        }
        if (request == "STOP")
        {
            return 0;
        }
        var requestMatch = System.Text.RegularExpressions.Regex.Match(
            request,
            "^(WARMUP|RUN) ([1-9][0-9]*) ([1-9][0-9]*)$",
            System.Text.RegularExpressions.RegexOptions.CultureInvariant);
        if (!requestMatch.Success ||
            !int.TryParse(
                requestMatch.Groups[2].Value,
                System.Globalization.NumberStyles.None,
                System.Globalization.CultureInfo.InvariantCulture,
                out var requestIndex) ||
            !int.TryParse(
                requestMatch.Groups[3].Value,
                System.Globalization.NumberStyles.None,
                System.Globalization.CultureInfo.InvariantCulture,
                out var repetitions) || repetitions > 1048576)
        {
            Console.WriteLine("ERROR 0 malformed-request");
            Console.Out.Flush();
            return 3;
        }
        try
        {
            long? checksum = null;
            for (var repetition = 0; repetition < repetitions; repetition++)
            {
                var repetitionChecksum = descriptor.Run(scale);
                if (checksum.HasValue && repetitionChecksum != checksum.Value)
                {
                    Console.WriteLine($"ERROR {requestIndex} repetition-checksum-mismatch");
                    Console.Out.Flush();
                    return 5;
                }
                checksum = repetitionChecksum;
            }
            Console.WriteLine($"DONE {requestIndex} {checksum!.Value}");
            Console.Out.Flush();
        }
        catch (Exception exception)
        {
            Console.WriteLine($"ERROR {requestIndex} workload-exception");
            Console.Out.Flush();
            Console.Error.WriteLine(exception);
            return 4;
        }
    }
}

Console.WriteLine(descriptor.PassBanner);
Console.WriteLine(descriptor.Run(scale));
return 0;
