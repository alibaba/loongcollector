@echo off

set mode=Release

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
.\common_logfileoperator_unittest.exe >>  %output%
.\common_machine_info_util_unittest.exe >>  %output%
.\common_sender_queue_unittest.exe >>  %output%
.\common_sliding_window_counter_unittest.exe >>  %output%
.\common_string_tools_unittest.exe >>  %output%
.\encoding_converter_unittest.exe >>  %output%
.\yaml_util_unittest.exe >>  %output%
cd ..\..
echo "====================================" >> %output%

echo "============== config ==============" >> %output%
cd config\%mode%
.\config_unittest.exe >> %output%
.\config_update_unittest.exe >> %output%
.\config_watcher_unittest.exe >> %output%
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
.\processor_split_log_string_native_unittest.exe >> %output%
.\processor_split_multiline_log_string_native_unittest.exe >> %output%
.\processor_parse_regex_native_unittest.exe >> %output%
.\processor_parse_json_native_unittest.exe >> %output%
.\processor_parse_timestamp_native_unittest.exe >> %output%
.\processor_tag_native_unittest.exe >> %output%
.\processor_parse_apsara_native_unittest.exe >> %output%
.\processor_parse_delimiter_native_unittest.exe >> %output%
.\processor_filter_native_unittest.exe >> %output%
.\processor_desensitize_native_unittest.exe >> %output%
cd ..\..
echo "====================================" >> %output%

echo "============== reader ==============" >> %output%
cd reader\%mode%
@REM if not exist testDataSet (
@REM 	xcopy /s %CURRENT_DIR%\reader\testDataSet .\testDataSet\
@REM )
.\log_file_reader_deleted_file_unittest.exe >> %output%
.\file_reader_options_unittest.exe >> %output%
.\json_log_file_reader_unittest.exe >> %output%
.\remove_last_incomplete_log_unittest.exe >> %output%
.\log_file_reader_unittest.exe >> %output%
.\source_buffer_unittest.exe >> %output%
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