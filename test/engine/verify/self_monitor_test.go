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

package verify

import (
	"regexp"
	"testing"
)

func TestMatchPluginSourceLabels(t *testing.T) {
	metrics := []selfMonitorMetricLine{
		{
			Name: "plugin_source",
			Labels: map[string]string{
				"file_name":        "/root/test/simple.log",
				"file_dev":         "8",
				"file_inode":       "123",
				"_container_name_": "demo-container-1",
				"_image_name_":     "demo-container:latest",
			},
		},
	}
	ok := map[string]*regexp.Regexp{
		"file_name":        regexp.MustCompile(`.*/root/test/simple.log$`),
		"_container_name_": regexp.MustCompile(`.*[-_]container[-_]1$`),
	}
	if err := matchPluginSourceLabels(metrics, ok); err != nil {
		t.Fatalf("expected match, got %v", err)
	}

	missing := map[string]*regexp.Regexp{
		"_pod_name_": regexp.MustCompile(`nginx-0`),
	}
	if err := matchPluginSourceLabels(metrics, missing); err == nil {
		t.Fatal("expected missing label to fail")
	}
}

func TestPluginSourceLabelsOmitContainerKeys(t *testing.T) {
	hostFile := []selfMonitorMetricLine{{
		Name: "plugin_source",
		Labels: map[string]string{
			"file_name":  "/root/test/simple.log",
			"file_dev":   "8",
			"file_inode": "123",
		},
	}}
	if err := pluginSourceLabelsOmitKeys(hostFile, containerPluginSourceLabelKeys); err != nil {
		t.Fatalf("host file labels should omit container keys, got %v", err)
	}

	withContainer := []selfMonitorMetricLine{{
		Name: "plugin_source",
		Labels: map[string]string{
			"file_name":        "/root/test/a.log",
			"_container_name_": "demo",
		},
	}}
	if err := pluginSourceLabelsOmitKeys(withContainer, containerPluginSourceLabelKeys); err == nil {
		t.Fatal("expected container key to fail the omit check")
	}
}
