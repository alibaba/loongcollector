// Copyright 2025 iLogtail Authors
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

package otlp

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
)

const otelgenImage = "ghcr.io/krzko/otelgen:latest"

// OtelgenSend generates OTLP data (logs, metrics, or traces) using otelgen
// via `docker run`, sending to the specified endpoint within the docker-compose network.
// The endpoint is resolved against the compose network (e.g., "loongcollector:4320").
func OtelgenSend(ctx context.Context, count int, dataType string, endpoint string, protocol string) (context.Context, error) {
	// Find the docker network used by the compose project
	network, err := findComposeNetwork()
	if err != nil {
		return ctx, fmt.Errorf("failed to find compose network: %v", err)
	}

	// Build otelgen args
	var args []string
	args = append(args, "--otel-exporter-otlp-endpoint", endpoint, "--insecure")
	// Use --protocol flag for http transport (supported by all subcommands)
	if protocol == "http" {
		args = append(args, "--protocol", "http")
	}
	// logs/traces use 'single' subcommand (one-shot); metrics uses 'sum' (runs continuously, killed by context timeout)
	if dataType == "metrics" {
		args = append(args, dataType, "sum")
	} else {
		args = append(args, dataType, "single")
	}

	logger.Infof(ctx, "running otelgen on network %s: %s %s", network, otelgenImage, strings.Join(args, " "))

	for i := 0; i < count; i++ {
		dockerArgs := append([]string{"run", "--rm", "--network", network, "--", otelgenImage}, args...)
		var cmd *exec.Cmd
		var cancel func()
		if dataType == "metrics" {
			// Metrics subcommand runs continuously; use context timeout to kill after 20s
			// to allow at least 2 batch cycles (5s each) for cumulative metrics
			timeoutCtx, cancelFn := context.WithTimeout(ctx, 20*time.Second)
			cancel = cancelFn
			cmd = exec.CommandContext(timeoutCtx, "docker", dockerArgs...)
		} else {
			cmd = exec.Command("docker", dockerArgs...)
		}
		output, err := cmd.CombinedOutput()
		if cancel != nil {
			cancel()
		}
		if err != nil {
			// For metrics, the context timeout kills the process with "signal: killed"
			// which is expected behavior, not an error
			if dataType == "metrics" && isTimeoutError(err) {
				logger.Infof(ctx, "otelgen metrics run killed by timeout (expected), output: %s", string(output))
			} else {
				logger.Errorf(ctx, "OTELGEN_SEND_ERROR",
					"iteration", i, "output", string(output), "err", err)
				return ctx, fmt.Errorf("otelgen failed on iteration %d: %v, output: %s", i, err, string(output))
			}
		} else {
			logger.Infof(ctx, "otelgen iteration %d output: %s", i, string(output))
		}
		// Small delay between iterations
		if count > 1 && i < count-1 {
			time.Sleep(500 * time.Millisecond)
		}
	}

	return ctx, nil
}

// findComposeNetwork discovers the docker-compose network by inspecting any running
// container whose name contains "otel-collector" or "loongcollectorC".
// isTimeoutError checks if the error is from a context deadline exceeded or signal killed
func isTimeoutError(err error) bool {
	if err == nil {
		return false
	}
	errStr := err.Error()
	return strings.Contains(errStr, "signal: killed") || strings.Contains(errStr, "context deadline exceeded")
}

func findComposeNetwork() (string, error) {
	// Find a container belonging to the compose project
	for _, pattern := range []string{"otel-collector", "loongcollectorC"} {
		cmd := exec.Command("docker", "ps", "--filter", "name="+pattern, "--format", "{{.ID}}")
		output, err := cmd.CombinedOutput()
		if err != nil {
			continue
		}

		containerID := strings.TrimSpace(string(output))
		if containerID == "" {
			continue
		}

		// Inspect the container's networks
		cmd = exec.Command("docker", "inspect", "--format",
			`{{range $k, $v := .NetworkSettings.Networks}}{{$k}}{{end}}`, containerID)
		output, err = cmd.CombinedOutput()
		if err != nil {
			continue
		}

		network := strings.TrimSpace(string(output))
		if network != "" {
			return network, nil
		}
	}

	return "", fmt.Errorf("no compose network found (no running otel-collector or loongcollectorC container)")
}
