# DLL Proxy Generator

A tool for creating proxy DLLs that can intercept and modify DLL function calls. This is particularly useful for DLL hijacking and sideloading scenarios.

Game → Your proxy DLL → Original DLL

Originally created by Kristoffer Blasiak ([ProxiFy - Automatic Proxy DLL Generation](https://www.codeproject.com/Articles/1179147/ProxiFy-Automatic-Proxy-DLL-Generation)), this project has been modified to better suit specific needs.

## Overview

This tool generates a proxy DLL that can intercept all function calls to the original DLL. It's particularly useful for:
- DLL Hijacking: Intercepting DLL loads to modify behavior
- DLL Sideloading: Loading custom DLLs instead of system DLLs
- Function Hooking: Modifying or monitoring DLL function calls
- Security Testing: Analyzing DLL dependencies and behavior

## Building

Open `DllProxyGenerator.sln` with Visual Studio and build the solution.

## Usage

### Generate the Proxy DLL Source

```bash
.\DllProxyGenerator.exe "path\to\your\dll"
```

> **Note**: Be careful when selecting DLLs to proxy. Public Windows DLLs like `d3d9` or `user32` work well, but game-specific DLLs with mangled function names may not work. This limitation may be addressed in future updates.

### Building the Proxy DLL

1. Create a new Visual Studio DLL project
2. Copy the generated proxy files into your project
3. Remove all other files (like `stdafx.h`)
4. Update the following project settings:
   - General > Project Defaults > Character Set = Use Multi-Byte Character Set
   - C/C++ > Precompiled Headers > Precompiled Header = Not Using Precompiled Headers

### Using the New DLL

There are several ways to use the proxy DLL:

1. **Direct Replacement**:
   - Copy your proxy DLL into the game directory
   - Rename it to match the original DLL name
   - The game will load your proxy instead of the original

2. **DLL Hijacking**:
   - Place your proxy DLL in a directory that appears before the original DLL in the search order
   - Windows will load your proxy instead of the original

3. **Sideloading**:
   - Use the proxy DLL to load a different version of the original DLL
   - Useful for testing or compatibility purposes

> **Warning**: Some games may have different DLL loading mechanisms. Depending on which DLL you generated, the game might not load your proxy DLL first, in which case the proxy won't work.

## Technical Details

### Architecture Support
- 32-bit (x86): Uses inline assembly for function jumps
- 64-bit (x64): Uses MASM assembly procedure for function jumps

### Generated Files
- `.def`: Defines exported functions
- `.cpp`: Main proxy DLL implementation
- `.asm`: Assembly code for 64-bit function jumps

### Security Considerations
- Always verify DLL signatures
- Be cautious when proxying system DLLs
- Some antivirus software may flag proxy DLLs

## License

This project is open source and available under the MIT License.