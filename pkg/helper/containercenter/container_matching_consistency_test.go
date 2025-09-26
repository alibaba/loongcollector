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

package containercenter

import (
	"encoding/json"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"testing"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Test data structures
type TestContainer struct {
	ID     string            `json:"id"`
	Labels map[string]string `json:"labels"`
	Env    []string          `json:"env"`
	K8S    TestK8SInfo       `json:"k8s"`
}

type TestK8SInfo struct {
	Namespace       string            `json:"namespace"`
	Pod             string            `json:"pod"`
	ContainerName   string            `json:"container_name"`
	Labels          map[string]string `json:"labels"`
	PausedContainer bool              `json:"paused_container"`
}

type TestFilters struct {
	K8sNamespaceRegex     string            `json:"K8sNamespaceRegex,omitempty"`
	K8sPodRegex           string            `json:"K8sPodRegex,omitempty"`
	IncludeK8sLabel       map[string]string `json:"IncludeK8sLabel,omitempty"`
	ExcludeK8sLabel       map[string]string `json:"ExcludeK8sLabel,omitempty"`
	K8sContainerRegex     string            `json:"K8sContainerRegex,omitempty"`
	IncludeEnv            map[string]string `json:"IncludeEnv,omitempty"`
	ExcludeEnv            map[string]string `json:"ExcludeEnv,omitempty"`
	IncludeContainerLabel map[string]string `json:"IncludeContainerLabel,omitempty"`
	ExcludeContainerLabel map[string]string `json:"ExcludeContainerLabel,omitempty"`
}

type TestCase struct {
	Name               string      `json:"name"`
	Description        string      `json:"description"`
	Filters            TestFilters `json:"filters"`
	ExpectedMatchedIDs []string    `json:"expected_matched_ids"`
}

type TestData struct {
	Containers []TestContainer `json:"containers"`
	TestCases  []TestCase      `json:"test_cases"`
}

// convertTestContainerToDockerInfoDetail converts test container data to DockerInfoDetail
func convertTestContainerToDockerInfoDetail(tc TestContainer) *DockerInfoDetail {
	// Create container JSON
	containerJSON := types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID: tc.ID,
			State: &types.ContainerState{
				Status: "running",
			},
		},
		Config: &container.Config{
			Labels: tc.Labels,
			Env:    tc.Env,
		},
	}

	// Create K8S info
	k8sInfo := &K8SInfo{
		Namespace:       tc.K8S.Namespace,
		Pod:             tc.K8S.Pod,
		ContainerName:   tc.K8S.ContainerName,
		Labels:          tc.K8S.Labels,
		PausedContainer: tc.K8S.PausedContainer,
	}

	return &DockerInfoDetail{
		ContainerInfo:    containerJSON,
		K8SInfo:          k8sInfo,
		ContainerNameTag: make(map[string]string),
		EnvConfigInfoMap: make(map[string]*EnvConfigInfo),
	}
}

// convertTestFiltersToGoFilters converts test filters to Go filter structures
func convertTestFiltersToGoFilters(tf TestFilters) (map[string]string, map[string]string, map[string]*regexp.Regexp, map[string]*regexp.Regexp, map[string]string, map[string]string, map[string]*regexp.Regexp, map[string]*regexp.Regexp, *K8SFilter) {
	// Container labels
	includeLabel, includeLabelRegex, _ := SplitRegexFromMap(tf.IncludeContainerLabel)
	excludeLabel, excludeLabelRegex, _ := SplitRegexFromMap(tf.ExcludeContainerLabel)

	// Environment variables
	includeEnv, includeEnvRegex, _ := SplitRegexFromMap(tf.IncludeEnv)
	excludeEnv, excludeEnvRegex, _ := SplitRegexFromMap(tf.ExcludeEnv)

	// K8S filters
	k8sFilter, _ := CreateK8SFilter(tf.K8sNamespaceRegex, tf.K8sPodRegex, tf.K8sContainerRegex, tf.IncludeK8sLabel, tf.ExcludeK8sLabel)

	return includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex,
		includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, k8sFilter
}

func TestContainerMatchingConsistency(t *testing.T) {
	// Get all test files from the container_matching_test_data directory
	testFiles, err := filepath.Glob("../../../core/unittest/container_manager/testDataSet/ContainerManagerUnittest/*.json")
	require.NoError(t, err)
	require.NotEmpty(t, testFiles, "No test files found in container_matching_test_data directory")

	// Run tests for each file
	for _, testFile := range testFiles {
		t.Run(filepath.Base(testFile), func(t *testing.T) {
			runTestFile(t, testFile)
		})
	}
}

