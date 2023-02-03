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

package char

import (
	"fmt"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// ProcessorSplitChar is a processor plugin to split field (SourceKey) with SplitSep (single byte)
// and reinsert extracted values into log with SplitKeys.
// Quote can be used if there are SplitSep in values to extract.
// If QuoteFlag is set and Quote is used, value of the key should be enclosed by Quote,
// for example (quote: ", SplitSep: ,), abc,"bcd""" is ok but abc,bc"d""" is not supported.
// If PreserveOthers is set, it will insert the remainder bytes (after splitting) into log
// with key '_split_preserve_' instead of dropping them.
type ProcessorSplitChar struct {
	Quote                  string
	QuoteFlag              bool
	SplitSep               string
	SplitKeys              []string
	SourceKey              string
	PreserveOthers         bool
	NoKeyError             bool
	NoMatchError           bool
	KeepSource             bool
	KeepSourceIfParseError bool

	quoteChar    byte
	splitSepChar byte
	context      pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorSplitChar) Init(context pipeline.Context) error {
	if len(p.SplitSep) != 1 {
		return fmt.Errorf("split char plugin only support single char, invalid sep : %s", p.SplitSep)
	}
	if p.QuoteFlag && len(p.Quote) > 0 {
		p.quoteChar = p.Quote[0]
		logger.Debug(context.GetRuntimeContext(), "init spliter, Quote", p.Quote, "len", len(p.Quote), "Quote char", p.quoteChar)
	}
	p.splitSepChar = p.SplitSep[0]
	p.context = context
	return nil
}

func (*ProcessorSplitChar) Description() string {
	return "single char splitor for logtail"
}

func (p *ProcessorSplitChar) splitValue(log *protocol.Log, value string) bool {
	if len(p.SplitKeys) == 0 {
		if p.PreserveOthers {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_split_preserve_", Value: value})
		}
		return true
	}

	keyIndex := 0
	lastValueIndex := 0
	i := 0
	if p.QuoteFlag {
	FORLOOP:
		for keyIndex = 0; keyIndex < len(p.SplitKeys) && i < len(value); keyIndex++ {
			// quota flag
			switch value[i] {
			case p.splitSepChar:
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[keyIndex], Value: ""})
				lastValueIndex = i + 1
				i++
			case p.quoteChar:
				i++
				// Enter quote, if a new quote byte appears, three cases are allowed:
				// 1. It is the last byte of value to represent an ending quote.
				// 2. The next byte is splitSepChar, it also represents an ending quote.
				// 3. Next byte is also a quote, which means escaping quote.
				for newValue := []byte{}; i < len(value); i++ {
					if value[i] == p.quoteChar {
						if i == len(value)-1 || value[i+1] == p.splitSepChar { // Case 1 and 2.
							i++
							log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[keyIndex], Value: string(newValue)})
							lastValueIndex = i + 1
							i++
							break
						}
						if value[i+1] == p.quoteChar { // Case 3.
							i++
							newValue = append(newValue, value[i])
						} else {
							logger.Warning(p.context.GetRuntimeContext(), "SPLIT_LOG_ALARM", "no continue quote", p.quoteChar, "log", util.CutString(value, 1024))
							return false
						}
					} else {
						newValue = append(newValue, value[i])
					}
				}
			default:
				if nextIndex := strings.IndexByte(value[i:], p.splitSepChar); nextIndex >= 0 {
					log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[keyIndex], Value: value[i : i+nextIndex]})
					lastValueIndex = i + nextIndex + 1
					i = lastValueIndex
					continue
				} else {
					break FORLOOP
				}
			}
		}
		if keyIndex >= len(p.SplitKeys) && lastValueIndex < len(value) {
			if p.PreserveOthers {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_split_preserve_", Value: value[lastValueIndex:]})
				lastValueIndex = len(value)
			}
		}
	} else {
		for i = 0; i < len(value); i++ {
			if value[i] == p.splitSepChar {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[keyIndex], Value: value[lastValueIndex:i]})
				keyIndex++
				lastValueIndex = i + 1

				if keyIndex >= len(p.SplitKeys) && i != len(value)-1 {
					if p.PreserveOthers {
						log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_split_preserve_", Value: value[i+1:]})
					}
					break
				}
			}
		}
	}
	if keyIndex < len(p.SplitKeys) && lastValueIndex < len(value) {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[keyIndex], Value: value[lastValueIndex:]})
		keyIndex++
	}
	if keyIndex < len(p.SplitKeys) && p.NoMatchError {
		logger.Warning(p.context.GetRuntimeContext(), "SPLIT_LOG_ALARM", "split keys not match, split len", keyIndex, "log", util.CutString(value, 1024))
	}
	return true
}

func (p *ProcessorSplitChar) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		findKey := false
		for i, cont := range log.Contents {
			if len(p.SourceKey) == 0 || p.SourceKey == cont.Key {
				findKey = true
				splitResult := p.splitValue(log, cont.Value)
				if !p.shouldKeepSource(splitResult) {
					log.Contents = append(log.Contents[:i], log.Contents[i+1:]...)
				}
				break
			}
		}
		if !findKey && p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), "SPLIT_FIND_ALARM", "cannot find key", p.SourceKey)
		}
	}

	return logArray
}

func (p *ProcessorSplitChar) shouldKeepSource(splitResult bool) bool {
	return p.KeepSource || (p.KeepSourceIfParseError && !splitResult)
}

func init() {
	pipeline.Processors["processor_split_char"] = func() pipeline.Processor {
		return &ProcessorSplitChar{
			SplitSep:               "\n",
			PreserveOthers:         true,
			KeepSourceIfParseError: true,
		}
	}
}
