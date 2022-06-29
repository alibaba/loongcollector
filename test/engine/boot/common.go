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

package boot

import (
	"errors"
	"fmt"
	"sync"

	"github.com/alibaba/ilogtail/test/config"
)

var networkMapping = make(map[string]string)
var mu sync.Mutex
var instance Booter
var started bool

// Booter start or stop the virtual environment.
type Booter interface {
	Start() error
	Stop() error
	CopyCoreLogs()
}

// Load configuration to the Booter.
func Load(cfg *config.Case) error {
	mu.Lock()
	defer mu.Unlock()
	if cfg.Boot.Category == composeCategory {
		instance = NewComposeBooter(cfg)
		return nil
	}
	return fmt.Errorf("%s category is not suppported", cfg.Boot.Category)
}

// Start initialize the virtual environment and execute setup commands on the host machine.
func Start() error {
	mu.Lock()
	defer mu.Unlock()
	if started {
		return nil
	}
	if instance == nil {
		return errors.New("cannot start booter when config unloading")
	}
	if err := instance.Start(); err != nil {
		return err
	}
	started = true
	return nil
}

// ShutDown close the virtual environment.
func ShutDown() error {
	mu.Lock()
	defer mu.Unlock()
	if !started {
		return nil
	}
	if instance == nil {
		return errors.New("cannot stop booter when config unloading")
	}
	if err := instance.Stop(); err != nil {
		return err
	}
	started = false
	return nil
}

func CopyCoreLogs() {
	mu.Lock()
	defer mu.Unlock()
	if !started {
		return
	}
	instance.CopyCoreLogs()
}

func GetPhysicalAddress(virtual string) string {
	mu.Lock()
	defer mu.Unlock()
	return networkMapping[virtual]
}

func registerNetMapping(virtual, physical string) {
	networkMapping[virtual] = physical
}
