@echo off
setlocal
set "BARON_REPO=%~dp0.."
if not defined BARON_CMAKE set "BARON_CMAKE=cmake"
if not defined BARON_QT set "BARON_QT=%BARON_REPO%\tools\Qt\5.15.2\msvc2019_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1
"%BARON_CMAKE%" -S "%BARON_REPO%" -B "%BARON_REPO%\build\windows" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%BARON_QT%"
if errorlevel 1 exit /b 1
"%BARON_CMAKE%" --build "%BARON_REPO%\build\windows"
exit /b %errorlevel%
