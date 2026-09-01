// DLL Proxy Generator
//
// Reads the export table of a Windows DLL and generates source for a proxy DLL
// that sits between an application and the original DLL.
//
//   App -> proxy DLL -> original DLL
//
// Two output modes:
//
//   forward     (default) One <stem>_proxy.cpp full of linker export-forwards
//               to the real DLL, plus a CMakeLists.txt. You override the one
//               function you want to intercept; everything else is forwarded
//               untouched. Works for x86 and x64, MSVC and clang. This is the
//               right shape for hooking a graphics DLL (d3d9/dxgi/opengl32/
//               SDL2) and drawing an ImGui overlay in EndScene / Present /
//               SwapBuffers / SDL_GL_SwapWindow.
//
//   trampoline  Legacy behaviour: naked jmp stubs per export via a .def + .cpp,
//               so you can put C code in front of every export. MSVC + x86 only
//               (inline __asm does not exist on x64). Use forward mode for x64.

#include <windows.h>
#include <imagehlp.h>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using std::string;
using std::vector;

struct Export
{
    unsigned ordinal = 0;   // real ordinal (Base + index)
    string name;            // empty => exported by ordinal only
    bool isForwarder = false;
    string forwardTarget;   // e.g. "NTDLL.RtlAllocateHeap" (informational)
};

static WORD gMachine = 0; // IMAGE_FILE_MACHINE_I386 / _AMD64

// --- filesystem helpers ------------------------------------------------------

static string baseName(const string &path)
{
    size_t p = path.find_last_of("/\\");
    return p == string::npos ? path : path.substr(p + 1);
}

// Strip a trailing ".dll"/".DLL" if present; otherwise keep the name as-is.
static string stripDllExt(const string &file)
{
    if (file.size() > 4)
    {
        string ext = file.substr(file.size() - 4);
        for (char &c : ext) c = (char)tolower((unsigned char)c);
        if (ext == ".dll")
            return file.substr(0, file.size() - 4);
    }
    return file;
}

// --- export parsing ----------------------------------------------------------

// Walk the PE export directory directly so we get ordinals, name-less exports
// and forwarders, not just AddressOfNames.
static bool readExports(const string &dllPath, vector<Export> &out)
{
    LOADED_IMAGE img;
    if (!MapAndLoad(dllPath.c_str(), nullptr, &img, TRUE, TRUE))
        return false;

    gMachine = img.FileHeader->FileHeader.Machine;

    ULONG size = 0;
    auto *dir = (IMAGE_EXPORT_DIRECTORY *)ImageDirectoryEntryToData(
        img.MappedAddress, FALSE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size);
    if (dir == nullptr)
    {
        UnMapAndLoad(&img);
        return true; // no exports is not an error, just an empty list
    }

    // A function whose target sits inside the export directory itself is a
    // forwarder. Compare in VA space (via ImageDirectoryEntryToData's returned
    // pointer + size) so this stays correct whether the target DLL is PE32 or
    // PE32+, regardless of how this generator was built.
    auto *expLo = (const char *)dir;
    auto *expHi = expLo + size;

    auto *funcs = (DWORD *)ImageRvaToVa(img.FileHeader, img.MappedAddress, dir->AddressOfFunctions, nullptr);
    auto *names = (DWORD *)ImageRvaToVa(img.FileHeader, img.MappedAddress, dir->AddressOfNames, nullptr);
    auto *nameOrds = (WORD *)ImageRvaToVa(img.FileHeader, img.MappedAddress, dir->AddressOfNameOrdinals, nullptr);

    // Map function-table index -> exported name (only some indices have names).
    vector<string> nameByIndex(dir->NumberOfFunctions);
    if (names && nameOrds)
    {
        for (DWORD i = 0; i < dir->NumberOfNames; i++)
        {
            WORD idx = nameOrds[i];
            auto *n = (char *)ImageRvaToVa(img.FileHeader, img.MappedAddress, names[i], nullptr);
            if (idx < dir->NumberOfFunctions && n)
                nameByIndex[idx] = n;
        }
    }

    for (DWORD i = 0; i < dir->NumberOfFunctions && funcs; i++)
    {
        DWORD rva = funcs[i];
        if (rva == 0)
            continue; // gap in the ordinal table

        Export e;
        e.ordinal = dir->Base + i;
        e.name = nameByIndex[i];
        auto *fva = (const char *)ImageRvaToVa(img.FileHeader, img.MappedAddress, rva, nullptr);
        if (fva && fva >= expLo && fva < expHi)
        {
            e.isForwarder = true;
            e.forwardTarget = fva;
        }
        out.push_back(e);
    }

    UnMapAndLoad(&img);
    return true;
}

