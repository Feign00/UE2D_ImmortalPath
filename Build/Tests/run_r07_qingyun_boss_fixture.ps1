param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunName,
    [string]$EngineRoot = 'E:\UE_5.7',
    [ValidateRange(30, 3600)]
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $ProjectRoot 'ImmortalPath.uproject'
$Executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$TestRoot = Join-Path $ProjectRoot 'Saved\Automation\QingyunBossArt'
$RunDirectory = Join-Path $TestRoot $RunName
if (Test-Path -LiteralPath $RunDirectory) {
    throw 'Use a fresh RunName. Existing isolated evidence and saves will not be overwritten.'
}
if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'UnrealEditor.exe not found.' }
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
$Log = Join-Path $RunDirectory 'R07QingyunBoss.log'
$Arguments = @(('"' + $Project + '"'), '/Game/GAME/Maps/NewMap', '-game', '-dx11',
    '-windowed', '-ResX=1920', '-ResY=320', '-unattended', '-nop4', '-nosplash', '-nosound',
    ('-UserDir="' + $RunDirectory + '"'), ('-abslog="' + $Log + '"'),
    '-ImmortalTestQingyunBossArt', '-ImmortalTestDisableAutoBattle', '-FixedSeed', '-ImmortalTestStage=10')
$Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $ProjectRoot `
    -WindowStyle Hidden -PassThru
Write-Output ("Controlled real map-Boss run started: pid={0}, directory={1}" -f $Process.Id, $RunDirectory)
$Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (!$Process.HasExited) {
    if ([DateTime]::UtcNow -ge $Deadline) {
        # This Process instance identifies only the application started above.
        # Never enumerate or stop unrelated Unreal Editor/game processes.
        $Process.Refresh()
        if (!$Process.HasExited) {
            $Process.Kill()
            if (!$Process.WaitForExit(10000)) {
                throw ("Boss fixture pid={0} timed out after {1}s and did not exit after termination. Evidence: {2}" -f
                    $Process.Id, $TimeoutSeconds, $RunDirectory)
            }
        }
        throw ("Boss fixture pid={0} timed out after {1}s. Existing log/screenshots are preserved: {2}" -f
            $Process.Id, $TimeoutSeconds, $RunDirectory)
    }
    Start-Sleep -Milliseconds 250
    $Process.Refresh()
}
$Process.Refresh()
if (!(Test-Path -LiteralPath $Log -PathType Leaf)) { throw 'Application produced no fixture log.' }
$Results = @(Select-String -LiteralPath $Log -SimpleMatch 'R07QingyunBossFixture finished:')
$FailedChecks = @(Select-String -LiteralPath $Log -Pattern 'R07QingyunBossFixture check \d+: FAIL')
$CheckLines = @(Select-String -LiteralPath $Log -Pattern 'R07QingyunBossFixture check \d+: (PASS|FAIL)')
$ExpectedNames = @('Idle', 'MoveLeft', 'MoveRight', 'AttackContact', 'AttackRecovery',
    'HurtMid', 'HurtHeld', 'DeathEarly', 'DeathHeld', 'PhaseOne', 'PhaseTwo', 'PhaseThree')
$Screenshots = @(Get-ChildItem -LiteralPath $RunDirectory -Filter 'R07_QingyunBoss_*.png' -File)
$Missing = @($ExpectedNames | Where-Object {
    !(Test-Path -LiteralPath (Join-Path $RunDirectory ('R07_QingyunBoss_' + $_ + '.png')) -PathType Leaf)
})
$SummaryChecks = 0
$SummaryPass = $Results.Count -eq 1 -and $Results[0].Line -match 'checks=(\d+) failures=0 screenshotsRequested=12 RESULT: PASS'
if ($SummaryPass) { $SummaryChecks = [int]$Matches[1] }
$Passed = $Process.ExitCode -eq 0 -and $SummaryPass -and $FailedChecks.Count -eq 0 `
    -and $CheckLines.Count -eq $SummaryChecks -and $SummaryChecks -gt 100 `
    -and $Screenshots.Count -eq 12 -and $Missing.Count -eq 0
Write-Output ("Application exit={0}; checks={1}/{2}; screenshots={3}/12; missing={4}" -f
    $Process.ExitCode, $CheckLines.Count, $SummaryChecks, $Screenshots.Count, ($Missing -join ','))
foreach ($Result in $Results) { Write-Output $Result.Line }
if (!$Passed) { throw 'Boss fixture failed or evidence is incomplete. Exit zero alone is not a pass.' }
Write-Output 'This verifies a controlled real map Boss, not natural Walking or Windows transparent composition.'
