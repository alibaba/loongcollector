// Copyright 2023 iLogtail Authors
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

package helper

import (
	"strconv"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

var FileOffsetKey = "__tag__:__file_offset__"
var ReservedFileOffsetKey = "__file_offset__"

func ReviseFileOffset(log *protocol.Log, offset int64, enableMeta bool) {
	if enableMeta {
		cont := GetFileOffsetTag(log)
		if cont != nil {
			if beginOffset, err := strconv.ParseInt(cont.Value, 10, 64); err == nil {
				cont.Value = strconv.FormatInt(beginOffset+offset, 10)
			}
		}
	}
}

func GetFileOffsetTag(log *protocol.Log) *protocol.Log_Content {
	for i := len(log.Contents) - 1; i >= 0; i-- {
		cont := log.Contents[i]
		if cont.Key == FileOffsetKey || cont.Key == ReservedFileOffsetKey {
			return cont
		}
	}
	return nil
}
