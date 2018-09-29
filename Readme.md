# Dll-Proxy Generator

This project creates a new dll which sits between a game and the original dll. This way you can intercept all dll calls.

Game -> Your proxy dll -> Original dll

Original by Kristoffer Blasiak (https://www.codeproject.com/Articles/1179147/ProxiFy-Automatic-Proxy-DLL-Generation) i modified his project so the output fits my needs. 

## Build

Open DllProxyGenerator.sln with Visual Studio and build it

## Usage

### Generate the proxy dll source
.\DllProxyGenerator.exe "path\to\your\dll"

Be careful which dlls you try to proxy. I tried public windows dlls like d3d9 or user32 whch work great. Game specific dlls with mangled function names wont work. Except someone knows how to counter this problem.

### Build the proxy dll
Create a new Visual Studio dll project. Copy the generated proxy files into your project.

Remove every other file like stdafx.h

Change the following settings.

* General > Project Defaults > Character Set = Use Multi-Byte Character Set
* C/C++ > Precompiled Headers > Precompiled Header = Not Using Precompiled Headers

### Use the new dll
Copy your new proxy inside the game directory. Some games have a different load mechanism. Depending on which dll you generated it could be possible that the game does not load your dll first in which case the proxy doesnt work.