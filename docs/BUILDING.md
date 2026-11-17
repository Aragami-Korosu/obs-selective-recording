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
[obs-selective-recording] loaded (version 0.1.0)
```

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

## Continuous integration

GitHub Actions builds Windows, macOS and Ubuntu on every push and pull request, using the workflows inherited from the OBS plugin template. Pushing a tag such as `1.0.0` creates a draft release with packaged builds.
