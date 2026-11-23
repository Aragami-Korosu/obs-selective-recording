# SPDX-License-Identifier: GPL-2.0-or-later
# Runs the integration tests against a disposable, portable OBS.
#
#   .\run.ps1 -Obs C:\path\to\portable-obs -Plugin C:\path\to\obs-selective-recording -WorkDir C:\temp\sr-tests
#
# -Obs     A copy of OBS Studio (31.1 or newer) with portable_mode.txt and Aitum Vertical installed.
# -Plugin  The packaged plugin folder (contains bin\64bit and data).
# -Python  A Python with websocket-client, numpy and pillow. ffmpeg and ffprobe must be on PATH.
param(
	[Parameter(Mandatory)] [string] $Obs,
	[Parameter(Mandatory)] [string] $Plugin,
	[Parameter(Mandatory)] [string] $WorkDir,
	[string] $Python = "python"
)

$ErrorActionPreference = "Stop"
$config = Join-Path $Obs "config\obs-studio"
New-Item -ItemType Directory -Force $WorkDir | Out-Null

# Fresh scene collection and plugin settings every run.
Remove-Item (Join-Path $config "basic\scenes\*") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $config "plugin_config\obs-selective-recording") -Recurse -Force -ErrorAction SilentlyContinue
& $Python (Join-Path $PSScriptRoot "fixture.py") --config $config

# Portable OBS only loads plugins from its own folder.
Copy-Item (Join-Path $Plugin "bin\64bit\*") (Join-Path $Obs "obs-plugins\64bit") -Force
$data = Join-Path $Obs "data\obs-plugins\obs-selective-recording"
New-Item -ItemType Directory -Force $data | Out-Null
Copy-Item (Join-Path $Plugin "data\*") $data -Recurse -Force

& $Python (Join-Path $PSScriptRoot "run_tests.py") --workdir $WorkDir --obs-dir $Obs
exit $LASTEXITCODE
