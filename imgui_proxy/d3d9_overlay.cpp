// d3d9 proxy + Dear ImGui overlay (x86)
//
// Pairs with the generator-produced d3d9_proxy.cpp, which forwards every other
// export to the real C:\Windows\System32\d3d9.dll. This translation unit owns
// the single DllMain and the two overridden exports Direct3DCreate9/Ex.
//
// Flow:
//   game -> our Direct3DCreate9  -> real Direct3DCreate9 (system32)
//   first call also spins up a throwaway device, reads its vtable, and hooks
//   EndScene (index 42) and Reset (index 16) with MinHook. The EndScene hook
//   draws ImGui::ShowDemoWindow; the Reset hook handles device-lost.

#include <windows.h>
#include <d3d9.h>

#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// --- real d3d9 entry points --------------------------------------------------

typedef IDirect3D9 *(WINAPI *Direct3DCreate9_t)(UINT);
typedef HRESULT(WINAPI *Direct3DCreate9Ex_t)(UINT, IDirect3D9Ex **);

static HMODULE             gRealD3D9 = nullptr;
static Direct3DCreate9_t   pRealCreate9 = nullptr;
static Direct3DCreate9Ex_t pRealCreate9Ex = nullptr;

// --- hooked device methods ---------------------------------------------------

typedef HRESULT(STDMETHODCALLTYPE *EndScene_t)(IDirect3DDevice9 *);
typedef HRESULT(STDMETHODCALLTYPE *Reset_t)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);

static EndScene_t oEndScene = nullptr;
static Reset_t    oReset = nullptr;

static bool    gVTableHooked = false;
static bool    gImGuiReady = false;
static HWND    gWindow = nullptr;
static WNDPROC gOrigWndProc = nullptr;

static void EnsureRealLoaded()
{
    if (gRealD3D9)
        return;
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    lstrcatA(path, "\\d3d9.dll");
    gRealD3D9 = LoadLibraryA(path);
    if (gRealD3D9)
    {
        pRealCreate9 = (Direct3DCreate9_t)GetProcAddress(gRealD3D9, "Direct3DCreate9");
        pRealCreate9Ex = (Direct3DCreate9Ex_t)GetProcAddress(gRealD3D9, "Direct3DCreate9Ex");
    }
}

// --- WndProc subclass for input ----------------------------------------------

static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;
    return CallWindowProc(gOrigWndProc, hWnd, msg, wParam, lParam);
}

// --- hooks -------------------------------------------------------------------

static HRESULT STDMETHODCALLTYPE hkEndScene(IDirect3DDevice9 *dev)
{
    if (!gImGuiReady)
    {
        D3DDEVICE_CREATION_PARAMETERS cp;
        if (SUCCEEDED(dev->GetCreationParameters(&cp)))
            gWindow = cp.hFocusWindow;
        if (gWindow == nullptr)
            gWindow = GetForegroundWindow();

        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui_ImplWin32_Init(gWindow);
        ImGui_ImplDX9_Init(dev);

        gOrigWndProc = (WNDPROC)SetWindowLongPtr(gWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
        gImGuiReady = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oEndScene(dev);
}

static HRESULT STDMETHODCALLTYPE hkReset(IDirect3DDevice9 *dev, D3DPRESENT_PARAMETERS *pp)
{
    if (gImGuiReady)
        ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = oReset(dev, pp);
    if (gImGuiReady)
        ImGui_ImplDX9_CreateDeviceObjects();
    return hr;
}

// --- one-time vtable hook via a throwaway device -----------------------------

static void HookDeviceVTable()
{
    if (gVTableHooked)
        return;

    EnsureRealLoaded();
    if (!pRealCreate9)
        return;

    IDirect3D9 *d3d = pRealCreate9(D3D_SDK_VERSION);
    if (!d3d)
        return;

    // A message-only window is enough for a windowed device.
    HWND tmp = CreateWindowA("STATIC", "d3d9proxy", WS_OVERLAPPED, 0, 0, 1, 1,
                             nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = tmp;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;

    IDirect3DDevice9 *dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, tmp,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                                   &pp, &dev);
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        if (tmp) DestroyWindow(tmp);
        return;
    }

    void **vtable = *reinterpret_cast<void ***>(dev);

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
    {
        dev->Release();
        d3d->Release();
        if (tmp) DestroyWindow(tmp);
        return;
    }

    MH_CreateHook(vtable[42], &hkEndScene, reinterpret_cast<void **>(&oEndScene)); // EndScene
    MH_CreateHook(vtable[16], &hkReset, reinterpret_cast<void **>(&oReset));       // Reset
    MH_EnableHook(MH_ALL_HOOKS);
    gVTableHooked = true;

    dev->Release();
    d3d->Release();
    if (tmp) DestroyWindow(tmp);
}

// --- overridden exports ------------------------------------------------------
// Names are declared cleanly (undecorated) in d3d9_overlay.def so the x86
// __stdcall decoration (_Direct3DCreate9@4 etc.) does not leak into the export
// table. No __declspec(dllexport) here on purpose - the .def owns the export.

extern "C" IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
    EnsureRealLoaded();
    HookDeviceVTable();
    return pRealCreate9 ? pRealCreate9(SDKVersion) : nullptr;
}

extern "C" HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
    EnsureRealLoaded();
    HookDeviceVTable();
    return pRealCreate9Ex ? pRealCreate9Ex(SDKVersion, ppD3D) : E_NOTIMPL;
}

// --- entry point -------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        EnsureRealLoaded();
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (gVTableHooked)
        {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
        }
    }
    return TRUE;
}
