#!/usr/bin/env python

# This script is used after calling `go mod vendor`, it will use files in external directory to override
# corresponding files in vendor directory.
# We decide to maintain this because of two reasons:
# 1. Go modules will not copy non-Go packages when calling `go mod vendor`, so cgo files will not be downloaded
#    in vendor. See https://github.com/golang/go/issues/26366 for more details.
# 2. Make some patches for packages we depend on.

import os


script_path = os.path.abspath(os.path.realpath(__file__))
script_dir_path = os.path.dirname(script_path)
script_name = os.path.basename(script_path)
for package in os.listdir(script_dir_path):
    if package == script_name:
        continue

    cmd = 'cp -r %s %s' % (os.path.join(script_dir_path, package),
                           os.path.join(script_dir_path, '../vendor/'))
    print('execute cmd %s: %s' % (cmd, os.system(cmd)))
