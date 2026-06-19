// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//go:build (linux || windows) && cgo
// +build linux windows
// +build cgo

package checkpoint

import "github.com/alibaba/ilogtail/pkg/logtail"

// UpdateFileCheckpoint advances the file-read checkpoint identified by the
// rotation-stable sourceID to offset, after the flusher has confirmed delivery.
// logPath is passed through for diagnostics only; the core keys checkpoints by
// sourceID. It is a no-op when sourceID is empty (i.e. non-file sources).
func UpdateFileCheckpoint(configName, sourceID, logPath string, offset int64) {
	if sourceID == "" {
		return
	}
	logtail.UpdateCheckpoint(configName, sourceID, logPath, offset)
}
