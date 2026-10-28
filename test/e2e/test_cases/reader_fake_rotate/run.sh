#!/usr/bin/env bash

# Copyright 2026 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Drive the scheme C dump collision (different path, same inode):
#   1. collect new/sub/app.log (inode X) — this reader stays in the active map
#      after delete (stale)
#   2. wait CloseTimeoutFilePtr so the inode can be freed
#   3. recreate inode X at old/app.log and collect (active)
#   4. rotate old/app.log -> old/rotated.log, then recreate old/app.log
#      so the active reader is moved to the rotator map (real path still exists)
#
# DumpAllHandlersMeta writes rotators first, then actives. Last-writer-wins
# therefore keeps the stale checkpoint on a pre-#2646 baseline (duplicates
# after reload). Scheme C keeps the checkpoint whose file still exists.
set -eu

VOLUME=/root/volume
mkdir -p "$VOLUME/old" "$VOLUME/new/sub"

reuse_inode() {
	src="$1"
	dst="$2"
	content="$3"
	old_inode=$(stat -c %i "$src")
	rm -f "$src"
	i=0
	while [ "$i" -lt 80 ]; do
		printf '%s' "$content" >"$dst"
		if [ "$(stat -c %i "$dst")" = "$old_inode" ]; then
			echo "$old_inode" >"$VOLUME/inode_reused"
			return 0
		fi
		rm -f "$dst"
		i=$((i + 1))
		# CloseTimeoutFilePtr may still hold the inode; wait and retry.
		if [ $((i % 5)) -eq 0 ]; then
			sleep 1
		fi
	done
	echo "$old_inode" >"$VOLUME/inode_reuse_failed"
	return 1
}

sleep 8
printf 'old-1\nold-2\nold-3\nold-4\nold-5\n' >"$VOLUME/new/sub/app.log"
sleep 12
reuse_inode "$VOLUME/new/sub/app.log" "$VOLUME/old/app.log" "$(printf 'new-1\nnew-2\nnew-3\nnew-4\nnew-5\n')"
# Let the active reader finish the new lines, then rotate so it enters the
# rotator map while the real path (rotated.log) still exists.
sleep 5
mv "$VOLUME/old/app.log" "$VOLUME/old/rotated.log"
sleep 2
printf 'extra-1\n' >"$VOLUME/old/app.log"
echo ok >"$VOLUME/scene_ready"
