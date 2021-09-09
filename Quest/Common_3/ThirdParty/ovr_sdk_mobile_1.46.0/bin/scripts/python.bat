
@rem This script simply locates python.exe inside of Android NDK
@rem and executes it with the requested command-line parameters
@rem this allows us to use python on windows without explicitly
@rem needing it installed stand-alone.

@setlocal enableextensions enabledelayedexpansion

@set P=


@IF exist "%ANDROID_NDK_HOME%" goto :HasNDK


@for /f "delims=;" %%i in ('where ndk-build.cmd') do @set P=%%i
@set ANDROID_NDK_HOME=%P:ndk-build.cmd=%
@echo setting ANDROID_NDK_HOME = %ANDROID_NDK_HOME%


:HasNDK


@IF NOT exist "%ANDROID_NDK_HOME%" (

	@echo Cannot find the Android NDK

	goto :End
)


@if exist "%ANDROID_NDK_HOME%\prebuilt\windows\bin\python.exe" (

	@"%ANDROID_NDK_HOME%\prebuilt\windows\bin\python.exe" %1 %2 %3 %4 %5 %6
) else (

	@if exist "%ANDROID_NDK_HOME%\prebuilt\windows-x86_64\bin\python.exe" (
		@"%ANDROID_NDK_HOME%\prebuilt\windows-x86_64\bin\python.exe" %1 %2 %3 %4 %5 %6
	) else (
		@echo Cannot find python.exe in the NDK.
		goto :End
	)
)

:End
