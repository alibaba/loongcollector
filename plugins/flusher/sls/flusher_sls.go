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

//go:build linux || windows
// +build linux windows

package sls

import (
	"fmt"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// SlsFlusher uses symbols in LogtailAdaptor(.so) to flush log groups to Logtail.
type SlsFlusher struct {
	EnableShardHash bool
	KeepShardHash   bool

	context    ilogtail.Context
	lenCounter ilogtail.CounterMetric
}

// Init ...
func (p *SlsFlusher) Init(context ilogtail.Context) error {
	p.context = context
	p.lenCounter = helper.NewCounterMetric("flush_sls_size")
	return nil
}

// Description ...
func (p *SlsFlusher) Description() string {
	return "logtail flusher for log service"
}

// IsReady ...
// There is a sending queue in Logtail, call LogtailIsValidToSend through cgo
// to make sure if there is any space for coming data.
func (p *SlsFlusher) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return logtail.IsValidToSend(logstoreKey)
}

// Flush ...
// Because IsReady is called before, Logtail must have space in sending queue,
// just call LogtailSendPb through cgo to push data into queue, Logtail will
// send data to its destination (SLS mostly) according to its config.
func (p *SlsFlusher) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		if len(logGroup.Logs) == 0 {
			continue
		}

		var shardHash string
		if p.EnableShardHash {
			for idx, tag := range logGroup.LogTags {
				if tag.Key == util.ShardHashTagKey {
					shardHash = tag.Value
					if !p.KeepShardHash {
						logGroup.LogTags = append(logGroup.LogTags[0:idx], logGroup.LogTags[idx+1:]...)
					}
					break
				}
			}
		}
		buf, err := logGroup.Marshal()
		if err != nil {
			return fmt.Errorf("loggroup marshal err %v", err)
		}
		bufLen := len(buf)
		p.lenCounter.Add(int64(bufLen))

		var rst int
		if !p.EnableShardHash {
			rst = logtail.SendPb(configName, logGroup.Category, buf, len(logGroup.Logs))
		} else {
			rst = logtail.SendPbV2(configName, logGroup.Category, buf, len(logGroup.Logs), shardHash)
		}
		if rst < 0 {
			return fmt.Errorf("send error %d", rst)
		}
	}

	return nil
}

// SetUrgent ...
// We do nothing here because necessary flag has already been set in Logtail
//   before this method is called. Any future call of IsReady will return
//   true so that remaining data can be flushed to Logtail (which will flush
//   data to local file system) before it quits.
func (*SlsFlusher) SetUrgent(flag bool) {
}

// Stop ...
// We do nothing here because SlsFlusher does not create any resources.
func (*SlsFlusher) Stop() error {
	return nil
}

func init() {
	ilogtail.Flushers["flusher_sls"] = func() ilogtail.Flusher {
		return &SlsFlusher{
			EnableShardHash: false,
			KeepShardHash:   true,
		}
	}
}
