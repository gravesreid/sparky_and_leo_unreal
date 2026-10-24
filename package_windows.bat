@echo off
rem Builds a standalone Windows version of Sparky and Leo and zips it up, ready to send to friends.
rem Run this on a Windows PC that has:
rem   - Unreal Engine 5.2 (free, from the Epic Games Launcher)
rem   - Visual Studio 2022 with the "Game development with C++" workload
rem Takes 20-60 minutes the first time. The result is SparkyAndLeo-Windows.zip next to this file.
setlocal
set "HERE=%~dp0"
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.2"
if not exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Can't find Unreal Engine 5.2 at "%UE_ROOT%".
    echo Install it from the Epic Games Launcher, or set UE_ROOT to the folder it's in.
    pause
    exit /b 1
)

echo === 1/3: compiling the game code for the editor (needed to cook the content)
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" SparkyLeoEditor Win64 Development -Project="%HERE%SparkyLeo.uproject" -WaitMutex
if errorlevel 1 goto failed

echo === 2/3: building, cooking and packaging the game
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%HERE%SparkyLeo.uproject" -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -package -archive -archivedirectory="%HERE%Packaged" -prereqs -nodebuginfo -unattended -utf8output
if errorlevel 1 goto failed

echo === 3/3: zipping
copy /y "%HERE%HOW_TO_PLAY.txt" "%HERE%Packaged\Windows\HOW_TO_PLAY.txt" >nul
powershell -NoProfile -Command "Compress-Archive -Force -Path '%HERE%Packaged\Windows\*' -DestinationPath '%HERE%SparkyAndLeo-Windows.zip'"
if errorlevel 1 goto failed
echo.
echo Done! Send SparkyAndLeo-Windows.zip to your friends.
pause
exit /b 0

:failed
echo.
echo Something went wrong - the messages above say what.
pause
exit /b 1
