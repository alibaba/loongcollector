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

package kvsplitter

import (
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type KeyValueSplitter struct {
	SourceKey string
	// Split key/value pairs.
	Delimiter string
	// Split key and value.
	Separator            string
	KeepSource           bool
	EmptyKeyPrefix       string
	NoSeparatorKeyPrefix string
	Quote                string

	DiscardWhenSeparatorNotFound bool
	ErrIfSourceKeyNotFound       bool
	ErrIfSeparatorNotFound       bool
	ErrIfKeyIsEmpty              bool

	context pipeline.Context
}

const (
	defaultDelimiter            = "\t"
	defaultSeparator            = ":"
	defaultEmptyKeyPrefix       = "empty_key_"
	defaultNoSeparatorKeyPrefix = "no_separator_key_"
)

func (s *KeyValueSplitter) Init(context pipeline.Context) error {
	s.context = context

	if len(s.Delimiter) == 0 {
		s.Delimiter = defaultDelimiter
	}
	if len(s.Separator) == 0 {
		s.Separator = defaultSeparator
	}
	if len(s.EmptyKeyPrefix) == 0 {
		s.EmptyKeyPrefix = defaultEmptyKeyPrefix
	}
	if len(s.NoSeparatorKeyPrefix) == 0 {
		s.NoSeparatorKeyPrefix = defaultNoSeparatorKeyPrefix
	}
	return nil
}

func (*KeyValueSplitter) Description() string {
	return "Processor to split key value pairs"
}

func (s *KeyValueSplitter) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		s.processLog(log)
	}
	return logArray
}

func (s *KeyValueSplitter) processLog(log *protocol.Log) {
	hasKey := false
	for idx, content := range log.Contents {
		if len(s.SourceKey) == 0 || s.SourceKey == content.Key {
			hasKey = true
			if !s.KeepSource {
				log.Contents = append(log.Contents[:idx], log.Contents[idx+1:]...)
			}
			s.splitKeyValue(log, content.Value)
			break
		}
	}
	if !hasKey && s.ErrIfSourceKeyNotFound {
		logger.Warningf(s.context.GetRuntimeContext(), "KV_SPLITTER_ALARM", "can not find key: %v", s.SourceKey)
	}
}

func (s *KeyValueSplitter) splitKeyValue(log *protocol.Log, content string) {
	emptyKeyIndex := 0
	noSeparatorKeyIndex := 0
	for {
		dIdx := strings.Index(content, s.Delimiter)
		var pair string
		if dIdx == -1 {
			pair = content
		} else {
			pair = content[:dIdx]
		}

		pair, dIdx = s.concatQuotePair(pair, content, dIdx)
		pos := strings.Index(pair, s.Separator)
		if pos == -1 {
			if s.ErrIfSeparatorNotFound {
				logger.Warningf(s.context.GetRuntimeContext(), "KV_SPLITTER_ALARM", "can not find separator in %v", pair)
			}
			if !s.DiscardWhenSeparatorNotFound {
				log.Contents = append(log.Contents, &protocol.Log_Content{
					Key:   s.NoSeparatorKeyPrefix + strconv.Itoa(noSeparatorKeyIndex),
					Value: s.getValue(pair),
				})
				noSeparatorKeyIndex++
			}
		} else {
			key := pair[:pos]
			value := s.getValue(pair[pos+len(s.Separator):])
			if len(key) == 0 {
				key = s.EmptyKeyPrefix + strconv.Itoa(emptyKeyIndex)
				emptyKeyIndex++
				if s.ErrIfKeyIsEmpty {
					logger.Warningf(s.context.GetRuntimeContext(), "KV_SPLITTER_ALARM",
						"the key of pair with value (%v) is empty", value)
				}
			}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: key, Value: value})
		}

		if dIdx == -1 || dIdx+len(s.Delimiter) > len(content) {
			break
		} else {
			content = content[dIdx+len(s.Delimiter):]
		}
	}
}

func (s *KeyValueSplitter) concatQuotePair(pair string, content string, dIdx int) (string, int) {
	// If Pair not end with quote,try to reIndex the pair
	// Separator+Quote or Quote in prefix
	if dIdx >= 0 && len(s.Quote) > 0 && !strings.HasSuffix(pair, s.Quote) &&
		(strings.Index(pair, s.Separator+s.Quote) > 0 || strings.HasPrefix(pair, s.Quote)) {
		// ReIndex from last delimiter to find next quote index
		// Ignore \Quote situation
		if lastQuote := s.getNearestQuote(content, dIdx); lastQuote >= 0 {
			dIdx = lastQuote
			pair = content[:dIdx]
		}
	}
	return pair, dIdx
}

func (s *KeyValueSplitter) getNearestQuote(content string, startPos int) int {
	for startPos < len(content) {
		if len(s.Quote) == 1 {
			lastQuoteContent := strings.Index(content[startPos:], " \\"+s.Quote)
			lastQuote := strings.Index(content[startPos+1:], s.Quote)
			// relate to last quote real position
			startPos = (lastQuote + startPos + 1 + len(s.Quote))
			if lastQuoteContent >= 0 {
				if lastQuoteContent+1 == lastQuote { // hit latent content
					continue
				} else if lastQuote >= 0 { // hit latent content,but quote comes first
					return startPos
				}
			} else { // no \\quote and has next quote
				return startPos
			}
		} else {
			startPos += (strings.Index(content[startPos+1:], s.Quote) + len(s.Separator+s.Quote))
			return startPos
		}
	}
	return startPos
}

func (s *KeyValueSplitter) getValue(value string) string {
	if lenQ := len(s.Quote); lenQ > 0 {
		// remove quote
		if len(value) >= 2*lenQ && strings.HasPrefix(value, s.Quote) && strings.HasSuffix(value, s.Quote) {
			value = value[lenQ : len(value)-lenQ]
		}
	}
	return value
}

func newKeyValueSplitter() *KeyValueSplitter {
	return &KeyValueSplitter{
		Delimiter:                    defaultDelimiter,
		Separator:                    defaultSeparator,
		KeepSource:                   true,
		EmptyKeyPrefix:               defaultEmptyKeyPrefix,
		NoSeparatorKeyPrefix:         defaultNoSeparatorKeyPrefix,
		ErrIfSourceKeyNotFound:       true,
		ErrIfSeparatorNotFound:       true,
		ErrIfKeyIsEmpty:              true,
		DiscardWhenSeparatorNotFound: false,
	}
}

func init() {
	pipeline.Processors["processor_split_key_value"] = func() pipeline.Processor {
		return newKeyValueSplitter()
	}
}
