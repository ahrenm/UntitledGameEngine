# Untitled Game Engine

The Untitled Game Engine is an experimental 2d game engine/framework written entirely by AI, predominantly Claude Sonnet 4.6.

A feature demonstration project is included with this repository.

__Dependency Notice__: This project relies on open source third-party libraries which are acquired via CMake's FetchContent module.

For a full list of dependencies, please refer to AGENTS.md

THIS RELEASE IS NOT INTENDED FOR PUBLIC CONSUMPTION.

## Build Instructions

### Using CLion (Highly Recommended)

Default CLion toolchain (Bundeled MinGW) : CMake->Ninja->g++ (x86_64)

1. Open the project root in CLion
2. CMake configuration happens automatically (Monitor via CMake output window)
3. Accept default MinGW toolchain when prompted
4. Build and run the project using CLion's build and run buttons (untitled)


### Visual Studio Community Edition 2026

Visual Studio Workloads:
* Desktop development with C++
* Linux, Mac and embedded development with C++ (required for CMake support)

MinGW toolchain (x86_64):
1. Install MinGW-w64 from [Mingw-w64](https://github.com/niXman/mingw-builds-binaries/releases) 
__Direct link:__ (https://github.com/niXman/mingw-builds-binaries/releases/download/16.1.0-rt_v14-rev0/x86_64-16.1.0-release-mcf-seh-ucrt-rt_v14-rev0.7z)
2. Add MinGW bin directory to system or user PATH (e.g., `C:\mingw-w64\mingw64\bin)

Lunch Visual Studio
1. Open the project root in Visual Studio and allow CMake to configure the project (Monitor via Output window)
2. Select "untitled.exe" from build configurations dropdown in Visual Studio

NOTE: IntelliSense is badly broken in this project. Recommend disabling Squiggles via `Tools->Options->Text Editor->General->Display->Show Error squiggles (off)`.
