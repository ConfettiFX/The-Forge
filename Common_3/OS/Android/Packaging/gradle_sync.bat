@echo off

SETLOCAL
IF "%GRADLE_MAX_CONCURRENT_PROCESSES%"=="" set GRADLE_MAX_CONCURRENT_PROCESSES=4
set PARENT_DIR=%~dp0
set GRADLEW=%PARENT_DIR%gradlew.bat
set MUTEX_FILE="%tmp%\gradle_sync.lock"



:gradle_sync_lock
@REM Ignore Error stream (outputs file open errors)
2>nul (
	@REM Open file for write (acts as try lock mutex). 
	@REM Stream 3 is unused stream that allows us to not write anything into the file.
	3>%MUTEX_FILE% (
		(call %PARENT_DIR%gradle_sync_impl.bat)
	)
) || goto :gradle_sync_lock

echo gradle_sync.bat: %BUSY_PROC_COUNT%/%GRADLE_MAX_CONCURRENT_PROCESSES% gradle instances running. Continuing build.

ENDLOCAL