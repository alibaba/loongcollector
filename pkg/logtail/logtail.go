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
#cgo !windows LDFLAGS: -L./ -lGoPluginAdapter -Wl,-rpath,/usr/lib64:
#cgo windows LDFLAGS: -L./ -lGoPluginAdapter -static-libgcc
#include "LogtailPluginAdapter.h"
*/
import "C"

import (
	"fmt"
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/util"
)

func ExecuteCMD(configName string, cmdType int, params []byte) error {
	var rstVal C.int
	if len(params) == 0 {
		rstVal = C.LogtailCtlCmd((*C.char)(util.StringPointer(configName)), C.int(len(configName)),
			C.int(cmdType),
			(*C.char)(unsafe.Pointer(nil)), C.int(0))
	} else {
		rstVal = C.LogtailCtlCmd((*C.char)(util.StringPointer(configName)), C.int(len(configName)),
			C.int(cmdType),
			(*C.char)(unsafe.Pointer(&params[0])), C.int(len(params)))
	}

	if rstVal < C.int(0) {
		return fmt.Errorf("execute cmd error %d", int(rstVal))
	}
	return nil
}

func SendPb(configName string, logstore string, pbBuffer []byte, lines int) int {
	rstVal := C.LogtailSendPb((*C.char)(util.StringPointer(configName)), C.int(len(configName)),
		(*C.char)(util.StringPointer(logstore)), C.int(len(logstore)),
		(*C.char)(unsafe.Pointer(&pbBuffer[0])), C.int(len(pbBuffer)),
		C.int(lines))
	return int(rstVal)
}

func SendPbV2(configName string, logstore string, pbBuffer []byte, lines int, hash string) int {
	rstVal := C.LogtailSendPbV2((*C.char)(util.StringPointer(configName)), C.int(len(configName)),
		(*C.char)(util.StringPointer(logstore)), C.int(len(logstore)),
		(*C.char)(unsafe.Pointer(&pbBuffer[0])), C.int(len(pbBuffer)),
		C.int(lines),
		(*C.char)(util.StringPointer(hash)), C.int(len(hash)))
	return int(rstVal)
}

func IsValidToSend(logstoreKey int64) bool {
	return C.LogtailIsValidToSend(C.longlong(logstoreKey)) == 0
}
