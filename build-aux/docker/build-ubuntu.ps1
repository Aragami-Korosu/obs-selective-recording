<#
.SYNOPSIS
    Builds and packages the Ubuntu plugin in a throwaway Ubuntu Docker container.

.EXAMPLE
    build-aux\docker\build-ubuntu.ps1
#>
param(
	[string] $Output = (Join-Path $PSScriptRoot '..\..\release\ubuntu'),
	[string] $Cache = (Join-Path $PSScriptRoot '..\..\.deps\docker-ubuntu')
)

$ErrorActionPreference = 'Stop'
$Image = 'ubuntu:24.04'
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
New-Item -ItemType Directory -Force $Cache, $Output | Out-Null
$Cache = (Resolve-Path $Cache).Path
$Output = (Resolve-Path $Output).Path

$DockerOS = docker info --format '{{.OSType}}' 2>$null
if ( $LASTEXITCODE -ne 0 ) {
	throw 'Docker is not available. Start Docker Desktop with Linux containers enabled.'
}
if ( $DockerOS.Trim() -ne 'linux' ) {
	throw "Expected Docker Linux containers but got '$DockerOS'"
}

$RunArgs = @('run', '--rm', '--name', "obs-sr-ubuntu-build-$PID",
	'-v', "${Repo}:/src:ro", '-v', "${Cache}:/cache", '-v', "${Output}:/out",
	$Image, 'bash', '/src/build-aux/docker/build-ubuntu.sh')

# Docker reports progress on stderr, which Windows PowerShell would treat as an error.
$ErrorActionPreference = 'Continue'
& docker @RunArgs
$Code = $LASTEXITCODE

if ($Code -ne 0) {
	Write-Host "Ubuntu packaging failed (exit code $Code)" -ForegroundColor Red
	exit $Code
}
Write-Host "Results: $Output"
