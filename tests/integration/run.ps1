# SPDX-License-Identifier: GPL-2.0-or-later
# Runs the integration tests against a disposable, portable OBS.
#
#   .\run.ps1 -Obs C:\path\to\portable-obs -Plugin C:\path\to\obs-selective-recording -WorkDir C:\temp\sr-tests
#
# -Obs     A copy of OBS Studio (31.1 or newer) with portable_mode.txt and Aitum Vertical installed.
# -Plugin  The packaged plugin folder (contains bin\64bit and data).
# -Python  A Python with websocket-client, numpy and pillow.
param(
	[Parameter(Mandatory)] [string] $Obs,
	[Parameter(Mandatory)] [string] $Plugin,
	[Parameter(Mandatory)] [string] $WorkDir,
	[string] $Python = "python"
)

$ErrorActionPreference = "Stop"
$config = Join-Path $Obs "config\obs-studio"
$pidFile = Join-Path $WorkDir "obs.pid"
New-Item -ItemType Directory -Force $WorkDir | Out-Null

function Stop-TestObs {
	if (Test-Path $pidFile) {
		$id = [int](Get-Content $pidFile)
		$proc = Get-Process -Id $id -ErrorAction SilentlyContinue
		if ($proc -and $proc.Path -like "$Obs*") {
			$proc.CloseMainWindow() | Out-Null
			if (-not $proc.WaitForExit(15000)) { Stop-Process -Id $id -Force }
		}
		Remove-Item $pidFile -Force
	}
}

Stop-TestObs

# Fresh scene collection every run.
Remove-Item (Join-Path $config "basic\scenes\*") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $config "plugin_config\obs-selective-recording") -Recurse -Force -ErrorAction SilentlyContinue
& $Python (Join-Path $PSScriptRoot "fixture.py") --config $config

# Portable OBS only loads plugins from its own folder.
Copy-Item (Join-Path $Plugin "bin\64bit\*") (Join-Path $Obs "obs-plugins\64bit") -Force
$data = Join-Path $Obs "data\obs-plugins\obs-selective-recording"
New-Item -ItemType Directory -Force $data | Out-Null
Copy-Item (Join-Path $Plugin "data\*") $data -Recurse -Force

$bin = Join-Path $Obs "bin\64bit"
$proc = Start-Process -FilePath (Join-Path $bin "obs64.exe") -WorkingDirectory $bin -PassThru `
	-ArgumentList '--portable', '--multi', '--disable-shutdown-check', '--disable-updater', '--minimize-to-tray'
Set-Content $pidFile $proc.Id

try {
	& $Python (Join-Path $PSScriptRoot "run_tests.py") --workdir $WorkDir
	$code = $LASTEXITCODE
} finally {
	Stop-TestObs
}

$log = Get-ChildItem (Join-Path $config "logs") -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Write-Host "OBS log: $($log.FullName)"
exit $code