// --- forward mode ------------------------------------------------------------

static bool isOverridden(const string &name, const vector<string> &overrides)
{
    for (const auto &o : overrides)
        if (o == name)
            return true;
    return false;
}

static void writeForwardProxy(const string &stem, const string &realModule,
                              const vector<string> &overrides, const vector<Export> &exports)
{
    std::ofstream f(stem + "_proxy.cpp");

    f << "// Auto-generated proxy for " << stem << ".dll\n";
    f << "// Forwards every export to \"" << realModule << "\" except the ones you override.\n";
    f << "//\n";
    f << "// realModule can be a bare name (a renamed copy next to the proxy, e.g.\n";
    f << "// SDL2_orig) or an absolute path without extension\n";
    f << "// (e.g. C:\\\\Windows\\\\System32\\\\d3d9) so the forward never resolves back\n";
    f << "// to this proxy. On x86, __stdcall exports you override need the decorated\n";
    f << "// name in the /export pragma (_Name@N); cdecl/x64 use the plain name.\n";
    f << "#include <windows.h>\n\n";

    unsigned forwarded = 0, ordinalOnly = 0, skipped = 0;
    for (const auto &e : exports)
    {
        if (!e.name.empty() && isOverridden(e.name, overrides))
        {
            f << "// OVERRIDDEN, implement yourself: " << e.name << "\n";
            skipped++;
            continue;
        }
        if (!e.name.empty())
        {
            f << "#pragma comment(linker, \"/export:" << e.name << "=" << realModule << "." << e.name << "\")\n";
            forwarded++;
        }
        else
        {
            // Name-less (ordinal-only) export. /export is name-centric and cannot
            // express this cleanly; forward it by ordinal in a .def if you need it.
            f << "// TODO ordinal-only export @" << e.ordinal
              << " - forward via .def: SomeName=" << realModule << ".#" << e.ordinal << " @" << e.ordinal << " NONAME\n";
            ordinalOnly++;
        }
    }

    f << "\n";
    if (!overrides.empty())
    {
        f << "// --- your overrides ---------------------------------------------------\n";
        f << "// Each must be exported. On x64 the plain name works; on x86 add the\n";
        f << "// matching decoration in a /export pragma or a .def file.\n";
        for (const auto &o : overrides)
        {
            f << "// extern \"C\" __declspec(dllexport) <ret> " << o << "(<args>) { /* hook, then call real */ }\n";
        }
        f << "\n";
    }

    f << "BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)\n";
    f << "{\n";
    f << "    if (reason == DLL_PROCESS_ATTACH)\n";
    f << "    {\n";
    f << "        // TODO: install your hook here (e.g. MinHook on the device vtable,\n";
    f << "        // or hook the forwarded export you left out above).\n";
    f << "    }\n";
    f << "    return TRUE;\n";
    f << "}\n";
    f.close();

    // CMake so nobody hand-configures a Visual Studio project.
    std::ofstream c("CMakeLists.txt");
    c << "cmake_minimum_required(VERSION 3.15)\n";
    c << "project(" << stem << "_proxy CXX)\n\n";
    c << "# Build for the SAME bitness as the target process:\n";
    c << "#   x86  ->  cmake -A Win32 -B build\n";
    c << "#   x64  ->  cmake -A x64   -B build\n";
    c << "add_library(" << stem << "_proxy SHARED " << stem << "_proxy.cpp)\n";
    c << "set_target_properties(" << stem << "_proxy PROPERTIES\n";
    c << "    OUTPUT_NAME \"" << stem << "\"   # produces " << stem << ".dll\n";
    c << "    PREFIX \"\")\n";
    c << "# Add MinHook / ImGui sources + target_link_libraries here for your overlay.\n";
    c.close();

    printf("forward mode: %s_proxy.cpp + CMakeLists.txt\n", stem.c_str());
    printf("  target machine: %s\n", gMachine == IMAGE_FILE_MACHINE_AMD64 ? "x64" : gMachine == IMAGE_FILE_MACHINE_I386 ? "x86" : "unknown");
    printf("  forwarded: %u by name, %u by ordinal, %u overridden\n", forwarded, ordinalOnly, skipped);
    if ((size_t)skipped != overrides.size())
        printf("  WARNING: %zu override name(s) did not match any export\n", overrides.size() - skipped);
}

// --- trampoline mode (legacy, x86 only) --------------------------------------

