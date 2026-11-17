[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
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
        $SignCommand = "`"${SignTool}`" sign /fd sha256 /f `"${PfxPath}`" /p `"${env:WINDOWS_SIGNING_PASSWORD}`" /tr http://timestamp.digicert.com /td sha256"

        $Binaries = Get-ChildItem "${ProjectRoot}/release/${Configuration}/${ProductName}/bin/64bit/*.dll"
        foreach ( $Binary in $Binaries ) {
            & $SignTool sign /fd sha256 /f $PfxPath /p $env:WINDOWS_SIGNING_PASSWORD /tr http://timestamp.digicert.com /td sha256 $Binary.FullName
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
    Compress-Archive -Force @CompressArgs
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
}

Package
