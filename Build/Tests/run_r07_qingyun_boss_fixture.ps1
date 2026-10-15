param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunName,
    [ValidateSet('Controlled', 'Walking')]
    [string]$Mode = 'Controlled',
    [string]$EngineRoot = 'E:\UE_5.7',
    [ValidateRange(30, 3600)]
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $ProjectRoot 'ImmortalPath.uproject'
$Executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$TestFolder = if ($Mode -eq 'Walking') { 'QingyunBossWalking' } else { 'QingyunBossArt' }
$TestRoot = Join-Path $ProjectRoot ('Saved\Automation\' + $TestFolder)
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
    '-FixedSeed', '-ImmortalTestStage=10')
if ($Mode -eq 'Walking') {
    $Arguments += '-ImmortalTestQingyunBossWalking'
    $Marker = 'R07QingyunBossWalking'
    $ExpectedNames = @('Walking_Move', 'Walking_Battle', 'Walking_Final')
    $MinimumChecks = 16
} else {
    $Arguments += @('-ImmortalTestQingyunBossArt', '-ImmortalTestDisableAutoBattle')
    $Marker = 'R07QingyunBossFixture'
    $ExpectedNames = @('Idle', 'MoveLeft', 'MoveRight', 'AttackContact', 'AttackRecovery',
        'HurtMid', 'HurtHeld', 'DeathEarly', 'DeathHeld', 'PhaseOne', 'PhaseTwo', 'PhaseThree')
    $MinimumChecks = 101
}
$ExpectedCount = $ExpectedNames.Count
$Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $ProjectRoot `
    -WindowStyle Hidden -PassThru
Write-Output ("{0} real map-Boss run started: pid={1}, directory={2}" -f $Mode, $Process.Id, $RunDirectory)
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
$Results = @(Select-String -LiteralPath $Log -SimpleMatch ($Marker + ' finished:'))
$FailedChecks = @(Select-String -LiteralPath $Log -Pattern ($Marker + ' check \d+: FAIL'))
$CheckLines = @(Select-String -LiteralPath $Log -Pattern ($Marker + ' check \d+: (PASS|FAIL)'))
$Screenshots = @(Get-ChildItem -LiteralPath $RunDirectory -Filter 'R07_QingyunBoss_*.png' -File)
$Missing = @($ExpectedNames | Where-Object {
    !(Test-Path -LiteralPath (Join-Path $RunDirectory ('R07_QingyunBoss_' + $_ + '.png')) -PathType Leaf)
})
$SummaryChecks = 0
$SummaryPass = $Results.Count -eq 1 -and $Results[0].Line -match ('checks=(\d+) failures=0 screenshotsRequested=' + $ExpectedCount + ' RESULT: PASS')
if ($SummaryPass) { $SummaryChecks = [int]$Matches[1] }
$Passed = $Process.ExitCode -eq 0 -and $SummaryPass -and $FailedChecks.Count -eq 0 `
    -and $CheckLines.Count -eq $SummaryChecks -and $SummaryChecks -ge $MinimumChecks `
    -and $Screenshots.Count -eq $ExpectedCount -and $Missing.Count -eq 0
Write-Output ("Application exit={0}; checks={1}/{2}; screenshots={3}/{4}; missing={5}" -f
    $Process.ExitCode, $CheckLines.Count, $SummaryChecks, $Screenshots.Count, $ExpectedCount, ($Missing -join ','))
foreach ($Result in $Results) { Write-Output $Result.Line }
if (!$Passed) { throw 'Boss fixture failed or evidence is incomplete. Exit zero alone is not a pass.' }
if ($Mode -eq 'Walking') {
    Write-Output 'This observes untouched natural map-Boss combat and Walking; source captures do not verify Windows transparent composition.'
} else {
    Write-Output 'This verifies a controlled real map Boss, not natural Walking or Windows transparent composition.'
}
