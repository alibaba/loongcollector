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

package debugfile

import (
	"bufio"
	"os"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

// InputDebugFile can reads some lines from head in specified file, then set them as same field.
type InputDebugFile struct {
	InputFilePath string
	FieldName     string
	LineLimit     int

	context pipeline.Context
	logs    []string
}

// Init ...
func (r *InputDebugFile) Init(context pipeline.Context) (int, error) {
	r.context = context
	file, err := os.Open(r.InputFilePath)
	if err != nil {
		return 0, err
	}
	defer file.Close() //nolint:gosec

	scanner := bufio.NewScanner(file)
	var count int

	for scanner.Scan() {
		line := scanner.Text()
		r.logs = append(r.logs, line)
		count++

		if count == r.LineLimit {
			break
		}
	}

	return 0, nil
}

// Description ...
func (r *InputDebugFile) Description() string {
	return "input plugin for debugging"
}

// Collect ...
func (r *InputDebugFile) Collect(collector pipeline.Collector) error {
	log := map[string]string{}
	log[r.FieldName] = strings.Join(r.logs, "\n")
	collector.AddData(nil, log)

	return nil
}

func (r *InputDebugFile) Read(context pipeline.PipelineContext) error {
	body := strings.Join(r.logs, "\n")
	log := models.NewLog("debug_log", util.ZeroCopyStringToBytes(body), "info", "", "", models.NewTags(), uint64(time.Now().Unix()))
	if r.FieldName != models.BodyKey {
		log.Contents.Add(r.FieldName, body)
	}
	context.Collector().Collect(&models.GroupInfo{}, log)
	return nil
}

func init() {
	pipeline.MetricInputs["metric_debug_file"] = func() pipeline.MetricInput {
		return &InputDebugFile{
			FieldName: models.ContentKey,
			LineLimit: 1000,
		}
	}
}
