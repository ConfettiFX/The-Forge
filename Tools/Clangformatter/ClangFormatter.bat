::
::	Copyright (c) 2017-2022 The Forge Interactive Inc.
::
:: 	This file is part of The-Forge
::	(see https://github.com/ConfettiFX/The-Forge).
::
:: 	Licensed to the Apache Software Foundation (ASF) under one
:: 	or more contributor license agreements.  See the NOTICE file
::	distributed with this work for additional information
::	regarding copyright ownership.  The ASF licenses this file
::	to you under the Apache License, Version 2.0 (the
:: 	"License"); you may not use this file except in compliance
::	with the License.  You may obtain a copy of the License at
::
::   http://www.apache.org/licenses/LICENSE-2.0
::
:: 	Unless required by applicable law or agreed to in writing,
:: 	software distributed under the License is distributed on an
:: 	"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
:: 	KIND, either express or implied.  See the License for the
::	specific language governing permissions and limitations
:: 	under the License.
::

@echo off

if "%1"=="" (
set root_directory="%~dp0..\..\"
) else (
set root_directory=%1 
)

:: Call from root of the forge with no extra arguments
:: Call from Scripts folder without extra Arguments
:: Call from anywhere with the root of the forge as the argument 
set ExcludedFolders="ThirdParty\ CommonRaytracing_3\ThirdParty\ Tools\ Scripts\ Common_3\Tools\SpirvTools\ AutoGenerateRendererAPI Shaders"
set extensions=*.h *.hpp *.c *.cpp *.m *.mm

:: Version of clang-format executable. Download from here:
:: https://llvm.org/builds/
set ClangFormatVersion=r351376

:: Full path of Clang-format executable including version
set ClangFormatExe=.\Tools\ClangFormatter\clang-format-%ClangFormatVersion%.exe

:: Copy .clang-format as it needs to be in parent dir
copy %~dp0.clang-format %root_directory%

:: Full command used to format individual files.
set ClangformatterCommand= %ClangFormatExe% -i -style=file -fallback-style=none

pushd %root_directory%
echo "Executing '%ClangformatterCommand%' on all files with following extensions: '%extensions%' and following Dirs excluded '%ExcludedFolders%'"

for /R %root_directory% %%f in (%extensions%) do (	
	ECHO "%%f"| findstr /v %ExcludedFolders%>NUL
	IF NOT ERRORLEVEL 1 %ClangformatterCommand% "%%f"
)
popd

:: Delete copy of .clang-format 
DEL %root_directory%.clang-format

pushd %root_directory%
echo "Outputing summary of changes due to clang-fomat"
git diff --shortstat
popd

exit /b 0 
