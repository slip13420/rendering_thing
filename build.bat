@echo off
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%"
set "BUILD_DIR=%PROJECT_ROOT%build"

echo Path Tracer Renderer - Build Script
echo ====================================
echo Platform: Windows
echo.

set CLEAN_BUILD=false
set BUILD_TYPE=Debug
set NUM_JOBS=4
set USE_GLFW=false
set NO_WINDOWING=false
set SHOW_HELP=false

:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="-c" set CLEAN_BUILD=true
if "%~1"=="--clean" set CLEAN_BUILD=true
if "%~1"=="-r" set BUILD_TYPE=Release
if "%~1"=="--release" set BUILD_TYPE=Release
if "%~1"=="-j" (
    shift
    set NUM_JOBS=%~1
)
if "%~1"=="--jobs" (
    shift
    set NUM_JOBS=%~1
)
if "%~1"=="--use-glfw" set USE_GLFW=true
if "%~1"=="--no-windowing" set NO_WINDOWING=true
if "%~1"=="-h" set SHOW_HELP=true
if "%~1"=="--help" set SHOW_HELP=true
shift
goto parse_args

:end_parse

if "%SHOW_HELP%"=="true" (
    echo Usage: %0 [options]
    echo.
    echo Options:
    echo   -c, --clean          Clean build directory before building
    echo   -r, --release        Build in Release mode ^(default: Debug^)
    echo   -j, --jobs N         Number of parallel jobs for building
    echo   --use-glfw           Use GLFW instead of SDL for windowing
    echo   --no-windowing       Build without windowing support (console only)
    echo   -h, --help           Show this help message
    echo.
    exit /b 0
)

if "%CLEAN_BUILD%"=="true" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo Configuring with CMake...
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if "%NO_WINDOWING%"=="true" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DUSE_SDL=OFF -DUSE_GLFW=OFF
    echo Building without windowing support (console only)
) else if "%USE_GLFW%"=="true" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DUSE_SDL=OFF -DUSE_GLFW=ON
    echo Using GLFW for windowing
) else (
    set CMAKE_ARGS=%CMAKE_ARGS% -DUSE_SDL=ON -DUSE_GLFW=OFF
    echo Using SDL for windowing
)

cmake %CMAKE_ARGS% "%PROJECT_ROOT%"
if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo.
echo Building with %NUM_JOBS% parallel jobs...
cmake --build . --config "%BUILD_TYPE%" -j %NUM_JOBS%
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executable location: %BUILD_DIR%\src\%BUILD_TYPE%\path_tracer_renderer.exe
echo.
echo To run the application:
echo   cd "%BUILD_DIR%\src\%BUILD_TYPE%" ^&^& path_tracer_renderer.exe