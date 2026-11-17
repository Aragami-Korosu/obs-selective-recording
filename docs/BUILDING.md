# Building from Source

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

`.github/scripts/Package-Windows.ps1` makes the ZIP and the installer. The installer is built with [Inno Setup 6](https://jrsoftware.org/isinfo.php) from [packaging/windows/installer.iss](../packaging/windows/installer.iss). To build it by hand after `cmake --install`:

```powershell
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" /DProductVersion=1.0.0 `
  /DSourceDir=release\RelWithDebInfo\obs-selective-recording /DOutputDir=release `
  /DOutputName=obs-selective-recording-1.0.0-windows-x64-installer packaging\windows\installer.iss
```

For a silent test install that doesn't need administrator rights, run the installer with `/CURRENTUSER /VERYSILENT /DIR=<folder>`.

## Testing

The end-to-end tests run the plugin inside a disposable, portable OBS, record through it, and check the files with ffmpeg. See [tests/integration/README.md](../tests/integration/README.md).

```powershell
tests\integration\run.ps1 -Obs C:\scratch\obs-portable -Plugin C:\path\to\obs-selective-recording -WorkDir C:\scratch\sr-tests
```

Run the full suite before opening a pull request that changes video, audio or recording code.

## Continuous integration

GitHub Actions builds Windows, macOS and Ubuntu on every push and pull request, using the workflows inherited from the OBS plugin template. Warnings are errors on macOS and Ubuntu. Pushing a tag such as `1.0.0` creates a draft release with packaged builds.

The Ubuntu job installs OBS from the OBS Project's package archive. If that server is unavailable (HTTP 503), re-run the failed job.
