# DLL Proxy Generator

This project creates a new DLL that acts as an intermediary between a game and the original DLL, allowing you to intercept all DLL calls.

Game → Your proxy DLL → Original DLL

Originally created by Kristoffer Blasiak ([ProxiFy - Automatic Proxy DLL Generation](https://www.codeproject.com/Articles/1179147/ProxiFy-Automatic-Proxy-DLL-Generation)), this project has been modified to better suit specific needs.

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

Copy your new proxy DLL into the game directory. Note that some games may have different DLL loading mechanisms. Depending on which DLL you generated, the game might not load your proxy DLL first, in which case the proxy won't work.