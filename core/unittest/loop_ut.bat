@echo off

echo %1
for /l %%x in (1, 1, %1) do (
    echo %%x
    call run_ut.bat
)