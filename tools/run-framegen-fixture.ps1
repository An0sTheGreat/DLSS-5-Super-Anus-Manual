param([Parameter(Mandatory=$true)][string]$Directory,[switch]$Recovery,[switch]$Capture,[switch]$ScaleChurn,[switch]$MfgCadence,[switch]$InputTrace,[int]$InitialScale=0,[switch]$BoundaryTrace,[switch]$NestedSource,[switch]$NestedCandidate,[switch]$ManagerRelease,[switch]$PassControlsPreview,[switch]$PassInputTrace,[switch]$OfficialBaseline,[switch]$CyberpunkRuntime,[switch]$PassHistoryFix,[switch]$ManagerRelease103)
$ErrorActionPreference='Stop'
if ($Recovery -and $MfgCadence) { throw 'Recovery and MFG cadence use different host loops; run them separately.' }
$nrRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$nrFixture=[IO.Path]::GetFullPath((Join-Path $nrRoot $Directory))
if (-not $nrFixture.StartsWith((Join-Path $nrRoot 'build\framegen-fixture-'),[StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture paths only.' }
if (-not (Test-Path -LiteralPath (Join-Path $nrFixture 'ReShade.ini')) -or (Test-Path -LiteralPath (Join-Path $nrFixture 'host.out'))) { throw 'Missing configuration or existing results.' }
$nrCandidate=Join-Path $nrRoot $(if ($BoundaryTrace) { 'build/tlou2-boundary-trace-2/renodx-dlss5-super-anus.addon64' } else { 'build/dx11-integrated-game-test.addon64' })
if ($BoundaryTrace -and -not (Test-Path -LiteralPath $nrCandidate)) { $nrCandidate=Join-Path $nrRoot '_archive/test-builds/build-variants-2026-09-12/folders/tlou2-boundary-trace-2/renodx-dlss5-super-anus.addon64' }
if ($NestedCandidate) { $nrCandidate=Join-Path $nrRoot 'build/tlou2-nested-source-1/renodx-dlss5-super-anus.addon64' }
if ($NestedCandidate -and -not (Test-Path -LiteralPath $nrCandidate)) { $nrCandidate=Join-Path $nrRoot '_archive/test-builds/build-variants-2026-09-12/folders/tlou2-nested-source-1/renodx-dlss5-super-anus.addon64' }
if ($ManagerRelease) {
    if ($InputTrace -or $BoundaryTrace -or $NestedCandidate) { throw 'Release excludes diagnostic probes.' }
    $nrCandidate=Join-Path $nrRoot 'build/manager-1.0.2-addon/renodx-dlss5-super-anus.addon64'
    if (-not (Test-Path -LiteralPath $nrCandidate)) { $nrCandidate=Join-Path $nrRoot '_archive/test-builds/build-variants-2026-09-12/folders/manager-1.0.2-addon/renodx-dlss5-super-anus.addon64' }
}
if ($PassControlsPreview) {
    if ($ManagerRelease -or $InputTrace -or $BoundaryTrace -or $NestedCandidate) { throw 'Preview uses its own non-diagnostic candidate.' }
    $nrCandidate=Join-Path $nrRoot 'build/pass-controls-9/renodx-dlss5-super-anus.addon64'
    if (-not (Test-Path -LiteralPath $nrCandidate)) { $nrCandidate=Join-Path $nrRoot '_archive/test-builds/release-1.0.3-cleanup-2026-09-12/build-folders/pass-controls-9/renodx-dlss5-super-anus.addon64' }
}
if ($PassInputTrace) {
    if ($ManagerRelease -or $PassControlsPreview -or $InputTrace -or $BoundaryTrace -or $NestedCandidate) { throw 'Pass-input diagnostic uses its own candidate.' }
    $nrCandidate=Join-Path $nrRoot 'build/cp-pass-input-trace-1/renodx-dlss5-super-anus.addon64'
    if (-not (Test-Path -LiteralPath $nrCandidate)) { $nrCandidate=Join-Path $nrRoot '_archive/test-builds/release-1.0.3-cleanup-2026-09-12/build-folders/cp-pass-input-trace-1/renodx-dlss5-super-anus.addon64' }
}
if ($PassHistoryFix) {
    if ($OfficialBaseline -or $ManagerRelease -or $PassControlsPreview -or $PassInputTrace -or $InputTrace -or $BoundaryTrace -or $NestedCandidate) { throw 'History fix uses its own candidate.' }
    $nrCandidate=Join-Path $nrRoot 'build/cp-pass-history-fix-1/renodx-dlss5-super-anus.addon64'
    if (-not (Test-Path -LiteralPath $nrCandidate)) { $nrCandidate=Join-Path $nrRoot '_archive/test-builds/release-1.0.3-cleanup-2026-09-12/build-folders/cp-pass-history-fix-1/renodx-dlss5-super-anus.addon64' }
}
if ($ManagerRelease103) {
    if ($OfficialBaseline -or $ManagerRelease -or $PassHistoryFix -or $PassControlsPreview -or $PassInputTrace -or $InputTrace -or $BoundaryTrace -or $NestedCandidate) { throw 'Manager 1.0.3 uses its own release candidate.' }
    $nrCandidate=Join-Path $nrRoot 'build/manager-1.0.3-addon/renodx-dlss5-super-anus.addon64'
}
if ($OfficialBaseline) {
    if (-not $Recovery -or $Capture -or $ScaleChurn -or $MfgCadence -or $InputTrace -or $BoundaryTrace -or $NestedSource -or $NestedCandidate -or $ManagerRelease -or $PassControlsPreview -or $PassInputTrace -or $InitialScale -ne 100 -or $env:NR_TEST_TEMPORAL_INPUT -notin @('5','6','7')) { throw 'Official baseline requires an isolated temporal recovery run at 100%.' }
    $nrCandidate=Join-Path $nrRoot 'updated-official-renodx-dlss.addon64'
    $nrIdentity=@{sha256='1D855CF226857DCE890CFFBF7206BA9B6497CE1D471B217C1C8B44B6CD5D27E9';preset_rva=$null;preset_bytes=$null}
    $nrSymbols=[pscustomobject]@{}
} else {
    $nrLines=@(& python (Join-Path $PSScriptRoot 'final_capture_test_identity.py') $nrCandidate)
    if ($LASTEXITCODE -ne 0 -or $nrLines.Count -ne 2) { throw 'Cannot resolve candidate.' }
    $nrIdentity=$nrLines[0] | ConvertFrom-Json
    $nrSymbols=$nrLines[1] | ConvertFrom-Json
}
Copy-Item -LiteralPath (Join-Path $nrRoot 'build/api-native-ui3-smoke/d3d11.dll') -Destination (Join-Path $nrFixture 'dxgi.dll')
$nrRuntime=if ($CyberpunkRuntime) { 'W:/SteamLibrary/steamapps/common/Cyberpunk 2077/bin/x64' } else { Join-Path $nrRoot 'build/api-native-ui3-smoke' }
foreach ($nrDll in @('nvngx_dlss.dll','nvngx_dlssnr.dll')) {
    $nrSource=Join-Path $nrRuntime $nrDll
    $nrDestination=Join-Path $nrFixture $nrDll
    Copy-Item -LiteralPath $nrSource -Destination $nrDestination
    $nrHash=(Get-FileHash -LiteralPath $nrSource).Hash
    if ((Get-FileHash -LiteralPath $nrDestination).Hash -ne $nrHash) { throw 'Runtime copy mismatch.' }
    Write-Output "$nrDll SHA256=$nrHash"
}
Copy-Item -LiteralPath (Join-Path $nrRoot 'build/framegen_nr_host.exe') -Destination (Join-Path $nrFixture 'framegen_nr_host.exe')
$nrAddon=Join-Path $nrFixture 'renodx-dlss5-super-anus.addon64'
Copy-Item -LiteralPath $nrCandidate -Destination $nrAddon
if ((Get-FileHash -LiteralPath $nrAddon).Hash -ne $nrIdentity.sha256) { throw 'Candidate changed.' }
$nrSaved=@{}
$nrVars=@{NR_PASS_TEST_RVA=$nrIdentity.preset_rva;NR_PASS_TEST_BYTES=$nrIdentity.preset_bytes}
$nrVars['NR_TEST_OFFICIAL_BASELINE']=if ($OfficialBaseline) { '1' } else { $null }
$nrVars['NR_CAPTURE_TEST_RVA']=if ($Capture) { $nrIdentity.rva } else { $null }
$nrVars['NR_CAPTURE_TEST_BYTES']=if ($Capture) { $nrIdentity.bytes } else { $null }
foreach ($nrEntry in $nrSymbols.PSObject.Properties) { $nrVars[$nrEntry.Name]=$nrEntry.Value }
$nrVars['NR_TEST_SCALE_CHURN']=if ($ScaleChurn) { '1' } else { $null }
$nrVars['NR_TEST_MFG_CADENCE']=if ($MfgCadence) { '1' } else { $null }
$nrVars['NR_TEST_INPUT_TRACE']=if ($InputTrace) { '1' } else { $null }
$nrVars['NR_TEST_BOUNDARY_TRACE']=if ($BoundaryTrace -or $NestedCandidate) { '1' } else { $null }
$nrVars['NR_TEST_NESTED_SOURCE']=if ($NestedSource) { '1' } else { $null }
$nrVars['NR_TEST_INITIAL_SCALE']=if ($InitialScale -ge 25 -and $InitialScale -le 150) { [string]$InitialScale } else { $null }
try {
    foreach ($nrName in $nrVars.Keys) { $nrSaved[$nrName]=[Environment]::GetEnvironmentVariable($nrName); [Environment]::SetEnvironmentVariable($nrName,$nrVars[$nrName]) }
    $nrArgument=if ($Recovery) { 'recovery' } else { 'framegen' }
    $nrProc=Start-Process -FilePath (Join-Path $nrFixture 'framegen_nr_host.exe') -ArgumentList $nrArgument -WorkingDirectory $nrFixture -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $nrFixture 'host.out') -RedirectStandardError (Join-Path $nrFixture 'host.err')
    # A cold NR model cache can exceed one minute on the 400-frame scale sweep.
    $nrFinished = $nrProc.WaitForExit(45000)
    if (-not $nrFinished) { $nrFinished = $nrProc.WaitForExit(45000) }
    if (-not $nrFinished) { Stop-Process -Id $nrProc.Id; throw 'Owned fixture timed out; results preserved.' }
    Get-Content -LiteralPath (Join-Path $nrFixture 'host.out')
    if ($nrProc.ExitCode -ne 0) { throw "FrameGen fixture failed: $($nrProc.ExitCode)" }
    if ($ManagerRelease -or $ManagerRelease103) {
        $nrLog=Get-Content -LiteralPath (Join-Path $nrFixture 'ReShade.log') -Raw
        $nrReleaseId=if ($ManagerRelease103) { 'NR BUILD ID: 1\.0\.3-manager-release\.3' } else { 'NR BUILD ID: 1\.0\.3-framegen-upstream\.4' }
        if ($nrLog -notmatch $nrReleaseId -or $nrLog -notmatch 'hook nested-source: enabled') { throw 'Release guard not active.' }
        if ($nrLog -match 'NR boundary hook (execute|signal|wait|tag|token)|tlou2-.*trace|trace START:') { throw 'Diagnostic probe active in release.' }
    }
    if ($InputTrace) {
        & python (Join-Path $PSScriptRoot 'validate-framegen-input-trace.py') $nrFixture
        if ($LASTEXITCODE -ne 0) { throw 'Input trace validation failed.' }
    }
    if ($PassControlsPreview) {
        $nrLog=Get-Content -LiteralPath (Join-Path $nrFixture 'ReShade.log') -Raw
        if ($nrLog -notmatch 'NR BUILD ID: 1\.0\.3-pass-controls\.9' -or $nrLog -notmatch 'hook nested-source: enabled') { throw 'Pass controls identity/guard missing.' }
    }
} finally { foreach ($nrName in $nrSaved.Keys) { [Environment]::SetEnvironmentVariable($nrName,$nrSaved[$nrName]) } }
