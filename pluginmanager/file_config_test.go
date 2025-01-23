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

package pluginmanager

import (
	"os"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/config"
)

func TestDoubleBuffer(t *testing.T) {
	db := NewDoubleBuffer()
	wg := sync.WaitGroup{}
	wg.Add(1)
	go func() {
		for i := 0; i < 100; i++ {
			db.Swap(2)
			time.Sleep(10 * time.Millisecond)
			db.Swap(1)
		}
		wg.Done()
	}()
	for i := 0; i < 100; i++ {
		db.Get()
	}
	wg.Wait()
	assert.Equal(t, db.buffer[0], 1)
	assert.Equal(t, db.buffer[1], 2)
}

func TestFileConfig(t *testing.T) {
	globalConfig := config.GlobalConfig{
		FileTagsPath:     "test.json",
		FileTagsInterval: 1,
	}
	InitFileConfig(&globalConfig)
	testJSON := []byte(`{
        "test1":  "test value1"
    }`)
	os.WriteFile("test.json", testJSON, 0644)
	time.Sleep(2 * time.Second)
	buffer := fileConfig.GetFileTags()
	assert.Equal(t, buffer["test1"].(string), "test value1")
	testJSON = []byte(`{
	    "test2": "test value2"
	}`)
	os.WriteFile("test.json", testJSON, 0644)
	time.Sleep(2 * time.Second)
	buffer = fileConfig.GetFileTags()
	assert.Equal(t, buffer["test2"].(string), "test value2")

	os.Remove("test.json")
	StopFileConfig()
}

func TestInstanceIdentity(t *testing.T) {
	globalConfig := config.GlobalConfig{
		FileTagsPath:          "test.json",
		FileTagsInterval:      1,
		AgentHostID:           "test",
		LoongCollectorDataDir: ".",
	}
	InitFileConfig(&globalConfig)
	testJSON := []byte(`{
        "instance-id": "test1",
        "owner-account-id": "test2",
        "region-id": "test3",
        "random-host": "test4",
        "ecs-assist-machine-id": "test5"
    }`)
	os.WriteFile(InstanceIdentityFilename, testJSON, 0644)
	time.Sleep(2 * time.Second)
	identity := fileConfig.GetInstanceIdentity()
	assert.Equal(t, identity.InstanceID, "test1")
	assert.Equal(t, identity.OwnerAccountID, "test2")
	assert.Equal(t, identity.RegionID, "test3")
	assert.Equal(t, identity.RandomHostID, "test4")
	assert.Equal(t, identity.ECSAssistMachineID, "test5")
	assert.Equal(t, identity.GFlagHostID, "test")

	testJSON = []byte(`{
        "instance-id": "test6",
        "owner-account-id": "test7",
        "region-id": "test8",
        "random-host": "test9",
        "ecs-assist-machine-id": "test10"
	}`)
	os.WriteFile(InstanceIdentityFilename, testJSON, 0644)
	time.Sleep(2 * time.Second)
	identity = fileConfig.GetInstanceIdentity()
	assert.Equal(t, identity.InstanceID, "test6")
	assert.Equal(t, identity.OwnerAccountID, "test7")
	assert.Equal(t, identity.RegionID, "test8")
	assert.Equal(t, identity.RandomHostID, "test9")
	assert.Equal(t, identity.ECSAssistMachineID, "test10")
	assert.Equal(t, identity.GFlagHostID, "test")

	os.Remove(InstanceIdentityFilename)
	StopFileConfig()
}
