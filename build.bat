@echo off

set BUILD_DIR=%CURRENT_DIR%\build
set SOURCE_DIR=%CURRENT_DIR%\source

call "E:\Visual Studio\VC\Auxiliary\Build\vcvarsall.bat" x64

pushd "%BUILD_DIR%

del myapp.pdb

cl /std:c++latest -Zi -D_AMD64_ -I"..\include" -Fd"myapp.pdb" -Fo"main.obj" -Fe"main.exe" "%SOURCE_DIR%\main.cpp" /link /LIBPATH:"..\lib" SDL3.lib d3d12.lib dxgi.lib d3dcompiler.lib 

if "%1"=="debug" (devenv main.exe) else if "%1"=="run" (main.exe)

popd
