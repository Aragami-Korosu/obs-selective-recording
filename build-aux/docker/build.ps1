# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Builds, checks and packages the Windows plugin and installer in a throwaway Docker container.

.DESCRIPTION
    Runs build-windows.sh in a temporary Debian container (Linux containers). The container is
    removed when it exits and uses bind mounts only, so it leaves no Docker volumes. The Debian
    image is removed afterwards too, unless it was already on this machine or -KeepImage is set.

    Downloads are cached in -Cache between runs. Results go to -Output\release: the plugin ZIP,
    the installer, Aitum Vertical Canvas's source archive and SHA256SUMS.txt.

.PARAMETER Signing
    A folder with signing.pfx and pfx-password.txt. When given, the DLL, the installer and its
    uninstaller are signed.

.EXAMPLE
    build-aux\docker\build.ps1
.EXAMPLE
    build-aux\docker\build.ps1 -Signing "$env:USERPROFILE\Documents\obs-selective-recording-signing"
#>
param(
	[string] $Output = (Join-Path $PSScriptRoot '..\..'),
	[string] $Cache = (Join-Path $PSScriptRoot '..\..\.deps\docker'),
	[string] $Signing,
	[switch] $KeepImage
)

$ErrorActionPreference = 'Stop'
$Image = 'debian:trixie'
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
New-Item -ItemType Directory -Force $Cache, $Output | Out-Null
$Cache = (Resolve-Path $Cache).Path
$Output = (Resolve-Path $Output).Path

$ImageWasPresent = [bool](docker image ls -q $Image)

$RunArgs = @('run', '--rm', '--name', "obs-sr-build-$PID",
	'-v', "${Repo}:/src:ro", '-v', "${Cache}:/cache", '-v', "${Output}:/out")
if ($Signing) {
	$Signing = (Resolve-Path $Signing).Path
	$RunArgs += @('-v', "${Signing}:/signing:ro")
}
$RunArgs += @($Image, 'bash', '/src/build-aux/docker/build-windows.sh')

# Docker reports progress on stderr, which Windows PowerShell would treat as an error.
$ErrorActionPreference = 'Continue'
try {
	& docker @RunArgs
	$Code = $LASTEXITCODE
} finally {
	if (-not $KeepImage -and -not $ImageWasPresent) {
		docker image rm $Image 2>&1 | Out-Null
	}
}

if ($Code -ne 0) {
	Write-Host "Build failed (exit code $Code)" -ForegroundColor Red
	exit $Code
}
Write-Host "Results: $(Join-Path $Output 'release')"
