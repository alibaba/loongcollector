@echo on

REM Procedures:
REM 1. Set environments.
REM 2. Copy output to dist package dir.
REM 3. Pack dir to zip archive.

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

set ARCH=amd64
set ILOGTAIL_PLUGIN_SRC_PATH="%P1Path%"
set OUTPUT_DIR=%ILOGTAIL_PLUGIN_SRC_PATH%\output
set DIST_DIR=%ILOGTAIL_PLUGIN_SRC_PATH%\dist
set PACKAGE_DIR=ilogtail-%ILOGTAIL_VERSION%
set Z_BIN="C:\Program Files\7-Zip\7z.exe"

cd %ILOGTAIL_PLUGIN_SRC_PATH%
md %DIST_DIR%\%PACKAGE_DIR%
xcopy /Y LICENSE %DIST_DIR%\%PACKAGE_DIR%
xcopy /Y README.md %DIST_DIR%\%PACKAGE_DIR%
xcopy /Y LICENSE README.md %DIST_DIR%\%PACKAGE_DIR%
xcopy /Y %OUTPUT_DIR% %DIST_DIR%\%PACKAGE_DIR%

cd %DIST_DIR%
del /f/s/q %PACKAGE_DIR%.windows-%ARCH%.zip
%Z_BIN% a -tzip %PACKAGE_DIR%.windows-%ARCH%.zip %PACKAGE_DIR%
rd /s /q %PACKAGE_DIR%
certUtil -hashfile %PACKAGE_DIR%.windows-%ARCH%.zip SHA256 | findstr /v "SHA256" | findstr /v CertUtil > %PACKAGE_DIR%.windows-%ARCH%.zip.sha256