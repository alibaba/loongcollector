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

package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"runtime"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/signals"
	"github.com/alibaba/ilogtail/pkg/util"
	_ "github.com/alibaba/ilogtail/plugin_main/wrapmemcpy"
	_ "github.com/alibaba/ilogtail/plugins/all"
)

// LoadConfig read the plugin content.
func LoadConfigPurPlugin() (globalCfg string, pluginCfgs []string, err error) {
	if gCfg, errRead := os.ReadFile(*flags.GlobalConfig); errRead != nil {
		globalCfg = flags.DefaultGlobalConfig
	} else {
		globalCfg = string(gCfg)
	}

	if !json.Valid([]byte(globalCfg)) {
		err = fmt.Errorf("illegal input global config:%s", globalCfg)
		return
	}

	var pluginCfg string
	if pCfg, errRead := os.ReadFile(*flags.PluginConfig); errRead == nil {
		pluginCfg = string(pCfg)
	} else {
		pluginCfg = flags.DefaultPluginConfig
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
	if *flags.FileIOFlag {
		var newCfg pipelineConfig
		if err := json.Unmarshal([]byte(pluginCfg), &newCfg); err == nil {
			// Input
			fileInput["detail"].(map[string]interface{})["InputFilePath"] = *flags.InputFile
			fileInput["detail"].(map[string]interface{})["FieldName"] = *flags.InputField
			fileInput["detail"].(map[string]interface{})["LineLimit"] = *flags.InputLineLimit
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
			fileOutput["detail"].(map[string]interface{})["FileName"] = *flags.OutputFile
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

// main export http control method in pure GO.
func main() {
	flag.Parse()
	defer logger.Flush()
	if *flags.Doc {
		generatePluginDoc()
		return
	}
	cpu := runtime.NumCPU()
	procs := runtime.GOMAXPROCS(0)
	fmt.Println("cpu num:", cpu, " GOMAXPROCS:", procs)
	fmt.Println("hostname : ", util.GetHostName())
	fmt.Println("hostIP : ", util.GetIPAddress())
	fmt.Printf("load config %s %s %s\n", *flags.GlobalConfig, *flags.PluginConfig, *flags.FlusherConfig)

	globalCfg, pluginCfgs, err := LoadConfigPurPlugin()
	fmt.Println("global config : ", globalCfg)
	fmt.Println("plugin config : ", pluginCfgs)
	if err != nil {
		return
	} else if InitPluginBaseV2(globalCfg) != 0 {
		return
	}
	if *flags.DeployMode == flags.DeploySingleton && *flags.EnableKubernetesMeta {
		instance := k8smeta.GetMetaManagerInstance()
		err := instance.Init("")
		if err != nil {
			logger.Error(context.Background(), "K8S_META_INIT_FAIL", "init k8s meta manager fail", err)
			return
		}
		stopCh := make(chan struct{})
		instance.Run(stopCh)
	}

	// load the static configs.
	for i, cfg := range pluginCfgs {
		p := fmt.Sprintf("PluginProject_%d", i)
		l := fmt.Sprintf("PluginLogstore_%d", i)
		c := fmt.Sprintf("1.0#PluginProject_%d##Config%d", i, i)
		if LoadConfig(p, l, c, 123, cfg) != 0 {
			logger.Warningf(context.Background(), "START_PLUGIN_ALARM", "%s_%s_%s start fail, config is %s", p, l, c, cfg)
			return
		}
	}

	Resume()

	// handle the first shutdown signal gracefully, and exit directly if FileIOFlag is true
	if !*flags.FileIOFlag {
		<-signals.SetupSignalHandler()
	}
	logger.Info(context.Background(), "########################## exit process begin ##########################")
	HoldOn(1)
	logger.Info(context.Background(), "########################## exit process done ##########################")
}

func generatePluginDoc() {
	for name, creator := range pipeline.ServiceInputs {
		doc.Register("service_input", name, creator())
	}
	for name, creator := range pipeline.MetricInputs {
		doc.Register("metric_input", name, creator())
	}
	for name, creator := range pipeline.Processors {
		doc.Register("processor", name, creator())
	}
	for name, creator := range pipeline.Aggregators {
		doc.Register("aggregator", name, creator())
	}
	for name, creator := range pipeline.Flushers {
		doc.Register("flusher", name, creator())
	}
	doc.Generate(*flags.DocPath)
}
