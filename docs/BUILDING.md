# Building from Source

## Build in Docker (no Visual Studio needed)

[build-aux/docker/build.ps1](../build-aux/docker/build.ps1) builds the Windows plugin and installer in a throwaway Linux container. You only need Docker (Linux containers).

```powershell
build-aux\docker\build.ps1                                   # unsigned
build-aux\docker\build.ps1 -Signing C:\path\to\signing-folder  # signed (signing.pfx + pfx-password.txt)
```

The container:
1. checks formatting (clang-format 19, gersemi);
2. cross-compiles the plugin with clang-cl and lld-link against the pinned OBS 31.1.1 headers and obs-deps Qt 6 from [`buildspec.json`](../buildspec.json), with warnings treated as errors;
3. packages the ZIP and builds the installer with Inno Setup under Wine, signing with osslsigncode when `-Signing` is given;
4. installs and uninstalls the installer in a clean Wine prefix, to check that both the plugin and Aitum Vertical Canvas are installed and that only the plugin is removed.

Results go to `release\`. Downloads are cached in `.deps\docker` (about 3 GB). Every download is pinned by SHA-256, except the MSVC runtime and Windows SDK, which [xwin](https://github.com/Jake-Shadle/xwin) fetches from Microsoft. **Running the build accepts Microsoft's license for those files.**

The container is removed when it finishes and uses only bind mounts, so no Docker volumes are left behind. The `debian:trixie` image is removed afterwards too, unless it was already on your machine or you pass `-KeepImage`.

After a build, run the [integration tests](#testing) against `release\RelWithDebInfo\obs-selective-recording`.

The repository's **Windows Docker Package** action can run this same build on a dedicated, self-hosted Windows runner with Docker Desktop. It is manual-only, accepts runs from `main` only, and uploads the unsigned packages as a 14-day artifact. Never assign public pull-request jobs to this runner.

To build Ubuntu packages locally on Windows, run:

```powershell
build-aux\docker\build-ubuntu.ps1
```

This uses an Ubuntu 24.04 container, caches downloaded dependencies in `.deps\docker-ubuntu`, and writes the Debian packages, source archive, and checksums to `release\ubuntu\`. The manual **Windows Docker Package** action also builds and uploads Ubuntu packages.

## Prerequisites (Windows)

| Tool | Version | Notes |
| --- | --- | --- |
| Visual Studio 2022 | 17.x | Install the **Desktop development with C++** workload and the Windows 10/11 SDK 10.0.22621 |
| CMake | 3.28 – 3.30 | Must be on your `PATH` |
| Git | Any recent version | |
| PowerShell | 7+ | Only needed to run the CI build scripts locally |

You don't need to download OBS, Qt or other dependencies yourself. On the first configure, CMake downloads the versions pinned in [`buildspec.json`](../buildspec.json) into `.deps/`.

| Dependency | Pinned version |
| --- | --- |
| OBS Studio (libobs, frontend API) | 31.1.1 |
| obs-deps / Qt6 | 2025-07-11 |

A plugin built against these versions runs on OBS 31.1 and newer.

## Build

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

The build output goes in `build_x64/`. To build in Debug instead:

```powershell
cmake --build build_x64 --config Debug
```

To open the project in Visual Studio, use `build_x64/obs-selective-recording.sln`.

## Install for testing

Copy the built plugin into OBS's plugin folder:

```powershell
cmake --install build_x64 --config RelWithDebInfo --prefix "$env:ProgramData\obs-studio\plugins"
```

The plugin's layout follows the OBS 30+ convention:

```
%ProgramData%\obs-studio\plugins\obs-selective-recording\
├── bin\64bit\obs-selective-recording.dll
└── data\locale\en-US.ini
```

Restart OBS. To check the plugin loaded, open **Help → Log Files → View Current Log** and look for:

```
[obs-selective-recording] loaded (version 1.0.0)
[obs-selective-recording] canvas 'main' (1920x1080) available
[obs-selective-recording] canvas 'Aitum Vertical' (1080x1920) available
```

The `Aitum Vertical` line appears only if Vertical Canvas 1.6.0 or newer is installed. Canvas names and sizes depend on your setup.

## Debugging

1. In Visual Studio, set `obs-selective-recording` as the startup project.
2. Under **Debugging**, set **Command** to `C:\Program Files\obs-studio\bin\64bit\obs64.exe` and **Working Directory** to `C:\Program Files\obs-studio\bin\64bit`.
3. Build in Debug, install, then press F5.

## Formatting

The CI rejects unformatted code. Before you push, run:

```sh
build-aux/run-clang-format   # C/C++ (clang-format 19)
build-aux/run-gersemi        # CMake
```

These scripts need a POSIX shell, so on Windows run them from Git Bash or WSL.

## Packaging

`.github/scripts/Package-Windows.ps1` always makes the ZIP and makes the installer only when invoked with `-Package $true`. The CI workflow skips installer packaging on ordinary pull requests; release and main-branch builds request it. The installer is built with [Inno Setup 6](https://jrsoftware.org/isinfo.php) from [packaging/windows/installer.iss](../packaging/windows/installer.iss). If Inno Setup is not installed at its default location, the script downloads and verifies version 6.7.3; set `INNO_SETUP_COMPILER` to use an existing `ISCC.exe` instead. To build it by hand after `cmake --install`:

To build installable artifacts on demand, open **Actions → Dispatch → Run workflow**, select `build`, and enable **Build installable packages**. The workflow uploads the Windows installer and ZIP, macOS package, and Debian packages as downloadable artifacts. Manually dispatched packages are unsigned; release signing runs only for version tags.

```powershell
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" /DProductVersion=1.0.0 `
  /DSourceDir=release\RelWithDebInfo\obs-selective-recording /DOutputDir=release `
  /DOutputName=obs-selective-recording-1.0.0-windows-x64-installer packaging\windows\installer.iss
```

