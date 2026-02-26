@echo off
chcp 65001>nul

:: 加载 VS 环境
echo [1/4] 正在加载 VS 编译环境...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

:: 检查上一步是否成功，如果找不到文件直接报错
if %errorlevel% neq 0 (
    echo [错误] 找不到 vcvars64.bat，请检查 VS 安装路径。
    pause
    exit /b
)

:: 编译 OpenCC (切换目录并执行)
echo [2/4] 正在编译 OpenCC...
if exist "3rdParty\OpenCC\" (
    pushd "3rdParty\OpenCC\"
    cmake -S . -B out -DCMAKE_INSTALL_PREFIX:PATH="%cd%\3rdParty\OpenCC\out\install\x64-Release"
    cmake --build out --config Release --target install
    popd
) else (
    echo [错误] 找不到 OpenCC 目录。
    pause
    exit /b
)

:: 编译
echo [3/4] 正在编译 GPPCLI 及其依赖项...
msbuild "GalTranslPP.slnx" /t:GPPCLI /p:Configuration=Release /p:Platform=x64 /p:CL_MPCount=0

:: 复制资产文件
echo [4/4] 正在复制资产文件到 Release 目录...
xcopy "Example\BaseConfig" "Release\GPPCLI\BaseConfig" /E /I /Y
xcopy "Example\sampleProject" "Release\GPPCLI\sampleProject" /E /I /Y

echo 完成
pause>nul