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
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestContainerSnapshotAndDeleteEvents(t *testing.T) {
	events := AppendContainerEvents(nil, []*ContainerDetail{{
		DataType: "container_info", Project: "project", ContainerID: "0123456789abcdef",
		ContainerIP: "127.0.0.1", ContainerName: "container", LogPath: "/container/log",
		Env: map[string]string{"APP": "test"}, ContainerLabels: map[string]string{"team": "example"}, K8sLabels: map[string]string{"component": "api"},
	}})
	require.Len(t, events, 1)
	log := events[0].(*models.Log)
	require.Equal(t, "0123456789ab", log.GetIndices().Get("container_id"))
	require.Equal(t, "project", log.GetIndices().Get("project"))
	require.Equal(t, "container", log.GetIndices().Get("container_name"))
	require.Equal(t, "/container/log", log.GetIndices().Get("log_path"))
	require.JSONEq(t, `{"APP":"test"}`, log.GetIndices().Get("env").(string))
	require.JSONEq(t, `{"team":"example"}`, log.GetIndices().Get("labels").(string))
	require.JSONEq(t, `{"component":"api"}`, log.GetIndices().Get("k8s_labels").(string))
	require.Equal(t, uint64(0), log.Timestamp%1e9)
	require.Positive(t, log.Timestamp)

	deleted := NewDeletedContainersEvent("project", "0123456789ab;test")
	require.Equal(t, "delete_containers", deleted.GetIndices().Get("type"))
	require.Equal(t, "project", deleted.GetIndices().Get("project"))
	require.Equal(t, "0123456789ab;test", deleted.GetIndices().Get("container_ids"))
	require.Equal(t, uint64(0), deleted.Timestamp%1e9)
	require.Positive(t, deleted.Timestamp)
}
