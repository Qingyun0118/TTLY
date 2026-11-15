@echo off
setlocal enabledelayedexpansion

REM 显式指定待处理音频目录
if "%~1"=="" (
    echo Usage: %~nx0 "audio-folder"
    exit /b 1
)
cd /d "%~1" || exit /b 1

REM 设置计数器初始值
set count=1

REM 按时间顺序遍历音频文件（支持常见格式）
for /f "delims=" %%a in ('dir /b /a-d /od *.mp3 *.wav *.flac *.aac *.m4a *.ogg *.wma 2^>nul ^| findstr /v "%~nx0"') do (
    REM 格式化序号为两位数
    if !count! lss 10 (
        ren "%%a" "0!count!%%~xa"
    ) else (
        ren "%%a" "!count!%%~xa"
    )
    set /a count+=1
)

echo 重命名完成！
pause