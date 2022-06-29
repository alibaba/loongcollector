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
	"flag"
	"fmt"
	"runtime"

	"github.com/alibaba/ilogtail"
	_ "github.com/alibaba/ilogtail/helper/envconfig"
	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/signals"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugin_main/flags"
	_ "github.com/alibaba/ilogtail/plugin_main/wrapmemcpy"
	_ "github.com/alibaba/ilogtail/plugins/all"
)

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

	globalCfg, pluginCfgs, err := flags.LoadConfig()
	if err != nil {
		return
	} else if InitPluginBaseV2(globalCfg) != 0 {
		return
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
	// handle the first shutdown signal gracefully
	<-signals.SetupSignalHandler()
	logger.Info(context.Background(), "########################## exit process begin ##########################")
	HoldOn(1)
	logger.Info(context.Background(), "########################## exit process done ##########################")
}

func generatePluginDoc() {
	for name, creator := range ilogtail.ServiceInputs {
		doc.Register("service_input", name, creator())
	}
	for name, creator := range ilogtail.MetricInputs {
		doc.Register("metric_input", name, creator())
	}
	for name, creator := range ilogtail.Processors {
		doc.Register("processor", name, creator())
	}
	for name, creator := range ilogtail.Aggregators {
		doc.Register("aggregator", name, creator())
	}
	for name, creator := range ilogtail.Flushers {
		doc.Register("flusher", name, creator())
	}
	doc.Generate(*flags.DocPath)
}
