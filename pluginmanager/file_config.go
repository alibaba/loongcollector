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
	"context"
	"encoding/json"
	"io/ioutil"
	"math"
	"os"
	"path/filepath"
	"sync/atomic"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
)

const (
	InstanceIdentityFilename = "instance_identity"
)

var fileConfig *FileConfig

type DoubleBuffer struct {
	buffer      []interface{}
	bufferIndex atomic.Int32
}

func NewDoubleBuffer() *DoubleBuffer {
	return &DoubleBuffer{
		buffer:      make([]interface{}, 2),
		bufferIndex: atomic.Int32{},
	}
}

func (db *DoubleBuffer) Get() interface{} {
	return db.buffer[db.bufferIndex.Load()]
}

func (db *DoubleBuffer) Swap(newBuffer interface{}) {
	db.buffer[1-db.bufferIndex.Load()] = newBuffer
	db.bufferIndex.Store(1 - db.bufferIndex.Load())
}

type InstanceIdentity struct {
	InstanceID         string `json:"instance-id"`
	OwnerAccountID     string `json:"owner-account-id"`
	RegionID           string `json:"region-id"`
	RandomHostID       string `json:"random-host"`
	ECSAssistMachineID string `json:"ecs-assist-machine-id"`
	GFlagHostID        string `json:"-"`
}

type FileConfig struct {
	fileTagsPath     string
	fileTagsInterval int
	fileTagsBuffer   *DoubleBuffer

	instanceIdentityPath   string
	instanceIdentityBuffer *DoubleBuffer

	fileConfigStopCh chan struct{}
}

func InitFileConfig(config *config.GlobalConfig) {
	fileConfig = &FileConfig{
		fileTagsPath:           config.FileTagsPath,
		fileTagsInterval:       config.FileTagsInterval,
		fileTagsBuffer:         NewDoubleBuffer(),
		instanceIdentityPath:   filepath.Join(config.LoongCollectorDataDir, InstanceIdentityFilename),
		instanceIdentityBuffer: NewDoubleBuffer(),
		fileConfigStopCh:       make(chan struct{}),
	}

	go fileConfig.loadLoop(config.AgentHostID)
}

func StopFileConfig() {
	close(fileConfig.fileConfigStopCh)
}

func (fc *FileConfig) GetFileTags() map[string]interface{} {
	result, ok := fc.fileTagsBuffer.Get().(map[string]interface{})
	if !ok {
		return nil
	}
	return result
}

func (fc *FileConfig) GetInstanceIdentity() *InstanceIdentity {
	result, ok := fc.instanceIdentityBuffer.Get().(InstanceIdentity)
	if !ok {
		return nil
	}
	return &result
}

func (fc *FileConfig) loadLoop(gFlagHostID string) {
	lastUpdateInstanceIdentity := time.Now()
	interval := 1
	for {
		select {
		case <-fc.fileConfigStopCh:
			return
		case <-time.After(time.Duration(math.Min(float64(fc.fileTagsInterval), float64(interval))) * time.Second):
			if fileConfig.fileTagsPath != "" {
				data, err := ReadFile(fc.fileTagsPath)
				if err != nil {
					continue
				}
				var fileTags map[string]interface{}
				err = json.Unmarshal(data, &fileTags)
				if err != nil {
					logger.Error(context.Background(), "LOAD_FILE_CONFIG_ALARM", "unmarshal file failed", err)
					continue
				}
				fc.fileTagsBuffer.Swap(fileTags)
			}
			if time.Now().Sub(lastUpdateInstanceIdentity) > time.Duration(interval)*time.Second {
				data, err := ReadFile(fc.instanceIdentityPath)
				var instanceIdentity InstanceIdentity
				if err == nil {
					err = json.Unmarshal(data, &instanceIdentity)
					if err != nil {
						logger.Error(context.Background(), "LOAD_FILE_CONFIG_ALARM", "unmarshal file failed", err)
					}
				}
				instanceIdentity.GFlagHostID = gFlagHostID
				if fc.instanceIdentityBuffer.Get() == nil || instanceIdentity.InstanceID != fc.instanceIdentityBuffer.Get().(InstanceIdentity).InstanceID {
					fc.instanceIdentityBuffer.Swap(instanceIdentity)
				}
				interval = int(math.Min(float64(interval*2), 3600*24))
			}
		}
	}
}

func ReadFile(path string) ([]byte, error) {
	_, err := os.Stat(path)
	if os.IsNotExist(err) {
		return nil, err
	}
	file, err := os.Open(path) //nolint:gosec
	if err != nil {
		logger.Error(context.Background(), "LOAD_FILE_CONFIG_ALARM", "open file failed", err)
		return nil, err
	}
	defer file.Close() //nolint:gosec
	data, err := ioutil.ReadAll(file)
	if err != nil {
		logger.Error(context.Background(), "LOAD_FILE_CONFIG_ALARM", "read file failed", err)
		return nil, err
	}
	return data, err
}
