[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [bool] $Package = $false
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.tar.xz"
        )
    }

    Remove-Item @RemoveArgs

    # Code signing is optional: it runs when the WINDOWS_SIGNING_PFX (base64) and
    # WINDOWS_SIGNING_PASSWORD secrets are provided.
    $SignCommand = $null
    if ( $env:WINDOWS_SIGNING_PFX -and $env:WINDOWS_SIGNING_PASSWORD ) {
        Log-Group "Signing ${ProductName}..."
        $SignTool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
        if ( ! $SignTool ) {
            throw 'signtool.exe was not found'
        }
        $PfxPath = Join-Path $env:RUNNER_TEMP 'signing.pfx'
        [IO.File]::WriteAllBytes($PfxPath, [Convert]::FromBase64String($env:WINDOWS_SIGNING_PFX))
        $SignCommand = "`"${SignTool}`" sign /fd sha256 /f `"${PfxPath}`" /p `"${env:WINDOWS_SIGNING_PASSWORD}`" /tr http://timestamp.digicert.com /td sha256 /timeout 30000"

        $Binaries = Get-ChildItem "${ProjectRoot}/release/${Configuration}/${ProductName}/bin/64bit/*.dll"
        foreach ( $Binary in $Binaries ) {
            Write-Host "Signing $($Binary.Name) with a 30-second timestamp timeout..."
            & $SignTool sign /fd sha256 /f $PfxPath /p $env:WINDOWS_SIGNING_PASSWORD /tr http://timestamp.digicert.com /td sha256 /timeout 30000 $Binary.FullName
            if ( $LASTEXITCODE -ne 0 ) {
                throw "Signing $($Binary.Name) failed"
            }
        }
        Log-Group
    }

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Write-Host 'Creating Windows ZIP archive...'
    Compress-Archive -Force @CompressArgs
    Log-Group

    if ( $Package ) {
        # The installer can also install Aitum Vertical Canvas. Its release is pinned and checked by
        # SHA-256, and its source (GPL-2.0) is published next to the installer.
        Log-Group "Fetching bundled Aitum Vertical Canvas..."
        $Vertical = Get-Content "${ProjectRoot}/packaging/windows/bundled-vertical-canvas.json" -Raw | ConvertFrom-Json
        $VerticalWork = Join-Path $env:RUNNER_TEMP 'vertical-canvas'
        Remove-Item $VerticalWork -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force $VerticalWork | Out-Null
        foreach ( $Part in 'package', 'source' ) {
            $Url = $Vertical.$Part.url
            $File = Join-Path $VerticalWork (Split-Path $Url -Leaf)
            Write-Host "Downloading bundled Vertical Canvas $Part..."
            Invoke-WebRequest -Uri $Url -OutFile $File -UseBasicParsing -TimeoutSec 60 -MaximumRetryCount 2 -RetryIntervalSec 5
            $Hash = (Get-FileHash $File -Algorithm SHA256).Hash.ToLower()
            if ( $Hash -ne $Vertical.$Part.sha256 ) {
                throw "Checksum mismatch for ${Url}: got ${Hash}"
            }
        }
        $VerticalPackage = Join-Path $VerticalWork 'package'
        Expand-Archive (Join-Path $VerticalWork (Split-Path $Vertical.package.url -Leaf)) $VerticalPackage
        $VerticalDir = Join-Path $VerticalPackage 'vertical-canvas'
        if ( ! ( Test-Path "${VerticalDir}/bin/64bit/vertical-canvas.dll" ) ) {
            throw 'The Vertical Canvas package has an unexpected layout'
        }
        $VerticalSource = Join-Path $VerticalWork (Split-Path $Vertical.source.url -Leaf)
        tar -xf $VerticalSource -C $VerticalWork "vertical-canvas-$($Vertical.version)-source/LICENSE"
        Copy-Item (Join-Path $VerticalWork "vertical-canvas-$($Vertical.version)-source/LICENSE") $VerticalDir
        Copy-Item $VerticalSource "${ProjectRoot}/release/${OutputName}-vertical-canvas-$($Vertical.version)-source.tar.xz"
        $VerticalComponents = (($Vertical.version -split '\.') + '0')[0..3] -join ','
        Log-Group

        Log-Group "Building installer for ${ProductName}..."
        $Iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        if ( ! ( Test-Path $Iscc ) ) {
            choco install innosetup --no-progress -y | Out-Host
        }
        $IsccArgs = @(
            '/Q'
            "/DProductVersion=${ProductVersion}"
            "/DSourceDir=${ProjectRoot}/release/${Configuration}/${ProductName}"
            "/DVerticalDir=${VerticalDir}"
            "/DVerticalVersion=$($Vertical.version)"
            "/DVerticalVersionComponents=${VerticalComponents}"
            "/DOutputDir=${ProjectRoot}/release"
            "/DOutputName=${OutputName}-installer"
            "${ProjectRoot}/packaging/windows/installer.iss"
        )
        if ( $SignCommand ) {
            $IsccArgs = @('/DSign', "/Ssigntool=${SignCommand} `$f") + $IsccArgs
        }
        & $Iscc @IsccArgs
        if ( $LASTEXITCODE -ne 0 ) {
            throw "Installer build failed with exit code ${LASTEXITCODE}"
        }
        Log-Group

        # The runner has no OBS or Vertical, so a silent install must install both, and uninstalling
        # must remove the plugin but keep Vertical.
        Log-Group "Testing installer..."
        $TestRoot = Join-Path $env:RUNNER_TEMP 'installer-test\plugins'
        $TestApp = Join-Path $TestRoot $ProductName
        $Installer = "${ProjectRoot}/release/${OutputName}-installer.exe"
        $Setup = Start-Process $Installer -PassThru -ArgumentList @(
            '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', "/DIR=`"${TestApp}`"",
            "/LOG=`"$(Join-Path $env:RUNNER_TEMP 'installer-test.log')`"")
        Write-Host 'Waiting up to 2 minutes for the installer smoke test...'
        if ( ! $Setup.WaitForExit(120000) ) {
            Stop-Process -Id $Setup.Id -Force -ErrorAction SilentlyContinue
            throw 'Installer smoke test timed out after 2 minutes'
        }
        $Setup.Refresh()
        $Expected = @(
            "${TestApp}/bin/64bit/${ProductName}.dll"
            "${TestRoot}/vertical-canvas/bin/64bit/vertical-canvas.dll"
            "${TestRoot}/vertical-canvas/LICENSE"
        )
        $Missing = $Expected | Where-Object { ! ( Test-Path $_ ) }
        if ( $Setup.ExitCode -ne 0 -or $Missing ) {
            Get-Content (Join-Path $env:RUNNER_TEMP 'installer-test.log') -ErrorAction SilentlyContinue | Out-Host
            throw "Installer test failed (exit $($Setup.ExitCode)); missing: $($Missing -join ', ')"
        }
        $Uninstall = Start-Process "${TestApp}/unins000.exe" -PassThru -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES'
        Write-Host 'Waiting up to 1 minute for the uninstaller smoke test...'
        if ( ! $Uninstall.WaitForExit(60000) ) {
            Stop-Process -Id $Uninstall.Id -Force -ErrorAction SilentlyContinue
            throw 'Uninstaller smoke test timed out after 1 minute'
        }
        $Uninstall.Refresh()
        # The uninstaller copies itself to a temporary folder and returns at once; wait for the files.
        for ( $i = 0; $i -lt 30 -and ( Test-Path $TestApp ); $i++ ) { Start-Sleep 1 }
        if ( $Uninstall.ExitCode -ne 0 -or ( Test-Path $TestApp ) -or
             ! ( Test-Path "${TestRoot}/vertical-canvas/bin/64bit/vertical-canvas.dll" ) ) {
            throw 'Uninstall test failed: the plugin should be removed and Vertical kept'
        }
        Log-Group
    } else {
        Write-Host 'Skipping installer packaging; the workflow requested archive-only output.'
    }
}

Package
