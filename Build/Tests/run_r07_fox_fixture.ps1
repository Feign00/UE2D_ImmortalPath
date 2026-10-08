param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Controlled', 'Walking')]
    [string]$Mode,
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunName,
    [string]$EngineRoot = 'E:\UE_5.7',
    [switch]$LogMonsterRanges
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $ProjectRoot 'ImmortalPath.uproject'
$Executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$TestRoot = Join-Path $ProjectRoot 'Saved\Automation\FoxArt'
$RunDirectory = Join-Path $TestRoot $RunName
if (Test-Path -LiteralPath $RunDirectory) {
    throw 'Use a fresh RunName. Existing isolated saves and evidence will not be overwritten.'
}
if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'UnrealEditor.exe not found.' }
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
$Log = Join-Path $RunDirectory 'R07Fox.log'
$Arguments = @(('"' + $Project + '"'), '/Game/GAME/Maps/NewMap', '-game', '-dx11',
    '-windowed', '-ResX=1920', '-ResY=320', '-unattended', '-nop4', '-nosplash', '-nosound',
    ('-UserDir="' + $RunDirectory + '"'), ('-abslog="' + $Log + '"'),
    '-ImmortalTestPet=Fox', '-ImmortalTestStage=1', '-FixedSeed')
if ($LogMonsterRanges) {
    $Arguments += '-ExecCmds="GetAll ImmortalMonsterCharacter AttackRange"'
}
if ($Mode -eq 'Controlled') {
    $Arguments += @('-ImmortalTestFoxArt', '-ImmortalTestDisableAutoBattle')
    $Marker = 'R07FoxFixture finished:'
    $ExpectedScreenshots = 17
} else {
    $Arguments += '-ImmortalTestFoxWalking'
    $Marker = 'R07FoxWalking finished:'
    $ExpectedScreenshots = 3
}
$Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $ProjectRoot `
    -WindowStyle Hidden -PassThru
Write-Output ("Isolated {0} run started: pid={1}, directory={2}" -f $Mode, $Process.Id, $RunDirectory)
$Process.WaitForExit()
$Process.Refresh()
$Results = @(Select-String -LiteralPath $Log -SimpleMatch $Marker)
$Screenshots = @(Get-ChildItem -LiteralPath $RunDirectory -Filter 'R07_Fox_*.png' -File).Count
$Passed = $Process.ExitCode -eq 0 -and $Results.Count -eq 1 `
    -and $Results[0].Line -match 'failures=0 RESULT: PASS' -and $Screenshots -eq $ExpectedScreenshots
Write-Output ("Application exit={0}; screenshots={1}/{2}" -f $Process.ExitCode, $Screenshots, $ExpectedScreenshots)
foreach ($Result in $Results) { Write-Output $Result.Line }
if (!$Passed) { throw 'Fixture failed or evidence is incomplete. Application exit zero alone is not a pass.' }
