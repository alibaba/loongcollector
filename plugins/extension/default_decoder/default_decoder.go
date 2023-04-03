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

package defaultdecoder

import (
	"encoding/json"
	"fmt"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder"
)

// ensure ExtensionDefaultDecoder implements the extensions.Decoder interface
var _ extensions.Decoder = (*ExtensionDefaultDecoder)(nil)

type ExtensionDefaultDecoder struct {
	extensions.Decoder

	Format  string
	options map[string]interface{} // additional properties map to here
}

func (d *ExtensionDefaultDecoder) UnmarshalJSON(bytes []byte) error {
	err := json.Unmarshal(bytes, &d.options)
	if err != nil {
		return err
	}

	format, ok := d.options["Format"].(string)
	if !ok {
		return fmt.Errorf("field Format should be type of string")
	}

	delete(d.options, "Format")
	d.Format = format
	return nil
}

func (d *ExtensionDefaultDecoder) Description() string {
	return "default decoder that support builtin formats"
}

func (d *ExtensionDefaultDecoder) Init(context pipeline.Context) error {
	var options decoder.Option
	err := mapstructure.Decode(d.options, &options)
	if err != nil {
		return err
	}
	d.Decoder, err = decoder.GetDecoderWithOptions(d.Format, options)
	return err
}

func (d *ExtensionDefaultDecoder) Stop() error {
	return nil
}

func init() {
	pipeline.AddExtensionCreator("ext_default_decoder", func() pipeline.Extension {
		return &ExtensionDefaultDecoder{}
	})
}
