@echo off
REM StarryAgent Windows ARM64 build script
REM Portable: uses vswhere for VS detection, STARRY_QT_ROOT for Qt path.
REM Usage: build_winarm64.bat [debug|release]

setlocal enabledelayedexpansion

set "BUILD_MODE=%~1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

REM --- Project root (relative to script location) ---
set "PROJECT_DIR=%~dp0.."
pushd "%PROJECT_DIR%" || (echo Error: cannot cd to project root & exit /b 1)

echo === StarryAgent Windows ARM64 build (%BUILD_MODE%) ===
echo Project: %PROJECT_DIR%

REM --- Find Visual Studio installation via vswhere ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: vswhere not found. Visual Studio 2022+ required.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath`) do set "VS_INSTALL_DIR=%%i"
if "%VS_INSTALL_DIR%"=="" (
    REM Fallback: check common BuildTools paths directly.
    for /f "tokens=*" %%a in ('"echo %ProgramFiles(x86)%"') do set "PF86=%%a"
    if exist "%PF86%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VS_INSTALL_DIR=%PF86%\Microsoft Visual Studio\2022\BuildTools"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VS_INSTALL_DIR=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
    ) else (
        echo Error: Visual Studio with ARM64 tools not found.
        echo Install VS 2022/2026 BuildTools or Community with ARM64 components.
        exit /b 1
    )
)
echo VS: %VS_INSTALL_DIR%

REM --- Find Qt ARM64 installation ---
if "%STARRY_QT_ROOT%"=="" (
    if exist "C:\Qt\6.8.3\msvc2022_arm64\bin\qmake.bat" (
        set "QT_ARM64=C:\Qt\6.8.3\msvc2022_arm64"
    ) else if exist "C:\Qt\6.8.3\msvc2022_arm64\bin\qmake.exe" (
        set "QT_ARM64=C:\Qt\6.8.3\msvc2022_arm64"
    ) else (
        REM Fallback: try to find any Qt ARM64 installation
        for /d %%i in (C:\Qt\6.*\msvc2022_arm64) do if exist "%%i\bin\qmake.bat" set "QT_ARM64=%%i"
        if "!QT_ARM64!"=="" for /d %%i in (C:\Qt\6.*\msvc2022_arm64) do if exist "%%i\bin\qmake.exe" set "QT_ARM64=%%i"
        if "!QT_ARM64!"=="" (
            echo Error: Qt ARM64 not found. Set STARRY_QT_ROOT to the Qt ARM64 installation path.
            exit /b 1
        )
    )
) else (
    set "QT_ARM64=%STARRY_QT_ROOT%"
)
echo Qt ARM64: %QT_ARM64%

REM --- Find Qt host tools (x64) ---
set "QT_HOST="
if not "%STARRY_QT_HOST%"=="" (
    set "QT_HOST=%STARRY_QT_HOST%"
) else if exist "%QT_ARM64:\msvc2022_arm64=\msvc2022_64%" (
    set "QT_HOST=%QT_ARM64:\msvc2022_arm64=\msvc2022_64%"
)
if "%QT_HOST%"=="" (
    echo Warning: Qt host tools not found. Set STARRY_QT_HOST if moc/rcc fail.
) else (
    echo Qt host: %QT_HOST%
)

REM --- Ensure qmake.exe exists in ARM64 Qt dir (xmake needs it) ---
if not exist "%QT_ARM64%\bin\qmake.exe" (
    if exist "%QT_HOST%\bin\qmake.exe" (
        copy /y "%QT_HOST%\bin\qmake.exe" "%QT_ARM64%\bin\qmake.exe" >nul
        echo Copied qmake.exe from host tools to ARM64 Qt dir.
    ) else if exist "%QT_ARM64%\bin\qmake.bat" (
        copy /y "%QT_ARM64%\bin\qmake.bat" "%QT_ARM64%\bin\qmake.exe" >nul
        echo Created qmake.exe from qmake.bat.
    ) else (
        echo Error: qmake not found in Qt ARM64 or host tools.
        exit /b 1
    )
)

REM --- Set up VC environment for ARM64 cross-compilation ---
REM Use short path name to avoid (x86) parenthesis issues with call.
for %%i in ("%VS_INSTALL_DIR%") do set "VCVARS=%%~si\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    set "VCVARS=%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvarsall.bat"
    if not exist "%VCVARS%" (
        echo Error: vcvarsall.bat not found at %VCVARS%
        exit /b 1
    )
)
call "%VCVARS%" x64_arm64
if errorlevel 1 (
    echo Error: vcvarsall.bat x64_arm64 failed.
    exit /b 1
)
echo VC env: x64_arm64

REM --- Configure xmake ---
echo Configuring xmake...
xmake f -c -p windows -a arm64 -m "%BUILD_MODE%" ^
    --qt="%QT_ARM64%" ^
    --qt_host="%QT_HOST%" ^
    --toolchain=msvc -y
if errorlevel 1 (
    echo Error: xmake configuration failed.
    exit /b 1
)

REM --- Build ---
echo Building...
xmake -y
if errorlevel 1 (
    echo Error: build failed.
    exit /b 1
)

echo === Build OK ===
popd
exit /b 0