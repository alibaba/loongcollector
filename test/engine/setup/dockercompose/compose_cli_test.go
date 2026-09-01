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

package dockercompose

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestParseComposePort(t *testing.T) {
	tests := []struct {
		name        string
		rawPort     interface{}
		privatePort string
		virtual     string
	}{
		{
			name:        "integer",
			rawPort:     8080,
			privatePort: "8080/tcp",
			virtual:     "server:8080",
		},
		{
			name:        "published and target",
			rawPort:     "18080:8080",
			privatePort: "8080/tcp",
			virtual:     "server:8080",
		},
		{
			name:        "host published target and protocol",
			rawPort:     "127.0.0.1:15353:5353/udp",
			privatePort: "5353/udp",
			virtual:     "server:5353",
		},
		{
			name: "long syntax",
			rawPort: map[string]interface{}{
				"target":    5353,
				"published": 15353,
				"protocol":  "udp",
			},
			privatePort: "5353/udp",
			virtual:     "server:5353",
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			port, err := parseComposePort("server", test.rawPort)
			if err != nil {
				t.Fatalf("parseComposePort() error = %v", err)
			}
			if port.privatePort != test.privatePort || port.virtual != test.virtual {
				t.Fatalf(
					"parseComposePort() = private %q, virtual %q; want private %q, virtual %q",
					port.privatePort,
					port.virtual,
					test.privatePort,
					test.virtual,
				)
			}
		})
	}
}

func TestParseComposePortOutput(t *testing.T) {
	tests := []struct {
		name   string
		output string
		want   string
	}{
		{name: "all IPv4 interfaces", output: "0.0.0.0:18080", want: "127.0.0.1:18080"},
		{name: "all IPv6 interfaces", output: "[::]:18080", want: "127.0.0.1:18080"},
		{name: "specific host", output: "192.0.2.1:18080", want: "192.0.2.1:18080"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got, err := parseComposePortOutput(test.output)
			if err != nil {
				t.Fatalf("parseComposePortOutput() error = %v", err)
			}
			if got != test.want {
				t.Fatalf("parseComposePortOutput() = %q, want %q", got, test.want)
			}
		})
	}
}

func TestComposePortCommand(t *testing.T) {
	command, err := composePortCommand(composePort{
		service:     "dns",
		privatePort: "5353/udp",
	})
	if err != nil {
		t.Fatalf("composePortCommand() error = %v", err)
	}
	want := []string{"port", "--protocol", "udp", "dns", "5353"}
	if strings.Join(command, " ") != strings.Join(want, " ") {
		t.Fatalf("composePortCommand() = %q, want %q", command, want)
	}
}

func TestRunComposeCommandUsesStandalone(t *testing.T) {
	tempDir := t.TempDir()
	executable := filepath.Join(tempDir, "docker-compose")
	script := "#!/bin/sh\nprintf '%s' \"$*\"\n"
	if err := os.WriteFile(executable, []byte(script), 0750); err != nil {
		t.Fatalf("write fake docker-compose: %v", err)
	}
	t.Setenv("PATH", tempDir)

	output, err := runComposeCommand(context.Background(), "compose.yaml", "project", "config")
	if err != nil {
		t.Fatalf("runComposeCommand() error = %v", err)
	}
	if output != "-f compose.yaml -p project config" {
		t.Fatalf("runComposeCommand() = %q", output)
	}
}

func TestRunComposeCommandUsesDockerPlugin(t *testing.T) {
	tempDir := t.TempDir()
	executable := filepath.Join(tempDir, "docker")
	script := "#!/bin/sh\nprintf '%s' \"$*\"\n"
	if err := os.WriteFile(executable, []byte(script), 0750); err != nil {
		t.Fatalf("write fake docker: %v", err)
	}
	t.Setenv("PATH", tempDir)

	output, err := runComposeCommand(context.Background(), "compose.yaml", "project", "config")
	if err != nil {
		t.Fatalf("runComposeCommand() error = %v", err)
	}
	if output != "compose -f compose.yaml -p project config" {
		t.Fatalf("runComposeCommand() = %q", output)
	}
}

func TestRunComposeCommandWithTimeout(t *testing.T) {
	tempDir := t.TempDir()
	executable := filepath.Join(tempDir, "docker-compose")
	script := "#!/bin/sh\nexec /bin/sleep 1\n"
	if err := os.WriteFile(executable, []byte(script), 0750); err != nil {
		t.Fatalf("write fake docker-compose: %v", err)
	}
	t.Setenv("PATH", tempDir)

	err := runComposeCommandWithTimeout(
		context.Background(),
		10*time.Millisecond,
		"compose.yaml",
		"project",
		"up",
	)
	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("runComposeCommandWithTimeout() error = %v, want deadline exceeded", err)
	}
}
