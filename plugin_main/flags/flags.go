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

package flags

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	defaultGlobalConfig  = `{"InputIntervalMs":5000,"AggregatIntervalMs":30,"FlushIntervalMs":30,"DefaultLogQueueSize":11,"DefaultLogGroupQueueSize":12}`
	defaultPluginConfig  = `{"inputs":[{"type":"metric_mock","detail":{"Tags":{"tag1":"aaaa","tag2":"bbb"},"Fields":{"content":"xxxxx","time":"2017.09.12 20:55:36"}}}],"flushers":[{"type":"flusher_stdout"}]}`
	defaultFlusherConfig = `{"type":"flusher_sls","detail":{}}`
)

// flags used to control ilogtail.
var (
	GlobalConfig     = flag.String("global", "./global.json", "global config.")
	PluginConfig     = flag.String("plugin", "./plugin.json", "plugin config.")
	FlusherConfig    = flag.String("flusher", "./default_flusher.json", "the default flusher configuration is used not only in the plugins without flusher but also to transfer the self telemetry data.")
	ForceSelfCollect = flag.Bool("force-statics", false, "force collect self telemetry data before closing.")
	AutoProfile      = flag.Bool("prof-auto", true, "auto dump prof file when prof-flag is open.")
	HTTPProfFlag     = flag.Bool("prof-flag", false, "http pprof flag.")
	Cpuprofile       = flag.String("cpu-profile", "cpu.prof", "write cpu profile to file.")
	Memprofile       = flag.String("mem-profile", "mem.prof", "write mem profile to file.")
	HTTPAddr         = flag.String("server", ":18689", "http server address.")
	Doc              = flag.Bool("doc", false, "generate plugin docs")
	DocPath          = flag.String("docpath", "./docs/en/plugins", "generate plugin docs")
	HTTPLoadFlag     = flag.Bool("http-load", false, "export http endpoint for load plugin config.")
	FileIOFlag       = flag.Bool("file-io", false, "use file for input or output.")
	InputFile        = flag.String("input-file", "./input.log", "input file")
	InputField       = flag.String("input-field", "content", "input file")
	InputLineLimit   = flag.Int("input-line-limit", 1000, "input file")
	OutputFile       = flag.String("output-file", "./output.log", "output file")
	StatefulSetFlag  = flag.Bool("ALICLOUD_LOG_STATEFULSET_FLAG", false, "alibaba log export ports flag, set true if you want to use it")
)

var (
	flusherType     string
	flusherCfg      map[string]interface{}
	flusherLoadOnce sync.Once
)

// LoadConfig read the plugin content.
func LoadConfig() (globalCfg string, pluginCfgs []string, err error) {
	if gCfg, errRead := ioutil.ReadFile(*GlobalConfig); errRead != nil {
		globalCfg = defaultGlobalConfig
	} else {
		globalCfg = string(gCfg)
	}

	if !json.Valid([]byte(globalCfg)) {
		err = fmt.Errorf("illegal input global config:%s", globalCfg)
		return
	}

	var pluginCfg string
	if pCfg, errRead := ioutil.ReadFile(*PluginConfig); errRead == nil {
		pluginCfg = string(pCfg)
	} else {
		pluginCfg = defaultPluginConfig
	}

	if !json.Valid([]byte(pluginCfg)) {
		err = fmt.Errorf("illegal input plugin config:%s", pluginCfg)
		return
	}

	var cfgs []map[string]interface{}
	errUnmarshal := json.Unmarshal([]byte(pluginCfg), &cfgs)
	if errUnmarshal != nil {
		pluginCfgs = append(pluginCfgs, changePluginConfigIO(pluginCfg))
		return
	}
	for _, cfg := range cfgs {
		bytes, _ := json.Marshal(cfg)
		pluginCfgs = append(pluginCfgs, changePluginConfigIO(string(bytes)))
	}
	return
}

// GetFlusherConfiguration returns the flusher category and options.
func GetFlusherConfiguration() (flusherCategory string, flusherOptions map[string]interface{}) {
	flusherLoadOnce.Do(func() {
		extract := func(cfg []byte) (string, map[string]interface{}, bool) {
			m := make(map[string]interface{})
			err := json.Unmarshal(cfg, &m)
			if err != nil {
				logger.Error(context.Background(), "DEFAULT_FLUSHER_ALARM", "err", err)
				return "", nil, false
			}
			c, ok := m["type"].(string)
			if !ok {
				return "", nil, false
			}
			options, ok := m["detail"].(map[string]interface{})
			if !ok {
				return c, nil, true
			}
			return c, options, true
		}
		if fCfg, err := ioutil.ReadFile(*FlusherConfig); err == nil {
			category, options, ok := extract(fCfg)
			if ok {
				flusherType = category
				flusherCfg = options
			} else {
				flusherType, flusherCfg, _ = extract([]byte(defaultFlusherConfig))
			}
		} else {
			flusherType, flusherCfg, _ = extract([]byte(defaultFlusherConfig))
		}

	})
	return flusherType, flusherCfg
}

func OverrideByEnv() {
	_ = util.InitFromEnvBool("LOGTAIL_DEBUG_FLAG", HTTPProfFlag, *HTTPProfFlag)
	_ = util.InitFromEnvBool("LOGTAIL_AUTO_PROF", AutoProfile, *AutoProfile)
	_ = util.InitFromEnvBool("LOGTAIL_FORCE_COLLECT_SELF_TELEMETRY", ForceSelfCollect, *ForceSelfCollect)
	_ = util.InitFromEnvBool("LOGTAIL_HTTP_LOAD_CONFIG", HTTPLoadFlag, *HTTPLoadFlag)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_STATEFULSET_FLAG", StatefulSetFlag, *StatefulSetFlag)
}

type pipelineConfig struct {
	Inputs      []interface{} `json:"inputs"`
	Processors  []interface{} `json:"processors"`
	Aggregators []interface{} `json:"aggregators"`
	Flushers    []interface{} `json:"flushers"`
}

var (
	fileInput = map[string]interface{}{
		"type": "metric_debug_file",
		"detail": map[string]interface{}{
			"InputFilePath": "./input.log",
			"FieldName":     "content",
			"LineLimit":     1000,
		},
	}
	fileOutput = map[string]interface{}{
		"type": "flusher_stdout",
		"detail": map[string]interface{}{
			"FileName": "./output.log",
		},
	}
)

func changePluginConfigIO(pluginCfg string) string {
	if *FileIOFlag {
		var newCfg pipelineConfig
		if err := json.Unmarshal([]byte(pluginCfg), &newCfg); err == nil {
			// Input
			fileInput["detail"].(map[string]interface{})["InputFilePath"] = *InputFile
			fileInput["detail"].(map[string]interface{})["FieldName"] = *InputField
			fileInput["detail"].(map[string]interface{})["LineLimit"] = *InputLineLimit
			newCfg.Inputs = []interface{}{fileInput}
			// Processors
			if newCfg.Processors == nil {
				newCfg.Processors = make([]interface{}, 0)
			}
			// Aggregators
			if newCfg.Aggregators == nil {
				newCfg.Aggregators = make([]interface{}, 0)
			}
			// Flushers
			fileOutput["detail"].(map[string]interface{})["FileName"] = *OutputFile
			newCfg.Flushers = append(newCfg.Flushers, fileOutput)

			cfg, _ := json.Marshal(newCfg)
			pluginCfg = string(cfg)
		} else {
			logger.Error(context.Background(), "PLUGIN_UNMARSHAL_ALARM", "err", err)
		}
		return pluginCfg
	}
	return pluginCfg
}
