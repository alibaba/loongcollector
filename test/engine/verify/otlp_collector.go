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

package verify

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/avast/retry-go/v4"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

// OTLPCollectorData represents the top-level structure of OTel Collector file exporter output.
// The file exporter writes one JSON object per line (JSON Lines format).
type OTLPCollectorData struct {
	ResourceLogs    []interface{} `json:"resourceLogs,omitempty"`
	ResourceMetrics []interface{} `json:"resourceMetrics,omitempty"`
	ResourceSpans   []interface{} `json:"resourceSpans,omitempty"`
}

// OTLPCollectorReceived verifies that the OTel Collector's file exporter has received
// at least the expected count of a given data type (logs, metrics, traces).
// It reads the file from the local bind-mounted otel-export directory.
func OTLPCollectorReceived(ctx context.Context, expect int, dataType string, filePath string) (context.Context, error) {
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()

	// Resolve the local file path from the container path.
	// The case.feature passes a container path like /tmp/otel-export/logs.json.
	// Since we use a bind mount to ./otel-export/ in the case directory,
	// we look for the file relative to the current case home.
	localFile := resolveLocalPath(config.CaseHome, filePath)

	var count int
	var err error

	err = retry.Do(
		func() error {
			count, err = countOTLPFileRecords(localFile, dataType)
			if err != nil {
				return fmt.Errorf("failed to count OTLP %s: %v", dataType, err)
			}
			if count < expect {
				return fmt.Errorf("otlp collector %s count not match, expect at least %d, got %d", dataType, expect, count)
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}

// resolveLocalPath converts a container-internal path to the local bind-mounted path.
// For example: /tmp/otel-export/logs.json -> <CaseHome>/otel-export/logs.json
func resolveLocalPath(caseHome, containerPath string) string {
	// Extract the relative path from the known mount target /tmp/otel-export/
	relPath := strings.TrimPrefix(containerPath, "/tmp/otel-export/")
	if relPath == containerPath {
		// Not under /tmp/otel-export, try /tmp/otel-export
		relPath = strings.TrimPrefix(containerPath, "/tmp/otel-export/")
	}
	localFile := filepath.Join(caseHome, "otel-export", relPath)
	logger.Debugf(context.Background(), "resolved OTLP file path: %s -> %s", containerPath, localFile)
	return localFile
}

// countOTLPFileRecords reads the file exporter output and counts records of the given type.
func countOTLPFileRecords(filePath string, dataType string) (int, error) {
	cleanPath, err := validateOTLPExportPath(filePath)
	if err != nil {
		return 0, err
	}

	// Read and parse the JSON Lines file
	// #nosec G304 -- path is validated by validateOTLPExportPath before opening.
	file, err := os.Open(cleanPath)
	if err != nil {
		// File may not exist yet (no data flushed)
		if os.IsNotExist(err) {
			return 0, nil
		}
		return 0, fmt.Errorf("failed to open file: %v", err)
	}
	defer file.Close()

	count := 0
	scanner := bufio.NewScanner(file)
	// Increase buffer size for potentially large lines
	scanner.Buffer(make([]byte, 1024*1024), 10*1024*1024)

	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}

		var data OTLPCollectorData
		if err := json.Unmarshal([]byte(line), &data); err != nil {
			// Try parsing as array format
			count += countFromArray(line, dataType)
			continue
		}

		switch dataType {
		case "logs":
			count += len(data.ResourceLogs)
		case "metrics":
			count += len(data.ResourceMetrics)
		case "traces":
			count += len(data.ResourceSpans)
		}
	}

	if err := scanner.Err(); err != nil {
		return 0, fmt.Errorf("scanner error: %v", err)
	}

	return count, nil
}

func validateOTLPExportPath(filePath string) (string, error) {
	baseDir := filepath.Clean(filepath.Join(config.CaseHome, "otel-export"))
	cleanPath := filepath.Clean(filePath)

	relPath, err := filepath.Rel(baseDir, cleanPath)
	if err != nil {
		return "", fmt.Errorf("failed to resolve otlp file path: %v", err)
	}
	if relPath == ".." || strings.HasPrefix(relPath, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("otlp file path is outside allowed directory: %s", cleanPath)
	}
	return cleanPath, nil
}

// countFromArray tries to parse the line as an array and count items.
func countFromArray(line string, dataType string) int {
	var arr []map[string]interface{}
	if err := json.Unmarshal([]byte(line), &arr); err != nil {
		return 0
	}

	count := 0
	for _, item := range arr {
		switch dataType {
		case "logs":
			if _, ok := item["resourceLogs"]; ok {
				count++
			}
		case "metrics":
			if _, ok := item["resourceMetrics"]; ok {
				count++
			}
		case "traces":
			if _, ok := item["resourceSpans"]; ok {
				count++
			}
		}
	}
	return count
}
