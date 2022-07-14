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

package csv

import (
	"encoding/csv"
	"fmt"
	"io"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ProcessorCSVDecoder struct {
	SourceKey        string   `comment:"The source key containing the CSV record"`
	NoKeyError       bool     `comment:"Optional. Whether to report error if no key in the log mathes the SourceKey, default to false"`
	SplitKeys        []string `comment:"The keys matching the decoded CSV fields"`
	SplitSep         string   `comment:"Optional. The Separator, default to ,"`
	TrimLeadingSpace bool     `comment:"Optional. Whether to ignore the leading space in each CSV field, default to false"`
	PreserveOthers   bool     `comment:"Optional. Whether to preserve the remaining record if #splitKeys < #CSV fields, default to false"`
	ExpandOthers     bool     `comment:"Optional. Whether to decode the remaining record if #splitKeys < #CSV fields, default to false"`
	ExpandKeyPrefix  string   `comment:"Required when ExpandOthers=true. The prefix of the keys for storing the remaining record fields"`
	KeepSource       bool     `comment:"Optional. Whether to keep the source log content given successful decoding, default to false"`

	sep     rune
	context ilogtail.Context
}

func (p *ProcessorCSVDecoder) Init(context ilogtail.Context) error {
	sepRunes := []rune(p.SplitSep)
	if len(sepRunes) != 1 {
		return fmt.Errorf("invalid separator: %s", p.SplitSep)
	}
	p.sep = sepRunes[0]
	p.context = context
	return nil
}

func (*ProcessorCSVDecoder) Description() string {
	return "csv decoder for logtail"
}

func (p *ProcessorCSVDecoder) decodeCSV(log *protocol.Log, value string) bool {
	if len(p.SplitKeys) == 0 {
		if p.PreserveOthers {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_decode_preserve_", Value: value})
		}
		return true
	}

	r := csv.NewReader(strings.NewReader(value))
	r.Comma = p.sep
	r.TrimLeadingSpace = p.TrimLeadingSpace

	var record []string
	record, err := r.Read()
	if err != nil && err != io.EOF {
		logger.Warning(p.context.GetRuntimeContext(), "DECODE_LOG_ALARM", "cannot decode log", err, "log", util.CutString(value, 1024))
		return false
	}
	// Empty value should also be considered as a valid field.
	// (To be compatible with the fact that value with only blank chars
	// and TrimLeadingSpace=true is considered valid by encoding/csv pkg)
	if err == io.EOF {
		record = append(record, "")
	}

	var keyIndex int
	for keyIndex = 0; keyIndex < len(p.SplitKeys) && keyIndex < len(record); keyIndex++ {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[keyIndex], Value: record[keyIndex]})
	}

	if keyIndex < len(record) && p.PreserveOthers {
		if p.ExpandOthers {
			for ; keyIndex < len(record); keyIndex++ {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.ExpandKeyPrefix + strconv.Itoa(keyIndex+1-len(p.SplitKeys)), Value: record[keyIndex]})
			}
		} else {
			var b strings.Builder
			w := csv.NewWriter(&b)
			w.Comma = p.sep
			_ = w.Write(record[keyIndex:])
			w.Flush()
			remained := b.String()
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_decode_preserve_", Value: remained[:len(remained)-1]})
		}
	}

	if len(p.SplitKeys) != len(record) {
		logger.Warning(p.context.GetRuntimeContext(), "DECODE_LOG_ALARM", "decode keys not match, split len", len(record), "log", util.CutString(value, 1024))
	}
	return true
}

func (p *ProcessorCSVDecoder) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		findKey := false
		for i, cont := range log.Contents {
			if len(p.SourceKey) == 0 || p.SourceKey == cont.Key {
				findKey = true
				res := p.decodeCSV(log, cont.Value)
				if !p.shouldKeepSrc(res) {
					log.Contents = append(log.Contents[:i], log.Contents[i+1:]...)
				}
				break
			}
		}
		if !findKey && p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), "DECODE_FIND_ALARM", "cannot find key", p.SourceKey)
		}
	}
	return logArray
}

func (p *ProcessorCSVDecoder) shouldKeepSrc(res bool) bool {
	return p.KeepSource || !res
}

func init() {
	ilogtail.Processors["processor_csv"] = func() ilogtail.Processor {
		return &ProcessorCSVDecoder{
			SplitSep: ",",
		}
	}
}
