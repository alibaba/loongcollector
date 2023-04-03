// Copyright 2023 iLogtail Authors
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

package cloudmeta

import (
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func Test_cloudMeta_ProcessLogs(t *testing.T) {
	c := new(ProcessorCloudMeta)
	c.Platform = platformmeta.Mock
	c.RenameMetadata = map[string]string{
		platformmeta.FlagInstanceID:         "__instance_id__",
		platformmeta.FlagInstanceName:       "__instance_name__",
		platformmeta.FlagInstanceZone:       "__zone__",
		platformmeta.FlagInstanceRegion:     "__region__",
		platformmeta.FlagInstanceType:       "__instance_type__",
		platformmeta.FlagInstanceVswitchID:  "__vswitch_id__",
		platformmeta.FlagInstanceVpcID:      "__vpc_id__",
		platformmeta.FlagInstanceImageID:    "__image_id__",
		platformmeta.FlagInstanceMaxIngress: "__max_ingress__",
		platformmeta.FlagInstanceMaxEgress:  "__max_egress__",
		platformmeta.FlagInstanceTags:       "__instance_tags__",
	}
	c.Metadata = []string{
		platformmeta.FlagInstanceID,
		platformmeta.FlagInstanceName,
		platformmeta.FlagInstanceRegion,
		platformmeta.FlagInstanceZone,
		platformmeta.FlagInstanceVpcID,
		platformmeta.FlagInstanceVswitchID,
		platformmeta.FlagInstanceTags,
		platformmeta.FlagInstanceType,
		platformmeta.FlagInstanceImageID,
		platformmeta.FlagInstanceMaxIngress,
		platformmeta.FlagInstanceMaxEgress,
	}
	require.NoError(t, c.Init(mock.NewEmptyContext("a", "b", "c")))

	log := &protocol.Log{
		Time: 1,
	}
	logs := c.ProcessLogs([]*protocol.Log{log})
	require.Equal(t, len(logs), 1)
	require.Equal(t, len(logs[0].Contents), 11)
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_id__"), "id_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_name__"), "name_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__zone__"), "zone_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__region__"), "region_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_type__"), "type_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__vswitch_id__"), "vswitch_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__vpc_id__"), "vpc_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__image_id__"), "image_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__max_ingress__"), "0")
	require.Equal(t, test.ReadLogVal(logs[0], "__max_egress__"), "0")
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_tags___tag_key"), "tag_val")

	log.Contents = log.Contents[:0]
	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	time.Sleep(time.Second * 2)
	logs = c.ProcessLogs([]*protocol.Log{log})
	require.Equal(t, len(logs), 1)
	require.Equal(t, len(logs[0].Contents), 11)
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_id__"), "id_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_name__"), "name_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__zone__"), "zone_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__region__"), "region_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_type__"), "type_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__vswitch_id__"), "vswitch_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__vpc_id__"), "vpc_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__image_id__"), "image_xxx")
	require.Equal(t, test.ReadLogVal(logs[0], "__max_ingress__"), "100")
	require.Equal(t, test.ReadLogVal(logs[0], "__max_egress__"), "1000")
	require.Equal(t, test.ReadLogVal(logs[0], "__instance_tags___tag_key"), "tag_val")
	lastRead := c.lastReadTime
	log.Contents = log.Contents[:0]
	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	logs = c.ProcessLogs([]*protocol.Log{log})
	require.Equal(t, test.ReadLogVal(logs[0], "__max_ingress__"), "100")
	require.Equal(t, test.ReadLogVal(logs[0], "__max_egress__"), "1000")
	require.Equal(t, c.lastReadTime, lastRead)
}

func Test_cloudMeta_ProcessJsonLogs(t *testing.T) {
	metas := []string{
		platformmeta.FlagInstanceID,
		platformmeta.FlagInstanceName,
		platformmeta.FlagInstanceTags,
	}
	type fields struct {
		JSONPath string
		Mode     platformmeta.Platform
	}
	tests := []struct {
		name           string
		fields         fields
		initError      bool
		content        string
		key            string
		logsLen        int
		logsContentLen int
		validator      func(log *protocol.Log, t *testing.T)
	}{
		{
			name:           "content mode",
			fields:         fields{},
			initError:      false,
			logsLen:        1,
			logsContentLen: 3,
			validator: func(log *protocol.Log, t *testing.T) {
				println(log.String())
				require.Equal(t, test.ReadLogVal(log, platformmeta.FlagInstanceID), "id_xxx")
				require.Equal(t, test.ReadLogVal(log, platformmeta.FlagInstanceName), "name_xxx")
				require.Equal(t, test.ReadLogVal(log, platformmeta.FlagInstanceTags+"_tag_key"), "tag_val")
			},
		},
		{
			name: "not exist key",
			fields: fields{
				JSONPath: "content",
			},
			initError:      false,
			content:        "json",
			key:            "con",
			logsLen:        1,
			logsContentLen: 2,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "con"), "json")
				require.Equal(t, test.ReadLogVal(log, "content"), "{\"__cloud_instance_id__\":\"id_xxx\",\"__cloud_instance_name__\":\"name_xxx\",\"__cloud_instance_tags___tag_key\":\"tag_val\"}")
			},
		},
		{
			name: "content val illegal",
			fields: fields{
				JSONPath: "content",
			},
			initError:      false,
			content:        "json",
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), "json")
			},
		},
		{
			name: "content val illegal 2",
			fields: fields{
				JSONPath: "content.a",
			},
			initError:      false,
			content:        `{"a":[]}`,
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), "{\"a\":[]}")
			},
		},
		{
			name: "path type illegal",
			fields: fields{
				JSONPath: "content.a",
			},
			initError:      false,
			content:        `{"a":"b"}`,
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), `{"a":"b"}`)
			},
		},
		{
			name: "path type illegal2",
			fields: fields{
				JSONPath: "content.a.b.c",
			},
			initError:      false,
			content:        `{"a": { "b": {"c": "d"}}}`,
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), "{\"a\":{\"b\":{\"c\":\"d\"}}}")
			},
		},
		{
			name: "path type legal",
			fields: fields{
				JSONPath: "content.a.b.c",
			},
			initError:      false,
			content:        `{"a": { "b": {"c": {"d":"e"}}}}`,
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), "{\"a\":{\"b\":{\"c\":{\"__cloud_instance_id__\":\"id_xxx\",\"__cloud_instance_name__\":\"name_xxx\",\"__cloud_instance_tags___tag_key\":\"tag_val\",\"d\":\"e\"}}}}")
			},
		},
		{
			name: "test auto mode",
			fields: fields{
				JSONPath: "content.a.b.c.f",
				Mode:     platformmeta.Auto,
			},
			initError:      false,
			content:        `{"a": { "b": {"c": {"d":"e"}}}}`,
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), "{\"a\": { \"b\": {\"c\": {\"d\":\"e\"}}}}")
			},
		},
		{
			name: "test append structure",
			fields: fields{
				JSONPath: "content.a.b.c.f",
			},
			initError:      false,
			content:        `{"a": { "b": {"c": {"d":"e"}}}}`,
			key:            "content",
			logsLen:        1,
			logsContentLen: 1,
			validator: func(log *protocol.Log, t *testing.T) {
				require.Equal(t, test.ReadLogVal(log, "content"), "{\"a\":{\"b\":{\"c\":{\"d\":\"e\",\"f\":{\"__cloud_instance_id__\":\"id_xxx\",\"__cloud_instance_name__\":\"name_xxx\",\"__cloud_instance_tags___tag_key\":\"tag_val\"}}}}}")
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c := new(ProcessorCloudMeta)
			if tt.fields.Mode == "" {
				c.Platform = platformmeta.Mock
			} else {
				c.Platform = tt.fields.Mode
			}
			c.Metadata = metas
			c.JSONPath = tt.fields.JSONPath
			if tt.initError {
				require.Error(t, c.Init(mock.NewEmptyContext("a", "b", "c")))
				return
			}
			require.NoError(t, c.Init(mock.NewEmptyContext("a", "b", "c")))
			log := &protocol.Log{
				Time: 1,
			}
			if tt.key != "" {
				log.Contents = append(log.Contents, &protocol.Log_Content{
					Key:   tt.key,
					Value: tt.content,
				})

			}
			logs := c.ProcessLogs([]*protocol.Log{log})
			require.Equal(t, len(logs), tt.logsLen)
			require.Equal(t, len(logs[0].Contents), tt.logsContentLen)
			tt.validator(logs[0], t)
		})
	}
}
