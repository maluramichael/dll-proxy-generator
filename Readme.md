# Dll-Proxy Generator

This project creates a new dll which sits between a game and the original dll. This way you can intercept all dll calls.

Game -> Your proxy dll -> Original dll

Original by Kristoffer Blasiak (https://www.codeproject.com/Articles/1179147/ProxiFy-Automatic-Proxy-DLL-Generation) i modified his project so the output fits my needs. 

## Build

Open DllProxyGenerator.sln with Visual Studio and build it

## Usage

.\DllProxyGenerator.exe "path\to\your\dll"

Be careful which dlls you try to proxy. I tried public windows dlls like d3d9 or user32 whch work great. Game specific dlls with mangled function names wont work. Except someone knows how to counter this problem.