// Copyright 2022 iLogtail Authors
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

package raw

import (
	"net/http"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
)

type Decoder struct {
	DisableUncompress bool
}

func (d *Decoder) DecodeV2(data []byte, req *http.Request) (groups []*models.PipelineGroupEvents, decodeErr error) {
	groupEvents := &models.PipelineGroupEvents{}
	groupEvents.Group = models.NewGroup(models.NewMetadata(), models.NewTags())
	groupEvents.Events = []models.PipelineEvent{models.ByteArray(data)}
	return []*models.PipelineGroupEvents{groupEvents}, nil
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	if d.DisableUncompress {
		return common.CollectRawBody(res, req, maxBodySize)
	}
	return common.CollectBody(res, req, maxBodySize)
}

func (d *Decoder) Decode(data []byte, req *http.Request, tags map[string]string) (logs []*protocol.Log, err error) {
	log := &protocol.Log{
		Time: uint32(time.Now().Unix()),
		Contents: []*protocol.Log_Content{
			{
				Key:   models.ContentKey,
				Value: string(data),
			},
		},
	}
	logs = append(logs, log)
	return logs, nil
}
