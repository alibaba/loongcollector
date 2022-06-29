@echo off

set mode=DEBUG

rem Input: /path/to/build_dir, such as build-all
set buildDir=build
if %1. == . (
	echo "Without setting build dir, use default: '%buildDir%'"
) else (
	set buildDir=%1
)

set CURRENT_DIR=%~dp0
set absoluteBuildDir=%CURRENT_DIR%..\%buildDir%
if not exist %absoluteBuildDir% (
	echo "Build dir '%absoluteBuildDir%' is not existing."
	goto END
)

For /f "tokens=1-4 delims=/ " %%a in ('date /t') do (set mydate=%%a-%%b-%%c)
For /f "tokens=1-2 delims=/:" %%a in ('time /t') do (set mytime=%%a-%%b)
set CURRENT_TIME=%mydate%-%mytime%
set OUTPUT_DIR=%CURRENT_DIR%ut.output\%CURRENT_TIME%
mkdir %OUTPUT_DIR%
set output=%OUTPUT_DIR%\output.log

cd %absoluteBuildDir%\unittest
if not ERRORLEVEL 0 (
	echo "Enter '%absoluteBuildDir%\unittest' failed"
	goto END
)

rem Clean up ilogtail.LOG in UTs.
for /f "delims=" %%f in ('dir /b /s ilogtail.LOG') do (
	del %%f
)

rem Start UTs.
echo "============== common =============" >> %output%
cd common\%mode%
.\common_simple_utils_unittest.exe >> %output%
.\common_util_unittest >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== config ==============" >> %output%
cd config\%mode%
.\config_match_unittest.exe >> %output%
.\config_updator_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== parser ==============" >> %output%
cd parser\%mode%
.\parser_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== polling ==============" >> %output%
cd polling\%mode%
.\polling_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== processor ==============" >> %output%
cd processor\%mode%
.\processor_filter_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== reader ==============" >> %output%
cd reader\%mode%
if not exist testDataSet (
	xcopy /s %CURRENT_DIR%\reader\testDataSet .\testDataSet\
)
.\reader_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== sender ==============" >> %output%
cd sender\%mode%
.\sender_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== profiler ==============" >> %output%
cd profiler\%mode%
.\profiler_data_integrity_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

rem Collect logs.
for /f "delims=" %%f in ('dir /ad /b') do (
	if exist %%f\ilogtail.LOG (
		mkdir %OUTPUT_DIR%\%%f
		copy %%f\ilogtail.LOG %OUTPUT_DIR%\%%f\
	)
)

:END
pause