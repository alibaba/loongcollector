// Copyright 2024 iLogtail Authors
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

package defaultencoder

import (
	"encoding/json"
	"errors"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/encoder"
)

// ensure ExtensionDefaultEncoder implements the extensions.EncoderExtension interface
var _ extensions.EncoderExtension = (*ExtensionDefaultEncoder)(nil)

type ExtensionDefaultEncoder struct {
	extensions.Encoder

	Format  string
	options map[string]any // additional properties map to here
}

func NewExtensionDefaultEncoder() *ExtensionDefaultEncoder {
	return &ExtensionDefaultEncoder{}
}

func (e *ExtensionDefaultEncoder) UnmarshalJSON(bytes []byte) error {
	err := json.Unmarshal(bytes, &e.options)
	if err != nil {
		return err
	}

	format, ok := e.options["Format"].(string)
	if !ok {
		return errors.New("field Format should be type of string")
	}

	delete(e.options, "Format")
	e.Format = format

	return nil
}

func (e *ExtensionDefaultEncoder) Description() string {
	return "default encoder that support builtin formats"
}

func (e *ExtensionDefaultEncoder) Init(context pipeline.Context) error {
	enc, err := encoder.NewEncoder(e.Format, e.options)
	if err != nil {
		return err
	}

	e.Encoder = enc

	logger.Infof(context.GetRuntimeContext(), "%s init success, encoder: %s", e.Description(), e.Format)

	return nil
}

func (e *ExtensionDefaultEncoder) Stop() error {
	return nil
}

func init() {
	pipeline.AddExtensionCreator("ext_default_encoder", func() pipeline.Extension {
		return NewExtensionDefaultEncoder()
	})
}
