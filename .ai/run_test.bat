@echo off
chcp 65001>nul

cd E:\Programs\SL-Cpp

echo ===== 开始同步项目 =====
echo.
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -G Ninja -S . -B cmake-build-debug

if %errorlevel% neq 0 (
    echo.
    echo ===== 同步项目失败，已跳过构建和测试 =====
    exit /b %errorlevel%
)

echo.
echo ===== 开始构建 =====
echo.
cmake --build cmake-build-debug --target all -j 22

if %errorlevel% neq 0 (
    echo.
    echo ===== 构建失败，已跳过测试 =====
    exit /b %errorlevel%
)

echo.
echo ===== 开始测试 =====
echo.
cd cmake-build-debug
ctest --extra-verbose -j 22
