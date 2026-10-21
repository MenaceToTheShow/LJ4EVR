@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d "D:\lj4evr\spikes\c-engineering"
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl > build-ninja-configure.log 2>&1
echo configure exit=%errorlevel%
cmake --build build-ninja > build-ninja-build.log 2>&1
echo build exit=%errorlevel%

