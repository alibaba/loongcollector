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
	"crypto/sha256"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"strings"

	composeModule "github.com/testcontainers/testcontainers-go/modules/compose"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/test/config"
)

// ensureComposeBuildEnv disables buildkit for compose sidecar builds (bash:latest etc.).
// Matches CI/local reliability; avoids buildx "multi-arch-builder" pulling from docker.io during tests.
func ensureComposeBuildEnv() {
	// Always disable buildkit for E2E compose sidecars (bash:latest, etc.).
	_ = os.Setenv("DOCKER_BUILDKIT", "0")
	_ = os.Setenv("COMPOSE_DOCKER_CLI_BUILD", "0")
}

// ComposeProjectName returns the hashed docker-compose project name for a test case directory.
func ComposeProjectName(caseHome string) string {
	trimmed := strings.TrimSuffix(caseHome, "/")
	parts := strings.Split(trimmed, "/")
	if len(parts) == 0 {
		return ""
	}
	projectName := parts[len(parts)-1]
	hasher := sha256.New()
	hasher.Write([]byte(projectName))
	return fmt.Sprintf("%x", hasher.Sum(nil))
}

// ComposeDown stops and removes containers for the given case compose file.
func ComposeDown(caseHome string) error {
	ensureComposeBuildEnv()
	if caseHome == "" {
		return nil
	}
	composeFile := caseHome + finalFileName
	if _, err := os.Stat(composeFile); err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	projectName := ComposeProjectName(caseHome)
	execError := composeModule.NewLocalDockerCompose([]string{composeFile}, projectName).Down()
	if execError.Error != nil {
		logger.Error(context.Background(), selfmonitor.DownDockerComposeError, "stdout", execError.Error.Error())
		return execError.Error
	}
	_ = os.Remove(composeFile)
	return nil
}

// RemoveLeftoverE2EContainers force-removes E2E containers for the current or given case project.
func RemoveLeftoverE2EContainers() error {
	caseHome := config.CaseHome
	if caseHome == "" {
		return removeContainersByNameFilters("loongcollectorC")
	}
	projectName := ComposeProjectName(caseHome)
	if projectName == "" {
		return removeContainersByNameFilters("loongcollectorC")
	}
	return removeContainersByNameFilters(projectName)
}

func removeContainersByNameFilters(filters ...string) error {
	if _, err := exec.LookPath("docker"); err != nil {
		return nil
	}
	var joined error
	for _, nameFilter := range filters {
		out, err := exec.Command("docker", "ps", "-aq", "--filter", "name="+nameFilter).CombinedOutput()
		if err != nil {
			joined = errors.Join(joined, err)
			continue
		}
		ids := strings.Fields(strings.TrimSpace(string(out)))
		if len(ids) == 0 {
			continue
		}
		args := append([]string{"rm", "-f"}, ids...)
		if rmOut, rmErr := exec.Command("docker", args...).CombinedOutput(); rmErr != nil {
			logger.Warning(context.Background(), selfmonitor.StopDockerComposeError,
				"filter", nameFilter, "err", rmErr, "output", string(rmOut))
			joined = errors.Join(joined, rmErr)
		} else {
			logger.Infof(context.Background(), "removed %d e2e container(s) matching name=%s", len(ids), nameFilter)
		}
	}
	return joined
}

func clearNetworkMappingLocked() {
	for k := range networkMapping {
		delete(networkMapping, k)
	}
}
