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

	"github.com/alibaba/ilogtail/pkg/logtail"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/pkg/util"
)

// SlsFlusher implements both the v1 (Flush) and v2 (Export) flusher interfaces
// so it can be loaded by either Runner; it uses symbols in LogtailAdaptor(.so)
// to push log groups into Logtail's sending queue.
var (
	_ pipeline.FlusherV1 = (*SlsFlusher)(nil)
	_ pipeline.FlusherV2 = (*SlsFlusher)(nil)
)

// SlsFlusher uses symbols in LogtailAdaptor(.so) to flush log groups to Logtail.
type SlsFlusher struct { // nolint:revive
	EnableShardHash bool
	KeepShardHash   bool

	context pipeline.Context
	// sendPb / sendPbV2 abstract the cgo calls into Logtail so the send path can
	// be exercised in unit tests without a live Logtail adaptor. They default to
	// the real logtail symbols in Init.
	sendPb   func(configName, logstore string, pbBuffer []byte, lines int) int
	sendPbV2 func(configName, logstore string, pbBuffer []byte, lines int, hash string) int
}

// Init ...
func (p *SlsFlusher) Init(context pipeline.Context) error {
	p.context = context
	if p.sendPb == nil {
		p.sendPb = logtail.SendPb
	}
	if p.sendPbV2 == nil {
		p.sendPbV2 = logtail.SendPbV2
	}
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
		if err := p.sendLogGroup(configName, logGroup); err != nil {
			return err
		}
	}

	return nil
}

// Export makes flusher_sls loadable in the v2 Runner as a transitional bridge:
// each v2 PipelineGroupEvents is converted to a legacy LogGroup (via B3's
// PipelineGroupEventsToLogGroup, which handles Log/Metric/Span structurally)
// and then pushed through the same SendPb cgo path used by Flush. This keeps a
// single wire format toward the C++ Logtail queue while the v2 pipeline rolls
// out; a native v2 send path can replace it later.
func (p *SlsFlusher) Export(groupEventsList []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	configName := p.context.GetConfigName()
	for _, groupEvents := range groupEventsList {
		logGroup, err := converter.PipelineGroupEventsToLogGroup(groupEvents)
		if err != nil {
			return fmt.Errorf("convert pipeline group events to loggroup err %v", err)
		}
		if logGroup == nil {
			continue
		}
		// The converter does not carry a category; for SLS the category is the
		// logstore, mirroring v1 where LogGroup.Category == logstore.
		if logGroup.Category == "" {
			logGroup.Category = p.context.GetLogstore()
		}
		if err := p.sendLogGroup(configName, logGroup); err != nil {
			return err
		}
	}
	return nil
}

// sendLogGroup marshals a single LogGroup and pushes it into Logtail's sending
// queue via cgo. Shared by the v1 Flush and v2 Export paths so both apply the
// same shard-hash handling and SendPb/SendPbV2 selection.
func (p *SlsFlusher) sendLogGroup(configName string, logGroup *protocol.LogGroup) error {
	if len(logGroup.Logs) == 0 {
		return nil
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

	var rst int
	if !p.EnableShardHash {
		rst = p.sendPb(configName, logGroup.Category, buf, len(logGroup.Logs))
	} else {
		rst = p.sendPbV2(configName, logGroup.Category, buf, len(logGroup.Logs), shardHash)
	}
	if rst < 0 {
		return fmt.Errorf("send error %d", rst)
	}
	return nil
}

// SetUrgent ...
// We do nothing here because necessary flag has already been set in Logtail
// before this method is called. Any future call of IsReady will return
// true so that remaining data can be flushed to Logtail (which will flush
// data to local file system) before it quits.
func (*SlsFlusher) SetUrgent(flag bool) {
}

// Stop ...
// We do nothing here because SlsFlusher does not create any resources.
func (*SlsFlusher) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_sls"] = func() pipeline.Flusher {
		return &SlsFlusher{
			EnableShardHash: false,
			KeepShardHash:   true,
		}
	}
}
