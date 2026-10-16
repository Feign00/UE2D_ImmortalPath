param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunName,
    [string[]]$Case = @(),
    [string]$EngineRoot = 'E:\UE_5.7',
    [ValidateRange(30, 3600)]
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $ProjectRoot 'ImmortalPath.uproject'
$Executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$SaveDirectory = Join-Path $ProjectRoot 'Saved\SaveGames'
$RunDirectory = Join-Path $ProjectRoot ('Saved\Automation\MalformedSave\' + $RunName)
$CaseNames = @(
    'Ascii43', 'Zero64', 'BadMagic', 'HeaderTruncated', 'FileVersionZero', 'FileVersionFuture',
    'BranchLengthMin', 'BranchLengthHuge', 'BranchUnterminated', 'CustomFormatInvalid',
    'CustomCountNegative', 'CustomCountHuge', 'CustomGuidTruncated', 'ClassLengthMin',
    'ClassLengthHuge', 'ClassUnterminated', 'WrongClass', 'PropertyNameLengthMin',
    'PropertyNameLengthHuge', 'PropertyNameOverLimit', 'PropertyNameUnterminated',
    'TypeNameLengthHuge', 'TypeNameOverLimit', 'TypeInnerCountNegative', 'TypeInnerCountHuge',
    'PropertySizeNegative', 'PropertySizeHuge', 'PropertyGuidTruncated', 'ArrayCountNegative',
    'ArrayCountHuge', 'NestedNameLengthMin', 'NestedNameOverLimit',
    'ActualNameValueLengthMin', 'ActualNameValueOverLimit', 'NestedGuidTruncated',
    'MissingNone', 'TrailingGarbage'
)
$CompatibleCaseNames = @(
    'Schema1', 'Schema6', 'Schema15', 'Schema20', 'Schema22', 'Schema23', 'Schema24',
    'LegacyOmittedFields', 'UnknownRemovedField'
)
$CaseNames += $CompatibleCaseNames
$CaseNames += 'GeneratedSnapshotProtection'
if ($Case.Count -eq 0) { $Case = $CaseNames }
if (@($Case | Select-Object -Unique).Count -ne $Case.Count) { throw 'Duplicate case names are not allowed.' }
foreach ($Name in $Case) {
    if ($CaseNames -cnotcontains $Name) { throw ('Unknown or incorrectly cased fixture: ' + $Name) }
}
if (Test-Path -LiteralPath $RunDirectory) {
    throw 'Use a fresh RunName. Existing evidence and isolated saves will not be overwritten.'
}
if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'UnrealEditor-Cmd.exe not found.' }
if (!(Test-Path -LiteralPath $SaveDirectory -PathType Container)) { throw 'Official save directory is missing; cannot establish protection baseline.' }

