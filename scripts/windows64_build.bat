@echo on

REM Procedures:
REM 1. Set environments.
REM 2. Build iLogtail.
REM 3. Build iLogtail plugin.
REM 4. Make package.

set ILOGTAIL_VERSION=2.0.0
if not "%1" == "" set ILOGTAIL_VERSION=%1
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



set ILOGTAIL_PLUGIN_SRC_PATH="%P1Path%"
set LOGTAIL_SRC_PATH=%ILOGTAIL_PLUGIN_SRC_PATH%\core

REM Change to where ilogtail-deps.windows-x64 locates
set ILOGTAIL_DEPS_PATH=D:\workspace\ilogtail-deps.windows-x64-2.0-version
REM Change to where cmake locates
set CMAKE_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake"
REM Change to where devenv locates
set DEVENV_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.com"
REM Change to where mingw locates
set MINGW_PATH=C:\local\mingw64\bin

REM Change to where boost_1_68_0 locates
set BOOST_ROOT=%ILOGTAIL_DEPS_PATH%\boost_1_68_0_64

set BOOL_OPENSOURCE=true

set ILOGTAIL_DEPS_PATH=%ILOGTAIL_DEPS_PATH:\=/%
set ILOGTAIL_PLUGIN_SRC_UNIX_PATH=%ILOGTAIL_PLUGIN_SRC_PATH:\=/%

set PUBLISH_DIR=C:\publish_dir\win64\%ILOGTAIL_VERSION%
set OUTPUT_UNIX_DIR=%PUBLISH_DIR:\=/%
set ILOGTAIL_CORE_BUILD_PATH=%ILOGTAIL_PLUGIN_SRC_PATH%\core\build

go env -w GOPROXY="https://goproxy.cn,direct"
set GOARCH=amd64
set GOFLAGS=-buildvcs=false
set CGO_ENABLED=1

set PATH=%PATH%%DEVENV_BIN%;%MINGW_PATH%

REM Clean up
IF exist %PUBLISH_DIR% ( rd /s /q %PUBLISH_DIR% )
mkdir %PUBLISH_DIR%

REM Build C++ core(ilogtail.exe, PluginAdapter.dll)
echo begin to compile core
cd %LOGTAIL_SRC_PATH%
IF exist build ( rd /s /q build )
mkdir build
cd build
%CMAKE_BIN% -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release -DLOGTAIL_VERSION=%ILOGTAIL_VERSION% -DBUILD_LOGTAIL_UT=OFF -DDEPS_ROOT=%ILOGTAIL_DEPS_PATH% ..
if not %ERRORLEVEL% == 0 (
    echo Run cmake failed.
    goto quit
)
%DEVENV_BIN% logtail.sln /Build "Release|x64" 1>build.stdout 2>build.stderr
if not %ERRORLEVEL% == 0 (
    echo Build iLogtail source failed.
    goto quit
)
echo Build core success

REM Build installer
cd %LOGTAIL_SRC_PATH%\installer
IF exist logtail_installer ( rd /s /q logtail_installer )
IF exist logtail_installer_with_pdb ( rd /s /q logtail_installer_with_pdb )
call make_package.bat



REM Import plugins
cd %ILOGTAIL_PLUGIN_SRC_PATH%
echo ===============GENERATING PLUGINS IMPORT==================
del /f/s/q %ILOGTAIL_PLUGIN_SRC_PATH%\plugins\all\all.go
del /f/s/q %ILOGTAIL_PLUGIN_SRC_PATH%\plugins\all\all_debug.go
del /f/s/q %ILOGTAIL_PLUGIN_SRC_PATH%\plugins\all\all_windows.go
del /f/s/q %ILOGTAIL_PLUGIN_SRC_PATH%\plugins\all\all_linux.go
go run -mod=mod %ILOGTAIL_PLUGIN_SRC_UNIX_PATH%/tools/builder -root-dir=%ILOGTAIL_PLUGIN_SRC_UNIX_PATH% -config="plugins.yml,external_plugins.yml" -modfile="go.mod"
echo generating plugins finished successfully

REM Build plugins (PluginBase.dll, PluginBase.h)
echo Begin to build plugins...

cd %ILOGTAIL_PLUGIN_SRC_PATH%
IF exist output ( rd /s /q output )
mkdir output
xcopy /Y %ILOGTAIL_CORE_BUILD_PATH%\go_pipeline\Release\PluginAdapter.dll %ILOGTAIL_PLUGIN_SRC_PATH%\pkg\logtail
set LDFLAGS="-X "github.com/alibaba/ilogtail/pluginmanager.BaseVersion=%ILOGTAIL_VERSION%""
go build -mod=mod -buildmode=c-shared -ldflags=%LDFLAGS% -o output\PluginBase.dll %ILOGTAIL_PLUGIN_SRC_UNIX_PATH%/plugin_main
if not %ERRORLEVEL% == 0 (
    echo Build iLogtail plugin source failed.
    goto quit
)
echo Build plugins success


xcopy /Y %ILOGTAIL_PLUGIN_SRC_PATH%\output\PluginBase.dll %LOGTAIL_SRC_PATH%\installer\logtail_installer\
REM xcopy /Y output\PluginBase.dll %LOGTAIL_SRC_PATH%\installer\logtail_installer_with_pdb\

REM Record git hash
echo "git commit for" %LOGTAIL_SRC_PATH% > %PUBLISH_DIR%\git_commit.txt
echo "git commit for" %ILOGTAIL_PLUGIN_SRC_PATH% >> %PUBLISH_DIR%\git_commit.txt
git show >> %PUBLISH_DIR%\git_commit.txt

echo %LOGTAIL_SRC_PATH%
echo %PUBLISH_DIR%




if "%BOOL_OPENSOURCE%"=="true" (
    call :pkgOpenSource
) else (
    call :pkgEnterprise
)


:quit
pause



:pkgOpenSource
xcopy /Y %ILOGTAIL_CORE_BUILD_PATH%\Release\ilogtail.exe %ILOGTAIL_PLUGIN_SRC_PATH%\output
xcopy /Y %ILOGTAIL_CORE_BUILD_PATH%\go_pipeline\Release\PluginAdapter.dll %ILOGTAIL_PLUGIN_SRC_PATH%\output
echo { >  %ILOGTAIL_PLUGIN_SRC_PATH%\output\ilogtail_config.json & echo } >> %ILOGTAIL_PLUGIN_SRC_PATH%\output\ilogtail_config.json
goto :eof


:pkgEnterprise
echo "pkgEnterprise"

XCOPY /s %LOGTAIL_SRC_PATH%\installer\logtail_installer %PUBLISH_DIR%\logtail_installer\
XCOPY /s %LOGTAIL_SRC_PATH%\installer\logtail_installer_with_pdb %PUBLISH_DIR%\logtail_installer_with_pdb\
cd %PUBLISH_DIR%\logtail_installer
dir

REM cd %OUTPUT_DIR%
REM dir

goto :eof