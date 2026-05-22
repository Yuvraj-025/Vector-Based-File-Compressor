@echo off
echo ============================================
echo  YVC - Vector-Based Semantic Compressor
echo  Build Script
echo ============================================
echo.

:: Create build directory
if not exist build mkdir build
cd build

:: Clean any stale CMake cache to avoid generator conflicts
if exist CMakeCache.txt del /f CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles

:: Configure with CMake - try generators in order
echo [1/2] Configuring with CMake...

:: Try Visual Studio 17 2022
cmake .. -G "Visual Studio 17 2022" -A x64
if not errorlevel 1 goto build_step

echo.
echo Trying MinGW Makefiles...
if exist CMakeCache.txt del /f CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles
cmake .. -G "MinGW Makefiles"
if not errorlevel 1 goto build_step

echo.
echo Trying Ninja...
if exist CMakeCache.txt del /f CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles
cmake .. -G "Ninja"
if not errorlevel 1 goto build_step

echo.
echo Trying NMake Makefiles...
if exist CMakeCache.txt del /f CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles
cmake .. -G "NMake Makefiles"
if not errorlevel 1 goto build_step

echo ERROR: CMake configuration failed.
echo Make sure CMake and a C++ compiler are installed.
pause
exit /b 1

:build_step
:: Build
echo.
echo [2/2] Building...
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Build successful!
echo  Executable: build\Release\yvc.exe (or build\yvc.exe)
echo ============================================
echo.
echo Usage:
echo   yvc compress input.png -o output.yvc --creator "Yuvraj"
echo   yvc decompress output.yvc -o restored.png
echo   yvc info output.yvc
echo.
pause
