@echo off
REM This script initializes the x64 Native Tools Command Prompt and then runs build_win64.bat.
REM IMPORTANT: You may need to adjust the path to vcvars64.bat below if your Visual Studio installation is different.
REM Common paths include:
REM "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
REM "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

CALL "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
IF %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to initialize x64 Native Tools Command Prompt. Please check the path to vcvars64.bat in start_build.bat.
    pause
    exit /b %ERRORLEVEL%
)

echo x64 Native Tools Command Prompt initialized. Running build_win64.bat...
CALL "%~dp0src\build_win64.bat"

echo Build process finished.
pause