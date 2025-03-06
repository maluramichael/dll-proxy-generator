#include <iostream>
#include <Windows.h>
#include <Commdlg.h>
#include <String.h>
#include <winnt.h>
#include <imagehlp.h>
#include <vector>
#include <string>
#include <fstream>
#include <tchar.h>
#include <stdio.h>
#include <memory>
#include <stdexcept>
#include <filesystem>

using namespace std;

// Architecture type of the target DLL
WORD g_architectureType;

// List of exported function names
vector<string> g_exportedNames;

/**
 * @brief Splits a string by a delimiter character
 * @param input The string to split
 * @param delimiter The character to split by
 * @return Vector of substrings
 */
const vector<string> splitString(const string& input, const char delimiter)
{
    vector<string> result;
    string currentToken;
    
    for (const char character : input)
    {
        if (character != delimiter)
        {
            currentToken += character;
        }
        else if (!currentToken.empty())
        {
            result.push_back(currentToken);
            currentToken.clear();
        }
    }
    
    if (!currentToken.empty())
    {
        result.push_back(currentToken);
    }
    
    return result;
}

/**
 * @brief Retrieves the NT headers from a PE file
 * @param filePath Path to the PE file
 * @param headers Reference to store the headers
 * @return true if successful, false otherwise
 */
bool getImageFileHeaders(const string& filePath, IMAGE_NT_HEADERS& headers)
{
    const wstring wideFilePath(filePath.begin(), filePath.end());
    
    // Use RAII for file handle
    struct FileHandle {
        HANDLE handle;
        FileHandle(const wstring& path) : handle(CreateFile(path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0)) {}
        ~FileHandle() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        operator HANDLE() const { return handle; }
    } fileHandle(wideFilePath);

    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // Use RAII for mapping handle
    struct MappingHandle {
        HANDLE handle;
        MappingHandle(HANDLE file) : handle(CreateFileMapping(file,
            nullptr,
            PAGE_READONLY,
            0,
            0,
            nullptr)) {}
        ~MappingHandle() { if (handle) CloseHandle(handle); }
        operator HANDLE() const { return handle; }
    } mappingHandle(fileHandle);

    if (!mappingHandle)
    {
        return false;
    }

    // Use RAII for mapped view
    struct MappedView {
        void* ptr;
        MappedView(HANDLE mapping) : ptr(MapViewOfFile(mapping,
            FILE_MAP_READ,
            0,
            0,
            0)) {}
        ~MappedView() { if (ptr) UnmapViewOfFile(ptr); }
        operator void*() const { return ptr; }
    } mappedView(mappingHandle);

    if (!mappedView)
    {
        return false;
    }

    PIMAGE_NT_HEADERS headersPtr = ImageNtHeader(mappedView);
    if (!headersPtr)
    {
        return false;
    }

    headers = *headersPtr;
    return true;
}

/**
 * @brief Lists all exported functions from a DLL
 * @param dllPath Path to the DLL
 * @param functionNames Vector to store the function names
 */
void listDLLFunctions(const string& dllPath, vector<string>& functionNames)
{
    functionNames.clear();
    
    _LOADED_IMAGE loadedImage;
    if (!MapAndLoad(dllPath.c_str(), nullptr, &loadedImage, TRUE, TRUE))
    {
        return;
    }

    // Use RAII for cleanup
    struct ImageCleanup {
        _LOADED_IMAGE& image;
        ImageCleanup(_LOADED_IMAGE& img) : image(img) {}
        ~ImageCleanup() { UnMapAndLoad(&image); }
    } cleanup(loadedImage);

    unsigned long directorySize;
    _IMAGE_EXPORT_DIRECTORY* exportDirectory = (_IMAGE_EXPORT_DIRECTORY*)ImageDirectoryEntryToData(
        loadedImage.MappedAddress,
        false,
        IMAGE_DIRECTORY_ENTRY_EXPORT,
        &directorySize);

    if (!exportDirectory)
    {
        return;
    }

    DWORD* nameRVAs = (DWORD*)ImageRvaToVa(
        loadedImage.FileHeader,
        loadedImage.MappedAddress,
        exportDirectory->AddressOfNames,
        nullptr);

    if (!nameRVAs)
    {
        return;
    }

    for (size_t i = 0; i < exportDirectory->NumberOfNames; ++i)
    {
        const char* functionName = (char*)ImageRvaToVa(
            loadedImage.FileHeader,
            loadedImage.MappedAddress,
            nameRVAs[i],
            nullptr);

        if (functionName)
        {
            functionNames.push_back(functionName);
        }
    }
}

/**
 * @brief Generates a DEF file for the proxy DLL
 * @param dllName Name of the DLL
 * @param functionNames List of exported function names
 */
void generateDEF(const string& dllName, const vector<string>& functionNames)
{
    ofstream file(dllName + ".def");
    if (!file.is_open())
    {
        throw runtime_error("Failed to create DEF file");
    }

    file << "LIBRARY " << dllName << endl;
    file << "EXPORTS" << endl;

    for (size_t i = 0; i < functionNames.size(); ++i)
    {
        file << "\t" << functionNames[i] << "=Fake" << functionNames[i] << " @" << i + 1 << endl;
    }
}

/**
 * @brief Generates the main CPP file for the proxy DLL
 * @param dllName Name of the DLL
 * @param functionNames List of exported function names
 */
