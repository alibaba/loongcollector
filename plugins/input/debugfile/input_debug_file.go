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
	"io/ioutil"

	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// InputDebugFile can reads all data in specified file, and set them as single field.
type InputDebugFile struct {
	InputFilePath string
	FieldName     string

	context pipeline.Context
	content string
}

// Init ...
func (r *InputDebugFile) Init(context pipeline.Context) (int, error) {
	r.context = context
	content, err := ioutil.ReadFile(r.InputFilePath)
	if err != nil {
		return 0, err
	}
	r.content = string(content)
	return 0, nil
}

// Description ...
func (r *InputDebugFile) Description() string {
	return "input plugin for debugging"
}

// Collect ...
func (r *InputDebugFile) Collect(collector pipeline.Collector) error {
	log := map[string]string{}
	log[r.FieldName] = r.content
	collector.AddData(nil, log)
	return nil
}

func init() {
	pipeline.MetricInputs["metric_debug_file"] = func() pipeline.MetricInput {
		return &InputDebugFile{FieldName: "content"}
	}
}
