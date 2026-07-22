#!/bin/sh
# Generate a single line longer than 16KB to trigger Docker json-file partial log splitting.
# Docker json-file driver splits log lines at 16KB boundaries, producing partial entries
# (log field without trailing \n). The fix should merge them back into a single log event.
#
# We generate ~20KB: "LONG_LINE_START_" + 20000 'A' chars + "_LONG_LINE_END"

prefix="LONG_LINE_START_"
suffix="_LONG_LINE_END"
padding=$(dd if=/dev/zero bs=1 count=20000 2>/dev/null | tr '\0' 'A')
printf '%s%s%s\n' "$prefix" "$padding" "$suffix"

sleep 90