$ProtectedAssets = @(
    (Join-Path $ProjectRoot 'Content\GAME\BP_DogMonster.uasset'),
    (Join-Path $ProjectRoot 'Content\GAME\BP_DesktopDogMonster.uasset'),
    (Join-Path $ProjectRoot 'Content\GAME\BP_DesktopQingyunBoss.uasset')
)
function Get-ProtectionFingerprint {
    # Enumerate again, rather than only rehashing the initial list. This also
    # detects added/deleted official save files and never opens them through UE.
    $Paths = @((Get-ChildItem -LiteralPath $SaveDirectory -File -Recurse).FullName) + $ProtectedAssets
    return (($Paths | Sort-Object | ForEach-Object {
        $File = Get-Item -LiteralPath $_
        $Hash = Get-FileHash -LiteralPath $_ -Algorithm SHA256
        '{0}|{1}|{2}' -f $File.FullName, $File.Length, $Hash.Hash
    }) -join "`n")
}
$OfficialSaveCount = @(Get-ChildItem -LiteralPath $SaveDirectory -File -Recurse).Count
if ($OfficialSaveCount -eq 0) { throw 'No official save files were found for the protection check.' }
$ProtectedCount = $OfficialSaveCount + $ProtectedAssets.Count
$Before = Get-ProtectionFingerprint
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
$Passed = 0
try {
    foreach ($Name in $Case) {
        $Directory = Join-Path $RunDirectory $Name
        $UserDirectory = Join-Path $Directory 'UserData'
        $ReportDirectory = Join-Path $Directory 'Report'
        $Log = Join-Path $Directory 'MalformedSave.log'
        New-Item -ItemType Directory -Path $UserDirectory | Out-Null
        $Compatible = $CompatibleCaseNames -ccontains $Name
        $Generated = $Name -ceq 'GeneratedSnapshotProtection'
        $TestPath = if ($Generated) { 'ImmortalPath.Save.GeneratedSnapshotProtection' } `
            elseif ($Compatible) { 'ImmortalPath.Save.CompatiblePayload.' + $Name } `
            else { 'ImmortalPath.Save.MalformedPayload.' + $Name }
        $Marker = if ($Generated) { 'R01GeneratedSave' } elseif ($Compatible) { 'R01CompatibleSave' } else { 'R01MalformedSave' }
        $Arguments = @(('"' + $Project + '"'), '-nullrhi', '-unattended', '-nop4',
            '-nosplash', '-nosound', '-NoLiveCoding',
            ('-UserDir="' + $UserDirectory + '"'), ('-abslog="' + $Log + '"'),
            # UE's test filter supports start/end anchors. Without both,
            # Schema1 would also run Schema15 and break one-case isolation.
            ('-ExecCmds="Automation RunTests ^' + $TestPath + '$"'),
            '-TestExit="Automation Test Queue Empty"',
            ('-ReportExportPath="' + $ReportDirectory + '"'))
        $Process = Start-Process -FilePath $Executable -ArgumentList $Arguments `
            -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru
        Write-Output ('R01 malformed-save case started: {0}; pid={1}' -f $Name, $Process.Id)
        $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (!$Process.HasExited) {
            if ([DateTime]::UtcNow -ge $Deadline) {
                $Process.Refresh()
                if (!$Process.HasExited) {
                    # Only terminate the exact process launched for this case.
                    $Process.Kill()
                    if (!$Process.WaitForExit(10000)) { throw ('Timed out and could not terminate own process: ' + $Name) }
                }
                throw ('Malformed-save case timed out: ' + $Name + '. Evidence is preserved in ' + $Directory)
            }
            Start-Sleep -Milliseconds 250
            $Process.Refresh()
        }
        $Process.Refresh()
        if (!(Test-Path -LiteralPath $Log -PathType Leaf)) { throw ('No application log: ' + $Name) }
        $ReportPath = Join-Path $ReportDirectory 'index.json'
        if (!(Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw ('No automation report: ' + $Name) }
        $Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
        $Tests = @($Report.tests)
        $Completions = @(Select-String -LiteralPath $Log -SimpleMatch ($Marker + ' case=' + $Name + ' completed:'))
        $Fatal = @(Select-String -LiteralPath $Log -Pattern 'Fatal error:|Assertion failed:|Unhandled Exception:|Ran out of memory|FName.*maximum size')
        $LeftoverSaves = @(Get-ChildItem -LiteralPath $UserDirectory -File -Recurse -Filter '*.sav')
        $ValidReport = $Report.succeeded -eq 1 -and $Report.succeededWithWarnings -eq 0 `
            -and $Report.failed -eq 0 -and $Report.notRun -eq 0 -and $Report.inProcess -eq 0 `
            -and $Tests.Count -eq 1 -and $Tests[0].fullTestPath -ceq $TestPath `
            -and $Tests[0].state -eq 'Success' -and $Tests[0].errors -eq 0 -and $Tests[0].warnings -eq 0
        if ($Process.ExitCode -ne 0 -or !$ValidReport -or $Completions.Count -ne 1 `
            -or $Fatal.Count -ne 0 -or $LeftoverSaves.Count -ne 0) {
            throw ('Malformed-save case failed: {0}; exit={1}; reportOK={2}; completion={3}; fatal={4}; leftoverSaves={5}. Evidence: {6}' -f
                $Name, $Process.ExitCode, $ValidReport, $Completions.Count, $Fatal.Count, $LeftoverSaves.Count, $Directory)
        }
        if ((Get-ProtectionFingerprint) -cne $Before) { throw ('Protected official files changed after case: ' + $Name) }
        ++$Passed
        Write-Output ('{0}: PASS; exit=0; report=1/1; warnings=0; fatal=0; leftoverSaves=0; protectedFiles={1}' -f $Name, $ProtectedCount)
    }
} finally {
    if ((Get-ProtectionFingerprint) -cne $Before) { throw 'Protected official save or Blueprint files changed. Evidence was preserved; no restoration was attempted.' }
}
Write-Output ('R01 save-validation subprocess matrix PASS: cases={0}/{1}; officialSaves={2}; assets={3}; protectedFiles={4}' -f
    $Passed, $Case.Count, $OfficialSaveCount, $ProtectedAssets.Count, $ProtectedCount)
