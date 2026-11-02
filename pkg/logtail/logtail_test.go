// Copyright 2026 iLogtail Authors
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

import "testing"

func TestSendPipelineEventGroupRejectsEmptyBuffer(t *testing.T) {
	if got := SendPipelineEventGroup("cfg", "ls", nil, 0, ""); got != -1 {
		t.Fatalf("nil buffer: got %d, want -1", got)
	}
	if got := SendPipelineEventGroup("cfg", "ls", []byte{}, 0, ""); got != -1 {
		t.Fatalf("empty buffer: got %d, want -1", got)
	}
}

func TestSendPipelineEventGroupWithoutCallback(t *testing.T) {
	// Adapter is linked but C++ has not registered the callback in this
	// unit-test process, so the CGO call must fail closed.
	if got := SendPipelineEventGroup("cfg", "ls", []byte{0x0a, 0x00}, 1, ""); got != -1 {
		t.Fatalf("unregistered callback: got %d, want -1", got)
	}
}

func TestSendPbCoexistsWithoutCallback(t *testing.T) {
	// C1 adds SendPipelineEventGroup without replacing SendPb/V2 symbols.
	buf := []byte{0x0a, 0x00}
	if got := SendPb("cfg", "ls", buf, 1); got != -1 {
		t.Fatalf("SendPb unregistered: got %d, want -1", got)
	}
	if got := SendPbV2("cfg", "ls", buf, 1, ""); got != -1 {
		t.Fatalf("SendPbV2 unregistered: got %d, want -1", got)
	}
}
