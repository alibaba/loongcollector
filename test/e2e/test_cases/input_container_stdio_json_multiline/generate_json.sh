#!/bin/sh
# Output multi-line JSON blocks to stdout, each field on its own line.
# Docker json-file driver will record each line separately;
# MergeType=json should merge them back into complete JSON blocks.

echo '{'
echo '  "name": "test_multiline",'
echo '  "value": 42,'
echo '  "nested": {'
echo '    "key": "hello"'
echo '  }'
echo '}'

sleep 2

echo '{"name": "test_multiline_single", "single_line": true}'

sleep 90
