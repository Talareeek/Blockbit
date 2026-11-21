@echo off
setlocal
set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build-windows-native"

cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
cmake --build "%BUILD_DIR%" --parallel
exit /b %errorlevel%
