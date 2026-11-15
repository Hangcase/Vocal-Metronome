@echo off
setlocal
cd /d "%~dp0"
where git >nul 2>nul
if errorlevel 1 (
 echo Git is required. Install Git for Windows, then reopen this script.
 pause
 exit /b 1
)
set "VOCAL_METRONOME_CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
 set "VOCAL_METRONOME_CMAKE=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
"%VOCAL_METRONOME_CMAKE%" -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto failed
"%VOCAL_METRONOME_CMAKE%" --build build --config Release --target VocalMetronome_VST3 --parallel
if errorlevel 1 goto failed
echo.
echo Build succeeded. Your plugin is in:
echo %CD%\build\VocalMetronome_artefacts\Release\VST3\Vocal-Metronome.vst3
echo Copy the ENTIRE Vocal-Metronome.vst3 folder to C:\Program Files\Common Files\VST3
echo Then scan for plugins in FL Studio. See README.md for details.
pause
exit /b 0
:failed
echo.
echo Build failed. Read the error above. No plugin has been installed.
pause
exit /b 1
