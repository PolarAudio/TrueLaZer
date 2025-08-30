@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
pushd %~dp0
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
@set OUT_DIR=Debug
@set OUT_EXE=TrueLaZer
@set INCLUDES=/I..\libs\imgui /I..\libs\imgui\backends /I..\libs\glfw\include /I..\libs\dameisdk
@set SOURCES=main.cpp Showbridge.cpp ..\libs\imgui\backends\imgui_impl_glfw.cpp ..\libs\imgui\backends\imgui_impl_opengl3.cpp ..\libs\imgui\imgui.cpp ..\libs\imgui\imgui_demo.cpp ..\libs\imgui\imgui_draw.cpp ..\libs\imgui\imgui_tables.cpp ..\libs\imgui\imgui_widgets.cpp ..\sdk\DameiSDK.cpp ..\sdk\EasySocket.cpp ..\sdk\SDKSocket.cpp ..\sdk\SocketErrors.cpp
@set LIBS=/LIBPATH:..\libs\glfw\lib glfw3.lib opengl32.lib gdi32.lib shell32.lib ws2_32.lib iphlpapi.lib
mkdir %OUT_DIR%
rc /fo %OUT_DIR%\icon.res icon.rc
@echo %SOURCES% > build_sources.txt
cl /nologo /Zi /MD /utf-8 /EHsc /D_WIN32_WINNT=0x0601 /DGL_SILENCE_DEPRECATION %INCLUDES% @build_sources.txt %OUT_DIR%\icon.res /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link /MACHINE:X64 %LIBS%
del build_sources.txt

popd
popd