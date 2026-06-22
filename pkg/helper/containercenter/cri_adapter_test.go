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

//go:build linux
// +build linux

package containercenter

import (
	"testing"
	"time"

	"github.com/docker/docker/api/types/container"

	"github.com/stretchr/testify/require"
)

func TestLookupContainerRootfsAbsDir(t *testing.T) {
	crirt := &CRIRuntimeWrapper{
		containerCenter: nil,
		client:          nil,
		runtimeInfo:     CriVersionInfo{},
		containers:      make(map[string]*innerContainerInfo),
		stopCh:          make(<-chan struct{}),
		rootfsCache:     make(map[string]string),
	}

	container := container.InspectResponse{
		ContainerJSONBase: &container.ContainerJSONBase{
			ID: "1234567890abcde",
		},
	}
	dir := crirt.lookupContainerRootfsAbsDir(container)
	require.Equal(t, dir, "")
}

func TestContainerShouldMarkRemove(t *testing.T) {
	crirt := &CRIRuntimeWrapper{
		containerCenter: nil,
		client:          nil,
		runtimeInfo:     CriVersionInfo{},
		containers:      make(map[string]*innerContainerInfo),
		stopCh:          make(<-chan struct{}),
		rootfsCache:     make(map[string]string),
	}

	tests := []struct {
		name                             string
		forceReleaseDeletedFileFDTimeout int // in seconds, -1 means disabled
		containerStatus                  string
		containerState                   CriContainerState
		expectedResult                   bool
	}{
		{
			name:                             "ForceRelease enabled (0) with exited status",
			forceReleaseDeletedFileFDTimeout: 0,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   true,
		},
		{
			name:                             "ForceRelease enabled (0) with running status",
			forceReleaseDeletedFileFDTimeout: 0,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease enabled (positive) with exited status",
			forceReleaseDeletedFileFDTimeout: 120,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   true,
		},
		{
			name:                             "ForceRelease enabled (positive) with running status",
			forceReleaseDeletedFileFDTimeout: 120,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease disabled with exited state",
			forceReleaseDeletedFileFDTimeout: -1,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerExited,
			expectedResult:                   true,
		},
		{
			name:                             "ForceRelease disabled with running state",
			forceReleaseDeletedFileFDTimeout: -1,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease disabled with created state",
			forceReleaseDeletedFileFDTimeout: -1,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerCreated,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease enabled with exited status and exited state",
			forceReleaseDeletedFileFDTimeout: 60,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerExited,
			expectedResult:                   true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Save and restore the global variable
			originalTimeout := ForceReleaseDeletedFileFDTimeout
			defer func() {
				ForceReleaseDeletedFileFDTimeout = originalTimeout
			}()

			// Set the timeout for this test case
			ForceReleaseDeletedFileFDTimeout = time.Duration(tt.forceReleaseDeletedFileFDTimeout) * time.Second

			innerContainer := &innerContainerInfo{
				State:  tt.containerState,
				Pid:    12345,
				Name:   "test-container",
				Status: tt.containerStatus,
			}

			result := crirt.containerShouldMarkRemove(innerContainer)
			require.Equal(t, tt.expectedResult, result)
		})
	}
}
