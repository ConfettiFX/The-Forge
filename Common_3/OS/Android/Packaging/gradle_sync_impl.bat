@REM Don't use this file directly. Use gradle_sync.bat instead.

@REM Get unique file name
:uniqueNameLoop
set uniqueFileName="%tmp%\gradle_sync-#%RANDOM%.tmp"
if exist "%uniqueFileName%" goto :uniqueNameLoop

@REM Save results of gradlew status into temp file
call %GRADLEW% --status > %uniqueFileName%

set BUSY_PROC_COUNT=0

@REM Count number of active processes
for /f "tokens=4 delims=: " %%a in ('find /c "BUSY" %uniqueFileName% 2^>nul') do (
set BUSY_PROC_COUNT=%%a
)

@REM Delete temp file
del %uniqueFileName%

IF %BUSY_PROC_COUNT% geq %GRADLE_MAX_CONCURRENT_PROCESSES% (
	echo gradle_sync.bat: %BUSY_PROC_COUNT%/%GRADLE_MAX_CONCURRENT_PROCESSES% gradle instances already running. Waiting.
	exit /b 1
)