:: ENTRY POINT
::
@echo off
@SETLOCAL EnableDelayedExpansion

:: Change active working directory in case we run script for outside of TheForge
cd /D "%~dp0"

set filename=Art.zip
if exist %filename% (
    del %filename%
)

echo Pulling Art Assets
"Tools/wget" -O %filename% http://www.conffx.com/%filename%

echo Unzipping Art Assets...
"Tools/7z" x %filename% -y > NUL

:: Copy ZipFiles to the right location as they're big to be committed without lfs
if exist "Art/ZipFilesDds" (
	move /Y "Art/ZipFilesDds" "Examples_3/Unit_Tests/UnitTestResources"
)
if exist "Art/ZipFilesKtx" (
	move /Y "Art/ZipFilesKtx" "Examples_3/Unit_Tests/UnitTestResources"
)

echo Finishing up...
del %filename%

exit /b 0
