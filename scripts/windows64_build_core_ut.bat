@echo off

REM Copyright 2021 iLogtail Authors
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM      http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
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

set LOONCOLLECTOR_PLUGIN_SRC_PATH="%P1Path%"
set LOONCOLLECTOR_PLUGIN_SRC_UNIX_PATH=%LOONCOLLECTOR_PLUGIN_SRC_PATH:\=/%
REM Change to where boost_1_68_0 locates
set BOOST_ROOT=C:\workspace\boost_1_68_0
REM Change to where ilogtail-deps.windows-x64 locates
set LOONCOLLECTOR_DEPS_PATH=C:\workspace\ilogtail-deps.windows-x64
set LOONCOLLECTOR_DEPS_PATH=%LOONCOLLECTOR_DEPS_PATH:\=/%
REM Change to where cmake locates
set CMAKE_BIN="C:\Program Files\CMake\bin\cmake"
REM Change to where devenv locates
set DEVENV_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.com"
REM Change to where mingw locates
set MINGW_PATH=C:\workspace\mingw64\bin

set OUTPUT_DIR=%LOONCOLLECTOR_PLUGIN_SRC_PATH%\output
set OUTPUT_UNIX_DIR=%OUTPUT_DIR:\=/%
set LOONCOLLECTOR_CORE_BUILD_PATH=%LOONCOLLECTOR_PLUGIN_SRC_PATH%\core\build

go env -w GOPROXY="https://goproxy.cn,direct"
set GOARCH=amd64
set GOFLAGS=-buildvcs=false
set CGO_ENABLED=1

set PATH=%DEVENV_BIN%;%MINGW_PATH%;%PATH%

REM Clean up
IF exist %OUTPUT_DIR% ( rd /s /q %OUTPUT_DIR% )
mkdir %OUTPUT_DIR%

REM Build C++ unittests
echo begin to compile core unittests
cd %LOONCOLLECTOR_PLUGIN_SRC_PATH%\core
IF exist build ( rd /s /q build )
mkdir build
cd build
%CMAKE_BIN% -DBUILD_LOGTAIL_UT=ON -DBUILD_LOGTAIL=OFF -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release -DDEPS_ROOT=%LOONCOLLECTOR_DEPS_PATH% ..
if not %ERRORLEVEL% == 0 (
    echo Run cmake failed.
    goto quit
)
%DEVENV_BIN% unittest/unittest_base.sln /Build "Release|x64" 1>build.stdout 2>build.stderr
if not %ERRORLEVEL% == 0 (
    echo Build core unittest source failed.
    goto quit
)
echo Build core unittest success

:quit
pause