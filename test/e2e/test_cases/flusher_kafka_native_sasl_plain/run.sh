#!/usr/bin/env bash

set -euo pipefail

sleep 3
for i in {1..30} ; do
  echo "{\"msg\":\"hello-$i\"}"
  sleep 0.1
done
sleep 3600
