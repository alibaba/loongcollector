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

package dictmap

import (
	"encoding/csv"
	"fmt"
	"io"
	"os"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const pluginType = "processor_dict_map"

type ProcessorDictMap struct {
	DictFilePath  string
	MapDict       map[string]string
	SourceKey     string
	DestKey       string
	HandleMissing bool
	Missing       string
	Mode          string
	MaxDictSize   int
	scanDestKey   bool
	context       pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorDictMap) Init(context pipeline.Context) error {
	p.context = context

	if p.SourceKey == "" {
		return fmt.Errorf("must specify SourceKey for plugin %v", pluginType)
	}

	if p.DestKey == "" || p.DestKey == p.SourceKey {
		p.scanDestKey = false
		p.DestKey = p.SourceKey
	} else {
		p.scanDestKey = true
	}

	if p.DictFilePath == "" && len(p.MapDict) == 0 {
		return fmt.Errorf("at least give one source map data for plugin %v", pluginType)
	}

	if len(p.MapDict) > p.MaxDictSize {
		return fmt.Errorf("map size exceed maximum length %v for plugin %v ", p.MaxDictSize, pluginType)
	}

	if p.Mode != "overwrite" && p.Mode != "fill" {
		return fmt.Errorf("invalid mode %v, you can only use \"fill\" or \"overwrite\" as Mode", p.Mode)
	}

	if p.DictFilePath != "" {
		if len(p.MapDict) > 0 {
			logger.Debugf(p.context.GetRuntimeContext(), "INFO: duplicate input ：json input:%v and file input:%v, only use file input \n", p.MapDict, p.DictFilePath)
		}
		p.MapDict = make(map[string]string, p.MaxDictSize)
		err := p.readCsvFile()
		if err != nil {
			return err
		}
	}

	return nil
}

func (*ProcessorDictMap) Description() string {
	return "dict map processor for logtail"
}

func (p *ProcessorDictMap) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

// read Csv File for map dict
func (p *ProcessorDictMap) readCsvFile() error {
	fs, err := os.Open(p.DictFilePath)
	if err != nil {
		return fmt.Errorf("can not read map file, err: %+v", err)
	}
	defer func(fs *os.File) {
		err := fs.Close()
		if err != nil {
			logger.Debugf(p.context.GetRuntimeContext(), "Close File err:%v", err)
		}
	}(fs)
	r := csv.NewReader(fs)
	r.Comma = ','

	for i := len(p.MapDict); i <= p.MaxDictSize; i++ {
		row, err := r.Read()
		if err != nil && err != io.EOF {
			return fmt.Errorf("can not read full map file, err: %+v", err)
		}

		if err == io.EOF {
			if i == 0 {
				return fmt.Errorf("empty file: %+v", p.DictFilePath)
			}
			break
		}

		if len(row) != 2 {
			return fmt.Errorf("illegal input: row %+v's length is not 2", i+1)
		}

		if value, exists := p.MapDict[row[0]]; exists && value != row[1] {
			return fmt.Errorf("hash crash, check whether the map rule redefined of value: %+v", value)
		}

		logger.Debugf(p.context.GetRuntimeContext(), "Plugin %v adds mapping rule %v : %v", pluginType, row[0], row[1])
		p.MapDict[row[0]] = row[1]
	}
	return nil
}

func (p *ProcessorDictMap) handleMode(log *protocol.Log, destKeyIdx int, fillIn string) {
	switch p.Mode {
	case "fill":
		return
	case "overwrite":
		log.Contents[destKeyIdx].Value = fillIn
		return
	}
}

// map log contents into target value
func (p *ProcessorDictMap) processLog(log *protocol.Log) {
	hasSourceKey, hasDestKey := false, false
	sourceKeyIdx, destKeyIdx := 0, 0
	fillIn := ""
	for idx := range log.Contents {
		if !hasSourceKey && p.SourceKey == log.Contents[idx].Key {
			hasSourceKey = true
			sourceKeyIdx = idx
			if value, exist := p.MapDict[log.Contents[sourceKeyIdx].Value]; exist {
				fillIn = value
				if !p.scanDestKey {
					log.Contents[sourceKeyIdx].Value = fillIn
					return
				}
			} else {
				return
			}
		}
		if p.scanDestKey && !hasDestKey && p.DestKey == log.Contents[idx].Key {
			hasDestKey = true
			destKeyIdx = idx
		}
		if hasSourceKey && hasDestKey {
			p.handleMode(log, destKeyIdx, fillIn)
			return
		}
	}
	if !hasSourceKey && p.HandleMissing {
		if hasDestKey {
			p.handleMode(log, destKeyIdx, p.Missing)
		} else {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.DestKey, Value: p.Missing})
		}
		return
	}
	if p.scanDestKey && !hasDestKey {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.DestKey, Value: fillIn})
		return
	}
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorDictMap{
			HandleMissing: false,
			Missing:       "Unknown",
			Mode:          "overwrite",
			MaxDictSize:   1000,
		}
	}
}
