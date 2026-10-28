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

package dockercompose

import (
	"context"
	"errors"
	"os"
	"os/exec"
	"strings"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

// ForceShutDown is an alias for ShutDown; always tears down compose and removes orphans.
func ForceShutDown() error {
	return ShutDown()
}

type BootType = string

const (
	DockerComposeBootTypeE2E       BootType = "e2e"
	DockerComposeBootTypeBenchmark BootType = "benchmark"
)

var networkMapping = make(map[string]string)
var mu sync.Mutex
var instance Booter
var started bool

// Booter start or stop the virtual environment.
type Booter interface {
	Start(ctx context.Context) error
	Stop() error
	CopyCoreLogs()
}

// Load configuration to the Booter.
func Load(bootType BootType) error {
	mu.Lock()
	defer mu.Unlock()
	switch bootType {
	case DockerComposeBootTypeE2E:
		instance = NewComposeBooter()
	case DockerComposeBootTypeBenchmark:
		instance = NewComposeBenchmarkBooter()
	default:
		return errors.New("invalid load type")
	}
	return nil
}

// Start initialize the virtual environment and execute setup commands on the host machine.
func Start(ctx context.Context) error {
	mu.Lock()
	defer mu.Unlock()
	if started {
		return nil
	}
	if instance == nil {
		return errors.New("cannot start booter when config unloading")
	}
	if err := instance.Start(ctx); err != nil {
		return err
	}
	started = true
	return nil
}

// ShutDown stops compose and removes any leftover E2E containers (even if Start failed).
func ShutDown() error {
	mu.Lock()
	defer mu.Unlock()
	var joined error
	if instance != nil {
		if err := instance.Stop(); err != nil {
			joined = errors.Join(joined, err)
		}
	} else if config.CaseHome != "" {
		if err := ComposeDown(config.CaseHome); err != nil {
			joined = errors.Join(joined, err)
		}
	}
	if err := RemoveLeftoverE2EContainers(); err != nil {
		joined = errors.Join(joined, err)
	}
	clearNetworkMappingLocked()
	started = false
	return joined
}

func CopyCoreLogs() {
	mu.Lock()
	defer mu.Unlock()
	if !started {
		return
	}
	instance.CopyCoreLogs()
}

// TryCopyCoreLogs exports loongcollector logs to test/e2e/report/<case>_log/ for debugging.
func TryCopyCoreLogs() {
	CopyCoreLogs()
	if config.LogDir == "" {
		return
	}
	if _, err := exec.LookPath("docker"); err != nil {
		return
	}
	out, err := exec.Command("docker", "ps", "-a", "--filter", "name=loongcollectorC", "--format", "{{.ID}}").CombinedOutput()
	if err != nil {
		return
	}
	ids := strings.Fields(strings.TrimSpace(string(out)))
	if len(ids) == 0 {
		return
	}
	_ = os.MkdirAll(config.LogDir, 0750)
	for _, id := range ids {
		copyLogFile(id, "loongcollector.LOG")
		copyLogFile(id, "go_plugin.LOG")
	}
}

func copyLogFile(containerID, name string) {
	dest := config.LogDir + "/" + name
	cmd := exec.Command("docker", "cp", containerID+":/usr/local/loongcollector/log/"+name, dest)
	if out, err := cmd.CombinedOutput(); err != nil {
		logger.Debugf(context.Background(), "TryCopyCoreLogs %s: %v %s", name, err, string(out))
	}
}

func GetPhysicalAddress(virtual string) string {
	mu.Lock()
	defer mu.Unlock()
	return networkMapping[virtual]
}

func registerNetMapping(virtual, physical string) {
	networkMapping[virtual] = physical
}
