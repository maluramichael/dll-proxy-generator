# Dll Proxy Generator

This project creates a new dll which sits between a game and the original dll. This way you can intercept all dll calls.

Game -> Your proxy dll -> Original dll

Original by Kristoffer Blasiak (<https://www.codeproject.com/Articles/1179147/ProxiFy-Automatic-Proxy-DLL-Generation>) i modified his project so the output fits my needs.

## Build

Open DllProxyGenerator.sln with Visual Studio and build it.

## Usage

The generator reads the export table of a DLL and writes proxy source. Two modes:

### forward (default)

Writes `<stem>_proxy.cpp` (all exports forwarded to the real DLL via linker
`/export` pragmas) plus a ready-to-build `CMakeLists.txt`. You override only the
function you want to intercept; everything else passes through untouched. Works
for x86 and x64. This is the right shape for hooking a graphics DLL and drawing
an overlay.

```
DllProxyGenerator d3d9.dll --real C:\Windows\System32\d3d9 --override Direct3DCreate9,Direct3DCreate9Ex
DllProxyGenerator SDL2.dll --real SDL2_orig --override SDL_GL_SwapWindow
```

`--real` is the forward target: a bare module name of a renamed copy next to the
proxy (`SDL2_orig`), or an absolute path without extension so the forward never
resolves back to the proxy itself (`C:\Windows\System32\d3d9`).

### trampoline (legacy)

`--trampoline` writes the old `.def` + `.cpp` naked-jmp stubs, one per export, so
you can put C code in front of every export. MSVC and x86 only (inline `__asm`
has no x64 equivalent); use `--forward` for a 64-bit proxy.

### Build the proxy

```
cmake -A Win32 -B build   # x86 target process (e.g. WoW 3.3.5a)
cmake -A x64   -B build   # x64 target process (e.g. WRATH)
cmake --build build --config Release
```

Copy the built proxy DLL into the game directory. The Windows DLL search order
loads the application directory before system32, so the proxy wins.

See `HANDOVER.md` for two worked ImGui-overlay examples (WoW 3.3.5a via d3d9,
WRATH via SDL2).
