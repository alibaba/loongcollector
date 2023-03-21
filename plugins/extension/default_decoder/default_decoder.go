package defaultdecoder

import (
	"encoding/json"
	"fmt"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/helper/decoder"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
)

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