void generateMainCPP(const string& dllName, const vector<string>& functionNames)
{
    ofstream file(dllName + ".cpp");
    if (!file.is_open())
    {
        throw runtime_error("Failed to create CPP file");
    }

    const size_t fileNameLength = dllName.size() + 6;

    file << "#include <windows.h>" << endl << endl;

    // Generate DLL structure
    file << "struct " << dllName << "_dll {\n"
         << "\tHMODULE dll;\n";

    for (const auto& functionName : functionNames)
    {
        file << "\tFARPROC Orignal" << functionName << ";\n";
    }
    file << "} " << dllName << ";\n\n";

    // Generate exports based on architecture
    file << "extern \"C\"\n{\n";

    if (g_architectureType == IMAGE_FILE_MACHINE_AMD64)
    {
        // 64-bit implementation using RunASM
        file << "\tFARPROC PA = 0;\n"
             << "\tint RunASM();\n\n";

        for (const auto& functionName : functionNames)
        {
            file << "\tvoid Fake" << functionName << "() { PA = " << dllName << ".Orignal" << functionName << "; RunASM(); }\n";
        }
    }
    else if (g_architectureType == IMAGE_FILE_MACHINE_I386)
    {
        // 32-bit implementation using inline assembly
        for (const auto& functionName : functionNames)
        {
            file << "\t__declspec(naked) void Fake" << functionName << "()\n"
                 << "\t{\n"
                 << "\t\t__asm\n"
                 << "\t\t{\n"
                 << "\t\t\tpush ebp\n"
                 << "\t\t\tmov ebp, esp\n"
                 << "\t\t\tjmp dword ptr [" << dllName << ".Orignal" << functionName << "]\n"
                 << "\t\t}\n"
                 << "\t}\n";
        }
    }
    else
    {
        throw runtime_error("Unsupported architecture type");
    }

    file << "}\n\n";

    // Generate setup function
    file << "void setupFunctions() {\n";
    for (const auto& functionName : functionNames)
    {
        file << "\t" << dllName << ".Orignal" << functionName << " = GetProcAddress(" << dllName << ".dll, \"" << functionName << "\");\n";
    }
    file << "}\n\n";

    // Generate DllMain
    file << "BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {\n"
         << "\tchar path[MAX_PATH];\n"
         << "\tswitch (ul_reason_for_call)\n"
         << "\t{\n"
         << "\tcase DLL_PROCESS_ATTACH:\n"
         << "\t{\n"
         << "\t\tCopyMemory(path + GetSystemDirectory(path, MAX_PATH - " << fileNameLength << "), \"\\\\" << dllName << ".dll\", " << fileNameLength + 1 << ");\n"
         << "\t\t" << dllName << ".dll = LoadLibrary(path);\n"
         << "\t\tif (" << dllName << ".dll == false)\n"
         << "\t\t{\n"
         << "\t\t\tMessageBox(0, \"Cannot load original " << dllName << ".dll library\", \"Proxy\", MB_ICONERROR);\n"
         << "\t\t\tExitProcess(0);\n"
         << "\t\t}\n"
         << "\t\tsetupFunctions();\n"
         << "\t\tbreak;\n"
         << "\t}\n"
         << "\tcase DLL_PROCESS_DETACH:\n"
         << "\t{\n"
         << "\t\tFreeLibrary(" << dllName << ".dll);\n"
         << "\t}\n"
         << "\tbreak;\n"
         << "\t}\n"
         << "\treturn TRUE;\n"
         << "}\n";
}

/**
 * @brief Generates the ASM file for the proxy DLL
 * @param dllName Name of the DLL
 */
void generateASM(const string& dllName)
{
    // Only generate ASM file for 64-bit
    if (g_architectureType != IMAGE_FILE_MACHINE_AMD64)
    {
        return;
    }

    ofstream file(dllName + ".asm");
    if (!file.is_open())
    {
        throw runtime_error("Failed to create ASM file");
    }

    file << ".data\n"
         << "extern PA : qword\n"
         << ".code\n"
         << "RunASM proc\n"
         << "    jmp qword ptr [PA]\n"
         << "RunASM endp\n"
         << "end\n";
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            cerr << "Usage: " << argv[0] << " <path_to_dll>" << endl;
            return 1;
        }

        const string dllPath = argv[1];
        if (!filesystem::exists(dllPath))
        {
            cerr << "Error: File does not exist: " << dllPath << endl;
            return 1;
        }

        IMAGE_NT_HEADERS headers;
        if (!getImageFileHeaders(dllPath, headers))
        {
            cerr << "Error: Failed to read DLL headers" << endl;
            return 1;
        }

        g_architectureType = headers.FileHeader.Machine;
        
        // Check for supported architectures
        if (g_architectureType != IMAGE_FILE_MACHINE_AMD64 && g_architectureType != IMAGE_FILE_MACHINE_I386)
        {
            cerr << "Error: Unsupported architecture type" << endl;
            return 1;
        }

        // Extract DLL name without extension
        vector<string> pathComponents = splitString(dllPath, '\\');
        string dllName = pathComponents.back();
        dllName = dllName.substr(0, dllName.size() - 4);

        // Get exported function names
        listDLLFunctions(dllPath, g_exportedNames);
        if (g_exportedNames.empty())
        {
            cerr << "Warning: No exported functions found in the DLL" << endl;
        }

        // Generate files
        generateDEF(dllName, g_exportedNames);
        generateMainCPP(dllName, g_exportedNames);
        
        // Only generate ASM file for 64-bit
        if (g_architectureType == IMAGE_FILE_MACHINE_AMD64)
        {
            generateASM(dllName);
        }

        cout << "Successfully generated proxy DLL files for: " << dllName << endl;
        cout << "Architecture: " << (g_architectureType == IMAGE_FILE_MACHINE_AMD64 ? "64-bit" : "32-bit") << endl;
        cout << "Number of exported functions: " << g_exportedNames.size() << endl;
        return 0;
    }
    catch (const exception& e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}