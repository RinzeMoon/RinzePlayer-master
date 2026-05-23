# Build Instructions

## Option 1: Use CMake (Recommended)

```bash
cd examples
mkdir build
cd build
cmake .. -DFFMPEG_DIR="D:/ffmpeg-8.0-full_build-shared"
cmake --build . --config Release
Release\feature_check.exe
```

## Option 2: Use the batch script (Windows only)

1. Open "x64 Native Tools Command Prompt for VS 2022"
2. Navigate to the examples directory: `cd d:\TraeProject\RinzePlayer\examples`
3. Edit `build.bat` to set the correct `FFMPEG_DIR` if needed
4. Run: `build.bat`

## Option 3: Manual compile (Command Prompt)

```batch
REM Run from "x64 Native Tools Command Prompt for VS 2022"
set FFMPEG_DIR=D:\ffmpeg-8.0-full_build-shared
cl /EHsc /std:c++17 /I"%FFMPEG_DIR%\include" ^
    example_main.cpp ffmpeg_feature_check.cpp vulkan_check.cpp ^
    /link /LIBPATH:"%FFMPEG_DIR%\lib" avfilter.lib avutil.lib ^
    /OUT:feature_check.exe

REM Copy DLLs
copy "%FFMPEG_DIR%\bin\*.dll" .\
```

## What you need

- Visual Studio 2022 (with C++ tools)
- FFmpeg development files (with libplacebo support)
- Vulkan Runtime (for Vulkan check to pass)
