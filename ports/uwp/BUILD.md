# Doom64EX Classic SDL2 UWP build notes

This is the Xbox UWP side of the SDL2 port.

The rough shape is:

- `Doom64EXClassicUWP.exe` is the packaged game
- `main.cpp` starts the SDL WinRT wrapper
- SDL2 and Mesa provide desktop OpenGL on UWP
- vcpkg provides Zlib and FluidLite
- Libpng 1.5.14 is built from `lib/lpng1514`
- Saves and config live in LocalState

## What You Need

You will want these installed:

- Visual Studio 2022 with UWP C++ support
- Windows 10 SDK `10.0.19041.0` or newer
- CMake
- vcpkg with the `x64-uwp-static-md` triplet available

Set `VCPKG_ROOT` to the vcpkg checkout you want CMake to use. It needs the UWP Zlib and FluidLite packages:

```powershell
$env:VCPKG_ROOT = "<vcpkg path>"
& "$env:VCPKG_ROOT\vcpkg.exe" install zlib:x64-uwp-static-md fluidlite:x64-uwp-static-md
```

`third_party/uwp-dep-sdl2` is the matched SDL2 and Mesa payload used by the package. Keep that folder together.

## Runtime DLLs

The MSIX stages these beside the exe:

```text
SDL2.dll
dxil.dll
glfw3.dll
libgallium_wgl.dll
libuwp.dll
opengl32.dll
z-1.dll
```

They all come from `third_party/uwp-dep-sdl2`.

## Storage

Saves, configuration, and screenshots stay in LocalState. The game looks for game data in this order:

```text
E:\doom64ex
LocalState
```

Put these two files together in either location:

```text
DOOM64.WAD
DOOMSND.SF2
```

## Custom WADs

The launcher appears before the game starts. `Start Game` runs the base game;
`Choose Custom WAD` opens the load-order screen.

Put custom WADs in either of these folders:

```text
E:\doom64ex\wads
LocalState\wads
```

The launcher merges both folders into one list. If a WAD has the same filename
in both places, the copy on `E:` wins. Add WADs to the right-hand list in the
order you want, then press Menu to start the game. The game receives them using
its normal `-file` option.

## Build

Start from a Visual Studio UWP developer prompt, or run `VsDevCmd.bat` with:

```text
-arch=x64 -host_arch=x64 -app_platform=UWP
```

For a fresh tree:

```powershell
cd <repo>\ports\uwp
$env:VCPKG_ROOT = "<vcpkg path>"
cmake --preset uwp-release
cmake --build --preset uwp-release
```

For a debug build, use the other preset:

```powershell
cmake --preset uwp-relwithdebinfo
cmake --build --preset uwp-relwithdebinfo
```

## Build Outputs

The MSIX lands under the matching build folder:

```text
ports\uwp\build\uwp-release\AppPackages\Doom64EXClassicUWP
ports\uwp\build\uwp-relwithdebinfo\AppPackages\Doom64EXClassicUWP
```
