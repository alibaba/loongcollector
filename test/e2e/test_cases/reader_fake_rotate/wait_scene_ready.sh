#!/usr/bin/env sh

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

# Poll the sidecar marker that inode X of new/sub/app.log was reused at
# old/app.log and the rotate-into-rotator step finished (scene_ready).
dir=test_cases/reader_fake_rotate/volume
ready=$dir/scene_ready
failed=$dir/inode_reuse_failed
i=0
while [ "$i" -lt 45 ]; do
	if [ -f "$failed" ]; then
		echo "inode reuse failed, recorded inode: $(cat "$failed")" >&2
		ls -la "$dir" "$dir/old" "$dir/new/sub" 2>/dev/null || true
		exit 1
	fi
	if [ -f "$ready" ]; then
		exit 0
	fi
	sleep 2
	i=$((i + 1))
done
ls -la "$dir" "$dir/old" "$dir/new/sub" 2>/dev/null || true
echo "scene ready marker not found: $ready" >&2
exit 1