For a silent test install that doesn't need administrator rights, run the installer with `/CURRENTUSER /VERYSILENT /DIR=<folder>`.

### Bundled Aitum Vertical Canvas

The installer can also install Aitum Vertical Canvas. [packaging/windows/bundled-vertical-canvas.json](../packaging/windows/bundled-vertical-canvas.json) pins the release and the SHA-256 of its ProgramData package and source archive. The packaging script:
1. downloads both and stops if a checksum doesn't match;
2. adds Vertical's `LICENSE` to the package;
3. puts the source archive in `release/` as `…-windows-x64-vertical-canvas-<version>-source.tar.xz`, so it's published with the installer as GPL-2.0 requires;
4. passes `/DVerticalDir`, `/DVerticalVersion` and `/DVerticalVersionComponents` to Inno Setup.

To update Vertical, change the version, URLs and checksums in that file. Aitum lists the checksums in each release's notes. Without `/DVerticalDir`, the installer is built without Vertical.

Vertical installs into the sibling folder `…\plugins\vertical-canvas` and is offered only when that version or newer isn't already installed there. If `vertical-canvas.dll` is in OBS's own `obs-plugins\64bit` folder, it isn't offered, because OBS would load two copies.

### Code signing

The Docker build signs the plugin DLL, the installer and its uninstaller when you pass `-Signing` with a folder containing `signing.pfx` and `pfx-password.txt`. It uses osslsigncode with SHA-256 and a DigiCert timestamp. The folder is mounted read-only and the password is never printed. Without `-Signing`, builds are unsigned.

GitHub Actions signs the same files with `signtool` on version-tag release builds when the repository secrets `WINDOWS_SIGNING_PFX` (the `.pfx` file, base64-encoded) and `WINDOWS_SIGNING_PASSWORD` are set.

Test builds currently use a **self-signed** "Aragami Korosu" certificate. Windows shows that name as the publisher only on machines that trust it. To trust it, run this in an administrator PowerShell:

```powershell
packaging\windows\trust-test-certificate.ps1           # trust
packaging\windows\trust-test-certificate.ps1 -Remove   # undo
```

Other people will still see "Unknown publisher" until the project uses a certificate from a public certificate authority.

### Signed release artifacts

Pushing a version tag such as `1.0.0` starts the release workflow. The workflow creates a draft only after all platform signing credentials are configured:

- Windows: `WINDOWS_SIGNING_PFX` (base64-encoded PFX) and `WINDOWS_SIGNING_PASSWORD`.
- macOS: `MACOS_SIGNING_CERT` (base64-encoded Developer ID PKCS#12), `MACOS_SIGNING_CERT_PASSWORD`, `MACOS_SIGNING_APPLICATION_IDENTITY`, `MACOS_SIGNING_INSTALLER_IDENTITY`, `MACOS_NOTARIZATION_USERNAME` and `MACOS_NOTARIZATION_PASSWORD`. The keychain password and provisioning profile are optional when applicable.
- Debian: `DEBIAN_SIGNING_KEY` (base64-encoded ASCII-armored private key) and `DEBIAN_SIGNING_PASSPHRASE`. The release includes detached `.asc` signatures and the public key for verification.

The self-signed Windows test certificate can produce a signature, but Windows does not trust it by default. Use a certificate from a public certificate authority for trusted Windows releases.

## Testing

The end-to-end tests run the plugin inside a disposable, portable OBS, record through it, and check the files with ffmpeg. See [tests/integration/README.md](../tests/integration/README.md).

```powershell
tests\integration\run.ps1 -Obs C:\scratch\obs-portable -Plugin C:\path\to\obs-selective-recording -WorkDir C:\scratch\sr-tests
```

Run the full suite before opening a pull request that changes video, audio or recording code.

## Continuous integration

The GitHub Actions workflows from the OBS plugin template are in `.github/workflows`, but they're **disabled** for now. Builds and releases are made locally with the [Docker build](#build-in-docker-no-visual-studio-needed) and the integration tests. To turn them back on, run `gh workflow enable` for each workflow, or use the repository's **Actions** tab.

When enabled, they build Windows, macOS and Ubuntu on every push and pull request, with warnings as errors on macOS and Ubuntu, and pushing a tag such as `1.0.0` creates a draft release with packaged builds. The Ubuntu job installs OBS from the OBS Project's package archive; if that server is unavailable (HTTP 503), re-run the failed job.
