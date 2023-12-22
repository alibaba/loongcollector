package protocol

import (
	"bytes"
	"fmt"

	jsoniter "github.com/json-iterator/go"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

var (
	sep = []byte("\n")
)

func (c *Converter) ConvertToJsonlineProtocolStreamFlatten(logGroup *protocol.LogGroup) ([]byte, []map[string]string, error) {
	convertedLogs, _, err := c.ConvertToSingleProtocolLogsFlatten(logGroup, nil)
	if err != nil {
		return nil, nil, err
	}
	joinedStream := *GetPooledByteBuf()
	for i, log := range convertedLogs {
		switch c.Encoding {
		case EncodingJSON:
			var err error
			joinedStream, err = marshalWithoutHTMLEscapedWithoutAlloc(log, bytes.NewBuffer(joinedStream))
			if err != nil {
				// release byte buffer
				PutPooledByteBuf(&joinedStream)
				return nil, nil, fmt.Errorf("unable to marshal log: %v", log)
			}
			// trim and append a \n
			joinedStream = trimRightByte(joinedStream, sep[0])
			if i < len(convertedLogs)-1 {
				joinedStream = append(joinedStream, sep[0])
			}
		default:
			return nil, nil, fmt.Errorf("unsupported encoding format: %s", c.Encoding)
		}
	}
	return joinedStream, nil, nil
}

func marshalWithoutHTMLEscapedWithoutAlloc(data interface{}, bf *bytes.Buffer) ([]byte, error) {
	enc := jsoniter.ConfigFastest.NewEncoder(bf)
	if err := enc.Encode(data); err != nil {
		return nil, err
	}
	return bf.Bytes(), nil
}

func trimRightByte(s []byte, c byte) []byte {
	for len(s) > 0 && s[len(s)-1] == c {
		s = s[:len(s)-1]
	}
	return s
}
