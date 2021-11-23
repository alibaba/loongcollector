# How to check license?

LogtailPlugin follows the standard Apache 2.0 license. So the contributor should check whether the contributing files
with this license.

## Check license

If you want to check the whole project files, please run the following command. The files without the license would be found and saved as
the `license_coverage.txt` file.
```makefile
   make check-license
```
When the fixed scope is required, please append the `SCOPE` in the commands.
```makefile
SCOPE=xxx make check-license
```

## Add license
When some files with the license founded, you could add the license to the files in the following way. Note that this way will repair all files that lack the license.
```makefile
   make license
```
Same as above, if you want to limit the changing range, please append the `SCOPE` in the commands.
```makefile
SCOPE=xxx make license
```


