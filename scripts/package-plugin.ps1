# Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
<#
.SYNOPSIS
    Package the ReactiveUIToolkit plugin per engine version — the LOCAL rail for what publish.yml's
    engine legs do in CI (MASTER_PLAN D-29; engine CI is default-unarmed, so this is the
    expected v1 path).

.DESCRIPTION
    For each engine install found (or passed), runs:
      RunUAT.bat BuildPlugin -Plugin=<ReactiveUIToolkit.uplugin> -Package=<staging> -TargetPlatforms=Win64 -Rocket -StrictIncludes
    then strips Binaries/ and Intermediate/ (Fab: "Epic's toolchain builds the binaries") and
    zips  dist/ReactiveUIToolkit-<VersionName>-UE<major.minor>.zip.

    -StrictIncludes is non-negotiable: marketplace builds compile with -DisableUnity -NoPCH,
    so include-leakage that works locally fails at Fab otherwise.

.PARAMETER EngineRoots  Engine install roots. Resolution chain when omitted: $env:UE_ROOT ->
                        .ruitk-local.json "engineRoot" -> every "C:\Program Files\Epic Games\UE_5.*"
                        (see CLAUDE.md "Machine-local paths").
.PARAMETER OutDir       Output directory for zips. Default: <repo>\dist
.EXAMPLE  .\scripts\package-plugin.ps1
.EXAMPLE  .\scripts\package-plugin.ps1 -EngineRoots "C:\Program Files\Epic Games\UE_5.6"
#>
[CmdletBinding()]
param(
    [string[]] $EngineRoots = @(),
    [string]   $OutDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$uplugin  = Join-Path $repoRoot 'Plugins\ReactiveUIToolkit\ReactiveUIToolkit.uplugin'
if (-not (Test-Path $uplugin)) { Write-Error "Not found: $uplugin"; exit 1 }
$versionName = (Get-Content $uplugin -Raw | ConvertFrom-Json).VersionName
if ([string]::IsNullOrWhiteSpace($OutDir)) { $OutDir = Join-Path $repoRoot 'dist' }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# Engine-root resolution chain (CLAUDE.md "Machine-local paths" — one chain, this order, and the
# error names every rung so nobody has to read the source to find the knob):
#   -EngineRoots  ->  $UE_ROOT  ->  .ruitk-local.json engineRoot  ->  Launcher installs.
# Packaging deliberately keeps enumerating ALL installs by default: a release ships one zip per
# engine version. The single-value rungs pin one install.
if ($EngineRoots.Count -eq 0 -and -not [string]::IsNullOrWhiteSpace($env:UE_ROOT)) {
    $EngineRoots = @($env:UE_ROOT)
}
if ($EngineRoots.Count -eq 0) {
    $localCfg = Join-Path $repoRoot '.ruitk-local.json'
    if (Test-Path $localCfg) {
        # PSObject.Properties guard: every key in that file is optional, and Set-StrictMode -Latest
        # throws on a missing property rather than yielding $null.
        $cfg = Get-Content $localCfg -Raw | ConvertFrom-Json
        if (($cfg.PSObject.Properties.Name -contains 'engineRoot') -and
            (-not [string]::IsNullOrWhiteSpace($cfg.engineRoot))) {
            $EngineRoots = @($cfg.engineRoot)
        }
    }
}
if ($EngineRoots.Count -eq 0) {
    $EngineRoots = @(Get-ChildItem 'C:\Program Files\Epic Games' -Directory -Filter 'UE_5.*' -ErrorAction SilentlyContinue |
                     Select-Object -ExpandProperty FullName)
}
if ($EngineRoots.Count -eq 0) {
    Write-Error ('No engine installs found. Pass -EngineRoots, set $env:UE_ROOT, add "engineRoot" ' +
                 'to .ruitk-local.json (copy .ruitk-local.example.json), or install UE via the Epic ' +
                 'Games Launcher under "C:\Program Files\Epic Games\UE_5.*".')
    exit 1
}

foreach ($engine in $EngineRoots) {
    $runUat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'
    if (-not (Test-Path $runUat)) { Write-Warning "Skipping $engine (no RunUAT.bat)"; continue }
    $engineVer = Split-Path $engine -Leaf                # UE_5.6
    $shortVer  = $engineVer -replace '^UE_', ''          # 5.6
    $staging   = Join-Path $OutDir "staging-$shortVer"
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }

    Write-Host ''
    Write-Host "-- BuildPlugin against $engineVer" -ForegroundColor Cyan
    & cmd.exe /c "`"$runUat`" BuildPlugin -Plugin=`"$uplugin`" -Package=`"$staging`" -TargetPlatforms=Win64 -Rocket -StrictIncludes"
    if ($LASTEXITCODE -ne 0) { Write-Error "BuildPlugin failed for $engineVer (exit $LASTEXITCODE)"; exit 1 }

    # Fab wants source-only zips — Epic compiles the distributed binaries themselves.
    foreach ($d in 'Binaries', 'Intermediate', 'Saved') {
        $p = Join-Path $staging $d
        if (Test-Path $p) { Remove-Item $p -Recurse -Force }
    }

    $zip = Join-Path $OutDir "ReactiveUIToolkit-$versionName-UE$shortVer.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip
    Write-Host "   -> $zip" -ForegroundColor Green
}

Write-Host ''
Write-Host "Done. Remember: each zip's .uplugin must carry the matching `"EngineVersion`" before" -ForegroundColor Yellow
Write-Host "Fab upload (the release-process skill's checklist covers this + the compliance items)." -ForegroundColor Yellow
