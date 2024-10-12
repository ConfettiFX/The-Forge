@echo off
set cwd=%cd%
IF "%cwd:~-9%" == "The-Forge" (
    Tools\python-3.6.0-embed-amd64\python.exe Common_3\Tools\ReloadServer\ReloadServer.py %*
) ELSE (
    if "%cwd:~-12%" == "ReloadServer" (
        ..\..\..\Tools\python-3.6.0-embed-amd64\python.exe ReloadServer.py %*
    ) ELSE (
        echo "ERROR: You must execute the daemon script from The-Forge root directory or by double clicking: .\\Common_3\\Tools\\ReloadServer\\ReloadServer.bat"
    )
)