// Auto-generated proxy for d3d9.dll
// Forwards every export to "C:\Windows\System32\d3d9" except the ones you override.
//
// realModule can be a bare name (a renamed copy next to the proxy, e.g.
// SDL2_orig) or an absolute path without extension
// (e.g. C:\\Windows\\System32\\d3d9) so the forward never resolves back
// to this proxy. On x86, __stdcall exports you override need the decorated
// name in the /export pragma (_Name@N); cdecl/x64 use the plain name.
#include <windows.h>

// TODO ordinal-only export @16 - forward via .def: SomeName=C:\Windows\System32\d3d9.#16 @16 NONAME
// TODO ordinal-only export @17 - forward via .def: SomeName=C:\Windows\System32\d3d9.#17 @17 NONAME
// TODO ordinal-only export @18 - forward via .def: SomeName=C:\Windows\System32\d3d9.#18 @18 NONAME
// TODO ordinal-only export @19 - forward via .def: SomeName=C:\Windows\System32\d3d9.#19 @19 NONAME
#pragma comment(linker, "/export:Direct3DCreate9On12=C:\\Windows\\System32\\d3d9.Direct3DCreate9On12")
#pragma comment(linker, "/export:Direct3DCreate9On12Ex=C:\\Windows\\System32\\d3d9.Direct3DCreate9On12Ex")
// TODO ordinal-only export @22 - forward via .def: SomeName=C:\Windows\System32\d3d9.#22 @22 NONAME
// TODO ordinal-only export @23 - forward via .def: SomeName=C:\Windows\System32\d3d9.#23 @23 NONAME
#pragma comment(linker, "/export:Direct3DShaderValidatorCreate9=C:\\Windows\\System32\\d3d9.Direct3DShaderValidatorCreate9")
#pragma comment(linker, "/export:PSGPError=C:\\Windows\\System32\\d3d9.PSGPError")
#pragma comment(linker, "/export:PSGPSampleTexture=C:\\Windows\\System32\\d3d9.PSGPSampleTexture")
#pragma comment(linker, "/export:D3DPERF_BeginEvent=C:\\Windows\\System32\\d3d9.D3DPERF_BeginEvent")
#pragma comment(linker, "/export:D3DPERF_EndEvent=C:\\Windows\\System32\\d3d9.D3DPERF_EndEvent")
#pragma comment(linker, "/export:D3DPERF_GetStatus=C:\\Windows\\System32\\d3d9.D3DPERF_GetStatus")
#pragma comment(linker, "/export:D3DPERF_QueryRepeatFrame=C:\\Windows\\System32\\d3d9.D3DPERF_QueryRepeatFrame")
#pragma comment(linker, "/export:D3DPERF_SetMarker=C:\\Windows\\System32\\d3d9.D3DPERF_SetMarker")
#pragma comment(linker, "/export:D3DPERF_SetOptions=C:\\Windows\\System32\\d3d9.D3DPERF_SetOptions")
#pragma comment(linker, "/export:D3DPERF_SetRegion=C:\\Windows\\System32\\d3d9.D3DPERF_SetRegion")
#pragma comment(linker, "/export:DebugSetLevel=C:\\Windows\\System32\\d3d9.DebugSetLevel")
#pragma comment(linker, "/export:DebugSetMute=C:\\Windows\\System32\\d3d9.DebugSetMute")
#pragma comment(linker, "/export:Direct3D9EnableMaximizedWindowedModeShim=C:\\Windows\\System32\\d3d9.Direct3D9EnableMaximizedWindowedModeShim")
// OVERRIDDEN, implement yourself: Direct3DCreate9
// OVERRIDDEN, implement yourself: Direct3DCreate9Ex

// --- your overrides ---------------------------------------------------
// Each must be exported. On x64 the plain name works; on x86 add the
// matching decoration in a /export pragma or a .def file.
// extern "C" __declspec(dllexport) <ret> Direct3DCreate9(<args>) { /* hook, then call real */ }
// extern "C" __declspec(dllexport) <ret> Direct3DCreate9Ex(<args>) { /* hook, then call real */ }

// DllMain and the Direct3DCreate9(Ex) overrides live in d3d9_overlay.cpp.
