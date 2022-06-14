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

//go:build !linux
// +build !linux

package helper

import (
	"errors"
	"runtime"

	docker "github.com/fsouza/go-dockerclient"
)

var errUninplemented = errors.New("Unimplemented on " + runtime.GOOS)

var containerdUnixSocket = "/run/containerd/containerd.sock"

var criRuntimeWrapper *CRIRuntimeWrapper

type CRIRuntimeWrapper struct {
}

func (cw *CRIRuntimeWrapper) fetchOne(containerID string) error {
	return nil
}

func (cw *CRIRuntimeWrapper) fetchAll() error {
	return nil
}

func (cw *CRIRuntimeWrapper) loopSyncContainers() {

}

func (cw *CRIRuntimeWrapper) sweepCache() {

}

func IsCRIRuntimeValid(_ string) bool {
	return false
}

func (cw *CRIRuntimeWrapper) lookupContainerRootfsAbsDir(_ *docker.Container) string {
	return ""
}

// NewCRIRuntimeWrapper ...
func NewCRIRuntimeWrapper(_ *DockerCenter) (*CRIRuntimeWrapper, error) {
	return nil, errUninplemented
}

func ContainerProcessAlive(pid int) bool {
	return true
}
