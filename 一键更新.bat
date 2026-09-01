@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  HanPinyin 一键更新（双击即用，无需 Python / 无需重装小狼毫）
REM
REM  原理（已查证 Rime/小狼毫官方机制，见 rime/weasel/WeaselDeployer.cpp）：
REM    小狼毫的"重新部署" = 运行  WeaselDeployer.exe /deploy
REM    它会把 %APPDATA%\Rime 下的 *.schema.yaml + *.dict.yaml
REM    重新编译成 build\*.bin（table.bin / prism.bin），打字才生效。
REM    单纯重启 WeaselServer.exe 不等于重新部署，所以必须用 /deploy。
REM ============================================================

set "RIME=%APPDATA%\Rime"
set "SRC=%~dp0rime\"

echo ============================================================
echo   HanPinyin update  (uses WeaselDeployer.exe /deploy)
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

echo [2/3] Locating WeaselDeployer.exe
set "DEP="
set "WS="
REM 优先：从正在运行的 WeaselServer.exe 所在目录取 WeaselDeployer.exe（同目录）
for /f "tokens=*" %%p in ('powershell -NoProfile -Command "(Get-Process WeaselServer -ErrorAction SilentlyContinue | Select-Object -First 1).Path" 2^>nul') do (
    set "WS=%%p"
)
if defined WS (
    for %%I in ("%WS%") do set "WSDIR=%%~dpI"
    if exist "%WSDIR%WeaselDeployer.exe" set "DEP=%WSDIR%WeaselDeployer.exe"
)
REM 回退：常见安装目录（版本号目录名会变，所以逐个试）
if not defined DEP (
    for %%d in (
        "%ProgramFiles(x86)%\Rime\Weasel\WeaselDeployer.exe"
        "%ProgramFiles%\Rime\Weasel\WeaselDeployer.exe"
        "%LOCALAPPDATA%\Programs\Weasel\WeaselDeployer.exe"
        "%LOCALAPPDATA%\Rime\Weasel\WeaselDeployer.exe"
    ) do (
        if not defined DEP if exist %%d set "DEP=%%d"
    )
)
if not defined DEP (
    echo   [ERROR] WeaselDeployer.exe not found. Is Weasel installed?
    goto :end
)
echo   found: %DEP%

echo [3/3] Stopping server, rebuilding, redeploying
REM 停掉小狼毫服务，释放 build 目录占用
taskkill /IM WeaselServer.exe /F >nul 2>nul
timeout /t 1 >nul
REM 清掉本方案旧的编译缓存，强制重新生成
if exist "%RIME%\build" (
    del /Q "%RIME%\build\hanpinyin.*" 2>nul
    del /Q "%RIME%\build\sino_mix.*" 2>nul
)
REM 真正的"重新部署"：WeaselDeployer.exe /deploy
"%DEP%" /deploy
echo   deployment done.
REM 重新启动小狼毫服务（加载刚编译好的 build/*.bin）
for %%I in ("%DEP%") do set "DEPDIR=%%~dpI"
start "" "%DEPDIR%WeaselServer.exe"
echo   WeaselServer restarted.

echo.
echo ============================================================
echo   Done! Switch to HanPinyin to use the update.
echo   If HanPinyin is not in the input list, right-click tray ->
echo   "设定" and enable "韩文拼音 HanPinyin" once.
echo ============================================================

:end
pause
