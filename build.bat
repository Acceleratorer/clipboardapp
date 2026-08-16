@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
g++ -std=gnu++11 -mwindows -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0501 -Isrc src\main.cpp src\AppState.cpp src\AllowedApps.cpp src\ClipboardLibrary.cpp src\ClipboardMonitor.cpp src\FileDialogAssist.cpp src\TextFileConverter.cpp src\Ui.cpp -o build\ClipboardTxtApp.exe -lshell32 -luser32 -lgdi32 -lole32 -luuid -lpsapi -lgdiplus -ladvapi32
if errorlevel 1 exit /b 1
echo Built build\ClipboardTxtApp.exe
