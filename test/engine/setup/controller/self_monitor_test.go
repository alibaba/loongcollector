// Copyright 2026 iLogtail Authors
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

package controller

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/alibaba/ilogtail/test/config"
)

func TestEnsureE2ESelfMonitorConfigSkipExistingInternalMetrics(t *testing.T) {
	dir := t.TempDir()
	config.ConfigDir = dir
	t.Cleanup(func() { config.ConfigDir = "" })

	if err := os.WriteFile(filepath.Join(dir, "case.yaml"), []byte("inputs:\n  - Type: input_internal_metrics\n"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := ensureE2ESelfMonitorConfig(); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(dir, e2eSelfMonitorConfigName)); !os.IsNotExist(err) {
		t.Fatalf("should skip injecting when case already has input_internal_metrics, err=%v", err)
	}
}

func TestEnsureE2ESelfMonitorConfigWrite(t *testing.T) {
	dir := t.TempDir()
	config.ConfigDir = dir
	t.Cleanup(func() { config.ConfigDir = "" })

	if err := os.WriteFile(filepath.Join(dir, "case.yaml"), []byte("inputs:\n  - Type: input_file\n"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := ensureE2ESelfMonitorConfig(); err != nil {
		t.Fatal(err)
	}
	content, err := os.ReadFile(filepath.Join(dir, e2eSelfMonitorConfigName))
	if err != nil {
		t.Fatal(err)
	}
	if string(content) != e2eSelfMonitorConfig {
		t.Fatalf("unexpected self-monitor config:\n%s", content)
	}
}
