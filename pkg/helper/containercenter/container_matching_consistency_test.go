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
	ContainerLabels TestLabelFilters `json:"container_labels"`
	Env             TestLabelFilters `json:"env"`
	K8S             TestK8SFilters   `json:"k8s"`
}

type TestLabelFilters struct {
	Include TestLabelFilterCriteria `json:"include"`
	Exclude TestLabelFilterCriteria `json:"exclude"`
}

type TestLabelFilterCriteria struct {
	Static map[string]string `json:"static"`
	Regex  map[string]string `json:"regex"`
}

type TestK8SFilters struct {
	NamespaceRegex string           `json:"namespace_regex"`
	PodRegex       string           `json:"pod_regex"`
	ContainerRegex string           `json:"container_regex"`
	Labels         TestLabelFilters `json:"labels"`
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
	includeLabel := make(map[string]string)
	excludeLabel := make(map[string]string)
	includeLabelRegex := make(map[string]*regexp.Regexp)
	excludeLabelRegex := make(map[string]*regexp.Regexp)

	for k, v := range tf.ContainerLabels.Include.Static {
		includeLabel[k] = v
	}
	for k, v := range tf.ContainerLabels.Exclude.Static {
		excludeLabel[k] = v
	}
	for k, v := range tf.ContainerLabels.Include.Regex {
		includeLabelRegex[k] = regexp.MustCompile(v)
	}
	for k, v := range tf.ContainerLabels.Exclude.Regex {
		excludeLabelRegex[k] = regexp.MustCompile(v)
	}

	// Environment variables
	includeEnv := make(map[string]string)
	excludeEnv := make(map[string]string)
	includeEnvRegex := make(map[string]*regexp.Regexp)
	excludeEnvRegex := make(map[string]*regexp.Regexp)

	for k, v := range tf.Env.Include.Static {
		includeEnv[k] = v
	}
	for k, v := range tf.Env.Exclude.Static {
		excludeEnv[k] = v
	}
	for k, v := range tf.Env.Include.Regex {
		includeEnvRegex[k] = regexp.MustCompile(v)
	}
	for k, v := range tf.Env.Exclude.Regex {
		excludeEnvRegex[k] = regexp.MustCompile(v)
	}

	// K8S filters
	var k8sFilter *K8SFilter
	if tf.K8S.NamespaceRegex != "" || tf.K8S.PodRegex != "" || tf.K8S.ContainerRegex != "" ||
		len(tf.K8S.Labels.Include.Static) > 0 || len(tf.K8S.Labels.Exclude.Static) > 0 ||
		len(tf.K8S.Labels.Include.Regex) > 0 || len(tf.K8S.Labels.Exclude.Regex) > 0 {

		var namespaceReg, podReg, containerReg *regexp.Regexp
		if tf.K8S.NamespaceRegex != "" {
			namespaceReg = regexp.MustCompile(tf.K8S.NamespaceRegex)
		}
		if tf.K8S.PodRegex != "" {
			podReg = regexp.MustCompile(tf.K8S.PodRegex)
		}
		if tf.K8S.ContainerRegex != "" {
			containerReg = regexp.MustCompile(tf.K8S.ContainerRegex)
		}

		includeLabels := make(map[string]string)
		excludeLabels := make(map[string]string)
		includeLabelRegs := make(map[string]*regexp.Regexp)
		excludeLabelRegs := make(map[string]*regexp.Regexp)

		for k, v := range tf.K8S.Labels.Include.Static {
			includeLabels[k] = v
		}
		for k, v := range tf.K8S.Labels.Exclude.Static {
			excludeLabels[k] = v
		}
		for k, v := range tf.K8S.Labels.Include.Regex {
			includeLabelRegs[k] = regexp.MustCompile(v)
		}
		for k, v := range tf.K8S.Labels.Exclude.Regex {
			excludeLabelRegs[k] = regexp.MustCompile(v)
		}

		k8sFilter, _ = CreateK8SFilter("", "", "", includeLabels, excludeLabels)
		k8sFilter.NamespaceReg = namespaceReg
		k8sFilter.PodReg = podReg
		k8sFilter.ContainerReg = containerReg
		k8sFilter.IncludeLabelRegs = includeLabelRegs
		k8sFilter.ExcludeLabelRegs = excludeLabelRegs
	}

	return includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex,
		includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, k8sFilter
}

func TestContainerMatchingConsistency(t *testing.T) {
	// Load test data
	data, err := os.ReadFile("./container_matching_test_data.json")
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

			// Clear both fullList and matchList to start fresh
			fullList = make(map[string]bool)
			matchList = make(map[string]*DockerInfoDetail)

			// Single call - all containers will be treated as "new" since fullList is empty
			_, _, _, _ = GetContainerByAcceptedInfoV2(fullList, matchList,
				includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex,
				includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex,
				k8sFilter)

			// Extract matched container IDs
			var actualMatchedIDs []string
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
