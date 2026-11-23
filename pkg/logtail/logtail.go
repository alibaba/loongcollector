// Copyright 2021 iLogtail Authors
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

//go:build linux || windows
// +build linux windows

package logtail

/*
#cgo CFLAGS: -I./
#cgo !windows,!corp LDFLAGS: -L./ -lGoPluginAdapter -Wl,-rpath,/usr/lib64:
#cgo !windows,corp LDFLAGS: -L./ -lPluginAdapter -Wl,-rpath,/usr/lib64:
#cgo windows LDFLAGS: -L./ -lGoPluginAdapter -static-libgcc
#include "LogtailPluginAdapter.h"
*/
import "C"

import (
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/util"
)

// SendPipelineEventGroup pushes a serialized PipelineEventGroup protobuf into
// the C++ SLS flusher. Reject an empty buffer before crossing the cgo boundary.
func SendPipelineEventGroup(configName string, logstore string, pbBuffer []byte, eventCount int, hash string) int {
	if len(pbBuffer) == 0 {
		return -1
	}
	rstVal := C.LogtailSendPipelineEventGroup((*C.char)(util.StringPointer(configName)), C.int(len(configName)),
		(*C.char)(util.StringPointer(logstore)), C.int(len(logstore)),
		(*C.char)(unsafe.Pointer(&pbBuffer[0])), C.int(len(pbBuffer)),
		C.int(eventCount),
		(*C.char)(util.StringPointer(hash)), C.int(len(hash)))
	return int(rstVal)
}

func IsValidToSend(logstoreKey int64) bool {
	return C.LogtailIsValidToSend(C.longlong(logstoreKey)) == 0
}
