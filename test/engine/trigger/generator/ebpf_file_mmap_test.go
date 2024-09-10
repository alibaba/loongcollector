// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package generator

import (
	"os"
	"strconv"
	"syscall"
	"testing"
)

func TestGenerateMmapCommand(t *testing.T) {
	commandCnt := getEnvOrDefault("COMMAND_CNT", "10")
	commandCntNum, err := strconv.Atoi(commandCnt)
	if err != nil {
		t.Fatalf("parse COMMAND_CNT failed: %v", err)
		return
	}
	filename := getEnvOrDefault("FILE_NAME", "/tmp/ilogtail/ebpfFileSecurityHook3.log")
	f, err := os.Create(filename)
	if err != nil {
		panic(err)
	}
	fd := int(f.Fd())
	for i := 0; i < commandCntNum; i++ {
		b, innerErr := syscall.Mmap(fd, 0, 20, syscall.PROT_READ|syscall.PROT_WRITE, syscall.MAP_SHARED)
		if innerErr != nil {
			panic(innerErr)
		}
		innerErr = syscall.Munmap(b)
		if innerErr != nil {
			panic(innerErr)
		}
	}
	err = os.Remove(filename)
	if err != nil {
		t.Fatalf("remove file failed: %v", err)
		return
	}
}
