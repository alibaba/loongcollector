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

REM Procedures:
REM 1. Set environments.
REM 2. Build iLogtail.
REM 3. Build iLogtail plugin.
REM 4. Make package.

set LOONCOLLECTOR_VERSION=3.0.0
if not "%1" == "" set LOONCOLLECTOR_VERSION=%1
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

set LOONGCOLLECTOR_SRC_PATH="%P1Path%"
REM avoid '\' beed treated as escape.
set LOONGCOLLECTOR_SRC_UNIX_PATH=%LOONGCOLLECTOR_SRC_PATH:\=/%
echo looncollector dir: %LOONGCOLLECTOR_SRC_PATH%

REM Change to where boost_1_68_0 locates
if not defined BOOST_ROOT (
    set BOOST_ROOT=D:\loongcollector-windows-build\boost_1_68_0
)

REM Change to where ilogtail-deps.windows-x64 locates
if not defined LOONCOLLECTOR_DEPS_PATH (
    set LOONCOLLECTOR_DEPS_PATH=D:\loongcollector-windows-build\ilogtail-deps.windows-x64\ilogtail-deps.windows-x64
)
REM avoid '\' beed treated as escape.
set LOONCOLLECTOR_DEPS_PATH=%LOONCOLLECTOR_DEPS_PATH:\=/%

REM Change to where cmake locates
if not defined CMAKE_BIN (
    set CMAKE_BIN="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake"
)

REM Change to where devenv locates
set DEVENV_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.com"
@REM REM Change to where mingw locates
@REM set MINGW_PATH=C:\workspace\mingw64\bin

set OUTPUT_DIR=%LOONGCOLLECTOR_SRC_PATH%\output
set LOONCOLLECTOR_CORE_BUILD_PATH=%LOONGCOLLECTOR_SRC_PATH%\core\build

go env -w GOPROXY="https://goproxy.cn,direct"
set GOARCH=amd64
set GOFLAGS=-buildvcs=false
set CGO_ENABLED=1
set LINK=/ignore:4099

set PATH=%DEVENV_BIN%;%MINGW_PATH%;%PATH%

REM Clean up
IF exist %OUTPUT_DIR% ( rd /s /q %OUTPUT_DIR% )
mkdir %OUTPUT_DIR%

REM Build C++ core(ilogtail.exe, GoPluginAdapter.dll)
echo begin to compile core
cd %LOONGCOLLECTOR_SRC_PATH%\core
IF exist build ( rd /s /q build )
mkdir build
cd build

set LOGTAIL_UT=OFF
if defined BUILD_LOGTAIL_UT (
    if /i "%BUILD_LOGTAIL_UT%"=="ON" (
        set LOGTAIL_UT=ON
    )
)
%CMAKE_BIN% -G "Visual Studio 15 2017 Win64" ^
    -DBUILD_LOGTAIL_UT=%LOGTAIL_UT% ^
    -DLOGTAIL_VERSION=%LOONCOLLECTOR_VERSION% ^
    -DDEPS_ROOT=%LOONCOLLECTOR_DEPS_PATH% ^
    -DCMAKE_BUILD_TYPE=Release ..

if not %ERRORLEVEL% == 0 (
    echo Run cmake failed.
    exit /b 1
)
%DEVENV_BIN% loongcollector.sln /Build "Release|x64" 1>build.stdout 2>build.stderr
if not %ERRORLEVEL% == 0 (
    echo Build looncollector source failed.
	type "%LOONGCOLLECTOR_SRC_PATH%\core\build\build.stdout"
    exit /b 1
)
echo Build core success

REM Import plugins
cd %LOONGCOLLECTOR_SRC_PATH%
echo ===============GENERATING PLUGINS IMPORT==================
del /f/s/q %LOONGCOLLECTOR_SRC_PATH%\plugins\all\all.go
del /f/s/q %LOONGCOLLECTOR_SRC_PATH%\plugins\all\all_debug.go
del /f/s/q %LOONGCOLLECTOR_SRC_PATH%\plugins\all\all_windows.go
del /f/s/q %LOONGCOLLECTOR_SRC_PATH%\plugins\all\all_linux.go
go run -mod=mod %LOONGCOLLECTOR_SRC_UNIX_PATH%\tools\builder -root-dir=%LOONGCOLLECTOR_SRC_UNIX_PATH% -modfile="go.mod"
echo generating plugins finished successfully

REM Build plugins (GoPluginBase.dll, GoPluginBase.h)
echo Begin to build plugins...
cd %LOONGCOLLECTOR_SRC_PATH%
IF exist output ( rd /s /q output )
mkdir output
xcopy /Y %LOONCOLLECTOR_CORE_BUILD_PATH%\go_pipeline\Release\GoPluginAdapter.dll %LOONGCOLLECTOR_SRC_PATH%\pkg\logtail\
set LDFLAGS="-X "github.com/alibaba/ilogtail/pluginmanager.BaseVersion=%LOONCOLLECTOR_VERSION%""
go build -mod=mod -buildmode=c-shared -ldflags=%LDFLAGS% -o output\GoPluginBase.dll %LOONGCOLLECTOR_SRC_UNIX_PATH%\plugin_main
if not %ERRORLEVEL% == 0 (
    echo Build iLogtail plugin source failed.
    exit /b 1
)
echo Build plugins success

REM Record git hash
echo "git commit for" %LOGTAIL_SRC_PATH% > %LOONCOLLECTOR_CORE_BUILD_PATH%\git_commit.txt
echo "git commit for" %LOONGCOLLECTOR_SRC_PATH% >> %LOONCOLLECTOR_CORE_BUILD_PATH%\git_commit.txt
git show >> %LOONCOLLECTOR_CORE_BUILD_PATH%\git_commit.txt

set BOOL_OPENSOURCE=true
if "%BOOL_OPENSOURCE%"=="true" (
    call :pkgOpenSource
) else (
    call :pkgEnterprise
)

:pkgOpenSource
xcopy /Y %LOONCOLLECTOR_CORE_BUILD_PATH%\Release\loongcollector.exe %OUTPUT_DIR%
xcopy /Y %LOONCOLLECTOR_CORE_BUILD_PATH%\go_pipeline\Release\GoPluginAdapter.dll %OUTPUT_DIR%
echo { >  %LOONGCOLLECTOR_SRC_PATH%\output\ilogtail_config.json & echo } >> %OUTPUT_DIR%\ilogtail_config.json

:pkgEnterprise
echo "pkgEnterprise"
REM TODO

dir %OUTPUT_DIR%

goto :eof

exit /b 0
