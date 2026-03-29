param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\\..")).Path
)

$ErrorActionPreference = "Stop"

Set-Location $RepoRoot

$codeFiles = git ls-files -- '*.h' '*.c' '*.hpp' '*.cpp'
$codeFiles = $codeFiles | Where-Object {
    $_ -notmatch '(^|/)third_party/' -and
    $_ -notlike 'build/*' -and
    $_ -notlike '.cache/*'
}

$headerFiles = $codeFiles | Where-Object { $_ -like '*.h' -or $_ -like '*.hpp' }

$violations = New-Object System.Collections.Generic.List[string]

function Add-ViolationSection {
    param(
        [string]$Title,
        [string[]]$Entries
    )

    if ($Entries -and $Entries.Count -gt 0) {
        $violations.Add($Title)
        foreach ($entry in ($Entries | Sort-Object -Unique)) {
            $violations.Add("  $entry")
        }
    }
}

function Get-MatchingLines {
    param(
        [string[]]$Paths,
        [string]$Pattern
    )

    $results = New-Object System.Collections.Generic.List[string]
    foreach ($path in $Paths) {
        if (-not (Test-Path $path)) {
            continue
        }
        $matches = Select-String -Path $path -Pattern $Pattern -AllMatches
        foreach ($match in $matches) {
            $results.Add(($path + ':' + $match.LineNumber + ':' + $match.Line.Trim()))
        }
    }
    return $results
}

function Get-DeclarationName {
    param(
        [string]$Line
    )

    if ($Line -notmatch 'ZR_[A-Z_]+_API' -and $Line -notmatch '^ZR_FORCE_INLINE') {
        return $null
    }

    $signature = $Line
    $braceIndex = $signature.IndexOf('{')
    if ($braceIndex -ge 0) {
        $signature = $signature.Substring(0, $braceIndex)
    }

    $parenIndex = $signature.IndexOf('(')
    if ($parenIndex -lt 0) {
        return $null
    }

    $beforeParen = $signature.Substring(0, $parenIndex)
    $parts = $beforeParen.Trim() -split '\s+'
    if ($parts.Count -eq 0) {
        return $null
    }

    return $parts[-1].Trim('*')
}

$legacyBaseAliases = @(
    'TChar',
    'TByte',
    'TBytePtr',
    'TUInt8',
    'TInt8',
    'TUInt16',
    'TInt16',
    'TUInt32',
    'TInt32',
    'TUInt64',
    'TInt64',
    'TFloat32',
    'TFloat',
    'TFloat64',
    'TDouble',
    'TBool',
    'TEnum',
    'TNativeString'
)

$legacyAliasEntries = New-Object System.Collections.Generic.List[string]
foreach ($alias in $legacyBaseAliases) {
    $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($alias) + '(?![A-Za-z0-9_])'
    foreach ($entry in (Get-MatchingLines -Paths $codeFiles -Pattern $pattern)) {
        $legacyAliasEntries.Add($entry)
    }
}
Add-ViolationSection -Title 'Legacy base aliases remain:' -Entries $legacyAliasEntries

Add-ViolationSection -Title 'Non-standard kZr macros remain:' -Entries (Get-MatchingLines -Paths $codeFiles -Pattern '\bkZr[A-Za-z0-9_]*\b')
Add-ViolationSection -Title 'Empty module stubs remain:' -Entries (Get-MatchingLines -Paths $codeFiles -Pattern '\bZR_EMPTY_MODULE\b|\bZR_EMPTY_FILE\b')

$publicCamelEntries = New-Object System.Collections.Generic.List[string]
$publicLowerSnakeEntries = New-Object System.Collections.Generic.List[string]

foreach ($path in $headerFiles) {
    $lineNumber = 0
    foreach ($line in Get-Content $path) {
        $lineNumber++
        $name = Get-DeclarationName -Line $line
        if (-not $name) {
            continue
        }

        $entry = $path + ':' + $lineNumber + ':' + $line.Trim()
        if ($name -cmatch '^Zr[A-Za-z0-9]+$') {
            $publicCamelEntries.Add($entry)
            continue
        }

        if ($line -match 'ZR_[A-Z_]+_API' -and $name -cmatch '^[a-z][a-z0-9_]+$') {
            $publicLowerSnakeEntries.Add($entry)
        }
    }
}

Add-ViolationSection -Title 'Pure CamelCase public APIs remain:' -Entries $publicCamelEntries
Add-ViolationSection -Title 'Lower snake public APIs remain:' -Entries $publicLowerSnakeEntries

if ($violations.Count -gt 0) {
    Write-Error ($violations -join [Environment]::NewLine)
}

Write-Output 'Naming audit passed.'
