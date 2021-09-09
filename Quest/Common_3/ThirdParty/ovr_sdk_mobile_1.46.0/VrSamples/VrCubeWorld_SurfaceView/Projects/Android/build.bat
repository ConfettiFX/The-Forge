@rem Only edit the master copy of this file in SDK_ROOT/bin/scripts/build/perproject

@setlocal enableextensions enabledelayedexpansion

@if not exist "build.gradle" @echo Build script must be executed from project directory. & goto :Abort

@set P=..

:TryAgain

@rem @echo P = %P%

@if exist "%P%\bin\scripts\build\build.py.bat" goto :Found

@if exist "%P%\bin\scripts\build" @echo "Could not find build.py.bat" & goto :Abort

@set P=%P%\..

@goto :TryAgain

:Found

@set P=%P%\bin\scripts\build
@call %P%\build.py.bat %1 %2 %3 %4 %5
@goto :End

:Abort

:End
