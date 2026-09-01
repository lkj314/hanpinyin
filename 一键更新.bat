@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  HanPinyin 一键更新  ——  双击即可，无需 Python / 无需重装小狼毫
REM ============================================================
REM  本脚本只做两件事：
REM   1) 把 rime\sino_mix.schema.yaml 和 rime\hanpinyin.dict.yaml
REM      复制到  %APPDATA%\Rime\  （即小狼毫的用户目录）
REM   2) 重启小狼毫引擎，触发「重新部署」（自动重新生成词库）
REM  所以平时更新 = 双击本文件，完事。永远不用重装小狼毫。
REM ============================================================

set "RIME=%APPDATA%\Rime"
set "SRC=%~dp0rime\"

echo ============================================================
echo   HanPinyin update  (no Python / no reinstall needed)
echo ============================================================
echo.

if not exist "%RIME%" (
    echo [ERROR] Rime user folder not found: %RIME%
    echo Please install Weasel (XiaoLangHao) from https://rime.im first.
    goto :end
)

echo [1/3] Copying schema and dictionary to %RIME%
if exist "%SRC%sino_mix.schema.yaml" (
    if exist "%RIME%\sino_mix.schema.yaml" copy /Y "%RIME%\sino_mix.schema.yaml" "%RIME%\sino_mix.schema.yaml.bak" >nul
    copy /Y "%SRC%sino_mix.schema.yaml" "%RIME%\" >nul
    echo   copied sino_mix.schema.yaml
) else (
    echo   [WARN] missing %SRC%sino_mix.schema.yaml
)
if exist "%SRC%hanpinyin.dict.yaml" (
    if exist "%RIME%\hanpinyin.dict.yaml" copy /Y "%RIME%\hanpinyin.dict.yaml" "%RIME%\hanpinyin.dict.yaml.bak" >nul
    copy /Y "%SRC%hanpinyin.dict.yaml" "%RIME%\" >nul
    echo   copied hanpinyin.dict.yaml
) else (
    echo   [WARN] missing %SRC%hanpinyin.dict.yaml
)

echo [2/3] Clearing old build cache (force rebuild)
if exist "%RIME%\build" (
    del /Q "%RIME%\build\hanpinyin.*" 2>nul
    del /Q "%RIME%\build\sino_mix.*" 2>nul
)

echo [3/3] Redeploying Weasel (restart engine)
set "WS="
for /f "tokens=*" %%p in ('powershell -NoProfile -Command "(Get-Process WeaselServer -ErrorAction SilentlyContinue | Select-Object -First 1).Path" 2^>nul') do set "WS=%%p"

if defined WS (
    taskkill /IM WeaselServer.exe /F >nul 2>nul
    timeout /t 2 >nul
    start "" "%WS%"
    echo   WeaselServer restarted, redeploy triggered.
) else (
    echo   Weasel not running, trying to start it...
    if exist "%ProgramFiles(x86)%\Rime\Weasel\WeaselServer.exe" (
        start "" "%ProgramFiles(x86)%\Rime\Weasel\WeaselServer.exe"
        echo   started WeaselServer
    ) else if exist "%ProgramFiles%\Rime\Weasel\WeaselServer.exe" (
        start "" "%ProgramFiles%\Rime\Weasel\WeaselServer.exe"
        echo   started WeaselServer
    ) else if exist "%LOCALAPPDATA%\Programs\Weasel\WeaselServer.exe" (
        start "" "%LOCALAPPDATA%\Programs\Weasel\WeaselServer.exe"
        echo   started WeaselServer
    ) else if exist "%LOCALAPPDATA%\Rime\Weasel\WeaselServer.exe" (
        start "" "%LOCALAPPDATA%\Rime\Weasel\WeaselServer.exe"
        echo   started WeaselServer
    ) else (
        echo   [TIP] Could not auto-start Weasel.
        echo   Right-click the Weasel tray icon -> Redeploy.
    )
)

echo.
echo ============================================================
echo   Done! Switch to HanPinyin to use the update.
echo   If candidates did not change, right-click tray -> Redeploy.
echo ============================================================

:end
pause
