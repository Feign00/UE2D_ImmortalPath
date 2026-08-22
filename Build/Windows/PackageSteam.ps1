param(
    [string]$EngineRoot = "E:\UE_5.7",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..\..")
)
$ProjectFile = Join-Path $ProjectRoot "ImmortalPath.uproject"
$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$ReleaseRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $ProjectRoot "Releases\Windows")
)
$ArchiveDirectory = [System.IO.Path]::GetFullPath(
    (Join-Path $ReleaseRoot "ImmortalPath-$Version")
)

if (-not (Test-Path -LiteralPath $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

if (-not (Test-Path -LiteralPath $RunUAT)) {
    throw "RunUAT not found: $RunUAT"
}

$ReleasePrefix = $ReleaseRoot.TrimEnd('\') + '\'
if (-not $ArchiveDirectory.StartsWith(
    $ReleasePrefix,
    [System.StringComparison]::OrdinalIgnoreCase
)) {
    throw "Refusing to clean archive outside the release directory: $ArchiveDirectory"
}

if (Test-Path -LiteralPath $ArchiveDirectory) {
    Remove-Item -LiteralPath $ArchiveDirectory -Recurse -Force
}

New-Item -ItemType Directory -Path $ReleaseRoot -Force | Out-Null

& $RunUAT BuildCookRun `
    "-project=$ProjectFile" `
    -noP4 `
    -platform=Win64 `
    -clientconfig=Shipping `
    -build `
    -cook `
    -stage `
    -pak `
    -iostore `
    -compressed `
    -prereqs `
    -archive `
    "-archivedirectory=$ArchiveDirectory" `
    -utf8output `
    -nocompileeditor `
    -nodebuginfo `
    -NoUBA

if ($LASTEXITCODE -ne 0) {
    throw "Windows Shipping package failed with exit code $LASTEXITCODE."
}

Write-Host "Shipping package created at: $ArchiveDirectory"
