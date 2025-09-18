@echo off

REM Copyright 2023 iLogtail Authors
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM      http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by authorised law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM See the License for the specific language governing permissions and
REM limitations under the License.

set CurrentPath=%~dp0
set P1Path=
set P2Path=
:begin
for /f "tokens=1,* delims=\" %%i in ("%CurrentPath%") do (set content=%%i&&set CurrentPath=%%j)
if "%P1Path%%content%\" == "%~dp0" goto end
set P2Path=%P1Path%
set P1Path=%P1Path%%content%\
goto begin
:end

set LOONCOLLECTOR_SRC_PATH=%P1Path%
set TARGET_ARTIFACT_PATH=%LOONCOLLECTOR_SRC_PATH%core\build\unittest

cd %TARGET_ARTIFACT_PATH%
echo unittest dir: %TARGET_ARTIFACT_PATH% 
call :search_files %TARGET_ARTIFACT_PATH%

:search_files
echo ============== search_files ==============
setlocal EnableDelayedExpansion
echo ============== search_files2 ==============

for /r %%f in ("%~1\*_unittest.exe") do (
    echo ============== %%~nxf ==============
    call "%%f"
	IF ERRORLEVEL 1 (
        echo %%~nxf failed!
        set success=false
    ) ELSE (
        echo %%~nxf passed successfully!
    )
    echo ====================================
)

if "!success!" == "false" (
    echo One or more tests failed.
    exit /B 1
) ELSE (
    echo All tests passed successfully!
    exit /B 0
)
goto :eof