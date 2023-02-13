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

package clickhouse

import (
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"strconv"
	"testing"

	"github.com/stretchr/testify/require"
)

// Invalid Test
func InvalidTestConnectAndWrite(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	f := NewFlusherClickHouse()
	f.Addresses = []string{"127.0.0.1:9000"}
	f.Authentication.PlainText.Username = ""
	f.Authentication.PlainText.Password = ""
	f.Authentication.PlainText.Database = "default"
	f.Cluster = ""
	f.Table = "demo"
	f.flusher = f.BufferFlush
	// Verify that we can connect to the ClickHouse
	lctx := mock.NewEmptyContext("p", "l", "c")
	err := f.Init(lctx)
	require.NoError(t, err)

	// Verify that we can successfully write data to the ClickHouse buffer engine table
	lgl := makeTestLogGroupList()
	err = f.Flush("projectName", "logstoreName", "configName", lgl.GetLogGroupList())
	require.NoError(t, err)
	_ = f.Stop()
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
			f["group"] = strconv.Itoa(i)
			f["message"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(f)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}
