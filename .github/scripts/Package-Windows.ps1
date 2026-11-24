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

function Invoke-BoundedDownload {
    param(
        [Parameter(Mandatory)]
        [string] $Uri,
        [Parameter(Mandatory)]
        [string] $Destination,
        [Parameter(Mandatory)]
        [string] $Name
    )

    $Curl = (Get-Command curl.exe -ErrorAction Stop).Source
    $StartInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $StartInfo.FileName = $Curl
    $StartInfo.UseShellExecute = $false
    $StartInfo.CreateNoWindow = $true
    $StartInfo.RedirectStandardError = $true
    foreach ( $Argument in @(
        '--fail', '--location', '--silent', '--show-error', '--retry', '1',
        '--retry-all-errors', '--retry-delay', '2', '--connect-timeout', '15',
        '--max-time', '60', '--output', $Destination, $Uri
    ) ) {
        [void] $StartInfo.ArgumentList.Add($Argument)
    }

    $Download = [System.Diagnostics.Process]::Start($StartInfo)
    $DownloadError = $Download.StandardError.ReadToEndAsync()
    if ( ! $Download.WaitForExit(130000) ) {
        Stop-Process -Id $Download.Id -Force -ErrorAction SilentlyContinue
        throw "Downloading ${Name} timed out after 130 seconds"
    }
    $Download.WaitForExit()
    if ( $Download.ExitCode -ne 0 ) {
        throw "Downloading ${Name} failed (curl exit $($Download.ExitCode)): $($DownloadError.Result.Trim())"
    }
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
            Invoke-BoundedDownload -Uri $Url -Destination $File -Name "bundled Vertical Canvas ${Part}"
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
        $Iscc = $env:INNO_SETUP_COMPILER
        if ( ! $Iscc ) {
            $Iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        }
        if ( ! ( Test-Path $Iscc ) ) {
            $InnoVersion = '6.7.3'
            $InnoSetup = Join-Path $env:RUNNER_TEMP "innosetup-${InnoVersion}.exe"
            $InnoUrl = "https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-${InnoVersion}.exe"
            Write-Host "Downloading Inno Setup ${InnoVersion}..."
            Invoke-BoundedDownload -Uri $InnoUrl -Destination $InnoSetup -Name "Inno Setup ${InnoVersion}"
            $InnoHash = (Get-FileHash $InnoSetup -Algorithm SHA256).Hash.ToLower()
            if ( $InnoHash -ne '9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732' ) {
                throw "Inno Setup ${InnoVersion} checksum mismatch: got ${InnoHash}"
            }

            $InnoDir = Join-Path $env:RUNNER_TEMP 'innosetup'
            Remove-Item $InnoDir -Recurse -Force -ErrorAction SilentlyContinue
            $Install = Start-Process $InnoSetup -ArgumentList @(
                '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', "/DIR=`"${InnoDir}`""
            ) -PassThru
            if ( ! $Install.WaitForExit(120000) ) {
                Stop-Process -Id $Install.Id -Force -ErrorAction SilentlyContinue
                throw 'Inno Setup installation timed out after 2 minutes'
            }
            $Install.Refresh()
            if ( $Install.ExitCode -ne 0 ) {
                throw "Inno Setup installation failed with exit code $($Install.ExitCode)"
            }
            $Iscc = Join-Path $InnoDir 'ISCC.exe'
        }
        if ( ! ( Test-Path $Iscc ) ) {
            throw "Inno Setup compiler was not found at ${Iscc}"
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
        Write-Host 'Compiling Windows installer (5-minute timeout)...'
        $CompilerInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $CompilerInfo.FileName = $Iscc
        $CompilerInfo.WorkingDirectory = $ProjectRoot
        $CompilerInfo.UseShellExecute = $false
        $CompilerInfo.CreateNoWindow = $true
        $CompilerInfo.RedirectStandardOutput = $true
        $CompilerInfo.RedirectStandardError = $true
        foreach ( $Argument in $IsccArgs ) {
            [void] $CompilerInfo.ArgumentList.Add($Argument)
        }
        $Compiler = [System.Diagnostics.Process]::Start($CompilerInfo)
        $CompilerOutput = $Compiler.StandardOutput.ReadToEndAsync()
        $CompilerError = $Compiler.StandardError.ReadToEndAsync()
        if ( ! $Compiler.WaitForExit(300000) ) {
            Stop-Process -Id $Compiler.Id -Force -ErrorAction SilentlyContinue
            throw 'Windows installer compilation timed out after 5 minutes'
        }
        $Compiler.WaitForExit()
        if ( $CompilerOutput.Result ) {
            Write-Host $CompilerOutput.Result
        }
        if ( $CompilerError.Result ) {
            Write-Host $CompilerError.Result
        }
        if ( $Compiler.ExitCode -ne 0 ) {
            throw "Installer build failed with exit code $($Compiler.ExitCode)"
        }
        Log-Group

        # The runner has no OBS or Vertical, so a silent install must install both, and uninstalling
        # must remove the plugin but keep Vertical.
        Log-Group "Testing installer..."
        $TestRoot = Join-Path $env:RUNNER_TEMP 'installer-test\plugins'
        $TestApp = Join-Path $TestRoot $ProductName
        Remove-Item $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
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
        $Expected = @("${TestApp}/bin/64bit/${ProductName}.dll")
        $VerticalAlreadyInstalled = $false
        $ObsRegistryKey = Get-Item -LiteralPath 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\OBS Studio' -ErrorAction SilentlyContinue
        if ( $ObsRegistryKey ) {
            $ObsInstallPath = $ObsRegistryKey.GetValue('')
            if ( $ObsInstallPath -and ( Test-Path (Join-Path $ObsInstallPath 'obs-plugins\64bit\vertical-canvas.dll') ) ) {
                $VerticalAlreadyInstalled = $true
            }
        }
        if ( ! $VerticalAlreadyInstalled ) {
            $ProgramDataVertical = Join-Path $env:ProgramData 'obs-studio\plugins\vertical-canvas\bin\64bit\vertical-canvas.dll'
            if ( Test-Path $ProgramDataVertical ) {
                $InstalledVersion = (Get-Item $ProgramDataVertical).VersionInfo.FileVersion
                $BundledVersion = $Vertical.version
                if ( $InstalledVersion -and ([version]$InstalledVersion -ge [version]$BundledVersion) ) {
                    $VerticalAlreadyInstalled = $true
                }
            }
        }
        if ( $VerticalAlreadyInstalled ) {
            Write-Host 'The installer correctly skipped Vertical Canvas because an equal or newer copy is already installed.'
        } else {
            $Expected += @(
                "${TestRoot}/vertical-canvas/bin/64bit/vertical-canvas.dll"
                "${TestRoot}/vertical-canvas/LICENSE"
            )
        }
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
        $UnexpectedVerticalRemoval = ! $VerticalAlreadyInstalled -and
            ! ( Test-Path "${TestRoot}/vertical-canvas/bin/64bit/vertical-canvas.dll" )
        if ( $Uninstall.ExitCode -ne 0 -or ( Test-Path $TestApp ) -or $UnexpectedVerticalRemoval ) {
            throw 'Uninstall test failed: the plugin should be removed and Vertical kept'
        }
        Log-Group
    } else {
        Write-Host 'Skipping installer packaging; the workflow requested archive-only output.'
    }
}

Package