func runTestFile(t *testing.T, testFilePath string) {
	// Load test data
	data, err := os.ReadFile(testFilePath)
	require.NoError(t, err)

	var testData TestData
	err = json.Unmarshal(data, &testData)
	require.NoError(t, err)

	// Convert containers to DockerInfoDetail
	containers := make(map[string]*DockerInfoDetail)
	for _, tc := range testData.Containers {
		containers[tc.ID] = convertTestContainerToDockerInfoDetail(tc)
	}

	// Set up the global container center with test data
	containerCenterInstance := getContainerCenterInstance()
	containerCenterInstance.lock.Lock()
	containerCenterInstance.containerMap = containers
	containerCenterInstance.lock.Unlock()
	defer func() {
		// Clean up after test
		containerCenterInstance.lock.Lock()
		containerCenterInstance.containerMap = make(map[string]*DockerInfoDetail)
		containerCenterInstance.lock.Unlock()
	}()

	// Clear K8S cache for all containers to ensure clean state between tests
	for _, container := range containers {
		if container.K8SInfo != nil {
			container.K8SInfo.mu.Lock()
			if container.K8SInfo.matchedCache != nil {
				container.K8SInfo.matchedCache = make(map[uint64]bool)
			}
			container.K8SInfo.mu.Unlock()
		}
	}

	// Run each test case
	for _, tc := range testData.TestCases {
		t.Run(tc.Name, func(t *testing.T) {
			// Clear K8S cache for each test case to ensure isolation
			for _, container := range containers {
				if container.K8SInfo != nil {
					container.K8SInfo.mu.Lock()
					container.K8SInfo.matchedCache = make(map[uint64]bool)
					container.K8SInfo.mu.Unlock()
				}
			}

			// Convert filters - ensure clean state for each test
			includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex,
				includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, k8sFilter :=
				convertTestFiltersToGoFilters(tc.Filters)

			// Test matching using the Go implementation
			fullList := make(map[string]bool)
			matchList := make(map[string]*DockerInfoDetail)

			// Single call - all containers will be treated as "new" since fullList is empty
			_, _, _, _ = GetContainerByAcceptedInfoV2(fullList, matchList,
				includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex,
				includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex,
				k8sFilter)

			// Extract matched container IDs
			actualMatchedIDs := make([]string, 0)
			for id := range matchList {
				actualMatchedIDs = append(actualMatchedIDs, id)
			}

			// Sort for consistent comparison
			sort.Strings(actualMatchedIDs)
			sort.Strings(tc.ExpectedMatchedIDs)

			// Verify results
			assert.Equal(t, tc.ExpectedMatchedIDs, actualMatchedIDs,
				"Test case '%s': expected matched IDs %v, got %v", tc.Name, tc.ExpectedMatchedIDs, actualMatchedIDs)
		})
	}
}

// TestContainerMatchingLogicValidation tests specific matching logic edge cases
func TestContainerMatchingLogicValidation(t *testing.T) {
	// Test case 1: Empty include filters should match all when no exclude filters
	t.Run("empty_include_filters", func(t *testing.T) {
		container := &DockerInfoDetail{
			ContainerInfo: types.ContainerJSON{
				Config: &container.Config{
					Labels: map[string]string{"app": "test"},
					Env:    []string{"KEY=value"},
				},
			},
			K8SInfo: &K8SInfo{
				Namespace:     "default",
				Pod:           "test-pod",
				ContainerName: "test-container",
				Labels:        map[string]string{"k8s-label": "value"},
			},
		}

		// Empty filters should match
		result := isContainerLabelMatch(map[string]string{}, map[string]string{},
			map[string]*regexp.Regexp{}, map[string]*regexp.Regexp{}, container)
		assert.True(t, result)

		result = isContainerEnvMatch(map[string]string{}, map[string]string{},
			map[string]*regexp.Regexp{}, map[string]*regexp.Regexp{}, container)
		assert.True(t, result)

		k8sFilter, _ := CreateK8SFilter("", "", "", map[string]string{}, map[string]string{})
		result = container.K8SInfo.IsMatch(k8sFilter)
		assert.True(t, result)
	})

	// Test case 2: Include filters with no matches should fail
	t.Run("include_filters_no_match", func(t *testing.T) {
		container := &DockerInfoDetail{
			ContainerInfo: types.ContainerJSON{
				Config: &container.Config{
					Labels: map[string]string{"app": "test"},
					Env:    []string{"KEY=value"},
				},
			},
		}

		// Include filter that doesn't match should fail
		result := isContainerLabelMatch(map[string]string{"nonexistent": "value"}, map[string]string{},
			map[string]*regexp.Regexp{}, map[string]*regexp.Regexp{}, container)
		assert.False(t, result)
	})

	// Test case 3: Exclude filters that match should fail
	t.Run("exclude_filters_match", func(t *testing.T) {
		container := &DockerInfoDetail{
			ContainerInfo: types.ContainerJSON{
				Config: &container.Config{
					Labels: map[string]string{"app": "test"},
				},
			},
		}

		// Exclude filter that matches should fail
		result := isContainerLabelMatch(map[string]string{}, map[string]string{"app": "test"},
			map[string]*regexp.Regexp{}, map[string]*regexp.Regexp{}, container)
		assert.False(t, result)
	})

	// Test case 4: Paused containers should not match K8S filters
	t.Run("paused_container_no_match", func(t *testing.T) {
		container := &DockerInfoDetail{
			K8SInfo: &K8SInfo{
				Namespace:       "default",
				Pod:             "test-pod",
				ContainerName:   "test-container",
				PausedContainer: true,
			},
		}

		k8sFilter, _ := CreateK8SFilter("", "", "", map[string]string{}, map[string]string{})
		result := container.K8SInfo.IsMatch(k8sFilter)
		assert.False(t, result)
	})
}
