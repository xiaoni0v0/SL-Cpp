@echo off
chcp 65001>nul

set PATH=C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin;C:\Program Files\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin;C:\Program Files\JetBrains\CLion 2026.1\bin\ninja;%PATH%

cd E:\Programs\SL-Cpp

echo ===== 开始构建 =====
echo.
cmake --build cmake-build-debug --target all -j 22

if %errorlevel% neq 0 (
    echo.
    echo ===== 构建失败,已跳过测试 =====
    exit /b %errorlevel%
)

echo.
echo ===== 开始测试 =====
echo.
cd cmake-build-debug
ctest --extra-verbose
