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

package odps

import (
	"strconv"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherOdps(t *testing.T) {
	d := NewFlusherOdps()
	require.NotNil(t, d)
}

func TestConnectAndWrite(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integratio n test in short mode")
	}

	projectName := "test_logagent_to_mc"
	tableName := "blob_1"
	partitionConfig := "pt=%Y%m%d"
	d := &FlusherOdps{
		AccessKeyID:     "xx",
		AccessKeySecret: "xx",
		Endpoint:        "",
		ProjectName:     projectName,
		TableName:       tableName,
		TimeRange:       60,
		PartitionConfig: partitionConfig,
	}

	lctx := mock.NewEmptyContext("p", "l", "c")
	err := d.Init(lctx)
	require.NoError(t, err)

	lgl := makeTestLogGroupList()

	err = d.Flush(projectName, tableName, "configName", lgl.GetLogGroupList())
	require.NoError(t, err)
	_ = d.Stop()
}

func makeTestLogGroupList() *protocol.LogGroupList {
	f := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs: make([]*protocol.Log, 0, 10),
		}
		for j := 1; j <= 10; j++ {
			f["field1"] = strconv.Itoa(i)
			f["field2"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(f)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}