static void writeTrampoline(const string &stem, const vector<Export> &exports)
{
    vector<string> names;
    for (const auto &e : exports)
        if (!e.name.empty() && !e.isForwarder)
            names.push_back(e.name);

    std::ofstream def(stem + ".def");
    def << "LIBRARY " << stem << "\nEXPORTS\n";
    for (size_t i = 0; i < names.size(); i++)
        def << "\t" << names[i] << "=Fake" << names[i] << " @" << i + 1 << "\n";
    def.close();

    std::ofstream f(stem + ".cpp");
    f << "#include <windows.h>\n\n";
    f << "struct " << stem << "_dll {\n\tHMODULE dll;\n";
    for (const auto &n : names)
        f << "\tFARPROC Original" << n << ";\n";
    f << "} " << stem << ";\n\n";

    for (const auto &n : names)
        f << "__declspec(naked) void Fake" << n << "() { __asm { jmp [" << stem << ".Original" << n << "] } }\n";

    f << "\nBOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {\n";
    f << "\tif (reason == DLL_PROCESS_ATTACH) {\n";
    f << "\t\tchar path[MAX_PATH];\n";
    f << "\t\tGetSystemDirectoryA(path, MAX_PATH);\n";
    f << "\t\tlstrcatA(path, \"\\\\" << stem << ".dll\");\n";
    f << "\t\t" << stem << ".dll = LoadLibraryA(path);\n";
    f << "\t\tif (!" << stem << ".dll) { MessageBoxA(0, \"Cannot load original " << stem << ".dll\", \"Proxy\", MB_ICONERROR); ExitProcess(0); }\n";
    for (const auto &n : names)
        f << "\t\t" << stem << ".Original" << n << " = GetProcAddress(" << stem << ".dll, \"" << n << "\");\n";
    f << "\t}\n";
    f << "\telse if (reason == DLL_PROCESS_DETACH) { FreeLibrary(" << stem << ".dll); }\n";
    f << "\treturn TRUE;\n}\n";
    f.close();

    printf("trampoline mode: %s.def + %s.cpp (%zu exports)\n", stem.c_str(), stem.c_str(), names.size());
}

// --- cli ---------------------------------------------------------------------

static void usage()
{
    printf(
        "DllProxyGenerator <path-to-dll> [options]\n"
        "\n"
        "  --forward            forward all exports to the real DLL (default)\n"
        "  --trampoline         legacy per-export naked-jmp stubs (MSVC, x86 only)\n"
        "  --real <module>      forward target for --forward. Bare name of a renamed\n"
        "                       copy (SDL2_orig) or absolute path without extension\n"
        "                       (C:\\Windows\\System32\\d3d9). Default: <stem>_orig\n"
        "  --override a,b,c     export names you implement yourself (comma separated)\n"
        "\n"
        "Examples:\n"
        "  DllProxyGenerator d3d9.dll --real C:\\Windows\\System32\\d3d9 --override Direct3DCreate9,Direct3DCreate9Ex\n"
        "  DllProxyGenerator SDL2.dll --real SDL2_orig --override SDL_GL_SwapWindow\n");
}

static vector<string> splitCsv(const string &s)
{
    vector<string> out;
    string cur;
    for (char c : s)
    {
        if (c == ',')
        {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        }
        else
            cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        usage();
        return 1;
    }

    string dllPath = argv[1];
    bool trampoline = false;
    string realModule;
    vector<string> overrides;

    for (int i = 2; i < argc; i++)
    {
        string a = argv[i];
        if (a == "--forward") trampoline = false;
        else if (a == "--trampoline") trampoline = true;
        else if (a == "--real" && i + 1 < argc) realModule = argv[++i];
        else if (a == "--override" && i + 1 < argc) overrides = splitCsv(argv[++i]);
        else { printf("unknown/incomplete option: %s\n\n", a.c_str()); usage(); return 1; }
    }

    vector<Export> exports;
    if (!readExports(dllPath, exports))
    {
        printf("error: cannot read '%s' (not found or not a PE image)\n", dllPath.c_str());
        return 1;
    }
    if (exports.empty())
    {
        printf("error: '%s' has no exports\n", dllPath.c_str());
        return 1;
    }

    string stem = stripDllExt(baseName(dllPath));

    if (trampoline)
    {
        if (gMachine == IMAGE_FILE_MACHINE_AMD64)
        {
            printf("error: trampoline mode is x86 only (inline __asm has no x64 equivalent).\n"
                   "       Use --forward for a 64-bit proxy.\n");
            return 1;
        }
        writeTrampoline(stem, exports);
    }
    else
    {
        if (realModule.empty())
            realModule = stem + "_orig";
        writeForwardProxy(stem, realModule, overrides, exports);
    }

    return 0;
}
